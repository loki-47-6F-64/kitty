#include <unistd.h>

#include <openssl/ssl.h>
#include <kitty/ssl/ssl_stream.h>
#include <kitty/ssl/ssl.h>

namespace file {
namespace stream {
ssl::ssl() : _eof(false), _ssl(nullptr) { }

void ssl::open(::ssl::Context &ctx, int fd) {
  _ssl = _SSL { SSL_new(ctx.get()) };
  
  if(!_ssl) {
    err::code = err::LIB_SSL;
    
    return;
  }
  
  SSL_set_fd(_ssl.get(), fd);
}

void ssl::operator=(ssl&& stream) {
  std::swap(_eof,stream._eof);
  
  _ssl = std::move(stream._ssl);
}

int ssl::operator>>(std::vector<unsigned char>& buf) {
  int bytes_read;

  if((bytes_read = SSL_read(_ssl.get(), buf.data(), buf.size())) < 0) {
    return -1;
  }
  else if(!bytes_read) {
    _eof = true;
  }

  // Make sure all bytes are read
  int pending = SSL_pending(_ssl.get());

  // Update number of bytes in buf
  buf.resize(bytes_read + pending);
  if(pending) {
    if((bytes_read = SSL_read(_ssl.get(), &buf.data()[bytes_read], buf.size() - bytes_read)) < 0) {
      return -1;
    }
    else if(!bytes_read) {
      _eof = true;
    }
  }

  return 0;
}

int ssl::operator<<(std::vector<unsigned char>&buf) {
  return SSL_write(_ssl.get(), buf.data(), buf.size());
}

void ssl::seal() {
  // fd is stored in _ssl
  int _fd = fd();

  _ssl.reset();

  if(_fd != -1)
    ::close(_fd);
}

int ssl::fd() {
  return SSL_get_fd(_ssl.get());
}

bool ssl::is_open() {
  return _ssl != nullptr;
}

bool ssl::eof() {
  return _eof;
}

}
}