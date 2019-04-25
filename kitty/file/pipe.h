//
// Created by loki on 12-4-19.
//

#ifndef T_MAN_PIPE_H
#define T_MAN_PIPE_H

#include <condition_variable>
#include <kitty/util/utility.h>
#include <kitty/file/file.h>
#include <kitty/file/tcp.h>

namespace file::stream {

template<class T>
class pipe_t {
public:
  using member_t = T;

  template<class... Args>
  pipe_t(Args &&... args) : _member { std::forward<Args>(args)... }, _polled { nullptr } {}
  pipe_t(pipe_t &&) noexcept = default;
  pipe_t&operator=(pipe_t &&) noexcept = default;

  int select(std::chrono::milliseconds to) {
    std::unique_lock<std::mutex> ul(_queue_mutex);
    while(true) {
      if(!is_open()) {
        err::code = err::FILE_CLOSED;
        return -1;
      }

      if(!_queue.empty()) {
        return err::OK;
      }

      // wait for timeout or until data is available
      if(_cv.wait_for(ul, to, [this]() { return !_queue.empty() || !is_open(); })) {
        if(!is_open()) {
          err::code = err::FILE_CLOSED;
          return -1;
        }

        return err::OK;
      }

      err::code = err::TIMEOUT;
      return -1;
    }
  }

  /**
   * blocking
   * @return the next data buffer from the queue
   */
  std::optional<std::vector<uint8_t>> pop() {
    std::unique_lock ul(_queue_mutex);

    while(true) {
      if(!is_open()) {
        err::code = err::code_t::FILE_CLOSED;

        return std::nullopt;
      }

      if(!_queue.empty()) {
        auto vec { std::move(_queue.front()) };
        _queue.erase(std::begin(_queue));

        return std::move(vec);
      }

      // wait until data is available
      _cv.wait(ul, [this]() { return !_queue.empty() || !is_open(); });
    }
  }

  bool empty() const {
    return _queue.empty();
  }

  void push_front(std::vector<std::uint8_t> &&buf) {
    std::lock_guard lg(_queue_mutex);
    _queue.emplace(std::begin(_queue), std::move(buf));
  }

  void push(std::string_view data) {
    std::vector<uint8_t> buf;
    buf.insert(std::begin(buf), std::begin(data), std::end(data));

    {
      std::lock_guard lg(_queue_mutex);
      _queue.emplace_back(std::move(buf));

      if(!_polled) {
        _cv.notify_one();

        return;
      }
    }

    // if(_polled)
    _polled->ring(true);
  }

  member_t &get_member() {
    return _member;
  }

  const member_t &get_member() const {
    return _member;
  };

  template<class Callable>
  void mod_member(Callable &&func) {
    std::lock_guard lg(_queue_mutex);

    func(get_member());
  }

  void set_alarm(util::Alarm<bool> *alarm) {
    std::lock_guard lg(_queue_mutex);

    _polled = alarm;
  }

  /**
   * You need to provide the following method definitions:
   *    1. int send(const std::string_view &data)
   *    2. void seal()
   *    3. bool is_open() const
   */

  /**
   * send data
   * @param data
   * @return
   *    -- bytes send on success
   *    -- < 0 on faiure
   */
  int send(const std::string_view &data);

  /**
   * close the pipe
   */
  void seal();

  /**
   * @return true if the pipe is open
   */
  bool is_open() const;

private:
  member_t _member;

  util::Alarm<bool> *_polled;

  std::vector<std::vector<uint8_t>> _queue;

  std::condition_variable _cv;
  std::mutex _queue_mutex;
};
}

#endif //T_MAN_PIPE_H
