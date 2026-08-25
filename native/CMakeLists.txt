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
    ${CMAKE_SOURCE_DIR}/../tests/native/test_joint_bridge.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_query_bridge.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_character_bridge.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_physics2d_bridge.cpp
    ${CMAKE_SOURCE_DIR}/../tests/native/test_determinism.cpp
  )
  target_include_directories(aether_tests PRIVATE ${CMAKE_SOURCE_DIR}/../tests/native)
  target_link_libraries(aether_tests PRIVATE aether_core aether_platform aether_rendergraph aether_rhi aether_physics)

  # --- benchmark_physics2d: evidência de throughput para a decisão do item 4.1.6 (não é um
  # AE_TEST de correção — imprime uma tabela de corpos/step-time e sai, não faz parte da
  # suíte de "compila e passa em teste" que aether_tests roda). -O2 explícito: sem otimização
  # (o Debug default do projeto) o número medido não reflete o custo real em produção.
  add_executable(benchmark_physics2d
    ${CMAKE_SOURCE_DIR}/../tests/native/benchmark_physics2d.cpp
  )
  target_include_directories(benchmark_physics2d PRIVATE ${CMAKE_SOURCE_DIR}/../tests/native)
  target_link_libraries(benchmark_physics2d PRIVATE aether_core aether_physics)
  target_compile_options(benchmark_physics2d PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-O2>)
endif()
