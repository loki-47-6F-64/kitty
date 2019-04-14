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
#include <kitty/file/pipe.h>
#include <kitty/p2p/config.h>
#include <kitty/p2p/uuid.h>

namespace file::stream {

namespace pj {

struct __member_t {
  KITTY_DEFAULT_CONSTR(__member_t)

  bool open = false;
  ::p2p::pj::ICECall call;
};
using pipe_t = pipe_t<__member_t>;
}

class p2p {
public:
  p2p() = default;
  explicit p2p(std::shared_ptr<pj::pipe_t> bridge);

  p2p(p2p &&) noexcept = default;
  p2p& operator =(p2p&& stream) noexcept = default;

  std::int64_t read(std::uint8_t *in, std::size_t size);
  int write(std::uint8_t *out, std::size_t size);

  bool is_open() const;
  bool eof() const;

  int select(std::chrono::milliseconds to, int mode);

  void seal();

  pj::pipe_t *fd() const;
private:
  std::shared_ptr<pj::pipe_t> _pipe;
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
  using fd_t     = stream::pj::pipe_t *;
  using poll_t   = stream::pj::pipe_t *;

  fd_t fd(const stream_t &stream);
  fd_t fd(const poll_t &p);

  poll_t read(const stream_t &stream);

  void remove(const fd_t &);

  int poll(
    std::vector<poll_t> &polls,
    std::chrono::milliseconds to,
    const std::function<void(fd_t, poll_result_t)> &f);

  std::unique_ptr<util::Alarm<bool>> poll_alarm;
};
}

#endif //KITTY_P2P_STREAM_H
