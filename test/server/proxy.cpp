//
// Created by loki on 8-4-19.
//

#include <gtest/gtest.h>
#include <kitty/util/set.h>
#include <kitty/server/proxy.h>
#include <kitty/file/tcp.h>

using namespace std::literals;
namespace kitty::test {

TEST(proxy_t, push) {
  auto sock_in = file::udp_init();
  auto port_in = file::sockport(sock_in);

  auto sock_out = file::udp({ "127.0.0.1"sv, port_in });
  auto port_out = file::sockport(sock_out);

  file::udp_connect(sock_in, { "127.0.0.1"sv, port_out });

  auto tuple = std::make_tuple(':', (uint16_t)2345);
  std::vector data_char { 'h', 'e', 'l', 'l', 'o', ' ', 't', 'e', 's', 't', '!' };
  std::vector data_str {
    "hello test! 0"sv,
    "hello test! 1"sv,
  };

  std::string_view data_char_str { data_char.data(), data_char.size() };

  ASSERT_EQ(0, server::proxy::push(sock_out, ':', (uint16_t)2345));
  ASSERT_EQ(0, server::proxy::push(sock_out, "hello test!"sv));
  ASSERT_EQ(0, server::proxy::push(sock_out));
  ASSERT_EQ(0, server::proxy::push(sock_out, "hello test!"s));
  ASSERT_EQ(0, server::proxy::push(sock_out, data_char));
  ASSERT_EQ(0, server::proxy::push(sock_out, data_str));
  ASSERT_EQ(0, server::proxy::push(sock_out, tuple));

  // primitives
  ASSERT_EQ(':', util::endian::little(file::read_struct<char>(sock_in)));
  ASSERT_EQ(2345, util::endian::little(file::read_struct<uint16_t>(sock_in)));

  // string_view
  ASSERT_EQ(/* sizeof string */ 11, util::endian::little(file::read_struct<uint16_t>(sock_in)));
  ASSERT_EQ("hello test!"sv, file::read_string(sock_in, 11));

  // string
  ASSERT_EQ(/* sizeof string */ 11, util::endian::little(file::read_struct<uint16_t>(sock_in)));
  ASSERT_EQ("hello test!"sv, file::read_string(sock_in, 11));

  // vector data_char
  ASSERT_EQ(data_char.size(), util::endian::little(file::read_struct<uint16_t>(sock_in)));
  ASSERT_EQ(data_char_str, file::read_string(sock_in, data_char.size()));

  // vector data_str
  ASSERT_EQ(data_str.size(), util::endian::little(file::read_struct<uint16_t>(sock_in)));
  ASSERT_EQ(data_str[0].size(), util::endian::little(file::read_struct<uint16_t>(sock_in)));
  ASSERT_EQ(data_str[0], file::read_string(sock_in, data_str[0].size()));
  ASSERT_EQ(data_str[1].size(), util::endian::little(file::read_struct<uint16_t>(sock_in)));
  ASSERT_EQ(data_str[1], file::read_string(sock_in, data_str[1].size()));

  // tuple<char, uint16_t>
  ASSERT_EQ(':', util::endian::little(file::read_struct<char>(sock_in)));
  ASSERT_EQ(2345, util::endian::little(file::read_struct<uint16_t>(sock_in)));


  std::vector data_vec_int {
    std::vector { 5, 8, 1, -5 },
    std::vector { 2, -1 }
  };

  std::tuple<std::tuple<std::string, std::string>, std::tuple<std::vector<std::vector<int>>, int>> tuple_vec =
    std::make_tuple(std::make_tuple("hello"s, "world!"s), std::make_tuple(std::move(data_vec_int), -1234));

  ASSERT_EQ(0, server::proxy::push(sock_out, tuple_vec));
  // std::tuple<std::string, std::string>
  ASSERT_EQ(5, util::endian::little(file::read_struct<std::uint16_t>(sock_in)));
  ASSERT_EQ("hello"sv, file::read_string(sock_in, 5));
  ASSERT_EQ(6, util::endian::little(file::read_struct<std::uint16_t>(sock_in)));
  ASSERT_EQ("world!"sv, file::read_string(sock_in, 6));

  // std::tuple<std::vector<std::vector<int>>, int>
  ASSERT_EQ(2, file::read_struct<std::uint16_t>(sock_in));
  ASSERT_EQ(4, file::read_struct<std::uint16_t>(sock_in));
  ASSERT_EQ(5, file::read_struct<int>(sock_in));
  ASSERT_EQ(8, file::read_struct<int>(sock_in));
  ASSERT_EQ(1, file::read_struct<int>(sock_in));
  ASSERT_EQ(-5, file::read_struct<int>(sock_in));
  ASSERT_EQ(2, file::read_struct<std::uint16_t>(sock_in));
  ASSERT_EQ(2, file::read_struct<int>(sock_in));
  ASSERT_EQ(-1, file::read_struct<int>(sock_in));
  ASSERT_EQ(-1234, file::read_struct<int>(sock_in));
}

// If proxy_t_push fails, then this test will also fail
TEST(proxy_t, load) {
  auto sock_in = file::udp_init();
  auto port_in = file::sockport(sock_in);

  auto sock_out = file::udp({ "127.0.0.1"sv, port_in });
  auto port_out = file::sockport(sock_out);

  file::udp_connect(sock_in, { "127.0.0.1"sv, port_out });

  auto tuple = std::make_tuple(':', (uint16_t)2345);
  std::vector data_char { 'h', 'e', 'l', 'l', 'o', ' ', 't', 'e', 's', 't', '!' };
  std::vector data_str {
    "hello test! 0"sv,
    "hello test! 1"sv,
  };

  std::vector data_vec_int {
    std::vector { 5, 8, 1, -5 },
    std::vector { 2, -1 }
  };

  ASSERT_EQ(0, server::proxy::push(sock_out, ':', (uint16_t)2345));
  ASSERT_EQ(0, server::proxy::push(sock_out, "hello test!"sv));
  ASSERT_EQ(0, server::proxy::push(sock_out));
  ASSERT_EQ(0, server::proxy::push(sock_out, "hello test!"s));
  ASSERT_EQ(0, server::proxy::push(sock_out, data_char));
  ASSERT_EQ(0, server::proxy::push(sock_out, data_str));
  ASSERT_EQ(0, server::proxy::push(sock_out, data_vec_int));
  ASSERT_EQ(0, server::proxy::push(sock_out, tuple));

  char     in_char;
  uint16_t in_uint16_t;
  std::string in_string;
  std::vector<char> in_data_char;
  std::vector<std::string> in_data_str;
  std::vector<std::vector<int>> in_data_vec_int;
  std::tuple<char, uint16_t> in_tuple;

  ASSERT_EQ(0, server::proxy::load(sock_in, in_char, in_uint16_t));
  ASSERT_EQ(0, server::proxy::load(sock_in, in_string));
  ASSERT_EQ(0, server::proxy::load(sock_in));
  ASSERT_EQ(0, server::proxy::load(sock_in, in_string));
  ASSERT_EQ(0, server::proxy::load(sock_in, in_data_char));
  ASSERT_EQ(0, server::proxy::load(sock_in, in_data_str));
  ASSERT_EQ(0, server::proxy::load(sock_in, in_data_vec_int));
  ASSERT_EQ(0, server::proxy::load(sock_in, in_tuple));


  ASSERT_EQ(':', in_char);
  ASSERT_EQ(2345, in_uint16_t);
  ASSERT_EQ("hello test!"sv, in_string);
  ASSERT_EQ(data_char, in_data_char);
  ASSERT_EQ(data_str, util::map(in_data_str, [](const auto &str) { return std::string_view { str }; }));
  ASSERT_EQ(data_vec_int, in_data_vec_int);
  ASSERT_EQ(tuple, in_tuple);
}

TEST(proxy_t, load_raw) {
  auto sock_in = file::udp_init();
  auto port_in = file::sockport(sock_in);

  auto sock_out = file::udp({ "127.0.0.1"sv, port_in });
  auto port_out = file::sockport(sock_out);

  sock_in.timeout() = sock_out.timeout() = 50ms;
  file::udp_connect(sock_in, { "127.0.0.1"sv, port_out });

  auto tuple = std::make_tuple(':', (uint16_t)2345);
  std::vector data_char { 'h', 'e', 'l', 'l', 'o', ' ', 't', 'e', 's', 't', '!' };
  std::vector data_str {
    "hello test! 0"sv,
    "hello test! 1"sv,
  };

  std::vector data_vec_int {
    std::vector { 5, 8, 1, -5 },
    std::vector { 2, -1 }
  };

  std::tuple<std::tuple<std::string, std::string>, std::tuple<std::vector<std::vector<int>>, int>> tuple_vec =
    std::make_tuple(std::make_tuple("hello"s, "world"s), std::make_tuple(std::move(data_vec_int), -1234));

  ASSERT_EQ(0, server::proxy::push(sock_out, ':', (uint16_t)2345));
  ASSERT_EQ(0, server::proxy::push(sock_out, "hello test!"sv));
  ASSERT_EQ(0, server::proxy::push(sock_out));
  ASSERT_EQ(0, server::proxy::push(sock_out, "hello test!"s));
  ASSERT_EQ(0, server::proxy::push(sock_out, data_char));
  ASSERT_EQ(0, server::proxy::push(sock_out, data_str));
  ASSERT_EQ(0, server::proxy::push(sock_out, tuple_vec));
  ASSERT_EQ(0, server::proxy::push(sock_out, tuple));


  auto loaded = server::proxy::load_raw<
    char,
    std::uint16_t,
    std::string,
    std::string,
    std::vector<char>,
    std::vector<std::string>,
    std::tuple<std::tuple<std::string, std::string>, std::tuple<std::vector<std::vector<int>>, int>>,
    std::tuple<char, uint16_t>
  >(sock_in);

  ASSERT_NE(std::nullopt, loaded);

  ASSERT_EQ(0, server::proxy::push(sock_in, file::raw(*loaded)));

  char     in_char;
  uint16_t in_uint16_t;
  std::string in_string;
  std::vector<char> in_data_char;
  std::vector<std::string> in_data_str;
  std::tuple<std::tuple<std::string, std::string>, std::tuple<std::vector<std::vector<int>>, int>> in_data_vec_int;
  std::tuple<char, uint16_t> in_tuple;

  ASSERT_EQ(0, server::proxy::load(sock_out, in_char, in_uint16_t));
  ASSERT_EQ(0, server::proxy::load(sock_out, in_string));
  ASSERT_EQ(0, server::proxy::load(sock_out));
  ASSERT_EQ(0, server::proxy::load(sock_out, in_string));
  ASSERT_EQ(0, server::proxy::load(sock_out, in_data_char));
  ASSERT_EQ(0, server::proxy::load(sock_out, in_data_str));
  ASSERT_EQ(0, server::proxy::load(sock_out, in_data_vec_int));
  ASSERT_EQ(0, server::proxy::load(sock_out, in_tuple));
}

}