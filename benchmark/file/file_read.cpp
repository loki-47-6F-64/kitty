//
// Created by loki on 4-3-19.
//

#include <fstream>
#include <cstring>
#include <celero/Celero.h>
#include <kitty/file/io_stream.h>

constexpr auto FILE_SIZE_SMALL  = 1024;
constexpr std::size_t FILE_SIZE_MEDIUM = FILE_SIZE_SMALL * 1024;

#define READ_FILE(x) "read_file_" #x

BASELINE(READ_SMALL, kitty_read, 20, 7200) {
  std::vector<std::uint8_t> buf;
  buf.resize(FILE_SIZE_SMALL);

  auto fd { file::ioRead(READ_FILE(small)) };

  if(fd.read(buf.data(), buf.size()) < 0) {
    throw std::runtime_error(
        "Couldn't read " READ_FILE(small) ": " + std::string(std::strerror(errno)));
  }

  celero::DoNotOptimizeAway(buf);
}

/**
 * This is a lot slower than reading everything in an entire bulk
 */
BENCHMARK(READ_SMALL, kitty_struct, 20, 7200) {
  using value_type = std::size_t;

  std::vector<value_type> buf;
  buf.resize(FILE_SIZE_SMALL / sizeof(value_type));

  auto fd { file::ioRead(READ_FILE(small)) };

  for(auto &v : buf) {
    v = *file::read_struct<value_type>(fd);
  }

  celero::DoNotOptimizeAway(buf);
}

BASELINE(READ_STRUCT, kitty_struct_bulk, 20, 7200) {
  using value_type = std::array<std::size_t, FILE_SIZE_SMALL / sizeof(std::size_t)>;
  constexpr std::size_t elements = FILE_SIZE_SMALL / sizeof(value_type);

  static_assert(elements != 0, "CRAP!");
  value_type values[elements];

  auto fd { file::ioRead(READ_FILE(SMALL)) };

  for(auto &v : values) {
    v = *file::read_struct<value_type>(fd);
  }

  celero::DoNotOptimizeAway(values);
}

/**
 * This is a lot slower than reading everything in an entire bulk
 */
BENCHMARK(READ_STRUCT, kitty_struct_s, 20, 3600) {
  using value_type = std::array<std::size_t, 1>;
  constexpr std::size_t elements = FILE_SIZE_SMALL / sizeof(value_type);

  static_assert(elements != 0, "CRAP!");
  value_type values[elements];

  auto fd { file::ioRead(READ_FILE(SMALL)) };

  for(auto &v : values) {
    v = *file::read_struct<value_type>(fd);
  }

  celero::DoNotOptimizeAway(values);
}

BENCHMARK(READ_STRUCT, kitty_struct_m, 20, 3600) {
  using value_type = std::array<std::size_t, (256 / sizeof(std::size_t))>;
  constexpr std::size_t elements = FILE_SIZE_SMALL / sizeof(value_type);

  static_assert(elements != 0, "CRAP!");
  value_type values[elements];

  auto fd { file::ioRead(READ_FILE(SMALL)) };

  for(auto &v : values) {
    v = *file::read_struct<value_type>(fd);
  }

  celero::DoNotOptimizeAway(values);
}

BASELINE(READ_MEDIUM, kitty_read, 20, 720) {
  std::vector<std::uint8_t> buf;
  buf.resize(FILE_SIZE_MEDIUM);

  auto fd { file::ioRead(READ_FILE(medium)) };
  if(fd.read(buf.data(), buf.size()) < 0) {
    throw std::runtime_error(
        "Couldn't read " READ_FILE(medium) ": " + std::string(std::strerror(errno)));
  }

  celero::DoNotOptimizeAway(buf);
}

BENCHMARK(READ_SMALL, c_read, 20, 7200) {
  std::vector<std::uint8_t> buf;
  buf.resize(FILE_SIZE_SMALL);

  FILE *in = fopen(READ_FILE(small), "r");

  fread(buf.data(), 1 << 3, FILE_SIZE_SMALL >> 3, in);

  fclose(in);
}

BENCHMARK(READ_MEDIUM, c_read, 20, 720) {
  std::vector<std::uint8_t> buf;
  buf.resize(FILE_SIZE_MEDIUM);

  FILE *in = fopen(READ_FILE(medium), "r");

  fread(buf.data(), 1 << 3, FILE_SIZE_MEDIUM >> 3, in);

  fclose(in);
}
