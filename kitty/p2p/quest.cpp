//
// Created by loki on 8-2-19.
//

#include <map>
#include <kitty/file/io_stream.h>
#include <kitty/util/utility.h>
#include <kitty/log/log.h>
#include <kitty/p2p/quest.h>
#include <kitty/p2p/uuid.h>
#include <kitty/p2p/pack.h>

#include <nlohmann/json.hpp>

namespace p2p {
using namespace std::string_literals;

void handle_quest(file::io &);

config_t config {
  { },
  { },
  { },
  { },
  { [](file::io &fd, std::monostate) {
    handle_quest(fd);
  }, [](file::io &, std::monostate) { }, [](file::io &fd, std::monostate) {
    print(info, "removing connection to the server");

    fd.seal();
  }},
  { uuid_t::generate() }
};

auto on_data = [](pj::ICECall call, std::string_view data) {
  print(debug, call.ip_addr.ip, ":", call.ip_addr.port, " [><] ", data);
};

auto on_call_connect = [](pj::ICECall call, pj::status_t status) {
  if(status != pj::success) {
    print(error, "failed negotiation");

    return;
  }

  print(debug, "<<<<<<<<<<<<<<<<<<<<< sending data >>>>>>>>>>>>>>>>>>>>>>>");

  call.send(util::toContainer("Hello peer!"));
};

std::shared_ptr<pj::ICETrans> peer(const uuid_t &uuid) {
  auto &peers = config.peers;

  auto it = peers.find(uuid);

  if(it == std::end(peers)) {
    err::code = err::code_t::OUT_OF_BOUNDS;
    return nullptr;
  }

  return it->second;
}

template<class F>
auto peer_create(const uuid_t &uuid, F &&func) {
  auto &peers = config.peers;

  if(peers.size() < config.max_peers && peers.find(uuid) == std::end(peers)) {
    return !peers.emplace(uuid, std::make_shared<pj::ICETrans>(
      config.pool.ice_trans(on_data, std::forward<F>(func), on_call_connect))).second;
  }

  err::code = err::code_t::OUT_OF_BOUNDS;
  return true;
};

auto peer_remove(const uuid_t &uuid) {
  return config.peers.erase(uuid);
}


void quest_error(file::io &server, const std::string_view &error) {
  nlohmann::json json_error;

  json_error["quest"] = "error";
  json_error["uuid"] = util::hex(config.uuid).to_string_view();
  json_error["message"] = error;

  auto error_str = json_error.dump();

  print(server, file::raw(util::endian::little((std::uint16_t) error_str.size())), error_str);
}

void send_invite(file::io &server, uuid_t uuid) {
  auto ice_trans = peer(uuid);
  if(!ice_trans) {
    print(error, "Could not find a transport for peer[", util::hex(uuid), "] :: ", err::current());
    return;
  }

  auto err = ice_trans->init_ice(pj::ice_sess_role_t::PJ_ICE_SESS_ROLE_CONTROLLING);
  if(err) {
    print(error, "ice_trans().init_ice(): ", pj::err(err));

    peer_remove(uuid);
    return;
  }

  auto candidates = ice_trans->get_candidates();
  if(candidates.empty()) {
    print(error, "No candidates");

    peer_remove(uuid);
    return;
  }

  auto remote_j = pack_remote({ ice_trans->credentials(), candidates });
  if(!remote_j) {
    print(error, "No remote: ", err::current());

    peer_remove(uuid);
    return;
  }

  nlohmann::json invite;
  invite["quest"] = "invite";
  invite["uuid"] = util::hex(uuid).to_string_view();
  invite["remote"] = *remote_j;

  auto invite_str = invite.dump();
  print(server, file::raw(util::endian::little((std::uint16_t) invite_str.size())), invite_str);
}

void send_register(file::io &server) {
  nlohmann::json register_;

  register_["quest"] = "register";

  auto register_str = register_.dump();

  print(server, file::raw(util::endian::little((std::uint16_t) register_str.size())), register_str);
  auto size = util::endian::little(file::read_struct<std::uint16_t>(server));
  if(!size) {
    print(error, "received no list of peers");
    return;
  }

  auto peers_json = file::read_string(server, *size);
  if(!peers_json) {
    print(error, "received no list of peers");
    return;
  }

  auto peers = unpack_peers(nlohmann::json::parse(*peers_json));

  peers.erase(std::remove(std::begin(peers), std::end(peers), config.uuid), std::end(peers));

  print(info, "received a list of ", peers.size(), " peers.");

  if(!peers.empty()) {
    if(peer_create(peers[0], [&server, peer = peers[0]](pj::ICECall call, pj::status_t status) {
      if(status != pj::success) {
        print(error, "failed initialization");

        return;
      }
      send_invite(server, peer);
    })
      ) {
      print(error, "Could not allocate a transport for peer[", util::hex(peers[0]), "] :: ", err::current());
      return;
    }
  }
}

void quest_accept(file::io &server, nlohmann::json &remote_json) {
  auto uuid = *util::from_hex<uuid_t>(remote_json["uuid"].get<std::string_view>());

  auto ice_trans = peer(uuid);
  if(!ice_trans) {
    print(error, "Could not find a transport for peer[", util::hex(uuid), "] :: ", err::current());
    quest_error(server,
                "Could not find a transport for peer[" + util::hex(uuid).to_string() + "] :: " + err::current());

    return;
  }

  auto remote = unpack_remote(remote_json["remote"]);
  if(!remote) {
    peer_remove(uuid);

    print(error, "unpacking remote: ", err::current());
    quest_error(server, "unpacking remote: "s + err::current());

    return;
  }

  // Attempt connection
  ice_trans->start_ice(*remote);
}

void quest_invite(file::io &server, nlohmann::json &remote_j) {
  auto uuid = *util::from_hex<uuid_t>(remote_j["uuid"].get<std::string_view>());


  auto failure = peer_create(uuid,
                             [uuid, &server, remote_j = std::move(remote_j)](pj::ICECall call, pj::status_t status) {
                               if(status != pj::success) {
                                 print(error, "failed initialization");

                                 return;
                               }

                               auto err = call.init_ice(pj::ice_sess_role_t::PJ_ICE_SESS_ROLE_CONTROLLED);
                               if(err) {
                                 print(error, "ice_trans->init_ice(): ", pj::err(err));

                                 return;
                               }

                               auto candidates = call.get_candidates();
                               if(candidates.empty()) {
                                 print(error, "no candidates");

                                 peer_remove(uuid);
                                 return;
                               }

                               auto remote_j_send = pack_remote(pj::remote_t {
                                 call.credentials(),
                                 candidates
                               });

                               if(!remote_j_send) {
                                 print(error, "no remote: ", err::current());

                                 peer_remove(uuid);
                                 return;
                               }

                               // pack accept
                               nlohmann::json accept;
                               accept["quest"] = "accept";
                               accept["uuid"] = remote_j["uuid"];
                               accept["remote"] = *remote_j_send;

                               auto accept_str = accept.dump();
                               print(server, file::raw(util::endian::little((std::uint16_t) accept_str.size())),
                                     accept_str);

                               auto remote = unpack_remote(remote_j["remote"]);
                               if(!remote) {
                                 peer_remove(uuid);
                                 print(error, "unpacking remote candidates: ", err::current());

                                 return;
                               }

                               call.start_ice(*remote);
                             });
  if(failure) {
    print(error, "Could not find a transport for peer[", util::hex(uuid), "] :: ", err::current());
    quest_error(server,
                "Could not find a transport for peer[" + util::hex(uuid).to_string() + "] :: " + err::current());

    return;
  }
}

void handle_quest(file::io &server) {
  auto size = util::endian::little(file::read_struct<std::uint16_t>(server));

  if(!size) {
    print(error, "Could not read size");

    return;
  }

  try {
    auto remote = nlohmann::json::parse(*read_string(server, *size));

    auto quest = remote["quest"].get<std::string_view>();

    print(debug, "handling quest [", quest, "]");
    if(quest == "error") {
      print(error, "uuid: ", remote["uuid"].get<std::string_view>(), ": message: ",
            remote["message"].get<std::string_view>());

      return;
    }

    if(quest == "invite") {
      // initiate connection between sender and recipient

      quest_invite(server, remote);
    }

    if(quest == "accept") {
      // tell both peers to start negotiation

      quest_accept(server, remote);
    }

  } catch(const std::exception &e) {
    print(error, "json exception caught: ", e.what());

    return;
  }
}

}