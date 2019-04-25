#ifndef KITTY_TCP_H
#define KITTY_TCP_H

#include <arpa/inet.h>
#include <kitty/file/io_stream.h>

namespace file {
constexpr sa_family_t INET  = AF_INET;
constexpr sa_family_t INET6 = AF_INET6;

struct ip_addr_buf_t;
struct ip_addr_t {
  std::string_view ip;
  std::uint16_t port;

  std::tuple<std::uint32_t, std::uint16_t> pack() const;
  static ip_addr_t unpack(std::vector<char> &buf, std::tuple<std::uint32_t, std::uint16_t>);

  static ip_addr_t from_sockaddr(std::vector<char> &buf, const sockaddr *);
  std::optional<sockaddr_storage> to_sockaddr() const;

  bool operator==(const file::ip_addr_t &r) const {
    return ip == r.ip && port == r.port;
  }

  bool operator==(const file::ip_addr_buf_t &r) const;

  bool operator<(const file::ip_addr_t &r) const {
    return port < r.port || (port == r.port && ip < r.ip);
  }

  bool operator<(const file::ip_addr_buf_t &r) const;
};

struct ip_addr_buf_t {
  std::string ip;
  std::uint16_t port;

  static ip_addr_buf_t unpack(std::tuple<std::uint32_t, std::uint16_t>);
  std::tuple<std::uint32_t, std::uint16_t> pack() const {
    return ((ip_addr_t)*this).pack();
  }

  static ip_addr_buf_t from_ip_addr(const ip_addr_t &ip_addr);
  static ip_addr_buf_t from_sockaddr(const sockaddr *);
  std::optional<sockaddr_storage> to_sockaddr() const;

  operator ip_addr_t() const {
    return ip_addr_t {
      ip,
      port
    };
  }

  bool operator==(const file::ip_addr_t &r) const {
    return ip == r.ip && port == r.port;
  }

  bool operator==(const file::ip_addr_buf_t &r) const {
    return ip == r.ip && port == r.port;
  }

  bool operator<(const file::ip_addr_buf_t &r) const {
    return port < r.port || (port == r.port && ip < r.ip);
  }

  bool operator<(const file::ip_addr_t &r) const {
    return port < r.port || (port == r.port && ip < r.ip);
  }
};

std::uint16_t sockport(const io &sock);
ip_addr_buf_t peername(const io &sock);

std::vector<file::ip_addr_buf_t> get_broadcast_ips(std::uint16_t port = 0, int family = 0);

io connect(const char *hostname, const char *port);
io connect(const ip_addr_t &ip_addr);

io udp_init(std::optional<std::uint16_t> port = std::nullopt);
int udp_connect(io &sock, const ip_addr_t &ip_addr);

io udp(const ip_addr_t &ip_addr, std::optional<std::uint16_t> port = std::nullopt);
}

namespace std {
template<>
struct hash<file::ip_addr_t> {
  using is_transparent = std::true_type;

  size_t operator()(const file::ip_addr_t &ip) const {
    auto url = std::string { ip.ip.data(), ip.ip.size() } + to_string(ip.port);

    return hash<decltype(url)>()(url);
  }
};

template<>
struct hash<file::ip_addr_buf_t> {
  using is_transparent = std::true_type;

  size_t operator()(const file::ip_addr_t &ip) const {
    return hash<file::ip_addr_t>()(ip);
  }

  size_t operator()(const file::ip_addr_buf_t &ip) const {
    return hash<file::ip_addr_t>()(ip);
  }
};
}
#endif