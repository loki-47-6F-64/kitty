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
  util::Alarm<std::optional<decline_t>> alarm { std::nullopt };

  auto peer = _peer_create(recipient, alarm,
    [&](pj::ICECall call, pj::status_t status) {
      auto guard {
        util::fail_guard([&]() {

          _peer_remove(recipient);
          alarm.status() = decline_t {
            decline_t::ERROR,
            err::current()
          };

          alarm.ring();
          return;
        })
      };

      if(status != pj::success) {
        err::set("failed initialization");

        return;
      }

      if(auto err = call.init_ice(pj::ice_sess_role_t::PJ_ICE_SESS_ROLE_CONTROLLED)) {
        err::set("ice_trans->init_ice(): "s + pj::err(err));

        return;
      }

      auto candidates = call.get_candidates();
      if(candidates.empty()) {
        err::set("no candidates");

        return;
      }


      auto err = ::server::proxy::push(server, recipient, uuid, "accept"sv,
        data::pack(call.credentials(), candidates));

      if(err) {
        print(error, "Couldn't push data: ", err::current());

        return;
      }

      guard.failure = false;
      call.start_ice(remote);
      return;
    }
  );

  if(!peer) {
    _send_decline(recipient, {
      decline_t::DECLINE, err::current()
    });

    return {};
  }

  alarm.wait([&]() { return alarm.status() || peer->is_open(); });

  if(alarm.status()) {
    _send_decline(recipient, *alarm.status());

    return {};
  }

  return file::p2p { std::move(*peer) };
}

void quest_t::_handle_decline(uuid_t sender) {
  auto ice_trans = _peer(sender);

  if(ice_trans) {
    ice_trans->on_connect()({}, decline_t::DECLINE);
  }
}

void quest_t::_send_decline(uuid_t recipient, decline_t decline) {
  ::server::proxy::push(server, recipient, uuid, "decline"sv, decline.reason, decline.msg);
}

void quest_t::_handle_accept(uuid_t sender, const pj::remote_buf_t &remote) {
  auto ice_trans = _peer(sender);
  if(!ice_trans) {
    print(error, "Could not find a transport for peer[", util::hex(sender), "] :: ", err::current());
    _send_error(sender, "Could not find a transport for peer[" + util::hex(sender).to_string() + "] :: " + err::current());

    return;
  }

  // Attempt connection
  ice_trans->start_ice(remote);
}

std::variant<int, file::p2p> quest_t::process_quest() {
  uuid_t to, from;
  std::string quest;

  if(::server::proxy::load(server, to, from, quest)) {
    print(error, err::current());

    return 0;
  }

  print(debug, "handling quest [", quest, "] from: ", util::hex(from), " to: ", util::hex(to));
  if(quest == "error") {
    std::string msg;

    ::server::proxy::load(server, msg);
    print(error, "sender: ", util::hex(from), ": message: ", msg);

    return 0;
  }

  if(quest == "invite" || quest == "accept") {

    data::remote_t remote_d;
    if(::server::proxy::load(server, remote_d)) {
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

    _handle_accept(from, remote);
  }

  if(quest == "decline") {
    _handle_decline(from);
  }
  return 0;
}

void quest_t::_send_error(uuid_t recipient, const std::string_view &err_str) {
  ::server::proxy::push(server, recipient, uuid, "error"sv, err_str);
}

int quest_t::send_register() {
  ::server::proxy::push(server, uuid_t {}, uuid, "register"sv);

  std::string quest;
  std::vector<uuid_t> peers;
  auto err = ::server::proxy::load(server, quest, peers);
  if(err) {
    err::set("received no list of peers: "s + err::current());
    return -1;
  }

  peers.erase(std::remove(std::begin(peers), std::end(peers), uuid), std::end(peers));

  print(info, "received a list of ", peers.size(), " peers.");

  vec_peers = std::move(peers);

  return 0;
}

std::optional<file::p2p> quest_t::_peer_create(const uuid_t &uuid, util::Alarm<std::optional<decline_t>> &alarm,
                                               pj::Pool::on_ice_create_f &&on_create) {

  if(_peers.size() < _max_peers && _peers.find(uuid) == std::end(_peers)) {
    auto pipe = std::make_shared<file::stream::pipe_t>();

    auto on_data = [pipe](pj::ICECall call, std::string_view data) {
      pipe->push(data);
    };

    auto on_connect = [this, pipe, &alarm, &uuid] (pj::ICECall call, pj::status_t status) {
      auto guard {
        util::fail_guard([&]() {
          alarm.status() = {
            decline_t::ERROR,
            err::current()
          };

          _peer_remove(uuid);

          alarm.ring();

          return;
        })
      };

      if(status == decline_t::DECLINE) {
        alarm.status() = {
          decline_t::DECLINE,
          "Invitee declined invitation"
        };


        guard.failure = false;
        alarm.ring();
        return;
      }

      if(status != pj::success) {
        err::set("failed negotiation");

        return;
      }

      pipe->set_call(call);

      guard.failure = false;
      alarm.ring();
    };

    _peers.emplace(uuid, std::make_shared<pj::ICETrans>(
      pool.ice_trans(std::move(on_data), std::move(on_create), std::move(on_connect))
      ));

    return file::p2p { 3s, pipe };
  }

  err::code = err::code_t::OUT_OF_BOUNDS;
  return std::nullopt;
}

void quest_t::_peer_remove(const p2p::uuid_t &uuid) {
  _peers.erase(uuid);
}

std::shared_ptr<pj::ICETrans> quest_t::_peer(const p2p::uuid_t &uuid) {
  auto it = _peers.find(uuid);

  if(it == std::end(_peers)) {
    return nullptr;
  }

  return it->second;
}

std::variant<decline_t, file::p2p> quest_t::invite(uuid_t recipient) {
  util::Alarm<std::optional<decline_t>> alarm { std::nullopt };

  auto peer = _peer_create(recipient, alarm,
    [&](pj::ICECall call, pj::status_t status) {
      auto guard {
        util::fail_guard([&]() {
          _peer_remove(recipient);

          alarm.status() = {
            decline_t::ERROR,
            err::current()
          };

          alarm.ring();

          return;
        })
      };

      if(status != pj::success) {
        err::set("failed initialization");

        return;
      }

      auto err = call.init_ice(pj::ice_sess_role_t::PJ_ICE_SESS_ROLE_CONTROLLING);
      if(err) {
        err::set("ice_trans->init_ice(): "s + pj::err(err));

        return;
      }

      auto candidates = call.get_candidates();
      if(candidates.empty()) {
        err::set("no candidates");

        return;
      }

      ::server::proxy::push(server, recipient, uuid, "invite"sv,
        data::pack(call.credentials(), candidates));

      guard.failure = false;
      return;
    }
  );

  if(!peer) {
    return decline_t {
      decline_t::ERROR,
      err::current()
    };
  }

  alarm.wait([&]() { return alarm.status() || peer->is_open(); });

  if(alarm.status()) {
    return *alarm.status();
  }

  return file::p2p { std::move(*peer) };
}

quest_t::quest_t(quest_t::accept_cb &&pred, const uuid_t &uuid) : uuid { uuid }, _accept_cb { std::move(pred) } {}

}

namespace server {



template<>
int p2p::_init_listen(const file::ip_addr_t &ip_addr) {
  _member.pool = ::p2p::pj::Pool { ::p2p::global.caching_pool, "Loki-ICE" };
  _member.pool.dns_resolv().set_ns(::p2p::config.dns);
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
