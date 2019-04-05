//
// Created by loki on 21-3-19.
//

#ifndef T_MAN_POOL_H
#define T_MAN_POOL_H

#include <kitty/p2p/pj_fake/nath.h>
#include <kitty/p2p/pj_fake/ice_trans.h>

namespace p2p::pj {

// TODO :: Configure [TURN]
class Pool {
public:
  using on_data_f       = ICETrans::on_data_f;
  using on_ice_create_f = ICETrans::on_ice_create_f;
  using on_connect_f    = ICETrans::on_connect_f;

  Pool() = default;

  Pool(caching_pool_t &caching_pool, const char *name);

  TimerHeap &timer_heap();

  IOQueue &io_queue();

  DNSResolv &dns();

  void set_stun(ip_addr_t ip_addr);

  ICETrans ice_trans(on_data_f &&on_data_recv,
                     on_ice_create_f &&on_ice_init,
                     on_connect_f &&on_call_connect);

  /**
   * Run the internal asynchronous pjnath functionality
   * @param max_to
   */
  void iterate(std::chrono::milliseconds max_to);

  /**
   * Create a caching_pool_t
   */
  static caching_pool_t init_caching_pool();

private:
  TimerHeap _timer_heap;
  IOQueue _io_queue;
  DNSResolv _dns_resolv;
};
}

#endif //T_MAN_POOL_H
