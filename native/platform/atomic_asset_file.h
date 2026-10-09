#pragma once
#include <cstddef>
#include <cstdint>

namespace ae::platform {
// Startup/IO only, single writer per path. Reader returns bytes, 0 at EOF, -1 on error.
// Paths are UTF-8 on every platform, including Windows.
using AssetRead = int (*)(void *context, void *buffer, size_t capacity);
enum class AssetFileStage : uint8_t { None, Arguments, Path, Open, Read, Write, Flush, Sync, Close, Publish };
struct AssetFileDiagnostic {AssetFileStage stage=AssetFileStage::None;int code=0;};
bool replaceAssetFile(const char *path, uint64_t expectedBytes, AssetRead read, void *context);
bool replaceAssetFile(const char *path, uint64_t expectedBytes, AssetRead read, void *context,AssetFileDiagnostic *diagnostic);
bool validAssetBuildId(const char *id, size_t length);
} // namespace ae::platform
