#pragma once
#include "resources/asset_registry.h"
#include <array>
#include <functional>

namespace ae::resources {
// M01-M08 research: Unity 6000.0 TerrainLayer and supplied MicroSplat 3.9.0
// (internal package.json version; distribution filename says 3.9.41, as
// recorded in docs/terrain-plugin-audit-2026-10-15.md). Numeric defaults here
// are Astra's neutral authoring values, not imported MicroSplat presets.
// TextureArrayConfig contracts; catalog channel meanings are explicit, never
// inferred from filenames. A zero texture GUID means a constant-only channel.
// https://docs.unity3d.com/6000.0/Documentation/ScriptReference/TerrainLayer.html
// Contract version 1 is independent of legacy four-texture recipes: migration
// must assign persistent layer GUIDs and resolve catalog texture GUIDs before
// remapping control weights. Unsupported versions are rejected, never guessed.
enum class TerrainSurfaceRole : u32 {
  Albedo,
  Normal,
  Height,
  AO,
  Roughness,
  Metallic,
  Emission
};
enum class TerrainSurfaceChannel : u32 { R, G, B, A, RGB, NormalRGB, NormalRG };
enum class TerrainSurfaceQuality : u32 { Low, Medium, High };
enum class TerrainProjection : u32 { UV, LocalTriplanar, WorldTriplanar };
enum class TerrainDistanceBlend : u32 { Constant, Noise, Crossfade };
struct TerrainSurfaceMap {
  AssetGuid texture;
  TerrainSurfaceChannel channel = TerrainSurfaceChannel::R;
  bool linear = true, invert = false;
  float factor = 1, remapMin = 0, remapMax = 1;
  // Unity TerrainLayer diffuse remaps are RGBA, unlike scalar packed-mask
  // remaps. These remain separate so importing either loses no channels.
  std::array<float, 4> colorFactor{1, 1, 1, 1};
  std::array<float, 4> colorRemapMin{0, 0, 0, 0}, colorRemapMax{1, 1, 1, 1};
};
struct TerrainSurfaceNoise {
  bool enabled = false;
  TerrainSurfaceMap map;
  float scale = 1, strength = 1, fadeStart = 0, fadeEnd = 100;
  TerrainProjection projection = TerrainProjection::LocalTriplanar;
};
struct TerrainSurfaceVariant {
  AssetGuid identity;
  // albedo, normal, height, AO, roughness, metallic, emission.
  std::array<TerrainSurfaceMap, 7> maps;
  float scale = 1, offsetX = 0, offsetY = 0, rotation = 0;
};
// An authoring array entry is referenced by identity, never GPU slice number.
// The three source roles match catalog noiseNormal/detailNoise/distanceNoise.
struct TerrainAntiTileEntry {
  AssetGuid identity;
  TerrainSurfaceNoise normal, detail, distance;
};
struct TerrainAntiTileArray {
  bool enabled = false;
  std::vector<TerrainAntiTileEntry> entries;
};
struct TerrainSurfaceCluster {
  bool enabled = false, stochastic = false;
  std::vector<TerrainSurfaceVariant> variants;
  TerrainSurfaceNoise selection;
  float contrast = 1, noiseBoost = 0;
  TerrainSurfaceQuality quality = TerrainSurfaceQuality::Medium;
};
struct TerrainSurfaceLayer {
  AssetGuid identity;
  std::string name;
  // Lossless provenance survives selection and saving. Empty fields identify
  // materials authored directly in Astra. Each raw JSON field is <=64 KiB.
  std::string sourceCatalogId;
  std::string sourcePropertiesJson, sourcePackingJson, sourceResolvedMapsJson;
  bool active = true;
  AssetGuid thumbnail;
  std::array<TerrainSurfaceMap, 7> maps;
  float scaleX = 1, scaleY = 1, offsetX = 0, offsetY = 0, rotation = 0;
  std::array<float, 4> tint{1, 1, 1, 1};
  float saturation = 1;
  TerrainProjection projection = TerrainProjection::LocalTriplanar;
  float axisContrast = 4;
  bool faceNormals = false;
  bool heightBlend = false;
  float heightContrast = 1, heightWidth = 1, heightInfluence = 1;
  TerrainSurfaceQuality heightQuality = TerrainSurfaceQuality::Medium;
  TerrainSurfaceNoise detailNoise, distanceNoise;
  std::array<TerrainSurfaceNoise, 3> normalNoise;
  // Participation weights retain each layer's independent noise settings.
  float globalDetailParticipation = 1, globalDistanceParticipation = 1;
  std::array<float, 3> globalNormalParticipation{1, 1, 1};
  AssetGuid antiTileEntry;
  // normal, detail, distance participation; zero GUID disables this binding.
  std::array<float, 3> antiTileParticipation{1, 1, 1};
  bool resampleAlbedo = false, resampleNormal = false,
       resampleHeightBlend = false;
  float resampleScale = 1, resampleStart = 10, resampleEnd = 100;
  TerrainDistanceBlend resampleBlend = TerrainDistanceBlend::Crossfade;
  TerrainSurfaceNoise resampleNoise;
  TerrainSurfaceCluster cluster;
};
struct TerrainSurfaceAsset {
  // Vector order is authoring/control-map order, including inactive layers.
  std::vector<TerrainSurfaceLayer> layers;
  TerrainSurfaceNoise detailNoise, distanceNoise;
  std::array<TerrainSurfaceNoise, 3> normalNoise;
  TerrainAntiTileArray antiTileArray;
};
struct TerrainSurfaceDiagnostic {
  AssetGuid layer, texture;
  std::string location;
};
bool validTerrainSurface(const TerrainSurfaceAsset &, std::string &error);
bool serializeTerrainSurface(const TerrainSurfaceAsset &, std::string &,
                             std::string &error);
bool deserializeTerrainSurface(std::string_view, TerrainSurfaceAsset &,
                               std::string &error);
// Structural validation is separate from project resolution: every valid but
// unavailable texture, including inactive-layer refs, produces a diagnostic.
bool resolveTerrainSurface(const TerrainSurfaceAsset &,
                           const std::function<bool(AssetGuid)> &textureExists,
                           std::vector<TerrainSurfaceDiagnostic> &,
                           std::string &error);
std::vector<AssetGuid> activeTerrainSurfaceLayers(const TerrainSurfaceAsset &);
} // namespace ae::resources
