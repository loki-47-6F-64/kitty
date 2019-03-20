//
// Created by loki on 19-3-19.
//

#include <gtest/gtest.h>
#include <kitty/blueth/blue_client.h>

using namespace std::literals;
namespace kitty::test {
/* The server needs to stop every time a test ends,
 * thus you can't test starting without stopping */
TEST(blueth_server, start_stop) {
  server::bluetooth blueth;

  bt::HCI hci;

  auto f_ret = std::async(std::launch::async, &server::bluetooth::start, &blueth, [](auto) {}, std::cref(hci));

  while(!blueth.isRunning()) {
    std::this_thread::sleep_for(1ms);
  }

// If this fails, it will hang
  blueth.stop();
  blueth.join();

  EXPECT_LE(0, f_ret.get());
}

//TODO: properly test bluetooth
}
