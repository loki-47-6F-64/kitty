#ifndef BLUECLIENT_H
#define BLUECLIENT_H

#include <memory>
#include <bluetooth/l2cap.h>

#include <kitty/blueth/blueth.h>
#include <kitty/blueth/ble.h>
#include <kitty/file/io_stream.h>
#include <kitty/server/server.h>
#include "blue_stream.h"

namespace server {

struct blue_client_t {
  struct member_t {
    sockaddr_l2 sockaddr;
    bt::HCI *hci;

    pollfd listenfd {};
  };

  std::unique_ptr<file::blueth> socket;
  bt::device dev;
  size_t mtu;
  bt::Session *session;
  
  enum {
    LOW = 1,
    MEDIUM,
    HIGH
  } security;
};

typedef Server<blue_client_t> bluetooth;
}

#endif