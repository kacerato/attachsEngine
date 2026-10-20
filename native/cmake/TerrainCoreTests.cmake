# Compile production terrain code without dragging editor/physics/render assets
# into geometry and recipe iteration. The full suite also includes these cases.
function(aether_add_terrain_core_tests)
  add_library(aether_terrain_lod_dependencies STATIC EXCLUDE_FROM_ALL
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/meshoptimizer/simplifier.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/meshoptimizer/vcacheoptimizer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/meshoptimizer/indexanalyzer.cpp)
  add_executable(aether_terrain_core_tests EXCLUDE_FROM_ALL
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/native/family_base_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/native/test_terrain_core.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/native/test_terrain_lod.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/native/test_terrain_history_store.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/native/test_terrain_neighbors.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/native/test_terrain_recovery.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/native/test_terrain_surface_asset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/native/test_terrain_surface_modules.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/native/test_terrain_snow_next.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/native/test_terrain_surface_catalog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/native/test_terrain_fx_foundation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/native/test_terrain_fx_encoding.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/native/test_terrain_fx_recipe.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/native/test_terrain_surface_v4.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/native/test_terrain_flow_coverage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/native/test_terrain_static_fx_import.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/native/test_terrain_static_fx_table.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/terrain_asset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/terrain_fx_asset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/terrain_fx_field.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/terrain_static_fx_import.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer/authoring_texture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/terrain_history_store.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/terrain_neighbors.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/terrain_surface_asset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/terrain_surface_catalog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/animation_curve.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/asset_registry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/json_reader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/atomic_asset_file.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/mesh_lod_build.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer/lod_selection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer/static_collision_mesh.cpp)
  target_link_libraries(aether_terrain_core_tests PRIVATE aether_terrain_lod_dependencies)
  target_include_directories(aether_terrain_core_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../tests/native
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/meshoptimizer)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(aether_terrain_core_tests PRIVATE
      -Wall -Wextra -Werror -fno-exceptions -fno-rtti)
  endif()
endfunction()

# Optional boundary test uses the real importer and every codec it links to.
# Keep the geometry-only iteration free of these dependencies by default.
function(aether_add_terrain_import_tests)
  add_executable(aether_terrain_import_tests EXCLUDE_FROM_ALL
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/native/family_base_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/native/test_terrain_import.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/native/test_terrain_surface_domain.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/native/test_terrain_static_fx_pipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/terrain_asset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/terrain_fx_asset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/terrain_fx_field.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/terrain_static_fx_import.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/terrain_surface_asset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/terrain_surface_import.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/import_cache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/gltf_import.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/json_reader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/gltf_codecs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/image_decode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/tiff_decode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/texture_compression.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/mesh_lod_build.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/skeletal_animation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/animation_curve.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/asset_registry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer/authoring_texture.cpp)
  target_link_libraries(aether_terrain_import_tests PRIVATE aether_codecs aether_astcenc)
  target_include_directories(aether_terrain_import_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../tests/native)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(aether_terrain_import_tests PRIVATE
      -Wall -Wextra -Werror -fno-exceptions -fno-rtti)
  endif()
endfunction()

function(aether_add_terrain_surface_preview)
  add_executable(aether_terrain_surface_preview EXCLUDE_FROM_ALL
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/native/terrain_surface_preview_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/editor/editor_terrain_screen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/editor/editor_terrain_fx_controller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/editor/editor_view.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/scene_graph.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/input_actions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/transform_math.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/asset_registry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/terrain_asset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/terrain_fx_asset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/terrain_fx_field.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/terrain_static_fx_import.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer/authoring_texture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/terrain_surface_asset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/terrain_surface_catalog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/animation_curve.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/json_reader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/ui_draw_list.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/ui_text.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/ui_input.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/ui_font.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/ui_icon_atlas.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/ui_instance_builder.cpp)
  target_include_directories(aether_terrain_surface_preview PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../tests/native)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(aether_terrain_surface_preview PRIVATE
      -Wall -Wextra -Werror -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections)
    # Keep actual scene/transform helpers; no test substitutes for dependencies.
    if(APPLE)
      target_link_options(aether_terrain_surface_preview PRIVATE -Wl,-dead_strip)
    else()
      target_link_options(aether_terrain_surface_preview PRIVATE -Wl,--gc-sections)
    endif()
  endif()
endfunction()
