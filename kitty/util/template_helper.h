#ifndef KITTY_UTIL_TEMPLATE_HELPER_H
#define KITTY_UTIL_TEMPLATE_HELPER_H

#include <type_traits>
#include <string>
#include <string_view>

namespace util {

// True if <Optional, Optional<T>>
template<template<typename...> class X, class T, class...Y>
struct instantiation_of : public std::false_type {};

template<template<typename...> class X, class T, class... Y>
struct instantiation_of<X, X<T, Y...>> : public std::true_type {};

template<template<typename...> class X, class T, class... Y>
struct instantiation_of<X, X<T, Y...>&> : public std::true_type {};

template<template<typename...> class X, class T, class... Y>
struct instantiation_of<X, X<T, Y...>&&> : public std::true_type {};



// True if <Optional, X<Optional<T>>>
template<template<typename...> class X, class T, class...Y>
struct contains_instantiation_of : public std::false_type {};
  
template<template<typename...> class X, class T, class... Y>
struct contains_instantiation_of<X, X<T, Y...>> : public std::true_type {};
template<template<typename...> class X, class T, class... Y>

struct contains_instantiation_of<X, X<T, Y...>&> : public std::true_type {};

template<template<typename...> class X, class T, class... Y>
struct contains_instantiation_of<X, X<T, Y...>&&> : public std::true_type {};
  
template<template<typename...> class X, template<typename...> class T, class... Y>
struct contains_instantiation_of<X, T<Y...>> : public contains_instantiation_of<X, Y...> {};

template<template<typename...> class X, template<typename...> class T, class... Y>
struct contains_instantiation_of<X, T<Y...>&> : public contains_instantiation_of<X, Y...> {};

template<template<typename...> class X, template<typename...> class T, class... Y>
struct contains_instantiation_of<X, T<Y...>&&> : public contains_instantiation_of<X, Y...> {};

template<class T, template<class...> class X>
struct copy_types;

template<template<class...> class T, template<class...> class X, class... Args>
struct copy_types<T<Args...>, X> {

  using type = X<Args...>;
};

template<class T, class ...Prepend>
struct prepend_types;

template<template<class...> class T, class... Prepend, class... Args>
struct prepend_types<T<Args...>, Prepend...> {

  using type = T<Prepend..., Args...>;
};

template<class T, class ...Append>
struct append_types;

template<template<class...> class T, class... Append, class... Args>
struct append_types<T<Args...>, Append...> {

  using type = T<Args..., Append...>;
};

template<class T, class RT>
struct _reverse_types_helper;

template<template<class...> class T, template<class...> class RT, class... RArgs>
struct _reverse_types_helper<T<>, RT<RArgs...>> {
  using type = RT<RArgs...>;
};

template<template<class...> class T, template<class...> class RT, class H, class... Args, class... RArgs>
struct _reverse_types_helper<T<H, Args...>, RT<RArgs...>> {
  using type = typename _reverse_types_helper<T<Args...>, RT<H, RArgs...>>::type;
};

template<class T>
struct reverse_types;

template<template<class...> class T, class ... Args>
struct reverse_types<T<Args...>> {
  using type = typename _reverse_types_helper<T<Args...>, T<>>::type;
};

template<class T>
struct tail_types;

template<template<class...> class T, class H, class... Args>
struct tail_types<T<H, Args...>> {
  using type = T<Args...>;
};

template<class T>
struct init_types {
  using type = typename reverse_types<typename tail_types<typename reverse_types<T>::type>::type>::type;
};

template<class T>
struct head_types;

template<template<class...> class T, class H, class ...Args>
struct head_types<T<H, Args...>> {
  using type = H;
};

template<class T>
struct last_types {
  using type = typename head_types<typename reverse_types<T>::type>::type;
};

template<class T, template<class> class X>
struct map_types;

template<template<class...> class T, template<class> class X, class... Args>
struct map_types<T<Args...>, X> {

  using type = T<typename X<Args>::type...>;
};

template<class X>
struct to_view;

template<>
struct to_view<std::string> {
  using type = std::string_view;
};

template<>
struct to_view<std::string_view> {
  using type = std::string_view;
};

template<class T>
struct type_id {
  using type = T;
};
}
#endif