#ifndef DOSSIER_LOG_H
#define DOSSIER_LOG_H

#include <ctime>
#include <vector>
#include <string>
#include <mutex>

#include <kitty/file/io_stream.h>
#include <kitty/util/thread_local.h>

#ifdef KITTY_DEBUG
#define ON_DEBUG( x ) x                                                                                                                 
#define DEBUG_LOG( ... ) print(debug, __FILE__, ':', __LINE__,':', __VA_ARGS__)
#define KITTY_DEBUG_LOG( ... ) print(debug, __FILE__, ':', __LINE__,':', __VA_ARGS__)
#else                                                                                                                                     
#define ON_DEBUG( ... )                                                                                                                 
#define DEBUG_LOG( ... ) do {} while(0)
#define KITTY_DEBUG_LOG( ... ) do {} while(0)
#endif

namespace file {
namespace stream {
constexpr int DATE_BUFFER_SIZE = 21 +1 +1; // Full string plus " \0"

extern THREAD_LOCAL util::ThreadLocal<char[DATE_BUFFER_SIZE]> _date;

template<class Stream>
class Log {
  Stream _stream;

  std::string _prepend;
public:

  Log() = default;
  Log(Log &&other) noexcept = default;

  Log &operator =(Log&& stream) noexcept = default;

  template<class... Args>
  Log(std::string&& prepend, Args&&... params) : _stream(std::forward<Args>(params)...), _prepend(std::move(prepend)) {}

  int read(std::uint8_t*, std::size_t) {
    return -1;
  }

  int write(std::uint8_t *data, std::size_t size) {
    std::time_t t = std::time(nullptr);
    strftime(_date, DATE_BUFFER_SIZE, "[%Y:%m:%d:%H:%M:%S] ", std::localtime(&t));

    _stream.write((std::uint8_t*)_prepend.data(), _prepend.size());
    _stream.write((std::uint8_t*)(&*_date), DATE_BUFFER_SIZE - 1);

    auto bytes_written =  _stream.write(data, size);

    std::uint8_t nl { '\n' };
    _stream.write(&nl, 1);

    return bytes_written;
  }

  bool is_open() const {
    return _stream.is_open();
  }

  bool eof() const {
    return false;
  }

  void seal() {
    _stream.seal();
  }

  int select(std::chrono::milliseconds to, const int mode) const {
    return _stream.select(to, mode);
  }

  int fd() const {
    return _stream.fd();
  }
};

}
extern void log_open(const char *logPath);

typedef FD<stream::Log<stream::io>> Log;
}

extern file::Log error;
extern file::Log warning;
extern file::Log info;
extern file::Log debug;
#endif
