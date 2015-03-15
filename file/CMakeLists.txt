project(Kitty-File)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

##################################################################
#Source kitty-files
FILE (GLOB_RECURSE C_SOURCES "./*.c")
FILE (GLOB_RECURSE CPP_SOURCES "./*.cpp")

add_library(kitty-file STATIC ${C_SOURCES} ${CPP_SOURCES})

set_target_properties(kitty-file PROPERTIES
  PUBLIC_HEADER "file.h;io_stream.h;tcp.h"
)
