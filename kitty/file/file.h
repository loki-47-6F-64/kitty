#ifndef DOSSIER_FILE_H
#define DOSSIER_FILE_H

#include <functional>
#include <vector>
#include <chrono>

#include <sys/select.h>

#include <type_traits>

#include <kitty/err/err.h>
#include <kitty/util/optional.h>
#include <kitty/util/string.h>

namespace file {
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

  ~FD() { seal(); }

  Stream &getStream() { return _stream; }

  // Write to file
  int out() {
    if ((_select(WRITE))) {
      return -1;
    }

    // On success clear
    if ((_stream.write(_out.cache)) >= 0) {
      return err::OK;
    }
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
        cache.push_back(_out.cache[_out.data_p++]);

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
    static void run(std::vector<uint8_t> &cache, T &&container) {
      cache.insert(std::end(cache), std::begin(container), std::end(container));
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

      // TODO: Don't allocate a string ;)
      std::string_view _pointer { pointer };

      cache.insert(cache.end(), _pointer.cbegin(), _pointer.cend());
    }
  };
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

