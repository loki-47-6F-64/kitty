//
// Created by loki on 12-2-19.
//

#include <future>
#include <queue>
#include <kitty/log/log.h>
#include <kitty/p2p/p2p_stream.h>
#include <kitty/p2p/p2p.h>
#include "p2p_stream.h"

namespace file {
poll_traits<stream::p2p>::fd_t poll_traits<stream::p2p>::fd(const stream_t &stream) {
  return stream.fd();
}

poll_traits<stream::p2p>::fd_t poll_traits<stream::p2p>::fd(const poll_t &p) {
  return p;
}

poll_traits<stream::p2p>::poll_t poll_traits<stream::p2p>::read(const stream_t &stream) {
  auto fd = stream.fd();

  fd->set_alarm(poll_alarm.get());

  return fd;
}

int poll_traits<stream::p2p>::poll(std::vector<poll_t> &polls, std::chrono::milliseconds to,
                                    const std::function<void(fd_t, poll_result_t)> &f) {
  bool res = poll_alarm->wait_for(to, [&]() {
    return std::any_of(std::begin(polls), std::end(polls), [](poll_t poll) {
      return !poll->empty();
    });
  });

  if(!res) {
    return 0;
  }

  for(poll_t poll : polls) {
    if(!poll->empty()) {
      f(poll, poll_result_t::IN);
    }
  }

  return 0;
}

void poll_traits<stream::p2p>::remove(const fd_t &fd) {
  fd->set_alarm(nullptr);
}
}

namespace file::stream {
template<>
void pj::pipe_t::seal() {
  get_member().open = false;
}

template<>
bool pj::pipe_t::is_open() const {
  return get_member().open;
}

p2p::p2p(std::shared_ptr<pj::pipe_t> bridge) : _pipe { std::move(bridge) } {}

std::int64_t p2p::read(std::uint8_t *in, std::size_t size) {
  auto data { _pipe->pop() };
  if(!data) {
    return -1;
  }

  auto size_data = data->size();

  auto bytes = std::min(size_data, size);
  std::copy_n(data->data(), bytes, in);

  if(size_data > size) {
    data->erase(std::begin(*data), std::begin(*data) + bytes);
    _pipe->push_front(std::move(*data));
  }

  KITTY_DEBUG_LOG("bytes [", bytes, ']');
  return bytes;
}

int p2p::write(std::uint8_t *out, std::size_t size) {
  _pipe->get_member().call.send(util::toContainer(out, size));

  return size;
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

pj::pipe_t *p2p::fd() const {
  return _pipe.get();
}
}
