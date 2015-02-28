#ifndef UTILITY_H
#define UTILITY_H

#include <vector>
#include <memory>

#include <kitty/util/optional.h>

namespace util {
template<class T>
class Hex {
public:
  typedef T elem_type;
private:
  const char _bits[16] {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
  };

  uint8_t _hex[sizeof(elem_type) * 2];
public:
  Hex(elem_type && elem) {
    const uint8_t *data = reinterpret_cast<const uint8_t *>(&elem) + sizeof(elem_type);
    for (elem_type *it = begin(); it < cend();) {
      *it++ = _bits[*--data / 16];
      *it++ = _bits[*data   % 16];
    }
  }

  Hex(elem_type &elem) {
    const uint8_t *data = reinterpret_cast<const uint8_t *>(&elem) + sizeof(elem_type);
    for (uint8_t *it = begin(); it < cend();) {
      *it++ = _bits[*--data / 16];
      *it++ = _bits[*data   % 16];
    }
  }

  uint8_t *begin() { return _hex; }
  uint8_t *end() { return _hex + sizeof(elem_type) * 2; }

  const uint8_t *cbegin() { return _hex; }
  const uint8_t *cend() { return _hex + sizeof(elem_type) * 2; }
};

template<class T>
Hex<T> hex(T &elem) {
  return Hex<T>(elem);
}

template<class T, class... Args>
std::unique_ptr<T> mk_uniq(Args && ... args) {
  typedef T elem_type;

  return std::unique_ptr<elem_type> {
    new elem_type { std::forward<Args>(args)... }
  };
}

template<class T>
using Error = Optional<T>;
}
#endif
