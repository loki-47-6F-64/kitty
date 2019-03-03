#include <fstream>
#include <cstring>
#include <celero/Celero.h>
#include <kitty/file/io_stream.h>

CELERO_MAIN

constexpr auto FILE_SIZE_SMALL  = 1024;
constexpr auto FILE_SIZE_MEDIUM = FILE_SIZE_SMALL << 4;

#define READ_FILE(x) "read_file_" #x

BASELINE(READ_SMALL, kitty_read, 10, 7200) {
  std::vector<std::uint8_t> buf;
  buf.resize(FILE_SIZE_SMALL);

  auto fd { file::ioRead(READ_FILE(small)) };

  if(fd.read(buf.data(), buf.size()) < 0) {
    throw std::runtime_error("Couldn't read " READ_FILE(small) ": " + std::string(std::strerror(errno)));
  }

  celero::DoNotOptimizeAway(buf);
}

BASELINE(READ_MEDIUM, kitty_read, 10, 3600) {
  std::vector<std::uint8_t> buf;
  buf.resize(FILE_SIZE_MEDIUM);

  auto fd { file::ioRead(READ_FILE(medium)) };
  if(fd.read(buf.data(), buf.size()) < 0) {
    throw std::runtime_error("Couldn't read " READ_FILE(medium) ": " + std::string(std::strerror(errno)));
  }

  celero::DoNotOptimizeAway(buf);
}

BENCHMARK(READ_SMALL, std_read, 10, 3600) {
  std::vector<std::uint8_t> buf;
  buf.resize(FILE_SIZE_SMALL);

  std::ifstream in(READ_FILE(small), std::ios::binary | std::ios::ate);

  if(!in) {
    throw std::runtime_error("Couldn't open " READ_FILE(small) ": " + std::string(std::strerror(errno)));
  }

  in.seekg(0, std::ios::beg);

  if(!in.read((char*)buf.data(), buf.size())) {
    throw std::runtime_error("Couldn't read " READ_FILE(small) ": " + std::string(std::strerror(errno)));
  }

  celero::DoNotOptimizeAway(buf);
}

BENCHMARK(READ_MEDIUM, std_read, 10, 3600) {
  std::vector<std::uint8_t> buf;
  buf.resize(FILE_SIZE_MEDIUM);

  std::ifstream in(READ_FILE(medium), std::ios::binary | std::ios::ate);

  if(!in) {
    throw std::runtime_error("Couldn't open " READ_FILE(medium) ": " + std::string(std::strerror(errno)));
  }

  in.seekg(0, std::ios::beg);

  if(!in.read((char*)buf.data(), buf.size())) {
    throw std::runtime_error("Couldn't read " READ_FILE(medium) ": " + std::string(std::strerror(errno)));
  }

  celero::DoNotOptimizeAway(buf);
}