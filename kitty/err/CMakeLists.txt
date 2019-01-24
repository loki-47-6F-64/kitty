project(Kitty-Err)

##################################################################
#Source files
FILE (GLOB_RECURSE C_SOURCES "./*.c")
FILE (GLOB_RECURSE CPP_SOURCES "./*.cpp")
FILE (GLOB_RECURSE HEADERS "./*.h")

add_library(kitty-err STATIC ${C_SOURCES} ${CPP_SOURCES} ${HEADER})

target_link_libraries(kitty-err
  ${OPENSSL_LIBRARIES}
)

set_target_properties(kitty-err PROPERTIES
  PUBLIC_HEADER "err.h"
)
