#ifndef DOSSIER_SERVER_H
#define DOSSIER_SERVER_H

#include <poll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <vector>
#include <mutex>
#include <tuple>
#include <variant>

#include <kitty/util/thread_pool.h>
#include <kitty/util/move_by_copy.h>
#include <kitty/util/auto_run.h>

#include <kitty/file/file.h>

#include <kitty/log/log.h>
#include <kitty/err/err.h>

namespace server {
util::ThreadPool &tasks();

template<class T, class... InitArgs>
class Server {
public:
  using client_t  = T;
  using member_t = typename client_t::member_t;

private:
  util::AutoRun<void> _autoRun;

  member_t _member;
public:
  template<class... Args>
  Server(Args&& ...args) : _member { std::forward<Args>(args)... } { }

  ~Server() { stop(); }

  member_t &get_member() {
    return _member;
  }

  /*
   * 1 -- listen
   * 2 -- poll
   * 3 -- accept
   * 4 -- cleanup
   */

  // Returns -1 on failure
  int start(std::function<void(client_t &&)> f, InitArgs ...args) {
    if(_init_listen(std::forward<InitArgs>(args)...)) {
      print(error, "Couldn't set listener: ", err::current());

      return -1;
    }

    return _listen(f);
  }

  void stop() { _autoRun.stop(); }
  void join() { _autoRun.join(); }

  inline bool isRunning() const { return _autoRun.isRunning(); }

private:

  int _listen(std::function<void(client_t &&)> _action) {
    bool failure = false;

    _autoRun.run([&]() {
      auto client = _accept();
      if(std::holds_alternative<client_t>(client)) {
        auto c = util::cmove(std::get<client_t>(client));
        tasks().push([_action, c]() mutable {
          _action(c);
        });
      }
      else if(std::get<err::code_t>(client) != err::TIMEOUT) {
        failure = true;

        _autoRun.stop();
      }
    });

    _cleanup();
    if(failure) {
      print(error, "Stopped server -- ", err::current());

      return -1;
    }
    
    return 0;
  }

  /*
   * User defined methods
   */

  /**
   * Perform necessary instructions to setup listening
   * @return result < 0 on failure
   */
  int _init_listen(InitArgs...);

  /**
   * Poll for incoming client
   * @return
   *    result <  0 on failure
   *    result == 0 on timeout
   *    result >  0 on success
   */
  int _poll();
  /**
   * Perform necessary instructions to accept/decline
   * @return
   *    err::code_t -- on error on any err::code_t != err::TIMEOUT
   *    client_t    -- on acceptance of a client
   */
  std::variant<err::code_t, client_t> _accept();

  /**
   * Perform instructions to close the server
   */
  void _cleanup();
};

struct tcp_client_t {
  struct member_t {
    sa_family_t family;

    pollfd listenfd;
  };

  std::unique_ptr<file::io> socket;
  std::string ip_addr;
};

typedef Server<tcp_client_t, const sockaddr *const> tcp;
}
#endif
