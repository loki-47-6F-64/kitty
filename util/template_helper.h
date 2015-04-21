#ifndef KITTY_UTIL_TEMPLATE_HELPER_H
#define KITTY_UTIL_TEMPLATE_HELPER_H

#include <type_traits>

template<template<typename...> class X, typename T>
struct instantiation_of : public std::false_type {};

template<template<typename...> class X, typename... Y>
struct instantiation_of<X, X<Y...>> : public std::true_type {};

#endif