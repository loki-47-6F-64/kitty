//
// Created by loki on 11-4-19.
//

#include "multiplex.h"

namespace server {
using namespace std::literals;

void __seal_multiplex(__multiplex_shared &multiplex, file::ip_addr_t ip_addr) {
  std::lock_guard lg(multiplex.lock);

  auto it = multiplex.hash_to_fd.find(ip_addr_key { ip_addr });
  if(it != std::end(multiplex.hash_to_fd)) {
    multiplex.hash_to_fd.erase(it);
  }
}

file::multiplex &__get_sock(__multiplex_shared &multiplex) {
  return multiplex.sock;
}

Multiplex::Multiplex(std::shared_ptr<__multiplex_shared> &&mux, file::ip_addr_buf_t &&url) noexcept : _mux { std::move(mux) }, _url { std::move(url) } {}

std::pair<std::shared_ptr<file::stream::mux::pipe_t>, bool> Multiplex::demux(const file::ip_addr_t &ip_addr) {
  auto &hash_to_fd = _mux->hash_to_fd;

  std::lock_guard lg(_mux->lock);
  auto it = hash_to_fd.find(ip_addr_key { ip_addr });

  bool new_item { false };
  if(it == std::end(hash_to_fd)) {
    new_item = true;

    it = hash_to_fd.emplace(
      file::ip_addr_buf_t::from_ip_addr(ip_addr),
      std::make_shared<file::stream::mux::pipe_t>(_mux, ip_addr)

    ).first;
  }

  return { it->second, new_item };
}

file::ip_addr_t Multiplex::local_addr() {
  return _url;
}

file::multiplex &Multiplex::mux() {
  return _mux->sock;
}

}

