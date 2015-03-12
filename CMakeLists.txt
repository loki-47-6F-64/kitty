cmake_minimum_required(VERSION 2.8)

project(Kitty)


set(KITTY_MAJOR_VERSION 0)
set(KITTY_MINOR_VERSION 9)
set(KITTY_PATCH_VERSION 0)

set(KITTY_VERSION
  ${KITTY_MAJOR_VERSION}.${KITTY_MINOR_VERSION}.${KITTY_PATCH_VERSION}
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Offer the user the choice of overriding the installation directories
set(INSTALL_LIB_DIR lib CACHE PATH "Installation directory for libraries")
set(INSTALL_BIN_DIR bin CACHE PATH "Installation directory for executables")
set(INSTALL_INCLUDE_DIR include CACHE PATH "Installation directory for header files")
if(WIN32 AND NOT CYGWIN)
  set(DEF_INSTALL_CMAKE_DIR cmake)
else()
  set(DEF_INSTALL_CMAKE_DIR lib/cmake/Kitty)
endif()

set(INSTALL_CMAKE_DIR ${DEF_INSTALL_CMAKE_DIR} CACHE PATH "Installation directory for CMake files")

# Make relative paths absolute (needed later on)
foreach(p LIB BIN INCLUDE CMAKE)
  set(var INSTALL_${p}_DIR)
  if(NOT IS_ABSOLUTE "${${var}}")
    set(${var} "${CMAKE_INSTALL_PREFIX}/${${var}}")
  endif()
endforeach()
 
# set up include-directories
include_directories(
  "${PROJECT_SOURCE_DIR}"
  "${PROJECT_BINARY_DIR}"
  ${OPENSSL_INCLUDE_DIR}
  "../"
)

if(BUILD_SSL OR BUILD_BLUETOOTH)
  if(NOT BUILD_SERVER)
    message("Warning: Implicitly set(BUILD_SERVER ON)")
  endif()
  
  set(BUILD_SERVER ON)
endif()

set(KITTY_TARGETS
  kitty-err
  kitty-file
  kitty-log
  kitty-utils
)

string(TOUPPER ${CMAKE_BUILD_TYPE} BUILD_TYPE)
if(${BUILD_TYPE} STREQUAL "DEBUG")
  add_definitions( -DKITTY_DEBUG=1 )
endif()

if(BUILD_SERVER)
  find_package(Threads)
endif()

if(BUILD_SSL)
  find_package(OpenSSL)
  
  add_definitions( -DKITTY_BUILD_SSL=1 )
endif()

if(BUILD_BLUETOOTH)
  find_package(Bluez)
endif()

# Add sub-directories
set(INCLUDE_DIR include)

add_subdirectory(err)
add_subdirectory(util)
add_subdirectory(file)
add_subdirectory(log)
set(INCLUDE_DIR ${INCLUDE_DIR} ${Kitty-Log_SOURCE_DIR}/include)
if(BUILD_SERVER)
  set(KITTY_TARGETS ${KITTY_TARGETS} kitty-server)
  add_subdirectory(server)
endif()

if(BUILD_SSL)
  set(KITTY_TARGETS ${KITTY_TARGETS} kitty-ssl)
  add_subdirectory(ssl)
endif()

if(BUILD_BLUETOOTH)  
  set(KITTY_TARGETS ${KITTY_TARGETS} kitty-bluetooth)
  add_subdirectory(blueth)
endif()

# The interesting stuff goes here
# ===============================
 
# Add all targets to the build-tree export set
export(TARGETS ${KITTY_TARGETS}
  FILE "${PROJECT_BINARY_DIR}/KittyTargets.cmake"
)
 
# Export the package for use from the build-tree
# (this registers the build-tree with a global CMake-registry)
export(PACKAGE Kitty)
 
# Create the FooBarConfig.cmake and FooBarConfigVersion files
file(RELATIVE_PATH REL_INCLUDE_DIR "${INSTALL_CMAKE_DIR}"
   "${INSTALL_INCLUDE_DIR}"
)
   
# ... for the build tree
set(CONF_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}" "${PROJECT_BINARY_DIR}")
configure_file(KittyConfig.cmake.in
  "${PROJECT_BINARY_DIR}/KittyConfig.cmake" @ONLY
)
  
  
# ... for the install tree
set(CONF_INCLUDE_DIRS "\${KITTY_CMAKE_DIR}/${REL_INCLUDE_DIR}")
configure_file(KittyConfig.cmake.in
  "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/KittyConfig.cmake" @ONLY)
# ... for both
configure_file(KittyConfigVersion.cmake.in
  "${PROJECT_BINARY_DIR}/KittyConfigVersion.cmake" @ONLY
)
