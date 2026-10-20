#pragma once
#include "resources/terrain_fx_asset.h"
#include <functional>
#include <span>
namespace ae::resources {
struct TerrainFxDirtyRect {
  u32 x0=0,z0=0,x1=0,z1=0; // half-open texel rectangle; empty if either span is zero
  bool empty()const{return x0>=x1||z0>=z1;}
  bool operator==(const TerrainFxDirtyRect &)const=default;
};
struct TerrainFxField {
  u32 width=0,height=0;
  TerrainFxDomain domain;
  std::vector<std::array<float,4>> values; // authoring precision; never normalize RGBA
};
// slope is required only for filtered strokes and is the same-size normalized
// source field. Work/cancellation/output are transactional; previous is read-only.
bool rasterTerrainFx(const TerrainFxAsset &,std::span<const float> slope,
  const TerrainFxField *previous,TerrainFxField &,TerrainFxDirtyRect &,std::string &,
  const TerrainFxLimits & = {},const std::function<bool(double)> &progress = {});
bool quantizeTerrainFx(const TerrainFxField &,std::vector<u8> &,std::string &,const TerrainFxLimits & = {});
// Commit captured authoring bytes without dropping the full stroke log. Caller
// checks job revision/generation before committing this returned asset.
bool checkpointTerrainFx(const TerrainFxAsset &,const TerrainFxField &,TerrainFxAsset &,
  std::string &,const TerrainFxLimits & = {});
// Sample the exact published bytes using linear Clamp and pixel-center UV,
// matching GPU texture sampling, not the unquantized authoring approximation.
bool sampleTerrainFxBytes(std::span<const u8>,u32 width,u32 height,
  double u,double v,std::array<float,4> &);
TerrainFxDirtyRect terrainFxMipDirtyRect(TerrainFxDirtyRect,u32 width,u32 height,u32 mip);
}
