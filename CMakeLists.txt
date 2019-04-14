cmake_minimum_required(VERSION 2.8)

project(Kitty)

option(KITTY_BUILD_SERVER  "Compile kitty/server" OFF)
option(KITTY_BUILD_SSL     "Compile kitty/ssl" OFF)
option(KITTY_BUILD_SQLITE3 "Compile the wrapper for sqlite3 kitty/sqlite" OFF)
option(KITTY_BUILD_TOPO    "Compile kitty/topo" OFF) 
option(KITTY_BUILD_P2P     "Compile kitty/p2p --incomplete-- " OFF)
option(KITTY_BUILD_P2P_SIM "Compile kitty/p2p --for shadow network simulator-- " OFF)
option(KITTY_BENCHMARK     "Benchmark kitty")

add_definitions( -DKITTY_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/asset" )

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(KITTY_BENCHMARK OR KITTY_TEST)
  if(NOT KITTY_BUILD_SERVER)
    message("Warning: Implicitly set(KITTY_BUILD_SERVER ON)")
  endif()
  if(NOT KITTY_BUILD_SSL)
    message("Warning: Implicitly set(KITTY_BUILD_SSL ON)")
  endif()
  if(NOT KITTY_BUILD_BLUETOOTH)
    message("Warning: Implicitly set(KITTY_BUILD_BLUETOOTH ON)")
  endif()
  if(NOT KITTY_BUILD_SQLITE3)
    message("Warning: Implicitly set(KITTY_BUILD_SQLITE3 ON)")
  endif()
  if(NOT KITTY_BUILD_TOPO)
    message("Warning: Implicitly set(KITTY_BUILD_TOPO ON)")
  endif()
  if(NOT (KITTY_BUILD_P2P OR KITTY_BUILD_P2P_SIM))
    message("Warning: Implicitly set(KITTY_BUILD_P2P ON)")
  endif()

  set(KITTY_BUILD_SERVER    ON)
  set(KITTY_BUILD_SSL       ON)
  set(KITTY_BUILD_BLUETOOTH ON)
  set(KITTY_BUILD_SQLITE3   ON)
  set(KITTY_BUILD_TOPO      ON)

  if(NOT KITTY_BUILD_P2P_SIM)
    set(KITTY_BUILD_P2P     ON)
  endif()
endif()


if(KITTY_BUILD_SSL OR KITTY_BUILD_BLUETOOTH OR KITTY_BUILD_P2P)
  if(NOT KITTY_BUILD_SERVER)
    message("Warning: Implicitly set(KITTY_BUILD_SERVER ON)")
  endif()
  
  set(KITTY_BUILD_SERVER ON)
endif()

if(KITTY_BUILD_TOPO)
  if(NOT KITTY_BUILD_SQLITE3)
    message("Warning: Implicitly set(KITTY_BUILD_SQLITE3 ON)")
  endif()
  set(KITTY_BUILD_SQLITE3 ON)
endif()

set(KITTY_LIBRARIES kitty)

list(APPEND KITTY_COMPILE_OPTIONS -fPIC -Wall -Wno-missing-braces -Wno-maybe-uninitialized)

string(TOUPPER ${CMAKE_BUILD_TYPE} BUILD_TYPE)
if("x${BUILD_TYPE}" STREQUAL "xDEBUG")
  list(APPEND KITTY_DEFINITIONS -DKITTY_DEBUG=1)
  list(APPEND KITTY_COMPILE_OPTIONS -O0 -pedantic -ggdb3)
elseif("x${BUILD_TYPE}" STREQUAL "xRELEASE")
  list(APPEND KITTY_COMPILE_OPTIONS -O3)
endif()

if(KITTY_BUILD_SERVER)
  find_package(Threads REQUIRED)
  list(APPEND KITTY_EXTERNAL_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})
endif()

if(KITTY_BUILD_SSL)
  find_package(OpenSSL REQUIRED)
  list(APPEND KITTY_EXTERNAL_LIBRARIES ${OPENSSL_LIBRARIES})
  list(APPEND KITTY_DEFINITIONS -DKITTY_BUILD_SSL=1)
endif()

if(KITTY_BUILD_BLUETOOTH)
  find_package(Bluez REQUIRED)
  list(APPEND KITTY_EXTERNAL_LIBRARIES ${BLUEZ_LIBRARIES})
endif()

if(KITTY_BUILD_P2P)
  if(NOT DEFINED PJSIP_INCLUDE_DIRS OR NOT DEFINED PJSIP_LIBRARIES)
    message(FATAL_ERROR "Please specify the root to pjsip -- \${PJSIP_INCLUDE_DIRS} and \${PJSIP_LIBRARIES}")
  endif()
  list(APPEND KITTY_EXTERNAL_LIBRARIES ${PJSIP_LIBRARIES})
endif()

if(KITTY_BUILD_P2P OR KITTY_BUILD_P2P_SIM)
  if(NOT DEFINED MINIUPNP_INCLUDE_DIRS OR NOT DEFINED MINIUPNP_LIBRARIES)
    message(FATAL_ERROR "Please specify the root to miniupnp -- \${MINIUPNP_INCLUDE_DIRS} and \${MINIUPNP_LIBRARIES}")
  endif()

  list(APPEND KITTY_EXTERNAL_LIBRARIES ${MINIUPNP_LIBRARIES})
endif()

if(KITTY_BUILD_SQLITE3)
  if(NOT DEFINED SQLITE3_INCLUDE_DIRS OR NOT DEFINED SQLITE3_LIBRARIES)
    message(FATAL_ERROR "Please specify the root to sqlite3 -- \${SQLITE3_INCLUDE_DIRS} and \${SQLITE3_LIBRARIES}")
  endif()
  list(APPEND KITTY_EXTERNAL_LIBRARIES ${SQLITE3_LIBRARIES})
endif()

if(KITTY_BUILD_TOPO)
  list(APPEND KITTY_EXTERNAL_LIBRARIES stdc++fs)
endif()

# set up include-directories
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${OPENSSL_INCLUDE_DIR}
  ${BLUEZ_INCLUDE_DIRS}
  ${PJSIP_INCLUDE_DIRS}
  ${SQLITE3_INCLUDE_DIRS}
  ${MINIUPNP_INCLUDE_DIRS}
)

macro(kitty_add dir)
  file(GLOB KITTY_TARGETS "kitty/${dir}/*.[c,h]*")
  set(KITTY_TARGET_FILES ${KITTY_TARGET_FILES} ${KITTY_TARGETS})
endmacro()

kitty_add(err)
kitty_add(util)
kitty_add(file)
kitty_add(log)

if(KITTY_BUILD_SQLITE3)
  kitty_add(sqlite3)
endif()

if(KITTY_BUILD_SERVER)
  kitty_add(server)
endif()

if(KITTY_BUILD_SSL)
  kitty_add(ssl)
endif()

if(KITTY_BUILD_BLUETOOTH)  
  kitty_add(blueth)
endif()

if(KITTY_BUILD_TOPO)
  kitty_add(topo)
endif()

if(KITTY_BUILD_P2P)
  kitty_add(p2p/pj)
  kitty_add(p2p)
  kitty_add(p2p/upnp)
endif()

if(KITTY_BUILD_P2P_SIM)
  list(APPEND KITTY_DEFINITIONS -DKITTY_P2P_SIM=1)

  kitty_add(p2p/pj_fake)
  kitty_add(p2p)
  kitty_add(p2p/upnp)
endif()

add_library(kitty STATIC ${KITTY_TARGET_FILES})
#target_link_libraries(kitty ${CMAKE_THREAD_LIBS_INIT} ${PJSIP_LIBRARIES} ${SQLITE3_LIBRARIES} ${OPENSSL_LIBRARIES} ${BLUEZ_LIBRARIES} stdc++fs)
target_link_libraries(kitty ${KITTY_EXTERNAL_LIBRARIES})
target_compile_definitions(kitty PUBLIC ${KITTY_DEFINITIONS})
set_target_properties(kitty PROPERTIES CXX_STANDARD 17)

target_compile_options(kitty PRIVATE ${KITTY_COMPILE_OPTIONS})

set(KITTY_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR} KITTY_INCLUDE_DIR)

if(KITTY_BUILD_P2P OR KITTY_BUILD_P2P_SIM)
  add_subdirectory(kitty/p2p/bootstrap)
endif()

if(KITTY_TEST)
  add_subdirectory(test)
endif()

if(KITTY_BENCHMARK)
  add_subdirectory(benchmark)
endif()

get_directory_property(has_parent PARENT_DIRECTORY)
if(has_parent)
  set(KITTY_INCLUDE_DIR ${KITTY_INCLUDE_DIR} PARENT_SCOPE)
  
  set(KITTY_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR})
  set(KITTY_INCLUDE_DIRS ${KITTY_INCLUDE_DIRS} PARENT_SCOPE)
  
  set(KITTY_LIBRARIES ${KITTY_LIBRARIES} PARENT_SCOPE)
endif()
