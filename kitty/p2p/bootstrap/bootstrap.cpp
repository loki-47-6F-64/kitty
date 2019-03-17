//
// Created by loki on 29-1-19.
//

#include <iostream>
#include <random>

#include <kitty/server/server.h>
#include <kitty/log/log.h>
#include <kitty/p2p/uuid.h>
#include <kitty/p2p/bootstrap/quest.h>
#include <cstring>

using namespace std::literals;
namespace p2p::bootstrap {

void accept_client(server::tcp::client_t &&client) {
  print(debug, "Accepted client :: ", client.ip_addr);

  handle_quest(*client.socket);
}

}

int main(int argc, char *argv[]) {
  std::uint16_t port = 2345;
  auto family = AF_INET6;

  sockaddr_storage addr {};
  //std::memset(&addr, 0, sizeof(addr));
  std::for_each(argv +1, argv + argc, [&](char *str) {
    std::string_view arg { str };

    if(arg == "--inet4"sv) {
      family = AF_INET;

      return;
    }

    auto n_end = arg.find('=');

    if(n_end == std::string_view::npos || n_end == arg.size()) {
      return;
    }

    if(arg.substr(0, n_end) == "--port"sv) {
      std::size_t _;
      port = (std::uint16_t) std::stoul(arg.data() + n_end +1, &_, 10);
    }

    if(arg.substr(0, n_end) == "--log"sv) {
      file::log_open(arg.data() + n_end +1);
    }
  });

  if(family == AF_INET6) {
    auto tmp_addr = (sockaddr_in6*)&addr;

    tmp_addr->sin6_family = AF_INET6;
    tmp_addr->sin6_port = util::endian::big(port);
  }
  else {
    auto tmp_addr = (sockaddr_in*)&addr;

    tmp_addr->sin_family = AF_INET;
    tmp_addr->sin_port = util::endian::big(port);
  }

  DEBUG_LOG("Initialising tcp server");
  server::tcp server;
  util::AutoRun<void> auto_run;

  DEBUG_LOG("Starting worker thread");
  std::thread worker_thread([&]() {
    auto_run.run([]() {
      p2p::bootstrap::poll().poll(500ms);
    });
  });

  DEBUG_LOG("starting server...");
  return server.start(p2p::bootstrap::accept_client, (sockaddr*)&addr);
}
