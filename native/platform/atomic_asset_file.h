#pragma once
#include <cstddef>
#include <cstdint>

namespace ae::platform {
// Startup/IO only, single writer per path. Reader returns bytes, 0 at EOF, -1 on error.
using AssetRead = int (*)(void *context, void *buffer, size_t capacity);
bool replaceAssetFile(const char *path, uint64_t expectedBytes, AssetRead read, void *context);
bool validAssetBuildId(const char *id, size_t length);
} // namespace ae::platform
