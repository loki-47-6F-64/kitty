#ifndef BLUECLIENT_H
#define BLUECLIENT_H

#include <memory>
#include <bluetooth/l2cap.h>

#include "blueth.h"
#include "ble.h"
#include "server/server.h"
#include "blue_stream.h"

namespace server {

struct BlueClient {
  typedef sockaddr_l2 _sockaddr;

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

template<>
struct DefaultType<BlueClient> {
  typedef bt::HCI* Type;
};

typedef Server<BlueClient> bluetooth;
}

#endif