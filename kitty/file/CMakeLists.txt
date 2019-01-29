project(Kitty-File)

##################################################################
#Source kitty-files
FILE (GLOB_RECURSE C_SOURCES "./*.c")
FILE (GLOB_RECURSE CPP_SOURCES "./*.cpp")
FILE (GLOB_RECURSE HEADERS "./*.h")

add_library(kitty-file STATIC ${C_SOURCES} ${CPP_SOURCES} ${HEADERS})

target_link_libraries(kitty-file kitty-err)
set_target_properties(kitty-file PROPERTIES
  PUBLIC_HEADER "file.h;io_stream.h;tcp.h"
)
