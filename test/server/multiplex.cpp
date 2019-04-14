//
// Created by loki on 11-4-19.
//

#include <gtest/gtest.h>
#include <kitty/server/proxy.h>
#include <kitty/server/multiplex.h>

namespace kitty::test {
using namespace std::literals;


TEST(multiplex, start_stop) {
  server::multiplex multiplex;

  util::Alarm<bool> alarm;
  std::string hello_world;

  auto f = std::async(std::launch::async, &server::multiplex::start, &multiplex, [&](server::demux_client_t &&client) {
    EXPECT_TRUE(client.socket.is_open());

    server::proxy::load(client.socket, hello_world);

    alarm.ring(true);
  }, 0);

  auto g = util::fail_guard([&]() {
    multiplex.stop();

    if(std::future_status::ready != f.wait_for(4s)) {
      std::abort();
    }
  });

  while(!multiplex.isRunning()) {
    auto f_stat = f.wait_for(1ms);
    if(f_stat == std::future_status::ready) {
      FAIL();
    }
  }
}

TEST(multiplex, accept) {
  server::multiplex multiplex;

  util::Alarm<bool> alarm;
  std::string hello_world;

  auto f = std::async(std::launch::async, &server::multiplex::start, &multiplex, [&](server::demux_client_t &&client) {
    EXPECT_TRUE(client.socket.is_open());

    server::proxy::load(client.socket, hello_world);

    alarm.ring(true);
  }, 0);

  auto g = util::fail_guard([&]() {
    multiplex.stop();

    if(std::future_status::ready != f.wait_for(4s)) {
      std::abort();
    }
  });

  while(!multiplex.isRunning()) {
    auto f_stat = f.wait_for(1ms);
    if(f_stat == std::future_status::ready) {
      FAIL();
    }
  }

  auto addr = multiplex.get_member().local_addr();
  ASSERT_FALSE(addr.ip.empty());

  auto sock = file::udp(addr);

  EXPECT_EQ(0, server::proxy::push(sock, "hello test!"sv));

  // test for contact
  EXPECT_TRUE(alarm.wait_for(1s));
  EXPECT_EQ("hello test!"sv, hello_world);
}

TEST(multiplex, demultiplex) {
  server::multiplex multiplex;

  util::Alarm<file::demultiplex> alarm_0;
  util::Alarm<file::demultiplex> alarm_1;
  util::Alarm<file::demultiplex> alarm_2;

  auto f = std::async(std::launch::async, &server::multiplex::start, &multiplex, [&](server::demux_client_t &&client) {
    EXPECT_TRUE(client.socket.is_open());

    if(!alarm_0.status()) {
      alarm_0.ring(std::move(client.socket));
    }

    else if(!alarm_1.status()) {
      alarm_1.ring(std::move(client.socket));
    }

    else if(!alarm_2.status()) {
      alarm_2.ring(std::move(client.socket));
    }
  }, 0);

  auto g = util::fail_guard([&]() {
    multiplex.stop();

    if(std::future_status::ready != f.wait_for(4s)) {
      std::abort();
    }
  });

  while(!multiplex.isRunning()) {
    auto f_stat = f.wait_for(1ms);
    if(f_stat == std::future_status::ready) {
      FAIL();
    }
  }

  auto addr = multiplex.get_member().local_addr();
  ASSERT_FALSE(addr.ip.empty());

  auto sock_0 = file::udp(addr);
  auto sock_1 = file::udp(addr);
  auto sock_2 = file::udp(addr);

  sock_0.timeout() = 50ms;
  sock_1.timeout() = 50ms;
  sock_2.timeout() = 50ms;

  print(sock_0, '0');
  print(sock_1, '1');
  print(sock_2, '2');

  EXPECT_TRUE(alarm_0.wait_for(1s));
  EXPECT_EQ('0', file::read_struct<char>(*alarm_0.status()));

  EXPECT_TRUE(alarm_1.wait_for(1s));
  EXPECT_EQ('1', file::read_struct<char>(*alarm_1.status()));

  EXPECT_TRUE(alarm_2.wait_for(1s));
  EXPECT_EQ('2', file::read_struct<char>(*alarm_2.status()));

  alarm_0.status()->timeout() = 50ms;
  alarm_1.status()->timeout() = 50ms;
  alarm_2.status()->timeout() = 50ms;

  ASSERT_EQ(0, server::proxy::push(sock_0, -1234));
  EXPECT_EQ(-1234, file::read_struct<int>(*alarm_0.status()));
  EXPECT_EQ(std::nullopt, file::read_struct<int>(*alarm_1.status()));
  EXPECT_EQ(std::nullopt, file::read_struct<int>(*alarm_2.status()));

  ASSERT_EQ(0, server::proxy::push(sock_1, -1234));
  EXPECT_EQ(std::nullopt, file::read_struct<int>(*alarm_0.status()));
  EXPECT_EQ(-1234, file::read_struct<int>(*alarm_1.status()));
  EXPECT_EQ(std::nullopt, file::read_struct<int>(*alarm_2.status()));

  ASSERT_EQ(0, server::proxy::push(*alarm_0.status(), -1234));
  EXPECT_EQ(-1234, file::read_struct<int>(sock_0));
  EXPECT_EQ(std::nullopt, file::read_struct<int>(sock_1));
  EXPECT_EQ(std::nullopt, file::read_struct<int>(sock_2));

  ASSERT_EQ(0, server::proxy::push(*alarm_1.status(), -1234));
  EXPECT_EQ(std::nullopt, file::read_struct<int>(sock_0));
  EXPECT_EQ(-1234, file::read_struct<int>(sock_1));
  EXPECT_EQ(std::nullopt, file::read_struct<int>(sock_2));
}

TEST(multiplex, close) {
  server::multiplex multiplex;

  util::Alarm<file::demultiplex> alarm;

  auto f = std::async(std::launch::async, &server::multiplex::start, &multiplex, [&](server::demux_client_t &&client) {
    EXPECT_TRUE(client.socket.is_open());

    std::string hello_world;
    server::proxy::load(client.socket, hello_world);

    alarm.ring(std::move(client.socket));
  }, 0);

  auto g = util::fail_guard([&]() {
    multiplex.stop();

    if(std::future_status::ready != f.wait_for(4s)) {
      std::abort();
    }
  });

  while(!multiplex.isRunning()) {
    auto f_stat = f.wait_for(1ms);
    if(f_stat == std::future_status::ready) {
      FAIL();
    }
  }

  auto addr = multiplex.get_member().local_addr();
  ASSERT_FALSE(addr.ip.empty());

  auto sock_0 = file::udp(addr);

  EXPECT_EQ(0, server::proxy::push(sock_0, "hello test!"sv));

  // test for contact
  ASSERT_TRUE(alarm.wait_for(1s));

  auto demux { *std::move(alarm.status()) };
  alarm.reset();

  EXPECT_EQ(0, server::proxy::push(sock_0, ':'));
  EXPECT_EQ(':', file::read_struct<char>(demux));

  // test no contact made
  EXPECT_FALSE(alarm.status());

  demux.seal();

  EXPECT_EQ(0, server::proxy::push(sock_0, "hello test!"sv));

  // test for contact
  ASSERT_TRUE(alarm.wait_for(1s));
}
}