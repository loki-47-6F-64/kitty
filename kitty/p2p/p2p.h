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
using namespace std::literals;
struct decline_t {
  enum : int {
    DECLINE,
    ERROR,
    INTERNAL_ERROR
  } reason;

  std::string_view to_string_view() {
    switch (reason) {
      case DECLINE:
        return "Peer declined"sv;
      case ERROR:
        return "Peer had an error"sv;
      case INTERNAL_ERROR:
        return err::current();
    }
  }
};

struct accept_t {
  pj::remote_buf_t remote;
};

using answer_t = std::variant<decline_t, accept_t>;

class pending_t {
  pj::ICETrans _ice_trans;
  util::Alarm<decline_t> &_alarm;
public:
  pending_t(pj::ICETrans &&, util::Alarm<decline_t> &) noexcept;

  void operator()(answer_t &&);
};

class quest_t {
public:
  using accept_cb = std::function<std::optional<decline_t>(uuid_t)>;

  quest_t(accept_cb &&pred, const uuid_t &uuid, std::chrono::milliseconds to = 500ms);

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
  void _send_decline(uuid_t recipient, const decline_t &decline);

  /**
   * Handles connecting to the invitee
   * @param sender the invitee
   * @param remote the remote candidates
   */
  void _handle_accept(uuid_t sender, pj::remote_buf_t &&remote);

  /**
   * Handles declining the connection
   * @param recipient the invitee
   */
  void _handle_decline(uuid_t recipient, decline_t decline);

  /**
   * Allocate a transport
   * @param uuid the recipient of the invitation
   * @param alarm the alarm to ring when finished
   * @param _on_create callback when candidates are gathered
   * @return an fd on successfully allocating a transport
   */
  std::optional<file::p2p> _peer_create(const uuid_t &uuid, util::Alarm<decline_t> &alarm,
                                        pj::ice_sess_role_t role, std::function<void(pj::ICECall)> &&);

  /**
   * remove a transport from peers
   * @param uuid The uuid of the transport
   */
  void _peer_remove(const uuid_t &uuid);

  pending_t *_peer(const uuid_t &uuid);

  void _send_error(uuid_t recipient, const std::string_view &err_str);
public:
  /**
   * The maximum peers connected at any time
   */
  std::size_t max_peers;
  uuid_t uuid;

  std::chrono::milliseconds poll_to;

  pj::Pool pool;

  std::thread auto_run_thread;
  util::AutoRun<void> auto_run;

  std::vector<uuid_t> vec_peers;

  file::io bootstrap;
private:
  accept_cb _accept_cb;

  std::map<uuid_t, std::unique_ptr<pending_t>> _peers;
};

int init();
}

namespace server {

struct peer_t {
  using member_t = ::p2p::quest_t;

  std::unique_ptr<file::p2p> socket;
//  p2p::pj::ip_addr_t ip_addr;
};

using p2p = Server<peer_t, const p2p::pj::ip_addr_t &, const p2p::pj::ip_addr_t &, const std::vector<std::string_view> &, std::size_t>;
}

#endif //T_MAN_QUEST_H
