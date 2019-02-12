//
// Created by loki on 24-1-19.
//

#ifndef T_MAN_POOL_H
#define T_MAN_POOL_H

#include <kitty/p2p/pj/nath.h>
#include <kitty/p2p/pj/timer_heap.h>
#include <kitty/p2p/pj/io_queue.h>
#include <kitty/p2p/pj/ice_trans.h>
#include <kitty/p2p/pj/dns_resolv.h>

namespace p2p::pj {

// TODO :: Configure [TURN]
class Pool {
public:
  Pool() = default;
  Pool(const char *name);

  TimerHeap & timer_heap();
  IOQueue   & io_queue();
  DNSResolv & dns_resolv();

  void set_stun(ip_addr_t ip_addr);

  ICETrans ice_trans(std::function<void(ICECall, std::string_view)> &&on_data_recv,
                     std::function<void(ICECall, status_t)> &&on_ice_init,
                     std::function<void(ICECall, status_t)> &&on_call_connect);


private:
  ice_trans_cfg_t _ice_cfg;

  pool_t _pool;

  TimerHeap _timer_heap;
  IOQueue   _io_queue;
  DNSResolv _dns_resolv;
};

}

#endif //T_MAN_POOL_H
