project(KittyBenchmark)

set(CELERO_COMPILE_DYNAMIC_LIBRARIES OFF CACHE BOOL "Overwrite compile mode option")

add_subdirectory(Celero)

include_directories(kitty-benchmark Celero/include)
add_executable(kitty-benchmark
  benchmark.cpp
        file/file_read.cpp file/file_write.cpp)

string(TOUPPER ${CMAKE_BUILD_TYPE} BUILD_TYPE)
if("x${BUILD_TYPE}" STREQUAL "xRELEASE")
    set_target_properties(kitty-benchmark PROPERTIES COMPILE_FLAGS "-Os -save-temps")
endif()

target_link_libraries(kitty-benchmark ${KITTY_LIBRARIES} celero)