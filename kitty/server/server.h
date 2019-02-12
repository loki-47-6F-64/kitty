#ifndef DOSSIER_SERVER_H
#define DOSSIER_SERVER_H

#include <poll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <vector>
#include <mutex>
#include <tuple>

#include <kitty/util/thread_pool.h>
#include <kitty/util/optional.h>
#include <kitty/util/move_by_copy.h>
#include <kitty/util/auto_run.h>

#include <kitty/file/file.h>

#include <kitty/log/log.h>
#include <kitty/err/err.h>

namespace server {
util::ThreadPool &tasks();

template<class T>
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

  /*
   * 1 -- listen
   * 2 -- poll
   * 3 -- accept
   * 4 -- cleanup
   */

  // Returns -1 on failure
  int start(std::function<void(client_t &&)> f) {
    if(_init_listen()) {
      return -1;
    }

    return _listen(f);
  }

  void stop() { _autoRun.stop(); }
  void join() { _autoRun.join(); }

  inline bool isRunning() const { return _autoRun.isRunning(); }

private:

  int _listen(std::function<void(client_t &&)> _action) {
    int result = 0;

    
    _autoRun.run([&]() {
      result = _poll();

      if(result > 0) {
        DEBUG_LOG("Accepting client");

        auto client = _accept();
        if(client) {
          auto c = util::cmove(*client);
          tasks().push([_action, c]() mutable {
            _action(c);
          });
        }
      }
      else if(result < 0) {
        print(error, "Cannot poll socket: ", err::current());

        _autoRun.stop();
      }
    });
    
    if(result == -1) {
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
  int _init_listen();

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
   * @return a client on success
   */
  std::optional<client_t> _accept();

  /**
   * Perform instructions to close the server
   */
  void _cleanup();
};

struct tcp_client_t {
  struct member_t {
    member_t(const sockaddr_in6 &sockaddr) : sockaddr { sockaddr } {}

    sockaddr_in6 sockaddr;

    pollfd listenfd {};
  };

  std::unique_ptr<file::io> socket;
  std::string ip_addr;
};

typedef Server<tcp_client_t> tcp;
}
#endif
