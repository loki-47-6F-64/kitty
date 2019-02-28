#pragma once

#include <tuple>
#include <type_traits>
#include <algorithm>
#include <array>
#include <optional>

#include <sqlite3.h>

#include <kitty/util/utility.h>
#include <kitty/util/template_helper.h>
#include <kitty/util/literal.h>

namespace sqlite {
typedef util::safe_ptr_v2<sqlite3, int, sqlite3_close> sDatabase;
typedef util::safe_ptr_v2<sqlite3_stmt, int, sqlite3_finalize> sStatement;

sDatabase mk_db(const char *dbName);

sStatement mk_statement(sqlite3 *db, const char *sql);

enum comp_t : int {
  eq,
  nt_eq,
  gt,
  st,
  st_eq,
  gt_eq,
  is,
  is_not,
};

extern const char *comp_to_string[];

// Add room for id
template<class Tuple>
using model_t = typename util::__append_types<Tuple, sqlite3_int64>::type;

// Type aliasing without make std::is_same<T, unique_t<T>> true
template<class T, class V = void>
class unique_t : public T {
public:
  typedef T elem_t;
  using T::T;

  template<class ...Args>
  unique_t(Args &&... args) : T { std::forward<Args>(args)... } { }

  unique_t(const unique_t &) = default;

  unique_t(unique_t &&) noexcept = default;

  unique_t &operator=(const unique_t &) = default;

  unique_t &operator=(unique_t &&) noexcept = default;

  unique_t &operator=(const T &other) {
    _this() = other;

    return *this;
  }

  unique_t &operator=(T &&other) {
    _this() = std::move(other);

    return *this;
  }

private:
  T &_this() { return *reinterpret_cast<T *>(this); }
};

// Type aliasing without make std::is_same<T, unique_t<T>> true
template<class T>
class unique_t<T, std::enable_if_t<!std::is_class<T>::value>> {
  T _t;
public:
  typedef T elem_t;

  unique_t() = default;

  unique_t(const unique_t &) = default;

  unique_t(unique_t &&) noexcept = default;

  unique_t &operator=(const unique_t &) = default;

  unique_t &operator=(unique_t &&) noexcept = default;

  unique_t(const T &other) : _t { other } { }

  unique_t(T &&other) : _t { std::move(other) } { }

  operator elem_t &() { return _t; }

  operator const elem_t &() const { return _t; }
};

template<class T>
struct range_t {
  using value_type = T;

  T lower;
  T upper;
};

template<class T>
using optional_unique_t = std::optional<unique_t<T>>;

template<class ...Args>
class __filter_t {
  template<class T>
  struct __to_optional {
    template<class X, class V = void>
    struct __to_optional_helper {
      using type = std::optional<std::pair<comp_t, X>>;
    };


    template<class X>
    struct __to_optional_helper<X, std::enable_if_t<util::contains_instantiation_of_v<range_t, X>>> {
      using type = std::optional<X>;
    };

    using type = typename __to_optional_helper<T>::type;
  };

public:
  // using tuple_optional = std::tuple<std::optional<std::pair<comp_t, Args>>...>;
  using tuple_optional = util::map_types<std::tuple<Args...>, __to_optional>;

  __filter_t() = default;
  __filter_t(__filter_t &&) noexcept = default;

  tuple_optional tuple;

  template<std::size_t elem, class ...ConstructArgs>
  __filter_t &set(comp_t comp, ConstructArgs &&... args) {
    // Get the undelying element type from the optional
    using elem_t = typename std::tuple_element_t<elem, tuple_optional>::value_type;
    static_assert(!util::contains_instantiation_of_v<range_t, elem_t>, "Don't pass a comp_t to a range_t");

    std::get<elem>(tuple) = elem_t { comp, { std::forward<ConstructArgs>(args)... }};

    return *this;
  }

  template<std::size_t elem, class... ConstructArgs>
  __filter_t &set(ConstructArgs&&... args) {
    using elem_t = typename std::tuple_element_t<elem, tuple_optional>::value_type;
    static_assert(util::contains_instantiation_of_v<range_t, elem_t>, "Pass a comp_t for any non-range_t type");

    std::get<elem>(tuple) = elem_t { std::forward<ConstructArgs>(args)... };

    return *this;
  }
};

template<class T>
struct type_value;

template<>
struct type_value<bool> {
  using type = literal::concat_t<literal::literal_t<'b','o','o','l'>, literal::string_literal_t<5>>;
};

template<>
struct type_value<std::int8_t> {
  using type = literal::concat_t<literal::literal_t<'c', 'h', 'a', 'r'>, literal::string_literal_t<8>>;
};

template<>
struct type_value<std::int16_t> {
  using type = literal::concat_t<literal::literal_t<'s', 'h', 'o', 'r', 't'>, literal::string_literal_t<16>>;
};

template<>
struct type_value<std::int32_t> {
  using type = literal::concat_t<literal::literal_t<'i', 'n', 't'>, literal::string_literal_t<32>>;
};

template<>
struct type_value<std::int64_t> {
  using type = literal::concat_t<literal::literal_t<'l', 'o', 'n', 'g'>, literal::string_literal_t<64>>;
};

template<>
struct type_value<std::uint8_t> {
  using type = literal::concat_t<literal::literal_t<'b', 'y', 't', 'e', ' ', 'u', 'n', 's', 'i', 'g', 'n', 'e', 'd'>, literal::string_literal_t<8>>;
};

template<>
struct type_value<std::uint16_t> {
  using type = literal::concat_t<literal::literal_t<'s', 'h', 'o', 'r', 't', ' ', 'u', 'n', 's', 'i', 'g', 'n', 'e', 'd'>, literal::string_literal_t<16>>;
};

template<>
struct type_value<std::uint32_t> {
  using type = literal::concat_t<literal::literal_t<'i', 'n', 't', ' ', 'u', 'n', 's', 'i', 'g', 'n', 'e', 'd'>, literal::string_literal_t<32>>;
};

template<>
struct type_value<std::uint64_t> {
  using type = literal::concat_t<literal::literal_t<'l', 'o', 'n', 'g', ' ', 'u', 'n', 's', 'i', 'g', 'n', 'e', 'd'>, literal::string_literal_t<64>>;
};

template<>
struct type_value<double> {
  using type = literal::literal_t<'d','o','u','b','l','e'>;
};

template<>
struct type_value<float> {
  using type = literal::literal_t<'f','l','o','a','t'>;
};

template<>
struct type_value<std::string> {
  using type = literal::concat_t<literal::literal_t<'s', 't', 'r', 'i', 'n', 'g'>, literal::string_literal_t<128>>;
};

template<class T>
struct type_value<std::vector<T>> {
  using type = literal::concat_t<typename type_value<T>::type, literal::literal_t<'v', 'e', 'c', 't', 'o', 'r'>>;
};

template<class T>
struct type_value<unique_t<T>> {
  using type = literal::concat_t<typename type_value<T>::type, literal::literal_t<'U', 'n', 'i', 'q', 'u', 'e'>>;
};

template<class T>
struct type_value<std::optional<T>> {
  using type = literal::concat_t<typename type_value<T>::type, literal::literal_t<'o', 'p', 't', 'i', 'o', 'n', 'a', 'l'>>;
};

template<class T>
struct type_value<range_t<T>> {
  using type = literal::concat_t<typename type_value<T>::type, literal::literal_t<'r','a','n','g','e','_','t'>>;
};

template<class T>
using type_value_t = typename type_value<T>::type;

template<std::size_t I, class T>
struct fold_columns_name;

template<std::size_t I, class T>
struct fold_columns_bind;

template<std::size_t I, class T>
struct fold_columns_update;

template<std::size_t I, auto...Args>
struct fold_columns_name<I, literal::literal_t<Args...>> {
  using type = literal::concat_t<
    literal::concat_t<literal::string_literal_quote_t<I>, literal::literal_t<','>>,
    literal::literal_t<Args...>
    >;
};

template<std::size_t I, auto...Args>
struct fold_columns_bind<I, literal::literal_t<Args...>> {
  using type = literal::concat_t<literal::literal_t<Args...>, literal::literal_t<'?',','>>;
};

template<std::size_t I, auto...Args>
struct fold_columns_update<I, literal::literal_t<Args...>> {
  using type = literal::concat_t<
    literal::concat_t<literal::string_literal_quote_t<I>, literal::literal_t<'=', '?',','>>,
    literal::literal_t<Args...>
  >;
};

template<class tuple_t, std::size_t tuple_index, class _ = void>
class __SQL {
  static constexpr std::size_t tuple_size = std::tuple_size_v<tuple_t>;

  using elem_t = std::tuple_element_t<tuple_index, tuple_t>;
  using filter_t = util::copy_types<tuple_t, __filter_t>;
public:
  template<std::size_t table_offset>
  static constexpr auto init() {
    auto constexpr sql_begin = __elem_funcs<elem_t>::template table<table_offset>();

    if constexpr (!(tuple_index < tuple_size -1)) {
      return sql_begin + __elem_funcs<elem_t>::template constraint<table_offset>();
    }
    else {
      constexpr auto table_offset_new = util::contains_instantiation_of_v<range_t, elem_t> ? table_offset +1 : table_offset;

      constexpr auto sql_middle = sql_begin + "," +
                                  __SQL<tuple_t, tuple_index + 1>::template init<table_offset_new>() +
                                  __elem_funcs<elem_t>::template constraint<table_offset>();
      if constexpr (tuple_index == 0) {
        return sql_middle + ");";
      } else {
        return sql_middle;
      }
    }
  }

  static void bind(int bound, sStatement &statement, const tuple_t &tuple) {
    int bound_new = util::contains_instantiation_of_v<range_t, elem_t> ? bound +2 : bound +1;

    __elem_funcs<elem_t>::bind(bound, statement, std::get<tuple_index>(tuple));

    __SQL<tuple_t, tuple_index +1>::bind(bound_new, statement, tuple);
  }

  template<class ...Columns>
  static tuple_t row(int table_offset, sStatement &statement, Columns &&... columns) {
    int table_offset_new = util::contains_instantiation_of_v<range_t, elem_t> ? table_offset +1 : table_offset;

    return __SQL<tuple_t, tuple_index +1>::row(table_offset_new, statement, std::forward<Columns>(columns)...,
                                       __elem_funcs<elem_t>::get(tuple_index + table_offset, statement));
  }

  template<std::size_t table_offset>
  static std::string filter(const filter_t &filter, bool empty = true) {
    auto &optional_value = std::get<tuple_index>(filter.tuple);

    constexpr auto has_range = util::contains_instantiation_of_v<range_t, elem_t>;
    std::string sql;
    if(optional_value) {
      if constexpr (has_range) {
        if(empty) {
          sql = ("((? BETWEEN " +
            literal::string_literal_quote_t<tuple_index + table_offset>::to_string() + " AND " +
            literal::string_literal_quote_t<tuple_index + table_offset +1>::to_string() + ") ").native();
        }
        else {
          sql = ("AND (BETWEEN " +
                literal::string_literal_quote_t<tuple_index + table_offset>::to_string() + " AND " +
                literal::string_literal_quote_t<tuple_index + table_offset +1>::to_string() + ")) ").native();
        }
      }
      else {
        if(empty) {
          sql = "(" + literal::string_literal_quote_t<tuple_index + table_offset>::to_string().native() + " " +
                comp_to_string[optional_value->first] + " ? ";
        } else {
          sql = "AND " + literal::string_literal_quote_t<tuple_index + table_offset>::to_string().native() + " " +
                comp_to_string[optional_value->first] + " ? ";
        }
      }
    }

    return sql + __SQL<tuple_t, tuple_index +1>::template
      filter<has_range ? table_offset +1 : table_offset>(filter, optional_value ? false : empty);
  }

  static int optional_bind(int bound, sStatement &statement, const filter_t &filter) {
    auto &optional_value = std::get<tuple_index>(filter.tuple);

    if(optional_value) {
      if constexpr (util::contains_instantiation_of_v<range_t, elem_t>) {
        __elem_funcs<std::decay_t<decltype(*optional_value)>>::bind(bound, statement, *optional_value);
      }
      else {
        __elem_funcs<decltype(optional_value->second)>::bind(bound, statement, optional_value->second);
      }

      return __SQL<tuple_t, tuple_index +1>::optional_bind(bound + 1, statement, filter);
    } else {
      return __SQL<tuple_t, tuple_index +1>::optional_bind(bound, statement, filter);
    }
  }

  /* very */ private:
  template<class T, class S = void>
  struct __elem_funcs {
    typedef decltype(*std::begin(std::declval<T>())) raw_type;
    typedef std::decay_t<raw_type> base_type;

    template<std::size_t table_offset>
    static constexpr auto table() {
      return literal::string_literal_quote_t<tuple_index + table_offset>::to_string() + " " + type() + " NOT NULL";
    }


    static constexpr auto type() {
      return literal::string_t { std::is_same<char, base_type>::value ? "TEXT" : "BLOB" };
    }

    template<std::size_t table_offset>
    static constexpr auto constraint() {
      return literal::string_t {};
    }

    static void deleter(void *ptr) {
      delete[] reinterpret_cast<base_type *>(ptr);
    }

    static void bind(int element, sStatement &statement, const T &value) {
      auto begin = std::begin(value);
      auto end = std::end(value);

      int distance = (int) std::distance(begin, end);
      auto buf = std::unique_ptr<base_type[]>(new base_type[distance + 1]);

      std::copy(begin, end, buf.get());

      // reinterpret_cast is used to ensure it compiles.
      if(std::is_same<char, base_type>::value) {
        sqlite3_bind_text(statement.get(), element +1, reinterpret_cast<const char *>(buf.release()), distance,
                          deleter);
      } else {
        sqlite3_bind_blob(statement.get(), element +1, reinterpret_cast<const void *>(buf.release()), distance,
                          deleter);
      }
    }

    static T get(int table_offset, sStatement &statement) {
      const base_type *begin;
      if constexpr (std::is_same_v<base_type, char>) {
        begin = reinterpret_cast<const base_type *>(sqlite3_column_text(statement.get(), table_offset));
      }
      else {
        begin = reinterpret_cast<const base_type *>(sqlite3_column_blob(statement.get(), table_offset));
      }

      const base_type *end = begin + sqlite3_column_bytes(statement.get(), table_offset);

      return { begin, end };
    }
  };

  template<class T>
  struct __elem_funcs<T, std::enable_if_t<std::is_integral<T>::value>> {
    template<std::size_t table_offset>
    static constexpr auto table() {
      return literal::string_literal_quote_t<tuple_index + table_offset>::to_string() + " " + type() + " NOT NULL";
    }

    static constexpr auto  type() {
      return literal::string_t { "INTEGER" };
    }

    template<std::size_t table_offset>
    static constexpr auto constraint() {
      return literal::string_t {};
    }

    static void bind(int element, sStatement &statement, const T &value) {
      sqlite3_bind_int64(statement.get(), element +1, value);
    }

    static T get(int table_offset, sStatement &statement) {
      return (T) sqlite3_column_int64(statement.get(), table_offset);
    }
  };

  template<class T>
  struct __elem_funcs<T, std::enable_if_t<std::is_floating_point<T>::value>> {
    template<std::size_t table_offset>
    static constexpr auto table() {
      return literal::string_literal_quote_t<tuple_index + table_offset>::to_string() + " " + type() + " NOT NULL";
    }

    static constexpr auto type() {
      return literal::string_t { "REAL" };
    }

    template<std::size_t table_offset>
    static constexpr auto constraint() {
      return literal::string_t {};
    }

    static void bind(int element, sStatement &statement, const T &value) {
      sqlite3_bind_double(statement.get(), element +1, value);
    }

    static T get(int table_offset, sStatement &statement) {
      return sqlite3_column_double(statement.get(), table_offset);
    }
  };

  template<class T>
  struct __elem_funcs<T, std::enable_if_t<std::is_pointer<T>::value>> {
    static_assert(!std::is_pointer_v<T>, "Pointers are not allowed!");
  };

  template<class T>
  struct __elem_funcs<T, std::enable_if_t<util::instantiation_of_v<std::optional, T>>> {
    typedef typename T::value_type elem_t;

    static_assert(!util::contains_instantiation_of_v<std::optional, elem_t>,
                  "std::optional<std::optional<T>> detected.");

    template<std::size_t table_offset>
    static constexpr auto table() {
      if constexpr (util::contains_instantiation_of_v<range_t, T>) {
        return
          literal::string_literal_quote_t<tuple_index + table_offset>::to_string()    + " " + type() + "," +
          literal::string_literal_quote_t<tuple_index + table_offset +1>::to_string() + " " + type();
      }
      else {
        return literal::string_literal_quote_t<tuple_index + table_offset>::to_string() + " " + type();
      }
    }

    static constexpr auto type() {
      return __elem_funcs<elem_t>::type();
    }

    template<std::size_t table_offset>
    static constexpr auto constraint() {
      return __elem_funcs<elem_t>::template constraint<table_offset>();
    }

    static void bind(int element, sStatement &statement, const T &value) {
      if(value) {
        __elem_funcs<elem_t>::bind(element, statement, *value);
      } else {
        sqlite3_bind_null(statement.get(), element +1);
        if constexpr (util::contains_instantiation_of_v<range_t, elem_t>) {
          sqlite3_bind_null(statement.get(), element +2);
        }
      }
    }

    static T get(int table_offset, sStatement &statement) {
      if(sqlite3_column_type(statement.get(), tuple_index + table_offset) == SQLITE_NULL) {
        return std::nullopt;
      }

      return __elem_funcs<elem_t>::get(table_offset, statement);
    }
  };

  template<class T>
  struct __elem_funcs<T, std::enable_if_t<util::instantiation_of_v<range_t, T>>> {
    using elem_t = typename T::value_type;

    static_assert(!util::contains_instantiation_of_v<std::optional, elem_t>,
                  "std::optional should be the top constraint.");
    static_assert(!util::contains_instantiation_of_v<unique_t, elem_t>, "unique_t should be the top constraint.");

    static_assert(!util::contains_instantiation_of_v<range_t, elem_t>,
                  "range_t<range_t<T>> detected");

    template<std::size_t table_offset>
    static constexpr auto table() {
      return
        literal::string_literal_quote_t<tuple_index + table_offset>::to_string() + " "    + type() + " NOT NULL," +
        literal::string_literal_quote_t<tuple_index + table_offset +1>::to_string() + " " + type() + " NOT NULL";
    }

    static constexpr auto type() {
      return __elem_funcs<elem_t>::type();
    }

    template<std::size_t table_offset>
    static constexpr auto constraint() {
      return literal::string_t {};
    }

    static void bind(int element, sStatement &statement, const T &value) {
      __elem_funcs<elem_t>::bind(element, statement, value.lower);
      __elem_funcs<elem_t>::bind(element +1, statement, value.upper);
    }

    static T get(int table_offset, sStatement &statement) {
      return {
        __elem_funcs<elem_t>::get(table_offset, statement),
        __elem_funcs<elem_t>::get(table_offset +1, statement),
      };
    }
  };

  template<class T>
  struct __elem_funcs<T, std::enable_if_t<util::instantiation_of_v<unique_t, T>>> {
    typedef typename T::elem_t elem_t;

    static_assert(!util::contains_instantiation_of_v<std::optional, elem_t>,
                  "std::optional should be the top constraint.");
    static_assert(!util::contains_instantiation_of_v<unique_t, elem_t>, "unique_t<unique_t<T>> detected.");

    template<std::size_t table_offset>
    static constexpr auto table() {
      if constexpr (util::contains_instantiation_of_v<range_t, T>) {
        return
          literal::string_literal_quote_t<tuple_index + table_offset>::to_string()    + " " + type() + "," +
          literal::string_literal_quote_t<tuple_index + table_offset +1>::to_string() + " " + type();
      }
      else {
        return literal::string_literal_quote_t<tuple_index + table_offset>::to_string() + " " + type();
      }
    }

    static constexpr auto type() {
      return __elem_funcs<elem_t>::type();
    }

    template<std::size_t table_offset>
    static constexpr auto constraint() {
      if constexpr (util::contains_instantiation_of_v<range_t, T>) {
        return ",UNIQUE (" + literal::string_literal_quote_t<tuple_index + table_offset>::to_string() + "," +
          literal::string_literal_quote_t<tuple_index + table_offset +1>::to_string() + ")";
      }
      else {
        return ",UNIQUE (" + literal::string_literal_quote_t<tuple_index + table_offset>::to_string() + ")";
      }
    }

    static void bind(int element, sStatement &statement, const T &value) {
      __elem_funcs<elem_t>::bind(element, statement, value);
    }

    static T get(int table_offset, sStatement &statement) {
      return __elem_funcs<elem_t>::get(table_offset, statement);
    }
  };
};

template<class tuple_t, std::size_t tuple_index>
class __SQL<tuple_t, tuple_index, std::enable_if_t<(tuple_index == std::tuple_size<tuple_t>::value)>> {
public:
  using filter_t = util::copy_types<tuple_t, __filter_t>;

  static constexpr auto init() { return literal::string_t {}; }

  static void bind(int, sStatement&, const tuple_t &) { }

  template<class ...Columns>
  static tuple_t row(int, sStatement&, Columns &&... columns) {
    return std::make_tuple(std::forward<Columns>(columns)...);
  }

  template<std::size_t>
  static std::string filter(const filter_t &filter, bool empty = true) { return ")"; }

  // Return the amount of values bound
  static int optional_bind(int bound, sStatement &statement, const filter_t &filter) { return bound; }
};

// Fowler–Noll–Vo hash function
constexpr std::size_t hash_mul = sizeof(std::size_t) == 4 ? 16777619 : 1099511628211;

template<std::size_t hash, class T>
struct apply_t;

template<std::size_t hash>
struct apply_t<hash, literal::literal_t<>> {
  static constexpr std::size_t value = hash;
};

template<std::size_t hash, char... Args>
struct apply_t<hash, literal::literal_t<Args...>> {
  static constexpr std::size_t value = apply_t<(hash * hash_mul) ^ literal::literal_t<Args...>::value[0], literal::tail_t<literal::literal_t<Args...>>>::value;
};

template<std::size_t hash, class T>
struct hash_t;

template<std::size_t hash>
struct hash_t<hash, std::tuple<>> {
  static constexpr std::size_t value = hash;
};

template<std::size_t hash, class...Args>
struct hash_t<hash, std::tuple<Args...>> {
  static constexpr std::size_t value = hash_t<
    apply_t<hash, type_value_t<typename util::__head_types<std::tuple<Args...>>::type>>::value,
    typename util::__tail_types<std::tuple<Args...>>::type
  >::value;
};

template<class T>
static constexpr std::size_t hash_v = hash_t<14695981039346656037ul, T>::value;

template<class T>
struct __CountPred {
  static constexpr bool value = util::contains_instantiation_of_v<range_t, T>;
};

template<class ...Args>
class Model {
public:
  using tuple_t = std::tuple<Args...>;
  using filter_t = __filter_t<Args...>;

  static constexpr std::size_t tuple_size = std::tuple_size<tuple_t>::value;
private:
  static constexpr auto _range_count = util::count_if_v<tuple_t, __CountPred>;

  using _index_t = literal::index_sequence_t<tuple_size + _range_count -1>;
  using _table = literal::string_literal_quote_t<hash_v<tuple_t>>;

  sStatement _transaction_start;
  sStatement _transaction_end;

  sStatement _insert;
  sStatement _insert_with_id;
  sStatement _update;
  sStatement _delete;
public:
  Model(sqlite3 *db) {
    char *buf { nullptr };
    sqlite3_exec(db, sql_init.begin(), nullptr, nullptr, &buf);
    if(buf) {
      err::set(buf);

      sqlite3_free(buf);

      return;
    }

    _transaction_start = sStatement(mk_statement(db, "BEGIN TRANSACTION;"));
    _transaction_end   = sStatement(mk_statement(db, "COMMIT;"));

    _insert         = sStatement(mk_statement(db, sql_insert.begin()));
    _insert_with_id = sStatement(mk_statement(db, sql_insert_with_id.begin()));
    _update         = sStatement(mk_statement(db, sql_update.begin()));
    _delete         = sStatement(mk_statement(db, sql_delete.begin()));
  }


  auto transaction() {
    auto func = [this]() {
      sqlite3_reset(_transaction_end.get());
      auto err = sqlite3_step(_transaction_end.get());

      if(err == SQLITE_ERROR || err == SQLITE_MISUSE || err == SQLITE_CONSTRAINT) {
        err::set(sqlite3_errmsg(sqlite3_db_handle(_transaction_end.get())));

        return;
      }
    };

    using fg_t = decltype(util::fail_guard(std::move(func)));

    sqlite3_reset(_transaction_start.get());

    auto err = sqlite3_step(_transaction_start.get());

    if(err == SQLITE_ERROR || err == SQLITE_MISUSE || err == SQLITE_CONSTRAINT) {
      err::set(sqlite3_errmsg(sqlite3_db_handle(_transaction_start.get())));

      return std::optional<fg_t> { std::nullopt };
    }

    return std::optional<fg_t> { util::fail_guard(std::move(func)) };
  }

  std::optional<model_t<tuple_t>> build(tuple_t &&tuple) {
    if(_input(_insert, tuple, 0)) {
      return std::nullopt;
    }

    sqlite3_int64 id = sqlite3_last_insert_rowid(sqlite3_db_handle(_insert.get()));

    return std::tuple_cat(std::move(tuple), std::make_tuple(id));
  }

  std::optional<model_t<tuple_t>> build_with_id(model_t<tuple_t> &&tuple) {
    if(_input(_insert_with_id, util::init(std::move(tuple)), std::get<std::tuple_size_v<tuple_t>>(tuple))) {
      return std::nullopt;
    }

    return std::move(tuple);
  }

  int save(const model_t<tuple_t> &tuple) {
    if(_input(_update, tuple)) {
      return -1;
    }
    return 0;
  }

  template<class Callable>
  int load(Callable &&f) {
    auto statement = mk_statement(sqlite3_db_handle(_insert.get()), (sql_select + ";").begin());
    return _output(statement, std::forward<Callable>(f));
  }

  template<class Callable>
  int load(const std::vector<filter_t> &filters, Callable &&f) {
    auto statement = mk_statement(sqlite3_db_handle(_insert.get()), (_sql_select().begin() + sql_where(filters)).c_str());
    return _output(statement, filters, std::forward<Callable>(f));
  }

  template<class Callable>
  int load(sqlite3_int64 id, Callable &&f) {
    auto statement = mk_statement(sqlite3_db_handle(_insert.get()), (_sql_select() + " WHERE (ID=?);").begin());

    sqlite3_bind_int64(statement.get(), 1, id);

    return _exec(statement, std::forward<Callable>(f));
  }

  int delete_(const model_t<tuple_t> &tuple) {
    return _input(_delete, std::make_tuple(std::get<tuple_size>(tuple)));
  }

  template<class T>
  int _input(sStatement &statement, const T &tuple, sqlite3_int64 custom_id) {
    sqlite3_reset(statement.get());
    sqlite3_clear_bindings(statement.get());

    if(custom_id) {
      sqlite3_bind_int64(statement.get(), (int)std::tuple_size_v<tuple_t> +1, custom_id);
    }

    __SQL<tuple_t, 0>::bind(0, statement, tuple);

    while(true) {
      auto err = sqlite3_step(statement.get());

      if(err == SQLITE_ERROR || err == SQLITE_MISUSE || err == SQLITE_CONSTRAINT) {
        err::set(sqlite3_errmsg(sqlite3_db_handle(statement.get())));

        return -1;
      }

      if(err != SQLITE_ROW) {
        return 0;
      }
    }
  }

  template<class Callable>
  int _output(sStatement &statement, Callable &&f) {
    sqlite3_reset(statement.get());
    sqlite3_clear_bindings(statement.get());

    return _exec(statement, std::forward<Callable>(f));
  }

  template<class Callable>
  int _output(sStatement &statement, const std::vector<filter_t> &filters, Callable &&f) {
    int bound = 0;
    for(auto &filter : filters) {
      bound = __SQL<tuple_t, 0>::optional_bind(bound, statement, filter);
    }

    return _exec(statement, std::forward<Callable>(f));
  }

  template<class Callable>
  int _exec(sStatement &statement, Callable &&f) {
    static_assert(std::is_same_v<err::code_t, std::decay_t<decltype(f(__SQL<tuple_t, 0>::row(0, statement)))>>, "The function must return an err::code_t");

    while(true) {
      auto err = sqlite3_step(statement.get());

      if(err == SQLITE_ERROR || err == SQLITE_MISUSE) {
        err::set(sqlite3_errmsg(sqlite3_db_handle(statement.get())));

        return -1;
      }

      if(err != SQLITE_ROW) {
        return 0;
      }

      auto return_val = f(__SQL<model_t<tuple_t>, 0>::row(0, statement));

      if(return_val != err::OK) {
        return return_val == err::BREAK ? 0 : -1;
      }
    }
  }

  constexpr static auto _sql_init() {
    return "CREATE TABLE IF NOT EXISTS " + _table::to_string() + " (ID INTEGER PRIMARY KEY," + __SQL<tuple_t, 0>::template init<0>();
  }

  template<bool custom_id>
  auto static constexpr _sql_insert() {
    using last_literal_t = literal::string_literal_quote_t<tuple_size + _range_count -1>;

    constexpr literal::string_t sql_begin  = "INSERT INTO " + _table::to_string() + " (" + literal::fold_t<_index_t, fold_columns_name, last_literal_t>::to_string();
    constexpr literal::string_t sql_middle = literal::fold_t<_index_t, fold_columns_bind, literal::literal_t<>>::to_string();

    if constexpr (custom_id) {
      return sql_begin + ",ID) VALUES (" + sql_middle + "?,?);";
    }
    else {
      return sql_begin + ") VALUES (" + sql_middle + "?);";
    }
  }

  static constexpr auto _sql_update() {
    using last_literal_t = literal::concat_t<literal::string_literal_quote_t<tuple_size + _range_count -1>, literal::literal_t<'=','?'>>;

    return "UPDATE " + _table::to_string() + " SET " +
      literal::fold_t<_index_t, fold_columns_update, last_literal_t>::to_string() +
      " WHERE ID=?;";
  }

  static constexpr auto _sql_delete() {
    return "DELETE FROM " + _table::to_string() + " WHERE ID=?;";
  }

  static constexpr auto _sql_select() {
    using last_literal_t = literal::string_literal_quote_t<tuple_size + _range_count -1>;

    return
      "SELECT " +
      literal::fold_t<_index_t, fold_columns_name, last_literal_t>::to_string() +
      ",ID FROM " + _table::to_string();
  }
public:
  static std::string sql_where(const std::vector<filter_t> &filters) {
    if(filters.empty()) {
      return std::string {};
    }

    const filter_t &first = filters[0];

    std::string sql = " WHERE " + __SQL<tuple_t, 0>::template filter<0>(first);

    std::for_each(std::begin(filters) + 1, std::end(filters), [&](const filter_t &filter) {
      sql.append(" OR " + __SQL<tuple_t, 0>::template filter<0>(filter));
    });

    return sql + ';';
  }

  static constexpr literal::string_t sql_init           = _sql_init();
  static constexpr literal::string_t sql_select         = _sql_select();
  static constexpr literal::string_t sql_insert         = _sql_insert<false>();
  static constexpr literal::string_t sql_insert_with_id = _sql_insert<true>();
  static constexpr literal::string_t sql_update         = _sql_update();
  static constexpr literal::string_t sql_delete         = _sql_delete();
};

template<class Tuple>
using model_controller_t = util::copy_types<Tuple, Model>;

class Database {
  sDatabase _db;
public:

  Database(const char *dbName);
  Database(const std::string &dbName);

  template<class Tuple, class ...Args>
  auto mk_model() {
    return model_controller_t<Tuple>(_db.get());
  }
};
}
