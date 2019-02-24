//
// Created by loki on 20-2-19.
//

#ifndef T_MAN_TOPO_H
#define T_MAN_TOPO_H

#include <filesystem>
#include <kitty/sqlite3/sqlite3.h>
#include <kitty/file/tcp.h>

namespace topo {

struct coord_t {
  double latitude;
  double longitude;

  double distance(const coord_t &);
};

coord_t coordinates(sqlite::Database &database, const std::string_view &country_code);
std::string country_code(sqlite::Database &, const file::ip_addr_t &);

std::tuple<std::uint32_t, std::uint32_t> ip_block(const std::string_view &view);

sqlite::Database init(const std::filesystem::path &map_ip_country_root, const std::filesystem::path &map_coordinates_root);
}
#endif //T_MAN_TOPO_H
