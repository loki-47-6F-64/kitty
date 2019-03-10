project(KittyBootstrap)

include_directories(kitty-benchmark Celero/include)
add_executable(kitty-bootstrap
        quest.cpp quest.h
        bootstrap.cpp)

target_link_libraries(kitty-bootstrap ${KITTY_LIBRARIES})
set_target_properties(kitty-bootstrap PROPERTIES
        CXX_STANDARD 17
        CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-missing-braces"
        CXX_FLAGS_DEBUG "${CMAKE_FLAGS_DEBUG} -O0 -pedantic -g"
        CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3"
        )