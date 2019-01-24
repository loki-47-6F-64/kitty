project(Kitty-Server)

##################################################################
#Source kitty-servers
FILE (GLOB_RECURSE C_SOURCES "./*.c")
FILE (GLOB_RECURSE CPP_SOURCES "./*.cpp")
FILE (GLOB_RECURSE HEADERS "./*.h")

add_library(kitty-server STATIC ${C_SOURCES} ${CPP_SOURCES} ${HEADERS})

set_target_properties(kitty-server PROPERTIES
  PUBLIC_HEADER "server.h;proxy.h;tcp_client.h"
)

set(KITTY_LIBRARIES ${KITTY_LIBRARIES} kitty-server)
set(KITTY_INCLUDE_DIRS ${KITTY_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR})
