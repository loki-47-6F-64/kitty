#include <unistd.h>
#include <sys/socket.h>

#include <netdb.h>
#include <cstring>

#include <kitty/file/tcp.h>

namespace err {
extern void set(const char *err);
}

namespace file {
io connect(const char *hostname, const char *port) {
  constexpr long timeout = -1;

  io ioFd;
  int serverFd = socket(AF_INET, SOCK_STREAM, 0);

  addrinfo hints;
  addrinfo *server;

  std::memset(&hints, 0, sizeof (hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  int err;
  if((err = getaddrinfo(hostname, port, &hints, &server))) {
    err::set(gai_strerror(err));
    return ioFd;
  }


  if(connect(serverFd, server->ai_addr, server->ai_addrlen)) {
    freeaddrinfo(server);

    err::code = err::LIB_SYS;
    return ioFd;
  }

  freeaddrinfo(server);
  
  ioFd = io(timeout, serverFd);
  
  return ioFd;
}
}