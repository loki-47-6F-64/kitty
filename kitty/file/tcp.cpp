#include <unistd.h>
#include <sys/socket.h>

#include <netdb.h>
#include <cstring>

#include <kitty/file/tcp.h>
#include <kitty/err/err.h>

namespace file {
io connect(const char *hostname, const char *port) {
  constexpr std::chrono::seconds timeout { 0 };
  
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
}
