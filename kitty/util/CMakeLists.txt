project(Kitty-Utils)

##################################################################
#Source files
FILE (GLOB_RECURSE C_SOURCES "./*.c")
FILE (GLOB_RECURSE CPP_SOURCES "./*.cpp")
FILE (GLOB_RECURSE HEADERS "./*.h")

add_custom_target(kitty-utils SOURCES ${C_SOURCES} ${CPP_SOURCES} ${HEADERS})
