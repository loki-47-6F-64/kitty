//
// Created by loki on 21-3-19.
//

#include "pool.h"

namespace p2p::pj {

void on_data(file::io &fd, const connection_t&c) {
  ICECall call { c.ice_trans, c.ip_addr };

  auto &cache = fd.get_read_cache();
  auto bytes_read = fd.getStream().read(cache.cache.get(), cache.capacity);

  if(bytes_read > 0) {
    c.ice_trans->on_data(call, std::string_view { (const char*)cache.cache.get(), (size_t)bytes_read });
  }

  // TODO: on error
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

void Pool::iterate(std::chrono::milliseconds max_to) {
  while(auto f = io_queue().task_pool.pop()) {
    (*f)->run();
  }

  io_queue().poll.poll();
}

caching_pool_t Pool::init_caching_pool() {
  return { 1 };
}

void DNSResolv::set_ns(const std::vector<std::string_view> &) {}
}