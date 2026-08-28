#include "platform/atomic_asset_file.h"
#include <cstdio>
#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace ae::platform {
bool validAssetBuildId(const char *id, size_t length) {
  if (id == nullptr || length != 64) return false;
  for (size_t i = 0; i < length; ++i)
    if (!((id[i] >= '0' && id[i] <= '9') || (id[i] >= 'a' && id[i] <= 'f'))) return false;
  return true;
}

bool replaceAssetFile(const char *path, uint64_t expectedBytes, AssetRead read, void *context) {
  if (path == nullptr || *path == '\0' || read == nullptr) return false;
  char temporary[1024];
  const int length = std::snprintf(temporary, sizeof(temporary), "%s.aether-tmp", path);
  if (length <= 0 || static_cast<size_t>(length) >= sizeof(temporary)) return false;
  FILE *output = std::fopen(temporary, "wb");
  if (output == nullptr) return false;
  char buffer[65536];
  uint64_t total = 0;
  bool ok = true;
  for (;;) {
    const int count = read(context, buffer, sizeof(buffer));
    if (count == 0) break;
    if (count < 0 || static_cast<size_t>(count) > sizeof(buffer) ||
        static_cast<uint64_t>(count) > expectedBytes - total) { ok = false; break; }
    total += static_cast<uint64_t>(count);
    if (std::fwrite(buffer, 1, static_cast<size_t>(count), output) != static_cast<size_t>(count)) {
      ok = false; break;
    }
  }
  if (total != expectedBytes || std::fflush(output) != 0) ok = false;
#if !defined(_WIN32)
  if (ok && fsync(fileno(output)) != 0) ok = false;
#endif
  if (std::fclose(output) != 0) ok = false;
  if (ok) {
#if defined(_WIN32)
    ok = MoveFileExA(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    ok = std::rename(temporary, path) == 0;
#endif
  }
  if (!ok) std::remove(temporary);
  return ok;
}
} // namespace ae::platform
