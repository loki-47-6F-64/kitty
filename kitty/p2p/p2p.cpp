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

using namespace std::string_literals;
using namespace std::chrono_literals;
namespace p2p {

void handle_quest(file::io &);
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
      accept["quest"]  = "accept";
      accept["to"]     = util::hex(recipient).to_string_view();
      accept["from"]   = util::hex(uuid).to_string_view();
      accept["remote"] = *remote_j_send;

      auto accept_str = accept.dump();
      print(server, file::raw(util::endian::little((std::uint16_t) accept_str.size())),
            accept_str);

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

std::optional<pj::remote_buf_t> quest_t::_process_invite(uuid_t sender, const nlohmann::json &remote_j) {
  auto remote = unpack_remote(remote_j["remote"]);
  if(!remote) {
    _peer_remove(sender);

    _send_decline(sender, {
      decline_t::ERROR,
      "unpacking remote: "s + err::current()
    });

    return std::nullopt;
  }

  return remote;
}

void quest_t::_handle_decline(uuid_t sender) {
  auto ice_trans = _peer(sender);

  if(ice_trans) {
    ice_trans->on_connect()({}, decline_t::DECLINE);
  }
}

void quest_t::_send_decline(uuid_t recipient, decline_t decline) {
  nlohmann::json decline_j;

  decline_j["quest"]  = "decline";
  decline_j["to"]     = util::hex(recipient).to_string_view();
  decline_j["from"]   = util::hex(uuid).to_string_view();
  decline_j["reason"] = { decline.reason, decline.msg };

  auto decline_str = decline_j.dump();

  print(server, file::raw(util::endian::little((std::uint16_t)decline_str.size())), decline_str);
}

void quest_t::_handle_accept(uuid_t sender, const nlohmann::json &remote_j) {
  auto ice_trans = _peer(sender);
  if(!ice_trans) {
    print(error, "Could not find a transport for peer[", util::hex(sender), "] :: ", err::current());
    _send_error(sender, "Could not find a transport for peer[" + util::hex(sender).to_string() + "] :: " + err::current());

    return;
  }

  auto remote = unpack_remote(remote_j["remote"]);
  if(!remote) {
    _peer_remove(sender);

    print(error, "Could not unpack remote: ", err::current());
    _send_error(sender, "Could not unpack remote: "s + err::current());

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
    auto sender = *util::from_hex<uuid_t>(remote_j["from"].get<std::string_view>());

    auto quest = remote_j["quest"].get<std::string_view>();

    print(debug, "handling quest [", quest, "]");
    if(quest == "error") {
      print(error, "uuid: ", remote_j["from"].get<std::string_view>(), ": message: ",
            remote_j["message"].get<std::string_view>());

      return 0;
    }

    if(quest == "invite") {
      // initiate connection between sender and recipient

      auto decline = _accept_cb(sender);
      if(decline) {
        _send_decline(sender, *decline);

        return 0;
      }

      auto remote = _process_invite(sender, remote_j);
      if(remote) {
        return _send_accept(sender, *remote);
      }

      print(error, "Could not unpack remote: ", err::current());
      return 0;
    }

    if(quest == "accept") {
      // tell both peers to start negotiation

      _handle_accept(sender, remote_j);
    }

    if(quest == "decline") {
      _handle_decline(sender);
    }

  } catch(const std::exception &e) {
    print(error, "json exception caught: ", e.what());

    return 0;
  }

  return 0;
}

void quest_t::_send_error(uuid_t recipient, const std::string_view &err_str) {
  nlohmann::json json_error;

  json_error["quest"] = "error";
  json_error["to"]    = util::hex(recipient).to_string_view();
  json_error["from"]  = util::hex(uuid).to_string_view();
  json_error["message"] = err_str;

  auto error_str = json_error.dump();

  print(server, file::raw(util::endian::little((std::uint16_t) error_str.size())), error_str);
}

int quest_t::send_register() {
  nlohmann::json register_;

  register_["quest"] = "register";
  register_["from"]  = util::hex(uuid).to_string_view();
  register_["to"]    = util::hex(uuid_t {}).to_string_view();

  auto register_str = register_.dump();

  print(server, file::raw(util::endian::little((std::uint16_t) register_str.size())), register_str);
  auto size = util::endian::little(file::read_struct<std::uint16_t>(server));
  if(!size) {
    err::set("received no list of peers");
    return -1;
  }

  auto peers_str = file::read_string(server, *size);
  if(!peers_str) {
    err::set("received no list of peers");
    return -1;
  }

  nlohmann::json peers_json;
  try {
    peers_json = nlohmann::json::parse(*peers_str);
    auto quest = peers_json["quest"].get<std::string_view>();

    if(quest == "error") {
      print(error, "something something dark side");
      // TODO handle error
    }
  } catch(std::exception &e) {
    print(error, "Caught json exception: ", e.what());

    return -1;
  }

  auto peers = unpack_peers(peers_json["peers"]);
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

      // pack accept
      nlohmann::json accept;
      accept["quest"]  = "invite";
      accept["to"]     = util::hex(recipient).to_string_view();
      accept["from"]   = util::hex(uuid).to_string_view();
      accept["remote"] = *remote_j_send;

      auto accept_str = accept.dump();
      print(server, file::raw(util::endian::little((std::uint16_t) accept_str.size())),
            accept_str);

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
