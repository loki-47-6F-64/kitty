#include <unistd.h>

#include <openssl/ssl.h>
#include <kitty/ssl/ssl_stream.h>

namespace file {
namespace stream {
ssl::ssl() : _eof(false), _ssl(nullptr) { }

void ssl::open(SSL *ssl) {
  _ssl = ssl;
}

void ssl::operator=(ssl&& stream) {
  this->_eof =  stream._eof;
  this->_ssl =  stream._ssl;

  stream._ssl = nullptr;
  stream._eof = true;
}

int ssl::operator>>(std::vector<unsigned char>& buf) {
  int bytes_read;

  if((bytes_read = SSL_read(_ssl, buf.data(), buf.size())) < 0) {
    return -1;
  }
  else if(!bytes_read) {
    _eof = true;
  }

  // Make sure all bytes are read
  int pending = SSL_pending(_ssl);

  // Update number of bytes in buf
  buf.resize(bytes_read + pending);
  if(pending) {
    if((bytes_read = SSL_read(_ssl, &buf.data()[bytes_read], buf.size() - bytes_read)) < 0) {
      return -1;
    }
    else if(!bytes_read) {
      _eof = true;
    }
  }

  return 0;
}

int ssl::operator<<(std::vector<unsigned char>&buf) {
  return SSL_write(_ssl, buf.data(), buf.size());
}

void ssl::seal() {
  // fd is stored in _ssl
  int _fd = fd();

  if(_ssl)
    SSL_free(_ssl);
  _ssl = nullptr;

  if(_fd != -1)
    ::close(_fd);
}

int ssl::fd() {
  return SSL_get_fd(_ssl);
}

bool ssl::is_open() {
  return _ssl != nullptr;
}

bool ssl::eof() {
  return _eof;
}

}
}