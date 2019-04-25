//
// Created by loki on 11-4-19.
//

#ifndef T_MAN_MULTIPLEX_H
#define T_MAN_MULTIPLEX_H

#include <kitty/util/unordered_map_key.h>
#include <kitty/util/utility.h>
#include <kitty/p2p/uuid.h>
#include <kitty/server/multiplex_stream.h>

KITTY_GEN_HASH_KEY(server, ip_addr_key, file::ip_addr_buf_t, file::ip_addr_t)

namespace server {

class Multiplex {
public:
  using hash_to_fd = std::unordered_map<ip_addr_key, std::shared_ptr<file::stream::mux::pipe_t>>;

  Multiplex(std::shared_ptr<__multiplex_shared> &&mux, file::ip_addr_buf_t &&url) noexcept;
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
  std::shared_ptr<__multiplex_shared> _mux;
  file::ip_addr_buf_t _url;
};

struct __multiplex_shared {
  __multiplex_shared() = default;
  __multiplex_shared(__multiplex_shared&&other) noexcept : sock { std::move(other.sock) }, hash_to_fd { std::move(other.hash_to_fd) } {}

  explicit __multiplex_shared(file::io &&sock) : sock { std::move(sock) } {}
  file::multiplex sock;
  Multiplex::hash_to_fd hash_to_fd;

  std::mutex lock;
};


}

#endif //T_MAN_MULTIPLEX_H
