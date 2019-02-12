//
// Created by loki on 8-2-19.
//

#ifndef T_MAN_QUEST_H
#define T_MAN_QUEST_H

#include "../../../../../../usr/include/c++/7/map"
#include "../../../../../../usr/include/c++/7/variant"
#include "../../kitty/kitty/file/io_stream.h"
#include "../ice_trans.h"
#include "../pool.h"
#include "../uuid.h"

void send_register(file::io &server);

struct config_t {
  constexpr static std::size_t MAX_PEERS = 1;
  struct {
    file::io fd;
    const char *hostname = "192.168.0.115";
    const char *port = "2345";
  } server;

  std::size_t max_peers = MAX_PEERS;

  std::map<uuid_t, std::shared_ptr<pj::ICETrans>> peers;
  pj::Pool pool;

  file::poll_t<file::io, std::monostate> poll;

  uuid_t uuid;
};
extern config_t config;

#endif //T_MAN_QUEST_H
