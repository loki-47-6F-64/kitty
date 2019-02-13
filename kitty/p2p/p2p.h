//
// Created by loki on 8-2-19.
//

#ifndef T_MAN_QUEST_H
#define T_MAN_QUEST_H

#include <map>
#include <variant>
#include <nlohmann/json.hpp>
#include <kitty/file/io_stream.h>
#include <kitty/server/server.h>
#include <kitty/p2p/pj/ice_trans.h>
#include <kitty/p2p/pj/pool.h>
#include <kitty/p2p/uuid.h>
#include <kitty/p2p/p2p_stream.h>

namespace p2p {
class quest_t {
public:
  using accept_cb = std::function<bool(uuid_t)>;

  quest_t(accept_cb &&pred, const uuid_t &uuid);

  /**
   * register node on the network
   * @return non-zero on failure or timeout
   */
  int send_register();

  /**
   * process the quest.
   * quest["invite"] --> extract remote candidates and call accept predicate
   * quest["accept|decline"] --> transparent from the server perspective
   *                         --> callback from file::p2p initiating the connection
   * @return result < 0  on error
   */
  std::variant<int, file::p2p> process_quest();

  /**
   * Invite recipient to connect
   * @param uuid the recipient
   * @return the connection
   */
  file::p2p invite(uuid_t recipient);
private:
  /**
   * start ICE
   * @param remote The remote candidates
   * @return the connection
   */
  file::p2p _send_accept(uuid_t uuid, const pj::remote_buf_t &remote);

  /**
   * Extract the remote candidates from the package
   * @param remote_j the json object
   * @return the remote candidates on success
   */
  std::optional<pj::remote_buf_t> _process_invite(uuid_t uuid, const nlohmann::json &remote_j);

  /**
   * Handles connecting to the invitee
   * @param remote_j the remote candidates
   */
  void _handle_accept(uuid_t uuid, const nlohmann::json &remote_j);

  /**
   * Allocate a transport
   * @param uuid the recipient of the invitation
   * @param on_data
   * @param on_create callback when candidates are gathered
   * @param on_connect callback when finished negotiating a connection
   * @return true on successfully allocating a transport
   */
  bool _peer_create(const uuid_t &uuid,
                   pj::Pool::on_data_f &&on_data,
                   pj::Pool::on_ice_create_f &&on_create,
                   pj::Pool::on_connect_f &&on_connect);

  /**
   * remove a transport from peers
   * @param uuid The uuid of the transport
   */
  void _peer_remove(const uuid_t &uuid);

  std::shared_ptr<pj::ICETrans> _peer(const uuid_t &uuid);

  void _send_error(const std::string_view &err_str);
public:
  file::io server;

  pj::Pool pool;

  std::thread auto_run_thread;
  util::AutoRun<void> auto_run;

  uuid_t uuid;

  std::vector<uuid_t> vec_peers;
private:
  accept_cb _accept_cb;

  /**
   * The maximum peers connected at any time
   */
  std::size_t _max_peers;

  std::map<uuid_t, std::shared_ptr<pj::ICETrans>> _peers;
};

struct config_t {
  constexpr static std::size_t MAX_PEERS = 16;

  const char *log_file {};
  pj::ip_addr_t server_addr { "192.168.0.115", 2345 };
  pj::ip_addr_t stun_addr   { "stun.l.google.com", 19302 };
  std::vector<std::string_view> dns { "8.8.8.8" };

  std::size_t max_peers = MAX_PEERS;

  std::chrono::milliseconds poll_tm { 50 };

  uuid_t uuid {};
};
extern config_t config;

int init();
}

namespace server {

struct peer_t {
  using member_t = ::p2p::quest_t;

  std::unique_ptr<file::p2p> socket;
//  p2p::pj::ip_addr_t ip_addr;
};

using p2p = Server<peer_t, const file::ip_addr_t &>;
}

#endif //T_MAN_QUEST_H
