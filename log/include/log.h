#ifndef DOSSIER_LOG_H
#define DOSSIER_LOG_H

#include <ctime>
#include <vector>
#include <string>
#include <mutex>

#include "io_stream.h"

#ifdef VIKING_DEBUG
#define ON_DEBUG( x ) x                                                                                                                 
#define DEBUG_LOG( ... ) print(debug, __FILE__, ':', __LINE__,':', __VA_ARGS__)                                                         
#else                                                                                                                                     
#define ON_DEBUG( ... )                                                                                                                 
#define DEBUG_LOG( ... ) do {} while(0)                                                                                                   
#endif

namespace file {
namespace stream {
constexpr int DATE_BUFFER_SIZE = 21 + 1; // Full string plus '\0'

class _LogBase {
public:
  static std::mutex _lock;
  static char _date[DATE_BUFFER_SIZE];
};

template<class Stream>
class Log : public _LogBase {
  Stream _stream;

  std::string _prepend;
public:

  Log() { }

  void operator =(Log&& stream) {
    _stream = std::forward(stream);
  }

  template<class... Args>
  void open(std::string&& prepend, Args&&... params) {
    _prepend = std::move(prepend);
    _stream.open(std::forward<Args>(params)...);
  }

  int operator >>(std::vector<unsigned char>& buf) {
    return 0;
  }

  int operator <<(std::vector<unsigned char>& buf) {
    std::lock_guard<std::mutex> lock(_LogBase::_lock);

    std::time_t t = std::time(NULL);
    strftime(_LogBase::_date, DATE_BUFFER_SIZE, "[%Y:%m:%d:%H:%M:%S]", std::localtime(&t));

    buf.insert(buf.begin(), _prepend.cbegin(), _prepend.cend());

    buf.insert(
            buf.begin(),
            _LogBase::_date,
            _LogBase::_date + DATE_BUFFER_SIZE - 1 //omit '\0'
            );

    buf.push_back('\n');
    return _stream << buf;
  }

  bool is_open() {
    return _stream.is_open();
  }

  bool eof() {
    return false;
  }

  int access(const char *path, std::function<int(Stream &, const char *)> open) {
    return open(_stream, path);
  }

  void seal() {
    _stream.seal();
  }

  int fd() {
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
