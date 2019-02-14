//
// Created by loki on 8-2-19.
//

#include <map>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>
#include <kitty/util/auto_run.h>
#include <kitty/file/io_stream.h>
#include <kitty/util/utility.h>
#include <kitty/file/tcp.h>
#include <kitty/log/log.h>
#include <kitty/p2p/p2p.h>
#include <kitty/p2p/uuid.h>
#include <kitty/p2p/pack.h>
#include <kitty/p2p/p2p.h>
#include "p2p.h"


namespace p2p {
using namespace std::string_literals;
using namespace std::chrono_literals;

void handle_quest(file::io &);
struct {
  pj::caching_pool_t caching_pool;
} global;

config_t config { };

util::AutoRun<void> auto_run;

int init() {
  if(config.uuid == uuid_t {}) {
    config.uuid = uuid_t::generate();
  }

  pj::init(config.log_file);
  global.caching_pool = pj::Pool::init_caching_pool();
  return 0;
}

file::p2p quest_t::_send_accept(uuid_t uuid, const pj::remote_buf_t &remote) {
  util::Alarm<bool> alarm { false };

  auto peer = _peer_create(uuid, alarm,
    [&](pj::ICECall call, pj::status_t status) {
      auto guard {
        util::fail_guard([&]() {
          _peer_remove(uuid);

          alarm.status() = true;
          alarm.ring();
          return;
        })
      };

      if(status != pj::success) {
        err::set("failed initialization");



        // TODO send decline

        return;
      }

      auto err = call.init_ice(pj::ice_sess_role_t::PJ_ICE_SESS_ROLE_CONTROLLED);
      if(err) {
        err::set("ice_trans->init_ice(): "s + pj::err(err));

        return;
      }

      auto candidates = call.get_candidates();
      if(candidates.empty()) {
        err::set("no candidates");

        return;
      }

      auto remote_j_send = pack_remote(pj::remote_t {
        call.credentials(),
        candidates
      });

      if(!remote_j_send) {
        err::set("no remote: "s + err::current());

        return;
      }

      guard.failure = false;

      // pack accept
      nlohmann::json accept;
      accept["quest"] = "accept";
      accept["uuid"] = util::hex(uuid).to_string_view();
      accept["remote"] = *remote_j_send;

      auto accept_str = accept.dump();
      print(server, file::raw(util::endian::little((std::uint16_t) accept_str.size())),
            accept_str);

      call.start_ice(remote);
      return;
    }
  );

  if(!peer) {
    // TODO send decline

    return {};
  }

  alarm.wait([&]() { return alarm.status() || peer->is_open(); });

  return file::p2p { std::move(*peer) };
}

std::optional<pj::remote_buf_t> quest_t::_process_invite(uuid_t uuid, const nlohmann::json &remote_j) {
  if(!_accept_cb) {
    // TODO: decline

    return std::nullopt;
  }

  auto remote = unpack_remote(remote_j["remote"]);
  if(!remote) {
    _peer_remove(uuid);

    _send_error("unpacking remote: "s + err::current());

    return std::nullopt;
  }

  return remote;
}

void quest_t::_handle_accept(uuid_t uuid, const nlohmann::json &remote_j) {
  auto ice_trans = _peer(uuid);
  if(!ice_trans) {
    print(error, "Could not find a transport for peer[", util::hex(uuid), "] :: ", err::current());
    _send_error("Could not find a transport for peer[" + util::hex(uuid).to_string() + "] :: " + err::current());

    return;
  }

  auto remote = unpack_remote(remote_j["remote"]);
  if(!remote) {
    _peer_remove(uuid);

    print(error, "unpacking remote: ", err::current());
    _send_error("unpacking remote: "s + err::current());

    return;
  }

  // Attempt connection
  ice_trans->start_ice(*remote);
}

std::variant<int, file::p2p> quest_t::process_quest() {
  auto size = util::endian::little(file::read_struct<std::uint16_t>(server));

  if(!size) {
    print(error, "Could not read size", err::current());

    return 0;
  }

  try {
    auto remote_j_str = file::read_string(server, *size);
    if(!remote_j_str) {
      print(error, "Could not read the json string: ", err::current());
    }

    auto remote_j = nlohmann::json::parse(*remote_j_str);
    auto uuid = *util::from_hex<uuid_t>(remote_j["uuid"].get<std::string_view>());

    auto quest = remote_j["quest"].get<std::string_view>();

    print(debug, "handling quest [", quest, "]");
    if(quest == "error") {
      print(error, "uuid: ", remote_j["uuid"].get<std::string_view>(), ": message: ",
            remote_j["message"].get<std::string_view>());

      return 0;
    }

    if(quest == "invite") {
      // initiate connection between sender and recipient

      auto remote = _process_invite(uuid, remote_j);
      if(remote) {
        // TODO return p2p fd
        return _send_accept(uuid, *remote);
      }

      print(error, "Could not unpack remote: ", err::current());
      return 0;
    }

    if(quest == "accept") {
      // tell both peers to start negotiation

      _handle_accept(uuid, remote_j);
    }

  } catch(const std::exception &e) {
    print(error, "json exception caught: ", e.what());

    return 0;
  }

  return 0;
}

void quest_t::_send_error(const std::string_view &err_str) {
  nlohmann::json json_error;

  json_error["quest"] = "error";
  json_error["uuid"] = util::hex(uuid).to_string_view();
  json_error["message"] = err_str;

  auto error_str = json_error.dump();

  print(server, file::raw(util::endian::little((std::uint16_t) error_str.size())), error_str);
}

int quest_t::send_register() {
  nlohmann::json register_;

  register_["quest"] = "register";

  auto register_str = register_.dump();

  print(server, file::raw(util::endian::little((std::uint16_t) register_str.size())), register_str);
  auto size = util::endian::little(file::read_struct<std::uint16_t>(server));
  if(!size) {
    err::set("received no list of peers");
    return -1;
  }

  auto peers_json = file::read_string(server, *size);
  if(!peers_json) {
    err::set("received no list of peers");
    return -1;
  }

  auto peers = unpack_peers(nlohmann::json::parse(*peers_json));

  peers.erase(std::remove(std::begin(peers), std::end(peers), uuid), std::end(peers));

  print(info, "received a list of ", peers.size(), " peers.");

  vec_peers = std::move(peers);

  return 0;
}

std::optional<file::p2p> quest_t::_peer_create(const uuid_t &uuid, util::Alarm<bool> &alarm, pj::Pool::on_ice_create_f &&on_create) {

  if(_peers.size() < _max_peers && _peers.find(uuid) == std::end(_peers)) {
    auto pipe = std::make_shared<file::stream::pipe_t>();

    auto on_data = [pipe](pj::ICECall call, std::string_view data) {
      pipe->push(data);
    };

    auto on_connect = [this, pipe, &alarm, &uuid] (pj::ICECall call, pj::status_t status) {
      auto guard {
        util::fail_guard([&]() {
          alarm.status() = true;

          _peer_remove(uuid);
          alarm.ring();

          return;
        })
      };

      if(status != pj::success) {
        err::set("failed initialization");
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

file::p2p quest_t::invite(uuid_t recipient) {
  util::Alarm<bool> alarm { false };

  auto peer = _peer_create(recipient, alarm,
    [&](pj::ICECall call, pj::status_t status) {
      auto guard {
        util::fail_guard([&]() {
          _peer_remove(recipient);

          alarm.status() = true;
          alarm.ring();

          return;
        })
      };

      if(status != pj::success) {
        err::set("failed initialization");



        // TODO send decline

        return;
      }

      auto err = call.init_ice(pj::ice_sess_role_t::PJ_ICE_SESS_ROLE_CONTROLLED);
      if(err) {
        err::set("ice_trans->init_ice(): "s + pj::err(err));

        return;
      }

      auto candidates = call.get_candidates();
      if(candidates.empty()) {
        err::set("no candidates");

        return;
      }

      auto remote_j_send = pack_remote(pj::remote_t {
        call.credentials(),
        candidates
      });

      if(!remote_j_send) {
        err::set("no remote: "s + err::current());

        return;
      }

      guard.failure = false;

      // pack accept
      nlohmann::json accept;
      accept["quest"] = "invite";
      accept["uuid"] = util::hex(recipient).to_string_view();
      accept["remote"] = *remote_j_send;

      auto accept_str = accept.dump();
      print(server, file::raw(util::endian::little((std::uint16_t) accept_str.size())),
            accept_str);

      return;
    }
  );

  if(!peer) {
    // TODO send decline

    return {};
  }

  alarm.wait([&]() { return alarm.status() || peer->is_open(); });

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

      std::chrono::milliseconds max_tm { 500 };
      std::chrono::milliseconds milli { 0 };

      _member.pool.timer_heap().poll(milli);

      milli = std::min(milli, max_tm);

      auto c = _member.pool.io_queue().poll(milli);
      if(c < 0) {
        print(error, __FILE__, ": ", ::p2p::pj::err(::p2p::pj::get_netos_err()));

        std::abort();
      }
    });
  });

  print(_member.server, file::raw(util::endian::little(_member.uuid)));

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
    // auto fd = _member.connect(std::get<::p2p::pj::remote_buf_t>(remote));

    auto &fd = std::get<file::p2p>(remote);
    if(!fd.is_open()) {
      return err::TIMEOUT;
    }

    // std::this_thread::sleep_for(std::chrono::milliseconds(50));
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
