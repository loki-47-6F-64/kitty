//
// Created by loki on 12-2-19.
//

#include <future>
#include <queue>
#include <kitty/log/log.h>
#include <kitty/p2p/p2p_stream.h>
#include <kitty/p2p/p2p.h>


namespace file::stream {
p2p::p2p(std::shared_ptr<pipe_t> bridge) : _pipe { std::move(bridge) } {}

int p2p::read(std::uint8_t *in, std::size_t size) {
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
