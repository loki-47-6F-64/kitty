#ifndef KITTY_UTIL_SET_H
#define KITTY_UTIL_SET_H

#include <iterator>
#include <vector>
#include <utility>
#include <type_traits>

namespace util {
template<class Container, class It>
Container concat(It begin, It end) {
  Container str;
  
  for(;begin < end; ++begin) {
    str.insert(str.end(), std::begin(*begin), std::end(*begin));
  }
  
  return str;
}

template<class Container, class Array>
Container concat(Array &&container) {
  return concat<Container>(std::begin(container), std::end(container));
}

template<class From, class Function>
inline auto map(From &&from, Function f) -> std::vector<decltype(f(*std::begin(from)))> {
  typedef decltype(f(*std::begin(from))) return_type;
  
  std::vector<return_type> result;
  for(auto &input : from) {
    result.emplace_back(f(input));
  }
  
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
  To to;
  for(auto it = begin; it != end; ++it) {
    to.emplace_back(std::move(*it));
  }
  
  return to;
}

template<class To, class From>
To move_to(From &&from) {  
  return move_to<To>(std::begin(from), std::end(from));
}

template<class Container, class Type>
std::vector<typename std::remove_reference<Container>::type>
split(Container &&container, const Type &&val) {
  
  std::vector<typename std::remove_reference<Container>::type> vecContainer;
  
  auto begin = std::begin(container);
  for(auto end = begin; end < std::end(container); ++end) {
    if(val == *end) {
      vecContainer.emplace_back(begin, end);
    }
  }
  
  return vecContainer;
}


template<class Container, class Function>
bool any(Container &&container, Function f) {
  for(auto &input : container) {
    if(f(input))
      return true;
  }
  
  return false;
}
}
#endif