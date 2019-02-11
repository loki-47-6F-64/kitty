project(Kitty-Bluetooth)

##################################################################
#Source bluetooths
FILE (GLOB_RECURSE C_SOURCES "./*.c")
FILE (GLOB_RECURSE CPP_SOURCES "./*.cpp")
FILE (GLOB_RECURSE HEADERS "./*.h")


add_library(kitty-bluetooth STATIC ${C_SOURCES} ${CPP_SOURCES} ${HEADERS})

target_link_libraries(kitty-bluetooth kitty-server kitty-log ${BLUEZ_LIBRARIES})
