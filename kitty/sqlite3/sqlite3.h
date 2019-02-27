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

template<class ...Args>
class Filter {
public:
  typedef std::tuple<std::optional<std::pair<comp_t, Args>>...> tuple_optional;

  Filter() = default;
  Filter(Filter &&) noexcept = default;

  tuple_optional tuple;

  template<std::size_t elem, class ...ConstructArgs>
  Filter &set(comp_t comp, ConstructArgs &&... args) {
    // Get the undelying element type from the optional
    typedef typename std::tuple_element<elem, tuple_optional>::type::value_type elem_t;

    std::get<elem>(tuple) = elem_t { comp, { std::forward<ConstructArgs>(args)... }};

    return *this;
  }
};

// Add room for id
template<class Tuple>
using model_t = typename util::append_types<Tuple, sqlite3_int64>::type;

// Type aliasing without make std::is_same<T, Unique<T>> true
template<class T, class V = void>
class Unique : public T {
public:
  typedef T elem_t;
  using T::T;

  template<class ...Args>
  Unique(Args &&... args) : T { std::forward<Args>(args)... } { }

  Unique(const Unique &) = default;

  Unique(Unique &&) noexcept = default;

  Unique &operator=(const Unique &) = default;

  Unique &operator=(Unique &&) noexcept = default;

  Unique &operator=(const T &other) {
    _this() = other;

    return *this;
  }

  Unique &operator=(T &&other) {
    _this() = std::move(other);

    return *this;
  }

private:
  T &_this() { return *reinterpret_cast<T *>(this); }
};

// Type aliasing without make std::is_same<T, Unique<T>> true
template<class T>
class Unique<T, std::enable_if_t<!std::is_class<T>::value>> {
  T _t;
public:
  typedef T elem_t;

  Unique() = default;

  Unique(const Unique &) = default;

  Unique(Unique &&) noexcept = default;

  Unique &operator=(const Unique &) = default;

  Unique &operator=(Unique &&) noexcept = default;

  Unique(const T &other) : _t { other } { }

  Unique(T &&other) : _t { std::move(other) } { }

  operator elem_t &() { return _t; }

  operator const elem_t &() const { return _t; }
};

template<class T>
using optional_unique_t = std::optional<Unique<T>>;

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
struct type_value<Unique<T>> {
  using type = literal::concat_t<typename type_value<T>::type, literal::literal_t<'U', 'n', 'i', 'q', 'u', 'e'>>;
};

template<class T>
struct type_value<std::optional<T>> {
  using type = literal::concat_t<typename type_value<T>::type, literal::literal_t<'o', 'p', 't', 'i', 'o', 'n', 'a', 'l'>>;
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

template<class tuple_t, std::size_t offset, class _ = void>
class __SQL {
  static constexpr std::size_t tuple_size = std::tuple_size_v<tuple_t>;

  using elem_t = std::tuple_element_t<offset, tuple_t>;
  using filter_t = typename util::copy_types<tuple_t, Filter>::type;
public:
  static constexpr auto init() {
    auto constexpr sql_begin =
      literal::string_literal_quote_t<offset>::to_string() + " " +
      __elem_funcs<elem_t>::not_null();

    if constexpr (!(offset < tuple_size -1)) {
      return sql_begin + __elem_funcs<elem_t>::constraint();
    }
    else {
      constexpr auto sql_middle = sql_begin + "," +
                                  __SQL<tuple_t, offset + 1>::init() +
                                  __elem_funcs<elem_t>::constraint();
      if constexpr (offset == 0) {
        return sql_middle + ");";
      } else {
        return sql_middle;
      }
    }
  }

  static void bind(sStatement &statement, const tuple_t &tuple) {
    __elem_funcs<elem_t>::bind(statement, std::get<offset>(tuple));

    __SQL<tuple_t, offset + 1>::bind(statement, tuple);
  }

  template<class ...Columns>
  static tuple_t row(sStatement &statement, Columns &&... columns) {
    return __SQL<tuple_t, offset +1>::row(statement, std::forward<Columns>(columns)...,
                                       __elem_funcs<elem_t>::get(statement));
  }

  static std::string filter(const filter_t &filter, bool empty = true) {
    auto &optional_value = std::get<offset>(filter.tuple);

    std::string sql;
    if(optional_value) {
      if(empty) {
        sql = "(" + literal::string_literal_quote_t<offset>::to_string().native() + " " + comp_to_string[optional_value->first] + " ? ";
      } else {
        sql = "AND " + literal::string_literal_quote_t<offset>::to_string().native() + " " + comp_to_string[optional_value->first] + " ? ";
      }
    }

    return sql + __SQL<tuple_t, offset +1>::filter(filter, optional_value ? false : empty);
  }

  static int optional_bind(int bound, sStatement &statement, const filter_t &filter) {
    auto &optional_value = std::get<offset>(filter.tuple);

    if(optional_value) {
      _optional_bind_helper(bound, statement, optional_value->second);

      return __SQL<tuple_t, offset +1>::optional_bind(bound + 1, statement, filter);
    } else {
      return __SQL<tuple_t, offset +1>::optional_bind(bound, statement, filter);
    }
  }

  /* very */ private:
  template<class T>
  static void _optional_bind_helper(int bound, sStatement &statement, const T &value) {
    __elem_funcs<T>::bind(statement, value, bound);
  }

  template<class T, class S = void>
  struct __elem_funcs {
    typedef decltype(*std::begin(std::declval<T>())) raw_type;
    typedef std::decay_t<raw_type> base_type;

    static constexpr auto not_null() {
      return type() + " NOT NULL";
    }


    static constexpr auto type() {
      return literal::string_t { std::is_same<char, base_type>::value ? "TEXT" : "BLOB" };
    }

    static constexpr auto constraint() {
      return literal::string_t {};
    }

    static void deleter(void *ptr) {
      delete[] reinterpret_cast<base_type *>(ptr);
    }

    static void bind(sStatement &statement, const T &value, int element = offset) {
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

    static T get(sStatement &statement) {
      const base_type *begin;
      if constexpr (std::is_same_v<base_type, char>) {
        begin = reinterpret_cast<const base_type *>(sqlite3_column_text(statement.get(), offset));
      }
      else {
        begin = reinterpret_cast<const base_type *>(sqlite3_column_blob(statement.get(), offset));
      }

      const base_type *end = begin + sqlite3_column_bytes(statement.get(), offset);

      return { begin, end };
    }
  };

  template<class T>
  struct __elem_funcs<T, std::enable_if_t<std::is_integral<T>::value>> {
    static constexpr auto not_null() {
      return type() + " NOT NULL";
    }

    static constexpr auto  type() {
      return literal::string_t { "INTEGER" };
    }

    static constexpr auto constraint() {
      return literal::string_t {};
    }

    static void bind(sStatement &statement, const T &value, int element = offset) {
      sqlite3_bind_int64(statement.get(), element + 1, value);
    }

    static T get(sStatement &statement) {
      return (T) sqlite3_column_int64(statement.get(), offset);
    }
  };

  template<class T>
  struct __elem_funcs<T, std::enable_if_t<std::is_floating_point<T>::value>> {
    static constexpr auto not_null() {
      return literal::string_t { "REAL NOT NULL" };
    }

    static constexpr auto type() {
      return literal::string_t { "REAL" };
    }

    static constexpr auto constraint() {
      return literal::string_t {};
    }

    static void bind(sStatement &statement, const T &value, int element = offset) {
      sqlite3_bind_double(statement.get(), element + 1, value);
    }

    static T get(sStatement &statement) {
      return sqlite3_column_double(statement.get(), offset);
    }
  };

  template<class T>
  struct __elem_funcs<T, std::enable_if_t<std::is_pointer<T>::value>> {
    static_assert(!std::is_pointer_v<T>, "Pointers are not allowed!");
  };

  template<class T>
  struct __elem_funcs<T, std::enable_if_t<util::instantiation_of<std::optional, T>::value>> {
    typedef typename T::value_type elem_t;

    static_assert(!util::contains_instantiation_of<std::optional, elem_t>::value,
                  "std::optional<std::optional<T>> detected.");

    static constexpr auto not_null() {
      return __elem_funcs<elem_t>::type();
    }

    static constexpr auto constraint() {
      return __elem_funcs<elem_t>::constraint();
    }

    static void bind(sStatement &statement, const T &value, int element = offset) {
      if(value) {
        __elem_funcs<elem_t>::bind(statement, *value, element);
      } else {
        sqlite3_bind_null(statement.get(), element + 1);
      }
    }

    static T get(sStatement &statement) {
      auto p = sqlite3_column_type(statement.get(), offset);
      if(p == SQLITE_NULL) {
        return std::nullopt;
      }

      return __elem_funcs<elem_t>::get(statement);
    }
  };

  template<class T>
  struct __elem_funcs<T, std::enable_if_t<util::instantiation_of<Unique, T>::value>> {
    typedef typename T::elem_t elem_t;

    static_assert(!util::contains_instantiation_of<std::optional, elem_t>::value,
                  "std::optional should be the top constraint.");
    static_assert(!util::contains_instantiation_of<Unique, elem_t>::value, "Unique<Unique<T>> detected.");

    static constexpr auto not_null() {
      return __elem_funcs<elem_t>::not_null();
    }

    static constexpr auto type() {
      return __elem_funcs<elem_t>::type();
    }

    static constexpr auto constraint() {
      return __elem_funcs<elem_t>::constraint() + ",UNIQUE (" + literal::string_literal_quote_t<offset>::to_string() + ")";
    }

    static void bind(sStatement &statement, const T &value, int element = offset) {
      __elem_funcs<elem_t>::bind(statement, value, element);
    }

    static T get(sStatement &statement) {
      return __elem_funcs<elem_t>::get(statement);
    }
  };
};

template<class tuple_t, int offset>
class __SQL<tuple_t, offset, std::enable_if_t<(offset == std::tuple_size<tuple_t>::value)>> {
public:
  using filter_t = typename util::copy_types<tuple_t, Filter>::type;

  static constexpr auto init() { return literal::string_t {}; }

  static void bind(sStatement &statement, const tuple_t &tuple) { }

  template<class ...Columns>
  static tuple_t row(sStatement &statement, Columns &&... columns) {
    return std::make_tuple(std::forward<Columns>(columns)...);
  }

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
    apply_t<hash, type_value_t<typename util::head_types<std::tuple<Args...>>::type>>::value,
    typename util::tail_types<std::tuple<Args...>>::type
  >::value;
};

template<class T>
static constexpr std::size_t hash_v = hash_t<14695981039346656037ul, T>::value;

template<class ...Args>
class Model {
public:
  typedef std::tuple<Args...> tuple_t;
  typedef Filter<Args...> filter_t;

  static constexpr std::size_t tuple_size = std::tuple_size<tuple_t>::value;
private:
  using _index_t = literal::index_sequence_t<tuple_size -1>;

  static constexpr auto _table = literal::string_literal_quote_t<hash_v<tuple_t>>::to_string();

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
    auto statement = mk_statement(sqlite3_db_handle(_insert.get()), (_sql_select().begin() + _sql_where(filters)).c_str());
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

  filter_t mk_filter() { return filter_t {}; }

  template<class T>
  int _input(sStatement &statement, const T &tuple, sqlite3_int64 custom_id) {
    sqlite3_reset(statement.get());
    sqlite3_clear_bindings(statement.get());

    if(custom_id) {
      sqlite3_bind_int64(statement.get(), (int)std::tuple_size_v<tuple_t> +1, custom_id);
    }

    __SQL<tuple_t, 0>::bind(statement, tuple);

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
    static_assert(std::is_same_v<err::code_t, std::decay_t<decltype(f(__SQL<tuple_t, 0>::row(statement)))>>, "The function must return an err::code_t");

    while(true) {
      auto err = sqlite3_step(statement.get());

      if(err == SQLITE_ERROR || err == SQLITE_MISUSE) {
        err::set(sqlite3_errmsg(sqlite3_db_handle(statement.get())));

        return -1;
      }

      if(err != SQLITE_ROW) {
        return 0;
      }

      auto return_val = f(__SQL<model_t<tuple_t>, 0>::row(statement));

      if(return_val != err::OK) {
        return return_val == err::BREAK ? 0 : -1;
      }
    }
  }

  constexpr static auto _sql_init() {
    return "CREATE TABLE IF NOT EXISTS " + _table + " (ID INTEGER PRIMARY KEY," + __SQL<tuple_t, 0>::init();
  }

  template<bool custom_id>
  auto static constexpr _sql_insert() {
    using last_literal_t = literal::string_literal_quote_t<tuple_size -1>;

    constexpr literal::string_t sql_begin  = "INSERT INTO " + _table + " (" + literal::fold_t<_index_t, fold_columns_name, last_literal_t>::to_string();
    constexpr literal::string_t sql_middle = literal::fold_t<_index_t, fold_columns_bind, literal::literal_t<>>::to_string();

    if constexpr (custom_id) {
      return sql_begin + ",ID) VALUES (" + sql_middle + "?,?);";
    }
    else {
      return sql_begin + ") VALUES (" + sql_middle + "?);";
    }
  }

  static constexpr auto _sql_update() {
    using last_literal_t = literal::concat_t<literal::string_literal_quote_t<tuple_size -1>, literal::literal_t<'=','?'>>;

    return "UPDATE " + _table + " SET " +
      literal::fold_t<_index_t, fold_columns_update, last_literal_t>::to_string() +
      " WHERE ID=?;";
  }

  static constexpr auto _sql_delete() {
    return "DELETE FROM " + _table + " WHERE ID=?;";
  }

  static constexpr auto _sql_select() {
    using last_literal_t = literal::string_literal_quote_t<tuple_size -1>;

    return
      "SELECT " +
      literal::fold_t<_index_t, fold_columns_name, last_literal_t>::to_string() +
      "ID FROM " + _table;
  }

  std::string _sql_where(const std::vector<filter_t> &filters) {
    if(filters.empty()) {
      return std::string {};
    }

    const filter_t &first = filters[0];

    std::string sql = " WHERE " + __SQL<tuple_t, 0>::filter(first);

    std::for_each(std::begin(filters) + 1, std::end(filters), [&](const filter_t &filter) {
      sql.append(" OR " + __SQL<tuple_t, 0>::filter(filter));
    });

    return sql;
  }
public:
  static constexpr literal::string_t sql_init           = _sql_init();
  static constexpr literal::string_t sql_select         = _sql_select();
  static constexpr literal::string_t sql_insert         = _sql_insert<false>();
  static constexpr literal::string_t sql_insert_with_id = _sql_insert<true>();
  static constexpr literal::string_t sql_update         = _sql_update();
  static constexpr literal::string_t sql_delete         = _sql_delete();
};

template<class Tuple>
using model_controller_t = typename util::copy_types<Tuple, Model>::type;

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
