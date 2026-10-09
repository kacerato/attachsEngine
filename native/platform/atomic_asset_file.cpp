#include "platform/atomic_asset_file.h"
#include <cstdio>
#include <cerrno>
#if defined(_WIN32)
#include <windows.h>
#include <io.h>
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
  return replaceAssetFile(path,expectedBytes,read,context,nullptr);
}
bool replaceAssetFile(const char *path, uint64_t expectedBytes, AssetRead read, void *context,AssetFileDiagnostic *diagnostic) {
  if(diagnostic)*diagnostic={};
  const auto fail=[&](AssetFileStage stage,int code) {if(diagnostic&&diagnostic->stage==AssetFileStage::None)*diagnostic={stage,code};return false;};
  if (path == nullptr || *path == '\0' || read == nullptr) return fail(AssetFileStage::Arguments,EINVAL);
  char temporary[1024];
  const int length = std::snprintf(temporary, sizeof(temporary), "%s.aether-tmp", path);
  if (length <= 0 || static_cast<size_t>(length) >= sizeof(temporary)) return fail(AssetFileStage::Path,ENAMETOOLONG);
#if defined(_WIN32)
  wchar_t targetWide[1024],temporaryWide[1024];
  if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,targetWide,1024) ||
     !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,temporary,-1,temporaryWide,1024)) return fail(AssetFileStage::Path,static_cast<int>(GetLastError()));
  FILE *output = _wfopen(temporaryWide,L"wb");
#else
  FILE *output = std::fopen(temporary, "wb");
#endif
  if (output == nullptr) return fail(AssetFileStage::Open,errno);
  char buffer[65536];
  uint64_t total = 0;
  bool ok = true;
  for (;;) {
    const int count = read(context, buffer, sizeof(buffer));
    if (count == 0) break;
    if (count < 0 || static_cast<size_t>(count) > sizeof(buffer) ||
        static_cast<uint64_t>(count) > expectedBytes - total) { ok = fail(AssetFileStage::Read,count); break; }
    total += static_cast<uint64_t>(count);
    if (std::fwrite(buffer, 1, static_cast<size_t>(count), output) != static_cast<size_t>(count)) {
      ok = fail(AssetFileStage::Write,errno); break;
    }
  }
  if (total != expectedBytes)ok=fail(AssetFileStage::Read,EINVAL);
  if (std::fflush(output) != 0)ok=fail(AssetFileStage::Flush,errno);
#if defined(_WIN32)
  if(ok&&!FlushFileBuffers(reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(output)))))ok=fail(AssetFileStage::Sync,static_cast<int>(GetLastError()));
#else
  if (ok && fsync(fileno(output)) != 0) ok = fail(AssetFileStage::Sync,errno);
#endif
  if (std::fclose(output) != 0) ok = fail(AssetFileStage::Close,errno);
  if (ok) {
#if defined(_WIN32)
    ok = MoveFileExW(temporaryWide, targetWide, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if(!ok)fail(AssetFileStage::Publish,static_cast<int>(GetLastError()));
#else
    ok = std::rename(temporary, path) == 0;
    if(!ok)fail(AssetFileStage::Publish,errno);
#endif
  }
  if (!ok) {
#if defined(_WIN32)
    _wremove(temporaryWide);
#else
    std::remove(temporary);
#endif
  }
  return ok;
}
} // namespace ae::platform
