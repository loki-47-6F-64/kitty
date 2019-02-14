project(Kitty-Utils)

##################################################################
#Source files
FILE (GLOB_RECURSE C_SOURCES "./*.c")
FILE (GLOB_RECURSE CPP_SOURCES "./*.cpp")
FILE (GLOB_RECURSE HEADERS "./*.h")

enable_language(CXX)

add_library(kitty-utils STATIC ${C_SOURCES} ${CPP_SOURCES} ${HEADERS})

set_target_properties(kitty-utils PROPERTIES LINKER_LANGUAGE CXX)
