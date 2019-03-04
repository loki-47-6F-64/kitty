//
// Created by loki on 4-3-19.
//

#include <fstream>
#include <cstring>
#include <random>
#include <celero/Celero.h>
#include <kitty/file/io_stream.h>

constexpr auto FILE_SIZE_SMALL  = 1024;
constexpr auto FILE_SIZE_MEDIUM = FILE_SIZE_SMALL << 4;

struct global_t {
  std::vector<std::uint8_t> small;
  std::vector<std::uint8_t> medium;

  global_t() : small(FILE_SIZE_SMALL), medium(FILE_SIZE_MEDIUM) {
    std::random_device seed;

    std::default_random_engine re { seed() };

    std::generate(std::begin(small), std::end(small), re);
    std::generate(std::begin(medium), std::end(medium), re);
  }
} global;

BASELINE(WRITE_SMALL, kitty_write, 20, 3600) {
  auto out { file::ioWrite("write_file_small") };

  print(out, global.small);
}

BASELINE(WRITE_MEDIUM, kitty_write, 20, 3600) {
  auto out { file::ioWrite("write_file_medium") };

  print(out, global.small);
}

BENCHMARK(WRITE_SMALL, std_write, 20, 3600) {
  std::ofstream out("write_file_small", std::ios::out | std::ios::binary);
  out.write((char*)global.small.data(), global.small.size());
}

BENCHMARK(WRITE_SMALL, c_write, 20, 3600) {
  FILE *out = fopen("write_file_small", "w");

  fwrite((char*)global.small.data(), 1, global.small.size(), out);
}

BENCHMARK(WRITE_MEDIUM, std_write, 20, 3600) {
  std::ofstream out("write_file_medium", std::ios::out | std::ios::binary);
  out.write((char*)global.medium.data(), global.medium.size());
}

BENCHMARK(WRITE_MEDIUM, c_write, 20, 3600) {
  FILE *out = fopen("write_file_medium", "w");

  fwrite((char*)global.medium.data(), 1, global.medium.size(), out);
}
