//
// Created by loki on 17-3-19.
//

#include <netinet/in.h>
#include <gtest/gtest.h>
#include <kitty/ssl/ssl_client.h>
#include <kitty/file/tcp.h>

using namespace std::literals;

#define TEST_CA_DIR KITTY_ASSET_DIR "/testCA"
namespace kitty::test {

struct ssl_server_t : testing::Test {
  server::ssl ssl;

  ssl::Context client_ctx;

  std::future<int> f_ret;
  ssl_server_t() :
    ssl { ssl::init_ctx_server(TEST_CA_DIR "/fullchain.pem", TEST_CA_DIR "/server.crt", TEST_CA_DIR "/server.key") },
    client_ctx { ssl::init_ctx_client(TEST_CA_DIR "/fullchain.pem") }
  {
    if(!ssl.get_member().ctx) {
      throw std::runtime_error("server_ctx == nullptr");
    }

    if(!client_ctx) {
      throw std::runtime_error("client_ctx == nullptr");
    }
  }

  ~ssl_server_t() override {
    if(ssl.isRunning()) {
      ssl.stop();
      ssl.join();
    }

    if(f_ret.valid()) {
      f_ret.get();
    }
  }
};

struct ssl_verify_t : testing::Test {
  server::ssl ssl;

  ssl::Context client_ctx_good;
  ssl::Context client_ctx_bad;

  std::future<int> f_ret;
  ssl_verify_t() :
    ssl { ssl::init_ctx_server(TEST_CA_DIR "/server.crt", TEST_CA_DIR "/server.crt", TEST_CA_DIR "/server.key", true) },
    client_ctx_good { ssl::init_ctx_client(TEST_CA_DIR "/server.crt", TEST_CA_DIR "/server.crt", TEST_CA_DIR "/server.key") },
    client_ctx_bad  { ssl::init_ctx_client(TEST_CA_DIR "/server.crt", TEST_CA_DIR "/bad.crt", TEST_CA_DIR "/bad.key") }
  {
    if(!ssl.get_member().ctx) {
      throw std::runtime_error("server_ctx == nullptr");
    }

    if(!client_ctx_bad) {
      throw std::runtime_error("client_ctx_bad == nullptr");
    }

    if(!client_ctx_good) {
      throw std::runtime_error("client_ctx_good == nullptr");
    }
  }

  ~ssl_verify_t() override {
    if(ssl.isRunning()) {
      ssl.stop();
      ssl.join();
    }

    if(f_ret.valid()) {
      f_ret.get();
    }
  }
};

/* The server needs to stop every time a test ends,
 * thus you can't test starting without stopping */
TEST_F(ssl_server_t, start_stop) {
  sockaddr_in addr { };
  addr.sin_family = file::INET;

  auto f_ret = std::async(std::launch::async, &server::ssl::start, &ssl, [](auto) { }, (sockaddr *) &addr, 1);

  while(!ssl.isRunning()) {
    auto f_stat = f_ret.wait_for(1ms);
    if(f_stat == std::future_status::ready) {
      break;
    }
  }

// If this fails, it will hang
  ssl.stop();
  ssl.join();

  EXPECT_LE(0, f_ret.get());
}

TEST_F(ssl_server_t, accept_client_inet4) {
  sockaddr_in addr { };
  addr.sin_family = file::INET;
  addr.sin_port = util::endian::big<std::uint16_t>(2345);

  util::Alarm<bool> has_connected;

  std::string client_ip;
  auto client_accept = [&](server::ssl_client_t &&client) {
    client_ip = std::move(client.ip_addr);

    has_connected.ring(true);
  };

  f_ret = std::async(std::launch::async, &server::ssl::start, &ssl, client_accept, (sockaddr *) &addr, 1);

  while(!ssl.isRunning()) {
    auto f_stat = f_ret.wait_for(1ms);

    if(f_stat == std::future_status::ready) {
      FAIL();
    }
  }

  auto fd = ssl::connect(client_ctx, "localhost", "2345");

  has_connected.wait_for(100ms);

  fd.seal();
  EXPECT_TRUE(has_connected.status());
  EXPECT_FALSE(client_ip.empty());
}

TEST_F(ssl_server_t, accept_client_inet6) {
  sockaddr_in6 addr { };
  addr.sin6_family = file::INET6;
  addr.sin6_port = util::endian::big<std::uint16_t>(2345);

  util::Alarm<bool> has_connected;

  std::string client_ip;
  auto client_accept = [&](server::ssl_client_t &&client) {
    client_ip = std::move(client.ip_addr);

    has_connected.ring(true);
  };

  f_ret = std::async(std::launch::async, &server::ssl::start, &ssl, client_accept, (sockaddr *) &addr, 1);

  while(!ssl.isRunning()) {
    auto f_stat = f_ret.wait_for(1ms);

    if(f_stat == std::future_status::ready) {
      FAIL();
    }
  }

  auto fd = ssl::connect(client_ctx, "localhost", "2345");

  has_connected.wait_for(100ms);

  fd.seal();
  EXPECT_TRUE(has_connected.status());
  EXPECT_FALSE(client_ip.empty());
}

TEST_F(ssl_verify_t, client_verify) {
  sockaddr_in6 addr { };
  addr.sin6_family = file::INET6;
  addr.sin6_port = util::endian::big<std::uint16_t>(2345);

  util::Alarm<bool> has_connected;

  std::string client_ip;
  auto client_accept = [&](server::ssl_client_t &&client) {
    client_ip = std::move(client.ip_addr);

    has_connected.ring(true);
  };

  f_ret = std::async(std::launch::async, &server::ssl::start, &ssl, client_accept, (sockaddr *) &addr, 1);

  while(!ssl.isRunning()) {
    auto f_stat = f_ret.wait_for(1ms);

    if(f_stat == std::future_status::ready) {
      FAIL();
    }
  }

  auto fd_bad = ssl::connect(client_ctx_bad, "localhost", "2345");

  has_connected.wait_for(100ms);

  EXPECT_FALSE(fd_bad.is_open());
  EXPECT_FALSE(has_connected.status());
  EXPECT_TRUE(client_ip.empty());

  has_connected.reset();
  auto fd_good = ssl::connect(client_ctx_good, "localhost", "2345");

  has_connected.wait_for(100ms);

  EXPECT_TRUE(fd_good.is_open());
  EXPECT_TRUE(has_connected.status());
  EXPECT_FALSE(client_ip.empty());
}

/*
 * If someone tries to connect to the server without an ssl socket,
 * the server shouldn't hang
 */
TEST_F(ssl_server_t, accept_client_nohang) {
  sockaddr_in6 addr { };
  addr.sin6_family = file::INET6;
  addr.sin6_port = util::endian::big<std::uint16_t>(2345);

  auto f_ret = std::async(std::launch::async, &server::ssl::start, &ssl, [](auto){}, (sockaddr *) &addr, 1);

  while(!ssl.isRunning()) {
    auto f_stat = f_ret.wait_for(1ms);

    if(f_stat == std::future_status::ready) {
      FAIL();
    }
  }

  auto fd = file::connect("localhost", "2345");

  ssl.stop();
  EXPECT_EQ(std::future_status::ready, f_ret.wait_for(1s));

  fd.seal();
}
}