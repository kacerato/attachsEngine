#include "harness.h"
#include "resources/json_reader.h"
#include "resources/terrain_surface_catalog.h"
#include "core/sha256.h"
#include <fstream>
#include <iterator>
#include <set>
using namespace ae::resources;
namespace {
std::string catalogText() {
  std::ifstream file("assets/terrain/library/catalog.json", std::ios::binary);
  return {std::istreambuf_iterator<char>(file),
          std::istreambuf_iterator<char>()};
}
bool replaceOnce(std::string &text, const std::string &before,
                 const std::string &after) {
  auto p = text.find(before);
  if (p == std::string::npos)
    return false;
  text.replace(p, before.size(), after);
  return true;
}
} // namespace
AE_TEST(terrain_surface_catalog_committed_inventory_and_all_materials) {
  auto text = catalogText();
  AE_EXPECT_TRUE(!text.empty(),
                 "run from repository root to load committed catalog");
  TerrainSurfaceCatalog c;
  std::string e;
  AE_EXPECT_TRUE(loadTerrainSurfaceCatalog(text, c, e), e.c_str());
  AE_EXPECT_EQ(c.materials.size(), size_t(82),
               "actual committed material inventory");
  AE_EXPECT_EQ(c.textures.size(), size_t(148),
               "actual committed texture inventory");
  std::set<AssetGuid> identities;
  for (auto &t : c.textures) {
    AE_EXPECT_TRUE(identities.insert(t.identity).second,
                   "texture imports keep distinct GUIDs");
    AE_EXPECT_EQ(t.sourceHash.size(), size_t(64), "original hash retained");
    AE_EXPECT_EQ(t.runtimeHash.size(), size_t(64), "runtime hash retained");
    AE_EXPECT_TRUE(t.width > 0 && t.height > 0, "original dimensions retained");
  }
  for (auto &m : c.materials) {
    AE_EXPECT_EQ(m.layer.identity,
                 assetGuidFromSeed("astra.terrain.catalog.material:" + m.id),
                 "exact material identity seed");
    JsonDocument source;
    AE_EXPECT_TRUE(JsonDocument::parse(m.sourcePropertiesJson, source),
                   "lossless original properties JSON retained");
    std::vector<std::string> ids{m.id};
    TerrainSurfaceSelection result;
    AE_EXPECT_TRUE(makeTerrainSurface(c, ids, result, e), e.c_str());
    AE_EXPECT_TRUE(validTerrainSurface(result.surface, e), e.c_str());
    std::vector<TerrainSurfaceDiagnostic> missing;
    AE_EXPECT_TRUE(resolveTerrainSurface(
                       result.surface,
                       [&](AssetGuid id) {
                         for (auto &t : result.requiredTextures)
                           if (t.identity == id)
                             return true;
                         return false;
                       },
                       missing, e),
                   e.c_str());
    AE_EXPECT_EQ(result.surface.layers[0].identity, m.layer.identity,
                 "selection preserves stable layer identity");
    std::string saved;
    TerrainSurfaceAsset reloaded;
    AE_EXPECT_TRUE(serializeTerrainSurface(result.surface, saved, e),
                   e.c_str());
    AE_EXPECT_TRUE(deserializeTerrainSurface(saved, reloaded, e), e.c_str());
    AE_EXPECT_EQ(reloaded.layers[0].sourceCatalogId, m.id,
                 "catalog identity survives project save");
    AE_EXPECT_EQ(reloaded.layers[0].sourcePropertiesJson,
                 m.sourcePropertiesJson,
                 "exact unsupported source properties survive save/reload");
    AE_EXPECT_EQ(reloaded.layers[0].sourcePackingJson, m.packingJson,
                 "exact source packing survives save/reload");
    AE_EXPECT_EQ(reloaded.layers[0].sourceResolvedMapsJson, m.resolvedMapsJson,
                 "exact mapped source roles survive save/reload");
    if (m.clusterIds.size() == 1)
      AE_EXPECT_TRUE(!m.layer.cluster.enabled,
                     "single texture never misrepresented as cluster");
  }
  std::vector<std::string> ids{c.materials[2].id, c.materials[0].id};
  TerrainSurfaceSelection selected;
  AE_EXPECT_TRUE(makeTerrainSurface(c, ids, selected, e), e.c_str());
  AE_EXPECT_EQ(selected.surface.layers[0].identity,
               c.materials[2].layer.identity, "explicit choice order retained");
  AE_EXPECT_EQ(selected.surface.layers[1].identity,
               c.materials[0].layer.identity,
               "second explicit choice retained");
}
AE_TEST(terrain_surface_catalog_original_geo_pair_provenance_and_bytes) {
  TerrainSurfaceCatalog c;std::string error;
  AE_EXPECT_TRUE(loadTerrainSurfaceCatalog(catalogText(),c,error),error.c_str());
  const char *guids[]{"863f19b183b2443859f959a96f7dce32","192aaea3237044f608d663c9884e0c70"};
  const char *hashes[]{"a3b69d0594c9e95b2cfc2f9b844d2ca6903a112e0856293de4d66c84e527851a","ff742cdcd52c1c87bd361d821dc6d46b4061f52007f5f6e169628730f2a0fe62"};
  const char *names[]{"microsplat_def_geomap_01.png","microsplat_def_geomap_norm_01.png"};
  const size_t lengths[]{15544,16092};
  for(size_t i=0;i<2;++i) {
    const TerrainCatalogTexture *found=nullptr;
    for(const auto &texture:c.textures)if(texture.identity.text()==guids[i])found=&texture;
    AE_EXPECT_TRUE(found,"supplied Geo GUID exists in real catalog");
    if(!found)return;
    AE_EXPECT_EQ(found->id,std::string(hashes[i]),"content identity exact");
    AE_EXPECT_EQ(found->sourceHash,std::string(hashes[i]),"source digest exact");
    AE_EXPECT_EQ(found->runtimeHash,std::string(hashes[i]),"runtime uses original bytes");
    AE_EXPECT_EQ(found->sourcePath,std::string("sources/")+hashes[i]+".png","original path exact");
    AE_EXPECT_EQ(found->runtimePath,found->sourcePath,"no quality derivative substituted");
    AE_EXPECT_EQ(found->originalAssetPath,std::string("Packages/com.jbooth.microsplat.global-texture/Textures/")+names[i],"source provenance exact");
    AE_EXPECT_EQ(found->archiveHash,std::string("071cbf56ceebf0dc9e2edd8e1bec2ed8219315772808fa9d78406305949a6772"),"supplied archive provenance exact");
    AE_EXPECT_EQ(found->width,ae::u32(1),"original width");
    AE_EXPECT_EQ(found->height,ae::u32(256),"original height");
    std::ifstream file("assets/terrain/library/"+found->runtimePath,std::ios::binary);
    std::vector<ae::u8> bytes{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
    AE_EXPECT_EQ(bytes.size(),lengths[i],"original encoded length");
    AE_EXPECT_EQ(ae::Sha256::hex(std::span<const ae::u8>(bytes)),std::string(hashes[i]),"actual original bytes verified");
  }
}
AE_TEST(terrain_surface_catalog_exact_resolved_channels_and_raw_sources) {
  auto text = catalogText();
  TerrainSurfaceCatalog c;
  std::string e;
  JsonDocument d;
  AE_EXPECT_TRUE(loadTerrainSurfaceCatalog(text, c, e), e.c_str());
  AE_EXPECT_TRUE(JsonDocument::parse(text, d), "original JSON parse");
  auto *materials = d.member(*d.root(), "materials");
  size_t checked = 0;
  for (size_t i = 0; i < c.materials.size(); ++i) {
    auto &m = c.materials[i];
    auto &n = *d.child(*materials, ae::u32(i));
    auto *maps = d.member(n, "resolved_maps");
    const char *names[]{"albedo",     "normal",   "height",  "ao",
                        "smoothness", "metallic", "emission"};
    for (size_t role = 0; role < 7; ++role) {
      auto *source = d.member(*maps, names[role]);
      if (!source)
        continue;
      auto id = d.string(*source, "texture");
      const TerrainCatalogTexture *texture = nullptr;
      for (auto &t : c.textures)
        if (t.id == id)
          texture = &t;
      AE_EXPECT_TRUE(texture, "every committed mapped ref exists");
      auto &target = m.layer.maps[role];
      AE_EXPECT_EQ(target.texture, texture->identity, "texture GUID exact");
      AE_EXPECT_EQ(target.linear, d.string(*source, "color_space") == "linear",
                   "color space exact");
      auto *channel = d.member(*source, "channel");
      if (channel && channel->kind == JsonDocument::Kind::Number)
        AE_EXPECT_EQ(ae::u32(target.channel), ae::u32(channel->number),
                     "packed channel exact");
      auto *invert = d.member(*source, "roughness_invert");
      if (invert)
        AE_EXPECT_EQ(target.invert, invert->boolean,
                     "roughness conversion explicit");
      ++checked;
    }
    // Raw JSON is a substring, not a JSON reserialization that changes floats.
    AE_EXPECT_TRUE(text.find(m.sourcePropertiesJson) != std::string::npos,
                   "original property bytes retained");
    if (!m.packingJson.empty())
      AE_EXPECT_TRUE(text.find(m.packingJson) != std::string::npos,
                     "original packing bytes retained");
  }
  AE_EXPECT_TRUE(checked > 200, "broad real channel coverage");
}
AE_TEST(
    terrain_surface_catalog_malicious_paths_duplicates_and_truncation_atomic) {
  auto text = catalogText();
  TerrainSurfaceCatalog c;
  std::string e;
  AE_EXPECT_TRUE(loadTerrainSurfaceCatalog(text, c, e), e.c_str());
  auto originalId = c.materials[0].id;
  auto bad = text;
  AE_EXPECT_TRUE(replaceOnce(bad, c.textures[0].runtimePath, "../escape.png"),
                 "path fixture replacement");
  AE_EXPECT_TRUE(!loadTerrainSurfaceCatalog(bad, c, e), "traversal rejected");
  AE_EXPECT_EQ(c.materials[0].id, originalId, "failed parse atomic");
  bad = text;
  AE_EXPECT_TRUE(replaceOnce(bad, c.textures[0].runtimePath, "C:/escape.png"),
                 "absolute path fixture");
  AE_EXPECT_TRUE(!loadTerrainSurfaceCatalog(bad, c, e),
                 "absolute path rejected");
  bad = text;
  AE_EXPECT_TRUE(replaceOnce(bad, c.textures[1].id, c.textures[0].id),
                 "duplicate fixture");
  AE_EXPECT_TRUE(!loadTerrainSurfaceCatalog(bad, c, e),
                 "duplicate texture identity rejected");
  AE_EXPECT_TRUE(
      !loadTerrainSurfaceCatalog(text.substr(0, text.size() / 2), c, e),
      "truncated JSON rejected");
  TerrainSurfaceSelection selection;
  std::vector<std::string> ids{originalId, originalId};
  AE_EXPECT_TRUE(!makeTerrainSurface(c, ids, selection, e),
                 "repeated selected material rejected");
}
AE_TEST(terrain_surface_catalog_missing_and_unsupported_maps_are_diagnostic) {
  auto text = catalogText();
  TerrainSurfaceCatalog c;
  std::string e;
  AE_EXPECT_TRUE(loadTerrainSurfaceCatalog(text, c, e), e.c_str());
  auto mapStart = text.find("\"resolved_maps\"");
  AE_EXPECT_TRUE(mapStart != std::string::npos, "resolved map fixture exists");
  auto objectStart = text.find('{', mapStart);
  auto bad = text;
  bad.insert(objectStart + 1, "\"unsupportedRole\": {\"texture\": \"" +
                                  c.textures[0].id +
                                  "\", \"color_space\": \"linear\"},");
  TerrainSurfaceCatalog extended;
  AE_EXPECT_TRUE(loadTerrainSurfaceCatalog(bad, extended, e), e.c_str());
  AE_EXPECT_TRUE(!extended.materials[0].diagnostics.empty(),
                 "unsupported role not silently dropped");
  AE_EXPECT_TRUE(extended.materials[0].resolvedMapsJson.find(
                     "unsupportedRole") != std::string::npos,
                 "unsupported map metadata retained exactly");
  bad = text;
  auto textureStart = bad.find("\"texture\"", mapStart);
  auto colon = bad.find(':', textureStart);
  auto quote = bad.find('"', colon);
  auto end = bad.find('"', quote + 1);
  bad.replace(quote + 1, end - quote - 1, "missing-texture-reference");
  AE_EXPECT_TRUE(loadTerrainSurfaceCatalog(bad, extended, e), e.c_str());
  AE_EXPECT_TRUE(!extended.materials[0].diagnostics.empty(),
                 "missing mapped texture reported");
  std::vector<std::string> selected{extended.materials[0].id};
  TerrainSurfaceSelection result;
  AE_EXPECT_TRUE(makeTerrainSurface(extended, selected, result, e), e.c_str());
  AE_EXPECT_TRUE(!result.diagnostics.empty(),
                 "selection carries missing reference diagnostics to caller");
}
