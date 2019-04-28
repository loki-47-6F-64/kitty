//
// Created by loki on 15-4-19.
//

#include <gtest/gtest.h>
#include <kitty/server/proxy.h>
#include <kitty/p2p/kademlia/kademlia.h>
#include <kitty/p2p/kademlia/data_types.h>


namespace p2p {
using namespace std::literals;
enum class __kademlia_e : std::uint8_t {
  PING,
  LOOKUP,
  RESPONSE
};

std::string_view from_type(__kademlia_e type);
}

namespace kitty::test {

using namespace std::literals;
TEST(kademlia, join_network) {
  file::multiplex sock = file::udp_init();

  auto port = file::sockport(sock);
  auto uuid = p2p::uuid_t::generate();

  p2p::Kademlia kad {
    p2p::uuid_t::generate(),
    20,
    3,
    3600s,
    4000s,
    100ms
  };

  std::vector stable_nodes {
    p2p::node_t { uuid, file::ip_addr_buf_t { "127.0.0.1"s, port } }
  };

  auto result = std::make_shared<util::Alarm<err::code_t>>();
  auto f_ret = std::async(std::launch::async,
    &p2p::Kademlia::start, &kad, util::cmove(stable_nodes), 0, result);

  auto g = util::fail_guard([&] () {
    kad.stop();
  });

  while(!kad.is_running()) {
    auto f_stat = f_ret.wait_for(1ms);
    if(f_stat == std::future_status::ready) {
      FAIL();
    }
  }

  TUPLE_2D(addr, uuid_peer, kad.addr());

  sockaddr_storage storage;
  auto addr_p = (sockaddr*)&storage;

  auto msg_id = file::read_struct<p2p::Kademlia::msg_id_t>(sock, addr_p);
  ASSERT_NE(std::nullopt, msg_id);

  ASSERT_EQ(uuid_peer, file::read_struct<p2p::uuid_t>(sock, addr_p));
  ASSERT_EQ(p2p::__kademlia_e::LOOKUP, file::read_struct<p2p::__kademlia_e>(sock, addr_p));
  ASSERT_EQ(uuid_peer, file::read_struct<p2p::uuid_t>(sock, addr_p));

  ASSERT_EQ(0, server::proxy::push(sock, std::make_tuple(addr_p), *msg_id, uuid, p2p::__kademlia_e::RESPONSE));

  ASSERT_TRUE(result->wait_for(500ms));
  ASSERT_EQ(0, result->status());
}

TEST(kademlia, join_network_timeout) {
  file::multiplex sock = file::udp_init();

  auto port = file::sockport(sock);
  auto uuid = p2p::uuid_t::generate();

  p2p::Kademlia kad {
    p2p::uuid_t::generate(),
    20,
    3,
    3600s,
    4000s,
    100ms
  };

  std::vector stable_nodes {
    p2p::node_t { uuid, file::ip_addr_buf_t { "127.0.0.1"s, port } }
  };

  auto result = std::make_shared<util::Alarm<err::code_t>>();
  auto f_ret = std::async(std::launch::async,
                          &p2p::Kademlia::start, &kad, util::cmove(stable_nodes), 0, std::move(result));

  auto g = util::fail_guard([&] () {
    kad.stop();
  });

  while(!kad.is_running()) {
    auto f_stat = f_ret.wait_for(1ms);
    if(f_stat == std::future_status::ready) {
      FAIL();
    }
  }

  TUPLE_2D(addr, uuid_peer, kad.addr());

  sockaddr_storage storage;
  auto addr_p = (sockaddr*)&storage;

  auto msg_id = file::read_struct<p2p::Kademlia::msg_id_t>(sock, addr_p);
  ASSERT_NE(std::nullopt, msg_id);

  ASSERT_EQ(uuid_peer, file::read_struct<p2p::uuid_t>(sock, addr_p));
  ASSERT_EQ(p2p::__kademlia_e::LOOKUP, file::read_struct<p2p::__kademlia_e>(sock, addr_p));
  ASSERT_EQ(uuid_peer, file::read_struct<p2p::uuid_t>(sock, addr_p));

  ASSERT_TRUE(result->wait_for(500ms));
  ASSERT_EQ(err::TIMEOUT, result->status());
}


}