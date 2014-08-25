project(Kitty-Server)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

##################################################################
#Source kitty-servers
FILE (GLOB_RECURSE C_SOURCES "./*.c")
FILE (GLOB_RECURSE CPP_SOURCES "./*.cpp")

add_library(kitty-server STATIC ${C_SOURCES} ${CPP_SOURCES})

set_target_properties(kitty-server PROPERTIES
  PUBLIC_HEADER "server.h;proxy.h;tcp_client.h"
)

install(TARGETS kitty-server
  # IMPORTANT: Add the foo library to the "export-set"
  EXPORT KittyTargets
  RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
  LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
  ARCHIVE DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
  PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}/kitty/server"
  COMPONENT dev
)
