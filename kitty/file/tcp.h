#ifndef KITTY_TCP_H
#define KITTY_TCP_H

#include <arpa/inet.h>
#include <kitty/file/io_stream.h>

namespace file {
constexpr sa_family_t INET  = AF_INET;
constexpr sa_family_t INET6 = AF_INET6;

struct ip_addr_t {
  std::string_view ip;
  std::uint16_t port;

  std::tuple<std::uint32_t, std::uint16_t> pack() const;
  static ip_addr_t unpack(std::vector<char> &buf, std::tuple<std::uint32_t, std::uint16_t>);

  static ip_addr_t from_sockaddr(std::vector<char> &buf, const sockaddr *);
  std::optional<sockaddr_storage> to_sockaddr() const;
};

struct ip_addr_buf_t {
  std::string ip;
  std::uint16_t port;

  static ip_addr_buf_t unpack(std::tuple<std::uint32_t, std::uint16_t>);
  std::tuple<std::uint32_t, std::uint16_t> pack() const {
    return ((ip_addr_t)*this).pack();
  }

  static ip_addr_buf_t from_sockaddr(const sockaddr *);
  std::optional<sockaddr_storage> to_sockaddr() const;

  operator ip_addr_t() const {
    return ip_addr_t {
      ip,
      port
    };
  }
};

std::vector<file::ip_addr_buf_t> get_broadcast_ips(int family = 0);

io connect(const char *hostname, const char *port);
io connect(const ip_addr_t &ip_addr);

io udp_init(std::optional<std::uint16_t> port = std::nullopt);
int udp_connect(io &sock, const ip_addr_t &ip_addr);

io udp(const ip_addr_t &ip_addr, std::optional<std::uint16_t> port = std::nullopt);
}

#endif