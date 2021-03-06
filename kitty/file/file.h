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
  std::vector<uint8_t>::size_type data_p;
};
/* Represents file in memory, storage or socket */
template <class Stream>
class FD { /* File descriptor */
  typedef std::chrono::milliseconds duration_t;
  Stream _stream;

  // Change of cacheSize only affects next load
  static constexpr std::vector<uint8_t>::size_type _cacheSize = 1024;
  
  duration_t _millisec;
  

  buffer_t _in;
  buffer_t _out;

  static constexpr int READ = 0, WRITE = 1;
public:
  FD(FD && other) noexcept : _in(std::move(other._in)), _out(std::move(other._out)) {
    _stream = std::move(other._stream);
    _millisec = other._millisec;
  }

  FD& operator=(FD && other) noexcept {
    std::swap(_stream, other._stream);
    std::swap(_in, other._in);
    std::swap(_out, other._out);
    std::swap(_millisec, other._millisec);
    
    return *this;
  }

  FD() = default;

  template<class T1, class T2, class... Args>
  FD(std::chrono::duration<T1,T2> duration, Args && ... params)
  : _stream(std::forward<Args>(params)...), _millisec(std::chrono::duration_cast<duration_t>(duration)), _in { {}, 0 }, _out { {}, 0 } {}

  ~FD() noexcept { seal(); }

  Stream &getStream() { return _stream; }

  // Write to file
  int out() {
    if ((_select(WRITE))) {
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

      return err::OK;
    }

    write_clear();
    return -1;
  }

  // Useful when fine control is necessary
  util::Optional<uint8_t> next() {
    // Load new _cache if end of buffer is reached
    if (_endOfBuffer()) {
      if (_load(_cacheSize)) {
        return util::Optional<uint8_t>();
      }
    }

    // If cache.empty() return '\0'
    return _in.cache.empty() ? util::Optional<uint8_t>() : util::Optional<uint8_t>(_in.cache[_in.data_p++]);
  }

  template<class Function>
  int eachByte(Function &&f, std::uint64_t max = std::numeric_limits<std::uint64_t>::max()) {
    while(!eof() && max) {
      if(_endOfBuffer()) {

        if(_load(_cacheSize)) {
          return -1;
        }

        continue;
      }

      --max;

      if (err::code_t err = f(_in.cache[_in.data_p++])) {
        // Return FileErr::OK if err_code != FileErr::BREAK
        return err == err::BREAK ? 0 : -1;
      }
    }

    return err::OK;
  }

  template<class T>
  FD &append(T &&container) {
    AppendFunc<T>::run(_out.cache, std::forward<T>(container));

    return *this;
  }

  FD &write_clear() {
    _out.cache.clear();
    _out.data_p = 0;


    return *this;
  }

  FD &read_clear() {
    _in.cache.clear();
    _in.data_p = 0;

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
      if(_endOfBuffer()) {
        if(_load(_cacheSize)) {
          return -1;
        }

        continue;
      }

      while (!_endOfBuffer()) {
        cache.push_back(_in.cache[_in.data_p++]);

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

private:
  int _select(const int read) {
    if(_millisec.count() > 0) {
      auto dur_micro = (suseconds_t)std::chrono::duration_cast<std::chrono::microseconds>(_millisec).count();
      timeval tv {
        0,
        dur_micro
      };

      fd_set selected;

      FD_ZERO(&selected);
      FD_SET(_stream.fd(), &selected);

      int result;
      if (read == READ) {
        result = select(_stream.fd() + 1, &selected, nullptr, nullptr, &tv);
      }
      else /*if (read == WRITE)*/ {
        result = select(_stream.fd() + 1, nullptr, &selected, nullptr, &tv);
      }

      if (result < 0) {
        err::code = err::LIB_SYS;
        
        if(_stream.fd() <= 0) err::set("FUCK THIS SHIT!");
        return -1;
      }

      else if (result == 0) {
        err::code = err::TIMEOUT;
        return -1;
      }
    }

    return err::OK;
  }

  // Load file into cache.data(), replaces old cache.data()
  int _load(std::vector<uint8_t>::size_type max_bytes) {
    if(_select(READ)) {
      return -1;
    }
    
    _in.cache.resize(max_bytes);
    
    if(_stream.read(_in.cache) < 0)
      return -1;
    
    _in.data_p = 0;
    return err::OK;
  }
  
  bool _endOfBuffer() {
    return _in.data_p == _in.cache.size();
  }
  
  template<class T, class S = void>
  struct AppendFunc {
    static void run(std::vector<uint8_t> &cache, const T &container) {
      cache.insert(std::end(cache), std::cbegin(container), std::cend(container));
    }
  };

  template<class T>
  struct AppendFunc<T, std::enable_if_t<util::instantiation_of<raw_t, T>::value>> {
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

  auto *x = buf;
  int err = io.eachByte([&](uint8_t ch) {
    *x++ = ch;

    return err::OK;
  }, data_len);

  auto *val = (T*)buf;

  if(err) {
    return std::nullopt;
  }

  return *val;
}

template<class T>
std::optional<std::string> read_string(FD<T> &socket, std::size_t size) {
  std::string buf;

  auto err = socket.eachByte([&buf](std::uint8_t byte) {
    buf += byte;

    return err::OK;
  }, size);

  if(err) {
    return std::nullopt;
  }

  return buf;
}

template<class T, class X>
class poll_t {
  static_assert(util::instantiation_of<FD, T>::value, "template parameter T must be an instantiation of file::FD");
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

//TODO: print is not thread_safe

template<class Stream>
int _print(file::FD<Stream> &file) {
  return file.out();
}

template<class Stream, class Out, class... Args>
int _print(file::FD<Stream> &file, Out && out, Args && ... params) {
  return _print(file.append(std::forward<Out>(out)), std::forward<Args>(params)...);
}

/*
 * First clear file, then recursively print all params
 */
template<class Stream, class... Args>
int print(file::FD<Stream> &file, Args && ... params) {
  return _print(file.write_clear(), std::forward<Args>(params)...);
}
#endif

