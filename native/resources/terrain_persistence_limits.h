#pragma once
#include "core/base.h"
namespace ae::resources {
inline constexpr u64 TerrainRecipeLegacyMaximumBytes=8ull*1048576;
inline constexpr u64 TerrainFxMetadataMaximumBytes=64ull*1048576;
inline constexpr u64 TerrainRecipeMaximumBytes=TerrainRecipeLegacyMaximumBytes+TerrainFxMetadataMaximumBytes+1024;
inline constexpr u64 TerrainRecipeGlbMaximumJsonBytes=2*TerrainRecipeMaximumBytes+TerrainRecipeLegacyMaximumBytes;
inline constexpr u64 TerrainSourceMaximumBytes=256ull*1048576;
}
