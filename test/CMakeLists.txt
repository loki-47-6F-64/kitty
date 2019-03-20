project(KittyTest)

set(CMAKE_CXX_STANDARD 17)
add_subdirectory(googletest)

add_executable(kitty-test test.cpp server/server.cpp ssl/server.cpp blueth/server.cpp p2p/server.cpp)

include_directories(kitty-test googletest)
target_link_libraries(kitty-test ${KITTY_LIBRARIES} gtest_main)
set_target_properties(kitty-test PROPERTIES
  CXX_STANDARD 17
  COMPILE_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-missing-braces"
  CXX_FLAGS_DEBUG "${CMAKE_FLAGS_DEBUG} -O0 -pedantic -g"
  CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3"
)
