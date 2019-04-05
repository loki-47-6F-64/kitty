//
// Created by loki on 21-3-19.
//

#include <kitty/util/set.h>
#include <kitty/util/move_by_copy.h>

#include "ice_trans.h"

namespace p2p::pj {
using namespace std::literals;
creds_buf_t::operator creds_t() const {
  return creds_t {
    ufrag,
    passwd
  };
}

ICEState::ICEState(ice_trans_state_t state) : _state { state } {}


// TODO: implement state
bool ICEState::created() const {
  return _state == ice_trans_state_t::CREATED;
}

bool ICEState::initializing() const {
  return _state == ice_trans_state_t::INITIALIZING;
}

bool ICEState::initialized() const {
  return _state > ice_trans_state_t::INITIALIZING;
}

bool ICEState::has_session() const {
  return _state > ice_trans_state_t::INITIALIZED;
}

bool ICEState::connecting() const {
  return _state == ice_trans_state_t::CONNECTING;
}

bool ICEState::connected() const {
  return _state == ice_trans_state_t::CONNECTED;
}

bool ICEState::failed() const {
  return _state == ice_trans_state_t::FAILED;
}

int __connect(file::io &socket, const ip_addr_t &ip) {
  static auto constexpr to = 100ms;

  if(file::udp_connect(socket, ip) < 0) {
    return -1;
  }

  //TODO: verify connection
  std::this_thread::sleep_for(to);

  return 0;
}

std::vector<ip_addr_buf_t> sockname(const file::io &sock) {
  auto sock_fd = sock.getStream().fd();

  sockaddr_storage addr;

  socklen_t size_addr = sizeof(addr);
  if(getsockname(sock_fd, (::sockaddr*)&addr, &size_addr) < 0) {
    return {};
  }

  auto ip_addrs = file::get_broadcast_ips(file::INET);

  auto port = util::endian::big(((::sockaddr_in*)&addr)->sin_port);
  for(auto &ip_addr : ip_addrs) {
    ip_addr.port = port;
  }

  return ip_addrs;
}

void __start_ice(ip_addr_buf_t &&ip, __ice_trans_t *ice_trans) {
  ICECall call { ice_trans, ip };

  auto err = __connect(ice_trans->socket, ip);
  if(err < 0) {
    ice_trans->_state = ice_trans_state_t::FAILED;
    ice_trans->on_connect(call, (status_t)err::LIB_SYS );
  }
  else {
    ice_trans->_state = ice_trans_state_t::CONNECTED;
    ice_trans->on_connect(call, success );

    ice_trans->io_queue_p->poll.read(ice_trans->socket, connection_t { ice_trans, ip_addr_buf_t { call.ip_addr.ip.data(), call.ip_addr.port } });
  }
}

ICETrans::ICETrans(ICETrans::func_t &&callback, IOQueue *io_queue_p) : _ice_trans { std::make_unique<__ice_trans_t>() } {
  _ice_trans->io_queue_p = io_queue_p;
  _ice_trans->_state = ice_trans_state_t::CREATED;

  _ice_trans->on_data       = std::move(std::get<0>(*callback));
  _ice_trans->on_ice_create = std::move(std::get<1>(*callback));
  _ice_trans->on_connect    = std::move(std::get<2>(*callback));


  ICECall call { _ice_trans.get(), {}};
  _ice_trans->io_queue_p->task_pool.push(_ice_trans->on_ice_create, call, success);
}

status_t ICETrans::init_ice(ice_sess_role_t role) {
  _ice_trans->_role = role;
  _ice_trans->_state = ice_trans_state_t::INITIALIZING;

  _ice_trans->socket = file::udp_init();
  if(!_ice_trans->socket.is_open()) {
    _ice_trans->_state = ice_trans_state_t::FAILED;

    return -1;
  }

  _ice_trans->_state = ice_trans_state_t::INITIALIZED;
  return 0;
}

status_t ICETrans::set_role(ice_sess_role_t role) {
  _ice_trans->_role = role;

  return 0;
}

status_t ICETrans::end_call() {
  if(_ice_trans->socket.is_open()) {
    _ice_trans->socket.seal();
  }

  _ice_trans->_state = std::min(_ice_trans->_state, ice_trans_state_t::INITIALIZED);
  return 0;
}

status_t ICETrans::end_session() {
  if(_ice_trans->socket.is_open()) {
    _ice_trans->socket.seal();
  }

  _ice_trans->_state = std::min(_ice_trans->_state, ice_trans_state_t::CREATED);
  return 0;
}

status_t ICETrans::start_ice(const remote_t &remote) {
  auto &candidate = remote.candidates[0];

  _ice_trans->_state = ice_trans_state_t::CONNECTING;

  auto ip = file::ip_addr_buf_t::from_sockaddr((::sockaddr*)&candidate.addr);
  _ice_trans->io_queue_p->task_pool.push(__start_ice, util::cmove(ip), _ice_trans.get());

  return 0;
}

status_t ICETrans::start_ice(const remote_buf_t &remote) {
  auto &candidate = remote.candidates[0];

  _ice_trans->_state = ice_trans_state_t::CONNECTING;

  auto ip = file::ip_addr_buf_t::from_sockaddr((::sockaddr*)&candidate.addr);
  _ice_trans->io_queue_p->task_pool.push(__start_ice, util::cmove(ip), _ice_trans.get());

  return 0;
}

ICEState ICETrans::get_state() const {
  return ICEState { _ice_trans->_state };
}

creds_t ICETrans::credentials() {
  return creds_t {};
}

std::vector<ice_sess_cand_t> ICETrans::get_candidates(unsigned int comp_cnt) {
  assert(_ice_trans->socket.is_open());

  auto candidates = sockname(_ice_trans->socket);
  return util::map(candidates, [](file::ip_addr_buf_t &ip) {
    ice_sess_cand_t cand {};

    cand.addr = *ip.to_sockaddr();

    return cand;
  });
}

ICETrans::on_data_f &ICETrans::on_data() {
  return _ice_trans->on_data;
}

ICETrans::on_ice_create_f &ICETrans::on_ice_create() {
  return _ice_trans->on_ice_create;
}

ICETrans::on_connect_f &ICETrans::on_connect() {
  return _ice_trans->on_connect;
}

ICECall::ICECall(ice_trans_t::pointer ice_trans, ip_addr_t ip_addr) : ip_addr { ip_addr }, _ice_trans { ice_trans } {}

status_t ICECall::init_ice(ice_sess_role_t role) {
  _ice_trans->_role = role;
  _ice_trans->_state = ice_trans_state_t::INITIALIZING;

  _ice_trans->socket = file::udp_init();
  if(!_ice_trans->socket.is_open()) {
    _ice_trans->_state = ice_trans_state_t::FAILED;

    return -1;
  }

  _ice_trans->_state = ice_trans_state_t::INITIALIZED;
  return 0;
}

status_t ICECall::set_role(ice_sess_role_t role) {
  _ice_trans->_role = role;

  return 0;
}

status_t ICECall::end_call() {
  if(_ice_trans->socket.is_open()) {
    _ice_trans->socket.seal();
  }

  _ice_trans->_state = std::min(_ice_trans->_state, ice_trans_state_t::INITIALIZED);
  return 0;
}

status_t ICECall::end_session() {
  if(_ice_trans->socket.is_open()) {
    _ice_trans->socket.seal();
  }

  _ice_trans->_state = std::min(_ice_trans->_state, ice_trans_state_t::CREATED);
  return 0;
}

status_t ICECall::start_ice(const remote_t &remote) {
  auto &candidate = remote.candidates[0];

  _ice_trans->_state = ice_trans_state_t::CONNECTING;

  auto ip = file::ip_addr_buf_t::from_sockaddr((::sockaddr*)&candidate.addr);
  _ice_trans->io_queue_p->task_pool.push(__start_ice, util::cmove(ip), _ice_trans);

  return 0;
}

status_t ICECall::start_ice(const remote_buf_t &remote) {
  auto &candidate = remote.candidates[0];

  _ice_trans->_state = ice_trans_state_t::CONNECTING;

  auto ip = file::ip_addr_buf_t::from_sockaddr((::sockaddr*)&candidate.addr);
  _ice_trans->io_queue_p->task_pool.push(__start_ice, util::cmove(ip), _ice_trans);

  return 0;
}

creds_t ICECall::credentials() {
  return creds_t {};
}

ICEState ICECall::get_state() const {
  return ICEState { _ice_trans->_state  };
}

std::vector<ice_sess_cand_t> ICECall::get_candidates(unsigned int comp_cnt) {
  assert(_ice_trans->socket.is_open());

  auto candidates = sockname(_ice_trans->socket);
  return util::map(candidates, [](file::ip_addr_buf_t &ip) {
    ice_sess_cand_t cand {};

    cand.addr = *ip.to_sockaddr();

    return cand;
  });
}

}