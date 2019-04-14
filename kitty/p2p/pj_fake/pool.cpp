//
// Created by loki on 21-3-19.
//

#include <kitty/server/proxy.h>
#include <kitty/log/log.h>
#include "pool.h"

namespace p2p::pj {

void on_data(file::io &fd, const connection_t&c) {
  ICECall call { c.ice_trans, c.ip_addr };

  __prepend_e type;
  server::proxy::load(fd, type);

  switch(type) {
    case __prepend_e::DATA: {
      print(info, "received data");

      std::vector<char> data;
      server::proxy::load(fd, data);

      c.ice_trans->on_data(call, std::string_view { data.data(), data.size() });
      break;
    }
    case __prepend_e::CONNECTING:
      print(info, c.ip_addr.ip, ':', c.ip_addr.port, " received connection attempt");
      server::proxy::push(fd, __prepend_e::CONFIRM);
  }
}

void nothing(file::io &fd, const connection_t&) {}

Pool::Pool(caching_pool_t &caching_pool, const char *name) :
_io_queue {
  file::poll_t<file::io, connection_t> {
    &on_data, &nothing, &nothing
  }
} {}

TimerHeap &Pool::timer_heap() {
  return _timer_heap;
}

IOQueue &Pool::io_queue() {
  return _io_queue;
}

DNSResolv &Pool::dns() {
  return _dns_resolv;
}

void Pool::set_stun(ip_addr_t ip_addr) {}

ICETrans Pool::ice_trans(Pool::on_data_f &&on_data_recv, Pool::on_ice_create_f &&on_ice_init,
                         Pool::on_connect_f &&on_call_connect) {
  // TODO: create ice_trans
  return pj::ICETrans {
    std::make_unique<ICETrans::func_t::element_type>(std::make_tuple(std::move(on_data_recv), std::move(on_ice_init), std::move(on_call_connect))),
    &io_queue()
  };
}

int Pool::iterate(std::chrono::milliseconds max_to) {
  auto &task_pool = io_queue().task_pool;

  auto now = std::chrono::steady_clock::now();
  auto to = std::chrono::floor<std::chrono::milliseconds>(util::either(task_pool.next(), now + max_to) - now);

  to = std::min(to, max_to);

  if(io_queue().poll.poll(to)) {
    return -1;
  }

  if(auto f = io_queue().task_pool.pop()) {
    (*f)->run();
  }

  return 0;
}

caching_pool_t Pool::init_caching_pool() {
  return { 1 };
}

void DNSResolv::set_ns(const std::vector<std::string_view> &) {}
}