//
// Created by loki on 11-4-19.
//

#include "multiplex.h"

namespace server {
using namespace std::literals;

struct __multiplex {
  __multiplex() = default;
  __multiplex(__multiplex&&other) noexcept : sock { std::move(other.sock) }, hash_to_fd { std::move(other.hash_to_fd) } {}

  explicit __multiplex(file::io &&sock) : sock { std::move(sock) } {}
  file::multiplex sock;
  Multiplex::hash_to_fd hash_to_fd;

  std::mutex lock;
};

void __seal_multiplex(__multiplex &multiplex, file::ip_addr_t ip_addr) {
  std::lock_guard lg(multiplex.lock);

  auto it = multiplex.hash_to_fd.find(ip_addr_key { ip_addr });
  if(it != std::end(multiplex.hash_to_fd)) {
    multiplex.hash_to_fd.erase(it);
  }
}

file::multiplex &__get_sock(__multiplex &multiplex) {
  return multiplex.sock;
}

Multiplex::Multiplex(std::shared_ptr<__multiplex> &&mux, file::ip_addr_buf_t &&url) noexcept : _mux { std::move(mux) }, _url { std::move(url) } {}

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

template<>
void multiplex::_cleanup() {}

template<>
int multiplex::_init_listen(std::uint16_t port) {
  auto sock = file::udp_init(port);

  if(!sock.is_open()) {
    return -1;
  }

  if(!port) {
    port = file::sockport(sock);
  }

  auto ips = file::get_broadcast_ips(port);
  if(ips.empty()) {
    if(err::code == err::OK) {
      err::set("Couldn't get candidate ip's");
    }

    return -1;
  }

  _member = Multiplex(std::make_shared<__multiplex>(std::move(sock)), std::move(ips.front()));

  return 0;
}

template<>
int multiplex::_poll() {
  if(auto result = _member.mux().wait_for(file::READ)) {
    if(result == err::TIMEOUT) {
      return 0;
    }

    return -1;
  }

  return 1;
}

template<>
std::variant<err::code_t, multiplex::client_t> multiplex::_accept() {
  auto &sock = _member.mux();

  //TODO: reduce timeout to 0
  auto data = sock.next_batch();
  auto ip_addr = std::move(sock.getStream().last_read());
  if(!data) {
    print(error, "Couldn't read multiplexed socket ", ip_addr.ip, ':', ip_addr.port, ": ", err::current());
    return err::code;
  }

  TUPLE_2D(pipe, new_item, _member.demux(ip_addr));
  pipe->push(*data);

  if(!new_item) {
    return err::OK;
  }

  return client_t { file::demultiplex { 3s, pipe } };
}

}

