#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include "blue_client.h"
#include "utility.h"
namespace server {
template<>
int bluetooth::_accept(Client& client) {
  Client::_sockaddr client_addr;

  socklen_t addr_size = sizeof (client_addr);
  int client_fd = accept(_listenfd.fd, (sockaddr *) & client_addr, &addr_size);

  if (client_fd < 0) {
    return -1;
  }

  client = Client {
    util::mk_uniq<file::io>(3000 * 1000, client_fd),
      { client_addr.l2_bdaddr, 0 },
      23,
      nullptr,
      Client::LOW
  };

  return 0;
}

template<>
int bluetooth::_socket() {
  return socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
}
}