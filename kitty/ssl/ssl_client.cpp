#include <optional>
#include <memory>

#include <kitty/ssl/ssl_client.h>
#include <kitty/ssl/ssl.h>
#include <kitty/util/utility.h>

namespace server {
template<>
int ssl::_poll() {
  pollfd &pfd = _member.listenfd;

  if(auto result = poll(&pfd, 1, 100); result != 0) {
    if(pfd.revents & POLLIN) {
      return 1;
    }

    if(result < 0) {
      err::code = err::LIB_SYS;

      return -1;
    }
  }

  return 0;
}

template<>
std::variant<err::code_t, ssl::client_t> ssl::_accept() {
  int result;
  if((result = _poll()) <= 0) {
    if(result < 0) {
      return err::code;
    }

    return err::TIMEOUT;
  }


  auto family = _member.family;

  sockaddr_storage client_addr {};
  socklen_t sockaddr_size = family == AF_INET ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);

  int client_fd = accept(_member.listenfd.fd, (sockaddr *)& client_addr, &sockaddr_size);

  if (client_fd < 0) {
    err::code = err::LIB_SSL;

    return err::code;
  }

  char ip_buf[INET6_ADDRSTRLEN];

  if(family == AF_INET) {
    auto temp_addr = (sockaddr_in*)&client_addr;
    inet_ntop(AF_INET, &temp_addr->sin_addr, ip_buf, INET_ADDRSTRLEN);
  }
  else {
    auto temp_addr = (sockaddr_in6*)&client_addr;
    inet_ntop(AF_INET6, &temp_addr->sin6_addr, ip_buf, INET6_ADDRSTRLEN);
  }

  file::ssl socket = ::ssl::accept(_member.ctx, client_fd);
  if(!socket.is_open()) {
    print(info, "Couldn't accept ssl socket: ", err::current());
    return err::OK;
  }

  return client_t {
    std::make_unique<file::ssl>(std::move(socket)),
    ip_buf
  };
}

template<>
int ssl::_init_listen(const sockaddr *const server) {
  auto family = server->sa_family;
  socklen_t sockaddr_size = family == AF_INET ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);

  pollfd pfd {
    // A lack of sock_nonblock causes epoll error on poll/epoll/accept
    socket(family, SOCK_STREAM, 0),
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

  if(bind(pfd.fd, server, sockaddr_size) < 0) {
    err::code = err::LIB_SYS;
    return -1;
  }

  if(listen(pfd.fd, 128) < 0) {
    err::code = err::LIB_SYS;
    return -1;
  }

  _member.family   = family;
  _member.listenfd = pfd;

  return 0;
}

template<>
void ssl::_cleanup() {
  close(_member.listenfd.fd);
}
}