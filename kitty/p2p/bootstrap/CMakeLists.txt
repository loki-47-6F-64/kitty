project(KittyBootstrap)

add_executable(kitty-bootstrap
        quest.cpp quest.h
        bootstrap.cpp)

target_link_libraries(kitty-bootstrap ${KITTY_LIBRARIES})
set_target_properties(kitty-bootstrap PROPERTIES
        CXX_STANDARD 17
        COMPILE_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Wall -Wno-missing-braces"
        CXX_FLAGS_DEBUG "${CMAKE_FLAGS_DEBUG} -O0 -pedantic -ggdb3"
        CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3"
        )