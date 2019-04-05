//
// Created by loki on 24-1-19.
//

#ifndef T_MAN_NATH_H
#define T_MAN_NATH_H

#ifdef ANDROID
#include <pj/config_site.h>
#endif

#include <chrono>

#include <pjlib.h>
#include <pjlib-util.h>
#include <pjnath.h>

#include <kitty/util/utility.h>

namespace p2p::pj {

auto constexpr success = PJ_SUCCESS;
auto constexpr True    = PJ_TRUE;
auto constexpr False   = PJ_FALSE;

auto constexpr ICE_MAX_COMP     = PJ_ICE_MAX_COMP;
auto constexpr THREAD_DESC_SIZE = PJ_THREAD_DESC_SIZE;

using status_t        = pj_status_t;
using bool_t          = pj_bool_t;
using ssize_t         = pj_ssize_t;
using str_t           = pj_str_t;
using time_t          = pj_time_val;
using ice_sess_cand_t = pj_ice_sess_cand;
using ice_cand_type_t = pj_ice_cand_type;
using caching_pool_raw    = pj_caching_pool;

using func_err_t = status_t (*)();
extern func_err_t get_netos_err;
extern func_err_t get_os_err;

void dns_resolv_destroy(pj_dns_resolver *);

using thread_ptr        = util::safe_ptr_v2<pj_thread_t, status_t, pj_thread_destroy>;
using dns_resolv_t      = util::safe_ptr<pj_dns_resolver, dns_resolv_destroy>;
using pool_t            = util::safe_ptr<pj_pool_t, pj_pool_release>;
using ice_trans_t       = util::safe_ptr_v2<pj_ice_strans, status_t, pj_ice_strans_destroy>;
using timer_heap_t      = util::safe_ptr<pj_timer_heap_t, pj_timer_heap_destroy>;
using io_queue_t        = util::safe_ptr_v2<pj_ioqueue_t, status_t, pj_ioqueue_destroy>;

struct caching_pool_t {
  std::unique_ptr<pj_caching_pool> ptr;

  caching_pool_t() = default;
  caching_pool_t(caching_pool_t&&) noexcept = default;

  caching_pool_t& operator=(caching_pool_t&&) noexcept = default;
  pj_caching_pool* operator->() {
    return ptr.get();
  }

  const pj_caching_pool* operator->() const {
    return ptr.get();
  }

  ~caching_pool_t() {
    if(ptr) {
      pj_caching_pool_destroy(ptr.get());
    }
  }
};

struct thread_t {
  std::unique_ptr<std::array<long, THREAD_DESC_SIZE>> descriptor;
  thread_ptr thread;
};

status_t init(const char *log_file);

/**
 * register your thread before using pj
 * @return thread handle
 */
thread_t register_thread();

str_t string(const std::string_view &str);
std::string_view string(const str_t &str);

template<class T1, class T2>
time_t time(const std::chrono::duration<T1,T2> &duration) {
  auto sec  = std::chrono::floor<std::chrono::seconds>(duration);

  std::chrono::milliseconds msec;
  if(sec > std::chrono::seconds { 0 }) {
    msec = std::chrono::ceil<std::chrono::milliseconds>(duration % sec);
  }
  else {
    msec = std::chrono::ceil<std::chrono::milliseconds>(duration);
  }

  return { (long)sec.count(), (long)msec.count() };
}

std::string err(status_t err_code);
}

constexpr bool operator < (const p2p::pj::time_t &l, const p2p::pj::time_t &r) {
  return l.sec < r.sec || (l.sec == r.sec && l.msec < r.msec);
}

#endif //T_MAN_NATH_H
