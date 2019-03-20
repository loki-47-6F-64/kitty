//
// Created by loki on 17-3-19.
//

#include <gtest/gtest.h>
#include <kitty/p2p/p2p.h>

int main(int argc, char *argv[]) {
  file::log_open("test.log");

  p2p::init();

  testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}