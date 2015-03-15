project(Kitty-Bluetooth)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

##################################################################
#Source bluetooths
FILE (GLOB_RECURSE C_SOURCES "./*.c")
FILE (GLOB_RECURSE CPP_SOURCES "./*.cpp")


add_library(kitty-bluetooth STATIC ${C_SOURCES} ${CPP_SOURCES})

set_target_properties(kitty-bluetooth PROPERTIES
  PUBLIC_HEADER "blueth.h;blue_client.h;ble.h;uuid.h;blue_stream.h"
)
