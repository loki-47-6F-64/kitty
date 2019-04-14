#ifndef DOSSIER_PROXY_H
#define DOSSIER_PROXY_H

#include <vector>
#include <string>

#include <memory>

#include <kitty/err/err.h>
#include <kitty/file/file.h>
#include <kitty/util/utility.h>
#include <kitty/util/template_helper.h>

namespace server::proxy {
/*
 * All values in the request are send in a null-terminated byte string
 * On failure a non-zero value is returned and err_msg is set.
 */

template<class Stream, class... R, class... W>
int load(file::FD<Stream, std::tuple<R...>, std::tuple<W...>> &fd, R... read_args) {
  assert(fd.is_open());
  if(fd.eof()) {
    err::code = err::FILE_CLOSED;
    return -1;
  }

  return 0;
}

template<class Stream, class... R, class... W, class T, class... Args>
int load(file::FD<Stream, std::tuple<R...>, std::tuple<W...>> &fd, R... read_args, T &param, Args&... args) {
  if constexpr (util::instantiation_of_v<std::tuple, T>) {
    auto err = std::apply([&fd, &read_args...](auto &&...args) {
      return load(fd, std::forward<R...>(read_args)..., std::forward<decltype(args)>(args)...);
    }, param);

    if(err) {
      return -1;
    }

    return load(fd, std::forward<R...>(read_args)..., args...);
  }
  else if constexpr (util::instantiation_of_v<std::optional, T>) {
    auto ch = file::read_struct<char>(fd, std::forward<R>(read_args)...);
    if(!ch) {
      return -1;
    }

    if(!*ch) {
      param = std::nullopt;
      return load(fd, std::forward<R...>(read_args)..., args...);
    }
    else {
      return load(fd, std::forward<R...>(read_args)..., *param, args...);
    }
  }
  else if constexpr (util::instantiation_of_v<std::vector, T>) {
    auto size_opt = util::endian::little(file::read_struct<std::uint16_t>(fd, std::forward<R>(read_args)...));

    if(!size_opt) {
      return -1;
    }

    param.reserve(*size_opt);
    for(uint16_t x = 0; x < size_opt; ++x) {
      typename T::value_type val;
      auto err = load(fd, std::forward<R...>(read_args)..., val);

      if(err) {
        return err;
      }

      param.emplace_back(std::move(val));
    }

    return load(fd, std::forward<R...>(read_args)..., args...);
  }
  else if constexpr (std::is_same_v<std::string, T>) {
    auto size_opt { util::endian::little(file::read_struct<std::uint16_t>(fd, std::forward<R>(read_args)...)) };

    if(!size_opt) {
      return -1;
    }

    auto str { file::read_string(fd, std::forward<R>(read_args)..., *size_opt) };
    if(!str) {
      return -1;
    }

    param = std::move(*str);

    return load(fd, std::forward<R...>(read_args)..., args...);
  }
  else {
    auto struct_opt { util::endian::little(file::read_struct<T>(fd, std::forward<R>(read_args)...)) };

    if(!struct_opt) {
      return -1;
    }

    param = *struct_opt;

    return load(fd, std::forward<R...>(read_args)..., args...);
  }
}

template<class T, class... Args, class Stream, class... R, class... W>
int _load_raw(file::FD<Stream, std::tuple<R...>, std::tuple<W...>> &fd, R... read_args, std::vector<std::uint8_t> &vec);


/*
 * load the raw data from a tuple
 */
template<class... Args>
struct _load_raw_helper {
public:
  template<class Stream, class... R, class... W>
  static int run(file::FD<Stream, std::tuple<R...>, std::tuple<W...>> &fd, R... read_args, std::vector<std::uint8_t> &vec) {
    return _load_raw<Args...>(fd, std::forward<R>(read_args)..., vec);
  }
};

template<class Stream, class...R, class... W>
int _load_raw(file::FD<Stream, std::tuple<R...>, std::tuple<W...>> &fd, R... read_args, std::vector<std::uint8_t>&) {
  return load(fd, std::forward<R>(read_args)...);
}

template<class T, class... Args, class Stream, class...R, class... W>
int _load_raw(file::FD<Stream, std::tuple<R...>, std::tuple<W...>> &fd, R... read_args, std::vector<std::uint8_t> &vec) {
  std::size_t size;

  if constexpr (util::instantiation_of_v<std::tuple, T>) {
    auto err = util::copy_types<T, _load_raw_helper>::run(fd, std::forward<R>(read_args)..., vec);

    if(err) {
      return -1;
    }

    size = 0;
  }
  else if constexpr (util::instantiation_of_v<std::optional, T>) {
    auto ch = file::read_struct<char>(fd, std::forward<R>(read_args)...);
    if(!ch) {
      return -1;
    }

    vec.push_back(*ch);
    if(*ch) {
      if(_load_raw<typename T::value_type>(fd, std::forward<R>(read_args)..., vec)) {
        return -1;
      }
    }

    size = 0;
  }
  else if constexpr (util::instantiation_of_v<std::vector, T>) {
    auto size_opt = file::read_struct<std::uint16_t>(fd, std::forward<R>(read_args)...);

    if(!size_opt) {
      return -1;
    }

    util::append_struct(vec, *size_opt);

    auto temp_size = util::endian::little(*size_opt);
    for(auto x = 0; x < temp_size; ++x) {
      if(_load_raw<typename T::value_type>(fd, std::forward<R>(read_args)..., vec)) {
        return -1;
      }
    }

    size = 0;
  }
  else if constexpr (std::is_same_v<std::string, T>) {
    auto size_opt = file::read_struct<std::uint16_t>(fd, std::forward<R>(read_args)...);

    if(!size_opt) {
      return -1;
    }

    util::append_struct(vec, *size_opt);

    size = util::endian::little(*size_opt);
  }
  else {
    size = sizeof(T);
  }

  auto err = fd.eachByte([&vec](auto ch) {
    vec.push_back(ch);

    return err::OK;
  }, std::forward<R>(read_args)..., size);

  if(err) {
    return -1;
  }

  return _load_raw<Args...>(fd, std::forward<R>(read_args)..., vec);
}

template<class... Args, class Stream, class...R, class... W>
std::optional<std::vector<uint8_t>> load_raw(file::FD<Stream, std::tuple<R...>, std::tuple<W...>> &fd, R... read_args) {
  std::vector<uint8_t> vec;

  auto err = _load_raw<Args...>(fd, std::forward<R>(read_args)..., vec);
  if(err) {
    return std::nullopt;
  }

  return std::move(vec);
}

template<class Stream, class... R, class... W, class T>
void __push_parameter(file::FD<Stream, std::tuple<R...>, std::tuple<W...>> &fd, const T &param) {
  if constexpr (util::instantiation_of_v<std::tuple, T>) {
    std::apply([&fd](auto&&... args) {
      (__push_parameter(fd, std::forward<decltype(args)>(args)), ...);
    }, param);
  }
  else if constexpr (util::instantiation_of_v<file::raw_t, T>) {
    fd.append(param.val);
  }
  else if constexpr (util::instantiation_of_v<std::optional, T>) {
    if(param) {
      fd.append('\x01').append(*param);
    }
    else {
      fd.append('\x00');
    }
  }
  else if constexpr (util::instantiation_of_v<std::vector, T>) {
    fd.append(file::raw(util::endian::little((std::uint16_t)param.size())));

    for(auto &el : param) {
      __push_parameter(fd, el);
    }
  }
  else if constexpr (std::is_same_v<std::string, T> || std::is_same_v<std::string_view, T>) {
    fd.append(file::raw(util::endian::little((std::uint16_t)param.size()))).append(param);
  }
  else {
    fd.append(file::raw(util::endian::little(param)));
  }
}

template<class Stream, class... R, class... W, class... Args>
int push(file::FD<Stream, std::tuple<R...>, std::tuple<W...>> &fd, W... write_args, const Args&... args) {
  assert(fd.is_open());

  fd.write_clear();
  (__push_parameter(fd, args), ...);

  return fd.out(std::forward<W>(write_args)...);
}

}
#endif
