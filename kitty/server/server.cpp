#include <kitty/server/server.h>
#include <kitty/util/utility.h>
namespace server {
template<>
std::optional<tcp::client_t> tcp::_accept() {
  sockaddr_in6 client_addr {};
  socklen_t addr_size {sizeof (client_addr)};

  int client_fd = accept(_member.listenfd.fd, (sockaddr *) & client_addr, &addr_size);

  if (client_fd < 0) {
    return std::nullopt;
  }

  char ip_buf[INET6_ADDRSTRLEN];
  inet_ntop(AF_INET6, &client_addr.sin6_addr, ip_buf, INET6_ADDRSTRLEN);

  return client_t {
    util::mk_uniq<file::io>(std::chrono::seconds(3), client_fd), {
      ip_buf
    }
  };
}

template<>
int tcp::_init_listen() {
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
int tcp::_poll() {
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
void tcp::_cleanup() {
  close(_member.listenfd.fd);
}

util::ThreadPool &tasks() {
  static util::ThreadPool tasks { 1 };

  return tasks;
}
};