//
// Created by loki on 10-3-19.
//

#ifndef T_MAN_POLL_H
#define T_MAN_POLL_H

// TODO: This should be in a seperate header
#if __APPLE__
#define KITTY_POLLIN POLLIN
#define KITTY_POLLOUT POLLOUT
#define POLLRDHUP (POLLHUP | POLLNVAL)
#elif __linux__
#define KITTY_POLLIN  (POLLIN  | POLLRDHUP)
#define KITTY_POLLOUT (POLLOUT | POLLRDHUP)
#endif

#include <sys/select.h>
#include <poll.h>
#include <kitty/file/file.h>
namespace file {

enum class poll_result_t {
  IN,
  OUT,
  ERROR
};

template<class T>
class poll_traits {
public:
  using stream_t = T;
  using fd_t     = int;
  using poll_t   = pollfd;

  static fd_t fd(const stream_t & t) {
    return t.fd();
  }

  static fd_t fd(const poll_t &p) {
    return p.fd;
  }

  static poll_t read(const stream_t &stream) {
    return { stream.fd(), KITTY_POLLIN, 0 };
  }

  static poll_t write(const stream_t &stream) {
    return { stream.fd(), KITTY_POLLOUT, 0 };
  }

  static poll_t read_write(const stream_t &stream) {
    return { stream.fd(), KITTY_POLLIN | KITTY_POLLOUT, 0 };
  }

  static void poll(
    std::vector<poll_t> &polls,
    std::chrono::milliseconds to,
    const std::function<void(fd_t, poll_result_t)> &f) {

    auto res = ::poll(polls.data(), polls.size(), to.count());
    if(res > 0) {
      for(auto &poll : polls) {
        if(poll.revents & (POLLHUP | POLLRDHUP | POLLERR)) {
          f(poll.fd, poll_result_t::ERROR);
          continue;
        }

        if(poll.revents & POLLIN) {
          f(poll.fd, poll_result_t::IN);
        }

        if(poll.revents & POLLOUT) {
          f(poll.fd, poll_result_t::OUT);
        }
      }
    }
  }
};

template<class T, class X>
class poll_t {
  static_assert(util::instantiation_of_v<FD, T>, "template parameter T must be an instantiation of file::FD");
public:
  using file_t = T;
  using user_t = X;

private:
  using __stream_t = typename file_t::stream_t;

  using __fd_t   = typename poll_traits<__stream_t>::fd_t;
  using __poll_t = typename poll_traits<__stream_t>::poll_t;

public:

  template<class FR, class FW, class FH>
  poll_t(FR &&fr, FW &&fw, FH &&fh) : _read_cb { std::forward<FR>(fr) }, _write_cb { std::forward<FW>(fw) }, _remove_cb { std::forward<FH>(fh) } {}

  poll_t(poll_t&&) = default;
  poll_t& operator=(poll_t&&) = default;

  void read(file_t &fd, user_t user_val) {
    std::lock_guard<std::mutex> lg(_mutex_add);

    _queue_add.emplace_back(user_val, &fd, poll_traits<__stream_t>::read(fd.getStream()));
  }

  void write(file_t &fd, user_t user_val) {
    std::lock_guard<std::mutex> lg(_mutex_add);

    _queue_add.emplace_back(user_val, &fd, poll_traits<__stream_t>::write(fd.getStream()));
  }

  void read_write(file_t &fd, user_t user_val) {
    std::lock_guard<std::mutex> lg(_mutex_add);

    _queue_add.emplace_back(user_val, &fd, poll_traits<__stream_t>::read_write(fd.getStream()));
  }

  void remove(file_t &fd) {
    std::lock_guard<std::mutex> lg(_mutex_add);

    _queue_remove.emplace_back(&fd);
  }

  void poll(std::chrono::milliseconds milli = std::chrono::milliseconds(50)) {
    _apply_changes();

    poll_traits<__stream_t>::poll(_pollfd, milli, [this](__fd_t fd, poll_result_t result) {
      auto it = _fd_to_file.find(fd);
      switch(result) {
        case poll_result_t::IN:
          _read_cb(*std::get<1>(it->second), std::get<0>(it->second));
          break;
        case poll_result_t::OUT:
          _write_cb(*std::get<1>(it->second), std::get<0>(it->second));
          break;
        case poll_result_t::ERROR:
          remove(*std::get<1>(it->second));
      }
    });
  }
private:
  void _apply_changes() {
    std::lock_guard<std::mutex> lg(_mutex_add);

    for(auto &el : _queue_add) {
      _pollfd.emplace_back(std::get<2>(el));

      _fd_to_file.emplace(poll_traits<__stream_t>::fd(std::get<1>(el)->getStream()), std::pair<user_t, file_t*> { std::get<0>(el), std::get<1>(el) });
    }

    for(auto &el : _queue_remove) {
      auto it = _fd_to_file.find(poll_traits<__stream_t>::fd(el->getStream()));
      auto cp = *it;

      _fd_to_file.erase(it);
      _pollfd.erase(std::remove_if(std::begin(_pollfd), std::end(_pollfd), [&el](const auto &l) {
        return poll_traits<__stream_t>::fd(l) == poll_traits<__stream_t>::fd(el->getStream());
      }), std::end(_pollfd));

      _remove_cb(*std::get<1>(cp.second), std::get<0>(cp.second));
    }

    _queue_add.clear();
    _queue_remove.clear();
  }

  std::function<void(file_t &file, user_t)> _read_cb;
  std::function<void(file_t &file, user_t)> _write_cb;
  std::function<void(file_t &file, user_t)> _remove_cb;

  std::map<__fd_t, std::pair<user_t, file_t*>> _fd_to_file;

  std::vector<std::tuple<user_t, file_t*, __poll_t>> _queue_add;
  std::vector<file_t*> _queue_remove;

  std::vector<__poll_t> _pollfd;

  std::mutex _mutex_add;
};
}

#endif //T_MAN_POLL_H
