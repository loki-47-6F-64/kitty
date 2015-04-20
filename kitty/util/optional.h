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
    
    uint8_t _obj[sizeof(obj_t)];
    
    bool _constructed;
  public:
    object() : _constructed(false) {}
    object(obj_t &&obj) {
      // Call constructor add the address of _obj
      new (_obj) obj_t(std::move(obj));
      
      _constructed = true;
    }
    
    object(object &&other) : _constructed(false) {
      _construct_with(other);
    }
    
    object & operator = (obj_t &&other) {
      if(_constructed) {
        get() = std::move(other.get());
      }
      else {
        new (_obj) obj_t(std::move(other.get()));
        
        _constructed = true;
      }
    }
    
    // Swap objects
    object & operator = (object &&other) {    
      if(_constructed) {
        if(other.constructed()) {
          get() = std::move(other.get());
          
          std::swap(_constructed, other._constructed);
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
      if(_constructed) {
        get().~obj_t();
      }
    }
    
    const bool constructed() const { return _constructed; }
    
    const obj_t &get() const { return *reinterpret_cast<const obj_t*>(_obj); }
    obj_t &get() { return *reinterpret_cast<obj_t*>(_obj); }
    
  private:
    void _construct_with(object &other) {
      if(other.constructed()) {
        new (_obj) obj_t(std::move(other.get()));
        
        other._constructed = false;
        _constructed = true;
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
  
  bool isEnabled() const {
    return _obj.constructed();
  }
  
  explicit operator bool () const {
    return _obj.constructed();;
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
