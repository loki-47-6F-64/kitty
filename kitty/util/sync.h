//
// Created by loki on 16-4-19.
//

#ifndef T_MAN_SYNC_H
#define T_MAN_SYNC_H

#include <utility>
#include <mutex>
#include <array>

namespace util {

template<class T, std::size_t N = 1>
class Sync {
public:
  static_assert(N > 0, "Sync should have more than zero mutexes");
  using value_type = T;

  template<std::size_t I = 0>
  std::lock_guard<std::mutex> lock() {
    return std::lock_guard { std::get<I>(_lock) };
  }

  template<class ...Args>
  Sync(Args&&... args) : raw { std::forward<Args>(args)... } {}

  Sync &operator=(Sync &&other) noexcept {
    for(auto &l : _lock) {
      l.lock();
    }

    for(auto &l : other._lock) {
      l.lock();
    }

    raw = std::move(other.raw);

    for(auto &l : _lock) {
      l.unlock();
    }

    for(auto &l : other._lock) {
      l.unlock();
    }

    return *this;
  }

  Sync &operator=(Sync &other) noexcept {
    for(auto &l : _lock) {
      l.lock();
    }

    for(auto &l : other._lock) {
      l.lock();
    }

    raw = other.raw;

    for(auto &l : _lock) {
      l.unlock();
    }

    for(auto &l : other._lock) {
      l.unlock();
    }

    return *this;
  }

  Sync &operator=(const value_type &val) noexcept {
    for(auto &l : _lock) {
      l.lock();
    }

    raw = val;

    for(auto &l : _lock) {
      l.unlock();
    }

    return *this;
  }

  Sync &operator=(value_type &&val) noexcept {
    for(auto &l : _lock) {
      l.lock();
    }

    raw = std::move(val);

    for(auto &l : _lock) {
      l.unlock();
    }

    return *this;
  }

  value_type *operator->() {
    return &raw;
  }

  value_type raw;
private:
  std::array<std::mutex, N> _lock;
};

}


#endif //T_MAN_SYNC_H
