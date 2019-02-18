//
// Created by loki on 8-2-19.
//

#ifndef T_MAN_QUEST_H
#define T_MAN_QUEST_H

#include <map>
#include <variant>
#include <kitty/file/io_stream.h>
#include <kitty/server/server.h>
#include <kitty/p2p/pj/ice_trans.h>
#include <kitty/p2p/pj/pool.h>
#include <kitty/p2p/uuid.h>
#include <kitty/p2p/p2p_stream.h>

namespace p2p {
struct decline_t {
  enum : pj::status_t {
    DECLINE = PJ_ERRNO_START_SYS +1,
    ERROR
  } reason;
  std::string_view msg;
};

class quest_t {
public:
  using accept_cb = std::function<std::optional<decline_t>(uuid_t)>;

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
   * @return
   *    std::holds_alternative<decline_t> -- The reason for declining
   *    std::holds_alternative<file::p2p> -- The connection
   */
  std::variant<decline_t, file::p2p> invite(uuid_t recipient);
private:
  /**
   * start ICE
   * It will call _send_decline on error
   * @param remote The remote candidates
   * @return the connection
   */
  file::p2p _send_accept(uuid_t recipient, const pj::remote_buf_t &remote);

  /**
   * Notify inviter that you're declining connection
   * @param recipient
   */
  void _send_decline(uuid_t recipient, decline_t decline);

  /**
   * Handles connecting to the invitee
   * @param sender the invitee
   * @param remote the remote candidates
   */
  void _handle_accept(uuid_t sender, const pj::remote_buf_t &remote);

  /**
   * Handles declining the connection
   * @param recipient the invitee
   */
  void _handle_decline(uuid_t recipient);

  /**
   * Allocate a transport
   * @param uuid the recipient of the invitation
   * @param alarm the alarm to ring when finished
   * @param on_create callback when candidates are gathered
   * @return an fd on successfully allocating a transport
   */
  std::optional<file::p2p> _peer_create(const uuid_t &uuid, util::Alarm<std::optional<decline_t>> &alarm,
                                        pj::Pool::on_ice_create_f &&on_create);

  /**
   * remove a transport from peers
   * @param uuid The uuid of the transport
   */
  void _peer_remove(const uuid_t &uuid);

  std::shared_ptr<pj::ICETrans> _peer(const uuid_t &uuid);

  void _send_error(uuid_t recipient, const std::string_view &err_str);
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
  pj::ip_addr_t server_addr { "localhost", 2345 };
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
