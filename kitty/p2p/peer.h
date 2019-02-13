//
// Created by loki on 12-2-19.
//

#ifndef T_MAN_PEER_H
#define T_MAN_PEER_H

#include <memory>

#include <kitty/server/server.h>
#include <kitty/p2p/pj/ice_trans.h>
#include <kitty/p2p/p2p_stream.h>


namespace server {

struct peer_t {
  struct member_t {
    file::io server;
  };

  std::unique_ptr<file::p2p> socket;

  p2p::pj::ip_addr_t ip_addr;
};

using p2p = Server<peer_t>;
}

#endif //T_MAN_PEER_H
