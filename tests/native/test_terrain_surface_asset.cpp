#include "harness.h"
#include "resources/terrain_surface_asset.h"
#include "renderer/terrain_surface_table.h"
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

AE_TEST(terrain_noise_table_relocates_all_absolute_pointers_and_retains_atomic_failure) {
  using namespace ae::renderer;
  TerrainSurfaceBinding binding;
  TerrainSurfaceLayer layer;layer.identity={11,1};
  layer.globalDetailParticipation=.25f;layer.detailNoise=noise(20);
  binding.surface.layers.push_back(layer);
  binding.surface.detailNoise=noise(21);
  binding.textures.push_back({{91,20},false,false,0});
  binding.textures.push_back({{91,21},false,false,1});
  std::array<ae::u32,2> slots{7,9};
  std::string error;std::vector<float> part;ae::u32 offset=0;
  AE_EXPECT_TRUE(appendTerrainSurfaceTable(binding,slots,part,offset,error),error.c_str());
  AE_EXPECT_EQ(offset,ae::u32(1),"canonical publication starts at zero");
  const auto canonical=part;
  const auto advanced=ae::usize(part[2])-1;
  AE_EXPECT_EQ(part[advanced*4],1.f,"advanced version");
  AE_EXPECT_EQ(part[(advanced+2)*4+8],9.f,"global noise descriptor");
  const auto directory=ae::usize(part[advanced*4+3]);
  AE_EXPECT_EQ(part[(directory+1)*4],.25f,"participation distinct from source noise");
  std::vector<float> direct(12,0.f);
  AE_EXPECT_TRUE(appendTerrainSurfaceTable(binding,slots,direct,offset,error),error.c_str());
  AE_EXPECT_TRUE(relocateTerrainSurfaceTable(part,3,error),error.c_str());
  AE_EXPECT_EQ(std::vector<float>(direct.begin()+12,direct.end()),part,
               "relocated canonical part equals direct append including every pointer");
  part=canonical;part[advanced*4]=99;const auto invalid=part;
  AE_EXPECT_TRUE(!relocateTerrainSurfaceTable(part,3,error),"unknown directory version rejected");
  AE_EXPECT_EQ(part,invalid,"relocation failure atomic");
  auto missing=binding;missing.textures.pop_back();part={1,2,3,4};const auto before=part;offset=99;
  AE_EXPECT_TRUE(!appendTerrainSurfaceTable(missing,slots,part,offset,error),"missing noise descriptor blocks publication");
  AE_EXPECT_EQ(part,before,"missing-map append atomic");AE_EXPECT_EQ(offset,ae::u32(99),"failure retains caller offset");
  part=canonical;
  AE_EXPECT_TRUE(!relocateTerrainSurfaceTable(part,TerrainSurfaceMaximumRows,error),"advanced capacity overflow rejected");
  AE_EXPECT_EQ(part,canonical,"overflow retains canonical part");
}

AE_TEST(terrain_m06_normal_and_antitile_table_identity_relocation_and_validation) {
  using namespace ae::renderer;
  TerrainSurfaceBinding binding;TerrainSurfaceLayer layer;layer.identity={11,1};layer.antiTileEntry={31,1};
  layer.globalNormalParticipation={.2f,.4f,.6f};layer.antiTileParticipation={.3f,.5f,.7f};
  TerrainSurfaceNoise normal;normal.enabled=true;normal.map.texture={91,1};normal.map.channel=TerrainSurfaceChannel::NormalRGB;
  binding.surface.normalNoise[2]=normal;layer.normalNoise[1]=normal;binding.surface.layers.push_back(layer);
  binding.surface.antiTileArray.enabled=true;
  TerrainAntiTileEntry entry;entry.identity={31,1};entry.normal=normal;
  auto second=entry;second.identity={31,2};second.normal.strength=.5f;
  binding.surface.antiTileArray.entries={second,entry};
  binding.textures.push_back({{91,1},true,true,0});
  std::array<ae::u32,1> slots{9};std::string error;ae::u32 offset=0;std::vector<float> canonical;
  AE_EXPECT_TRUE(appendTerrainSurfaceTable(binding,slots,canonical,offset,error),error.c_str());
  auto reordered=binding;std::swap(reordered.surface.antiTileArray.entries[0],reordered.surface.antiTileArray.entries[1]);
  std::vector<float> other;
  AE_EXPECT_TRUE(appendTerrainSurfaceTable(reordered,slots,other,offset,error),error.c_str());
  AE_EXPECT_EQ(canonical,other,"antiTile authored reorder cannot change selected stable identity or canonical GPU key");
  std::vector<float> appended(28,0.f);
  AE_EXPECT_TRUE(appendTerrainSurfaceTable(binding,slots,appended,offset,error),error.c_str());
  other=canonical;AE_EXPECT_TRUE(relocateTerrainSurfaceTable(other,7,error),error.c_str());
  AE_EXPECT_EQ(other,std::vector<float>(appended.begin()+28,appended.end()),"every M06 normal and antiTile pointer relocates");
  auto missing=binding;missing.surface.layers[0].antiTileEntry={31,99};
  const auto before=other;
  AE_EXPECT_TRUE(!appendTerrainSurfaceTable(missing,slots,other,offset,error),"missing antiTile identity blocks GPU publication");
  AE_EXPECT_EQ(other,before,"missing identity publication atomic");
  auto wrongRole=binding;wrongRole.textures[0].normal=false;
  AE_EXPECT_TRUE(!appendTerrainSurfaceTable(wrongRole,slots,other,offset,error),"normal noise cannot consume color texture view");
  AE_EXPECT_EQ(other,before,"wrong-role view preserves previous table");
}

AE_TEST(terrain_m07_resampling_table_relocation_preserves_normal_and_antitile_directories) {
  using namespace ae::renderer;
  TerrainSurfaceBinding binding;TerrainSurfaceLayer layer;layer.identity={1,1};
  layer.resampleAlbedo=layer.resampleNormal=layer.resampleHeightBlend=true;
  layer.resampleScale=4;layer.resampleStart=8;layer.resampleEnd=80;
  layer.resampleBlend=TerrainDistanceBlend::Noise;
  layer.resampleNoise.enabled=true;layer.resampleNoise.map.texture={91,1};
  binding.surface.layers={layer};binding.textures.push_back({{91,1},true,false,0});
  binding.surface.antiTileArray.enabled=true;
  TerrainAntiTileEntry entry;entry.identity={31,1};entry.detail=layer.resampleNoise;
  binding.surface.antiTileArray.entries={entry};binding.surface.layers[0].antiTileEntry=entry.identity;
  std::array<ae::u32,1> slots{7};std::string error;ae::u32 offset=0;std::vector<float> canonical;
  AE_EXPECT_TRUE(appendTerrainSurfaceTable(binding,slots,canonical,offset,error),error.c_str());
  const auto advanced=ae::usize(canonical[2])-1;
  const auto directory=ae::usize(canonical[advanced*4+3]);
  const auto record=ae::usize(canonical[(directory+1)*4+2]);
  AE_EXPECT_TRUE(record>advanced && record<canonical.size()/4,"resample record published");
  AE_EXPECT_EQ(canonical[record*4+3],4.f,"authored far frequency multiplier");
  AE_EXPECT_EQ(canonical[(record+1)*4+2],1.f,"authored Noise mode");
  std::vector<float> direct(20,0.f);
  AE_EXPECT_TRUE(appendTerrainSurfaceTable(binding,slots,direct,offset,error),error.c_str());
  auto relocated=canonical;AE_EXPECT_TRUE(relocateTerrainSurfaceTable(relocated,5,error),error.c_str());
  AE_EXPECT_EQ(relocated,std::vector<float>(direct.begin()+20,direct.end()),"all resampling and anti pointers relocate consistently");
  auto bad=canonical;bad[(directory+1)*4+2]=float(bad.size()/4);const auto untouched=bad;
  AE_EXPECT_TRUE(!relocateTerrainSurfaceTable(bad,5,error),"invalid resample directory rejected before relocation");
  AE_EXPECT_EQ(bad,untouched,"resampling validation failure atomic");
}

AE_TEST(terrain_m08_cluster_publication_all_extensions_relocate_and_reject_corruption) {
  using namespace ae::renderer;
  TerrainSurfaceBinding binding;TerrainSurfaceLayer layer;layer.identity={1,1};
  layer.cluster.enabled=true;layer.cluster.stochastic=true;layer.cluster.quality=TerrainSurfaceQuality::High;
  for(ae::u64 i=0;i<3;++i){TerrainSurfaceVariant v;v.identity={11,i+1};v.maps[0].texture={91,i+1};v.scale=float(i+2);layer.cluster.variants.push_back(v);binding.textures.push_back({{91,i+1},false,false,ae::u32(i)});}
  layer.resampleAlbedo=true;layer.resampleScale=4;
  binding.surface.layers={layer};std::array<ae::u32,3> slots{4,7,9};std::string error;ae::u32 offset=0;
  std::vector<float> canonical;
  AE_EXPECT_TRUE(appendTerrainSurfaceTable(binding,slots,canonical,offset,error),error.c_str());
  std::vector<float> direct(44,0.f);AE_EXPECT_TRUE(appendTerrainSurfaceTable(binding,slots,direct,offset,error),error.c_str());
  auto relocated=canonical;AE_EXPECT_TRUE(relocateTerrainSurfaceTable(relocated,11,error),error.c_str());
  AE_EXPECT_EQ(relocated,std::vector<float>(direct.begin()+44,direct.end()),"cluster plus resampling pointers relocate consistently");
  const auto advanced=ae::usize(canonical[2])-1;
  const auto directory=ae::usize(canonical[advanced*4+3]);
  const auto cluster=ae::usize(canonical[(directory+1)*4+3]);
  auto bad=canonical;bad[(cluster+1)*4+3]=float(bad.size()/4+1);const auto before=bad;
  AE_EXPECT_TRUE(!relocateTerrainSurfaceTable(bad,11,error),"cluster record cannot exceed final table extent");
  AE_EXPECT_EQ(bad,before,"malformed cluster failure remains atomic");
  auto missing=binding;missing.textures.pop_back();relocated=canonical;
  AE_EXPECT_TRUE(!appendTerrainSurfaceTable(missing,slots,relocated,offset,error),"every authored variant map must resolve before publication");
  AE_EXPECT_EQ(relocated,canonical,"missing variant map retains applied table");
  binding.surface.layers[0].cluster.enabled=false;binding.surface.layers[0].cluster.variants.clear();
  canonical.clear();AE_EXPECT_TRUE(appendTerrainSurfaceTable(binding,slots,canonical,offset,error),error.c_str());
  AE_EXPECT_TRUE(relocateTerrainSurfaceTable(canonical,11,error),"stochastic base without variants has a valid relocatable table");
}
