//
// Created by loki on 11-4-19.
//

#include "multiplex_stream.h"

#include <future>
#include <queue>
#include <kitty/log/log.h>
#include <kitty/server/multiplex_stream.h>

namespace server {
void __seal_multiplex(__multiplex &multiplex, file::ip_addr_t);
file::multiplex &__get_sock(__multiplex &multiplex);
}

namespace file::stream {

template<>
void mux::pipe_t::seal() {
  std::lock_guard lg(_queue_mutex);

  server::__seal_multiplex(*get_member()._multiplex, file::ip_addr_buf_t::from_sockaddr((sockaddr*)&get_member()._sockaddr));
  get_member()._multiplex.reset();
}

template<>
bool mux::pipe_t::is_open() const {
  return (bool)get_member()._multiplex;
}

template<>
int mux::pipe_t::send(const std::string_view &data) {
  return server::__get_sock(*get_member()._multiplex).getStream().write(
    (std::uint8_t*)data.data(), data.size(), (sockaddr*)&get_member()._sockaddr);
}

}

namespace file {

poll_traits<stream::demultiplex>::fd_t poll_traits<stream::demultiplex>::fd(const stream_t &stream) {
  return stream.fd();
}

poll_traits<stream::demultiplex>::fd_t poll_traits<stream::demultiplex>::fd(const poll_t &p) {
  return p;
}

poll_traits<stream::demultiplex>::poll_t poll_traits<stream::demultiplex>::read(const stream_t &stream) {
  auto fd = stream.fd();

  fd->set_alarm(poll_alarm.get());

  return fd;
}

int poll_traits<stream::demultiplex>::poll(std::vector<poll_t> &polls, std::chrono::milliseconds to,
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

void poll_traits<stream::demultiplex>::remove(const fd_t &fd) {
  fd->set_alarm(nullptr);
}
}

namespace file::stream {
std::int64_t multiplex::read(std::uint8_t *data, std::size_t size) {
  sockaddr_in6 addr;
  socklen_t addr_size = sizeof(addr);

  if(ssize_t bytes_read; (bytes_read = ::recvfrom(fd(), data, size, 0, (sockaddr*)&addr, &addr_size)) > 0) {
    _last_read_ip = file::ip_addr_buf_t::from_sockaddr((sockaddr*)&addr);

    return bytes_read;
  } else if(!bytes_read) {
    seal();

    return 0;
  }

  err::code = err::LIB_SYS;
  return -1;
}

int multiplex::write(std::uint8_t *data, std::size_t size, sockaddr *dest) {
  socklen_t addr_size = dest->sa_family == file::INET ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);

  ssize_t bytes_written = ::sendto(fd(), data, size, 0, dest, addr_size);
  if(bytes_written < 0) {
    err::code = err::LIB_SYS;

    return -1;
  }

  return (int) bytes_written;
}

ip_addr_buf_t &multiplex::last_read() {
  return _last_read_ip;
}

multiplex::multiplex(io &&other) noexcept: io(std::move(other)) {}

demultiplex::demultiplex(std::shared_ptr<mux::pipe_t> pipe) : _pipe { std::move(pipe) } {}

std::int64_t demultiplex::read(std::uint8_t *in, std::size_t size) {
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

int demultiplex::write(std::uint8_t *out, std::size_t size) {
  return _pipe->send({(char*)out, size });
}

bool demultiplex::is_open() const {
  return (bool)_pipe && _pipe->is_open();
}

bool demultiplex::eof() const {
  return false;
}

void demultiplex::seal() {
  _pipe->seal();
}

int demultiplex::select(std::chrono::milliseconds to, const int mode) {
  if(mode == READ) {
    return _pipe->select(to);
  }

  return err::OK;
}

mux::pipe_t *demultiplex::fd() const {
  return _pipe.get();
}

std::shared_ptr<mux::pipe_t> demultiplex::pipe() const {
  return _pipe;
}

}