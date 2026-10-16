# Compile production terrain code without dragging editor/physics/render assets
# into geometry and recipe iteration. The full suite also includes these cases.
function(aether_add_terrain_core_tests)
  add_executable(aether_terrain_core_tests EXCLUDE_FROM_ALL
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/native/family_base_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/native/test_terrain_core.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/terrain_asset.cpp)
  target_include_directories(aether_terrain_core_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../tests/native)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(aether_terrain_core_tests PRIVATE
      -Wall -Wextra -Werror -fno-exceptions -fno-rtti)
  endif()
endfunction()
