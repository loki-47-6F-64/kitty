//
// Created by loki on 24-1-19.
//

#include <string>
#include <array>
#include <optional>

#include <kitty/log/log.h>
#include <kitty/p2p/pj/nath.h>
#include <kitty/p2p/pj/pool.h>

namespace p2p::pj {
// log callback to write to file
static void log_func(int level, const char *data, int len) {
  std::string_view str { data, (std::size_t)len };

  if(str.back() == '\n') {
    str.remove_suffix(1);
  }

  switch (level) {
    case 0:
      print(error, "FATAL --> aborting... ", str);
      std::abort();
    case 1:
      print(info, str);
      break;
    case 2:
      print(info, str);
      break;
    case 3:
      print(info, str);
      break;
    case 4:
      print(info, str);
      break;
    case 5:
      print(debug, str);
      break;
  }
}

thread_t register_thread() {
  auto desc = std::make_unique<std::array<long, THREAD_DESC_SIZE>>();

  pj_thread_t *thread_raw;
  pj_thread_register(nullptr, desc->data(), &thread_raw);
  return thread_t {
    std::move(desc),
    thread_ptr { thread_raw }
  };
}

status_t init(const char *log_file) {
  pj_log_set_level(2);
  pj_log_set_log_func(&log_func);

  if(log_file) {
    file::log_open(log_file);
  }

  if(auto status = pj_init()) {
    return status;
  }

  if(auto status = pjlib_util_init()) {
    return status;
  }

  if(auto status = pjnath_init()) {
    return status;
  }

  return success;
}

str_t string(const std::string_view &str) {
  return str_t { const_cast<char*>(str.data()), (ssize_t)str.size() };
}

std::string_view string(const str_t &str) {
  return std::string_view { str.ptr, (std::size_t)str.slen };
}

void dns_resolv_destroy(pj_dns_resolver *ptr) {
  pj_dns_resolver_destroy(ptr, False);
}

std::chrono::milliseconds time(const time_t& duration) {
  return std::chrono::milliseconds(duration.msec) + std::chrono::seconds(duration.sec);
}

std::string err(status_t err_code) {
  std::string err_str;
  err_str.resize(PJ_ERR_MSG_SIZE);

  pj_strerror(err_code, err_str.data(), PJ_ERR_MSG_SIZE);

  err_str.resize(PJ_ERR_MSG_SIZE -1);
  return err_str;
}

func_err_t get_netos_err = pj_get_netos_error;
func_err_t get_os_err = pj_get_os_error;

}
