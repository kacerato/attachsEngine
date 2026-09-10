#pragma once

#include <cstdint>

namespace ae::platform::android {

// Hospeda o CoreCLR dentro do processo nativo do shell Android (item 0.1.4 do
// plano: "carregar CoreCLR/Mono, chamar C# do C++ e vice-versa"). Usa o fluxo
// oficial de hospedagem hostfxr — o mesmo que `dotnet app.dll` usa por baixo,
// mas dirigido a partir de C++ em vez de linha de comando (ver
// native/third_party/dotnet-runtime/README.md para os binários vendorizados
// e o porquê de não usar nethost/apphost).
//
// Ciclo de vida: DotNetHost é dono do host_context_handle do hostfxr e das
// bibliotecas dinâmicas carregadas via dlopen. Uma vez inicializado, o
// processo de hospedagem do CoreCLR não é desfeito por design da própria API
// (hostfxr_close encerra o contexto de inicialização, não descarrega o
// runtime) — shutdown() existe para o caso de falha durante initialize(),
// não para um ciclo normal de destruição do app.
class DotNetHost final {
public:
  DotNetHost() = default;
  ~DotNetHost();

  DotNetHost(const DotNetHost &) = delete;
  DotNetHost &operator=(const DotNetHost &) = delete;

  // `nativeLibraryDir` é o diretório onde o Android extraiu os .so do APK
  // (ApplicationInfo.nativeLibraryDir do lado Java/Kotlin, repassado ao C++
  // via android_main) — é lá que libhostfxr.so acaba depois do empacotamento
  // Gradle. `dotnetRoot` é a raiz esperada pelo hostfxr no layout padrão
  // `shared/Microsoft.NETCore.App/<versão>/` (ver hostfxr_initialize_parameters
  // em hostfxr.h) — é ONDE libcoreclr.so, libhostpolicy.so, a BCL gerenciada
  // E os manifestos Microsoft.NETCore.App.deps.json/.runtimeconfig.json
  // precisam morar; sem os dois manifestos, hostfxr recusa o diretório com
  // "No frameworks were found" mesmo com todos os .so presentes — descoberto
  // em hardware real, não documentado de forma óbvia no header oficial. Ver
  // native/third_party/dotnet-runtime/README.md para o layout exato validado.
  // O assembly gerenciado precisa ser publicado framework-dependent (não
  // self-contained: a API de hospedagem de componente recusa runtimeconfig
  // self-contained, confirmado em execução real — "Initialization for
  // self-contained components is not supported"). `runtimeConfigPath`/
  // `managedAssemblyPath` apontam para o módulo raiz da composição atual
  // (Aether.Rendering.runtimeconfig.json / Aether.Rendering.dll). Suas
  // dependências Core/Scene são publicadas juntas, no mesmo contexto de carga.
  bool initialize(const char *nativeLibraryDir, const char *dotnetRoot,
                  const char *runtimeConfigPath, const char *managedAssemblyPath);
  void shutdown();
  bool isReady() const { return loadAssemblyAndGetFunctionPointer_ != nullptr; }

  // Stack-owned region for native event/render work. The vendored bionic VM is
  // Mono/SGen behind the CoreCLR hosting ABI and needs cooperative GC boundaries.
  // UnmanagedCallersOnly trampolines handle managed re-entry inside this region.
  class NativeRegion final {
  public:
    explicit NativeRegion(DotNetHost &host);
    ~NativeRegion();
    NativeRegion(const NativeRegion &) = delete;
    NativeRegion &operator=(const NativeRegion &) = delete;
  private:
    void *stackMarker_ = nullptr;
    void *cookie_ = nullptr;
    void (*exit_)(void *, void **) = nullptr;
  };

  // Resolve um ponteiro de função para um método gerenciado estático marcado
  // com [UnmanagedCallersOnly] (delegate_type_name = UNMANAGEDCALLERSONLY_METHOD
  // internamente — não expomos delegate customizado nesta primeira fatia,
  // sem marshaling automático, para o contrato ficar explícito nos dois lados
  // da fronteira, mesma disciplina de jolt_bridge.h). `typeName` é o nome
  // qualificado por assembly (ex.: "Aether.Interop.NativeEntryPoints,
  // Aether.Core"), `methodName` o método estático público.
  //
  // Devolve nullptr em falha — não lança, para o chamador decidir se a
  // ausência de um ponto de entrada específico é fatal ou apenas desativa
  // aquele caminho (mesmo padrão de retorno de handle inválido do resto do
  // shell, nunca exceção atravessando a fronteira C++/C#).
  void *getManagedFunctionPointer(const char *typeName, const char *methodName);

private:
  void *(*enterNative_)(void **) = nullptr;
  void (*exitNative_)(void *, void **) = nullptr;
  void *hostfxrLibrary_ = nullptr;
  void *hostContextHandle_ = nullptr;
  void *loadAssemblyAndGetFunctionPointer_ = nullptr; // load_assembly_and_get_function_pointer_fn
  // Cópia própria, não um ponteiro para o buffer do caller: getManagedFunctionPointer é chamado
  // muito depois de initialize() retornar (ex.: de InstancedRenderer::initialize, em outro
  // momento do lifecycle) — guardar só o ponteiro do caller é use-after-scope real, pego em
  // execução (o primeiro getManagedFunctionPointer funcionava porque a pilha do caller ainda não
  // tinha sido reutilizada; o segundo, mais tarde, lia lixo e falhava com "arquivo não encontrado").
  char managedAssemblyPath_[512] = {};
};

} // namespace ae::platform::android
