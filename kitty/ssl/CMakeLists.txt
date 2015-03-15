project(Kitty-Ssl)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

##################################################################
#Source kitty-ssls
FILE (GLOB_RECURSE C_SOURCES "./*.c")
FILE (GLOB_RECURSE CPP_SOURCES "./*.cpp")

add_library(kitty-ssl STATIC ${C_SOURCES} ${CPP_SOURCES})

target_link_libraries(kitty-ssl ${OPENSSL_LIBRARIES})
set_target_properties(kitty-ssl PROPERTIES
  PUBLIC_HEADER "ssl.h;ssl_client.h;ssl_stream.h"
)
