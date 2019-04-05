//
// Created by loki on 25-1-19.
//

#include <cassert>
#include <kitty/p2p/pj/timer_heap.h>

namespace p2p::pj {

TimerHeap::TimerHeap(pool_t &pool, ice_trans_cfg_t &ice_trans) {
  pj_timer_heap_create(pool.get(), 100, &ice_trans.stun_cfg.timer_heap);

  _timer_heap.reset(ice_trans.stun_cfg.timer_heap);
}

timer_heap_t::pointer pj::TimerHeap::raw() const {
  return _timer_heap.get();
}

unsigned TimerHeap::poll(time_t &duration) {
  auto c = pj_timer_heap_poll(_timer_heap.get(), &duration);

  assert(duration.msec >= 0 && duration.sec >= 0);

  return c;
}

}
