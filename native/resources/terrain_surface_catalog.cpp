#include "resources/terrain_surface_catalog.h"
#include "resources/json_reader.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace ae::resources {
namespace {
using Node = JsonDocument::Node;
using Kind = JsonDocument::Kind;
bool safePath(std::string_view path) {
  if (path.empty() || path.size() > 2048 || path.front() == '/' ||
      path.front() == '\\')
    return false;
  size_t start = 0;
  for (size_t i = 0; i <= path.size(); ++i) {
    if (i < path.size() && (static_cast<unsigned char>(path[i]) < 32 ||
                            path[i] == ':' || path[i] == '\\'))
      return false;
    if (i == path.size() || path[i] == '/') {
      auto part = path.substr(start, i - start);
      if (part.empty() || part == "." || part == "..")
        return false;
      start = i + 1;
    }
  }
  return true;
}
bool hash(std::string_view value) {
  return value.size() == 64 &&
         std::all_of(value.begin(), value.end(), [](char c) {
           return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
         });
}
size_t skipSpace(std::string_view text, size_t i) {
  while (i < text.size() && (text[i] == ' ' || text[i] == '\r' ||
                             text[i] == '\n' || text[i] == '\t'))
    ++i;
  return i;
}
// Input is already validated by JsonDocument. This scanner only retains exact
// source spans; it does not interpret numbers or implement a second JSON
// parser.
size_t valueEnd(std::string_view text, size_t i) {
  if (i >= text.size())
    return i;
  if (text[i] == '"') {
    for (++i; i < text.size(); ++i) {
      if (text[i] == '\\') {
        ++i;
        continue;
      }
      if (text[i] == '"')
        return i + 1;
    }
    return text.size();
  }
  if (text[i] == '{' || text[i] == '[') {
    char end = text[i] == '{' ? '}' : ']';
    ++i;
    while (i < text.size()) {
      i = skipSpace(text, i);
      if (text[i] == end)
        return i + 1;
      if (text[i] == ',' || text[i] == ':') {
        ++i;
        continue;
      }
      i = valueEnd(text, i);
    }
    return i;
  }
  while (i < text.size() && text[i] != ',' && text[i] != ']' &&
         text[i] != '}' && text[i] != ' ' && text[i] != '\r' &&
         text[i] != '\n' && text[i] != '\t')
    ++i;
  return i;
}
std::string_view rawMember(std::string_view object, std::string_view name) {
  size_t i = skipSpace(object, 0);
  if (i >= object.size() || object[i++] != '{')
    return {};
  while ((i = skipSpace(object, i)) < object.size() && object[i] != '}') {
    size_t keyEnd = valueEnd(object, i);
    auto key = object.substr(i, keyEnd - i);
    i = skipSpace(object, keyEnd);
    if (i >= object.size() || object[i++] != ':')
      return {};
    i = skipSpace(object, i);
    size_t end = valueEnd(object, i);
    if (key.size() == name.size() + 2 && key.substr(1, name.size()) == name)
      return object.substr(i, end - i);
    i = skipSpace(object, end);
    if (i < object.size() && object[i] == ',')
      ++i;
  }
  return {};
}
std::vector<std::string_view> rawArray(std::string_view array) {
  std::vector<std::string_view> result;
  size_t i = skipSpace(array, 0);
  if (i >= array.size() || array[i++] != '[')
    return result;
  while ((i = skipSpace(array, i)) < array.size() && array[i] != ']') {
    size_t end = valueEnd(array, i);
    result.push_back(array.substr(i, end - i));
    i = skipSpace(array, end);
    if (i < array.size() && array[i] == ',')
      ++i;
  }
  return result;
}
bool uniqueObjects(const JsonDocument &d, const Node &n) {
  if (n.kind != Kind::Object && n.kind != Kind::Array)
    return true;
  std::set<std::string_view> keys;
  for (u32 i = 0; i < n.childCount; ++i) {
    auto &c = *d.child(n, i);
    if (n.kind == Kind::Object && !keys.insert(d.keyOf(c)).second)
      return false;
    if (!uniqueObjects(d, c))
      return false;
  }
  return true;
}
bool number(const JsonDocument &d, const Node &n, std::string_view key,
            float &out) {
  auto *v = d.member(n, key);
  if (!v)
    return false;
  if (v->kind != Kind::Number || !std::isfinite(v->number) ||
      std::abs(v->number) > 1e8)
    return false;
  out = float(v->number);
  return true;
}
bool vector4(const JsonDocument &d, const Node &n, std::string_view key,
             std::array<float, 4> &out) {
  auto *v = d.member(n, key);
  if (!v || v->kind == Kind::Null)
    return true;
  if (v->kind != Kind::Object)
    return false;
  return number(d, *v, "x", out[0]) && number(d, *v, "y", out[1]) &&
         number(d, *v, "z", out[2]) && number(d, *v, "w", out[3]);
}
const TerrainCatalogTexture *findTexture(const TerrainSurfaceCatalog &c,
                                         std::string_view id) {
  for (auto &t : c.textures)
    if (t.id == id)
      return &t;
  return nullptr;
}
const TerrainCatalogMaterial *findMaterial(const TerrainSurfaceCatalog &c,
                                           std::string_view id) {
  for (auto &m : c.materials)
    if (m.id == id)
      return &m;
  return nullptr;
}
int role(std::string_view name) {
  if (name == "albedo")
    return 0;
  if (name == "normal")
    return 1;
  if (name == "height")
    return 2;
  if (name == "ao")
    return 3;
  if (name == "roughness" || name == "smoothness")
    return 4;
  if (name == "metallic")
    return 5;
  if (name == "emission")
    return 6;
  return -1;
}
} // namespace
bool loadTerrainSurfaceCatalog(std::string_view text,
                               TerrainSurfaceCatalog &out, std::string &error) {
  auto fail = [&](const char *s) {
    error = s;
    return false;
  };
  if (text.size() > 16 * 1024 * 1024)
    return fail("Terrain catalog exceeds 16 MiB budget");
  JsonDocument d;
  if (!JsonDocument::parse(text, d) || !d.root() ||
      !uniqueObjects(d, *d.root()))
    return fail("Malformed terrain catalog or duplicate JSON keys");
  auto &root = *d.root();
  auto *textures = d.member(root, "textures");
  auto *materials = d.member(root, "materials");
  if (d.number(root, "version", 0) != 1 ||
      d.number(root, "material_contract_version", 0) != 1 || !textures ||
      !materials || textures->kind != Kind::Array ||
      materials->kind != Kind::Array || textures->childCount > 4096 ||
      materials->childCount > 4096)
    return fail("Unsupported terrain catalog version or inventory size");
  TerrainSurfaceCatalog c;
  std::set<std::string> textureIds, materialIds;
  std::set<AssetGuid> textureGuids;
  for (u32 i = 0; i < textures->childCount; ++i) {
    auto &n = *d.child(*textures, i);
    TerrainCatalogTexture t;
    t.id = d.string(n, "id");
    t.sourcePath = d.string(n, "source");
    t.runtimePath = d.string(n, "runtime");
    t.thumbnailPath = d.string(n, "thumbnail");
    t.originalAssetPath = d.string(n, "source_path");
    t.sourceHash = d.string(n, "source_sha256");
    t.runtimeHash = d.string(n, "runtime_sha256");
    t.archiveHash = d.string(n, "archive_sha256");
    auto *dimensions = d.member(n, "dimensions");
    if (t.id.empty() || t.id.size() > 256 || !textureIds.insert(t.id).second ||
        !AssetGuid::parse(d.string(n, "guid"), t.identity) ||
        !t.identity.valid() || !textureGuids.insert(t.identity).second ||
        !safePath(t.sourcePath) || !safePath(t.runtimePath) ||
        !safePath(t.thumbnailPath) || !safePath(t.originalAssetPath) ||
        !hash(t.sourceHash) || !hash(t.runtimeHash) || !hash(t.archiveHash) ||
        !dimensions || dimensions->kind != Kind::Array ||
        dimensions->childCount != 2)
      return fail("Invalid texture identity, hashes, paths or dimensions");
    auto *w = d.child(*dimensions, 0);
    auto *h = d.child(*dimensions, 1);
    if (w->kind != Kind::Number || h->kind != Kind::Number || w->number < 1 ||
        w->number > 65536 || h->number < 1 || h->number > 65536 ||
        std::floor(w->number) != w->number ||
        std::floor(h->number) != h->number)
      return fail("Invalid terrain texture dimensions");
    t.width = u32(w->number);
    t.height = u32(h->number);
    c.textures.push_back(std::move(t));
  }
  auto rawMaterials = rawArray(rawMember(text, "materials"));
  if (rawMaterials.size() != materials->childCount)
    return fail("Cannot retain exact material source JSON spans");
  for (u32 i = 0; i < materials->childCount; ++i) {
    auto &n = *d.child(*materials, i);
    TerrainCatalogMaterial m;
    m.id = d.string(n, "id");
    m.package = d.string(n, "package");
    m.sourcePath = d.string(n, "source");
    if (m.id.empty() || m.id.size() > 256 || !materialIds.insert(m.id).second ||
        !safePath(m.sourcePath))
      return fail("Invalid or duplicate material identity/path");
    m.layer.identity =
        assetGuidFromSeed("astra.terrain.catalog.material:" + m.id);
    m.layer.name = d.string(n, "name");
    m.sourcePropertiesJson =
        std::string(rawMember(rawMaterials[i], "source_properties"));
    m.packingJson = std::string(rawMember(rawMaterials[i], "packing"));
    m.resolvedMapsJson =
        std::string(rawMember(rawMaterials[i], "resolved_maps"));
    m.layer.sourceCatalogId = m.id;
    m.layer.sourcePropertiesJson = m.sourcePropertiesJson;
    m.layer.sourcePackingJson = m.packingJson;
    m.layer.sourceResolvedMapsJson = m.resolvedMapsJson;
    if (m.sourcePropertiesJson.empty())
      return fail("Missing lossless source properties JSON");
    auto *defaults = d.member(n, "defaults");
    auto *maps = d.member(n, "resolved_maps");
    if (!defaults || defaults->kind != Kind::Object || !maps ||
        maps->kind != Kind::Object)
      return fail("Missing resolved material channel contracts");
    auto *size = d.member(*defaults, "tile_size");
    auto *offset = d.member(*defaults, "tile_offset");
    if (!size || !offset || size->kind != Kind::Object ||
        offset->kind != Kind::Object ||
        !number(d, *size, "x", m.layer.scaleX) ||
        !number(d, *size, "y", m.layer.scaleY) ||
        !number(d, *offset, "x", m.layer.offsetX) ||
        !number(d, *offset, "y", m.layer.offsetY) ||
        !number(d, *defaults, "normal_strength", m.layer.maps[1].factor) ||
        !number(d, *defaults, "roughness", m.layer.maps[4].factor) ||
        !number(d, *defaults, "metallic", m.layer.maps[5].factor) ||
        !vector4(d, *defaults, "albedo_remap_min",
                 m.layer.maps[0].colorRemapMin) ||
        !vector4(d, *defaults, "albedo_remap_max",
                 m.layer.maps[0].colorRemapMax))
      return fail("Invalid material default numeric values");
    std::array<float, 4> maskMin{0, 0, 0, 0}, maskMax{1, 1, 1, 1};
    if (!vector4(d, *defaults, "mask_remap_min", maskMin) ||
        !vector4(d, *defaults, "mask_remap_max", maskMax))
      return fail("Invalid packed mask remap");
    const int maskRoles[]{5, 3, 2, 4};
    for (size_t j = 0; j < 4; ++j) {
      m.layer.maps[maskRoles[j]].remapMin = maskMin[j];
      m.layer.maps[maskRoles[j]].remapMax = maskMax[j];
    }
    for (u32 j = 0; j < maps->childCount; ++j) {
      auto &mapNode = *d.child(*maps, j);
      auto name = d.keyOf(mapNode);
      int r = role(name);
      TerrainSurfaceMap *target = nullptr;
      if (r >= 0)
        target = &m.layer.maps[size_t(r)];
      else if (name == "noiseNormal")
        target = &m.layer.normalNoise[0].map;
      else if (name == "detailNoise")
        target = &m.layer.detailNoise.map;
      else if (name == "distanceNoise")
        target = &m.layer.distanceNoise.map;
      std::string textureId(d.string(mapNode, "texture"));
      auto *texture = findTexture(c, textureId);
      if (!texture) {
        m.diagnostics.push_back("Missing texture reference for " +
                                std::string(name) + ": " + textureId);
        continue;
      }
      if (std::find(m.requiredTextureIds.begin(), m.requiredTextureIds.end(),
                    textureId) == m.requiredTextureIds.end())
        m.requiredTextureIds.push_back(textureId);
      if (!target) {
        m.diagnostics.push_back(
            "Unsupported mapped role retained in source metadata: " +
            std::string(name));
        continue;
      }
      target->texture = texture->identity;
      auto space = d.string(mapNode, "color_space");
      if (space != "linear" && space != "sRGB")
        return fail("Unknown material channel color space");
      target->linear = space == "linear";
      auto *channelNode = d.member(mapNode, "channel");
      if (channelNode && channelNode->kind != Kind::Null) {
        if (channelNode->kind != Kind::Number || channelNode->number < 0 ||
            channelNode->number > 3 ||
            std::floor(channelNode->number) != channelNode->number)
          return fail("Invalid texture channel mapping");
        target->channel = TerrainSurfaceChannel(u32(channelNode->number));
      } else
        target->channel = (r == 1 || name == "noiseNormal")
                              ? TerrainSurfaceChannel::NormalRGB
                          : (r == 0 || r == 6) ? TerrainSurfaceChannel::RGB
                                               : TerrainSurfaceChannel::R;
      auto *invert = d.member(mapNode, "roughness_invert");
      if (invert) {
        if (invert->kind != Kind::Boolean)
          return fail("Invalid roughness inversion flag");
        target->invert = invert->boolean;
      }
      // MicroSplat's resolved texture maps replace the constant fallback;
      // Unity TerrainLayer's normal strength remains a multiplier.
      if (r >= 0 && r != 1)
        target->factor = 1;
      if (name == "noiseNormal" || name == "detailNoise" ||
          name == "distanceNoise")
        m.diagnostics.push_back(
            "Noise texture retained; activation/scale/strength require "
            "explicit authoring: " +
            std::string(name));
    }
    if (auto *clusters = d.member(n, "cluster_variants")) {
      if (clusters->kind != Kind::Array || clusters->childCount > 3)
        return fail("Invalid cluster reference count");
      for (u32 j = 0; j < clusters->childCount; ++j) {
        auto *id = d.child(*clusters, j);
        if (id->kind != Kind::String)
          return fail("Invalid cluster reference identity");
        m.clusterIds.emplace_back(d.textOf(*id));
      }
    }
    c.materials.push_back(std::move(m));
  }
  for (auto &m : c.materials) {
    if (m.clusterIds.empty())
      continue;
    std::set<AssetGuid> albedos, identities;
    bool valid = true;
    for (auto &id : m.clusterIds) {
      auto *variant = findMaterial(c, id);
      if (!variant || !identities.insert(variant->layer.identity).second ||
          !variant->layer.maps[0].texture.valid() ||
          variant->layer.scaleX != variant->layer.scaleY ||
          !albedos.insert(variant->layer.maps[0].texture).second) {
        valid = false;
        break;
      }
      TerrainSurfaceVariant v;
      v.identity = variant->layer.identity;
      v.maps = variant->layer.maps;
      v.scale = variant->layer.scaleX;
      v.offsetX = variant->layer.offsetX;
      v.offsetY = variant->layer.offsetY;
      v.rotation = variant->layer.rotation;
      m.layer.cluster.variants.push_back(v);
      for (auto &textureId : variant->requiredTextureIds)
        if (std::find(m.requiredTextureIds.begin(), m.requiredTextureIds.end(),
                      textureId) == m.requiredTextureIds.end())
          m.requiredTextureIds.push_back(textureId);
    }
    if (valid && m.clusterIds.size() >= 2)
      m.layer.cluster.enabled = true;
    else if (valid)
      m.layer.cluster.variants.clear();
    else {
      m.layer.cluster.variants.clear();
      m.diagnostics.push_back(
          "Cluster references missing, repeated, anisotropic "
          "or not distinct; cluster remains disabled");
    }
  }
  for (auto &m : c.materials) {
    TerrainSurfaceAsset a;
    a.layers = {m.layer};
    std::string validation;
    if (!validTerrainSurface(a, validation))
      return fail(
          ("Invalid converted material: " + m.id + ": " + validation).c_str());
  }
  out = std::move(c);
  error.clear();
  return true;
}
bool makeTerrainSurface(const TerrainSurfaceCatalog &c,
                        std::span<const std::string> ids,
                        TerrainSurfaceSelection &out, std::string &error) {
  if (ids.empty() || ids.size() > 32) {
    error = "Select 1..32 catalog material identities";
    return false;
  }
  TerrainSurfaceSelection result;
  std::set<std::string> selected, required;
  for (auto &id : ids) {
    auto *m = findMaterial(c, id);
    if (!m || !selected.insert(id).second) {
      error = "Unknown or repeated selected catalog material identity";
      return false;
    }
    result.surface.layers.push_back(m->layer);
    for (auto &diagnostic : m->diagnostics)
      result.diagnostics.push_back(id + ": " + diagnostic);
    for (auto &textureId : m->requiredTextureIds)
      if (required.insert(textureId).second) {
        auto *t = findTexture(c, textureId);
        if (!t) {
          error = "Unresolved required catalog texture identity";
          return false;
        }
        result.requiredTextures.push_back(*t);
      }
  }
  if (!validTerrainSurface(result.surface, error))
    return false;
  out = std::move(result);
  error.clear();
  return true;
}
} // namespace ae::resources
