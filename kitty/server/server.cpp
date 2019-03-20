#include <memory>
#include <variant>

#include <kitty/server/server.h>
#include <kitty/util/utility.h>

namespace server {
template<>
int tcp::_poll() {
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
std::variant<err::code_t, tcp::client_t> tcp::_accept() {
  auto family = _member.family;

  sockaddr_storage client_addr {};
  socklen_t sockaddr_size = family == AF_INET ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);

  int client_fd = accept(_member.listenfd.fd, (sockaddr *)& client_addr, &sockaddr_size);

  if (client_fd < 0) {
    err::code = err::LIB_SYS;

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

  //TODO: Use ip_addr_t
  return client_t {
    std::make_unique<file::io>(std::chrono::seconds(3), client_fd), {
      ip_buf
    }
  };
}

template<>
int tcp::_init_listen(const sockaddr *const server) {
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
void tcp::_cleanup() {
  close(_member.listenfd.fd);
}

util::ThreadPool &tasks() {
  static util::ThreadPool tasks { 1 };

  return tasks;
}
};
