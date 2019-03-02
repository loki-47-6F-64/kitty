#ifndef KITTY_UTIL_TEMPLATE_HELPER_H
#define KITTY_UTIL_TEMPLATE_HELPER_H

#include <type_traits>
#include <string>
#include <string_view>

namespace util {

template<class T>
struct __print_type;

template<class T>
void print_type(T& type) {
  __print_type<T> pt;
  __print_type<decltype(type)> ppt;
}

template<class T>
void print_uni_type(T&& type) {
  __print_type<T> pt;
  __print_type<decltype(type)> ppt;
}

template<class T, template<class...> class X>
struct __copy_types;

template<template<class...> class T, template<class...> class X, class... Args>
struct __copy_types<T<Args...>, X> {

  using type = X<Args...>;
};

template<class T, template<class...> class X>
using copy_types = typename __copy_types<T, X>::type;

template<class T, class ...Prepend>
struct __prepend_types;

template<template<class...> class T, class... Prepend, class... Args>
struct __prepend_types<T<Args...>, Prepend...> {

  using type = T<Prepend..., Args...>;
};

template<class T, class ...Prepend>
using prepend_types = typename __prepend_types<T, Prepend...>::type;

template<class T, class ...Append>
struct __append_types;

template<template<class...> class T, class... Append, class... Args>
struct __append_types<T<Args...>, Append...> {

  using type = T<Args..., Append...>;
};

template<class T, class ...Append>
using append_types = typename __append_types<T, Append...>::type;

template<class T, class RT>
struct __reverse_types_helper;

template<template<class...> class T, template<class...> class RT, class... RArgs>
struct __reverse_types_helper<T<>, RT<RArgs...>> {
  using type = RT<RArgs...>;
};

template<template<class...> class T, template<class...> class RT, class H, class... Args, class... RArgs>
struct __reverse_types_helper<T<H, Args...>, RT<RArgs...>> {
  using type = typename __reverse_types_helper<T<Args...>, RT<H, RArgs...>>::type;
};

template<class T>
struct __reverse_types;

template<template<class...> class T, class ... Args>
struct __reverse_types<T<Args...>> {
  using type = typename __reverse_types_helper<T<Args...>, T<>>::type;
};

template<class T>
using reverse_types = typename __reverse_types<T>::type;

template<class T>
struct __tail_types;

template<template<class...> class T, class H, class... Args>
struct __tail_types<T<H, Args...>> {
  using type = T<Args...>;
};

template<class T>
using tail_types = typename __tail_types<T>::type;
template<class T>
struct __init_types {
  using type = reverse_types<tail_types<reverse_types<T>>>;
};

template<class T>
using init_types = typename __init_types<T>::type;

template<class T>
struct __head_types;

template<template<class...> class T, class H, class ...Args>
struct __head_types<T<H, Args...>> {
  using type = H;
};

template<class T>
using head_types = typename __head_types<T>::type;

template<class T>
struct __last_types {
  using type = typename __head_types<typename __reverse_types<T>::type>::type;
};

template<class T>
using last_types = typename __last_types<T>::type;

template<class T, template<class> class X>
struct __map_types;

template<template<class...> class T, template<class> class X, class... Args>
struct __map_types<T<Args...>, X> {

  using type = T<typename X<Args>::type...>;
};

template<class T, template<class> class X>
using map_types = typename __map_types<T, X>::type;

template<class X>
struct __to_view;

template<>
struct __to_view<std::string> {
  using type = std::string_view;
};

template<>
struct __to_view<std::string_view> {
  using type = std::string_view;
};

template<class X>
using to_view_t = typename __to_view<X>::type;

template<class T, template<class, auto> class X, auto D>
struct __fold_v;

template<template<class...> class T, template<class, auto> class X, auto D>
struct __fold_v<T<>, X, D> {
  static constexpr auto value = D;
};

template<template<class...> class T, template<class, auto> class X, auto D, class V, class...Args>
struct __fold_v<T<V, Args...>, X, D> {
  static constexpr auto value = X<V, __fold_v<T<Args...>, X, D>::value>::value;
};

template<class T, template<class, auto> class X, auto D>
static constexpr auto fold_v = __fold_v<T, X, D>::value;

template<class T, template<class> class Pred>
struct __count_if {
  template<class A, std::size_t V>
  struct __fold_count_helper {
    static constexpr auto value = Pred<A>::value ? V +1 : V;
  };

  static constexpr std::size_t value = util::fold_v<T, __fold_count_helper, (std::size_t)0>;
};

template<class T, template<class> class Pred>
static constexpr auto count_if_v = __count_if<T, Pred>::value;

// True if <Optional, Optional<T>>
template<template<typename...> class X, class...Y>
struct __instantiation_of : public std::false_type {};

template<template<typename...> class X, class... Y>
struct __instantiation_of<X, X<Y...>> : public std::true_type {};

template<template<typename...> class X, class T, class...Y>
static constexpr auto instantiation_of_v = __instantiation_of<X, T, Y...>::value;

// True if <Optional, X<Optional<T>>>
template<template<typename...> class X, class T, class...Y>
struct __contains_instantiation_of : public std::false_type {};

template<template<typename...> class X, class... Y>
struct __contains_instantiation_of<X, X<Y...>> : public std::true_type {};

template<template<typename...> class X, template<typename...> class T, class... Y>
struct __contains_instantiation_of<X, T<Y...>> {
  template<class In>
  struct __Pred {
    static constexpr bool value = __contains_instantiation_of<X, In>::value;
  };

  static constexpr bool value = count_if_v<std::tuple<Y...>, __Pred> > 0;
};

template<template<typename...> class X, class T, class...Y>
static constexpr auto contains_instantiation_of_v = __contains_instantiation_of<X, T, Y...>::value;
}
#endif