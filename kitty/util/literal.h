//
// Created by loki on 25-2-19.
//

#ifndef KITTY_STRING_H
#define KITTY_STRING_H

#include <cstddef>

namespace literal {

struct __concat_impl_t {};
struct __string_impl_t {};
struct __empty_impl_t  {};

template<std::size_t N, class T>
struct string_t;

template<>
struct string_t<0, __empty_impl_t> {
  const char *begin() const {
    return nullptr;
  }

  const char *end() const {
    return nullptr;
  }

  constexpr auto size() const {
    return (std::size_t)0;
  }

  std::string native() const {
    return std::string { };
  }

  constexpr std::string_view to_view() const {
    return std::string_view { nullptr, size() };
  }
};

template<std::size_t N>
struct string_t<N, __string_impl_t> {
  const char (&data)[N +1];

  constexpr string_t(const char (&data)[N +1]) noexcept : data { data } {}

  const char *begin() const {
    return data;
  }

  const char *end() const {
    return data + N;
  }

  constexpr auto size() const {
    return N;
  }

  std::string native() const {
    return std::string { data, size() };
  }

  constexpr std::string_view to_view() const {
    return std::string_view { data, size() };
  }
};

template<std::size_t N>
struct string_t<N, __concat_impl_t> {
  char data[N +1];

  template<std::size_t N1, class X1, class X2>
  constexpr string_t(const string_t<N1, X1> &l, const string_t<N - N1, X2> &r) noexcept : data {} {
    for(auto x = 0; x < N1; ++x) {
      data[x] = l.data[x];
    }

    for(auto x = 0; x < N - N1; ++x) {
      data[x + N1] = r.data[x];
    }
  }

  constexpr string_t(const std::array<char, N> &array) noexcept : data {} {
    for(auto x = 0; x < N; ++x) {
      data[x] = array[x];
    }
  }

  const char *begin() const {
    return data;
  }

  const char *end() const {
    return data + N;
  }

  constexpr auto size() const {
    return N;
  }

  std::string native() const {
    return std::string { data, size() };
  }

  constexpr std::string_view to_view() const {
    return std::string_view { data, size() };
  }
};

template<std::size_t N>
string_t(const char (&data)[N]) -> string_t<N -1, __string_impl_t>;
string_t() -> string_t<0, __empty_impl_t>;

template<std::size_t N1, std::size_t N2, class T1, class T2>
constexpr auto operator +(const string_t<N1, T1> &l, const string_t<N2, T2> &r) {
  return string_t<N1 + N2, __concat_impl_t>(l, r);
}

template<std::size_t N1, class T1>
constexpr auto operator +(const string_t<N1, T1> &l, const string_t<0, __empty_impl_t> &r) {
  return l;
}

template<std::size_t N2, class T2>
constexpr auto operator +(const string_t<0, __empty_impl_t> &l, const string_t<N2, T2> &r) {
  return r;
}

template<std::size_t N2>
constexpr auto operator +(const string_t<0, __empty_impl_t>&, const char (&r)[N2]) {
  return string_t { r };
}

template<std::size_t N1>
constexpr auto operator +(const char (&l)[N1], const string_t<0, __empty_impl_t>&) {
  return string_t { l };
}

template<std::size_t N1, std::size_t N2, class T1>
constexpr auto operator +(const string_t<N1, T1> &l, const char (&r)[N2]) {
  return string_t<N1 + N2 -1, __concat_impl_t>(l, string_t { r });
}

template<std::size_t N1, std::size_t N2, class T2>
constexpr auto operator +(const char (&l)[N1], const string_t<N2, T2> &r) {
  return string_t<N1 + N2 -1, __concat_impl_t>(string_t { l }, r);
}

template<auto... Elements>
struct literal_t {
  static constexpr std::array value { Elements... };

  using value_type = typename decltype(value)::value_type;

  constexpr static string_t<sizeof...(Elements), __concat_impl_t> to_string() {
    return string_t<sizeof...(Elements), __concat_impl_t> {
      value
    };
  }
};

template<>
struct literal_t<> {
  constexpr static string_t<0, __empty_impl_t> to_string() {
    return string_t<0, __empty_impl_t> {};
  }
};

template<class T, class X>
struct __concat_literal;

template<auto... ElL, auto... ElR>
struct __concat_literal<literal_t<ElL...>, literal_t<ElR...>> {
  using type = literal_t<ElL..., ElR...>;
};

template<class T, class X>
using concat_t = typename __concat_literal<T, X>::type;

template<auto V, char... Elements>
struct __from_integral_helper {
  using type = typename __from_integral_helper<V / 10, (char)(V % 10) + '0', Elements...>::type;
};

template<char... Elements>
struct __from_integral_helper<0, Elements...> {
  using type = literal_t<Elements...>;
};

template<auto V, char... Elements>
struct __to_string_literal {
  using type = typename __from_integral_helper<V / 10, (char)(V % 10) + '0', Elements...>::type;
};

template<auto V>
using string_literal_t = typename __to_string_literal<V>::type;

template<class T>
struct __tail;

template<class T>
struct __head;

template<auto _, auto...Elements>
struct __tail<literal_t<_, Elements...>> {
  using type = literal_t<Elements...>;
};

template<auto H, auto..._>
struct __head<literal_t<H, _...>> {
  using type = literal_t<H>;
};

template<class T>
using tail_t = typename __tail<T>::type;

template<class T>
using head_t = typename __head<T>::type;

template<class T, template<auto> class X>
struct __map;

template<template<auto...> class T, template<auto> class X, auto... Args>
struct __map<T<Args...>, X> {
  using type = typename T<X<Args>::value...>::type;
};

template<class T, template<auto> class X>
using map_t = typename __map<T, X>::type;

template<class T, template<auto, class> class X, class D>
struct __fold;

template<template<auto...> class T, template<auto, class> class X, class D>
struct __fold<T<>, X, D> {
  using type = D;
};

template<template<auto...> class T, template<auto, class> class X, class D, auto V, auto...Args>
struct __fold<T<V, Args...>, X, D> {
  using type = typename X<V, typename __fold<T<Args...>, X, D>::type>::type;
};

template<class T, template<auto, class> class X, class D>
using fold_t = typename __fold<T, X, D>::type;

template<class T>
struct __from_index_sequence;

template<class X, X...Args>
struct __from_index_sequence<std::integer_sequence<X, Args...>> {
  using type = literal_t<Args...>;
};

template<auto T>
using index_sequence_t = typename __from_index_sequence<std::make_index_sequence<T>>::type;

template<class T, auto... X>
struct __prepend;

template<class T, auto... X>
struct __append;

template<auto... T, auto...Args>
struct __prepend<literal_t<Args...>, T...> {
  using type = literal_t<T..., Args...>;
};

template<auto... T, auto...Args>
struct __append<literal_t<Args...>, T...> {
  using type = literal_t<Args..., T...>;
};

template<class T, auto... X>
using prepend_t = typename __prepend<T, X...>::type;

template<class T, auto... X>
using append_t = typename __append<T, X...>::type;

template<class T>
using quote_t = prepend_t<append_t<T, '"'>, '"'>;

template<auto T>
using string_literal_quote_t = quote_t<string_literal_t<T>>;
}

#endif //KITTY_STRING_H
