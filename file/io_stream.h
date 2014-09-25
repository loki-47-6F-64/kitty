#ifndef IO_STREAM_H
#define IO_STREAM_H

#include <kitty/file/file.h>

namespace file {
namespace stream {
class io {
  bool _eof;
  int _fd;

public:
  io();

  void operator =(io&& stream);
  void open(int fd);

  int operator>>(std::vector<unsigned char>& buf);
  int operator<<(std::vector<unsigned char>& buf);

  bool is_open();
  bool eof();

  void seal();

  int fd();
};
}
typedef FD<stream::io> io;

io ioRead (const char *file_path);
io ioWrite(const char *file_path);
io ioWriteAppend(const char *file_path);
}

#endif