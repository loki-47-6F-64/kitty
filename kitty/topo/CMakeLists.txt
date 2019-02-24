project(Kitty-Topo)

##################################################################
#Source kitty-files
FILE (GLOB_RECURSE C_SOURCES "./*.c")
FILE (GLOB_RECURSE CPP_SOURCES "./*.cpp")
FILE (GLOB_RECURSE HEADERS "./*.h")

add_library(kitty-topo STATIC ${C_SOURCES} ${CPP_SOURCES} ${HEADERS})

target_link_libraries(kitty-topo kitty-sqlite3 kitty-file stdc++fs)
set_target_properties(kitty-topo PROPERTIES LINKER_LANGUAGE CXX)
