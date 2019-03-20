#ifndef DOSSIER_FILE_H
#define DOSSIER_FILE_H

#include <cassert>
#include <functional>
#include <vector>
#include <chrono>
#include <string>
#include <optional>
#include <memory>
#include <map>

#include <type_traits>

#include <kitty/err/err.h>
#include <kitty/util/optional.h>
#include <kitty/util/template_helper.h>

namespace file {
using namespace std::literals;
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

struct buffer_in_t {
  std::uint8_t *data_p;
  std::uint8_t *data_end;

  buffer_in_t() = default;
  buffer_in_t(std::size_t capacity) : capacity { capacity }, cache { new std::uint8_t[capacity] } {
    data_p = data_end = cache.get();
  }

  std::size_t capacity;
  std::unique_ptr<std::uint8_t[]> cache;

  std::size_t size() {
    return (std::size_t)(data_end - data_p);
  }
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
  static constexpr std::size_t _cache_size = 1024;

  duration_t _timeout;

  buffer_in_t _in;
  std::vector<std::uint8_t> _out;

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
  : _stream(std::forward<Args>(params)...), _timeout(std::chrono::duration_cast<duration_t>(duration)), _in { _cache_size }, _out {} {}

  ~FD() noexcept { seal(); }

  stream_t &getStream() { return _stream; }

  // Write to file
  int out() {
    if(wait_for(WRITE)) {
      // Don't clear cache on timeout
      return -1;
    }

    // On success clear
    auto data_p    = _out.data();
    auto data_size = _out.size();

    while(data_size) {
      auto bytes_written = _stream.write(data_p, data_size);

      if(bytes_written < 0) {
        return -1;
      }

      data_p += bytes_written;
      data_size -= bytes_written;
    }

    write_clear();

    return 0;
  }

  template<class Function>
  int eachByte(Function &&f, std::uint64_t max = std::numeric_limits<std::uint64_t>::max()) {
    while(max) {
      if(_end_of_buffer()) {
        _in.data_p = _in.cache.get();
        auto bytes_read = _stream.read(_in.data_p, _in.capacity);
        if(bytes_read <= 0) {
          return -1;
        }

        _in.data_end = _in.data_p + bytes_read;
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
  int read_cached(std::uint8_t *in, std::size_t size) {
    while(size) {
      if(_end_of_buffer()) {
        _in.data_p = _in.cache.get();
        auto bytes_read = _stream.read(_in.data_p, _in.capacity);
        if(bytes_read <= 0) {
          return -1;
        }

        _in.data_end = _in.data_p + bytes_read;
      }

      auto cached_bytes = std::min(size, _in.size());
      std::copy_n(_in.data_p, cached_bytes, in);

      _in.data_p += cached_bytes;
      in += cached_bytes;
      size -= cached_bytes;
    }


    return 0;
  }

  /**
   * This is faster if you don't need to process byte by byte
   * @param in the input buffer
   * @param size
   * @return 0 on success
   */
  int read(std::uint8_t *in, std::size_t size) {
    // eachByte could have been called --> some bytes might have been cached
    auto cached_bytes = std::min(size, _in.size());
    if(cached_bytes > 0) {
      std::copy_n(_in.data_p, cached_bytes, in);
      _in.data_p += cached_bytes;

      in += cached_bytes;
      size -= cached_bytes;
    }

    while(size) {
      if(wait_for(READ)) {
        return -1;
      }

      auto bytes_read = _stream.read(in, size);

      /**
       * On EOF --> eof() is true
       */
      if(bytes_read <= 0) {
        return -1;
      }

      size -= bytes_read;
      in += bytes_read;
    }

    return 0;
  }

  template<class T>
  FD &append(T &&container) {
    AppendFunc<T>::run(_out, std::forward<T>(container));

    return *this;
  }

  FD &write_clear() {
    _out.clear();

    return *this;
  }

  FD &read_clear() {
    _in.data_end = _in.data_p = _in.cache.get();

    return *this;
  }

  buffer_in_t &get_read_cache() {
    return _in;
  }

  std::vector<std::uint8_t> &get_write_cache() {
    return _out;
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

  /**
   * Wait for READ/WRITE
   * @param mode either READ or WRITE
   * @return non-zero on timeout or error
   */
  int wait_for(const int mode) {
    if(_timeout <= 0ms) {
      return 0;
    }

    return _stream.select(_timeout, mode);
  }

private:
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

  auto err = io.read_cached(buf, data_len);
  if(err) {
    return std::nullopt;
  }

  auto *val = (T*)buf;
  return *val;
}

template<class T>
std::optional<std::string> read_string(FD<T> &io, std::size_t size) {
  std::string buf; buf.resize(size);

  auto err = io.read_cached((std::uint8_t*)buf.data(), size);

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

struct __test_stream_has_lock {
  template<class P>
  static constexpr auto lock(P *p) -> std::decay_t<decltype(p->get_lock(), std::true_type())>;

  template<class>
  static constexpr auto lock(...) -> std::false_type;
};

template<class T>
static constexpr bool __has_lock_v = std::decay_t<decltype(__test_stream_has_lock::template lock<T>(nullptr))>::value;
}
/*
 * First clear file, then recursively print all params
 */
template<class Stream, class... Args>
int print(file::FD<Stream> &file, Args && ... params) {
  assert(file.is_open());

  if constexpr (file::__has_lock_v<Stream>) {
    std::lock_guard lg(file.getStream().get_lock());
    file.write_clear();

    (file.append(std::forward<Args>(params)),...);

    return file.out();
  }
  else {
    file.write_clear();

    (file.append(std::forward<Args>(params)), ...);

    return file.out();
  }
}
#endif

