//
// Created by loki on 11-4-19.
//

#ifndef T_MAN_MULTIPLEX_STREAM_H
#define T_MAN_MULTIPLEX_STREAM_H

#include <vector>
#include <cstdint>
#include <queue>
#include <future>
#include <bitset>
#include <kitty/file/file.h>
#include <kitty/file/poll.h>
#include <kitty/file/pipe.h>
#include <kitty/file/io_stream.h>

namespace server {
struct __multiplex_shared;
}

namespace file {
namespace stream {

class multiplex : public io {
public:
  using io::io;

  explicit multiplex(io &&) noexcept;

  std::int64_t read(std::uint8_t *data, std::size_t size, sockaddr *peer);
  int write(std::uint8_t *data, std::size_t size, sockaddr *dest);
};

}

using multiplex = FD<stream::multiplex, std::tuple<sockaddr*>, std::tuple<sockaddr*>>;

std::uint16_t sockport(const multiplex &sock);

namespace stream {
namespace mux {

struct __member_t {
  KITTY_DEFAULT_CONSTR(__member_t)

  __member_t(std::shared_ptr<server::__multiplex_shared> mux, const ip_addr_t &ip_addr) : _multiplex { std::move(mux) }, _sockaddr { *ip_addr.to_sockaddr() } {}
  std::shared_ptr<server::__multiplex_shared> _multiplex;
  sockaddr_storage _sockaddr;
};

using pipe_t = pipe_t<__member_t>;
}

class demultiplex {
public:
  demultiplex() = default;
  explicit demultiplex(std::shared_ptr<mux::pipe_t> pipe);

  demultiplex(demultiplex &&) noexcept = default;
  demultiplex& operator =(demultiplex&& stream) noexcept = default;

  std::int64_t read(std::uint8_t *in, std::size_t size);
  int write(std::uint8_t *out, std::size_t size);

  bool is_open() const;
  bool eof() const;

  int select(std::chrono::milliseconds to, int mode);

  void seal();

  mux::pipe_t *fd() const;
  std::shared_ptr<mux::pipe_t> pipe() const;
private:
  std::shared_ptr<mux::pipe_t> _pipe;
};

}

using demultiplex = FD<stream::demultiplex>;

ip_addr_buf_t peername(const demultiplex &sock);

template<>
class poll_traits<stream::demultiplex> {
public:
  using stream_t = stream::demultiplex;
  using fd_t     = stream::mux::pipe_t *;
  using poll_t   = stream::mux::pipe_t *;

  fd_t fd(const stream_t &stream);
  fd_t fd(const poll_t &p);

  poll_t read(const stream_t &stream);

  void remove(const fd_t &);

  int poll(
    std::vector<poll_t> &polls,
    std::chrono::milliseconds to,
    const std::function<void(fd_t, poll_result_t)> &f);

  std::unique_ptr<util::Alarm<bool>> poll_alarm = std::make_unique<util::Alarm<bool>>();
};

}
#endif //T_MAN_MULTIPLEX_STREAM_H
