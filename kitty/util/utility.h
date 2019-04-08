#ifndef UTILITY_H
#define UTILITY_H

#include <vector>
#include <memory>
#include <type_traits>
#include <algorithm>
#include <optional>
#include <mutex>
#include <condition_variable>

#include <kitty/util/optional.h>
#include <kitty/err/err.h>
#include <kitty/util/literal.h>

namespace util {

template<class T>
class Alarm {
public:
  // either bool, pointer or optional, in that order
  using status_t = either_t<std::is_same_v<T, bool>, bool, either_t<std::is_pointer_v<T>, T, std::optional<T>>>;

  template<class... Args>
  Alarm() : _ul(_lock), _status { literal::false_v<status_t> } {}

  void ring(const status_t &status) {
    std::lock_guard lg(_lock);

    _status = status;
    _cv.notify_one();
  }

  void ring(status_t &&status) {
    std::lock_guard lg(_lock);

    _status = std::move(status);
    _cv.notify_one();
  }

  template<class Rep, class Period>
  auto wait_for(const std::chrono::duration<Rep, Period>& rel_time) {
    auto ret = _cv.wait_for(_ul, rel_time, [this]() { return (bool)status(); });

    _ul.unlock();

    return ret;
  }

  template<class Rep, class Period, class Pred>
  auto wait_for(const std::chrono::duration<Rep, Period>& rel_time, Pred &&pred) {
    auto ret = _cv.wait_for(_ul, rel_time, [this, &pred]() { return (bool)status() || pred(); });

    _ul.unlock();

    return ret;
  }

  template<class Rep, class Period>
  auto wait_until(const std::chrono::duration<Rep, Period>& rel_time) {
    auto ret = _cv.wait_until(_ul, rel_time, [this]() { return (bool)status(); });

    _ul.unlock();

    return ret;
  }

  template<class Rep, class Period, class Pred>
  auto wait_until(const std::chrono::duration<Rep, Period>& rel_time, Pred &&pred) {
    auto ret = _cv.wait_until(_ul, rel_time, [this, &pred]() { return (bool)status() || pred(); });

    _ul.unlock();

    return ret;
  }

  auto wait() {
    _cv.wait(_ul, [this]() { return (bool)status(); });
    _ul.unlock();
  }

  template<class Pred>
  auto wait(Pred &&pred) {
    _cv.wait(_ul, [this, &pred]() { return (bool)status() || pred(); });
    _ul.unlock();
  }

  const status_t &status() const {
    return _status;
  }

  status_t &status() {
    return _status;
  }

  void reset() {
    if(!_ul.owns_lock()) {
      _ul.lock();

      _status = status_t {};
    }
  }
private:
  std::mutex _lock;
  std::unique_lock<std::mutex> _ul;

  std::condition_variable _cv;

  status_t _status;
};

template<class T>
class FailGuard {
public:
  FailGuard() = delete;
  FailGuard(T && f) noexcept : _func { std::forward<T>(f) } {}
  FailGuard(FailGuard &&other) noexcept : _func { std::move(other._func) } {
    this->failure = other.failure;

    other.failure = false;
  }

  FailGuard(const FailGuard &) = delete;

  FailGuard &operator=(const FailGuard &) = delete;
  FailGuard &operator=(FailGuard &&other) = delete;

  ~FailGuard() noexcept {
    if(failure) {
      _func();
    }
  }

  void disable() { failure = false; }
  bool failure { true };
private:
  T _func;
};

template<class T>
auto fail_guard(T && f) {
  return FailGuard<T> { std::forward<T>(f) };
}

template<class T>
void append_struct(std::vector<uint8_t> &buf, const T &_struct) {
  constexpr size_t data_len = sizeof(_struct);

  buf.reserve(data_len);

  auto *data = (uint8_t *) & _struct;

  for (size_t x = 0; x < data_len; ++x) {
    buf.push_back(data[x]);
  }
}

template<class T>
class Hex {
public:
  typedef T elem_type;
private:
  const char _bits[16] {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
  };

  char _hex[sizeof(elem_type) * 2];
public:
  Hex(const elem_type &elem) {
    const uint8_t *data = reinterpret_cast<const uint8_t *>(&elem) + sizeof(elem_type);
    for (auto it = begin(); it < cend();) {
      *it++ = _bits[*--data / 16];
      *it++ = _bits[*data % 16];
    }
  }

  char *begin() { return _hex; }
  char *end() { return _hex + sizeof(elem_type) * 2; }

  const char *begin() const { return _hex; }
  const char *end() const { return _hex + sizeof(elem_type) * 2; }

  const char *cbegin() const { return _hex; }
  const char *cend() const { return _hex + sizeof(elem_type) * 2; }

  std::string to_string() const {
    return { begin(), end() };
  }

  std::string_view to_string_view() const {
    return { begin(), sizeof(elem_type) * 2 };
  }
};

template<class T>
Hex<T> hex(const T &elem) {
  return Hex<T>(elem);
}

template<class T>
std::optional<T> from_hex(const std::string_view &hex) {
  if(hex.size() < sizeof(T)*2) {
    err::code = err::OUT_OF_BOUNDS;

    return std::nullopt;
  }

  std::uint8_t buf[sizeof(T)];

  const char *data = hex.data() + hex.size();

  auto convert = [] (char ch) -> std::uint8_t {
    if(ch >= '0' && ch <= '9') {
      return (std::uint8_t)ch - '0';
    }

    return (std::uint8_t)(ch | (char)32) - 'a' + (char)10;
  };

  for(auto &el : buf) {
    std::uint8_t ch_r = convert(*--data);
    std::uint8_t ch_l = convert(*--data);

    el = (ch_l << 4) | ch_r;
  }

  return *reinterpret_cast<T *>(buf);
}

template<class T>
auto enm(const T& val) -> const std::underlying_type_t<T>& {
  return *reinterpret_cast<const std::underlying_type_t<T>*>(&val);
}

template<class T>
auto enm(T& val) -> std::underlying_type_t<T>& {
  return *reinterpret_cast<std::underlying_type_t<T>*>(&val);
}

template<class T>
using Error = Optional<T>;

template<class ReturnType, class ...Args>
struct Function {
  typedef ReturnType (*type)(Args...);
};

template<class T, class ReturnType, typename Function<ReturnType, T>::type function>
struct Destroy {
  typedef T pointer;
  
  void operator()(pointer p) {
    function(p);
  }
};

template<class T, typename Function<void, T*>::type function>
using safe_ptr = std::unique_ptr<T, Destroy<T*, void, function>>;

// You cannot specialize an alias
template<class T, class ReturnType, typename Function<ReturnType, T*>::type function>
using safe_ptr_v2 = std::unique_ptr<T, Destroy<T*, ReturnType, function>>;


template<class T>
class FakeContainer {
  typedef T pointer;

  pointer _begin;
  pointer _end;

public:
  FakeContainer(pointer begin, pointer end) : _begin(begin), _end(end) {}

  pointer begin() { return _begin; }
  pointer end() { return  _end; }

  const pointer begin() const { return _begin; }
  const pointer end() const { return _end; }

  const pointer cbegin() const { return _begin; }
  const pointer cend() const { return _end; }

  pointer data() { return begin(); }
  const pointer data() const { return cbegin(); }

  std::size_t size() const { return std::distance(begin(), end()); }
};

template<class T>
FakeContainer<T> toContainer(T begin, T end) {
  return { begin, end };
}

template<class T>
FakeContainer<T> toContainer(T begin, std::size_t end) {
  return { begin, begin + end };
}

template<class T>
FakeContainer<T*> toContainer(T * const begin) {
  T *end = begin;

  auto default_val = T();
  while(*end != default_val) {
    ++end;
  }

  return toContainer(begin, end);
}

template<class T, class H>
struct _init_helper;

template<template<class...> class T, class H, class... Args>
struct _init_helper<T<Args...>, H> {
  using type = T<Args...>;

  static type move(Args&&... args, H&&) {
    return std::make_tuple(std::move(args)...);
  }

  static type copy(const Args&... args, const H&) {
    return std::make_tuple(args...);
  }
};

template<class... Args>
auto init(std::tuple<Args...> &&tuple) {
  using tuple_t = std::tuple<Args...>;
  return std::apply(_init_helper<typename __init_types<tuple_t>::type, typename __last_types<tuple_t>::type>::move, std::move(tuple));
}

template<class... Args>
auto init(const std::tuple<Args...> &tuple) {
  using tuple_t = std::tuple<Args...>;
  return std::apply(_init_helper<typename __init_types<tuple_t>::type, typename __last_types<tuple_t>::type>::copy, tuple);
}

namespace endian {
template<class T = void>
struct endianness {
  enum : bool {
#if defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN || \
    defined(__BIG_ENDIAN__) || \
    defined(__ARMEB__) || \
    defined(__THUMBEB__) || \
    defined(__AARCH64EB__) || \
    defined(_MIBSEB) || defined(__MIBSEB) || defined(__MIBSEB__)
    // It's a big-endian target architecture
    little = false,
#elif defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN || \
    defined(__LITTLE_ENDIAN__) || \
    defined(__ARMEL__) || \
    defined(__THUMBEL__) || \
    defined(__AARCH64EL__) || \
    defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__)
    // It's a little-endian target architecture
    little = true,
#else
#error "Unknown Endianness"
#endif
    big = !little
  };
};

template<class T, class S = void>
struct endian_helper { };

template<class T>
struct endian_helper<T, std::enable_if_t<
  !(instantiation_of_v<std::optional, T> || instantiation_of_v<Optional, T>)
>> {
  static inline T big(T x) {
    if constexpr (endianness<T>::little) {
      uint8_t *data = reinterpret_cast<uint8_t*>(&x);

      std::reverse(data, data + sizeof(x));
    }

    return x;
  }

  static inline T little(T x) {
    if constexpr (endianness<T>::big) {
      uint8_t *data = reinterpret_cast<uint8_t*>(&x);

      std::reverse(data, data + sizeof(x));
    }

    return x;
  }
};

template<class T>
struct endian_helper<T, std::enable_if_t<
  instantiation_of_v<std::optional, T> || instantiation_of_v<Optional, T>
  >> {
  static inline T little(T x) {
    if(!x) return x;

    if constexpr (endianness<T>::big) {
      auto *data = reinterpret_cast<uint8_t*>(&*x);

      std::reverse(data, data + sizeof(*x));
    }

    return x;
  }

  static inline T big(T x) {
    if(!x) return x;

    if constexpr (endianness<T>::big) {
      auto *data = reinterpret_cast<uint8_t*>(&*x);

      std::reverse(data, data + sizeof(*x));
    }

    return x;
  }
};

template<class T>
inline auto little(T x) { return endian_helper<T>::little(x); }

template<class T>
inline auto big(T x) { return endian_helper<T>::big(x); }
} /* endian */

inline std::int64_t from_chars(const char *begin, const char *end) {
  std::int64_t res {};
  std::int64_t mul = 1;
  while(begin != --end) {
    res += (std::int64_t)(*end - '0') * mul;

    mul *= 10;
  }

  return *begin != '-' ? res + (std::int64_t)(*begin - '0') * mul : -res;
}

template<class T>
T either(std::optional<T> &&l, T &&r) {
  if(l) {
    return std::move(*l);
  }

  return std::forward<T>(r);
}

} /* util */
#endif
