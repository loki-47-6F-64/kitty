#include <optional>
#include <kitty/ssl/ssl_client.h>
#include <kitty/ssl/ssl.h>
#include <kitty/util/utility.h>

namespace server {
template<>
std::optional<ssl::client_t> ssl::_accept() {
  sockaddr_in6 client_addr {};
  socklen_t addr_size {sizeof (client_addr)};

  int client_fd = accept(_member.listenfd.fd, (sockaddr *) & client_addr, &addr_size);

  if (client_fd < 0) {
    return std::nullopt;
  }

  char ip_buf[INET6_ADDRSTRLEN];
  inet_ntop(AF_INET6, &client_addr.sin6_addr, ip_buf, INET6_ADDRSTRLEN);


  file::ssl socket = ::ssl::accept(_member.ctx, client_fd);
  if(!socket.is_open()) {
    return std::nullopt;
  }
  
  return client_t {
    util::mk_uniq<file::ssl>(std::move(socket)),
    ip_buf
  };
}

template<>
int ssl::_init_listen() {
  pollfd pfd {
    socket(AF_INET6, SOCK_STREAM, 0),
    POLLIN,
    0
  };

  if(pfd.fd == -1) {
    err::code = err::LIB_SYS;
    return -1;
  }

  // Allow reuse of local addresses
  if(setsockopt(pfd.fd, SOL_SOCKET, SO_REUSEADDR, &pfd.fd, sizeof(pfd.fd))) {
    err::code = err::LIB_SYS;
    return -1;
  }

  if(bind(pfd.fd, (const sockaddr *) &_member.sockaddr, sizeof(_member.sockaddr)) < 0) {
    err::code = err::LIB_SYS;
    return -1;
  }

  listen(pfd.fd, 1);

  _member.listenfd = pfd;

  return 0;
}

template<>
int ssl::_poll() {
  int result {};

  if((result = poll(&_member.listenfd, 1, 100)) > 0) {
    if(_member.listenfd.revents == POLLIN) {
      return 1;
    }

    if(result < 0) {
      err::code = err::LIB_SYS;

      return 0;
    }
  }

  return 0;
}

template<>
void ssl::_cleanup() {
  close(_member.listenfd.fd);
}
}