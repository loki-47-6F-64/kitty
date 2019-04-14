//
// Created by loki on 11-4-19.
//

#ifndef T_MAN_MULTIPLEX_H
#define T_MAN_MULTIPLEX_H

#include <kitty/util/unordered_map_key.h>
#include <kitty/util/utility.h>
#include <kitty/server/server.h>
#include <kitty/p2p/uuid.h>
#include <kitty/p2p/upnp/upnp.h>
#include <kitty/server/multiplex_stream.h>

KITTY_GEN_HASH_KEY(server, ip_addr_key, file::ip_addr_buf_t, file::ip_addr_t)

namespace server {

class Multiplex {
public:
  using hash_to_fd = std::unordered_map<ip_addr_key, std::shared_ptr<file::stream::mux::pipe_t>>;

  Multiplex(std::shared_ptr<__multiplex> &&mux, file::ip_addr_buf_t &&url) noexcept;
  Multiplex() noexcept = default;
  Multiplex(Multiplex&&) noexcept = default;

  Multiplex&operator=(Multiplex&&) noexcept = default;

  /**
   * demultiplex the signal
   * @param ip_addr the address to send to and receive from
   * @return
   *    -- new file::multiplex if not already exist in _hash_to_fd, second parameter == true
   *    -- old file::multiplex if already exist in _hash_to_fd, second parameter == false
   */
  std::pair<std::shared_ptr<file::stream::mux::pipe_t>, bool> demux(const file::ip_addr_t &ip_addr);

  /**
   * @return the socket used for multiplexing
   */
  file::multiplex &mux();


  file::ip_addr_t local_addr();
private:
  std::shared_ptr<__multiplex> _mux;
  file::ip_addr_buf_t _url;
};

struct demux_client_t {
  using member_t = Multiplex;

  file::demultiplex socket;
};

using multiplex = Server<demux_client_t, std::uint16_t>;
}

#endif //T_MAN_MULTIPLEX_H
