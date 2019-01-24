project(Kitty-Utils)

##################################################################
#Source files
FILE (GLOB_RECURSE C_SOURCES "./*.c")
FILE (GLOB_RECURSE CPP_SOURCES "./*.cpp")
FILE (GLOB_RECURSE HEADERS "./*.h")

enable_language(CXX)

add_library(kitty-utils STATIC ${C_SOURCES} ${CPP_SOURCES} ${HEADERS})

set_target_properties(kitty-utils PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(kitty-utils PROPERTIES
  PUBLIC_HEADER "move_by_copy.h;optional.h;thread_pool.h;thread_local.h;thread_t.h;utility.h;set.h;string.h;iterator.h;auto_run.h;template_helper.h"
)
