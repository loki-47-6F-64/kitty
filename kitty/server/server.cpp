#include <memory>
#include <variant>

#include <kitty/server/server.h>
#include <kitty/util/utility.h>

namespace server {
using namespace std::literals;

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
    std::make_unique<file::io>(3s, client_fd), {
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

template<>
void udp::_cleanup() {}

template<>
int udp::_init_listen(std::uint16_t port) {
  auto sock = file::udp_init(port);

  if(!sock.is_open()) {
    return -1;
  }

  if(!port) {
    port = file::sockport(sock);
  }

  auto ips = file::get_broadcast_ips(port);
  if(ips.empty()) {
    if(err::code == err::OK) {
      err::set("Couldn't get candidate ip's");
    }

    return -1;
  }

  _member = Multiplex(std::make_shared<__multiplex_shared>(std::move(sock)), std::move(ips.front()));

  return 0;
}

template<>
int udp::_poll() {
  if(auto result = _member.mux().wait_for(file::READ)) {
    if(result == err::TIMEOUT) {
      return 0;
    }

    return -1;
  }

  return 1;
}

template<>
std::variant<err::code_t, udp::client_t> udp::_accept() {
  auto &sock = _member.mux();

  //TODO: reduce timeout to 0
  sockaddr_in6 addr;
  addr.sin6_family = file::INET6;

  auto data = sock.next_batch((sockaddr*)&addr);
  auto ip_addr = file::ip_addr_buf_t::from_sockaddr((sockaddr*)&addr);

  if(!data) {
    print(error, "Couldn't read multiplexed socket ", ip_addr.ip, ':', ip_addr.port, ": ", err::current());
    return err::code;
  }

  print(info, "received [", data->size(), "] bytes from demultiplexed socket ", ip_addr.ip, ':', ip_addr.port);

  TUPLE_2D(pipe, new_item, _member.demux(ip_addr));
  pipe->push(*data);

  if(!new_item) {
    return err::OK;
  }

  return client_t { file::demultiplex { 3s, pipe } };
}

Multiplex &get_multiplex(udp &server) {
  return server.get_member();
}
}
