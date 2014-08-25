project(Kitty-Utils)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

##################################################################
#Source files
FILE (GLOB_RECURSE C_SOURCES "./*.c")
FILE (GLOB_RECURSE CPP_SOURCES "./*.cpp")

include_directories(
  ${INCLUDE_DIR}
)


add_library(kitty-utils STATIC ${C_SOURCES} ${CPP_SOURCES})

set_target_properties(kitty-utils PROPERTIES
  PUBLIC_HEADER "move_by_copy.h;optional.h;thread_pool.h;thread_t.h;utility.h"
)
 
install(TARGETS kitty-utils
  # IMPORTANT: Add the foo library to the "export-set"
  EXPORT KittyTargets
  RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
  LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
  ARCHIVE DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
  PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}/kitty"
  COMPONENT dev
)
