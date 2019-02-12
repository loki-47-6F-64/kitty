#include <unistd.h>

#include <openssl/ssl.h>
#include <kitty/ssl/ssl_stream.h>
#include <kitty/ssl/ssl.h>

namespace file {
namespace stream {
ssl::ssl() : _eof(false), _ssl(nullptr) { }
ssl::ssl(Context &ctx, int fd) : _eof(false), _ssl(SSL_new(ctx.get())) {  
  if(!_ssl) {
    err::code = err::LIB_SSL;
    
    return;
  }
  
  SSL_set_fd(_ssl.get(), fd);
}

ssl& ssl::operator=(ssl&& stream) {
  std::swap(_eof,stream._eof);
  
  _ssl = std::move(stream._ssl);

  return *this;
}

int ssl::read(std::vector<unsigned char>& buf) {
  int bytes_read;

  if((bytes_read = SSL_read(_ssl.get(), buf.data(), (int)buf.size())) < 0) {
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
    if((bytes_read = SSL_read(_ssl.get(), &buf.data()[bytes_read], (int)buf.size() - bytes_read)) < 0) {
      return -1;
    }
    else if(!bytes_read) {
      _eof = true;
    }
  }

  return 0;
}

int ssl::write(std::vector<unsigned char>&buf) {
  return SSL_write(_ssl.get(), buf.data(), (int)buf.size());
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

bool ssl::is_open() const {
  return _ssl != nullptr;
}

bool ssl::eof() const {
  return _eof;
}

int ssl::select(std::chrono::milliseconds to, const int read) const {
  if(to.count() > 0) {
    auto dur_micro = (suseconds_t)std::chrono::duration_cast<std::chrono::microseconds>(to).count();
    timeval tv {
      0,
      dur_micro
    };

    fd_set selected;

    FD_ZERO(&selected);
    FD_SET(fd(), &selected);

    int result;
    if (read == file::READ) {
      result = ::select(fd() + 1, &selected, nullptr, nullptr, &tv);
    }
    else /*if (read == WRITE)*/ {
      result = ::select(fd() + 1, nullptr, &selected, nullptr, &tv);
    }

    if (result < 0) {
      err::code = err::LIB_SYS;

      if(fd() <= 0) {
        err::code = err::code_t::FILE_CLOSED;
      }

      return -1;
    }

    else if (result == 0) {
      err::code = err::TIMEOUT;
      return -1;
    }
  }

  return err::OK;
}

}
}