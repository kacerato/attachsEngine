cmake_minimum_required(VERSION 3.22)
project(AstraTerrainControlGpu LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(ASTRA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
find_path(VULKAN_HEADERS vulkan/vulkan.h REQUIRED)
find_program(GLSLC glslc REQUIRED)
foreach(STAGE vert frag)
  add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/control.${STAGE}.spv"
    COMMAND "${GLSLC}" -I "${ASTRA_ROOT}/native/rhi/shaders"
      "${CMAKE_CURRENT_SOURCE_DIR}/control.${STAGE}" -o "${CMAKE_CURRENT_BINARY_DIR}/control.${STAGE}.spv"
    DEPENDS "control.${STAGE}" "${ASTRA_ROOT}/native/rhi/shaders/terrain_surface.glsl"
      "${ASTRA_ROOT}/native/rhi/shaders/terrain_pbr.glsl"
      "${ASTRA_ROOT}/native/rhi/shaders/terrain_snow.glsl"
      "${ASTRA_ROOT}/native/rhi/shaders/material_uv_transform.glsl")
endforeach()
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/fallback.frag.spv"
  COMMAND "${GLSLC}" -DNO_CONTROL_MAP -I "${ASTRA_ROOT}/native/rhi/shaders"
    "${CMAKE_CURRENT_SOURCE_DIR}/control.frag" -o "${CMAKE_CURRENT_BINARY_DIR}/fallback.frag.spv"
  DEPENDS control.frag "${ASTRA_ROOT}/native/rhi/shaders/terrain_surface.glsl"
    "${ASTRA_ROOT}/native/rhi/shaders/terrain_pbr.glsl"
      "${ASTRA_ROOT}/native/rhi/shaders/terrain_snow.glsl"
    "${ASTRA_ROOT}/native/rhi/shaders/material_uv_transform.glsl")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/pbr.frag.spv"
  COMMAND "${GLSLC}" -DTEST_PBR -I "${ASTRA_ROOT}/native/rhi/shaders"
    "${CMAKE_CURRENT_SOURCE_DIR}/control.frag" -o "${CMAKE_CURRENT_BINARY_DIR}/pbr.frag.spv"
  DEPENDS control.frag "${ASTRA_ROOT}/native/rhi/shaders/terrain_surface.glsl"
    "${ASTRA_ROOT}/native/rhi/shaders/terrain_pbr.glsl"
      "${ASTRA_ROOT}/native/rhi/shaders/terrain_snow.glsl"
    "${ASTRA_ROOT}/native/rhi/shaders/material_uv_transform.glsl")
add_executable(astra_terrain_control_gpu main.cpp
  "${ASTRA_ROOT}/native/resources/terrain_surface_asset.cpp"
  "${ASTRA_ROOT}/native/resources/animation_curve.cpp"
  "${CMAKE_CURRENT_BINARY_DIR}/control.vert.spv" "${CMAKE_CURRENT_BINARY_DIR}/control.frag.spv"
  "${CMAKE_CURRENT_BINARY_DIR}/fallback.frag.spv" "${CMAKE_CURRENT_BINARY_DIR}/pbr.frag.spv")
# Isolate Vulkan headers: an NDK sysroot's C headers must not shadow host CRT.
file(COPY "${VULKAN_HEADERS}/vulkan" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/include")
if(EXISTS "${VULKAN_HEADERS}/vk_video")
  file(COPY "${VULKAN_HEADERS}/vk_video" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/include")
endif()
target_include_directories(astra_terrain_control_gpu PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}/include" "${ASTRA_ROOT}/native")
if(UNIX)
  target_link_libraries(astra_terrain_control_gpu PRIVATE dl)
endif()
enable_testing()
add_test(NAME terrain_fragment_control COMMAND astra_terrain_control_gpu "${CMAKE_CURRENT_BINARY_DIR}")
