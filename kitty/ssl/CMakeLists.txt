project(Kitty-Ssl)
##################################################################
#Source kitty-ssls
FILE (GLOB_RECURSE C_SOURCES "./*.c")
FILE (GLOB_RECURSE CPP_SOURCES "./*.cpp")
FILE (GLOB_RECURSE HEADERS "./*.h")

add_library(kitty-ssl STATIC ${C_SOURCES} ${CPP_SOURCES} ${HEADERS})

target_link_libraries(kitty-ssl ${OPENSSL_LIBRARIES})
set_target_properties(kitty-ssl PROPERTIES
  PUBLIC_HEADER "ssl.h;ssl_client.h;ssl_stream.h"
)
