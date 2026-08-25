cmake_minimum_required(VERSION 3.22.1)
project(AetherNative C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

# --- Jolt Physics (vendorizado em third_party/, não um pacote externo baixado no
# build — ver native/third_party/JoltPhysics/VENDORED_COMMIT.txt) ----------------
# Precisa entrar ANTES do add_compile_options(-Werror ...) abaixo: propriedades de
# diretório do CMake (compile options incluídas) são herdadas por um add_subdirectory()
# no estado em que estavam NO MOMENTO da chamada — entrando aqui, o subdiretório do
# Jolt nunca vê nossas flags -Werror/-fno-rtti/-fno-exceptions, então um warning no
# CÓDIGO DE TERCEIROS nunca quebra nosso build (não é responsabilidade nossa arrumar
# warning de upstream, e -Werror em código que não escrevemos é frágil por natureza).
set(USE_ASSERTS ON CACHE BOOL "" FORCE)
set(DOUBLE_PRECISION OFF CACHE BOOL "" FORCE) # engine usa float em toda a matemática (ver CONVENCOES.md)
set(CPP_EXCEPTIONS_ENABLED OFF CACHE BOOL "" FORCE)
set(CPP_RTTI_ENABLED OFF CACHE BOOL "" FORCE)
set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "" FORCE) # não deixa o Jolt decidir CMAKE_CXX_FLAGS_* do projeto inteiro
set(ENABLE_OBJECT_STREAM OFF CACHE BOOL "" FORCE) # serialização é a nossa (Aether.Core.Serialization), não a do Jolt
set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE) # link estático interno, nunca instalado como pacote do sistema
set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE) # o Jolt tem seu próprio debug renderer via DX12/VK/Metal — não queremos essa dependência; a engine tem o próprio render graph
set(DEBUG_RENDERER_IN_DISTRIBUTION OFF CACHE BOOL "" FORCE)
set(JPH_USE_DX12 OFF CACHE BOOL "" FORCE)
set(JPH_USE_VK OFF CACHE BOOL "" FORCE)
set(JPH_USE_MTL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/JoltPhysics/Build EXCLUDE_FROM_ALL)

# Engine mobile: sem RTTI, sem exceções, warnings tratados como erro nos alvos de runtime.
# (só para o NOSSO código — ver a nota acima sobre por que o Jolt entra antes desta linha.)
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

# --- aether_physics: fronteira C ABI blittable sobre o Jolt Physics (item 4.1.1) --
# Só transporta tipos blittable pela fronteira (mesma regra de C#<->C++ do CONVENCOES.md,
# aplicada aqui entre nosso C++ e a API C++ do Jolt): todo tipo cruzando jolt_bridge.h é
# POD simples (float, uint32_t, ponteiro opaco), nunca um tipo do Jolt exposto diretamente —
# isso mantém o C# livre de conhecer qualquer detalhe interno do Jolt.
add_library(aether_physics STATIC
  physics/jolt_bridge.cpp
)
target_include_directories(aether_physics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(aether_physics PUBLIC aether_core PRIVATE Jolt)
# O Jolt foi compilado com CPP_RTTI_ENABLED/CPP_EXCEPTIONS_ENABLED=OFF (ver acima), então o
# link é consistente com -fno-rtti -fno-exceptions do resto do nosso código nativo.

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
    ${CMAKE_SOURCE_DIR}/../tests/native/test_jolt_bridge.cpp
  )
  target_include_directories(aether_tests PRIVATE ${CMAKE_SOURCE_DIR}/../tests/native)
  target_link_libraries(aether_tests PRIVATE aether_core aether_platform aether_rendergraph aether_rhi aether_physics)
endif()
