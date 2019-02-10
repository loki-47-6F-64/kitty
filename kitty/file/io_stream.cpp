#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <kitty/file/io_stream.h>
#include <kitty/file/file.h>
#include <kitty/err/err.h>
#include "io_stream.h"


namespace file {
io ioRead(const char *file_path) {
  return file::io { std::chrono::seconds(0), ::open(file_path, O_RDONLY, 0) };
}

io ioWrite(const char *file_path) {
  int _fd = ::open(file_path,
    O_CREAT | O_WRONLY,
    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH
  );

  return file::io { std::chrono::seconds(0), _fd };
}

io ioWriteAppend(const char *file_path) {
  int _fd = ::open(file_path,
    O_CREAT | O_APPEND | O_WRONLY,
    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH
  );

  return io { std::chrono::seconds(0), _fd };
}

namespace stream {
io::io() : _eof(false), _fd(-1)  { }
io::io(int fd) : _eof(false), _fd(fd) {
  if(fd <= 0) {
    err::code = err::LIB_SYS;
  }
}

io& io::operator=(io&& stream) noexcept {
  std::swap(this->_fd, stream._fd);
  std::swap(this->_eof, stream._eof);

  return *this;
}

int io::read(std::vector<unsigned char> &buf) {
  ssize_t bytes_read;

  if((bytes_read = ::read(_fd, buf.data(), buf.size())) < 0) {
    err::code = err::LIB_SYS;
    return -1;
  }
  else if(!bytes_read) {
    _eof = true;
  }

  // Update number of bytes in buf
  buf.resize((std::size_t)bytes_read);
  return 0;
}

int io::write(const std::vector<unsigned char> &buf) {
  ssize_t bytes_written = ::write(_fd, buf.data(), buf.size());

  if(bytes_written < 0) {
    err::code = err::LIB_SYS;

    return -1;
  }

  return (int)bytes_written;
}

void io::seal() {
  close(_fd);
  _fd = -1;
}

int io::fd() const {
  return _fd;
};

bool io::is_open() const {
  return _fd != -1;
}

bool io::eof() const {
  return _eof;
}

io::io(io &&other) noexcept {
  _fd = other._fd;
  other._fd = -1;
}
}
}
