#ifndef BLUECLIENT_H
#define BLUECLIENT_H

#include <memory>
#include <bluetooth/l2cap.h>

#include "blueth.h"
#include "ble.h"
#include "server/server.h"
#include "file/io_stream.h"

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