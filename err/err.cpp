#include <errno.h>
#include <string.h>

#include <openssl/err.h>

#include <kitty/err/err.h>

namespace err {

constexpr int MAX_ERROR_BUFFER = 120;
thread_local char err_buf[MAX_ERROR_BUFFER] = {'\0'};
thread_local code_t code;

const char *sys() {
  return strerror_r(errno, err_buf, MAX_ERROR_BUFFER);
}

const char *ssl() {
#ifdef VIKING_BUILD_SSL
  int err = ERR_get_error();
  if(err) {
    ERR_error_string_n(err, err_buf, MAX_ERROR_BUFFER);
    return err_buf;
  }

  return "";
#else
  return "SSL support is not compiled with the library";
#endif
}

void set(const char *src) {
  int x;
  for(x = 0; src[x] && x < MAX_ERROR_BUFFER - 1; ++x) {
    err_buf[x] = src[x];
  }
  err_buf[x] = '\0';
}

const char *current() {
  switch(code) {
    case OK:
      return "Success";
    case TIMEOUT:
      return "Timeout occurred";
    case FILE_CLOSED:
      return "Connection prematurely closed";
    case INPUT_OUTPUT:
      return "Input/Output error";
    case OUT_OF_BOUNDS:
      return "Out of bounds";
    case BREAK:
      return "Break: You shouldn't be seeing this error";
    case LIB_GAI:
      // Special exception: error_code is returned to the caller,
      // the caller must set the error message
      return current();
    case LIB_SSL:
      return ssl();
    case LIB_SYS:
      return sys();
  };
  
  return "Unknown Error";
}
}