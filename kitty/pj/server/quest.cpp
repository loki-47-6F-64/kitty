//
// Created by loki on 9-2-19.
//

#include <iostream>

#include <kitty/util/utility.h>
#include <kitty/log/log.h>
#include "nlohmann/json.hpp"
#include "quest.h"

std::map<uuid_t, file::io> &peers() {
  static std::map<uuid_t, file::io> peers;

  return peers;
}

file::poll_t<file::io, uuid_t> &poll() {
  static file::poll_t<file::io, uuid_t> poll([](file::io &fd, uuid_t uuid) {
    handle_quest(fd, uuid);
  }, [](file::io&,uuid_t){}, [](file::io& fd, uuid_t uuid) {
    print(info, "removing uuid: ", util::hex(uuid));

    peers().erase(uuid);
  });

  return poll;
}

void quest_error(file::io &client, const std::string_view &error) {
  nlohmann::json json_error;

  json_error["quest"]   = "error";

  // the 0 uuid is for error messages from the server
  json_error["uuid"]    = util::hex(uuid_t {{0}}).to_string_view();
  json_error["message"] = error;

  auto error_str = json_error.dump();

  print(client, file::raw(util::endian::little((std::uint16_t)error_str.size())), error_str);
}

void quest_accept(file::io &client, const uuid_t &sender, nlohmann::json &remote) {
  auto receiver = util::from_hex<uuid_t>(remote["uuid"].get<std::string_view>());

  auto recipient = peers().find(*receiver);

  if(recipient == peers().cend()) {
    // The intended recipient is not on the list

    print(error, "the recipient [", util::hex(*receiver), "] is not connected to the server");

    quest_error(client, "the recipient [" + util::hex(*receiver).to_string() + "] is not connected to the server");
    return;
  }

  nlohmann::json accept;
  accept["quest"]      = "accept";
  accept["uuid"]       = util::hex(sender).to_string_view();
  accept["remote"]     = remote["remote"];

  auto accept_str = accept.dump();
  print(recipient->second, file::raw(util::endian::little((std::uint16_t)accept_str.size())), accept_str);
}

void quest_invite(file::io &client, const uuid_t &sender, nlohmann::json &remote) {
  auto receiver = util::from_hex<uuid_t>(remote["uuid"].get<std::string_view>());

  auto recipient = peers().find(*receiver);

  if(recipient == peers().cend()) {
    // The intended recipient is not on the list

    print(error, "the recipient [", util::hex(*receiver), "] is not connected to the server");
    quest_error(client, "the recipient [" + util::hex(*receiver).to_string() + "] is not connected to the server");
    return;
  }
  // pack invite

  nlohmann::json invite;
  invite["quest"]      = "invite";
  invite["uuid"]       = util::hex(sender).to_string_view();
  invite["remote"] = remote["remote"];

  auto invite_str = invite.dump();
  print(recipient->second, file::raw(util::endian::little((std::uint16_t)invite_str.size())), invite_str);
}

void handle_register(file::io &client, const uuid_t &sender, nlohmann::json &remote) {
  if(peers().find(sender) == peers().cend()) {
    auto it = peers().emplace(sender, std::move(client));

    poll().read(it.first->second, sender);

    nlohmann::json peers_json = {};
    for(auto &[uuid,_] : peers()) {
      peers_json += util::hex(uuid).to_string_view();
    }

    auto peers_str = peers_json.dump();
    print(it.first->second, file::raw(util::endian::little((std::uint16_t)peers_str.size())), peers_str);
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

  try {
    auto remote = nlohmann::json::parse(*remote_str);

    auto quest = remote["quest"].get<std::string_view>();

    print(debug, "handling quest [", quest, "] from :", util::hex(uuid));
    if(quest == "register") {
      // register the client

      handle_register(client, uuid, remote);
    }

    if(quest == "invite") {
      // initiate connection between sender and recipient

      quest_invite(client, uuid, remote);
    }

    if(quest == "accept") {
      // tell both peers to start negotiation

      quest_accept(client, uuid, remote);
    }


    if(quest == "leave") {
      // remote the client from the list

      peers().erase(uuid);
    }

  } catch (const std::exception &e) {
    print(error, "json exception caught: ", e.what());

    poll().remove(client);

    return;
  }
}