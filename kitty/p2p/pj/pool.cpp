//
// Created by loki on 24-1-19.
//

#include <mutex>

#include <atomic>
#include <kitty/p2p/pj/pool.h>
#include <kitty/log/log.h>
#include <pjlib-util.h>
#include "pool.h"


namespace p2p::pj {
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

DNSResolv &Pool::dns() {
  return _dns_resolv;
}

void Pool::set_stun(ip_addr_t ip_addr) {
  if(ip_addr.port) {
    _ice_cfg.stun.port = (std::uint16_t) ip_addr.port;
  }

  _ice_cfg.stun.server = string(ip_addr.ip);
}

caching_pool_t Pool::init_caching_pool() {
  auto cache = std::make_unique<pj_caching_pool>();

  pj_caching_pool_init(cache.get(), nullptr, 0);

  return caching_pool_t { std::move(cache) };
}

void Pool::iterate(std::chrono::milliseconds max_to) {
  std::chrono::milliseconds milli { 0 };

  timer_heap().poll(milli);

  milli = std::min(milli, max_to);

  auto c = io_queue().poll(milli);
  if(c < 0) {
    print(error, "Cannot poll io_queue: ", err(get_netos_err()));

    std::abort();
  }
}

void detect_nat_cb(void *data, const stun_nat_type *result) {
  auto &alarm = *((util::Alarm<stun_nat_type>*)data);

  alarm.ring(*result);
}

status_t Pool::detect_nat(util::Alarm<stun_nat_type> &alarm, const sockaddr &addr) {
  return pj_stun_detect_nat_type2(&addr, &_ice_cfg.stun_cfg, &alarm, &detect_nat_cb);
}

std::variant<status_t, stun_nat_type> Pool::detect_nat() {
  util::Alarm<std::variant<::p2p::pj::status_t, ::p2p::pj::sockaddr>> alarm_dns;

  if(auto err = dns().resolv(alarm_dns, string(_ice_cfg.stun.server))) {
    err::set(::p2p::pj::err(err));

    return err;
  }

  // Synchronize with dns request
  alarm_dns.wait();
  auto &addr = std::get<::p2p::pj::sockaddr>(*alarm_dns.status());
  addr.ipv4.sin_port = util::endian::big(_ice_cfg.stun.port);

  util::Alarm<::p2p::pj::stun_nat_type> alarm;
  if(auto err = detect_nat(alarm, addr); err != ::p2p::pj::success) {
    err::set(::p2p::pj::err(err));

    return err;
  }

  // Synchronize with nat detect request
  alarm.wait();
  return *alarm.status();
}
}