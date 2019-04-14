//
// Created by loki on 19-3-19.
//

#include <gtest/gtest.h>

#include <kitty/p2p/p2p.h>
#include <kitty/p2p/quest.h>
#include <kitty/server/proxy.h>

using namespace std::literals;

namespace kitty::test {
static std::uint16_t port { 1345 };

struct bootstrap_t : testing::Test {
  util::AutoRun<void> auto_run;
  std::thread bootstrap_thread, auto_run_thread;
  server::tcp bootstrap;

  bootstrap_t() {
    sockaddr_in6 addr {};
    addr.sin6_family = file::INET6;
    addr.sin6_port   = util::endian::big<std::uint16_t>(port);

    bootstrap_thread = std::thread {
      [addr, this]() {
        auto ret = bootstrap.start([](auto &&client) { p2p::bootstrap::handle_quest(*client.socket); }, (sockaddr*)&addr);

        if(ret < 0) {
          FAIL();
        }
      }
    };

    auto_run_thread = std::thread([&]() {
      auto_run.run([&]() {
        if(p2p::bootstrap::poll().poll(500ms)) {
          print(error, "Couldn't poll: ", err::current());

          auto_run.stop();
        }
      });
    });
  }

  ~bootstrap_t() override {
    // prevent address already in use
    ++port;

    if(bootstrap.isRunning()) {
      bootstrap.stop();
      bootstrap.join();
    }

    if(bootstrap_thread.joinable()) {
      bootstrap_thread.join();
    }

    if(auto_run.isRunning()) {
      auto_run.stop();
    }

    if(auto_run_thread.joinable()) {
      auto_run_thread.join();
    }
  }
};

TEST_F(bootstrap_t, start_stop) {
  auto accept_cb = [](bool &asked) {
    return [&](p2p::uuid_t uuid) {
      if(!asked) {
        asked = true;

        return p2p::answer_t::ACCEPT;
      }

      return p2p::answer_t::DECLINE;
    };
  };

  bool asked {};
  server::p2p peer(accept_cb(asked), p2p::uuid_t::generate());

  auto f_ret = std::async(std::launch::async, [&]() {
    auto t = p2p::pj::register_thread();
    return peer.start([](auto){}, { "127.0.0.1", port }, {}, {}, 128);
  });

  while(!peer.isRunning()) {
    auto f_stat = f_ret.wait_for(1ms);
    ASSERT_NE(std::future_status::ready, f_stat);
  }

  peer.stop();

  EXPECT_EQ(std::future_status::ready, f_ret.wait_for(4s));
  EXPECT_LE(0, f_ret.get());
}

TEST_F(bootstrap_t, peer_connect) {
  // some random maxima
  constexpr static auto MAX_ACCEPTED    = 16;
  constexpr static auto MAX_CONNECTIONS = 12;

  auto accept_cb = [](int &asked) {
    return [&](p2p::uuid_t uuid) {
      if(asked) {
        --asked;
        return p2p::answer_t::ACCEPT;
      }

      return p2p::answer_t::DECLINE;
    };
  };

  int asked[3] {
    MAX_ACCEPTED,
    1,
    MAX_ACCEPTED
  };

  p2p::uuid_t uuid[3] {
    p2p::uuid_t::generate(), p2p::uuid_t::generate(), p2p::uuid_t::generate()
  };

  // Peer 0 shall allow  only a single connection at a time
  // Peer 1 shall accept only a single connection at the entirety of the test
  // Peer 2 shall connect with the other peers
  server::p2p peer_0(accept_cb(asked[0]), uuid[0]);
  server::p2p peer_1(accept_cb(asked[1]), uuid[1]);
  server::p2p peer_2(accept_cb(asked[2]), uuid[2]);

  auto f_ret_0 = std::async(std::launch::async, [&]() {
    auto t = p2p::pj::register_thread();

    return peer_0.start([](auto) {}, { "127.0.0.1", port }, {}, {}, MAX_CONNECTIONS);
  });

  auto f_ret_1 = std::async(std::launch::async, [&]() {
    auto t = p2p::pj::register_thread();

    return peer_1.start([](auto) {}, { "127.0.0.1", port }, {}, {}, MAX_CONNECTIONS);
  });

  auto f_ret_2 = std::async(std::launch::async, [&]() {
    auto t = p2p::pj::register_thread();

    return peer_2.start([](auto) {}, { "127.0.0.1", port }, {}, {}, 1);
  });

  while(!peer_2.isRunning()) {
    auto f_stat = f_ret_2.wait_for(1ms);
    if(f_stat == std::future_status::ready) {
      FAIL();
      return;
    }
  }

  while(!peer_1.isRunning()) {
    auto f_stat = f_ret_1.wait_for(1ms);
    if(f_stat == std::future_status::ready) {
      FAIL();
      return;
    }
  }

  while(!peer_0.isRunning()) {
    auto f_stat = f_ret_0.wait_for(1ms);
    if(f_stat == std::future_status::ready) {
      FAIL();
      return;
    }
  }

  /* Test max peers
   * Test user declines
   * Test multiple connections at once
   */
  //connect peer_2 --> peer_1, peer_1 accepts
  //connect peer_0 --> peer_1, peer_1 declines
  //connect peer_0 --> peer_2, peer_2 declines without prompt (maximum peers reached)
  //connect peer_1 --> peer_0, peer_0 accepts


  ////////////////////////////////////////////////////////////////////////////////
  auto answer_0 = peer_2.get_member().invite(uuid[1]);
  EXPECT_TRUE(std::holds_alternative<file::p2p>(answer_0));

  // Test accept callback called
  EXPECT_EQ(0, asked[1]);

  if(std::holds_alternative<file::p2p>(answer_0)) {
    auto &fd = std::get<file::p2p>(answer_0);

    EXPECT_TRUE(fd.is_open());
  }

  ////////////////////////////////////////////////////////////////////////////////
  auto answer_1 = peer_0.get_member().invite(uuid[1]);
  EXPECT_TRUE(std::holds_alternative<p2p::answer_t>(answer_1));

  if(std::holds_alternative<file::p2p>(answer_1)) {
    auto &fd = std::get<file::p2p>(answer_1);

    fd.seal();
  }

  ////////////////////////////////////////////////////////////////////////////////
  auto answer_2 = peer_0.get_member().invite(uuid[2]);
  EXPECT_TRUE(std::holds_alternative<p2p::answer_t>(answer_2));

  // Test accept callback not called
  EXPECT_EQ(MAX_ACCEPTED, asked[2]);

  if(std::holds_alternative<file::p2p>(answer_2)) {
    auto &fd = std::get<file::p2p>(answer_2);

    fd.seal();
  }

  ////////////////////////////////////////////////////////////////////////////////
  auto answer_3 = peer_1.get_member().invite(uuid[0]);
  EXPECT_TRUE(std::holds_alternative<file::p2p>(answer_3));

  // Test accept callback called
  EXPECT_EQ(MAX_ACCEPTED -1, asked[0]);

  if(std::holds_alternative<file::p2p>(answer_3)) {
    auto &fd = std::get<file::p2p>(answer_3);

    EXPECT_TRUE(fd.is_open());
  }

  peer_0.stop();
  peer_1.stop();
  peer_2.stop();

  EXPECT_LE(0, f_ret_2.get());
  EXPECT_LE(0, f_ret_1.get());
  EXPECT_LE(0, f_ret_0.get());
}

void p2p_accept_client(server::peer_t &&client) {
  print(debug, "p2p_accept_client");
  auto &fd = *client.socket;
  if(!fd.is_open()) {
    GTEST_FATAL_FAILURE_("client.socket->is_open() is False");
  }

  std::string in;

  int err { 0 };

  EXPECT_EQ(0, err = server::proxy::push(fd, "hello test!"sv));

  if(err) {
    print(error, "Couldn't push data: ", err::current());
  }

  EXPECT_EQ(0, err = server::proxy::load(fd, in));

  if(err) {
    print(error, "Couldn't load data: ", err::current());
  }

  ASSERT_EQ("hello test!"sv, in);
}

TEST_F(bootstrap_t, send_msg) {
  constexpr static auto MAX_CONNECTIONS = 12;

  auto accept_cb = [] (auto) { return p2p::answer_t::ACCEPT; };

  p2p::uuid_t uuid[2] {
    p2p::uuid_t::generate(), p2p::uuid_t::generate()
  };

  server::p2p peer_0(accept_cb, uuid[0]);
  server::p2p peer_1(accept_cb, uuid[1]);

  auto f_ret_0 = std::async(std::launch::async, [&]() {
    auto t = p2p::pj::register_thread();

    return peer_0.start([](auto) {}, { "127.0.0.1", port }, {}, {}, MAX_CONNECTIONS);
  });

  auto g_0 = util::fail_guard([&]() {
    peer_0.stop();
  });

  auto f_ret_1 = std::async(std::launch::async, [&]() {
    auto t = p2p::pj::register_thread();

    return peer_1.start(&p2p_accept_client, { "127.0.0.1", port }, {}, {}, MAX_CONNECTIONS);
  });

  auto g_1 = util::fail_guard([&]() {
    peer_1.stop();
  });

  while(!peer_1.isRunning()) {
    auto f_stat = f_ret_1.wait_for(1ms);
    if(f_stat == std::future_status::ready) {
      break;
    }
  }

  while(!peer_0.isRunning()) {
    auto f_stat = f_ret_0.wait_for(1ms);
    if(f_stat == std::future_status::ready) {
      break;
    }
  }

  // connect peer_0 --> peer_1
  auto p2p_fd = peer_0.get_member().invite(uuid[1]);

  if(std::holds_alternative<p2p::answer_t>(p2p_fd)) {
    auto answer = std::get<p2p::answer_t>(p2p_fd);

    print(error, "answer: ", answer.to_string_view());
  }

  ASSERT_TRUE(std::holds_alternative<file::p2p>(p2p_fd));

  std::string str_in;
  ASSERT_EQ(0, server::proxy::push(std::get<file::p2p>(p2p_fd), "hello test!"sv));
  ASSERT_EQ(0, server::proxy::load(std::get<file::p2p>(p2p_fd), str_in));
  ASSERT_EQ("hello test!"sv, str_in);

  peer_0.stop();
  peer_1.stop();

  EXPECT_LE(0, f_ret_1.get());
  EXPECT_LE(0, f_ret_0.get());
}

//TODO: test for incorrect input
}