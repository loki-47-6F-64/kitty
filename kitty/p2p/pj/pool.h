//
// Created by loki on 24-1-19.
//

#ifndef T_MAN_POOL_H
#define T_MAN_POOL_H

#include <variant>
#include <kitty/p2p/pj/nath.h>
#include <kitty/p2p/pj/timer_heap.h>
#include <kitty/p2p/pj/io_queue.h>
#include <kitty/p2p/pj/ice_trans.h>
#include <kitty/p2p/pj/dns_resolv.h>

namespace p2p::pj {

using stun_nat_type = pj_stun_nat_detect_result;

// TODO :: Configure [TURN]
class Pool {
public:
  using on_data_f       = ICETrans::on_data_f;
  using on_ice_create_f = ICETrans::on_ice_create_f;
  using on_connect_f    = ICETrans::on_connect_f;

  Pool() = default;
  Pool(caching_pool_t &caching_pool, const char *name);

  TimerHeap & timer_heap();
  IOQueue   & io_queue();
  DNSResolv & dns();

  void set_stun(ip_addr_t ip_addr);

  /**
   * Detect the NAT type you are behind
   * @return
   *    status_t on failure starting either the resolv hostname or detect_nat
   *    stun_nat_type on successfully completing the NAT detection
   */
  std::variant<status_t, stun_nat_type> detect_nat();

  /**
   * @param alarm
   * @param addr
   * @return success on successfully starting the asynchronous request
   */
  status_t detect_nat(util::Alarm<stun_nat_type> &alarm, const sockaddr &addr);

  ICETrans ice_trans(on_data_f &&on_data_recv,
                     on_ice_create_f &&on_ice_init,
                     on_connect_f &&on_call_connect);

  /**
   * Run the internal asynchronous pjnath functionality
   * @param max_to
   */
  void iterate(time_t max_to);

  /**
   * Create a caching_pool_t
   */
  static caching_pool_t init_caching_pool();
private:
  ice_trans_cfg_t _ice_cfg;

  pool_t _pool;

  TimerHeap _timer_heap;
  IOQueue   _io_queue;
  DNSResolv _dns_resolv;
};

}

#endif //T_MAN_POOL_H
