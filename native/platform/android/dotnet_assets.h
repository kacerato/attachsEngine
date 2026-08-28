#pragma once

struct ANativeActivity;

namespace ae::platform::android {

// Copia o runtime .NET vendorizado (assemblies gerenciados + BCL + manifestos
// do framework compartilhado, empacotados como assets do APK em
// assets gerados pelo Gradle, com BCL vendorizada) para um diretório real —
// necessário porque AAssetManager não expõe caminho de arquivo utilizável
// por hostfxr/dlopen, só streams (ver AAsset_openFileDescriptor, que também
// não serve aqui: hostfxr espera um PATH de diretório, não um descritor de
// um único arquivo). Extraído para `<internalDataPath>/dotnet/`, guiado por
// dotnet_manifest.txt (lista de paths relativos
// gerada em tempo de build a partir da árvore vendorizada — AAssetManager não
// tem uma API de listagem recursiva confiável para descobrir isso sozinho).
//
// Idempotente por SHA-256 do build + tamanho dos arquivos. Mudança de conteúdo,
// mesmo com tamanho igual, invalida a extração. Substituições são atômicas;
// .build-id é confirmado por último. Startup monothread antes de carregar CoreCLR.
//
// `outDotnetRoot` recebe o caminho de `<internalDataPath>/dotnet` em caso de
// sucesso (usado por DotNetHost::initialize como base de runtimeConfigPath/
// managedAssemblyPath/dotnetRoot).
bool ensureDotNetAssetsExtracted(ANativeActivity *activity, char *outDotnetRoot,
                                 int outDotnetRootSize);

} // namespace ae::platform::android
