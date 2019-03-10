#include <unistd.h>

#include <openssl/ssl.h>
#include <kitty/file/poll.h>
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

ssl& ssl::operator=(ssl&& stream) noexcept {
  std::swap(_eof,stream._eof);
  
  _ssl = std::move(stream._ssl);

  return *this;
}

int ssl::read(std::uint8_t *data, std::size_t size) {
  if(int bytes_read; (bytes_read = ::SSL_read(_ssl.get(), data, (int)size)) > 0) {
    return bytes_read;
  } else if(!bytes_read) {
    _eof = true;

    return 0;
  }

  err::code = err::LIB_SSL;
  return -1;
}

int ssl::write(std::vector<std::uint8_t>& buf) {
  return SSL_write(_ssl.get(), buf.data(), (int)buf.size());
}

void ssl::seal() {
  // fd is stored in _ssl
  int _fd = fd();

  _ssl.reset();

  if(_fd != -1)
    ::close(_fd);
}

int ssl::fd() const {
  return SSL_get_fd(_ssl.get());
}

bool ssl::is_open() const {
  return _ssl != nullptr;
}

bool ssl::eof() const {
  return _eof;
}

int ssl::select(std::chrono::milliseconds to, const int read) const {
  pollfd pfd;
  pfd.fd = fd();

  if(read == READ) {
    pfd.events = KITTY_POLLIN;
  } else { /* read == WRITE */
    pfd.events = KITTY_POLLOUT;
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
