//
// Created by loki on 12-2-19.
//

#include <future>
#include <queue>
#include <kitty/log/log.h>
#include <kitty/p2p/p2p_stream.h>
#include <kitty/p2p/p2p.h>
#include "p2p_stream.h"


namespace file::stream {

class pipe_t {
public:
  /**
   * blocking
   * @return the next data buffer from the queue
   */
  std::optional<std::vector<uint8_t>> pop() {
    std::lock_guard<std::mutex> lg(_queue_mutex);

    while(true) {
      if(_sealed) {
        err::code = err::code_t::FILE_CLOSED;

        return std::nullopt;
      }

      if(!_queue.empty()) {
        auto vec { std::move(_queue.front()) };
        _queue.pop();

        return vec;
      }

      // wait until data is available
      std::unique_lock<std::mutex> ul(_queue_mutex);
      _cv.wait(ul, [this]() { return !_queue.empty() || _sealed; });
    }
  }

  int select(std::chrono::milliseconds to) {
    std::lock_guard<std::mutex> lg(_queue_mutex);

    while(true) {
      if(_sealed) {
        return err::FILE_CLOSED;
      }

      if(!_queue.empty()) {
        return err::OK;
      }

      // wait for timeout or until data is available
      std::unique_lock<std::mutex> ul(_queue_mutex);
      if(_cv.wait_for(ul, to, [this]() { return !_queue.empty() || _sealed; })) {
        if(_sealed) {
          return err::FILE_CLOSED;
        }

        return err::OK;
      }

      return err::TIMEOUT;
    }
  }

  void push(std::string_view data) {
    std::vector<uint8_t> buf;
    buf.insert(std::begin(buf), std::begin(data), std::end(data));

    {
      std::lock_guard<std::mutex> lg(_queue_mutex);
      _queue.push(std::move(buf));
    }

    _cv.notify_all();
  }

  void seal() {
    std::lock_guard<std::mutex> lg(_queue_mutex);

    _sealed = true;
  }

  void set_call(::p2p::pj::ICECall call) {
    _sealed = false;

    _call = call;
  }

  ::p2p::pj::ICECall &get_call() {
    return _call;
  }

  bool is_open() const {
    return !_sealed;
  }
private:
  bool _sealed = true;

  ::p2p::pj::ICECall _call;

  std::queue<std::vector<uint8_t>> _queue;

  std::condition_variable _cv;
  std::mutex _queue_mutex;
};

p2p::p2p(std::shared_ptr<pipe_t> bridge) : _pipe { std::move(bridge) } {}

int p2p::read(std::vector<std::uint8_t> &buf) {
  auto data { _pipe->pop() };
  if(!data) {
    return -1;
  }

  buf.resize(0);
  buf.insert(std::end(buf), std::begin(*data), std::end(*data));

  return 0;
}

int p2p::write(const std::vector<std::uint8_t> &buf) {
  _pipe->get_call().send(util::toContainer(buf.data(), buf.size()));

  return (int)buf.size();
}

bool p2p::is_open() const {
  return (bool)_pipe && _pipe->is_open();
}

bool p2p::eof() const {
  return false;
}

void p2p::seal() {
  _pipe->seal();
}

int p2p::select(std::chrono::milliseconds to, const int mode) {
  if(mode == READ) {
    return _pipe->select(to);
  }

  return err::OK;
}
}

namespace p2p {
std::future<file::p2p> connect(uuid_t uuid) {
  return std::async(std::launch::async, [uuid]() {
    auto t = pj::register_thread();

    std::mutex m;

    bool p2p_fail = false;
    std::condition_variable cv;

    auto bridge = std::make_shared<file::stream::pipe_t>();

    auto failure = ::p2p::peer_create(uuid, [bridge](pj::ICECall call, std::string_view data) {
      bridge->push(data);
    }, [uuid, &cv, &p2p_fail](pj::ICECall call, pj::status_t status) {
      if(status != pj::success) {
        p2p_fail = true;
        cv.notify_all();

        err::set("failed initialization");

        ::p2p::peer_remove(uuid);
        return;
      }

      // on_create
      ::p2p::send_invite(uuid);
    }, [uuid, &bridge, &cv, &p2p_fail](pj::ICECall call, pj::status_t status) {
      // on_connect

      if(status != pj::success) {
        p2p_fail = true;
        cv.notify_all();

        err::set("failed negotiation");

        ::p2p::peer_remove(uuid);
        return;
      }

      bridge->set_call(call);
      cv.notify_all();
    });

    if(failure) {
      return file::p2p {};
    }

    std::unique_lock<std::mutex> ul(m);
    cv.wait(ul, [&]() { return bridge->is_open() || p2p_fail; });

    if(p2p_fail) {
      return file::p2p {};
    }

    return file::p2p { std::chrono::seconds(3), bridge };
  });
}
}
