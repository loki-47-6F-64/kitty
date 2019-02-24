project(Kitty-SQLite3)

##################################################################
#Source kitty-files
FILE (GLOB_RECURSE C_SOURCES "./*.c")
FILE (GLOB_RECURSE CPP_SOURCES "./*.cpp")
FILE (GLOB_RECURSE HEADERS "./*.h")

add_library(kitty-sqlite3 STATIC ${C_SOURCES} ${CPP_SOURCES} ${HEADERS})
target_link_libraries(kitty-sqlite3 ${SQLITE3_LIBRARIES})
set_target_properties(kitty-sqlite3 PROPERTIES LINKER_LANGUAGE CXX)
