//
// Created by loki on 29-1-19.
//

#include <iostream>
#include <random>

#include <kitty/server/server.h>
#include <kitty/server/tcp_client.h>

#include <kitty/log/log.h>
#include <kitty/pj/uuid.h>
#include <kitty/pj/server/quest.h>

#include <nlohmann/json.hpp>

using namespace std::chrono_literals;
void accept_client(server::tcp::Client &&client) {
  print(debug, "Accepted client :: ", client.ip_addr);

  auto uuid = util::endian::little(file::read_struct<uuid_t>(*client.socket));

  if(!uuid) {
    client.socket->seal();

    print(error, client.ip_addr, ": Could not read uuid: ", err::current());

    return;
  }

  print(info, client.ip_addr, ": uuid: ", util::hex(*uuid));
  handle_quest(*client.socket, *uuid);
}


int main(int args, char *argv[]) {
  std::uint16_t port = 2345;
  if(args > 1) {
    std::size_t _;
    port = (std::uint16_t)std::stoul(argv[1], &_, 10);
  }

  server::tcp server;

  server::tcp::Client::_sockaddr sockaddr { 0 };

  sockaddr.sin6_family = AF_INET6;
  sockaddr.sin6_port   = htons(port);

  util::AutoRun<void> auto_run;

  std::thread worker_thread([&]() {
    auto_run.run([]() {
      poll().poll(500ms);
    });
  });

  return server.start(sockaddr, accept_client);
}