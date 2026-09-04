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
# O Jolt transforma qualquer EPhysicsUpdateError em assert quando USE_ASSERTS
# está ligado. Isso é desejável em Debug/teste (fail-fast), mas impediria o
# contrato de Release de devolver flags, registrar contador e continuar com
# warning rate-limited. A política acompanha o tipo de build explicitamente.
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
  set(USE_ASSERTS OFF CACHE BOOL "" FORCE)
else()
  set(USE_ASSERTS ON CACHE BOOL "" FORCE)
endif()
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
# O check de IPO/LTO do Jolt reporta explicitamente que o NDK usado pelo alvo
# Android não suporta essa configuração. Mantê-lo ON só repete um warning em
# todo clean build; não desliga uma otimização que estivesse funcionando.
if(ANDROID)
  set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
endif()

# Item 4.1.8 do plano ("determinismo em ponto fixo, modo opcional"): o Jolt não tem um modo
# fixed-point — reescrever seu solver/matemática inteiros para ponto fixo seria uma reescrita
# profunda de código de terceiros vendorizado, não uma configuração (ver docs/ESTADO.md para a
# investigação completa). O que o PRÓPRIO Jolt oferece nativamente, sem tocar em uma linha do
# código dele, é CROSS_PLATFORM_DETERMINISTIC: desliga a fusão multiplicação-adição (FMA) — que
# o próprio comentário do Jolt (Build/CMakeLists.txt) documenta como incompatível com
# determinismo cross-platform — e usa /fp:precise (MSVC) ou -ffp-contract=off (Clang/GCC) em vez
# de fast-math. Também travamos aqui o nível de instrução SIMD em SSE4 (desligando
# AVX/AVX2/FMADD/F16C) quando este modo está ligado: resultados de FMA/AVX podem diferir de
# SSE4 em CPUs diferentes mesmo com fp:precise, e o objetivo do modo é "mesmo resultado em
# qualquer dispositivo Android/iOS", não só "sem FMA". Trade-off aceito: perde parte do ganho de
# performance de AVX2/FMA — mesmo trade-off que o próprio Jolt documenta e aceita para quem liga
# a flag. Modo OPCIONAL de verdade: alterna entre duas configurações de build da MESMA lib
# (como DOUBLE_PRECISION já funciona neste arquivo), não dois binários coexistindo — escolha em
# tempo de configuração do CMake (-DAETHER_PHYSICS_DETERMINISTIC=ON), não em runtime.
option(AETHER_PHYSICS_DETERMINISTIC "Compila o Jolt com CROSS_PLATFORM_DETERMINISTIC + SIMD travado em SSE4 (item 4.1.8) — troca parte do desempenho de AVX2/FMA por reprodutibilidade bit-exata entre plataformas" OFF)
set(CROSS_PLATFORM_DETERMINISTIC ${AETHER_PHYSICS_DETERMINISTIC} CACHE BOOL "" FORCE)
if(AETHER_PHYSICS_DETERMINISTIC)
  set(USE_AVX OFF CACHE BOOL "" FORCE)
  set(USE_AVX2 OFF CACHE BOOL "" FORCE)
  set(USE_FMADD OFF CACHE BOOL "" FORCE)
  set(USE_F16C OFF CACHE BOOL "" FORCE)
  # SSE4.1/SSE4.2/LZCNT/TZCNT permanecem ligados: são baseline em qualquer x86-64 relevante
  # (não introduzem a mesma divergência de resultado que FMA/AVX2 introduzem) e o Jolt não
  # documenta incompatibilidade deles com CROSS_PLATFORM_DETERMINISTIC.
endif()
add_subdirectory(third_party/JoltPhysics/Build EXCLUDE_FROM_ALL)

# --- Box2D v3.1.1 (somente benchmark comparativo do item 4.1.6) -----------
# Entra antes das flags estritas da engine pelo mesmo motivo do Jolt: warnings
# de código de terceiros não podem quebrar nossos alvos. A biblioteca NÃO é
# ligada a aether_physics nem ao runtime; existe apenas nos três executáveis de
# benchmark abaixo para tornar a decisão Jolt-restrito x Box2D mensurável.
set(BOX2D_DISABLE_SIMD OFF CACHE BOOL "" FORCE)
set(BOX2D_COMPILE_WARNING_AS_ERROR OFF CACHE BOOL "" FORCE)
set(BOX2D_PROFILE OFF CACHE BOOL "" FORCE)
set(BOX2D_VALIDATE OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/Box2D EXCLUDE_FROM_ALL)

# --- SQLite 3.53.4 (item 1.4.5: índice de dependências) --------------------
# Mesmo motivo de entrar antes das flags estritas da engine: warnings de código de terceiros
# (aqui, C puro dentro de um build majoritariamente C++) não podem quebrar nossos alvos.
# EXCLUDE_FROM_ALL: só compilada quando algo depende dela (aether_resources abaixo), não em
# todo `cmake --build .` genérico.
add_subdirectory(third_party/sqlite EXCLUDE_FROM_ALL)

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
  if(NOT EXISTS "${AETHER_VULKAN_HEADERS}/vulkan/vulkan.h")
    message(FATAL_ERROR "AETHER_VULKAN_HEADERS='${AETHER_VULKAN_HEADERS}' não contém vulkan/vulkan.h")
  endif()
  set(Vulkan_INCLUDE_DIRS ${AETHER_VULKAN_HEADERS})
elseif(NOT ANDROID)
  find_package(Vulkan QUIET)
  if(NOT Vulkan_INCLUDE_DIRS AND EXISTS "/usr/include/vulkan/vulkan.h")
    set(Vulkan_INCLUDE_DIRS /usr/include)
  endif()
  if(NOT Vulkan_INCLUDE_DIRS)
    message(FATAL_ERROR
      "Headers Vulkan não encontrados. Instale o Vulkan SDK ou configure "
      "-DAETHER_VULKAN_HEADERS=<diretório que contém vulkan/vulkan.h>. "
      "No CI Windows usamos os headers do NDK pinado, sem depender de cache global.")
  endif()
endif()

find_package(Threads REQUIRED)

# --- aether_transform: composição topológica em um único lote blittable -----
# O ECS e a compilação da hierarquia permanecem no C#. Esta biblioteca recebe
# somente um plano blittable de endereços estáveis por frame, evitando uma travessia
# P/Invoke/JNI e cópias por entidade, com fallback gerenciado quando o binário não existir.
add_library(aether_transform SHARED
  transform/transform_bridge.cpp
)
target_include_directories(aether_transform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
  set_target_properties(aether_transform PROPERTIES PREFIX "")
endif()
if(ANDROID)
  target_link_options(aether_transform PRIVATE "-Wl,-z,max-page-size=16384")
endif()

# --- aether_core: tipos base, alocadores, job system -----------------------
add_library(aether_core STATIC
  core/assert.cpp
  core/job_system.cpp
  profiler/frame_pressure.cpp
  profiler/frame_statistics.cpp
)
target_include_directories(aether_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(aether_core PUBLIC Threads::Threads)

# --- aether_platform: lifecycle portátil do shell -------------------------
add_library(aether_platform STATIC
  platform/app_lifecycle.cpp
  platform/atomic_asset_file.cpp
  platform/camera_route.cpp
  platform/first_person_controller.cpp
  platform/free_camera_controller.cpp
  platform/process_memory.cpp
  platform/virtual_joystick.cpp
)
target_include_directories(aether_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(aether_platform PUBLIC aether_core)

# --- aether_rendergraph: compilador de render graph (lógica pura) ----------
add_library(aether_rendergraph STATIC
  rendergraph/render_graph.cpp
)
target_include_directories(aether_rendergraph PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(aether_rendergraph PUBLIC aether_core)

# --- aether_renderer: recursos e conversões independentes do backend -------
add_library(aether_renderer STATIC
  renderer/dynamic_resolution.cpp
  renderer/environment_map.cpp
  renderer/frame_graph.cpp
  renderer/frustum_visibility.cpp
  renderer/gpu_draw_culling.cpp
  renderer/rendering_policy.cpp
  renderer/shadow_cascades.cpp
  renderer/hzb_visibility.cpp
  renderer/lod_selection.cpp
  renderer/spatial_render_chunks.cpp
  renderer/static_collision_mesh.cpp
  renderer/thermal_pressure_controller.cpp
)
target_include_directories(aether_renderer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# O render graph deixa de ser alvo apenas de teste: a politica de anexos do
# frame (load/store e memoryless do depth) e derivada dele, nao escrita a mao.
target_link_libraries(aether_renderer PUBLIC aether_core aether_rendergraph)

# --- aether_rhi: descritores/cache/perfis de dispositivo + wrappers Vulkan -
add_library(aether_rhi STATIC
  rhi/bindless_index_allocator.cpp
  rhi/bindless_registry.cpp
  rhi/compute.cpp
  rhi/compute_validation.cpp
  rhi/descriptor_cache.cpp
  rhi/device_profile.cpp
  rhi/device.cpp
  rhi/gpu_frame_timer.cpp
  rhi/memory_allocator.cpp
  rhi/memory_budget.cpp
  rhi/pipeline_cache.cpp
  rhi/resource.cpp
  rhi/resource_desc.cpp
  rhi/render_graph_vulkan.cpp
  rhi/render_graph_vulkan_mapping.cpp
  rhi/upload_context.cpp
)
target_include_directories(aether_rhi PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/VulkanMemoryAllocator/include
  ${Vulkan_INCLUDE_DIRS}
)
target_link_libraries(aether_rhi PUBLIC aether_core aether_rendergraph)

if(ANDROID)
  find_package(games-frame-pacing REQUIRED CONFIG)
  target_link_libraries(aether_rhi PUBLIC vulkan)

  set(NATIVE_APP_GLUE_DIR ${ANDROID_NDK}/sources/android/native_app_glue)
  add_library(aether_native_app_glue STATIC
    ${NATIVE_APP_GLUE_DIR}/android_native_app_glue.c
  )
  target_include_directories(aether_native_app_glue PUBLIC ${NATIVE_APP_GLUE_DIR})

  add_library(aether_android SHARED
    platform/android/android_main.cpp
    platform/android/android_texture_loader.cpp
    platform/android/dirt_road_resources.cpp
    platform/android/android_launch_options.cpp
    platform/android/android_performance.cpp
    platform/android/android_thermal_monitor.cpp
    platform/android/android_frame_profiler.cpp
    platform/android/android_frame_pacer.cpp
    platform/android/android_swappy_scheduler.cpp
    platform/android/android_window.cpp
    platform/android/android_paths.cpp
    platform/android/android_vulkan_surface.cpp
    platform/android/android_triangle_renderer.cpp
    platform/android/instanced_renderer.cpp
    platform/android/material_preview_resources.cpp
    platform/android/dotnet_host.cpp
    platform/android/dotnet_assets.cpp
    platform/android/astc_encode_probe.cpp
  )
  target_include_directories(aether_android PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/dotnet-runtime/include
  )
  target_link_libraries(aether_android PRIVATE
    aether_core
    aether_platform
    aether_renderer
    aether_rhi
    aether_native_app_glue
    android
    log
    vulkan
    dl
    games-frame-pacing::swappy_static
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
  physics/character_motor.cpp
  physics/jolt_bridge.cpp
)
target_include_directories(aether_physics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(aether_physics PUBLIC aether_core PRIVATE Jolt)
if(ANDROID)
  target_link_libraries(aether_android PRIVATE aether_physics)
endif()
# O Jolt foi compilado com CPP_RTTI_ENABLED/CPP_EXCEPTIONS_ENABLED=OFF (ver acima), então o
# link é consistente com -fno-rtti -fno-exceptions do resto do nosso código nativo.

# --- aether_physics_shared: mesmo código de aether_physics, mas como biblioteca dinâmica —
# item 4.1.2 do plano (fachada C# via P/Invoke) precisa de um .dll/.so carregável em runtime;
# uma lib estática não pode ser carregada por [LibraryImport]/[DllImport]. Alvo SEPARADO de
# aether_physics (não SHARED_LIBS global) porque aether_tests (abaixo) já linka a versão
# estática direto no executável nativo — mantendo os dois evita compilar jolt_bridge.cpp
# duas vezes com flags divergentes por engano.
if(NOT ANDROID)
  add_library(aether_physics_shared SHARED
    physics/jolt_bridge.cpp
  )
  # .NET resolve [LibraryImport("aether_physics")] para "aether_physics.dll" no Windows
  # (sem prefixo) e "libaether_physics.so" no Linux (com prefixo) — são convenções de
  # plataforma DIFERENTES, então PREFIX precisa ser desligado só no Windows/MinGW; no
  # Linux o prefixo "lib" default do CMake já é o que o runtime espera.
  set_target_properties(aether_physics_shared PROPERTIES OUTPUT_NAME "aether_physics")
  if(WIN32)
    set_target_properties(aether_physics_shared PROPERTIES PREFIX "")
  endif()
  target_include_directories(aether_physics_shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(aether_physics_shared PUBLIC aether_core PRIVATE Jolt)
endif()

# --- aether_resources: fronteira C ABI blittable sobre o SQLite vendorizado (item 1.4.5) --
# Só o binding (abrir/fechar, preparar/step, bind/coluna) — o schema do índice de dependências em
# si é trabalho futuro da Fase 7, quando existir consumidor real (ver sqlite_bridge.h).
add_library(aether_resources STATIC
  resources/sqlite_bridge.cpp
)
target_include_directories(aether_resources PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(aether_resources PUBLIC aether_core PRIVATE sqlite3)

# --- aether_resources_shared: mesma lógica de aether_physics_shared — P/Invoke precisa de
# biblioteca dinâmica carregável em runtime, não estática linkada no processo de teste nativo.
if(NOT ANDROID)
  add_library(aether_resources_shared SHARED
    resources/sqlite_bridge.cpp
  )
  set_target_properties(aether_resources_shared PROPERTIES OUTPUT_NAME "aether_resources")
  if(WIN32)
    set_target_properties(aether_resources_shared PROPERTIES PREFIX "")
  endif()
  target_include_directories(aether_resources_shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(aether_resources_shared PUBLIC aether_core PRIVATE sqlite3)
endif()

# --- aether_tests: runner próprio, sem dependência externa ------------------
if(NOT ANDROID)
  add_executable(aether_tests
    ../tests/native/test_atomic_asset_file.cpp
    ../tests/native/test_camera_route.cpp
    ../tests/native/test_free_camera_controller.cpp
    ../tests/native/test_first_person_controller.cpp
    ../tests/native/test_foliage_impostors.cpp
    ../tests/native/test_frustum_visibility.cpp
    ../tests/native/test_gpu_draw_culling.cpp
    ../tests/native/test_hzb_visibility.cpp
    ../tests/native/test_lod_selection.cpp
    ../tests/native/test_spatial_render_chunks.cpp
    ../tests/native/test_character_motor.cpp
    ../tests/native/test_map_package.cpp
    ../tests/native/test_material_preview.cpp
    ../tests/native/test_normal_matrix.cpp
    ../tests/native/test_process_memory.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/main.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_allocators.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_job_system.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_descriptor_cache.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_bindless_registry.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_device_profile.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_compute.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_render_graph_vulkan.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_descriptor_indexing_support.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_astc_probe_validation.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_memory_budget.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_rhi_resources.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_frame_graph.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_dynamic_resolution.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_environment_map.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_rendering_policy.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_shadow_cascades.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_surface_format.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_thermal_pressure_controller.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_render_graph.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_app_lifecycle.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_frame_pressure.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_frame_statistics.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_frame_policy.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_jolt_bridge.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_sqlite_bridge.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_trigger_bridge.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_body_batch_bridge.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_joint_bridge.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_query_bridge.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_character_bridge.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_physics2d_bridge.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_physics_capacity.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_determinism.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_transform_bridge.cpp
  )
  target_include_directories(aether_tests PRIVATE ${CMAKE_SOURCE_DIR}/../tests/native)
  target_link_libraries(aether_tests PRIVATE aether_core aether_platform aether_rendergraph aether_renderer aether_rhi aether_physics aether_transform aether_resources)

  # Probe separado porque induzir overflow em Debug dispara corretamente o
  # assert fail-fast do próprio Jolt. Em Release este binário valida a outra
  # metade do contrato: warning + flags + contadores, sem encerramento silencioso.
  add_executable(physics_overflow_probe
    ${CMAKE_SOURCE_DIR}/../tests/native/physics_overflow_probe.cpp
  )
  target_include_directories(physics_overflow_probe PRIVATE ${CMAKE_SOURCE_DIR}/../tests/native)
  target_link_libraries(physics_overflow_probe PRIVATE aether_core aether_physics)
endif()

# --- benchmark_physics2d*: harness A/B reproduzível do item 4.1.6 ---------
# Estes alvos ficam fora de NOT ANDROID para que os dois runners isolados
# também possam ser cross-compilados pelo NDK, enviados por ADB e medidos nos
# perfis móveis B/C. Não fazem parte da suíte de correção `aether_tests`.
set(AETHER_PHYSICS2D_BENCHMARK_SOURCE
  ${CMAKE_SOURCE_DIR}/../tests/native/benchmark_physics2d.cpp
)

add_executable(benchmark_physics2d ${AETHER_PHYSICS2D_BENCHMARK_SOURCE})
target_link_libraries(benchmark_physics2d PRIVATE aether_core aether_physics box2d::box2d)

add_executable(benchmark_physics2d_jolt ${AETHER_PHYSICS2D_BENCHMARK_SOURCE})
target_compile_definitions(benchmark_physics2d_jolt PRIVATE AETHER_BENCHMARK_JOLT_ONLY=1)
target_link_libraries(benchmark_physics2d_jolt PRIVATE aether_core aether_physics)

add_executable(benchmark_physics2d_box2d ${AETHER_PHYSICS2D_BENCHMARK_SOURCE})
target_compile_definitions(benchmark_physics2d_box2d PRIVATE AETHER_BENCHMARK_BOX2D_ONLY=1)
target_link_libraries(benchmark_physics2d_box2d PRIVATE box2d::box2d)

foreach(benchmark_target IN ITEMS
    benchmark_physics2d
    benchmark_physics2d_jolt
    benchmark_physics2d_box2d)
  target_compile_options(${benchmark_target} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-O2>)
endforeach()

if(WIN32)
  target_link_libraries(benchmark_physics2d PRIVATE psapi)
  target_link_libraries(benchmark_physics2d_jolt PRIVATE psapi)
  target_link_libraries(benchmark_physics2d_box2d PRIVATE psapi)
endif()
