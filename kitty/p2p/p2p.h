//
// Created by loki on 8-2-19.
//

#ifndef T_MAN_QUEST_H
#define T_MAN_QUEST_H

#include <map>
#include <variant>
#include <kitty/file/io_stream.h>
#include <kitty/p2p/pj/ice_trans.h>
#include <kitty/p2p/pj/pool.h>
#include <kitty/p2p/uuid.h>

namespace p2p {
struct config_t {
  constexpr static std::size_t MAX_PEERS = 1;

  const char *log_file {};
  pj::ip_addr_t server_addr { "192.168.0.115", 2345 };
  pj::ip_addr_t stun_addr   { "stun.l.google.com", 19302 };
  std::vector<std::string_view> dns { "8.8.8.8" };

  std::size_t max_peers = MAX_PEERS;

  uuid_t uuid {};
};
extern config_t config;

int init();
}
#endif //T_MAN_QUEST_H
