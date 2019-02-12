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
void send_invite(file::io &server, uuid_t uuid);
void send_invite(uuid_t uuid);

bool peer_create(const uuid_t &uuid, pj::Pool::on_data_f &&on_data, pj::Pool::on_ice_create_f &&on_create, pj::Pool::on_connect_f &&on_connect);
void peer_remove(const uuid_t &uuid);

std::optional<uuid_t> peer();
struct config_t {
  constexpr static std::size_t MAX_PEERS = 1;

  const char *log_file {};
  pj::ip_addr_t server_addr { "192.168.0.115", 2345 };
  pj::ip_addr_t stun_addr   { "stun.l.google.com", 19302 };
  std::vector<std::string_view> dns { "8.8.8.8" };

  std::size_t max_peers = MAX_PEERS;

  std::chrono::milliseconds poll_tm { 50 };

  uuid_t uuid {};
};
extern config_t config;

pj::ICETrans ice_trans(pj::Pool::on_data_f &&on_data, pj::Pool::on_ice_create_f &&on_create, pj::Pool::on_connect_f &&on_connect);
/**
 * Poll for incoming data
 */
void poll();
int init();
}
#endif //T_MAN_QUEST_H
