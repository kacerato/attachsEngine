#pragma once
#include "resources/asset_registry.h"
#include "resources/animation_curve.h"
#include "resources/terrain_texture_policy.h"
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
enum class TerrainSurfaceChannel : u32 { R, G, B, A, RGB, NormalRGB, NormalRG, NormalGA };
enum class TerrainSurfaceQuality : u32 { Low, Medium, High };
enum class TerrainProjection : u32 { UV, LocalTriplanar, WorldTriplanar };
enum class TerrainDistanceBlend : u32 { Constant, Noise, Crossfade };
struct TerrainSurfaceMap {
  TerrainTextureResidency textureResidency; // serialized in v3 extension, never the legacy visitor
  AssetGuid texture;
  TerrainSurfaceChannel channel = TerrainSurfaceChannel::R;
  bool linear = true, invert = false;
  float factor = 1, remapMin = 0, remapMax = 1;
  // Unity TerrainLayer diffuse remaps are RGBA, unlike scalar packed-mask
  // remaps. These remain separate so importing either loses no channels.
  std::array<float, 4> colorFactor{1, 1, 1, 1};
  std::array<float, 4> colorRemapMin{0, 0, 0, 0}, colorRemapMax{1, 1, 1, 1};
};
enum class TerrainFlowRainMode : u32 { None, StreamsOnly, StreamsAndWetness };
enum class TerrainFlowEmitterKind : u32 { Water, Lava };
enum class TerrainFlowColliderKind : u32 { Water, Lava, Both };
struct TerrainFlowLayerSettings {
  // Shared porosity authority remains TerrainSurfaceLayer::snowPorosity (V3).
  float foam=1;
};
struct TerrainFlowControllerState {
  AssetGuid identity;
  bool wetnessAvailable=false,puddleAvailable=false,streamAvailable=false,rainAvailable=false;
  std::array<float,2> wetness{0,1};
  float puddleMaximum=1,streamMaximum=1,rainIntensity=0;
};
struct TerrainFlowSimulationSettings {
  bool enabled=false;
  std::array<float,2> evaporation{.01f,.01f},strength{1,1},speed{1,1},resistance{.1f,.1f};
  float wetnessEvaporation=.01f,burnEvaporation=.01f;
  // Runtime topology/capacity decision must be authored; no hidden truncation.
  u32 maximumEmitters=64,maximumColliders=64;
  std::string sourcePropertiesJson;
};
struct TerrainFlowEmitterSettings {
  TerrainFlowEmitterKind kind=TerrainFlowEmitterKind::Water;
  bool enabled=true;float strength=1;
  // Entity transform supplies world position and radius; source strength scales
  // effective radius, not volumetric flow. Ownership/lifecycle is not implied.
  AssetGuid terrain;
};
struct TerrainFlowColliderSettings {
  TerrainFlowColliderKind kind=TerrainFlowColliderKind::Both;
  bool enabled=true;AssetGuid terrain;
};
struct TerrainFlowSettings {
  bool wetness=false,puddles=false,streams=false,lava=false;
  bool perTexPorosity=false,perTexFoam=false,dynamicFlows=false,wetnessMaskSnow=false;
  bool heightWetness=false,rainDrops=false,globalWetness=false,globalRain=false;
  bool globalStreams=false,globalPuddles=false,heightDampStreams=false,heightDampLava=false;
  bool specularFade=false,distanceWetness=false,lavaStochastic=false;
  // Adjacent external tracks authority; retain thresholds even while disabled.
  bool tracksEnabled=false;
  TerrainFlowRainMode sourceRainMode=TerrainFlowRainMode::None;
  AssetGuid controller;
  TerrainSurfaceMap streamControl=[](){TerrainSurfaceMap m;m.channel=TerrainSurfaceChannel::RGB;return m;}(),
    dynamicStreamControl=[](){TerrainSurfaceMap m;m.channel=TerrainSurfaceChannel::RGB;return m;}();
  TerrainSurfaceMap streamNormal=[](){TerrainSurfaceMap m;m.channel=TerrainSurfaceChannel::NormalRG;return m;}();
  TerrainSurfaceMap lavaDiffuse=[](){TerrainSurfaceMap m;m.channel=TerrainSurfaceChannel::RGB;return m;}();
  TerrainSurfaceMap rainDropTexture=[](){TerrainSurfaceMap m;m.channel=TerrainSurfaceChannel::RGB;return m;}();
  float globalPorosity=.4f;
  std::array<float,4> wetnessParams{0,1,0,0},puddleParams{6,1,0,0};
  std::array<float,4> heightWetnessParams{1,.1f,1,1},distanceWetnessParams{100,1,200,0};
  std::array<float,4> streamFlowParams{.4f,.5f,.3f,.2f},streamNormalFoam{.35f,12,0,0};
  float streamBlend=40,streamMaximum=1;
  std::array<float,4> streamUvScales{70,70,70,70},streamTint{.5f,.5f,.5f,1},streamFades{0,10,1000,1500};
  std::array<float,4> lavaParams{2.7f,1,.5f,.275f},lavaParams2{.05f,.017f,.15f,2};
  // Source snippets omit color alpha: explicit unused alpha is preserved here;
  // a source material import must retain its actual serialized alpha.
  std::array<float,4> lavaEdgeColor{.05f,.015f,0,1},lavaColorLow{.83f,0,0,1},lavaColorHighlight{.66f,.66f,0,1};
  std::array<float,4> lavaUvScale{70,70,0,0},lavaFades{0,10,1000,1500};
  float lavaDisplacementScale=1,lavaEmissiveMultiplier=2,lavaStochasticContrast=.5f,lavaStochasticSize=1;
  std::array<float,4> rainIntensityScale{0,150,0,0},specularFades{0,100,0,0},traxFxThresholds{0,0,0,0};
  TerrainFlowSimulationSettings simulation;
  // All reserved vector components remain in these vectors. Unknown source
  // properties, keywords and TextureEnv ST stay lossless in provenance.
  std::string sourcePropertiesJson,sourceKeywordsJson,sourceTextureEnvsJson;
};

struct TerrainSurfaceNoise {
  bool enabled = false;
  TerrainSurfaceMap map;
  float scale = 1, strength = 1, fadeStart = 0, fadeEnd = 100;
  TerrainProjection projection = TerrainProjection::LocalTriplanar;
};
inline std::array<TerrainSurfaceMap,7> neutralTerrainSurfaceMaps() {
  std::array<TerrainSurfaceMap,7> maps{};
  maps[0].channel=maps[6].channel=TerrainSurfaceChannel::RGB;
  maps[0].linear=maps[6].linear=false;
  maps[1].channel=TerrainSurfaceChannel::NormalRGB;
  maps[2].factor=maps[5].factor=maps[6].factor=0;
  return maps;
}
struct TerrainSurfaceVariant {
  AssetGuid identity;
  // albedo, normal, height, AO, roughness, metallic, emission.
  std::array<TerrainSurfaceMap, 7> maps=neutralTerrainSurfaceMaps();
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
// Capacity references: Unity Shader Graph 17.0 Sample Texture 2D / Blend /
// Position / Normal Blend, and Unity 6000.0 Keyframe weighted tangents.
// https://docs.unity3d.com/Packages/com.unity.shadergraph@17.0/manual/Blend-Node.html
// https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Keyframe.html
enum class TerrainGeoMode : u32 { Off, Albedo, AlbedoNormal };
enum class TerrainColorBlend : u32 { Multiply2x, Overlay, LighterColor, Crossfade };
enum class TerrainNormalBlend : u32 { Reoriented, Crossfade };
struct TerrainGeoSettings {
  TerrainGeoMode mode=TerrainGeoMode::Off;
  TerrainSurfaceMap albedo=neutralTerrainSurfaceMaps()[0],normal=neutralTerrainSurfaceMaps()[1];
  TerrainColorBlend blend=TerrainColorBlend::Multiply2x;
  float blendStrength=1,normalStrength=1,scaleMeters=100,offsetMeters=0;
  bool rangeEnabled=false;
  std::array<float,4> range{0,0,100,100};
  bool curveEnabled=false;
  AnimationCurve curve=[](){AnimationCurve c;AnimationCurveKey a,b;a.id=1;b.id=2;b.time=1;c.keys={a,b};return c;}();
  float curveScaleMeters=512,curveOffsetMeters=0,curveRotationDegrees=0,heightContrast=10;
};
struct TerrainGlobalMap {
  bool enabled=false;
  TerrainSurfaceMap map;
  float intensity=1,fadeStart=50,fadeEnd=200,nearStrength=1,farStrength=1;
  float scaleX=1,scaleY=1,offsetX=0,offsetY=0;
  TerrainColorBlend blend=TerrainColorBlend::Crossfade;
  TerrainNormalBlend normalBlend=TerrainNormalBlend::Reoriented;
  // SAOM role only: independent references/remaps; compatible references share
  // one sampled texel. R is smoothness, converted to roughness after mapping.
  TerrainSurfaceMap smoothness,ao=[](){TerrainSurfaceMap m;m.channel=TerrainSurfaceChannel::G;return m;}(),
    metallic=[](){TerrainSurfaceMap m;m.channel=TerrainSurfaceChannel::B;return m;}(),
    mask=[](){TerrainSurfaceMap m;m.channel=TerrainSurfaceChannel::A;return m;}();
};
inline std::array<TerrainGlobalMap,4> neutralTerrainGlobalMaps(){
  std::array<TerrainGlobalMap,4> maps;
  maps[0].map=maps[3].map=neutralTerrainSurfaceMaps()[0];maps[1].map=neutralTerrainSurfaceMaps()[1];return maps;
}
// M11/M12 proposed complete contract. Staging only until all consumers exist.
enum class TerrainSnowMode : u32 { Off, Simple, Rich };
struct TerrainSnowSettings {
  TerrainSnowMode mode=TerrainSnowMode::Off;
  TerrainSurfaceMap diffuse=neutralTerrainSurfaceMaps()[0];
  TerrainSurfaceMap height=[](){TerrainSurfaceMap m;m.channel=TerrainSurfaceChannel::A;return m;}();
  TerrainSurfaceMap normal=[](){TerrainSurfaceMap m;m.channel=TerrainSurfaceChannel::NormalGA;return m;}();
  TerrainSurfaceMap smoothness;
  TerrainSurfaceMap ao=[](){TerrainSurfaceMap m;m.channel=TerrainSurfaceChannel::B;return m;}();
  TerrainSurfaceMap normalNoiseMap=neutralTerrainSurfaceMaps()[1],mask=[](){TerrainSurfaceMap m;m.channel=TerrainSurfaceChannel::RGB;return m;}(),
    sparkleNoiseMap=[](){TerrainSurfaceMap m;m.channel=TerrainSurfaceChannel::RGB;return m;}();
  std::array<float,4> upVector{0,1,0,0},tint{1,1,1,1},uvScales{50,50,0,0};
  float amount=.5f;
  // Source height Y and distance W are lengths; these saved values are ends.
  std::array<float,4> heightAngleRange{50,140,0,1},parameters{0,0,.3f,.1f};
  std::array<float,4> normalNoiseParams{.1f,.5f,0,0},distanceResample{.1f,.5f,50,150};
  float stochasticScale=1,stochasticContrast=.3f;
  bool normalNoiseEnabled=false,distanceResampleEnabled=false,perLayerEnabled=false,maskEnabled=false,
    worldSpaceUV=false,stochasticEnabled=false,triplanarEnabled=false,useGlobalLevel=false,useGlobalHeight=false;
  bool sssEnabled=false,rimEnabled=false,sparkleEnabled=false;
  std::array<float,4> sssTint{1,1,1,.5f},rimColor{.5f,.5f,1,1},sparkleTint{1,1,1,1};
  float rimPower=8,sparkleEmission=.25f,sparkleStrength=1,sparkleSize=3,sparkleDensity=5,
    sparkleNoiseDensity=1,sparkleNoiseAmplitude=10,sparkleViewDependency=.3f;
  // Complete source provenance, including reserved vector components and
  // adjacent tracks/tessellation properties with no supported consumer yet.
  std::string sourcePropertiesJson,sourceKeywordsJson;
};
struct TerrainSnowControllerState {
  bool available=false;
  float level=.5f,heightStart=50,heightEnd=140;
  std::array<float,2> sourceAngleZW{0,1}; // preserved; shader angles remain local
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
  std::array<TerrainSurfaceMap, 7> maps=neutralTerrainSurfaceMaps();
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
  float geoParticipation=1;
  bool geoHeightFilterEnabled=false;
  float geoHeightThreshold=1,geoHeightStrength=1;
  std::array<float,4> globalMapParticipation{1,1,1,1};
  float snowParticipation=1,snowPorosity=.4f; // Shared material porosity; legacy serialized name.
  TerrainFlowLayerSettings flow;
};
struct TerrainSurfaceAsset {
  // Vector order is authoring/control-map order, including inactive layers.
  std::vector<TerrainSurfaceLayer> layers;
  TerrainSurfaceNoise detailNoise, distanceNoise;
  std::array<TerrainSurfaceNoise, 3> normalNoise;
  TerrainAntiTileArray antiTileArray;
  TerrainTextureResidency textureResidency{TerrainTextureResidencyMode::Original,0};
  TerrainSnowSettings snow;
  TerrainFlowSettings flow;
  TerrainGeoSettings geo;
  std::array<TerrainGlobalMap,4> globalMaps=neutralTerrainGlobalMaps();
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

// Explicit producer capabilities; defaults diagnose every active Flow consumer
// until publication supplies a real, validated implementation.
struct TerrainFlowConsumerCapabilities {
  bool wetness=false,puddles=false,streams=false,lava=false,dynamicFlows=false;
  bool heightWetness=false,distanceWetness=false,rain=false,snowClearing=false;
  bool streamHeightFilter=false,lavaHeightFilter=false,specularFade=false;
  bool lavaStochastic=false,tracks=false,displacement=false,globalController=false;
  bool staticControlProvided=false,dynamicControlProvided=false;
};
bool diagnoseTerrainFlowPublication(const TerrainSurfaceAsset &,
  const std::function<bool(AssetGuid)> &textureExists,
  const TerrainFlowConsumerCapabilities &,std::vector<TerrainSurfaceDiagnostic> &,
  std::string &error);

std::vector<AssetGuid> activeTerrainSurfaceLayers(const TerrainSurfaceAsset &);
} // namespace ae::resources
