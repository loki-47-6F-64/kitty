//
// Created by loki on 17-3-19.
//

#include <gtest/gtest.h>
#include <kitty/server/server.h>
#include <kitty/file/tcp.h>

using namespace std::literals;
namespace kitty::test {

/* The server needs to stop every time a test ends,
 * thus you can't test starting without stopping */
TEST(tcp_server, start_stop) {
  server::tcp tcp;

  sockaddr_in addr {};
  addr.sin_family = file::INET;
  auto f_ret = std::async(std::launch::async, &server::tcp::start, &tcp, [](auto) {}, (sockaddr*)&addr);

  while(!tcp.isRunning()) {
    auto f_stat = f_ret.wait_for(1ms);
    if(f_stat == std::future_status::ready) {
      FAIL();
    }
  }

  // If this fails, it will hang
  tcp.stop();
  tcp.join();

  EXPECT_LE(0, f_ret.get());
}

TEST(tcp_server, accept_client_inet4) {
  server::tcp tcp;

  sockaddr_in addr {};
  addr.sin_family = file::INET;
  addr.sin_port   = util::endian::big<std::uint16_t>(2345);

  util::Alarm<bool> has_connected;

  std::string client_ip;
  auto client_accept = [&](server::tcp_client_t &&client) {
    client_ip = std::move(client.ip_addr);

    has_connected.ring(true);
  };

  auto f_ret = std::async(std::launch::async, &server::tcp::start, &tcp, client_accept, (sockaddr*)&addr);

  while(!tcp.isRunning()) {
    auto f_stat = f_ret.wait_for(1ms);
    if(f_stat == std::future_status::ready) {
      FAIL();
    }
  }

  auto fd = file::connect("localhost", "2345");
  has_connected.wait_for(100ms);
  fd.seal();

  // If this fails, it will hang
  tcp.stop();
  tcp.join();

  EXPECT_TRUE(has_connected.status());
  EXPECT_FALSE(client_ip.empty());
}

TEST(tcp_server, accept_client_inet6) {
  server::tcp tcp;

  sockaddr_in6 addr {};
  addr.sin6_family = file::INET6;
  addr.sin6_port   = util::endian::big<std::uint16_t>(2345);

  util::Alarm<bool> has_connected;

  std::string client_ip;
  auto client_accept = [&](server::tcp_client_t &&client) {
    client_ip = std::move(client.ip_addr);

    has_connected.ring(true);
  };

  auto f_ret = std::async(std::launch::async, &server::tcp::start, &tcp, client_accept, (sockaddr*)&addr);

  while(!tcp.isRunning()) {
    auto f_stat = f_ret.wait_for(1ms);
    if(f_stat == std::future_status::ready) {
      FAIL();
    }
  }

  auto fd = file::connect("localhost", "2345");
  has_connected.wait_for(100ms);
  fd.seal();

  // If this fails, it will hang
  tcp.stop();
  tcp.join();

  EXPECT_TRUE(has_connected.status());
  EXPECT_FALSE(client_ip.empty());
}
}