project(KittyTest)

set(CMAKE_CXX_STANDARD 17)
add_subdirectory(googletest)

add_executable(kitty-test test.cpp server/server.cpp ssl/server.cpp blueth/server.cpp p2p/server.cpp file/tcp.cpp server/proxy.cpp server/multiplex.cpp p2p/kademlia/kademlia.cpp)

include_directories(kitty-test googletest)
target_link_libraries(kitty-test ${KITTY_LIBRARIES} gtest_main)
set_target_properties(kitty-test PROPERTIES CXX_STANDARD 17)
target_compile_options(kitty-test PRIVATE ${KITTY_COMPILE_OPTIONS})
