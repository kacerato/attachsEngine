# Keep host editor acceptance builds small. This affects debug information,
# never optimization, assertions, Android builds or runtime behavior.
# Used via CMAKE_PROJECT_INCLUDE; defer until the real targets exist.
if(PROJECT_NAME STREQUAL "AetherNative" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  function(aether_host_validation_symbols)
    foreach(target IN ITEMS aether_editor aether_gui_tests aether_gui_preview)
      if(TARGET ${target})
        target_compile_options(${target} PRIVATE -g0)
      endif()
    endforeach()
  endfunction()
  cmake_language(DEFER CALL aether_host_validation_symbols)
endif()
