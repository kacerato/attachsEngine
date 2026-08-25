cmake_minimum_required(VERSION 3.22.1)
project(AetherNative C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

# Engine mobile: sem RTTI, sem exceções, warnings tratados como erro nos alvos de runtime.
add_compile_options(
  $<$<COMPILE_LANGUAGE:CXX>:-Wall>
  $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
  $<$<COMPILE_LANGUAGE:CXX>:-Wpedantic>
  $<$<COMPILE_LANGUAGE:CXX>:-Werror>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
)

set(AETHER_VULKAN_HEADERS "" CACHE PATH "Diretório que contém vulkan/vulkan.h")
if(AETHER_VULKAN_HEADERS)
  set(Vulkan_INCLUDE_DIRS ${AETHER_VULKAN_HEADERS})
elseif(NOT ANDROID)
  find_package(Vulkan QUIET)
  if(NOT Vulkan_INCLUDE_DIRS)
    set(Vulkan_INCLUDE_DIRS /usr/include)
  endif()
endif()

find_package(Threads REQUIRED)

# --- aether_core: tipos base, alocadores, job system -----------------------
add_library(aether_core STATIC
  core/assert.cpp
  core/job_system.cpp
)
target_include_directories(aether_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(aether_core PUBLIC Threads::Threads)

# --- aether_platform: lifecycle portátil do shell -------------------------
add_library(aether_platform STATIC
  platform/app_lifecycle.cpp
)
target_include_directories(aether_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(aether_platform PUBLIC aether_core)

# --- aether_rendergraph: compilador de render graph (lógica pura) ----------
add_library(aether_rendergraph STATIC
  rendergraph/render_graph.cpp
)
target_include_directories(aether_rendergraph PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(aether_rendergraph PUBLIC aether_core)

# --- aether_rhi: descritores/cache/perfis de dispositivo + wrappers Vulkan -
add_library(aether_rhi STATIC
  rhi/descriptor_cache.cpp
  rhi/device_profile.cpp
  rhi/device.cpp
)
target_include_directories(aether_rhi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${Vulkan_INCLUDE_DIRS})
target_link_libraries(aether_rhi PUBLIC aether_core)

if(ANDROID)
  target_link_libraries(aether_rhi PUBLIC vulkan)

  set(NATIVE_APP_GLUE_DIR ${ANDROID_NDK}/sources/android/native_app_glue)
  add_library(aether_native_app_glue STATIC
    ${NATIVE_APP_GLUE_DIR}/android_native_app_glue.c
  )
  target_include_directories(aether_native_app_glue PUBLIC ${NATIVE_APP_GLUE_DIR})

  add_library(aether_android SHARED
    platform/android/android_main.cpp
    platform/android/android_window.cpp
    platform/android/android_vulkan_surface.cpp
  )
  target_include_directories(aether_android PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(aether_android PRIVATE
    aether_core
    aether_platform
    aether_rhi
    aether_native_app_glue
    android
    log
    vulkan
  )
  # NativeActivity localiza esta função por dlsym; sem a referência explícita
  # o linker pode removê-la por parecer não utilizada.
  target_link_options(aether_android PRIVATE
    "-uANativeActivity_onCreate"
    "-Wl,-z,max-page-size=16384"
  )
endif()

# --- aether_tests: runner próprio, sem dependência externa ------------------
if(NOT ANDROID)
  add_executable(aether_tests
    ${CMAKE_SOURCE_DIR}/../tests/native/main.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_allocators.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_job_system.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_descriptor_cache.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_device_profile.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_render_graph.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_app_lifecycle.cpp
  )
  target_include_directories(aether_tests PRIVATE ${CMAKE_SOURCE_DIR}/../tests/native)
  target_link_libraries(aether_tests PRIVATE aether_core aether_platform aether_rendergraph aether_rhi)
endif()
