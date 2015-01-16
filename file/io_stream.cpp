#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <kitty/file/io_stream.h>
#include <kitty/file/file.h>
#include <kitty/err/err.h>

namespace file {
io ioRead(const char *file_path) {
  return file::io { -1, ::open(file_path, O_RDONLY, 0) };
}

io ioWrite(const char *file_path) {
  int _fd = ::open(file_path,
    O_CREAT | O_WRONLY,
    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH
  );

  return file::io { -1, _fd };
}

io ioWriteAppend(const char *file_path) {
  int _fd = ::open(file_path,
    O_CREAT | O_APPEND | O_WRONLY,
    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH
  );

  return io { -1, _fd };
}  

namespace stream {
io::io() : _eof(false), _fd(-1)  { }

void io::open(int fd) {
  _fd = fd;
}

void io::operator=(io&& stream) {
  this->_fd = stream._fd;
  this->_eof = stream._eof;

  stream._fd = -1;
  stream._eof = true;
}

int io::operator>>(std::vector<unsigned char>& buf) {
  ssize_t bytes_read;

  if((bytes_read = read(_fd, buf.data(), buf.size())) < 0) {
    err::code = err::LIB_SYS;
    return -1;
  }
  else if(!bytes_read) {
    _eof = true;
  }

  // Update number of bytes in buf
  buf.resize(bytes_read);
  return 0;
}

int io::operator<<(std::vector<unsigned char>&buf) {
  int bytes_written = write(_fd, buf.data(), buf.size());

  if(bytes_written < 0) {
    err::code = err::LIB_SYS;

    return -1;
  }

  return 0;
}

void io::seal() {
  close(_fd);
  _fd = -1;
}

int io::fd() {
  return _fd;
};

bool io::is_open() {
  return _fd != -1;
}

bool io::eof() {
  return _eof;
}
}
}
