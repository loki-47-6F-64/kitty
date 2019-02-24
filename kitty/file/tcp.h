#ifndef KITTY_TCP_H
#define KITTY_TCP_H

#include <kitty/file/io_stream.h>

namespace file {
struct ip_addr_t {
  std::string_view ip;
  std::uint16_t port;

  std::tuple<std::uint32_t, std::uint16_t> pack() const;
  static ip_addr_t unpack(std::vector<char> &buf, std::tuple<std::uint32_t, std::uint16_t>);
};

io connect(const char *hostname, const char *port);
io connect(const ip_addr_t &ip_addr);
}

#endif