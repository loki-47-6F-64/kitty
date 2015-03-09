#ifndef DOSSIER_FILE_H
#define DOSSIER_FILE_H

#include <functional>
#include <vector>

#include <sys/select.h>

#include <type_traits>

#include <kitty/err/err.h>
#include <kitty/util/optional.h>
#include <kitty/util/string.h>

namespace file {
/* Represents file in memory, storage or socket */
template <class Stream>
class FD { /* File descriptor */
  Stream _stream;

  // Change of cacheSize only affects next load
  int _cacheSize;
  
  long _microsec;
  
  std::vector<uint8_t> _cache;
  decltype(_cache.size()) _data_p;

public:
  static constexpr int READ = 0, WRITE = 1;

  FD(FD && other) {
    *this = std::move(other);
  }

  void operator=(FD && other) {
    seal();
    
    _stream = std::move(other._stream);
    _cache = std::move(other._cache);

    std::swap(_data_p, other._data_p);
    std::swap(_microsec, other._microsec);
    std::swap(_cacheSize, other._cacheSize);
  }

  FD(long microsec = -1)
    : _cacheSize(1024), _microsec(microsec), _data_p(0) { }

  template<class... Args>
  FD(long microsec, Args && ... params)
    : _cacheSize(1024), _microsec(microsec), _data_p(0) {
    _stream.open(std::forward<Args>(params)...);
  }

  ~FD() {
    seal();
  }

  Stream &getStream() { return _stream; }
  // Load file into _cache.data(), replaces old _cache.data()

  /* Returns -1 on error.
     Returns  0 on succes
     Returns  1 on timeout */
  int load(size_t max_bytes) {
    if ((_select(READ))) {
      return -1;
    }

    _cache.resize(max_bytes);

    if ((_stream >> _cache) < 0)
      return -1;

    _data_p = 0;
    return err::OK;
  }

  // Write to file

  inline int out() {

    if ((_select(WRITE))) {
      return -1;
    }

    // On success clear
    if ((_stream << _cache) >= 0) {
      clear();
      return err::OK;
    }
    return -1;
  }

  // Useful when fine control is nessesary
  util::Optional<uint8_t> next() {
    // Load new _cache if end of buffer is reached
    if (end_of_buffer()) {
      if (load(_cacheSize)) {
        return util::Optional<uint8_t>();
      }
    }

    // If _cache.empty() return '\0'
    return _cache.empty() ? util::Optional<uint8_t>() : util::Optional<uint8_t>(_cache[_data_p++]);
  }

  template<class Function>
  int eachByte(Function &&f) {
    while (!eof()) {
      if (end_of_buffer()) {

        if ((load(_cacheSize))) {
          return -1;
        }

        continue;
      }

      if (int err = f(_cache[_data_p++])) {
        // Return FileErr::OK if err_code != FileErr::BREAK
        return err != err::BREAK ? -1 : 0;
      }
    }

    return err::OK;
  }

  template<class T>
  FD &append(T &&container) {
    AppendFunc<T>::run(_cache, std::forward<T>(container));

    return *this;
  }

  inline FD &clear() {
    _cache.clear();
    _data_p = 0;


    return *this;
  }

  inline std::vector<uint8_t> &getCache() {
    return _cache;
  }

  inline bool eof() {
    return _stream.eof();
  }

  inline bool end_of_buffer() {
    return _data_p == _cache.size();
  }

  inline bool is_open() {
    return _stream.is_open();
  }

  inline void seal() {
    if (is_open())
      _stream.seal();
  }

  /*
     Copies max bytes from this to out
     If max == -1 copy the whole file
     On failure: return (Error)-1 or (Timeout)1
     On success: return  0
   */
  template<class OutStream>
  int copy(FD<OutStream> &out, int64_t max = -1) {
    auto &cache = out.getCache();

    while (!eof() && max) {
      if (end_of_buffer()) {

        if ((load(_cacheSize))) {
          return -1;
        }
      }

      while (!end_of_buffer()) {
        cache.push_back(_cache[_data_p++]);

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
    if (_microsec >= 0) {
      timeval tv {0, _microsec};

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
        return -1;
      }

      else if (result == 0) {
        err::code = err::TIMEOUT;
        return -1;
      }
    }

    return err::OK;
  }

  template<class T, class S = void>
  struct AppendFunc {
    static void run(std::vector<uint8_t> &cache, T &&container) {
      cache.insert(std::end(cache), std::begin(container), std::end(container));
    }
  };

  template<class T>
  struct AppendFunc<T, typename std::enable_if<std::is_integral<typename std::remove_reference<T>::type>::value>::type> {
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
  struct AppendFunc<T, typename std::enable_if<std::is_pointer<typename std::decay<T>::type>::value>::type> {
    static void run(std::vector<uint8_t> &cache, T pointer) {
      static_assert(sizeof(*pointer) == 1, "pointers Must be const char * or const uint8_t *");

      std::string _pointer { pointer };

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
  return _print(file.append(std::forward<Out&&>(out)), std::forward<Args>(params)...);
}

/*
 * First clear file, then recursively print all params
 */
template<class Stream, class... Args>
int print(file::FD<Stream> &file, Args && ... params) {
  return _print(file.clear(), std::forward<Args>(params)...);
}
#endif

