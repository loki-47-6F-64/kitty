#ifndef KITTY_UTIL_SET_H
#define KITTY_UTIL_SET_H

#include <algorithm>
#include <iterator>
#include <vector>
#include <utility>
#include <type_traits>
#include <kitty/util/optional.h>

namespace util {
template<class Container, class It>
Container concat(It begin, It end) {
  Container str;
  
  for(;begin < end; ++begin) {
    std::copy(std::begin(*begin), std::end(*begin), std::back_inserter(str));
  }
  
  return str;
}

template<class Container, class Array>
Container concat(Array &&container) {
  return concat<Container>(std::begin(container), std::end(container));
}

template<class From, class Function>
inline auto map(From &&from, Function f) -> std::vector<decltype(f(*std::begin(from)))> {
  typedef decltype(f(*std::begin(from))) input_type;
  return map_if(std::forward<From>(from), [&](input_type &input) {
    return util::Optional<input_type> { f(input) };
  });
}

template<class From, class Function>
inline auto map_if(From &&from, Function f) -> 
  std::vector<
    typename std::remove_const<
      typename std::remove_reference<decltype(*f(*std::begin(from)))>::type
    >::type
  > 
{
  typedef
    typename std::remove_const<
      typename std::remove_reference<decltype(*f(*std::begin(from)))>::type
    >::type input_type;
  
  std::vector<input_type> result;
  result.reserve(std::distance(std::begin(from), std::end(from)));
  
  for(auto &input : from) {
    auto optional = f(input);
    
    if(optional) {
      result.emplace_back(std::move(*optional));
    }
  }
  
  return result;
}


template<class Container, class Function>
typename std::remove_reference<Container>::type
copy_if(Container &&from, Function f) {
  typename std::remove_reference<Container>::type result;
  
  std::copy_if(std::begin(from), std::end(from), std::back_inserter(result), f);
  
  return result;
}

template<class Container, class Function>
typename std::remove_reference<Container>::type
move_if(Container &&from, Function f) {
  typename std::remove_reference<Container>::type result;
  
  std::copy_if(
    std::make_move_iterator(std::begin(from)),
    std::make_move_iterator(std::end(from)),
    std::back_inserter(result),
    f
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
std::vector<typename std::remove_reference<Container>::type>
split(Container &&container, const Type &&val) {
  
  std::vector<typename std::remove_reference<Container>::type> vecContainer;
  
  auto begin = std::begin(container);
  for(auto end = begin; end < std::end(container); ++end) {
    if(val == *end && begin < end) {
      vecContainer.emplace_back(begin, end);
      begin = end;
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