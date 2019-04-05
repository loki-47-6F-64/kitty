//
// Created by loki on 21-3-19.
//

#include "nath.h"

namespace p2p::pj {

status_t init(const char *log_file) {
  return 0;
}

thread_t register_thread() {
  return 0;
}

std::string_view string(const std::string_view &str) {
  return str;
}

std::string err(status_t err_code) {
  err::code = (err::code_t)err_code;

  return err::current();
}
}