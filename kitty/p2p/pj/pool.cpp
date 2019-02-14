//
// Created by loki on 24-1-19.
//

#include <mutex>

#include <atomic>
#include <kitty/p2p/pj/pool.h>
#include <kitty/log/log.h>


namespace p2p::pj {
pj_caching_pool raw;

Pool::Pool(caching_pool_t &caching_pool, const char *name) {
  pj_ice_strans_cfg_default(&_ice_cfg);

  _ice_cfg.turn_tp->conn_type = PJ_TURN_TP_TCP;
  _ice_cfg.af = pj_AF_INET();
  _ice_cfg.stun_cfg.pf = &caching_pool->factory;

  _pool.reset(pj_pool_create(&caching_pool->factory, name, 512, 512, nullptr));

  _timer_heap = TimerHeap(_pool, _ice_cfg);
  _io_queue   = IOQueue(_pool, _ice_cfg);
  _dns_resolv = DNSResolv(_pool, _timer_heap, _io_queue);
}

TimerHeap &Pool::timer_heap() {
  return _timer_heap;
}

IOQueue &Pool::io_queue() {
  return _io_queue;
}

ICETrans Pool::ice_trans(std::function<void(ICECall, std::string_view)> &&on_data_recv,
                         std::function<void(ICECall, status_t)> &&on_ice_init,
                         std::function<void(ICECall, status_t)> &&on_call_connect) {
  return pj::ICETrans {
    _ice_cfg,
    std::make_unique<ICETrans::func_t::element_type>(std::make_tuple(std::move(on_data_recv), std::move(on_ice_init), std::move(on_call_connect)))
  };
}

DNSResolv &Pool::dns_resolv() {
  return _dns_resolv;
}

void Pool::set_stun(ip_addr_t ip_addr) {
  if(ip_addr.port) {
    _ice_cfg.stun.port = (std::uint16_t) ip_addr.port;
  }

  _ice_cfg.stun.server = string(ip_addr.ip);
}

caching_pool_t Pool::init_caching_pool() {
  static std::atomic<bool> called { false };

  if(called.exchange(true)) {
    print(error, "Pool::init_caching_pool() called more than once");

    std::abort();
  }

  pj_caching_pool_init(&raw, nullptr, 0);

  return caching_pool_t { &raw };
}

void Pool::iterate(std::chrono::milliseconds max_to) {
  std::chrono::milliseconds milli { 0 };

  timer_heap().poll(milli);

  milli = std::min(milli, max_to);

  auto c = io_queue().poll(milli);
  if(c < 0) {
    print(error, __FILE__, ": ", err(get_netos_err()));

    std::abort();
  }
}
}