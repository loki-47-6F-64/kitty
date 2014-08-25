project(Kitty-File)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

##################################################################
#Source kitty-files
FILE (GLOB_RECURSE C_SOURCES "./*.c")
FILE (GLOB_RECURSE CPP_SOURCES "./*.cpp")

include_directories(
  ${INCLUDE_DIR}
)


add_library(kitty-file STATIC ${C_SOURCES} ${CPP_SOURCES})

set_target_properties(kitty-file PROPERTIES
  PUBLIC_HEADER "file.h;io_stream.h"
)
 
install(TARGETS kitty-file
  # IMPORTANT: Add the foo library to the "export-set"
  EXPORT KittyTargets
  RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
  LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
  ARCHIVE DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
  PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}/kitty"
  COMPONENT dev
)
