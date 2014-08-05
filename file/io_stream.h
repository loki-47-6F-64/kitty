#ifndef IO_STREAM_H
#define IO_STREAM_H

#include "file.h"

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

int ioRead(io& fs, const char *file_path);

int ioWriteAppend(io& fs, const char *file_path);
int ioWrite(io& fs, const char *file_path);
}

typedef FD<stream::io> io;
}

#endif