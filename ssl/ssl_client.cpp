#include "ssl_client.h"
#include "_ssl.h"
#include "utility.h"

namespace server {
template<>
int ssl::_accept(Client& client) {
  sockaddr_in6 client_addr;
  socklen_t addr_size {sizeof (client_addr)};

  int client_fd = accept(_listenfd.fd, (sockaddr *) & client_addr, &addr_size);

  if (client_fd < 0) {
    return -1;
  }

  char ip_buf[INET6_ADDRSTRLEN];
  inet_ntop(AF_INET6, &client_addr.sin6_addr, ip_buf, INET6_ADDRSTRLEN);


  file::ssl socket = ::ssl::accept(_member, client_fd);
  if(!socket.is_open()) {
    return -1;
  }
  
  client = Client {
    util::mk_uniq<file::ssl>(std::move(socket)),
    ip_buf
  };
  
  return 0;
}

template<>
int ssl::_socket() {
  return socket(AF_INET6, SOCK_STREAM, 0);
}
}