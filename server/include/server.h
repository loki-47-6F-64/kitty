#ifndef DOSSIER_SERVER_H
#define DOSSIER_SERVER_H

#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <vector>
#include <mutex>
#include <tuple>

#include "thread_pool.h"
#include "move_by_copy.h"
#include "file.h"

#include "log.h"
#include "err.h"
namespace server {


template<typename>
struct DefaultType {
  typedef char Type[0];
};

template<class T>
class Server {
public:
  typedef std::lock_guard<std::mutex> _RAII_lock;

  typedef T Client;
  typedef typename DefaultType<Client>::Type Member;

private:
  // Should the server continue?
  bool _continue;

  pollfd _listenfd;

  std::function < void(Client &&) > _action;


  util::ThreadPool<void> _task;
  std::mutex _server_stop_lock;

  Member _member;
public:

  Server() : _continue(false), _task(5) {
    static_assert(sizeof(Member) == 0, "Default constructor cannot be used when DefaultType is overriden");
  }

  Server(Member && member) : _continue(false), _task(5), _member(std::move(member)) { }

  ~Server() {
    stop();
  }

  // Start server

  void operator()() {
    if (isRunning())
      return;

    _continue = true;
    _listen();
  }

  // Returns -1 on failure

  int setListener(typename Client::_sockaddr &server, std::function < void(Client &&) > f) {
    pollfd pfd {
      _socket(),
      POLLIN,
      0
    };

    if (pfd.fd == -1)
      return -1;

    // Allow reuse of local addresses
    if (setsockopt(pfd.fd, SOL_SOCKET, SO_REUSEADDR, &pfd.fd, sizeof(pfd.fd))) {
      err::code = err::LIB_SYS;
      return -1;
    }

    if (bind(pfd.fd, (sockaddr *) & server, sizeof(server)) < 0) {
      err::code = err::LIB_SYS;
      return -1;
    }

    listen(pfd.fd, 1);

    _listenfd = pfd;
    _action = f;

    return pfd.fd;
  }

  void stop() {
    _continue = false;
    _RAII_lock lg(_server_stop_lock);
  }

  inline bool isRunning() {
    return _continue;
  }

private:

  void _listen() {
    int result;

    _RAII_lock lg(_server_stop_lock);
    while (_continue) {
      if ((result = poll(&_listenfd, 1, 100)) > 0) {
        if (_listenfd.revents == POLLIN) {
          DEBUG_LOG("Accepting client");

          Client client;
          if (_accept(client)) {
            continue;
          }

          util::MoveByCopy<Client> me(std::move(client));
          _task.push(
            typename decltype(_task)::__task(std::bind(_action, me))
          );
        }

      }
      else if (result == -1) {
	err::code = err::LIB_SYS;
        print(error, "Cannot poll socket: ", err::current());

        std::terminate();
      }
    }

    // Cleanup
    close(_listenfd.fd);
  }

  /*
   * User defined methods
   */

  // Accept client
  int _accept(Client &client);

  // Generate socket
  int _socket();
};
}
#endif
