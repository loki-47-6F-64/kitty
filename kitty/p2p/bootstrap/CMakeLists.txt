project(KittyBootstrap)

if(KITTY_BUILD_P2P_SIM)
  list(APPEND CMAKE_MODULE_PATH "$ENV{HOME}/.shadow/share/cmake/Modules")
  include(ShadowTools)
  add_shadow_plugin(kitty-bootstrap bootstrap.cpp)
else()
  add_executable(kitty-bootstrap bootstrap.cpp)
endif()

target_link_libraries(kitty-bootstrap ${KITTY_LIBRARIES})
set_target_properties(kitty-bootstrap PROPERTIES CXX_STANDARD 17)
target_compile_options(kitty-bootstrap PRIVATE ${KITTY_COMPILE_OPTIONS})