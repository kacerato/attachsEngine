#include "platform/android/dotnet_assets.h"

#include <android/asset_manager.h>
#include <android/log.h>
#include <android/native_activity.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <vector>

namespace ae::platform::android {

namespace {
constexpr const char *LogTag = "Aether.Android";
constexpr const char *ManifestAssetPath = "dotnet_manifest.txt";
constexpr const char *AssetSourcePrefix = "dotnet/"; // ver android/app/src/main/assets/dotnet/

// mkdir -p mínimo: cria cada segmento do caminho, ignorando EEXIST — os
// manifestos de asset têm profundidade fixa e pequena (shared/Microsoft.
// NETCore.App/8.0.27/...), então uma varredura simples por '/' é suficiente,
// sem precisar de uma lib de path.
bool makeDirectoriesRecursive(char *path) {
  for (char *cursor = path + 1; *cursor != '\0'; ++cursor) {
    if (*cursor != '/') continue;
    *cursor = '\0';
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
      __android_log_print(ANDROID_LOG_ERROR, LogTag, "mkdir(%s) falhou: %s", path, strerror(errno));
      *cursor = '/';
      return false;
    }
    *cursor = '/';
  }
  // O loop acima só cria diretórios em cada '/' interno — o segmento final
  // (depois do último '/', que é o próprio `path` completo) nunca é
  // alcançado pelo `*cursor != '\0'` da condição de parada. Sem esta
  // chamada, o último nível do caminho nunca é criado e todo fopen()
  // subsequente falha com ENOENT (bug real, pego só ao rodar em hardware).
  if (mkdir(path, 0755) != 0 && errno != EEXIST) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "mkdir(%s) falhou: %s", path, strerror(errno));
    return false;
  }
  return true;
}

bool copyOneAsset(AAssetManager *assetManager, const char *relativePath,
                  const char *destinationRoot) {
  char assetPath[512];
  int written = std::snprintf(assetPath, sizeof(assetPath), "%s%s", AssetSourcePrefix, relativePath);
  if (written <= 0 || static_cast<size_t>(written) >= sizeof(assetPath)) return false;

  AAsset *asset = AAssetManager_open(assetManager, assetPath, AASSET_MODE_STREAMING);
  if (asset == nullptr) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "AAssetManager_open falhou para %s", assetPath);
    return false;
  }

  char destinationPath[512];
  written = std::snprintf(destinationPath, sizeof(destinationPath), "%s/%s", destinationRoot, relativePath);
  if (written <= 0 || static_cast<size_t>(written) >= sizeof(destinationPath)) {
    AAsset_close(asset);
    return false;
  }

  const off_t assetLength = AAsset_getLength(asset);

  // Já extraído com o tamanho certo numa execução anterior — pula, extração
  // completa de ~28 MB só precisa acontecer uma vez por instalação/atualização
  // do APK, não a cada abertura do app.
  struct stat existing{};
  if (stat(destinationPath, &existing) == 0 && existing.st_size == assetLength) {
    AAsset_close(asset);
    return true;
  }

  char directoryPath[512];
  std::strncpy(directoryPath, destinationPath, sizeof(directoryPath) - 1);
  directoryPath[sizeof(directoryPath) - 1] = '\0';
  char *lastSlash = std::strrchr(directoryPath, '/');
  if (lastSlash != nullptr) {
    *lastSlash = '\0';
    if (!makeDirectoriesRecursive(directoryPath)) {
      AAsset_close(asset);
      return false;
    }
  }

  FILE *outFile = std::fopen(destinationPath, "wb");
  if (outFile == nullptr) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "fopen(%s) falhou: %s", destinationPath,
                        strerror(errno));
    AAsset_close(asset);
    return false;
  }

  char buffer[64 * 1024];
  int readBytes;
  bool ok = true;
  while ((readBytes = AAsset_read(asset, buffer, sizeof(buffer))) > 0) {
    if (std::fwrite(buffer, 1, static_cast<size_t>(readBytes), outFile) !=
        static_cast<size_t>(readBytes)) {
      ok = false;
      break;
    }
  }
  if (readBytes < 0) ok = false;

  std::fclose(outFile);
  AAsset_close(asset);
  if (!ok) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao copiar asset %s para %s.", assetPath,
                        destinationPath);
  }
  return ok;
}
} // namespace

bool ensureDotNetAssetsExtracted(ANativeActivity *activity, char *outDotnetRoot,
                                 int outDotnetRootSize) {
  if (activity == nullptr || activity->assetManager == nullptr ||
      activity->internalDataPath == nullptr || outDotnetRoot == nullptr ||
      outDotnetRootSize <= 0) {
    return false;
  }

  int written = std::snprintf(outDotnetRoot, outDotnetRootSize, "%s/dotnet", activity->internalDataPath);
  if (written <= 0 || written >= outDotnetRootSize) return false;

  if (!makeDirectoriesRecursive(outDotnetRoot)) return false;

  AAsset *manifest =
      AAssetManager_open(activity->assetManager, ManifestAssetPath, AASSET_MODE_BUFFER);
  if (manifest == nullptr) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Manifesto de assets .NET (%s) não encontrado.",
                        ManifestAssetPath);
    return false;
  }

  const off_t manifestLength = AAsset_getLength(manifest);
  const void *manifestBuffer = AAsset_getBuffer(manifest);
  if (manifestBuffer == nullptr || manifestLength <= 0) {
    AAsset_close(manifest);
    return false;
  }

  // O manifesto é uma lista de paths relativos separados por '\n' (gerada em
  // tempo de build a partir da árvore vendorizada — ver dotnet_assets.h).
  // Copiamos para um buffer próprio porque AAsset_getBuffer não é
  // nul-terminated e precisamos tokenizar em memória gravável.
  std::vector<char> manifestCopy(static_cast<size_t>(manifestLength) + 1);
  std::memcpy(manifestCopy.data(), manifestBuffer, static_cast<size_t>(manifestLength));
  manifestCopy[static_cast<size_t>(manifestLength)] = '\0';
  AAsset_close(manifest);

  int copiedCount = 0;
  int failedCount = 0;
  char *line = std::strtok(manifestCopy.data(), "\n\r");
  while (line != nullptr) {
    if (*line != '\0') {
      if (copyOneAsset(activity->assetManager, line, outDotnetRoot)) {
        ++copiedCount;
      } else {
        ++failedCount;
      }
    }
    line = std::strtok(nullptr, "\n\r");
  }

  __android_log_print(ANDROID_LOG_INFO, LogTag,
                      "Extração de assets .NET concluída: %d ok, %d falharam.", copiedCount,
                      failedCount);
  return failedCount == 0;
}

} // namespace ae::platform::android
