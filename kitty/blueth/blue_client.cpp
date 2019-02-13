#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include <kitty/blueth/blue_client.h>
#include <kitty/util/utility.h>

namespace server {
template<>
std::variant<err::code_t, bluetooth::client_t> bluetooth::_accept() {
  sockaddr_l2 client_addr {};

  socklen_t addr_size = sizeof(client_addr);
  
  int client_fd = accept(_member.listenfd.fd, (sockaddr *) &client_addr, &addr_size);

  if (client_fd < 0) {
    err::code = err::LIB_SYS;

    return err::code;
  }

  auto dev = bt::device(client_addr.l2_bdaddr, 0);
  return client_t {
    util::mk_uniq<file::blueth>(std::chrono::seconds(3), client_fd, *_member.hci, dev),
    dev,
    23,
    nullptr,
    client_t::LOW
  };
}

template<>
int bluetooth::_init_listen(const sockaddr_l2 &server) {
  pollfd pfd {
    socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP),
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

  if(bind(pfd.fd, (const sockaddr *) &server, sizeof(server)) < 0) {
    err::code = err::LIB_SYS;
    return -1;
  }

  listen(pfd.fd, 1);

  _member.listenfd = pfd;

  return 0;
}

template<>
int bluetooth::_poll() {
  int result {};

  if((result = poll(&_member.listenfd, 1, 100)) > 0) {
    if(_member.listenfd.revents == POLLIN) {
      return 1;
    }

    if(result < 0) {
      err::code = err::LIB_SYS;

      return 0;
    }
  }

  return 0;
}

template<>
void bluetooth::_cleanup() {
  close(_member.listenfd.fd);
}
}