#pragma once

struct ANativeActivity;

namespace ae::platform::android {

// `ANativeActivity` (native_activity.h) expõe internalDataPath/externalDataPath
// nativamente, mas não nativeLibraryDir — só existe do lado Java em
// ApplicationInfo. `getNativeLibraryDir` faz a única chamada JNI necessária
// (context.getApplicationInfo().nativeLibraryDir) para obter o diretório onde
// o Android extraiu os .so do APK, incluindo os vendorizados em
// native/third_party/dotnet-runtime/ — é a base de onde `DotNetHost` localiza
// libhostfxr.so (ver dotnet_host.h).
//
// Escreve o resultado em `outBuffer` (tamanho `outBufferSize`), terminado em
// nul. Devolve false em qualquer falha (JNI indisponível, exceção Java,
// buffer pequeno demais) — nunca lança, mesmo padrão de retorno do resto do
// shell.
bool getNativeLibraryDir(ANativeActivity *activity, char *outBuffer, int outBufferSize);

} // namespace ae::platform::android
