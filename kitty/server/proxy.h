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

template<class Stream>
int load(file::FD<Stream> &fd) {
  if (fd.eof()) {
    err::code = err::FILE_CLOSED;
    return -1;
  }

  return 0;
}

template<class Stream, class T, class... Args>
int load(file::FD<Stream> &fd, T &param, Args&... args) {
  if constexpr (util::instantiation_of_v<std::tuple, T>) {
    auto err = std::apply([&fd](auto &&...args) {
      return load(fd, std::forward<decltype(args)>(args)...);
    }, param);

    if(err) {
      return -1;
    }

    return load(fd, args...);
  }
  else if constexpr (util::instantiation_of_v<std::optional, T>) {
    auto ch = fd.next();
    if(!ch) {
      return -1;
    }

    if(!*ch) {
      param = std::nullopt;
      return load(fd, args...);
    }
    else {
      return load(fd, *param, args...);
    }
  }
  else if constexpr (util::instantiation_of_v<std::vector, T>) {
    auto size_opt = util::endian::little(file::read_struct<std::uint16_t>(fd));

    if(!size_opt) {
      return -1;
    }

    param.reserve(*size_opt);
    for(uint16_t x = 0; x < size_opt; ++x) {
      typename T::value_type val;
      auto err = load(fd, val);

      if(err) {
        return err;
      }

      param.emplace_back(std::move(val));
    }

    return load(fd, args...);
  }
  else if constexpr (std::is_same_v<std::string, T>) {
    auto size_opt { util::endian::little(file::read_struct<std::uint16_t>(fd)) };

    if(!size_opt) {
      return -1;
    }

    auto str { file::read_string(fd, *size_opt) };
    if(!str) {
      return -1;
    }

    param = std::move(*str);

    return load(fd, args...);
  }
  else {
    auto struct_opt { util::endian::little(file::read_struct<T>(fd)) };

    if(!struct_opt) {
      return -1;
    }

    param = *struct_opt;

    return load(fd, args...);
  }
}

template<class Stream, class T, class... Args>
int _load_raw(file::FD<Stream> &fd, std::vector<std::uint8_t> &vec);

/*
 * load the raw data from a tuple
 */
template<class T, class... Args>
struct _load_raw_helper {
public:
  template<class Stream>
  static int run(file::FD<Stream> &fd, std::vector<std::uint8_t> &vec) {
    return _load_raw<Stream, T, Args...>(fd, vec);
  }
};

template<class Stream>
int _load_raw(file::FD<Stream> &fd, std::vector<std::uint8_t>&) {
  return load(fd);
}

template<class Stream, class T, class... Args>
int _load_raw(file::FD<Stream> &fd, std::vector<std::uint8_t> &vec) {
  std::size_t size;

  if constexpr (util::instantiation_of_v<std::tuple, T>) {
    auto err = util::copy_types<T, _load_raw_helper>::run(fd, vec);

    if(err) {
      return -1;
    }

    size = 0;
  }
  else if constexpr (util::instantiation_of_v<std::optional, T>) {
    auto ch = fd.next();
    if(!ch) {
      return -1;
    }

    vec.push_back(*ch);
    if(*ch) {
      if(_load_raw<Stream, typename T::value_type>(fd, vec)) {
        return -1;
      }
    }

    size = 0;
  }
  else if constexpr (util::instantiation_of_v<std::vector, T>) {
    auto size_opt = file::read_struct<std::uint16_t>(fd);

    if(!size_opt) {
      return -1;
    }

    util::append_struct(vec, *size_opt);

    for(auto x = 0; x < *size_opt; ++x) {
      if(_load_raw<Stream, typename T::value_type>(fd, vec)) {
        return -1;
      }
    }

    size = 0;
  }
  else if constexpr (std::is_same_v<std::string, T>) {
    auto size_opt = file::read_struct<std::uint16_t>(fd);

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
  }, size);

  if(err) {
    return -1;
  }

  return _load_raw<Stream, Args...>(fd, vec);
}

template<class Stream, class... Args>
std::optional<std::vector<uint8_t>> load_raw(file::FD<Stream> &fd) {
  std::vector<uint8_t> vec;

  auto err = _load_raw<Stream, Args...>(fd, vec);

  if(err) {
    return std::nullopt;
  }

  return std::move(vec);
}

template<class Stream>
int push(file::FD<Stream> &fd) {
  return fd.out();
}

template<class Stream, class T, class... Args>
int push(file::FD<Stream> &fd, const T &param, const Args&... args) {
  if constexpr (util::instantiation_of_v<std::tuple, T>) {
    auto err = std::apply([&](auto&&... args) {
      return push(fd, std::forward<decltype(args)>(args)...);
    }, param);

    if(err) {
      return -1;
    }

    return push(fd, args...);
  }
  else if constexpr (util::instantiation_of_v<file::raw_t, T>) {
    return push(fd.append(param.val), args...);
  }
  else if constexpr (util::instantiation_of_v<std::optional, T>) {
    if(param) {
      fd.append('\x01').append(*param);
    }
    else {
      fd.append('\x00');
    }

    return push(fd, args...);
  }
  else if constexpr (util::instantiation_of_v<std::vector, T>) {
    fd.append(file::raw(util::endian::little((std::uint16_t)param.size())));

    for(auto &el : param) {
      auto err = push(fd, el);
      if(err) {
        return err;
      }
    }

    return push(fd, args...);
  }
  else if constexpr (std::is_same_v<std::string, T> || std::is_same_v<std::string_view, T>) {
    fd.append(file::raw(util::endian::little((std::uint16_t)param.size()))).append(param);

    return push(fd, args...);
  }
  else {
    return push(fd.append(file::raw(util::endian::little(param))), args...);
  }
}

}
#endif
