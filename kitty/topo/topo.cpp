//
// Created by loki on 19-2-19.
//

#include <cmath>
#include <kitty/file/io_stream.h>
#include <kitty/log/log.h>
#include <kitty/util/set.h>
#include <kitty/topo/topo.h>
#include "topo.h"


using namespace std::literals;

namespace topo {

constexpr auto MAX_INSERT = 2048;

#define DISABLE false
#if DISABLE
#define DISABLE_IF(x) (false && x)
#else
#define DISABLE_IF(x) (x)
#endif
using country_t    = std::tuple<std::string, std::string, std::optional<std::string>, std::optional<std::string>>;
using ip_block_t   = std::tuple<sqlite::range_t<std::uint32_t>, std::optional<std::int64_t>, std::optional<std::int64_t>, bool, bool>;
using coordinate_t = std::tuple<std::string, double, double>;

util::copy_types<country_t, sqlite::Model> country_m(sqlite::Database &database) {
  return database.mk_model<country_t>();
}

util::copy_types<ip_block_t, sqlite::Model> ip_block_m(sqlite::Database &database) {
  return database.mk_model<ip_block_t>();
}

util::copy_types<coordinate_t, sqlite::Model> coord_m(sqlite::Database &database) {
  return database.mk_model<coordinate_t>();
}

auto parse_line_country(const std::string &line) {
  auto values = util::split(line, ',');
  auto id_str = values[0];

  auto id = util::from_chars(id_str.data(), id_str.data() + id_str.size());

  return sqlite::model_t<country_t> {
    values[2],
    values[3],
    values[4].empty() ? std::nullopt : std::optional<std::string> { values[4] },
    values[5].empty() ? std::nullopt : std::optional<std::string> { values[5] },
    id
  };
}

auto parse_line_ip_block(const std::string &line) {
  auto values = util::split(line, ',');

  // network,geoname_id,registered_country_geoname_id,represented_country_geoname_id,is_anonymous_proxy,is_satellite_provider

  auto id_str_c = values[1];
  auto id_str_r = values[2];

  bool ano_proxy = values[3][0] == '1';
  bool satellite = values[4][0] == '1';

  auto _ip_block = ip_block(values[0]);

  return ip_block_t {
    { std::get<0>(_ip_block), std::get<1>(_ip_block) },
    id_str_c.empty() ? std::nullopt : std::optional<std::int64_t> { util::from_chars(id_str_c.data(), id_str_c.data() + id_str_c.size()) },
    id_str_r.empty() ? std::nullopt : std::optional<std::int64_t> { util::from_chars(id_str_r.data(), id_str_r.data() + id_str_r.size()) },
    ano_proxy,
    satellite
  };
}

auto parse_line_coordinate(const std::string &line) {
  bool del_whitespace { true };
  auto line_sane = util::copy_if(line, [&](char ch) {
    if(ch == ' ' && del_whitespace) {
      return false;
    }

    if(ch == '\"') {
      del_whitespace = !del_whitespace;

      return false;
    }

    return true;
  });

  auto values = util::split(line_sane, ',');

  std::size_t _;
  double latitude  = std::stod(std::string { values[4].data(), values[4].size() }, &_);
  double longitude = std::stod(std::string { values[5].data(), values[5].size() }, &_);

  return coordinate_t {
    values[1],
    latitude,
    longitude
  };
}

void init_country(sqlite::Database &database, const char *path) {
  auto model = country_m(database);

  auto fd = file::ioRead(path);

  if(!fd.is_open()) {
    print(error, "Couldn't open ", path, ": ", err::current());

    std::abort();
  }

  auto _ = file::read_line(fd);

  while(DISABLE_IF(!fd.eof())) {
    auto g = model.transaction();

    if(!g) {
      print(error, err::current());

      std::abort();
    }

    for(auto i = 0; i < MAX_INSERT && !fd.eof(); ++i) {
      auto line = file::read_line(fd);

      if(line->empty()) {
        continue;
      }

      if(!line) {
        print(error, "Can't read line from ", path, ": ", err::current());

        std::abort();
      }

      auto c = model.build_with_id(parse_line_country(*line));
      if(!c) {
        print(error, "Couldn't insert model: ", err::current());

        std::abort();
      }
    }
  }
}

void init_ip_block(sqlite::Database &database, const char *path) {
  auto model = ip_block_m(database);

  auto fd = file::ioRead(path);

  if(!fd.is_open()) {
    print(error, "Couldn't open ", path, ": ", err::current());

    std::abort();
  }

  auto _ = file::read_line(fd);
  while(DISABLE_IF(!fd.eof())) {
    auto g = model.transaction();

    if(!g) {
      print(error, err::current());

      std::abort();
    }

    for(auto i = 0; i < MAX_INSERT && !fd.eof(); ++i) {
      auto line = file::read_line(fd);

      if(line->empty()) {
        continue;
      }

      if(!line) {
        print(error, "Can't read line from ", path, ": ", err::current());

        std::abort();
      }

      auto c = model.build(parse_line_ip_block(*line));
      if(!c) {
        print(error, "Couldn't insert model: ", err::current());

        std::abort();
      }
    }
  }
}

void init_coordinates(sqlite::Database &database, const char *path) {
  auto model = coord_m(database);

  auto fd = file::ioRead(path);

  if(!fd.is_open()) {
    print(error, "Couldn't open ", path, ": ", err::current());

    std::abort();
  }

  auto _ = file::read_line(fd);
  while(DISABLE_IF(!fd.eof())) {
    auto g = model.transaction();

    if(!g) {
      print(error, err::current());

      std::abort();
    }

    for(auto i = 0; i < MAX_INSERT && !fd.eof(); ++i) {
      auto line = file::read_line(fd);

      if(line->empty()) {
        continue;
      }

      if(!line) {
        print(error, "Can't read line from ", path, ": ", err::current());

        std::abort();
      }

      auto c = model.build(parse_line_coordinate(*line));
      if(!c) {
        print(error, "Couldn't insert model: ", err::current());

        std::abort();
      }
    }
  }
}

sqlite::Database init(const std::filesystem::path &map_ip_country_root, const std::filesystem::path &map_coordinates_root) {
  const char *db_name = "geocode";

  auto country_csv  = std::filesystem::path(map_ip_country_root);
  auto ip_block_csv = std::filesystem::path(map_ip_country_root);
  auto coord_csv    = std::filesystem::path(map_coordinates_root);

  country_csv  /= "GeoLite2-Country-Locations-en.csv"sv;
  ip_block_csv /= "GeoLite2-Country-Blocks-IPv4.csv"sv;
  coord_csv    /= "countries_codes_and_coordinates.csv"sv;

  std::error_code err_code;

  std::filesystem::file_time_type time {};
  for(auto &p : {country_csv, ip_block_csv, coord_csv}) {
    if(!std::filesystem::exists(p, err_code)) {
      print(error, "Couldn't find file: ", p.native());
    }

    time = std::max(time, std::filesystem::last_write_time(p, err_code));
  }

  auto db_path = std::filesystem::current_path(err_code);
  db_path /= "geocode"sv;

  if(std::filesystem::exists(db_path, err_code)) {
    if(time <= std::filesystem::last_write_time(db_path)) {
      print(info, "Database already initialized");

      return { db_name };
    }

    std::filesystem::remove(db_path, err_code);
  }

  sqlite::Database database { db_name };

  print(info, "Initializing table [countries]...");
  init_country(database, country_csv.c_str());

  print(info, "Initializing table [ip_block]...");
  init_ip_block(database, ip_block_csv.c_str());

  print(info, "Initializing table [coordinates]...");
  init_coordinates(database, coord_csv.c_str());

  print(info, "Database initialized");

  return database;
}

std::tuple<std::uint32_t, std::uint32_t> ip_block(const std::string_view &view) {
  auto values = util::split(view, '/');

  auto bits = 32 - util::from_chars(values[1].data(), values[1].data() + values[1].size());

  auto l = std::get<0>((file::ip_addr_t { values[0], 0 }).pack());

  return { l, l + ((1 << bits) -1) };
}

coord_t coordinates(sqlite::Database &database, const std::string_view &country_code) {
  auto coord_model = coord_m(database);

  std::vector<decltype(coord_model)::filter_t> filters { 1 };
  filters[0].set<0>(sqlite::comp_t::eq, std::string { country_code.data(), country_code.size() });

  coord_t coord {};
  coord_model.load(filters, [&coord](auto &&val) {
    coord.latitude  = std::get<1>(val);
    coord.longitude = std::get<2>(val);

    return err::OK;
  });

  return coord;
}

std::string country_code(sqlite::Database &database, const file::ip_addr_t &ip_addr) {
  std::string res;

  auto ip_block_model = ip_block_m(database);

  std::vector<decltype(ip_block_model)::filter_t> filters(1);
  filters[0].set<0>(std::get<0>(ip_addr.pack()));

  ip_block_model.load(filters, [&](auto &&val) {
    std::optional opt_reg = std::move(std::get<1>(val));
    std::optional opt_rep = std::move(std::get<2>(val));

    std::int64_t country_id {};
    if(opt_reg) {
      country_id = *opt_reg;
    }
    else if(opt_reg) {
      country_id = *opt_rep;
    }

    auto country_model = country_m(database);
    auto err = country_model.load(country_id, [&res](auto &&val) {
      res = std::move(*std::get<2>(val));

      return err::OK;
    });

    if(err)
      print(error, err::current());

    return err::OK;
  });

  return res;
}

double to_radian(double degree) {
  return degree * (M_PI / 180.0);
}

double coord_t::distance(const coord_t &coord) {
  auto lat_l = to_radian(latitude);
  auto lat_r = to_radian(coord.latitude);

  auto delta_lat = lat_r - lat_l;
  auto delta_lon = to_radian(coord.longitude - longitude);

  auto a = std::pow(std::sin(delta_lat / 2.0), 2) + std::cos(lat_l) * std::cos(lat_r) * std::pow(std::sin(delta_lon / 2.0), 2);
  auto c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1 - a));

  return 6371 * c;
}
}
