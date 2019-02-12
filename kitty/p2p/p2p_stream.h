//
// Created by loki on 12-2-19.
//

#ifndef KITTY_P2P_STREAM_H
#define KITTY_P2P_STREAM_H

#include <vector>
#include <cstdint>
#include <future>
#include <kitty/file/file.h>
#include <kitty/p2p/uuid.h>

namespace file::stream {
class pipe_t;

class p2p {
public:
  p2p() = default;
  explicit p2p(std::shared_ptr<pipe_t> bridge);

  p2p(p2p &&) noexcept = default;
  p2p& operator =(p2p&& stream) noexcept = default;

  int read(std::vector<std::uint8_t>& buf);
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
