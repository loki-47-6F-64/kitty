//
// Created by loki on 9-2-19.
//
#include <kitty/util/utility.h>
#include <kitty/log/log.h>
#include <kitty/server/proxy.h>
#include <kitty/util/set.h>
#include <kitty/p2p/pj/nath.h>
#include <kitty/p2p/data_types.h>
#include "quest.h"

namespace p2p::bootstrap {
using namespace std::literals;

using namespace server::proxy;
std::map<uuid_t, file::io> &peers() {
  static std::map<uuid_t, file::io> peers;

  return peers;
}

file::poll_t<file::io, uuid_t> &poll() {
  static file::poll_t<file::io, uuid_t> poll([](file::io &fd, uuid_t uuid) {
    handle_quest(fd);
  }, [](file::io &, uuid_t) { }, [](file::io &fd, uuid_t uuid) {
    print(info, "removing uuid: ", util::hex(uuid));

    peers().erase(uuid);
  });

  return poll;
}

void quest_error(file::io &client, const std::string_view &error) {
  auto err = push(client, "error"sv, util::hex(uuid_t {}).to_string_view(), error);
  if(err) {
    print(::error, "Couldn't send error: ", err::current());
  }
}

void handle_register(file::io &client, const uuid_t &sender) {
  if(peers().find(sender) == peers().cend()) {
    auto it = peers().emplace(sender, std::move(client));

    poll().read(it.first->second, sender);

    push(it.first->second, "register"sv, util::map(peers(), [](auto &el) {
      return el.first;
    }));
  }
}

void handle_quest(file::io &client) {
  uuid_t to, from;
  std::string quest;

  auto err = load(client, to, from, quest);
  if(err) {
    print(error, "Couldn't read quest header: ", err::current());

    return;
  }

  print(debug, "handling quest [", quest, "] to: ", util::hex(to), " from :", util::hex(from));
  if(quest == "register") {
    // register the client

    handle_register(client, from);
  } else if(quest == "leave") {
    // remove the client from the list

    peers().erase(from);
  } else {
    auto peer = peers().find(to);

    if(peer == peers().cend()) {
      // The intended recipient is not on the list
      print(error, "the recipient [", util::hex(to), "] is not connected to the server");

      quest_error(client, "the recipient [" + util::hex(to).to_string() + "] is not connected to the server");
      return;
    }

    if(quest == "invite" || quest == "accept") {
      /* ufrag, upasswd, candidates  */
      auto raw = load_raw<
        file::io::stream_t, data::remote_t
        >(client);

      if(!raw) {
        // The intended recipient is not on the list
        print(error, "Could not load the quest: ", err::current());

        quest_error(client, "Could not load the quest: "s + err::current());
        return;
      }

      push(peer->second, to, from, quest, file::raw(*raw));
    }

    else if(quest == "decline") {
      auto raw = load_raw<
        file::io::stream_t, int
      >(client);

      if(!raw) {
        // The intended recipient is not on the list
        print(error, "Could not load the quest: ", err::current());

        quest_error(client, "Could not load the quest: "s + err::current());
        return;
      }

      push(peer->second, to, from, quest, file::raw(*raw));
    }
  }

}
}