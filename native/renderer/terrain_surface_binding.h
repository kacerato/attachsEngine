#pragma once
#include "renderer/map_package.h"
#include "resources/terrain_surface_asset.h"

namespace ae::renderer {
// Source-local until publication; a GUID may have distinct linear/sRGB and
// normal-filtered instances. Never infer the role from the image filename.
struct TerrainSurfaceTextureBinding {
  resources::AssetGuid identity;
  bool linear=true, normal=false;
  u32 textureIndex=InvalidMapTexture;
};
struct TerrainSurfaceBinding {
  resources::TerrainSurfaceAsset surface;
  float localMetersPerUv=1;
  std::vector<TerrainSurfaceTextureBinding> textures;
};
// std430 vec4 rows. The first header contains layer count/version; the second
// contains local meters per stored UV. Each layer has five property rows and
// seven five-row map records. Texture indices in GPU rows are bindless slots.
inline constexpr u32 TerrainSurfaceHeaderRows=2;
inline constexpr u32 TerrainSurfaceLayerRows=40;
inline constexpr u32 TerrainSurfaceMapRows=5;
// Optional header0.z is an absolute advanced-header row plus one (zero=base).
// Advanced v1 header: version/layerCount/globalNoiseRow/layerDirectoryRow;
// reservedAntiTile/count/endExclusive/features (bit 0 = M05).
// Five seven-row noise slots: detail, distance, reserved normal0..2.
// Each layer directory occupies two rows followed by its five noise slots:
// localNoiseRow/participationRow/reserved/reserved; globalDetail/globalDistance/0/0.
// All directory pointers are absolute; per-layer stride is 37 rows.
inline constexpr u32 TerrainSurfaceAdvancedVersion=1;
inline constexpr u32 TerrainSurfaceNoiseRows=7;
inline constexpr u32 TerrainSurfaceAdvancedGlobalRows=37;
inline constexpr u32 TerrainSurfaceAdvancedLayerRows=37;
inline constexpr u32 TerrainSurfaceMaximumRows=262144;
}
