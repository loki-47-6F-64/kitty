//
// Created by loki on 21-3-19.
//

#ifndef T_MAN_ICE_TRANS_H
#define T_MAN_ICE_TRANS_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <kitty/util/utility.h>
#include <kitty/file/tcp.h>
#include <kitty/p2p/pj_fake/nath.h>

namespace p2p::pj {

class ICECall;
using on_data_f       = std::function<void(ICECall, std::string_view)>;
using on_ice_create_f = std::function<void(ICECall, status_t)>;
using on_connect_f    = std::function<void(ICECall, status_t)>;

enum class ice_trans_state_t {
  FAILED,
  CREATED,
  INITIALIZED,
  INITIALIZING,
  HAS_SESSION,
  CONNECTING,
  CONNECTED
};

struct ice_sess_cand_t {
  ice_cand_type_t type;
  sockaddr addr;
  std::string_view foundation;
  int prio, comp_id;
};

enum class ice_sess_role_t {
  PJ_ICE_SESS_ROLE_CONTROLLED,
  PJ_ICE_SESS_ROLE_CONTROLLING
};

struct __ice_trans_t {
  ice_trans_state_t _state;
  ice_sess_role_t   _role;

  IOQueue *io_queue_p;

  on_data_f       on_data;
  on_ice_create_f on_ice_create;
  on_connect_f    on_connect;

  file::io socket;
};

using ice_trans_t = std::unique_ptr<__ice_trans_t>;

struct creds_t {
  std::string_view ufrag;
  std::string_view passwd;
};

struct creds_buf_t {
  std::string ufrag;
  std::string passwd;

  creds_buf_t() = default;
  creds_buf_t(creds_buf_t&&) noexcept = default;
  creds_buf_t&operator=(creds_buf_t&&) noexcept = default;

  operator creds_t() const;
};

struct remote_t {
  creds_t creds;
  std::vector<ice_sess_cand_t> candidates;
};

struct remote_buf_t {
  creds_buf_t creds;
  std::vector<ice_sess_cand_t> candidates;
};

class ICEState {
public:
  explicit ICEState(ice_trans_state_t);

  /**
   * ICE stream transport is created.
   */
  bool created() const;

  /**
   * Gathering ICE candidates
   */
  bool initializing() const;

  /**
   * ICE stream transport initialization/candidate gathering process is
   * complete, ICE session may be created on this stream transport.
   */
  bool initialized() const;

  /**
   * New session has been created and the session is ready.
   */
  bool has_session() const;

  /**
   * Negotiating
   */
  bool connecting() const;

  /**
   * ICE negotiation has completed successfully.
   */
  bool connected() const;

  /**
   * ICE negotiation has completed with failure.
   */
  bool failed() const;
private:
  ice_trans_state_t _state;
};

enum class __prepend_e : std::uint16_t {
  CONNECTING,
  CONFIRM,
  DATA
};

class ICECall {
public:
  ICECall() = default;
  ICECall(ice_trans_t::pointer ice_trans, ip_addr_t ip_addr);

  status_t init_ice(ice_sess_role_t role = ice_sess_role_t::PJ_ICE_SESS_ROLE_CONTROLLED);
  status_t set_role(ice_sess_role_t role);

  status_t end_call();
  status_t end_session();

  status_t start_ice(const remote_t &remote);
  status_t start_ice(const remote_buf_t &remote);

  creds_t credentials();

  ICEState get_state() const;
  std::vector<ice_sess_cand_t> get_candidates(unsigned comp_cnt = 0);

  template<class T>
  status_t send(util::FakeContainer<T> data) {
    return print(_ice_trans->socket, file::raw(util::endian::little(__prepend_e::DATA)), file::raw(util::endian::little<std::uint16_t>(data.size())), data);
  }

  ip_addr_t ip_addr;
private:
  ice_trans_t::pointer _ice_trans;
};

class ICETrans {
public:
  using on_data_f       = std::function<void(ICECall, std::string_view)>;
  using on_ice_create_f = std::function<void(ICECall, status_t)>;
  using on_connect_f    = std::function<void(ICECall, status_t)>;

  using func_t = std::unique_ptr<std::tuple<on_data_f, on_ice_create_f, on_connect_f>>;

  ICETrans() = default;

  explicit ICETrans(ICETrans::func_t &&callback, IOQueue *io_queue_p);

  status_t init_ice(ice_sess_role_t role = ice_sess_role_t::PJ_ICE_SESS_ROLE_CONTROLLED);
  status_t set_role(ice_sess_role_t role);

  status_t end_call();
  status_t end_session();

  status_t start_ice(const remote_t &remote);
  status_t start_ice(const remote_buf_t &remote);

  ICEState get_state() const;

  creds_t credentials();

  std::vector<ice_sess_cand_t> get_candidates(unsigned comp_cnt = 0);

  on_data_f       & on_data();
  on_ice_create_f & on_ice_create();
  on_connect_f    & on_connect();

private:
  ice_trans_t _ice_trans;
};

}

#endif //T_MAN_ICE_TRANS_H
