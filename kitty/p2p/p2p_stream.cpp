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
std::condition_variable poll_traits<stream::p2p>::_cv;
std::mutex poll_traits<stream::p2p>::_poll_mutex;

poll_traits<stream::p2p>::fd_t poll_traits<stream::p2p>::fd(const stream_t &stream) {
  return stream.fd();
}

poll_traits<stream::p2p>::fd_t poll_traits<stream::p2p>::fd(const poll_t &p) {
  return p;
}

poll_traits<stream::p2p>::poll_t poll_traits<stream::p2p>::read(const stream_t &stream) {
  auto fd = stream.fd();

  fd->polled = true;

  return fd;
}

void poll_traits<stream::p2p>::poll(std::vector<poll_t> &polls, std::chrono::milliseconds to,
                                    const std::function<void(fd_t, poll_result_t)> &f) {
  std::unique_lock ul(_poll_mutex);

  if(polls.empty()) {
    std::this_thread::sleep_for(to);

    return;
  }

  if(std::all_of(std::begin(polls), std::end(polls), [](poll_t poll) { return poll->empty(); })) {
    _cv.wait_for(ul, to);
  }

  ul.unlock();
  for(poll_t poll : polls) {
    while(!poll->empty()) {
      f(poll, poll_result_t::IN);
    }
  }
}

void poll_traits<stream::p2p>::remove(const fd_t &fd) {
  fd->polled = false;
}
}

namespace file::stream {
p2p::p2p(std::shared_ptr<pipe_t> bridge) : _pipe { std::move(bridge) } {}

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
  _pipe->get_call().send(util::toContainer(out, size));

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

pipe_t *p2p::fd() const {
  return _pipe.get();
}

std::optional<std::vector<uint8_t>> pipe_t::pop() {
  std::unique_lock ul(_queue_mutex);

  while(true) {
    if(!_open) {
      err::code = err::code_t::FILE_CLOSED;

      return std::nullopt;
    }

    if(!_queue.empty()) {
      auto vec { std::move(_queue.front()) };
      _queue.erase(std::begin(_queue));

      return std::move(vec);
    }

    // wait until data is available
    _cv.wait(ul, [this]() { return !_queue.empty() || !_open; });
  }
}

int pipe_t::select(std::chrono::milliseconds to) {
  std::unique_lock<std::mutex> ul(_queue_mutex);
  while(true) {
    if(!_open) {
      err::code = err::FILE_CLOSED;
      return -1;
    }

    if(!_queue.empty()) {
      return err::OK;
    }

    // wait for timeout or until data is available
    if(_cv.wait_for(ul, to, [this]() { return !_queue.empty() || !_open; })) {
      if(!_open) {
        err::code = err::FILE_CLOSED;
        return -1;
      }

      return err::OK;
    }

    err::code = err::TIMEOUT;
    return -1;
  }
}

void pipe_t::push_front(std::vector<std::uint8_t> &&buf) {
  std::lock_guard lg(_queue_mutex);
  _queue.emplace(std::begin(_queue), std::move(buf));
}

void pipe_t::push(std::string_view data) {
  std::vector<uint8_t> buf;
  buf.insert(std::begin(buf), std::begin(data), std::end(data));

  {
    std::lock_guard lg(_queue_mutex);
    _queue.emplace_back(std::move(buf));
  }
  _cv.notify_all();

  if(polled) {
    std::lock_guard lg(poll_traits<stream::p2p>::_poll_mutex);
    poll_traits<stream::p2p>::_cv.notify_all();
  }
}

void pipe_t::seal() {
  _open = false;
}

void pipe_t::set_call(::p2p::pj::ICECall call) {
  _call = call;

  _open = true;
}

::p2p::pj::ICECall &pipe_t::get_call() {
  return _call;
}

bool pipe_t::is_open() const {
  return _open;
}

bool pipe_t::empty() {
  return _queue.empty();
}
}
