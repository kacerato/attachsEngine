#include "harness.h"
#include "resources/terrain_surface_asset.h"
#include <limits>
using namespace ae::resources;
namespace {
TerrainSurfaceMap channel(ae::u64 id) {
  TerrainSurfaceMap m;
  m.texture = {91, id};
  m.channel = TerrainSurfaceChannel::A;
  m.linear = false;
  m.invert = true;
  m.factor = .7f;
  m.remapMin = .1f;
  m.remapMax = .8f;
  m.colorFactor = {.2f, .4f, .6f, .8f};
  m.colorRemapMin = {.1f, .2f, .3f, .4f};
  m.colorRemapMax = {.6f, .7f, .8f, .9f};
  return m;
}
TerrainSurfaceNoise noise(ae::u64 id) {
  TerrainSurfaceNoise n;
  n.enabled = true;
  n.map = channel(id);
  n.scale = 3;
  n.strength = .4f;
  n.fadeStart = 5;
  n.fadeEnd = 55;
  n.projection = TerrainProjection::WorldTriplanar;
  return n;
}
TerrainSurfaceAsset surface() {
  TerrainSurfaceAsset a;
  TerrainSurfaceLayer l;
  l.identity = {1, 2};
  l.name = "Rock \\\"test\"";
  l.thumbnail = {91, 99};
  for (size_t i = 0; i < 7; ++i)
    l.maps[i] = channel(i + 1);
  l.scaleX = 3;
  l.scaleY = 5;
  l.offsetX = -2;
  l.offsetY = 9;
  l.rotation = 17;
  l.tint = {.3f, .5f, .7f, .9f};
  l.saturation = .6f;
  l.projection = TerrainProjection::WorldTriplanar;
  l.axisContrast = 7;
  l.faceNormals = true;
  l.heightBlend = true;
  l.heightContrast = 2;
  l.heightWidth = .4f;
  l.heightInfluence = .8f;
  l.heightQuality = TerrainSurfaceQuality::High;
  l.detailNoise = noise(20);
  l.distanceNoise = noise(21);
  l.globalDetailParticipation = .3f;
  l.globalDistanceParticipation = .7f;
  l.globalNormalParticipation = {.2f, .4f, .6f};
  l.antiTileEntry = {3, 1};
  l.antiTileParticipation = {.7f, .5f, .3f};
  for (size_t i = 0; i < 3; ++i)
    l.normalNoise[i] = noise(30 + i);
  l.resampleAlbedo = l.resampleNormal = l.resampleHeightBlend = true;
  l.resampleScale = 4;
  l.resampleStart = 20;
  l.resampleEnd = 200;
  l.resampleBlend = TerrainDistanceBlend::Noise;
  l.resampleNoise = noise(40);
  l.cluster.enabled = l.cluster.stochastic = true;
  l.cluster.selection = noise(50);
  l.cluster.contrast = 3;
  l.cluster.noiseBoost = 2;
  l.cluster.quality = TerrainSurfaceQuality::High;
  for (ae::u64 i = 0; i < 3; ++i) {
    TerrainSurfaceVariant v;
    v.identity = {2, i + 1};
    for (size_t c = 0; c < 7; ++c)
      v.maps[c] = channel(100 + i * 10 + c);
    v.scale = 2 + i;
    v.offsetX = -float(i);
    v.offsetY = float(i);
    v.rotation = 23;
    l.cluster.variants.push_back(v);
  }
  a.layers.push_back(l);
  l.identity = {1, 3};
  l.active = false;
  a.layers.push_back(l);
  l.identity = {1, 4};
  l.active = true;
  a.layers.push_back(l);
  a.detailNoise = noise(60);
  a.distanceNoise = noise(61);
  for (size_t i = 0; i < 3; ++i)
    a.normalNoise[i] = noise(70 + i);
  a.antiTileArray.enabled = true;
  TerrainAntiTileEntry entry;
  entry.identity = {3, 1};
  entry.normal = noise(80);
  entry.detail = noise(81);
  entry.distance = noise(82);
  a.antiTileArray.entries.push_back(entry);
  return a;
}
} // namespace
AE_TEST(terrain_surface_full_contract_roundtrip_and_active_order) {
  auto a = surface();
  std::string text, again, e;
  TerrainSurfaceAsset b;
  AE_EXPECT_TRUE(serializeTerrainSurface(a, text, e), e.c_str());
  AE_EXPECT_TRUE(deserializeTerrainSurface(text, b, e), e.c_str());
  AE_EXPECT_TRUE(serializeTerrainSurface(b, again, e), e.c_str());
  AE_EXPECT_EQ(text, again, "all authored values roundtrip canonically");
  AE_EXPECT_EQ(b.layers[0].maps[4].invert, true,
               "smoothness inversion retained");
  AE_EXPECT_EQ(b.layers[0].cluster.variants[2].maps[6].texture.low,
               ae::u64(126), "third variant emission retained");
  AE_EXPECT_EQ(b.layers[0].normalNoise[2].map.texture.low, ae::u64(32),
               "third independent normal noise retained");
  AE_EXPECT_EQ(b.antiTileArray.entries[0].normal.map.texture.low, ae::u64(80),
               "independent anti-tile source retained");
  AE_EXPECT_EQ(b.layers[0].antiTileParticipation[2], .3f,
               "distance anti-tile participation retained");
  AE_EXPECT_EQ(b.layers[0].maps[0].colorRemapMin[2], .3f,
               "RGBA remap preserves distinct color channels");
  AE_EXPECT_EQ(b.layers[0].name, a.layers[0].name, "quoted label retained");
  auto active = activeTerrainSurfaceLayers(b);
  AE_EXPECT_EQ(active, (std::vector<AssetGuid>{{1, 2}, {1, 4}}),
               "inactive layer retains index while active order preserved");
  std::swap(b.layers[0], b.layers[2]);
  AE_EXPECT_EQ(activeTerrainSurfaceLayers(b),
               (std::vector<AssetGuid>{{1, 4}, {1, 2}}),
               "reorder follows stable identities");
}
AE_TEST(terrain_surface_resolution_reports_every_missing_reference) {
  auto a = surface();
  std::vector<TerrainSurfaceDiagnostic> d;
  std::string e;
  AE_EXPECT_TRUE(!resolveTerrainSurface(
                     a, [](AssetGuid id) { return id.low != 5; }, d, e),
                 "missing reference explicit failure");
  AE_EXPECT_EQ(d.size(), size_t(3),
               "missing channel in all layers including inactive");
  AE_EXPECT_EQ(d[0].location, std::string("channel.4"),
               "missing roughness channel diagnosed");
  AE_EXPECT_EQ(d[1].layer, a.layers[1].identity,
               "inactive layer diagnostics retained");
  AE_EXPECT_TRUE(resolveTerrainSurface(
                     a, [](AssetGuid) { return true; }, d, e),
                 e.c_str());
  AE_EXPECT_TRUE(d.empty(), "fully resolved contract has no missing refs");
}
AE_TEST(terrain_surface_strict_validation_and_atomic_migration) {
  auto good = surface(), a = good;
  std::string e, text;
  a.layers[1].identity = a.layers[0].identity;
  AE_EXPECT_TRUE(!validTerrainSurface(a, e),
                 "duplicate layer identity rejected");
  a = good;
  a.layers[0].detailNoise.map.texture = {};
  AE_EXPECT_TRUE(!validTerrainSurface(a, e),
                 "enabled noise without texture rejected");
  a = good;
  a.layers[0].resampleEnd = a.layers[0].resampleStart;
  AE_EXPECT_TRUE(!validTerrainSurface(a, e), "zero-width transition rejected");
  a = good;
  a.layers[0].cluster.variants[2].maps[0] =
      a.layers[0].cluster.variants[0].maps[0];
  AE_EXPECT_TRUE(!validTerrainSurface(a, e),
                 "repeated texture is not a distinct cluster");
  a = good;
  a.normalNoise[0].strength = std::numeric_limits<float>::infinity();
  AE_EXPECT_TRUE(!validTerrainSurface(a, e), "non-finite noise rejected");
  a = good;
  a.layers[0].antiTileEntry = {3, 99};
  AE_EXPECT_TRUE(!validTerrainSurface(a, e),
                 "missing anti-tile identity rejected");
  a = good;
  a.antiTileArray.entries[0].normal.map.texture = {};
  AE_EXPECT_TRUE(!validTerrainSurface(a, e),
                 "anti-tile missing normal source rejected");
  a = good;
  a.layers[0].antiTileParticipation[0] = 2;
  AE_EXPECT_TRUE(!validTerrainSurface(a, e),
                 "anti-tile participation out of range rejected");
  a = good;
  a.antiTileArray.entries.push_back(a.antiTileArray.entries[0]);
  AE_EXPECT_TRUE(!validTerrainSurface(a, e),
                 "duplicate anti-tile entry rejected");
  a = good;
  a.layers[0].maps[0].colorRemapMin[2] = 2;
  AE_EXPECT_TRUE(!validTerrainSurface(a, e),
                 "inverted RGBA remap range rejected");
  AE_EXPECT_TRUE(serializeTerrainSurface(good, text, e), e.c_str());
  for (auto bad : std::vector<std::string>{
           "AETERRAINSURFACE2 0", text + " extra",
           text.substr(0, text.size() / 2), "AETERRAINSURFACE1 33",
           "AETERRAINSURFACE1 -1"}) {
    a = good;
    AE_EXPECT_TRUE(!deserializeTerrainSurface(bad, a, e),
                   "invalid version/truncated/extra/bounded input rejected");
    AE_EXPECT_EQ(a.layers[0].identity, good.layers[0].identity,
                 "parse failure preserves caller asset");
    AE_EXPECT_EQ(a.layers.size(), good.layers.size(),
                 "parse failure preserves layer list");
  }
}
AE_TEST(terrain_surface_provenance_bounds_and_writer_atomic_budget) {
  auto a = surface();
  std::string error, text = "preserved";
  a.layers[0].sourceCatalogId.assign(257, 'x');
  AE_EXPECT_TRUE(!serializeTerrainSurface(a, text, error),
                 "catalog identity bounded at 256 bytes");
  AE_EXPECT_EQ(text, std::string("preserved"),
               "invalid metadata leaves serialized output untouched");
  a = surface();
  a.layers[0].sourcePropertiesJson.assign(65537, 'x');
  AE_EXPECT_TRUE(!serializeTerrainSurface(a, text, error),
                 "raw provenance bounded at 64 KiB");
  a = surface();
  a.layers.clear();
  for (ae::u64 i = 0; i < 9; ++i) {
    TerrainSurfaceLayer l;
    l.identity = {11, i + 1};
    l.sourcePropertiesJson = "{\"raw\":\"" + std::string(64000, 'x') + "\"}";
    l.sourcePackingJson = l.sourcePropertiesJson;
    a.layers.push_back(l);
  }
  AE_EXPECT_TRUE(validTerrainSurface(a, error), error.c_str());
  AE_EXPECT_TRUE(!serializeTerrainSurface(a, text, error),
                 "writer enforces same 1 MiB budget as reader");
  AE_EXPECT_EQ(text, std::string("preserved"),
               "oversize serialization leaves output untouched");
  a = surface();
  a.layers[0].sourceCatalogId = "catalogID";
  a.layers[0].sourcePropertiesJson =
      "{ \"unsupported\": \"" + std::string(1024, 'q') + "\" }";
  AE_EXPECT_TRUE(serializeTerrainSurface(a, text, error), error.c_str());
  TerrainSurfaceAsset b;
  AE_EXPECT_TRUE(deserializeTerrainSurface(text, b, error), error.c_str());
  AE_EXPECT_EQ(b.layers[0].sourcePropertiesJson,
               a.layers[0].sourcePropertiesJson,
               "provenance strings over old 128-byte limit remain exact");
}
