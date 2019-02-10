#ifndef IO_STREAM_H
#define IO_STREAM_H

#include <string>
#include <kitty/file/file.h>
namespace file {
namespace stream {
class io {
  bool _eof;
  int _fd;

public:
  io();
  explicit io(int fd);

  io(io &&) noexcept;
  io& operator =(io&& stream) noexcept;

  int read(std::vector<unsigned char>& buf);
  int write(const std::vector<unsigned char> &buf);

  bool is_open() const;
  bool eof() const;

  void seal();

  int fd() const;
};
}
typedef FD<stream::io> io;

io ioRead(const char *file_path);
io ioWrite(const char *file_path);
io ioWriteAppend(const char *file_path);
}

#endif