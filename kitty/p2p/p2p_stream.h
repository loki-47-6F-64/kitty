//
// Created by loki on 12-2-19.
//

#ifndef KITTY_P2P_STREAM_H
#define KITTY_P2P_STREAM_H

#include <vector>
#include <cstdint>
#include <queue>
#include <future>
#include <kitty/file/file.h>
#include <kitty/p2p/pj/ice_trans.h>
#include <kitty/p2p/uuid.h>

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
        _queue.erase(std::begin(_queue));

        return vec;
      }

      // wait until data is available
      std::unique_lock<std::mutex> ul(_queue_mutex);
      _cv.wait(ul, [this]() { return !_queue.empty() || _sealed; });
    }
  }

  int select(std::chrono::milliseconds to) {
    std::unique_lock<std::mutex> ul(_queue_mutex);
    while(true) {
      if(_sealed) {
        return err::FILE_CLOSED;
      }

      if(!_queue.empty()) {
        return err::OK;
      }

      // wait for timeout or until data is available
      if(_cv.wait_for(ul, to, [this]() { return !_queue.empty() || _sealed; })) {
        if(_sealed) {
          return err::FILE_CLOSED;
        }

        return err::OK;
      }

      return err::TIMEOUT;
    }
  }

  void push_front(std::vector<std::uint8_t> &&buf) {
    {
      std::lock_guard<std::mutex> lg(_queue_mutex);
      _queue.emplace(std::begin(_queue), std::move(buf));
    }
  }

  void push(std::string_view data) {
    std::vector<uint8_t> buf;
    buf.insert(std::begin(buf), std::begin(data), std::end(data));

    {
      std::lock_guard<std::mutex> lg(_queue_mutex);
      _queue.emplace_back(std::move(buf));
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

  std::vector<std::vector<uint8_t>> _queue;

  std::condition_variable _cv;
  std::mutex _queue_mutex;
};

class p2p {
public:
  p2p() = default;
  explicit p2p(std::shared_ptr<pipe_t> bridge);

  p2p(p2p &&) noexcept = default;
  p2p& operator =(p2p&& stream) noexcept = default;

  int read(std::uint8_t *in, std::size_t size);
  int write(const std::vector<std::uint8_t> &buf);

  bool is_open() const;
  bool eof() const;

  int select(std::chrono::milliseconds to, int mode);

  void seal();

private:
  std::shared_ptr<pipe_t> _pipe;
};
}
namespace file {
using p2p = FD<stream::p2p>;
}

namespace p2p {
std::future<file::p2p> connect(uuid_t uuid);
}

#endif //KITTY_P2P_STREAM_H
