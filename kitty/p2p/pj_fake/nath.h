//
// Created by loki on 21-3-19.
//

#ifndef T_MAN_NATH_H
#define T_MAN_NATH_H

#include <sys/types.h>
#include <string_view>
#include <chrono>
#include <memory>
#include <kitty/file/io_stream.h>
#include <kitty/file/tcp.h>
#include <kitty/util/task_pool.h>
#include <kitty/file/poll.h>

namespace p2p::pj {

auto constexpr success = 0;
auto constexpr True  = 1;
auto constexpr False = 0;

auto constexpr ICE_MAX_COMP     = (2 << 1);
auto constexpr THREAD_DESC_SIZE = 64;

using status_t   = int;
using bool_t     = bool;
using ssize_t    = ::ssize_t;
using str_t      = std::string_view;
using time_val_t = std::chrono::milliseconds;
using thread_t   = int;

struct caching_pool_t { int ptr { 0 }; };

using ice_cand_type_t = int;

using sockaddr_t    = sockaddr;
using sockaddr      = sockaddr_storage;
using ip_addr_t     = file::ip_addr_t;
using ip_addr_buf_t = file::ip_addr_buf_t;

class TimerHeap { };

struct __ice_trans_t;
struct connection_t {
  __ice_trans_t *ice_trans;
  ip_addr_buf_t ip_addr;
};

class IOQueue {
public:
  file::poll_t<file::io, connection_t> poll;
  util::TaskPool task_pool;
};

class DNSResolv {
public:
  void set_ns(const std::vector<std::string_view> &);
};

status_t init(const char *log_file);
thread_t register_thread();

std::string_view string(const std::string_view &str);

template<class T1, class T2>
time_val_t time(const std::chrono::duration<T1,T2> &duration) {
  return std::chrono::ceil<std::chrono::milliseconds>(duration);
}

std::string err(status_t err_code);
}

#endif //T_MAN_NATH_H
