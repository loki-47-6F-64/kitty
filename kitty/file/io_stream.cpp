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
io::io() : _eof(false), _fd(-1) { }

io::io(int fd) : _eof(false), _fd(fd) {
  if(fd <= 0) {
    err::code = err::LIB_SYS;
  }
}

io &io::operator=(io &&stream) noexcept {
  std::swap(this->_fd, stream._fd);
  std::swap(this->_eof, stream._eof);

  return *this;
}

std::int64_t io::read(std::uint8_t *data, std::size_t size) {
  if(ssize_t bytes_read; (bytes_read = ::read(_fd, data, size)) > 0) {
    return bytes_read;
  } else if(!bytes_read) {
    _eof = true;

    return 0;
  }

  err::code = err::LIB_SYS;
  return -1;
}

int io::write(const std::vector<unsigned char> &buf) {
  ssize_t bytes_written = ::write(_fd, buf.data(), buf.size());

  if(bytes_written < 0) {
    err::code = err::LIB_SYS;

    return -1;
  }

  return (int) bytes_written;
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

int io::select(std::chrono::milliseconds to, const int read) const {
  pollfd pfd;
  pfd.fd = fd();

  if(read == READ) {
    pfd.events = POLLIN | POLLRDHUP;
  } else { /* read == WRITE */
    pfd.events = POLLOUT;
  }

  auto res = poll(&pfd, 1, (int) to.count());

  if(res > 0) {
    if(pfd.revents & (POLLHUP | POLLRDHUP | POLLERR)) {
      err::code = err::code_t::FILE_CLOSED;

      return -1;
    }

    if(pfd.revents & (POLLIN | POLLOUT)) {
      return err::OK;
    }
  }

  if(res < 0) {
    err::code = err::LIB_SYS;

    return -1;
  }

  err::code = err::TIMEOUT;
  return err::TIMEOUT;
}
}
}
