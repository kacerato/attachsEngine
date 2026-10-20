#pragma once
#include "resources/asset_registry.h"
#include "resources/terrain_persistence_limits.h"
#include <array>
#include <string>
#include <string_view>
#include <vector>
namespace ae::resources {
// Independent linear RGBA quantities; their sum is deliberately unrestricted.
enum class TerrainFxChannel:u32 { Wetness,Puddle,Stream,Lava };
enum class TerrainFxBrushShape:u32 { Circle,Box };
struct TerrainFxLimits {
  u32 maximumDimension=2048,maximumStrokes=65536;
  u64 maximumPixels=4ull*1024*1024,maximumWorkingBytes=96ull*1024*1024;
  u64 maximumMetadataBytes=TerrainFxMetadataMaximumBytes,maximumRasterSamples=128ull*1024*1024;
};
struct TerrainFxDomain {
  double originX=-32,originZ=-32,sizeX=64,sizeZ=64;
  bool operator==(const TerrainFxDomain &)const=default;
};
struct TerrainFxStroke {
  u64 identity=0;
  u32 gesture=0;
  TerrainFxChannel channel=TerrainFxChannel::Wetness;
  TerrainFxBrushShape shape=TerrainFxBrushShape::Circle;
  double x=0,z=0,radiusMeters=4,aspect=1,rotationDegrees=0;
  double flow=8,falloff=1,pressure=1,sampleSeconds=1.0/60,target=1;
  // Astra brush extensions with real raster consumers. Defaults preserve the
  // source Target formula; source Flow is never reinterpreted as volume/sec.
  double hardness=0,opacity=1;
  bool slopeFilter=false;
  double slopeMin=0,slopeMax=1; // 1-clamp(dot(normal,up),0,1), not degrees
  bool operator==(const TerrainFxStroke &)const=default;
};
struct TerrainFxAsset {
  static constexpr u32 Version=1;
  bool enabled=false;
  u32 width=256,height=256;
  TerrainFxDomain domain;
  AssetGuid sourceImage;
  std::string sourcePropertiesJson;
  // Optional immutable starting bitmap, exact RGBA8 linear source samples.
  // Empty means all zero. Imported source bytes/GUID remain separate assets.
  std::vector<u8> seedPixels;
  std::vector<TerrainFxStroke> strokes;
  // Complete authoring log remains intact. Prefix is already represented by
  // seedPixels, so a later terrain sculpt must not reapply old slope filters.
  u32 bakedStrokeCount=0;
  std::string checkpointBaseHash,checkpointPrefixHash,checkpointPixelsHash;
};
// Canonical prefix binds transforms/domain/resolution, source GUID/provenance,
// original base content hash and every captured stroke property. Not a seal
// against malicious rewriting; it rejects accidental incompatible checkpoints.
std::string terrainFxPrefixHash(const TerrainFxAsset &,u32 prefixCount,std::string_view baseHash);
bool validTerrainFxAsset(const TerrainFxAsset &,std::string &,const TerrainFxLimits & = {});
bool serializeTerrainFxAsset(const TerrainFxAsset &,std::string &,std::string &,const TerrainFxLimits & = {});
bool deserializeTerrainFxAsset(std::string_view,TerrainFxAsset &,std::string &,const TerrainFxLimits & = {});
}
