project(Kitty-Server)

##################################################################
#Source kitty-servers
FILE (GLOB_RECURSE C_SOURCES "./*.c")
FILE (GLOB_RECURSE CPP_SOURCES "./*.cpp")
FILE (GLOB_RECURSE HEADERS "./*.h")

add_library(kitty-server STATIC ${C_SOURCES} ${CPP_SOURCES} ${HEADERS})
target_link_libraries(kitty-server kitty-file)