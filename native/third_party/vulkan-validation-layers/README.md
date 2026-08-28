# Vulkan Validation Layers para Android (vendorizado)

`libVkLayer_khronos_validation.so` (arm64-v8a) — usado pelo item 2.1.6 do
plano ("camadas de validação, marcadores de debug, captura de frame") para
ativar `VK_LAYER_KHRONOS_validation` em build debug.

## O que está aqui e por quê

- `arm64-v8a/libVkLayer_khronos_validation.so` — o binário oficial da camada
  de validação Khronos, compilado para Android. O nome do diretório segue a
  convenção de ABI que `jniLibs.srcDirs` do Gradle exige (`<srcDir>/<ABI>/*.so`),
  não é arbitrário. Empacotado no APK **debug** via `jniLibs.srcDirs` (ver
  `android/app/build.gradle.kts`, `buildTypes.debug`) — nunca no APK release;
  o código nativo em si (`native/rhi/device.cpp`) também só tenta habilitar a
  camada quando compilado sem `NDEBUG`, então mesmo que o `.so` vazasse para
  um build release por engano, a segunda checagem (build type nativo)
  continuaria desligando a validação.
- A camada precisa do próprio `.so` presente no `nativeLibraryDir` do app
  instalado para que `vkEnumerateInstanceLayerProperties`/`vkCreateInstance`
  a encontrem em runtime — não existe forma de "linkar" uma layer Vulkan
  estaticamente; o loader Vulkan do sistema operacional a descobre por nome
  a partir dos `.so` presentes no processo, o mesmo mecanismo usado por
  qualquer app Android que embuta layers de debug.
- `VkDevice`/`VkInstance` continuam consultando a camada via
  `vkEnumerateInstanceLayerProperties` antes de pedir para habilitá-la (nunca
  presumida disponível) — mesma disciplina já usada para extensões ASTC e
  `VK_EXT_descriptor_indexing` em `device.cpp`. Sem o `.so` (ex.: build
  release, ou alguém remove o binário), a checagem simplesmente falha e o
  shell sobe sem validation layer, sem erro fatal.

## O que NÃO está aqui, de propósito

- Variantes `armeabi-v7a`/`x86`/`x86_64` do asset original — o projeto builda
  só `arm64-v8a` (ver `abiFilters` em `android/app/build.gradle.kts`).
- `libVkLayer_khronos_validation.json` (o manifesto de layer usado em
  desktop/Windows/Linux) — no Android o loader do sistema descobre a camada
  só pelo nome do `.so` presente no APK, sem manifesto JSON separado.

## Origem e licença

Ver `VENDORED_COMMIT.txt` para o release exato, URL do asset e checksum.
Distribuído sob a licença Apache 2.0 do projeto `KhronosGroup/Vulkan-ValidationLayers`
— ver `LICENSE.TXT`.
