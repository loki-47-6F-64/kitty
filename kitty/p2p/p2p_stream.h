//
// Created by loki on 12-2-19.
//

#ifndef KITTY_P2P_STREAM_H
#define KITTY_P2P_STREAM_H

#include <vector>
#include <cstdint>
#include <queue>
#include <future>
#include <bitset>
#include <kitty/file/file.h>
#include <kitty/file/poll.h>
#include <kitty/p2p/pj/ice_trans.h>
#include <kitty/p2p/uuid.h>

namespace file::stream {

class pipe_t {
public:
  int select(std::chrono::milliseconds to);

  /**
   * blocking
   * @return the next data buffer from the queue
   */
  std::optional<std::vector<uint8_t>> pop();

  bool empty();

  void push_front(std::vector<std::uint8_t> &&buf);
  void push(std::string_view data);

  void set_call(::p2p::pj::ICECall call);
  ::p2p::pj::ICECall &get_call();

  void seal();
  bool is_open() const;

  bool polled = false;
private:
  bool _open   = false;

  ::p2p::pj::ICECall _call;

  std::vector<std::vector<uint8_t>> _queue;

  std::condition_variable _cv;
  std::mutex _queue_mutex;

  static std::condition_variable _cv_poll;
  static std::mutex _poll_mutex;
};

class p2p {
public:
  p2p() = default;
  explicit p2p(std::shared_ptr<pipe_t> bridge);

  p2p(p2p &&) noexcept = default;
  p2p& operator =(p2p&& stream) noexcept = default;

  std::int64_t read(std::uint8_t *in, std::size_t size);
  int write(std::uint8_t *out, std::size_t size);

  bool is_open() const;
  bool eof() const;

  int select(std::chrono::milliseconds to, int mode);

  void seal();

  pipe_t *fd() const;
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

namespace file {
template<>
class poll_traits<stream::p2p> {
public:
  using stream_t = stream::p2p;
  using fd_t     = stream::pipe_t *;
  using poll_t   = stream::pipe_t *;

  static fd_t fd(const stream_t &stream);
  static fd_t fd(const poll_t &p);

  static poll_t read(const stream_t &stream);

  static void remove(const fd_t &);

  static void poll(
    std::vector<poll_t> &polls,
    std::chrono::milliseconds to,
    const std::function<void(fd_t, poll_result_t)> &f);

  static std::condition_variable _cv;
  static std::mutex _poll_mutex;
};
}

#endif //KITTY_P2P_STREAM_H
