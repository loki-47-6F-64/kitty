#include <unistd.h>
#include <sys/socket.h>

#include <netdb.h>

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
}
