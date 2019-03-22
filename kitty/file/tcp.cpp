#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <netdb.h>

#include <bitset>
#include <cstring>
#include <charconv>

#include <kitty/file/tcp.h>
#include <kitty/err/err.h>
#include <kitty/util/set.h>
#include <kitty/util/utility.h>
#include "tcp.h"


namespace file {
constexpr auto MAX_HOSTNAME_LEN = 1023;
io connect(const char *hostname, const char *port) {
  constexpr std::chrono::seconds timeout { 3 };
  
  addrinfo hints { 0 };
  addrinfo *server;

  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if(int err = getaddrinfo(hostname, port, &hints, &server)) {
    err::set(gai_strerror(err));
    return {};
  }
  
  io sock { timeout, socket(AF_INET, SOCK_STREAM, 0) };
  
  if(connect(sock.getStream().fd(), server->ai_addr, server->ai_addrlen)) {
    freeaddrinfo(server);
    
    err::code = err::LIB_SYS;
    return {};
  }
  
  freeaddrinfo(server);
  
  return sock;
}

io connect(const ip_addr_t &ip_addr) {
  char port[16];
  char addr[MAX_HOSTNAME_LEN +1];

  std::sprintf(port, "%hu", ip_addr.port);
  if(ip_addr.ip.size() >= MAX_HOSTNAME_LEN) {
    return io { };
  }

  addr[ip_addr.ip.size()] = '\0';
  ip_addr.ip.copy(addr, ip_addr.ip.size());

  return connect(addr, port);
}

std::tuple<std::uint32_t, std::uint16_t> ip_addr_t::pack() const {
  auto bytes = util::map(util::split(ip, '.'), [](auto &byte) {
    return util::from_chars(byte.data(), byte.data() + byte.size());
  });

  return { bytes[3] + (bytes[2] << 8) + (bytes[1] << 16) + (bytes[0] << 24), port };
}

ip_addr_t ip_addr_t::unpack(std::vector<char> &buf, std::tuple<std::uint32_t, std::uint16_t> ip_addr) {
  buf.resize(INET_ADDRSTRLEN);

  auto ip_buf = (std::uint8_t*)&std::get<0>(ip_addr);

  char *ptr = buf.data();
  std::for_each(std::reverse_iterator { ip_buf + 4 }, std::reverse_iterator { ip_buf + 1 }, [&ptr, &buf](std::uint8_t byte) {
    ptr = std::to_chars(ptr, buf.data() + buf.size(), byte).ptr;
    *ptr++ = '.';
  });

  std::size_t size = std::to_chars(ptr, buf.data() + buf.size(), ip_buf[0]).ptr - buf.data();


  return ip_addr_t {
    { buf.data(), size }, std::get<1>(ip_addr)
  };
}

ip_addr_t ip_addr_t::from_sockaddr(std::vector<char> &buf, const sockaddr *const ip_addr) {
  buf.resize(INET6_ADDRSTRLEN);

  auto family = ip_addr->sa_family;
  std::uint16_t port { 0 };
  if(family == AF_INET6) {
    inet_ntop(AF_INET6, &((sockaddr_in6*)ip_addr)->sin6_addr, buf.data(), INET6_ADDRSTRLEN);

    port = ((sockaddr_in6*)ip_addr)->sin6_port;
  }

  if(family == AF_INET) {
    inet_ntop(AF_INET, &((sockaddr_in*)ip_addr)->sin_addr, buf.data(), INET_ADDRSTRLEN);

    port = ((sockaddr_in*)ip_addr)->sin_port;
  }

  return ip_addr_t {
    { buf.data() },
    util::endian::big(port)
  };
}

std::optional<sockaddr_storage> ip_addr_t::to_sockaddr() {
  std::optional<sockaddr_storage> buf;

  char in[INET6_ADDRSTRLEN];
  ip.copy(in, ip.size());
  in[ip.size()] = '\0';

  if(inet_pton(AF_INET, ip.data(), &buf) <= 0) {
    err::code = err::INPUT_OUTPUT;

    return std::nullopt;
  }

  return buf;
}

ip_addr_buf_t ip_addr_buf_t::unpack(std::tuple<std::uint32_t, std::uint16_t> ip_addr) {
  char data[INET_ADDRSTRLEN];

  auto ip_buf = (std::uint8_t*)&std::get<0>(ip_addr);

  char *ptr = data;
  std::for_each(std::reverse_iterator { ip_buf + 4 }, std::reverse_iterator { ip_buf + 1 }, [&ptr, &data](std::uint8_t byte) {
    ptr = std::to_chars(ptr, data +  INET_ADDRSTRLEN, byte).ptr;
    *ptr++ = '.';
  });

  std::size_t size = std::to_chars(ptr, data +  INET_ADDRSTRLEN, ip_buf[0]).ptr - data;

  return ip_addr_buf_t {
    std::string { data, size }, std::get<1>(ip_addr)
  };
}

ip_addr_buf_t ip_addr_buf_t::from_sockaddr(const sockaddr *const ip_addr) {
  char data[INET6_ADDRSTRLEN];

  auto family = ip_addr->sa_family;
  std::uint16_t port { 0 };
  if(family == AF_INET6) {
    inet_ntop(AF_INET6, &((sockaddr_in6*)ip_addr)->sin6_addr, data, INET6_ADDRSTRLEN);

    port = ((sockaddr_in6*)ip_addr)->sin6_port;
  }

  if(family == AF_INET) {
    inet_ntop(AF_INET, &((sockaddr_in*)ip_addr)->sin_addr, data, INET_ADDRSTRLEN);

    port = ((sockaddr_in*)ip_addr)->sin_port;
  }

  return ip_addr_buf_t {
    std::string { data },
    util::endian::big(port)
  };
}

std::optional<sockaddr_storage> ip_addr_buf_t::to_sockaddr() {
  std::optional<sockaddr_storage> buf;

  if(inet_pton(AF_INET, ip.c_str(), &buf) <= 0) {
    err::code = err::INPUT_OUTPUT;

    return std::nullopt;
  }

  return buf;
}

using ifaddr_t = util::safe_ptr<ifaddrs, freeifaddrs>;

ifaddr_t get_ifaddrs() {
  ifaddrs *p { nullptr };

  getifaddrs(&p);

  return ifaddr_t { p };
}

std::vector<file::ip_addr_buf_t> get_broadcast_ips(int family) {
  std::bitset<2> family_f {};

  if(family == 0) {
    family_f[0] = true;
    family_f[1] = true;
  }

  if(family == INET) {
    family_f[0] = true;
  }

  if(family == INET6) {
    family_f[1] = true;
  }


  std::vector<file::ip_addr_buf_t> ip_addrs;

  auto ifaddr = get_ifaddrs();
  for(auto pos = ifaddr.get(); pos != nullptr; pos = pos->ifa_next) {
    if(pos->ifa_flags & IFF_UP && pos->ifa_flags & IFF_BROADCAST) {
      if(
        (family_f[0] && pos->ifa_addr->sa_family == AF_INET) ||
        (family_f[1] && pos->ifa_addr->sa_family == AF_INET6)
        ){

        ip_addrs.emplace_back(file::ip_addr_buf_t::from_sockaddr(pos->ifa_addr));
      }
    }
  }

  return ip_addrs;
};
}
