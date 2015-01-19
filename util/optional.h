#ifndef OPTIONAL_H
#define OPTIONAL_H

#include <utility>

namespace util {
template<class T>
class Optional {
  bool _enable;
  
public:
  T val;
  
  Optional() : _enable(false) {}
  Optional(T &&val) : _enable(true), val(std::move(val)) {}
  Optional(T &val) : _enable(true), val(std::move(val)) {}
  
  bool isEnabled() const {
    return _enable;
  }
  
  explicit operator bool () const {
    return _enable;
  }
  
  operator T&() {
    return val;
  }
  
  const T* operator->() const {
    return &val;
  }
  
  const T& operator*() const {
    return val;
  }

  T* operator->() {
    return &val;
  }

  T& operator*() {
    return val;
  }
};
}
#endif // OPTIONAL_H
