#include "platform/android/dotnet_host.h"

#include <android/log.h>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>

#include "coreclr_delegates.h"
#include "hostfxr.h"

namespace ae::platform::android {

namespace {
constexpr const char *LogTag = "Aether.Android";

using hostfxr_initialize_for_runtime_config_fn_t = int32_t (*)(
    const char_t *runtime_config_path, const hostfxr_initialize_parameters *parameters,
    hostfxr_handle *host_context_handle);
using hostfxr_get_runtime_delegate_fn_t = int32_t (*)(
    const hostfxr_handle host_context_handle, hostfxr_delegate_type type, void **delegate);
using hostfxr_close_fn_t = int32_t (*)(const hostfxr_handle host_context_handle);
} // namespace

DotNetHost::~DotNetHost() {
  shutdown();
}

bool DotNetHost::initialize(const char *nativeLibraryDir, const char *dotnetRoot,
                            const char *runtimeConfigPath, const char *managedAssemblyPath) {
  if (nativeLibraryDir == nullptr || dotnetRoot == nullptr || runtimeConfigPath == nullptr ||
      managedAssemblyPath == nullptr || isReady()) {
    return false;
  }
  if (std::strlen(managedAssemblyPath) >= sizeof(managedAssemblyPath_)) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "managedAssemblyPath excede o buffer interno.");
    return false;
  }

  // libhostfxr.so precisa estar carregado ANTES de libhostpolicy.so/libcoreclr.so, mas nunca
  // abrimos essas duas diretamente — hostfxr as localiza e carrega sozinho a partir do
  // .deps.json/.runtimeconfig.json resolvido, todas do mesmo diretório (o nativeLibraryDir onde
  // o Android extraiu os .so do APK; ver native/third_party/dotnet-runtime/README.md).
  char hostfxrPath[1024];
  int written = std::snprintf(hostfxrPath, sizeof(hostfxrPath), "%s/libhostfxr.so", nativeLibraryDir);
  if (written <= 0 || static_cast<size_t>(written) >= sizeof(hostfxrPath)) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Caminho de libhostfxr.so excede o buffer interno.");
    return false;
  }

  hostfxrLibrary_ = dlopen(hostfxrPath, RTLD_NOW | RTLD_LOCAL);
  if (hostfxrLibrary_ == nullptr) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "dlopen(%s) falhou: %s", hostfxrPath, dlerror());
    return false;
  }

  auto initForConfig = reinterpret_cast<hostfxr_initialize_for_runtime_config_fn_t>(
      dlsym(hostfxrLibrary_, "hostfxr_initialize_for_runtime_config"));
  auto getRuntimeDelegate = reinterpret_cast<hostfxr_get_runtime_delegate_fn_t>(
      dlsym(hostfxrLibrary_, "hostfxr_get_runtime_delegate"));
  if (initForConfig == nullptr || getRuntimeDelegate == nullptr) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag,
                        "dlsym falhou em resolver os símbolos hostfxr esperados: %s", dlerror());
    shutdown();
    return false;
  }

  hostfxr_initialize_parameters initParameters{};
  initParameters.size = sizeof(initParameters);
  initParameters.host_path = nullptr; // não somos o apphost, hostfxr não precisa disso aqui
  initParameters.dotnet_root = dotnetRoot;

  int32_t result = initForConfig(runtimeConfigPath, &initParameters, &hostContextHandle_);
  // Success = 0, Success_HostAlreadyInitialized = 1, Success_DifferentRuntimeProperties = 2 —
  // os três são inicializações válidas (ver comentário de hostfxr_initialize_for_runtime_config_fn
  // em hostfxr.h); qualquer outro valor (positivo alto ou negativo) é erro real.
  if (result < 0 || result > 2) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag,
                        "hostfxr_initialize_for_runtime_config falhou (código 0x%x) para %s.",
                        result, runtimeConfigPath);
    shutdown();
    return false;
  }

  void *delegatePointer = nullptr;
  result = getRuntimeDelegate(hostContextHandle_, hdt_load_assembly_and_get_function_pointer,
                              &delegatePointer);
  if (result != 0 || delegatePointer == nullptr) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag,
                        "hostfxr_get_runtime_delegate falhou (código 0x%x) — o CoreCLR não foi carregado.",
                        result);
    shutdown();
    return false;
  }

  loadAssemblyAndGetFunctionPointer_ = delegatePointer;
  std::strcpy(managedAssemblyPath_, managedAssemblyPath); // tamanho já checado acima
  __android_log_print(ANDROID_LOG_INFO, LogTag, "CoreCLR carregado com sucesso via hostfxr (%s).",
                      runtimeConfigPath);
  return true;
}

void *DotNetHost::getManagedFunctionPointer(const char *typeName, const char *methodName) {
  if (!isReady() || typeName == nullptr || methodName == nullptr) return nullptr;

  auto loadAssemblyAndGetFunctionPointer =
      reinterpret_cast<load_assembly_and_get_function_pointer_fn>(loadAssemblyAndGetFunctionPointer_);

  void *methodPointer = nullptr;
  int32_t result = loadAssemblyAndGetFunctionPointer(
      managedAssemblyPath_, typeName, methodName, UNMANAGEDCALLERSONLY_METHOD, nullptr,
      &methodPointer);
  if (result != 0 || methodPointer == nullptr) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag,
                        "Falha ao resolver %s.%s (código 0x%x) — verifique se o método é "
                        "public static e tem [UnmanagedCallersOnly].",
                        typeName, methodName, result);
    return nullptr;
  }
  return methodPointer;
}

void DotNetHost::shutdown() {
  // hostfxr_close encerra só o CONTEXTO DE INICIALIZAÇÃO — a spec de hostfxr.h é explícita que
  // isso não descarrega o CoreCLR já carregado (não existe unload de runtime na API pública).
  // Chamamos mesmo assim por higiene do handle, mas isReady()/os ponteiros de delegate
  // continuam válidos mesmo depois — só zeramos o que É seguro reinicializar (a checagem de
  // isReady() em initialize() já impede reentrância).
  if (hostContextHandle_ != nullptr && hostfxrLibrary_ != nullptr) {
    auto close = reinterpret_cast<hostfxr_close_fn_t>(dlsym(hostfxrLibrary_, "hostfxr_close"));
    if (close != nullptr) close(hostContextHandle_);
    hostContextHandle_ = nullptr;
  }
  if (hostfxrLibrary_ != nullptr) {
    // Deliberadamente NÃO chamamos dlclose aqui: descarregar libhostfxr.so depois que o CoreCLR
    // foi inicializado através dele tem comportamento não documentado/potencialmente inseguro
    // (o próprio guia de hospedagem nativa da Microsoft não cobre dlclose pós-init) — vazar o
    // handle da lib pelo tempo de vida do processo é o comportamento seguro conhecido, o mesmo
    // que hosts de referência (dotnet apphost) fazem implicitamente ao nunca descarregar.
    hostfxrLibrary_ = nullptr;
  }
  loadAssemblyAndGetFunctionPointer_ = nullptr;
  managedAssemblyPath_[0] = '\0';
}

} // namespace ae::platform::android
