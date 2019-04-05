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


using namespace std::literals;
namespace p2p {
pj::caching_pool_t caching_pool;

int init() {
  if(pj::init(nullptr)) {
    return -1;
  }

  caching_pool = pj::Pool::init_caching_pool();

  return !(bool)caching_pool.ptr;
}

file::p2p quest_t::_send_accept(uuid_t recipient, const pj::remote_buf_t &remote) {
  util::Alarm<answer_t> alarm;

  auto peer = _peer_create(recipient, alarm, pj::ice_sess_role_t::PJ_ICE_SESS_ROLE_CONTROLLED,
    [&remote](pj::ICECall call) {
      call.start_ice(remote);
    }
  );

  if(!peer) {
    _send_decline(recipient, answer_t::DECLINE);

    return {};
  }

  alarm.wait();

  if(alarm.status()->reason != answer_t::ACCEPT) {
    _send_decline(recipient, *alarm.status());

    return {};
  }

  return file::p2p { std::move(*peer) };
}

void quest_t::_handle_decline(uuid_t sender, answer_t decline) {
  auto pending = _pending_peer(sender);

  if(pending) {
    (*pending)(decline);
  }
}

void quest_t::_send_decline(uuid_t recipient, const answer_t &decline) {
  server::proxy::push(bootstrap, recipient, uuid, "decline"sv, decline.reason);
}

void quest_t::_handle_accept(uuid_t sender, pj::remote_buf_t &&remote) {
  auto pending = _pending_peer(sender);
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

  KITTY_DEBUG_LOG("processing...");
  if(server::proxy::load(bootstrap, to, from, quest)) {
    print(error, err::current());

    return 0;
  }

  print(debug, "handling quest [", quest, "] to: ", util::hex(to), " from: ", util::hex(from));
  if(quest == "error") {
    std::string msg;

    server::proxy::load(bootstrap, msg);
    print(error, "sender: ", util::hex(from), ": message: ", msg);

    return 0;
  }

  if(quest == "invite" || quest == "accept") {

    data::remote_t remote_d;
    if(server::proxy::load(bootstrap, remote_d)) {
      _pending_peer_remove(from);

      print(error, "Could not unpack remote: ", err::current());
      _send_error(from, "Could not unpack remote: "s + err::current());

      return 0;
    }

    auto remote = data::unpack(std::move(remote_d));

    if(remote.candidates.empty()) {
      print(error, "no remote candidates");

      _send_decline(from, answer_t::ERROR);

      return 0;
    }

    ON_DEBUG(for(auto &cand : remote.candidates) {
      auto remote_ip = pj::ip_addr_buf_t::from_sockaddr((pj::sockaddr_t*)&cand.addr);

      print(info, remote_ip.ip, ':', remote_ip.port);
    });

    if(quest == "invite") {
      // if already at max connections
      if(_pending_peers.size() >= max_peers) {
        _send_decline(from, answer_t::DECLINE);

        return 0;
      }

      // initiate connection between sender and recipient
      auto answer = _accept_cb(from);
      if(answer.reason != answer_t::ACCEPT) {
        _send_decline(from, answer);

        return 0;
      }

      return _send_accept(from, remote);
    }

    // tell both peers to start negotiation

    _handle_accept(from, std::move(remote));
  }

  else if(quest == "decline") {
    answer_t decline;
    if(server::proxy::load(bootstrap, decline.reason)) {
      print(error, "Couldn't load quest [decline]: ", err::current());

      return 0;
    }

    _handle_decline(from, decline);
  }
  return 0;
}

void quest_t::_send_error(uuid_t recipient, const std::string_view &err_str) {
  server::proxy::push(bootstrap, recipient, uuid, "error"sv, err_str);
}

int quest_t::send_register() {
  server::proxy::push(bootstrap, uuid_t {}, uuid, "register"sv);

  std::string quest;
  std::vector<uuid_t> peers;
  auto err = server::proxy::load(bootstrap, quest, peers);
  if(err) {
    err::set("received no list of peers: "s + err::current());
    return -1;
  }

  peers.erase(std::remove(std::begin(peers), std::end(peers), uuid), std::end(peers));

  print(info, "received a list of ", peers.size(), " peers.");

  vec_peers = std::move(peers);

  return 0;
}

std::optional<file::p2p> quest_t::_peer_create(const uuid_t &peer_uuid, util::Alarm<answer_t> &alarm,
                                               pj::ice_sess_role_t role,
                                               std::function<void(pj::ICECall)> &&on_create) {

  if(_pending_peers.size() < max_peers && _pending_peers.find(peer_uuid) == std::end(_pending_peers)) {
    auto pipe = std::make_shared<file::stream::pipe_t>();

    auto on_data = [pipe](pj::ICECall call, std::string_view data) {
      KITTY_DEBUG_LOG(call.ip_addr.ip, ':', call.ip_addr.port, " :: ", data);

      pipe->push(data);
    };

    auto _on_create = [this, on_create = std::move(on_create), role, &alarm, &peer_uuid](pj::ICECall call, pj::status_t status) {
      auto guard {
        util::fail_guard([&]() {
          _pending_peer_remove(peer_uuid);

          alarm.ring(answer_t { answer_t::INTERNAL_ERROR });

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

        //TODO: send decline when no candidates
        return;
      }

      if(server::proxy::push(bootstrap, peer_uuid, uuid, quest,
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

        _pending_peer_remove(peer_uuid);

        alarm.ring(answer_t::INTERNAL_ERROR);

        return;
      }

      pipe->set_call(call);
      alarm.ring(answer_t::ACCEPT);
    };

    _pending_peers.emplace(peer_uuid, std::make_unique<pending_t>(
      pool.ice_trans(std::move(on_data), std::move(_on_create), std::move(on_connect)),
      alarm
      ));

    return file::p2p { 3s, pipe };
  }

  err::code = err::code_t::OUT_OF_BOUNDS;
  return std::nullopt;
}

void quest_t::_pending_peer_remove(const p2p::uuid_t &uuid) {
  _pending_peers.erase(uuid);
}

pending_t *quest_t::_pending_peer(const p2p::uuid_t &uuid) {
  auto it = _pending_peers.find(uuid);

  if(it == std::end(_pending_peers)) {
    return nullptr;
  }

  return it->second.get();
}

std::variant<answer_t, file::p2p> quest_t::invite(uuid_t recipient) {
  util::Alarm<answer_t> alarm;

  auto peer = _peer_create(
    recipient,
    alarm,
    pj::ice_sess_role_t::PJ_ICE_SESS_ROLE_CONTROLLING,
    nullptr
  );

  if(!peer) {
    return answer_t { answer_t::INTERNAL_ERROR };
  }

  alarm.wait();
  if(alarm.status()->reason != answer_t::ACCEPT) {
    _pending_peer_remove(recipient);

    return *alarm.status();
  }

  return file::p2p { std::move(*peer) };
}

quest_t::quest_t(quest_t::accept_cb &&pred, const uuid_t &uuid, std::chrono::milliseconds to) : uuid { uuid }, poll_to(to), _accept_cb { std::move(pred) } {}

void pending_t::operator()(__answer_t &&answer) {
  if(std::holds_alternative<accept_t>(answer)) {
    auto &accept = std::get<accept_t>(answer);

    _ice_trans.start_ice(accept.remote);
  }
  else {
    auto &decline = std::get<answer_t>(answer);
    _alarm.ring(decline);
  }
}

pending_t::pending_t(pj::ICETrans &&ice_trans, util::Alarm<answer_t> &alarm) noexcept :
_ice_trans { std::move(ice_trans) }, _alarm { alarm } {}

}

namespace server {

template<>
int p2p::_init_listen(const ::p2p::pj::ip_addr_t &bootstrap, const ::p2p::pj::ip_addr_t &stun, const std::vector<std::string_view> &dns, std::size_t max_peers) {
  _member.pool = ::p2p::pj::Pool { ::p2p::caching_pool, "Loki-ICE" };

  if(!dns.empty()) {
    _member.pool.dns().set_ns(dns);
  }

  if(!stun.ip.empty()) {
    _member.pool.set_stun(stun);
  }

  _member.max_peers = max_peers;

  DEBUG_LOG("init_listen connecting to: ", bootstrap.ip, ":", bootstrap.port);
  _member.bootstrap = file::connect(bootstrap);

  if(!_member.bootstrap.is_open()) {
    DEBUG_LOG("init_listen connecting failed");
    return -1;
  }

  _member.auto_run_thread = std::thread ([this]() {
    auto thread_ptr = ::p2p::pj::register_thread();

    _member.auto_run.run([this]() {
      _member.pool.iterate(_member.poll_to);
    });
  });

  return _member.send_register();
}

template<>
int p2p::_poll() {
  if(auto result = _member.bootstrap.wait_for(file::READ)) {
    if(result == err::TIMEOUT) {
      return 0;
    }

    return -1;
  }

  return 1;
}

template<>
std::variant<err::code_t, p2p::client_t> p2p::_accept() {
  auto remote = _member.process_quest();
  if(std::holds_alternative<file::p2p>(remote)) {
    auto &fd = std::get<file::p2p>(remote);
    if(!fd.is_open()) {
      // TODO: log reason of decline of invitation
      return err::OK;
    }

    return client_t {
      std::make_unique<file::p2p>(std::move(fd))
    };
  }

  auto result = std::get<int>(remote);
  if(result < 0) {
    return err::code;
  }

  return err::OK;
}

template<>
void p2p::_cleanup() {
  if(_member.auto_run.isRunning()) {
    _member.auto_run.stop();
    _member.auto_run_thread.join();
  }
}
}
