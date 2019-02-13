#include <unistd.h>
#include <sys/socket.h>

#include <netdb.h>
#include <cstring>

#include <kitty/file/tcp.h>
#include <kitty/err/err.h>
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
  char port[std::numeric_limits<std::uint16_t>::digits10 +2];
  char addr[MAX_HOSTNAME_LEN +1];

  std::sprintf(port, "%hu", ip_addr.port);
  if(ip_addr.ip.size() >= MAX_HOSTNAME_LEN) {
    return io { };
  }

  addr[ip_addr.ip.size()] = '\0';
  ip_addr.ip.copy(addr, ip_addr.ip.size());

  return connect(addr, port);
}
}
