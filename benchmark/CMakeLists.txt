project(KittyBenchmark)

set(CELERO_COMPILE_DYNAMIC_LIBRARIES ON CACHE BOOL "Overwrite compile mode option")

add_subdirectory(Celero)

include_directories(kitty-benchmark Celero/include)
add_executable(kitty-benchmark
  benchmark.cpp
        file/file_read.cpp file/file_write.cpp)

string(TOUPPER ${CMAKE_BUILD_TYPE} BUILD_TYPE)

target_link_libraries(kitty-benchmark ${KITTY_LIBRARIES} celero)

set_target_properties(kitty-benchmark PROPERTIES
  CXX_STANDARD 17
  CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-missing-braces"
  CXX_FLAGS_DEBUG "${CMAKE_FLAGS_DEBUG} -O0 -pedantic -g"
  CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3"
)
