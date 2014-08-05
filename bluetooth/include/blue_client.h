#ifndef BLUECLIENT_H
#define BLUECLIENT_H

#include <memory>
#include <bluetooth/l2cap.h>

#include "_bluetooth.h"
#include "ble.h"
#include "server.h"
#include "io_stream.h"

namespace server {

struct BlueClient {
  typedef sockaddr_l2 _sockaddr;

  std::unique_ptr<file::io> socket;
  bt::device dev;
  size_t mtu;
  bt::Session *session;
  
  enum {
    LOW = 1,
    MEDIUM,
    HIGH
  } security;
};

typedef Server<BlueClient> bluetooth;
}

#endif