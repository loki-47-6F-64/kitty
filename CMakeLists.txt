cmake_minimum_required(VERSION 2.8)

project(Kitty)

set(KITTY_MAJOR_VERSION 1)
set(KITTY_MINOR_VERSION 0)
set(KITTY_PATCH_VERSION 0)

set(KITTY_VERSION
  ${KITTY_MAJOR_VERSION}.${KITTY_MINOR_VERSION}.${KITTY_PATCH_VERSION}
)


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-missing-braces -std=c++17")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")


list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(KITTY_BUILD_SSL OR KITTY_BUILD_BLUETOOTH)
  if(NOT KITTY_BUILD_SERVER)
    message("Warning: Implicitly set(KITTY_BUILD_SERVER ON)")
  endif()
  
  set(KITTY_BUILD_SERVER ON)
endif()

set(KITTY_LIBRARIES
  kitty-err
  kitty-file
  kitty-log
)

string(TOUPPER ${CMAKE_BUILD_TYPE} BUILD_TYPE)
if(${BUILD_TYPE} STREQUAL "DEBUG")
  add_definitions( -DKITTY_DEBUG=1 )

  # For std::assert
  add_definitions( -DNDEBUG )
endif()

if(KITTY_BUILD_SERVER)
  find_package(Threads)
endif()

if(KITTY_BUILD_SSL)
  find_package(OpenSSL)
  
  add_definitions( -DKITTY_BUILD_SSL=1 )
endif()

if(KITTY_BUILD_BLUETOOTH)
  find_package(Bluez)
endif()

# set up include-directories
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${OPENSSL_INCLUDE_DIR}
  ${BLUEZ_INCLUDE_DIRS}
)

add_subdirectory(kitty/err)
add_subdirectory(kitty/util)
add_subdirectory(kitty/file)
add_subdirectory(kitty/log)

if(KITTY_BUILD_SERVER)
  set(KITTY_LIBRARIES ${KITTY_LIBRARIES} kitty-server)
  add_subdirectory(kitty/server)
endif()

if(KITTY_BUILD_SSL)
  set(KITTY_LIBRARIES ${KITTY_LIBRARIES} kitty-ssl)
  add_subdirectory(kitty/ssl)
endif()

if(KITTY_BUILD_BLUETOOTH)  
  set(KITTY_LIBRARIES ${KITTY_LIBRARIES} kitty-bluetooth)
  add_subdirectory(kitty/blueth)
endif()

set(KITTY_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR} KITTY_INCLUDE_DIR)
set(KITTY_INCLUDE_DIR ${KITTY_INCLUDE_DIR} PARENT_SCOPE)

set(KITTY_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR})
set(KITTY_INCLUDE_DIRS ${KITTY_INCLUDE_DIRS} PARENT_SCOPE)

set(KITTY_LIBRARIES ${KITTY_LIBRARIES} PARENT_SCOPE)
