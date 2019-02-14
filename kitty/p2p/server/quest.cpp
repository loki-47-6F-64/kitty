//
// Created by loki on 9-2-19.
//

#include <iostream>

#include <kitty/util/utility.h>
#include <kitty/log/log.h>
#include "nlohmann/json.hpp"
#include "quest.h"

namespace p2p::server {
std::map<uuid_t, file::io> &peers() {
  static std::map<uuid_t, file::io> peers;

  return peers;
}

file::poll_t<file::io, uuid_t> &poll() {
  static file::poll_t<file::io, uuid_t> poll([](file::io &fd, uuid_t uuid) {
    handle_quest(fd, uuid);
  }, [](file::io &, uuid_t) { }, [](file::io &fd, uuid_t uuid) {
    print(info, "removing uuid: ", util::hex(uuid));

    peers().erase(uuid);
  });

  return poll;
}

void quest_error(file::io &client, const std::string_view &error) {
  nlohmann::json json_error;

  json_error["quest"] = "error";

  // the 0 uuid is for error messages from the server
  json_error["from"] = util::hex(uuid_t {}).to_string_view();
  json_error["message"] = error;

  auto error_str = json_error.dump();

  print(client, file::raw(util::endian::little((std::uint16_t) error_str.size())), error_str);
}

void handle_register(file::io &client, const uuid_t &sender) {
  if(peers().find(sender) == peers().cend()) {
    auto it = peers().emplace(sender, std::move(client));

    poll().read(it.first->second, sender);

    nlohmann::json peers_json;
    for(auto &[uuid, _] : peers()) {
      peers_json += util::hex(uuid).to_string_view();
    }

    nlohmann::json register_json;
    register_json["quest"] = "register";
    register_json["peers"] = peers_json;

    auto peers_str = register_json.dump();
    print(it.first->second, file::raw(util::endian::little((std::uint16_t) peers_str.size())), peers_str);
  }
}

void handle_quest(file::io &client, uuid_t uuid) {
  auto size = util::endian::little(file::read_struct<std::uint16_t>(client));
  if(!size) {
    print(debug, "uuid[", util::hex(uuid), "] Could not read size");

    poll().remove(client);
    return;
  }

  auto remote_str = read_string(client, *size);
  if(!remote_str) {
    print(debug, "uuid[", util::hex(uuid), "] Could not read the string");

    poll().remove(client);
    return;
  }

  print(info, *remote_str);

  try {
    auto remote = nlohmann::json::parse(*remote_str);

    auto quest     = remote["quest"].get<std::string_view>();
    auto recipient = util::from_hex<uuid_t>(remote["to"].get<std::string_view>());

    print(debug, "handling quest [", quest, "] from :", util::hex(uuid));
    if(quest == "register") {
      // register the client

      handle_register(client, uuid);
    }

    else if(quest == "leave") {
      // remove the client from the list

      peers().erase(uuid);
    }

    else {
      auto peer = peers().find(*recipient);

      if(peer == peers().cend()) {
        // The intended recipient is not on the list
        print(error, "the recipient [", util::hex(*recipient), "] is not connected to the server");

        quest_error(client, "the recipient [" + util::hex(*recipient).to_string() + "] is not connected to the server");
        return;
      }

      print(peer->second, file::raw(util::endian::little(*size)), *remote_str);
    }

  } catch(const std::exception &e) {
    print(error, "json exception caught: ", e.what());

    poll().remove(client);

    return;
  }
}
}