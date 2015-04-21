#ifndef OPTIONAL_H
#define OPTIONAL_H

#include <utility>
#include <cstdint>
namespace util {
template<class T>
class Optional {
  class object {
  public:
    typedef T obj_t;
  private:
    
    obj_t *_obj_p;
    uint8_t _obj[sizeof(obj_t)];
  public:
    object() : _obj_p(nullptr) {}
    object(obj_t &&obj) {
      // Call constructor add the address of _obj
      _obj_p = new (_obj) obj_t(std::move(obj));
    }
    
    object(object &&other) : _obj_p(nullptr) {
      _construct_with(other);
    }
    
    object & operator = (obj_t &&other) {
      if(constructed()) {
        get() = std::move(other);
      }
      else {
        _obj_p = new (_obj) obj_t(std::move(other));
      }
      
      return *this;
    }
    
    // Swap objects
    object & operator = (object &&other) {    
      if(constructed()) {
        if(other.constructed()) {
          get() = std::move(other.get());
          
          std::swap(_obj_p, other._obj_p);
        }
        else {
          other._construct_with(*this);
        }
      }
      else {
        _construct_with(other);
      }
      
      return *this;
    }
    
    ~object() {
      if(constructed()) {
        get().~obj_t();
      }
    }
    
    const bool constructed() const { return _obj_p != nullptr; }
    
    const obj_t &get() const {
      return *_obj_p;
    }
    
    obj_t &get() {
      return *_obj_p;
    }
    
  private:
    void _construct_with(object &other) {
      if(other.constructed()) {
        _obj_p = new (_obj) obj_t(std::move(other.get()));
        
        other._obj_p = nullptr;
      }
    }
  };  
public:
  typedef T elem_t;
  
  object _obj;
  
  Optional() = default;
  
  Optional(Optional&&) = default;
  
  Optional(elem_t &&val) : _obj(std::move(val)) {}
  Optional(elem_t &val) : _obj(std::move(val)) {}
  
  Optional &operator = (Optional&&) = default;
  
  elem_t &operator = (elem_t &&elem) { _obj = std::move(elem); return _obj.get(); }
  
  bool isEnabled() const {
    return _obj.constructed();
  }
  
  explicit operator bool () const {
    return _obj.constructed();
  }
  
  operator elem_t&() {
    return _obj.get();
  }
  
  const elem_t* operator->() const {
    return &_obj.get();
  }
  
  const elem_t& operator*() const {
    return _obj.get();
  }

  elem_t* operator->() {
    return &_obj.get();
  }

  elem_t& operator*() {
    return _obj.get();
  }
};
}
#endif // OPTIONAL_H
