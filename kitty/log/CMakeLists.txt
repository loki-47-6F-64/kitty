project(Kitty-Log)

##################################################################
#Source Logs
FILE (GLOB_RECURSE C_SOURCES "./*.c")
FILE (GLOB_RECURSE CPP_SOURCES "./*.cpp")
FILE (GLOB_RECURSE HEADERS "./*.h")

add_library(kitty-log STATIC ${C_SOURCES} ${CPP_SOURCES} ${HEADERS})

set_target_properties(kitty-log PROPERTIES
  PUBLIC_HEADER "log.h"
)
