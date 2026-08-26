# .NET runtime para Android (vendorizado)

Subconjunto do runtime .NET 8 (CoreCLR) compilado para `linux-bionic-arm64`
(Android arm64, libc bionic) — usado para hospedar C# gerenciado dentro do
processo nativo do shell Android (item 0.1.4 do plano), via a API oficial de
hospedagem `hostfxr`.

## O que está aqui e por quê

O layout de `android-arm64/` replica exatamente o que uma instalação `dotnet`
normal tem em `shared/Microsoft.NETCore.App/<versão>/` — não é arbitrário:
`hostfxr` recusa a inicialização se essa estrutura de diretório e os dois
arquivos `.json` de manifesto não existirem, mesmo com todos os `.so`
presentes (descoberto por tentativa e erro validando em hardware real — ver
`ESTADO.md`, item 0.1.4, para a mensagem de erro exata que motivou isso).

- `android-arm64/native/libhostfxr.so` — o único binário aberto diretamente
  via `dlopen` (por `DotNetHost::initialize`, caminho conhecido dentro do
  `nativeLibraryDir` do APK). Resolve a versão do runtime e inicializa o host
  a partir de um `.runtimeconfig.json`.
- `android-arm64/shared/Microsoft.NETCore.App/8.0.27/` — o "framework
  compartilhado" que `hostfxr` localiza via `hostfxr_initialize_parameters
  .dotnet_root` (não por variável de ambiente/registro global, que não
  existem dentro do processo de um app Android):
  - `Microsoft.NETCore.App.deps.json`/`.runtimeconfig.json` — manifestos que
    `hostfxr` usa para RECONHECER o diretório como uma instalação de
    framework válida. Sem eles: "No frameworks were found", mesmo com
    `libcoreclr.so` presente.
  - `libhostpolicy.so` — resolve dependências (`.deps.json` do app) e carrega
    o CoreCLR de fato. Carregado indiretamente por `libhostfxr.so`.
  - `libcoreclr.so` — o runtime de execução gerenciada (JIT, GC, carregador
    de assembly). Carregado indiretamente por `libhostpolicy.so`.
  - `libSystem.*.so` — P/Invokes nativos que a Base Class Library usa para
    I/O, globalização e criptografia.
- `include/hostfxr.h`, `include/coreclr_delegates.h` — headers C oficiais da
  Microsoft para o fluxo `hostfxr_initialize_for_runtime_config` →
  `hostfxr_get_runtime_delegate` → `load_assembly_and_get_function_pointer`,
  usados por `native/platform/android/dotnet_host.cpp`.

## Publicação do lado gerenciado (Aether.Core.dll)

`hostfxr_initialize_for_runtime_config` recusa um `.runtimeconfig.json`
**self-contained** ("Initialization for self-contained components is not
supported", erro confirmado em hardware) — o assembly gerenciado precisa ser
publicado **framework-dependent**:

```bash
dotnet publish managed/Aether.Core/Aether.Core.csproj -c Release \
  -r linux-bionic-arm64 --self-contained false \
  -p:GenerateRuntimeConfigurationFiles=true -o <saída>
```

`--self-contained false` é o que faz o `.runtimeconfig.json` gerado usar
`"framework": { "name": "Microsoft.NETCore.App", "version": "8.0.0" }` em vez
de `"includedFrameworks"` — só o primeiro formato é aceito pelo fluxo de
hospedagem de componente que `DotNetHost` usa.

## O que NÃO está aqui, de propósito

- A Base Class Library gerenciada (`System.*.dll`) e os assemblies do próprio
  projeto (`Aether.Core.dll` etc.) — esses são artefato de **build**
  (`dotnet publish`), não binário de terceiros vendorizado. Empacotados pelo
  Gradle a partir da saída do publish, nunca commitados aqui — mesma
  disciplina de `bin/`/`obj/` no `.gitignore`.
- `apphost`/`libnethost.*` — resolvem o hostfxr via variável de ambiente ou
  registro global do SDK, que não existem dentro do processo de um app
  Android. O shell abre `libhostfxr.so` direto por caminho conhecido dentro
  do `nativeLibraryDir` do APK (ver `dotnet_host.cpp`), não usa `nethost`.

## Origem e licença

Ver `VENDORED_COMMIT.txt` para os pacotes NuGet exatos e a data. Distribuído
sob a licença MIT da Microsoft — ver `LICENSE.TXT`.
