//
// Created by loki on 11-4-19.
//

#ifndef T_MAN_UNORDERED_MAP_KEY_H
#define T_MAN_UNORDERED_MAP_KEY_H

#include <functional>
#include <kitty/util/utility.h>

namespace util {

template<class X, class Y>
class map_key_t {
public:
  using value_type = X;
  using key_type   = Y;

  explicit map_key_t(value_type &&val) : payload { std::move(val) } {}
  explicit map_key_t(const value_type &val) : payload { val } {}

  explicit map_key_t(key_type &&key) : payload { std::move(key) } {}
  explicit map_key_t(const key_type &key) : payload { key } {}

  map_key_t() = default;
  map_key_t(map_key_t&&) noexcept = default;
  map_key_t&operator=(map_key_t&&) noexcept = default;


  util::Either<value_type, key_type> payload;

  std::size_t hash() const {
    if(payload.has_left()) {
      return std::hash<value_type>()(payload.left());
    }

    return std::hash<key_type>()(payload.right());
  }

  bool operator==(const map_key_t &r) const {
    if(payload.has_left()) {
      if(r.payload.has_left()) {
        return payload.left() == r.payload.left();
      }
      else {
        return payload.left() == r.payload.right();
      }
    }
    else {
      if(r.payload.has_left()) {
        return payload.right() == r.payload.left();
      }
      else {
        return payload.right() == r.payload.right();
      }
    }
  }

  bool operator<(const map_key_t &r) const {
    if(payload.has_left()) {
      if(r.payload.has_left()) {
        return payload.left() < r.payload.left();
      }
      else {
        return payload.left() < r.payload.right();
      }
    }
    else {
      if(r.payload.has_left()) {
        return payload.right() < r.payload.left();
      }
      else {
        return payload.right() < r.payload.right();
      }
    }
  }
};

}

#define KITTY_GEN_HASH_KEY(n, x, y, z) \
namespace n { using x = ::util::map_key_t<y,z>; }\
template<> struct std::hash<n::x> { std::size_t operator()(const n::x &key) const { return key.hash(); } };


#endif //T_MAN_UNORDERED_MAP_KEY_H
