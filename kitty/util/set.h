#ifndef KITTY_UTIL_SET_H
#define KITTY_UTIL_SET_H

#include <tuple>
#include <algorithm>
#include <iterator>
#include <vector>
#include <utility>
#include <type_traits>
#include <array>
#include <optional>

#include <kitty/util/template_helper.h>

namespace util {
  template<class It>
  auto concat(It begin, It end) {
    using Container = std::decay_t<decltype(*begin)>;
    Container str;
    
    for(;begin != end; ++begin) {
      std::copy(std::begin(*begin), std::end(*begin), std::back_inserter(str));
    }
    
    return str;
  }

  template<class Array>
  auto concat(Array &&container) {
    return concat(std::begin(container), std::end(container));
  }

  template<class T>
  auto concat(const std::initializer_list<T> &initializer_list) {
    return concat(std::begin(initializer_list), std::end(initializer_list));
  }
  
  template<class It, class Function>
  inline auto map_if(It begin, It end, Function &&f) {
    using output_t = std::decay_t<decltype(*f(*begin))>;
    
    std::vector<output_t> result;
    result.reserve(std::distance(begin, end));
    
    for(;begin != end; ++begin) {
      auto optional = f(*begin);
      
      if(optional) {
        result.emplace_back(std::move(*optional));
      }
    }
    
    return result;
  }
  
  template<class From, class Function>
  inline auto map_if(From &&from, Function &&f) {
    return map_if(std::begin(from), std::end(from), std::forward<Function>(f));
  }
  
  template<class It, class Function>
  inline auto map(It begin, It end, Function &&f) {
    return map_if(begin, end, [&](auto &input) {
      return std::optional { f(input) };
    });
  }
  
  template<class From, class Function>
  inline auto map(From &&from, Function &&f) {
    return map(std::begin(from), std::end(from), std::forward<Function>(f));
  }
  
  template<class It, class Function>
  inline auto fold(It begin, It end, Function &&f) {
    if(begin + 1 == end) {
      return *begin;
    }
    
    auto result = fold(begin + 1, end, f);
    
    return f(*begin, result);
  }
  
  template<class From, class Function>
  inline auto fold(From &&from, Function &&f) {
    return fold(std::begin(from), std::end(from), std::forward<Function>(f));
  }
  
  template<class Container, class Function>
  auto copy_if(Container &&from, Function &&f) {
    std::decay_t<Container> result;
    
    std::copy_if(std::begin(from), std::end(from), std::back_inserter(result), std::forward<Function>(f));
    
    return result;
  }
  
  template<class Container, class Function>
  typename std::remove_reference<Container>::type
  move_if(Container &&from, Function &&f) {
    typename std::remove_reference<Container>::type result;
    
    std::copy_if(
      std::make_move_iterator(std::begin(from)),
      std::make_move_iterator(std::end(from)),
      std::back_inserter(result),
      std::forward<Function>(f)
    );
    
    return result;
  }
  
  
  template<class To, class It>
  To copy_to(It begin, It end) {
    return To(begin, end);
  }
  
  template<class To, class From>
  To copy_to(From &&from) {
    return copy_to<To>(std::begin(from), std::end(from));
  }
  
  template<class To, class It>
  To move_to(It begin, It end) {
    return copy_to<To>(
      std::make_move_iterator(begin),
      std::make_move_iterator(end)
    );
  }
  
  template<class To, class From>
  To move_to(From &&from) {  
    return move_to<To>(std::begin(from), std::end(from));
  }
  
  template<class Container, class Type>
  auto split(Container &&container, const Type &val) {
    std::vector<typename __to_view<std::decay_t<Container>>::type> vecContainer;
    
    auto begin = std::begin(container);
    auto end = begin;
    for(; end < std::end(container); ++end) {
      if(val == *end) {
        vecContainer.emplace_back(&*begin, std::distance(begin, end));
        begin = end +1;
      }
    }
    
    if(begin < end) {
      vecContainer.emplace_back(&*begin, std::distance(begin, end));
    }

    return vecContainer;
  }

  template<class Container, class Function>
  bool any(Container &&container, Function &&f) {
    for(auto &input : container) {
      if(f(input))
        return true;
    }
    
    return false;
  }

  template<class Container, class Function>
  bool all(Container &&container, Function &&f) {
    for(auto &input : container) {
      if(!f(input))
        return false;
    }

    return true;
  }

  template<class T, class Function>
  bool any(const std::initializer_list<T> &container, Function &&f) {
    for(auto &input : container) {
      if(f(input))
        return true;
    }

    return false;
  }

  template<class T, class Function>
  bool all(const std::initializer_list<T> &container, Function &&f) {
    for(auto &input : container) {
      if(!f(input))
        return false;
    }

    return true;
  }
  
  template<std::size_t N, class Array, class Arg>
  Array _make_array(Array &array, Arg&& arg) {
    array[std::tuple_size<Array>::value - N] = std::forward<Arg>(arg);
    
    return array;
  }
  
  
  template<std::size_t N, class Array, class Arg, class... Args>
  Array _make_array(Array &array, Arg&& arg, Args&& ... args) {
    array[std::tuple_size<Array>::value - N] = std::forward<Arg>(arg);
    
    return _make_array<N - 1>(array, std::forward<Args>(args)...);
  }
  
  template<class... Args, std::size_t N = sizeof... (Args)>
  std::array<typename __head_types<Args...>::type, N> make_array(Args&& ... args) {
    typedef std::array<typename __head_types<Args...>::type, N> Array;
    
    Array arr;
    return _make_array<std::tuple_size<Array>::value - 1>(arr, std::forward<Args>(args)...);  
  }
  
}
#endif
