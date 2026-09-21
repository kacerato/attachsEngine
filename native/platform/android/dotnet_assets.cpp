#include "platform/android/dotnet_assets.h"
#include "platform/atomic_asset_file.h"

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

int readAsset(void *context, void *buffer, size_t capacity) {
  return AAsset_read(static_cast<AAsset *>(context), buffer, capacity);
}

// mkdir -p mínimo: cria cada segmento do caminho, ignorando EEXIST — os
// manifestos de asset têm profundidade fixa e pequena (shared/Microsoft.
// NETCore.App/<versão>/...), então uma varredura simples por '/' é suficiente,
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
                  const char *destinationRoot, bool sameBuild) {
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

  // Equal length alone does not identify an assembly. The build digest changes
  // even for a same-size edit; commit it only after the entire extraction succeeds.
  struct stat existing{};
  if (sameBuild && stat(destinationPath, &existing) == 0 && existing.st_size == assetLength) {
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

  const bool ok = replaceAssetFile(destinationPath, static_cast<uint64_t>(assetLength), readAsset, asset);
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

  char buildId[65]{};
  AAsset *buildAsset = AAssetManager_open(activity->assetManager, "dotnet_build_id.txt", AASSET_MODE_BUFFER);
  if (buildAsset == nullptr) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Build ID .NET ausente; APK deve ser reconstruído.");
    return false;
  }
  const bool validBuild = AAsset_getLength(buildAsset) == 64 && AAsset_read(buildAsset, buildId, 64) == 64 &&
      validAssetBuildId(buildId, 64);
  if (!validBuild) { AAsset_close(buildAsset); return false; }
  char markerPath[512];
  written = std::snprintf(markerPath, sizeof(markerPath), "%s/.build-id", outDotnetRoot);
  if (written <= 0 || static_cast<size_t>(written) >= sizeof(markerPath)) { AAsset_close(buildAsset); return false; }
  char previousId[65]{};
  FILE *marker = std::fopen(markerPath, "rb");
  bool sameBuild = false;
  if (marker != nullptr) {
    sameBuild = std::fread(previousId, 1, sizeof(previousId), marker) == 64 &&
        std::memcmp(previousId, buildId, 64) == 0;
    std::fclose(marker);
  }
  AAsset_close(buildAsset);

  // Invalidate before the first replacement: an interrupted upgrade followed by
  // an APK rollback must not accept a mixture of old/new files with equal sizes.
  if (!sameBuild && std::remove(markerPath) != 0 && errno != ENOENT) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao invalidar build .NET anterior: %s", strerror(errno));
    return false;
  }

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
      if (copyOneAsset(activity->assetManager, line, outDotnetRoot, sameBuild)) {
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
  if (failedCount != 0 || copiedCount == 0) return false;
  buildAsset = AAssetManager_open(activity->assetManager, "dotnet_build_id.txt", AASSET_MODE_STREAMING);
  if (buildAsset == nullptr) return false;
  const bool committed = sameBuild || replaceAssetFile(markerPath, 64, readAsset, buildAsset);
  AAsset_close(buildAsset);
  if (!committed) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao confirmar build .NET extraído.");
    return false;
  }
  __android_log_print(ANDROID_LOG_INFO, LogTag, "[ManagedBuild] id=%s reused=%d", buildId, sameBuild ? 1 : 0);
  return true;
}

} // namespace ae::platform::android
