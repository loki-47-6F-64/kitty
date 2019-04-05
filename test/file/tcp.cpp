//
// Created by loki on 1-4-19.
//

#include <gtest/gtest.h>
#include <netinet/in.h>
#include <kitty/util/utility.h>
#include <kitty/file/tcp.h>

namespace kitty::test {
using namespace std::literals;

TEST(ip_addr, to_from_sockaddr) {
  // 192.168.0.1
  uint32_t ip_packed = util::endian::big<uint32_t>(3232235521);

  sockaddr_in addr { };

  addr.sin_port = util::endian::big<uint16_t>(2345);
  addr.sin_family = file::INET;

  std::vector<char> buf;
  auto ip     = file::ip_addr_t::from_sockaddr(buf, (sockaddr*)&addr);
  auto ip_buf = file::ip_addr_buf_t::from_sockaddr((sockaddr*)&addr);

  EXPECT_EQ(ip    .ip, "0.0.0.0"sv);
  EXPECT_EQ(ip_buf.ip, "0.0.0.0"sv);

  addr.sin_addr.s_addr = ip_packed;

  ip     = file::ip_addr_t::from_sockaddr(buf, (sockaddr*)&addr);
  ip_buf = file::ip_addr_buf_t::from_sockaddr((sockaddr*)&addr);

  EXPECT_EQ(ip    .ip, "192.168.0.1"sv);
  EXPECT_EQ(ip_buf.ip, "192.168.0.1"sv);


  EXPECT_EQ(ip    .port, 2345);
  EXPECT_EQ(ip_buf.port, 2345);

  sockaddr_storage addr_0 = *ip.to_sockaddr();
  sockaddr_storage addr_1 = *ip_buf.to_sockaddr();
  EXPECT_EQ(file::INET, addr_0.ss_family);
  EXPECT_EQ(file::INET, addr_1.ss_family);

  ip = { "192.168.0.1"sv, 2345 };

  auto addr_id = ip.to_sockaddr();

  EXPECT_EQ(((sockaddr_in*)&*addr_id)->sin_addr.s_addr, ip_packed);
  EXPECT_EQ(((sockaddr_in*)&*addr_id)->sin_port, util::endian::big<uint16_t>(2345));

  auto addr_buf_id = ip_buf.to_sockaddr();

  EXPECT_EQ(((sockaddr_in*)&*addr_buf_id)->sin_addr.s_addr, ip_packed);
  EXPECT_EQ(((sockaddr_in*)&*addr_buf_id)->sin_port, util::endian::big<uint16_t>(2345));
}
}