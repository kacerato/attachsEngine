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
inline constexpr u32 TerrainSurfaceMaximumRows=262144;
}
