//
// Created by loki on 8-2-19.
//

#include <map>
#include <thread>
#include <chrono>
#include <kitty/server/proxy.h>
#include <kitty/util/auto_run.h>
#include <kitty/file/io_stream.h>
#include <kitty/util/utility.h>
#include <kitty/file/tcp.h>
#include <kitty/log/log.h>
#include <kitty/p2p/uuid.h>
#include <kitty/p2p/data_types.h>
#include <kitty/p2p/p2p.h>
#include "p2p.h"


using namespace std::literals;
namespace p2p {
struct {
  pj::caching_pool_t caching_pool;
} global;

config_t config { };

int init() {
  if(config.uuid == uuid_t {}) {
    config.uuid = uuid_t::generate();
  }

  pj::init(config.log_file);
  global.caching_pool = pj::Pool::init_caching_pool();
  return 0;
}

file::p2p quest_t::_send_accept(uuid_t recipient, const pj::remote_buf_t &remote) {
  util::Alarm<decline_t> alarm { std::nullopt };

  auto peer = _peer_create(recipient, alarm, pj::ice_sess_role_t::PJ_ICE_SESS_ROLE_CONTROLLED,
    [&remote](pj::ICECall call) {
      call.start_ice(remote);
    }
  );

  if(!peer) {
    _send_decline(recipient, { decline_t::DECLINE });

    return {};
  }

  alarm.wait([&]() { return peer->is_open(); });

  if(alarm.status()) {
    _send_decline(recipient, *alarm.status());

    return {};
  }

  return file::p2p { std::move(*peer) };
}

void quest_t::_handle_decline(uuid_t sender, decline_t decline) {
  auto pending = _peer(sender);

  if(pending) {
    (*pending)(decline);
  }
}

void quest_t::_send_decline(uuid_t recipient, const decline_t &decline) {
  server::proxy::push(server, recipient, uuid, "decline"sv, decline.reason);
}

void quest_t::_handle_accept(uuid_t sender, pj::remote_buf_t &&remote) {
  auto pending = _peer(sender);
  if(!pending) {
    print(error, "Could not find a transport for peer[", util::hex(sender), "] :: ", err::current());
    _send_error(sender, "Could not find a transport for peer[" + util::hex(sender).to_string() + "] :: " + err::current());

    return;
  }

  // Attempt connection
  (*pending)(accept_t { std::move(remote) });
}

std::variant<int, file::p2p> quest_t::process_quest() {
  uuid_t to, from;
  std::string quest;

  if(server::proxy::load(server, to, from, quest)) {
    print(error, err::current());

    return 0;
  }

  print(debug, "handling quest [", quest, "] from: ", util::hex(from), " to: ", util::hex(to));
  if(quest == "error") {
    std::string msg;

    server::proxy::load(server, msg);
    print(error, "sender: ", util::hex(from), ": message: ", msg);

    return 0;
  }

  if(quest == "invite" || quest == "accept") {

    data::remote_t remote_d;
    if(server::proxy::load(server, remote_d)) {
      _peer_remove(from);

      print(error, "Could not unpack remote: ", err::current());
      _send_error(from, "Could not unpack remote: "s + err::current());

      return 0;
    }

    auto remote = data::unpack(std::move(remote_d));

    if(quest == "invite") {
      // initiate connection between sender and recipient
      auto decline = _accept_cb(from);
      if(decline) {
        _send_decline(from, *decline);

        return 0;
      }

      return _send_accept(from, remote);
    }

    // tell both peers to start negotiation

    _handle_accept(from, std::move(remote));
  }

  else if(quest == "decline") {
    decline_t decline;
    if(server::proxy::load(server, decline.reason)) {
      print(error, "Couldn't load quest [decline]: ", err::current());

      return 0;
    }

    _handle_decline(from, decline);
  }
  return 0;
}

void quest_t::_send_error(uuid_t recipient, const std::string_view &err_str) {
  server::proxy::push(server, recipient, uuid, "error"sv, err_str);
}

int quest_t::send_register() {
  server::proxy::push(server, uuid_t {}, uuid, "register"sv);

  std::string quest;
  std::vector<uuid_t> peers;
  auto err = server::proxy::load(server, quest, peers);
  if(err) {
    err::set("received no list of peers: "s + err::current());
    return -1;
  }

  peers.erase(std::remove(std::begin(peers), std::end(peers), uuid), std::end(peers));

  print(info, "received a list of ", peers.size(), " peers.");

  vec_peers = std::move(peers);

  return 0;
}

std::optional<file::p2p> quest_t::_peer_create(const uuid_t &peer_uuid, util::Alarm<decline_t> &alarm,
                                               pj::ice_sess_role_t role,
                                               std::function<void(pj::ICECall)> &&on_create) {

  if(_peers.size() < _max_peers && _peers.find(peer_uuid) == std::end(_peers)) {
    auto pipe = std::make_shared<file::stream::pipe_t>();

    auto on_data = [pipe](pj::ICECall call, std::string_view data) {
      pipe->push(data);
    };

    auto _on_create = [this, on_create = std::move(on_create), role, &alarm, &peer_uuid](pj::ICECall call, pj::status_t status) {
      auto guard {
        util::fail_guard([&]() {
          _peer_remove(peer_uuid);

          alarm.status() = { decline_t::ERROR };
          alarm.ring();

          return;
        })
      };

      if(status != pj::success) {
        err::set("failed initialization");

        return;
      }

      if(auto err = call.init_ice(role)) {
        err::set("ice_trans->init_ice(): "s + pj::err(err));

        return;
      }

      const auto quest = role == pj::ice_sess_role_t::PJ_ICE_SESS_ROLE_CONTROLLING ? "invite"sv : "accept"sv;

      auto candidates = call.get_candidates();
      if(candidates.empty()) {
        err::set("no candidates");

        return;
      }

      if(server::proxy::push(server, peer_uuid, uuid, quest,
                          data::pack(call.credentials(), candidates))) {
        return;
      }

      guard.failure = false;

      if(on_create) {
        on_create(call);
      }
    };

    auto on_connect = [this, pipe, &alarm, &peer_uuid] (pj::ICECall call, pj::status_t status) {
      if(status != pj::success) {
        err::set("failed negotiation");

        alarm.status() = { decline_t::ERROR };

        _peer_remove(peer_uuid);

        alarm.ring();

        return;
      }

      pipe->set_call(call);
      alarm.ring();
    };

    _peers.emplace(peer_uuid, std::make_unique<pending_t>(
      pool.ice_trans(std::move(on_data), std::move(_on_create), std::move(on_connect)),
      alarm
      ));

    return file::p2p { 3s, pipe };
  }

  err::code = err::code_t::OUT_OF_BOUNDS;
  return std::nullopt;
}

void quest_t::_peer_remove(const p2p::uuid_t &uuid) {
  _peers.erase(uuid);
}

pending_t *quest_t::_peer(const p2p::uuid_t &uuid) {
  auto it = _peers.find(uuid);

  if(it == std::end(_peers)) {
    return nullptr;
  }

  return it->second.get();
}

std::variant<decline_t, file::p2p> quest_t::invite(uuid_t recipient) {
  util::Alarm<decline_t> alarm { std::nullopt };

  auto peer = _peer_create(
    recipient,
    alarm,
    pj::ice_sess_role_t::PJ_ICE_SESS_ROLE_CONTROLLING,
    nullptr
  );

  if(!peer) {
    return decline_t { decline_t::ERROR };
  }

  alarm.wait([&]() { return peer->is_open(); });

  if(alarm.status()) {
    return *alarm.status();
  }

  return file::p2p { std::move(*peer) };
}

quest_t::quest_t(quest_t::accept_cb &&pred, const uuid_t &uuid) : uuid { uuid }, _accept_cb { std::move(pred) } {}

void pending_t::operator()(answer_t &&answer) {
  if(std::holds_alternative<accept_t>(answer)) {
    auto &accept = std::get<accept_t>(answer);

    _ice_trans.start_ice(accept.remote);
  }
  else {
    auto &decline = std::get<decline_t>(answer);
    _alarm.status() = decline;
  }

  _alarm.ring();
}

pending_t::pending_t(pj::ICETrans &&ice_trans, util::Alarm<decline_t> &alarm) noexcept :
_ice_trans { std::move(ice_trans) }, _alarm { alarm } {}

}

namespace server {



template<>
int p2p::_init_listen(const file::ip_addr_t &ip_addr) {
  _member.pool = ::p2p::pj::Pool { ::p2p::global.caching_pool, "Loki-ICE" };
  _member.pool.dns().set_ns(::p2p::config.dns);
  _member.pool.set_stun(::p2p::config.stun_addr);

  _member.server = file::connect(ip_addr);

  if(!_member.server.is_open()) {
    return -1;
  }

  _member.auto_run_thread = std::thread ([this]() {
    auto thread_ptr = ::p2p::pj::register_thread();

    _member.auto_run.run([this]() {
      _member.pool.iterate(500ms);
    });
  });

  return _member.send_register();
}

template<>
int p2p::_poll() {
  if(auto result = _member.server.wait_for(file::READ)) {
    if(result == err::TIMEOUT) {
      return 0;
    }

    return -1;
  }

  return 1;
}

template<>
std::variant<err::code_t, p2p::client_t> p2p::_accept() {
  int result = _poll();
  if(result < 0) {
    return err::code;
  }

  if(!result) {
    return err::TIMEOUT;
  }

  auto remote = _member.process_quest();
  if(std::holds_alternative<file::p2p>(remote)) {
    auto &fd = std::get<file::p2p>(remote);
    if(!fd.is_open()) {
      return err::TIMEOUT;
    }

    return client_t {
      std::make_unique<file::p2p>(std::move(fd))
    };
  }

  result = std::get<int>(remote);
  if(result < 0) {
    return err::code;
  }

  return err::TIMEOUT;
}

template<>
void p2p::_cleanup() {
  _member.auto_run.stop();
  _member.auto_run_thread.join();
}
}
