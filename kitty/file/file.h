#ifndef DOSSIER_FILE_H
#define DOSSIER_FILE_H

// TODO: This should be in a seperate header
#if __APPLE__
#define KITTY_POLLIN POLLIN
#define KITTY_POLLOUT POLLOUT
#define POLLRDHUP (POLLHUP | POLLNVAL)
#elif __linux__
#define KITTY_POLLIN  (POLLIN  | POLLRDHUP)
#define KITTY_POLLOUT (POLLOUT | POLLRDHUP) 
#endif

#include <functional>
#include <vector>
#include <chrono>
#include <string>
#include <optional>
#include <map>

#include <sys/select.h>
#include <poll.h>

#include <type_traits>

#include <kitty/err/err.h>
#include <kitty/util/optional.h>
#include <kitty/util/template_helper.h>

namespace file {
static constexpr int READ = 0, WRITE = 1;

template<class T>
struct raw_t {
  using value_type = T;

  const value_type &val;
};

template<class T>
raw_t<T> raw(const T& val) {
  return {
    val
  };
}

struct buffer_t {
  std::vector<uint8_t> cache;
  std::vector<uint8_t>::value_type *data_p;
  std::vector<uint8_t>::value_type *data_end;
};
/* Represents file in memory, storage or socket */
template <class Stream>
class FD { /* File descriptor */
public:
  using duration_t = std::chrono::milliseconds;
  using stream_t = Stream;
private:
  stream_t _stream;

  // Change of cacheSize only affects next load
  static constexpr std::vector<uint8_t>::size_type _cacheSize = 1024 << 3;

  duration_t _timeout;

  buffer_t _in;
  buffer_t _out;

public:
  FD(FD && other) noexcept : _in(std::move(other._in)), _out(std::move(other._out)) {
    _stream = std::move(other._stream);
    _timeout = other._timeout;
  }

  FD& operator=(FD && other) noexcept {
    std::swap(_stream, other._stream);
    std::swap(_in, other._in);
    std::swap(_out, other._out);
    std::swap(_timeout, other._timeout);
    
    return *this;
  }

  FD() = default;

  template<class T1, class T2, class... Args>
  FD(std::chrono::duration<T1,T2> duration, Args && ... params)
  : _stream(std::forward<Args>(params)...), _timeout(std::chrono::duration_cast<duration_t>(duration)), _in { {}, 0 }, _out { {}, 0 } {}

  ~FD() noexcept { seal(); }

  stream_t &getStream() { return _stream; }

  // Write to file
  int out() {
    if ((wait_for(WRITE))) {
      // Don't clear cache on timeout
      return -1;
    }

    // On success clear
    auto size = _stream.write(_out.cache);
    if (size >= 0) {
      // It's possible not all bytes are written
      if(size < _out.cache.size()) {
        _out.cache.erase(std::begin(_out.cache), std::begin(_out.cache) +size);

        return out();
      }

      write_clear();
      return err::OK;
    }

    write_clear();
    return -1;
  }

  template<class Function>
  int eachByte(Function &&f, std::uint64_t max = std::numeric_limits<std::uint64_t>::max()) {
    while(!eof() && max) {
      if(_end_of_buffer()) {
        if(_load()) {
          return -1;
        }

        continue;
      }

      --max;

      f(*_in.data_p++);
    }

    return err::OK;
  }

  /**
   * This is faster if you don't need to process byte by byte
   * @param in the input buffer
   * @param size
   * @return 0 on success
   */
  int read(std::uint8_t *in, std::size_t size) {
    while(!eof() && size) {
      if(_end_of_buffer()) {
        if(_load()) {
          return -1;
        }

        continue;
      }

      auto _size = std::min(_in.cache.size(), size);
      std::copy_n(_in.data_p, size, in);
      in += _size;
      size -= _size;
    }

    return 0;
  }

  template<class T>
  FD &append(T &&container) {
    AppendFunc<T>::run(_out.cache, std::forward<T>(container));

    return *this;
  }

  FD &write_clear() {
    _out.cache.clear();
    _out.data_p = _out.cache.data();
    _out.data_end = _out.data_p + _out.cache.size();

    return *this;
  }

  FD &read_clear() {
    _in.cache.clear();
    _in.data_p = _in.cache.data();
    _in.data_end = _in.data_p + _in.cache.size();

    return *this;
  }

  std::vector<uint8_t> &get_read_cache() {
    return _in.cache;
  }

  std::vector<uint8_t> &get_write_cache() {
    return _out.cache;
  }

  bool eof() {
    return _stream.eof();
  }

  bool is_open() {
    return _stream.is_open();
  }

  void seal() {
    if(is_open()) {
      _stream.seal();
    }
  }

  /*
     Copies max bytes from this to out
     If max == -1 copy the whole file
     On failure: return (Error)-1 or (Timeout)1
     On success: return  0
   */
  template<class OutStream>
  int copy(FD<OutStream> &out, std::uint64_t max = std::numeric_limits<std::uint64_t>::max()) {
    auto &cache = out.get_write_cache();

    while (!eof() && max) {
      if(_end_of_buffer()) {
        if(_load()) {
          return -1;
        }

        continue;
      }

      while (!_end_of_buffer()) {
        cache.push_back(*_in.data_p++);

        if(!max) {
          break;
        }

        --max;
      }
      
      if (out.out()) {
        return -1;
      }
    }

    return err::OK;
  }

  /**
   * Wait for READ/WRITE
   * @param mode either READ or WRITE
   * @return non-zero on timeout or error
   */
  int wait_for(const int mode) const {
    return _stream.select(_timeout, mode);
  }

private:
  // Load file into _in.cache.data(), replaces old _in.cache.data()
  int _load() {
    if(wait_for(READ)) {
      return -1;
    }
    
    _in.cache.resize(_cacheSize);
    if(_stream.read(_in.cache) < 0)
      return -1;

    _in.data_p   = _in.cache.data();
    _in.data_end = _in.data_p + _in.cache.size();

    return err::OK;
  }
  
  bool _end_of_buffer() const {
    return _in.data_p == _in.data_end;
  }
  
  template<class T, class S = void>
  struct AppendFunc {
    static void run(std::vector<uint8_t> &cache, const T &container) {
      cache.insert(std::end(cache), std::begin(container), std::end(container));
    }
  };

  template<class T>
  struct AppendFunc<T, std::enable_if_t<util::instantiation_of_v<raw_t, T>>> {
    static void run(std::vector<uint8_t> &cache, const T& _struct) {
      constexpr size_t data_len = sizeof(typename T::value_type);

      cache.reserve(data_len);

      auto *data = (uint8_t *) & _struct.val;

      for (size_t x = 0; x < data_len; ++x) {
        cache.emplace_back(data[x]);
      }
    }
  };

  template<class T>
  struct AppendFunc<T, typename std::enable_if<std::is_integral<typename std::decay<T>::type>::value>::type> {
    static void run(std::vector<uint8_t> &cache, T integral) {
      if(sizeof(T) == 1) {
        cache.push_back(integral);

        return;
      }
      
      std::string _integral = std::to_string(integral);

      cache.insert(cache.end(), _integral.cbegin(), _integral.cend());
    }
  };
  
  template<class T>
  struct AppendFunc<T, typename std::enable_if<std::is_floating_point<typename std::decay<T>::type>::value>::type> {
    static void run(std::vector<uint8_t> &cache, T integral) {
      std::string _float = std::to_string(integral);
      
      cache.insert(cache.end(), _float.cbegin(), _float.cend());
    }
  };

  template<class T>
  struct AppendFunc<T, typename std::enable_if<std::is_pointer<typename std::decay<T>::type>::value>::type> {
    static void run(std::vector<uint8_t> &cache, T pointer) {
      static_assert(sizeof(*pointer) == 1, "pointers must be const char *");

      std::string_view _pointer { pointer };

      cache.insert(cache.end(), _pointer.cbegin(), _pointer.cend());
    }
  };
};

template<class T, class Stream>
std::optional<T> read_struct(FD<Stream> &io) {
  constexpr size_t data_len = sizeof(T);
  uint8_t buf[data_len];

  auto err = io.read(buf, data_len);
  auto *val = (T*)buf;

  if(err) {
    return std::nullopt;
  }

  return *val;
}

template<class T>
std::optional<std::string> read_string(FD<T> &io, std::size_t size) {
  std::string buf; buf.resize(size);

  auto err = io.read(buf.data(), size);

  if(err) {
    return std::nullopt;
  }

  return buf;
}

template<class T>
std::optional<std::string> read_line(FD<T> &socket, std::string buf = std::string {}) {
  auto err = socket.eachByte([&buf](std::uint8_t byte) {
    if(byte == '\r') {
      return err::OK;
    }
    if(byte == '\n') {
      return err::BREAK;
    }

    buf += byte;

    return err::OK;
  });

  if(err) {
    return std::nullopt;
  }

  return std::move(buf);
}

template<class T, class X>
class poll_t {
  static_assert(util::instantiation_of_v<FD, T>, "template parameter T must be an instantiation of file::FD");
public:
  using file_t = T;
  using user_t = X;

  template<class FR, class FW, class FH>
  poll_t(FR &&fr, FW &&fw, FH &&fh) : _read_cb { std::forward<FR>(fr) }, _write_cb { std::forward<FW>(fw) }, _remove_cb { std::forward<FH>(fh) } {}

  poll_t(poll_t&&) = default;
  poll_t& operator=(poll_t&&) = default;

  void read(file_t &fd, user_t user_val) {
    std::lock_guard<std::mutex> lg(_mutex_add);

    _queue_add.emplace_back(user_val, &fd, pollfd {fd.getStream().fd(), KITTY_POLLIN, 0});
  }

  void write(file_t &fd, user_t user_val) {
    std::lock_guard<std::mutex> lg(_mutex_add);

    _queue_add.emplace_back(user_val, &fd, {fd.getStream().fd(), KITTY_POLLOUT, 0});
  }

  void read_write(file_t &fd, user_t user_val) {
    std::lock_guard<std::mutex> lg(_mutex_add);

    _queue_add.emplace_back(user_val, &fd, {fd.getStream().fd(), KITTY_POLLIN | KITTY_POLLOUT, 0});
  }

  void remove(file_t &fd) {
    std::lock_guard<std::mutex> lg(_mutex_add);

    _queue_remove.emplace_back(&fd);
  }

  void poll(std::chrono::milliseconds milli = std::chrono::milliseconds(50)) {
    _apply_changes();

    auto res = ::poll(_pollfd.data(), _pollfd.size(), milli.count());

    if(res > 0) {
      for(auto &pollfd : _pollfd) {
        if(pollfd.revents & (POLLHUP | POLLRDHUP | POLLERR)) {
          auto it = _fd_to_file.find(pollfd.fd);

          remove(*std::get<1>(it->second));
          continue;
        }

        if(pollfd.revents & POLLIN) {
          auto it = _fd_to_file.find(pollfd.fd);
          _read_cb(*std::get<1>(it->second), std::get<0>(it->second));
        }

        if(pollfd.revents & POLLOUT) {
          auto it = _fd_to_file.find(pollfd.fd);
          _write_cb(*std::get<1>(it->second), std::get<0>(it->second));
        }
      }
    }
  }
private:
  void _apply_changes() {
    std::lock_guard<std::mutex> lg(_mutex_add);

    for(auto &el : _queue_add) {
      _pollfd.emplace_back(std::get<2>(el));

      _fd_to_file.emplace(std::get<1>(el)->getStream().fd(), std::pair<user_t, file_t*> { std::get<0>(el), std::get<1>(el) });
    }

    for(auto &el : _queue_remove) {
      auto it = _fd_to_file.find(el->getStream().fd());
      auto cp = *it;

      _fd_to_file.erase(it);
      _pollfd.erase(std::remove_if(std::begin(_pollfd), std::end(_pollfd), [&el](const auto &l) {
        return l.fd == el->getStream().fd();
      }), std::end(_pollfd));

      _remove_cb(*std::get<1>(cp.second), std::get<0>(cp.second));
    }

    _queue_add.clear();
    _queue_remove.clear();
  }

  std::function<void(file_t &file, user_t)> _read_cb;
  std::function<void(file_t &file, user_t)> _write_cb;
  std::function<void(file_t &file, user_t)> _remove_cb;

  std::map<int, std::pair<user_t, file_t*>> _fd_to_file;

  std::vector<std::tuple<user_t, file_t*, pollfd>> _queue_add;
  std::vector<file_t*> _queue_remove;

  std::vector<pollfd> _pollfd;

  std::mutex _mutex_add;
};

}

/*
 * First clear file, then recursively print all params
 */
template<class Stream, class... Args>
int print(file::FD<Stream> &file, Args && ... params) {
  file.write_clear();

  (file.append(std::forward<Args>(params)),...);

  return file.out();
}
#endif

