#include "resources/terrain_surface_asset.h"
#include <cmath>
#include <iomanip>
#include <locale>
#include <set>
#include <sstream>
#include <type_traits>

namespace ae::resources {
namespace {
template <class A> void fields(A &a, TerrainSurfaceMap &v) {
  a(v.texture, v.channel, v.linear, v.invert, v.factor, v.remapMin, v.remapMax,
    v.colorFactor, v.colorRemapMin, v.colorRemapMax);
}
template <class A> void fields(A &a, TerrainSurfaceNoise &v) {
  a(v.enabled, v.map, v.scale, v.strength, v.fadeStart, v.fadeEnd,
    v.projection);
}
template <class A> void fields(A &a, TerrainSurfaceVariant &v) {
  a(v.identity, v.maps, v.scale, v.offsetX, v.offsetY, v.rotation);
}
template <class A> void fields(A &a, TerrainAntiTileEntry &v) {
  a(v.identity, v.normal, v.detail, v.distance);
}
template <class A> void fields(A &a, TerrainAntiTileArray &v) {
  a(v.enabled, v.entries);
}
template <class A> void fields(A &a, TerrainSurfaceCluster &v) {
  a(v.enabled, v.stochastic, v.variants, v.selection, v.contrast, v.noiseBoost,
    v.quality);
}
template <class A> void fields(A &a, TerrainSurfaceLayer &v) {
  a(v.identity, v.name, v.sourceCatalogId, v.sourcePropertiesJson,
    v.sourcePackingJson, v.sourceResolvedMapsJson, v.active, v.thumbnail,
    v.maps, v.scaleX, v.scaleY, v.offsetX, v.offsetY, v.rotation, v.tint,
    v.saturation, v.projection, v.axisContrast, v.faceNormals, v.heightBlend,
    v.heightContrast, v.heightWidth, v.heightInfluence, v.heightQuality,
    v.detailNoise, v.distanceNoise, v.normalNoise, v.globalDetailParticipation,
    v.globalDistanceParticipation, v.globalNormalParticipation, v.antiTileEntry,
    v.antiTileParticipation, v.resampleAlbedo, v.resampleNormal,
    v.resampleHeightBlend, v.resampleScale, v.resampleStart, v.resampleEnd,
    v.resampleBlend, v.resampleNoise, v.cluster);
}
template <class A> void fields(A &a, TerrainSurfaceAsset &v) {
  a(v.layers, v.detailNoise, v.distanceNoise, v.normalNoise, v.antiTileArray);
}
struct Writer {
  std::ostringstream s;
  Writer() {
    s.imbue(std::locale::classic());
    s << std::setprecision(9);
  }
  template <class... T> void operator()(T &...v) { (one(v), ...); }
  void one(AssetGuid &v) {
    one(v.high);
    one(v.low);
  }
  void one(std::string &v) { s << std::quoted(v) << ' '; }
  template <class T, size_t N> void one(std::array<T, N> &v) {
    for (auto &x : v)
      one(x);
  }
  template <class T> void one(std::vector<T> &v) {
    s << v.size() << ' ';
    for (auto &x : v)
      one(x);
  }
  template <class T> void one(T &v) {
    if constexpr (std::is_enum_v<T>)
      s << u32(v) << ' ';
    else if constexpr (std::is_arithmetic_v<T>)
      s << v << ' ';
    else
      fields(*this, v);
  }
};
struct Reader {
  std::istringstream s;
  bool ok = true;
  explicit Reader(std::string_view text) : s(std::string(text)) {
    s.imbue(std::locale::classic());
  }
  template <class... T> void operator()(T &...v) { (one(v), ...); }
  void one(AssetGuid &v) {
    one(v.high);
    one(v.low);
  }
  void one(std::string &v) {
    s >> std::ws;
    if (s.peek() != '"') {
      ok = false;
      return;
    }
    if (!(s >> std::quoted(v)) || v.size() > 64 * 1024)
      ok = false;
  }
  template <class T, size_t N> void one(std::array<T, N> &v) {
    for (auto &x : v)
      one(x);
  }
  template <class T> void one(std::vector<T> &v) {
    u32 count = 0;
    one(count);
    if (count > 32) {
      ok = false;
      return;
    }
    v.resize(count);
    for (auto &x : v)
      one(x);
  }
  template <class T> void one(T &v) {
    if (!ok)
      return;
    if constexpr (std::is_enum_v<T>) {
      u32 n = 0;
      one(n);
      v = T(n);
    } else if constexpr (std::is_arithmetic_v<T>) {
      if constexpr (std::is_unsigned_v<T>) {
        s >> std::ws;
        if (s.peek() < '0' || s.peek() > '9') {
          ok = false;
          return;
        }
      }
      if (!(s >> v))
        ok = false;
    } else
      fields(*this, v);
  }
};
bool range(float v, float low, float high) {
  return std::isfinite(v) && v >= low && v <= high;
}
bool projection(TerrainProjection p) { return u32(p) <= 2; }
bool quality(TerrainSurfaceQuality q) { return u32(q) <= 2; }
bool map(const TerrainSurfaceMap &m) {
  if (!(u32(m.channel) <= 6 && range(m.factor, 0, 64) &&
        range(m.remapMin, -65536, 65536) && range(m.remapMax, -65536, 65536) &&
        m.remapMin <= m.remapMax))
    return false;
  for (size_t i = 0; i < 4; ++i)
    if (!range(m.colorFactor[i], 0, 64) ||
        !range(m.colorRemapMin[i], -65536, 65536) ||
        !range(m.colorRemapMax[i], -65536, 65536) ||
        m.colorRemapMin[i] > m.colorRemapMax[i])
      return false;
  return true;
}
bool noise(const TerrainSurfaceNoise &n) {
  return map(n.map) && projection(n.projection) &&
         range(n.scale, .0001f, 65536) && range(n.strength, 0, 64) &&
         range(n.fadeStart, 0, 1e8f) && range(n.fadeEnd, 0, 1e8f) &&
         n.fadeEnd > n.fadeStart && (!n.enabled || n.map.texture.valid());
}
template <class F> void refs(const TerrainSurfaceAsset &a, F f) {
  auto visitNoise = [&](AssetGuid layer, const TerrainSurfaceNoise &n,
                        const std::string &loc) {
    if (n.map.texture.valid())
      f(layer, n.map.texture, loc);
  };
  visitNoise({}, a.detailNoise, "global.detail");
  visitNoise({}, a.distanceNoise, "global.distance");
  for (size_t i = 0; i < 3; ++i)
    visitNoise({}, a.normalNoise[i], "global.normal." + std::to_string(i));
  for (auto &e : a.antiTileArray.entries) {
    visitNoise(e.identity, e.normal, "antiTile.normal");
    visitNoise(e.identity, e.detail, "antiTile.detail");
    visitNoise(e.identity, e.distance, "antiTile.distance");
  }
  for (auto &l : a.layers) {
    if (l.thumbnail.valid())
      f(l.identity, l.thumbnail, "thumbnail");
    for (size_t i = 0; i < 7; ++i)
      if (l.maps[i].texture.valid())
        f(l.identity, l.maps[i].texture, "channel." + std::to_string(i));
    visitNoise(l.identity, l.detailNoise, "detail");
    visitNoise(l.identity, l.distanceNoise, "distance");
    visitNoise(l.identity, l.resampleNoise, "resample");
    visitNoise(l.identity, l.cluster.selection, "cluster.selection");
    for (size_t i = 0; i < 3; ++i)
      visitNoise(l.identity, l.normalNoise[i], "normal." + std::to_string(i));
    for (size_t v = 0; v < l.cluster.variants.size(); ++v)
      for (size_t c = 0; c < 7; ++c)
        if (l.cluster.variants[v].maps[c].texture.valid())
          f(l.identity, l.cluster.variants[v].maps[c].texture,
            "cluster." + std::to_string(v) + ".channel." + std::to_string(c));
  }
}
} // namespace
bool validTerrainSurface(const TerrainSurfaceAsset &a, std::string &error) {
  auto fail = [&](const char *s) {
    error = s;
    return false;
  };
  if (a.layers.empty() || a.layers.size() > 32)
    return fail("Surface requires 1..32 layers");
  if (!noise(a.detailNoise) || !noise(a.distanceNoise))
    return fail("Invalid global noise texture or transition");
  for (auto &n : a.normalNoise)
    if (!noise(n))
      return fail("Invalid global normal noise");
  std::set<AssetGuid> antiTileIds;
  if (a.antiTileArray.entries.size() > 32 ||
      (a.antiTileArray.enabled && a.antiTileArray.entries.empty()))
    return fail("Invalid anti-tile array size");
  for (auto &entry : a.antiTileArray.entries) {
    if (!entry.identity.valid() || !antiTileIds.insert(entry.identity).second ||
        !noise(entry.normal) || !noise(entry.detail) || !noise(entry.distance))
      return fail("Invalid anti-tile entry identity/maps");
    if (!entry.normal.enabled && !entry.detail.enabled &&
        !entry.distance.enabled)
      return fail("Anti-tile array entry requires a source map");
  }
  std::set<AssetGuid> ids;
  for (auto &l : a.layers) {
    if (!l.identity.valid() || !ids.insert(l.identity).second ||
        l.name.size() > 128 || l.name.find('\0') != std::string::npos)
      return fail("Invalid or duplicate surface layer identity/name");
    if (l.sourceCatalogId.size() > 256 ||
        l.sourceCatalogId.find('\0') != std::string::npos)
      return fail("Invalid surface source catalog identity");
    for (const auto *raw : {&l.sourcePropertiesJson, &l.sourcePackingJson,
                            &l.sourceResolvedMapsJson})
      if (raw->size() > 64 * 1024 || raw->find('\0') != std::string::npos)
        return fail("Surface source provenance exceeds 64 KiB or contains NUL");
    for (auto &m : l.maps)
      if (!map(m))
        return fail("Invalid layer channel mapping or factor");
    if (!range(l.scaleX, .0001f, 65536) || !range(l.scaleY, .0001f, 65536) ||
        !range(l.offsetX, -1e8f, 1e8f) || !range(l.offsetY, -1e8f, 1e8f) ||
        !range(l.rotation, -36000, 36000) || !range(l.saturation, 0, 8) ||
        !projection(l.projection) || !range(l.axisContrast, .01f, 64))
      return fail("Invalid surface projection/transform");
    for (float t : l.tint)
      if (!range(t, 0, 64))
        return fail("Invalid surface tint");
    if (!range(l.heightContrast, .0001f, 64) ||
        !range(l.heightWidth, .0001f, 65536) ||
        !range(l.heightInfluence, 0, 1) || !quality(l.heightQuality))
      return fail("Invalid height blend");
    if (!noise(l.detailNoise) || !noise(l.distanceNoise) ||
        !noise(l.resampleNoise))
      return fail("Invalid layer noise texture or transition");
    for (auto &n : l.normalNoise)
      if (!noise(n))
        return fail("Invalid layer normal noise");
    if (!range(l.globalDetailParticipation, 0, 1) ||
        !range(l.globalDistanceParticipation, 0, 1))
      return fail("Invalid global noise participation");
    for (float p : l.globalNormalParticipation)
      if (!range(p, 0, 1))
        return fail("Invalid normal noise participation");
    if (l.antiTileEntry.valid() && !antiTileIds.contains(l.antiTileEntry))
      return fail("Unresolved anti-tile array entry identity");
    for (float p : l.antiTileParticipation)
      if (!range(p, 0, 1))
        return fail("Invalid anti-tile participation");
    if (!range(l.resampleScale, .0001f, 65536) ||
        !range(l.resampleStart, 0, 1e8f) || !range(l.resampleEnd, 0, 1e8f) ||
        l.resampleEnd <= l.resampleStart || u32(l.resampleBlend) > 2)
      return fail("Invalid distance resampling transition");
    if ((l.resampleAlbedo || l.resampleNormal) &&
        l.resampleBlend == TerrainDistanceBlend::Noise &&
        !l.resampleNoise.enabled)
      return fail("Noise resampling requires enabled noise texture");
    auto &c = l.cluster;
    if (!noise(c.selection) || !range(c.contrast, .0001f, 64) ||
        !range(c.noiseBoost, 0, 64) || !quality(c.quality) ||
        c.variants.size() > 3 || (c.enabled && c.variants.size() < 2))
      return fail("Invalid cluster variant count/noise/quality");
    std::set<AssetGuid> variants, albedos;
    for (auto &v : c.variants) {
      if (!v.identity.valid() || !variants.insert(v.identity).second ||
          !v.maps[0].texture.valid() ||
          !albedos.insert(v.maps[0].texture).second)
        return fail("Cluster requires distinct variant and albedo identities");
      if (!range(v.scale, .0001f, 65536) || !range(v.offsetX, -1e8f, 1e8f) ||
          !range(v.offsetY, -1e8f, 1e8f) || !range(v.rotation, -36000, 36000))
        return fail("Invalid cluster transform");
      for (auto &m : v.maps)
        if (!map(m))
          return fail("Invalid cluster channel");
    }
  }
  error.clear();
  return true;
}
bool serializeTerrainSurface(const TerrainSurfaceAsset &a, std::string &out,
                             std::string &error) {
  if (!validTerrainSurface(a, error))
    return false;
  auto copy = a;
  Writer w;
  w.s << "AETERRAINSURFACE1 ";
  w.one(copy);
  auto serialized = w.s.str();
  if (serialized.size() > 1024 * 1024) {
    error = "Surface metadata exceeds 1 MiB byte budget";
    return false;
  }
  out = std::move(serialized);
  return true;
}
bool deserializeTerrainSurface(std::string_view text, TerrainSurfaceAsset &out,
                               std::string &error) {
  if (text.size() > 1024 * 1024 || text.find('\0') != std::string_view::npos) {
    error = "Surface metadata exceeds byte budget or contains NUL";
    return false;
  }
  Reader r(text);
  std::string version;
  r.s >> version;
  if (version != "AETERRAINSURFACE1") {
    error = "Unsupported terrain surface version";
    return false;
  }
  TerrainSurfaceAsset a;
  r.one(a);
  r.s >> std::ws;
  if (!r.ok || !r.s.eof()) {
    error = "Invalid/truncated terrain surface or trailing tokens";
    return false;
  }
  if (!validTerrainSurface(a, error))
    return false;
  out = std::move(a);
  return true;
}
bool resolveTerrainSurface(const TerrainSurfaceAsset &a,
                           const std::function<bool(AssetGuid)> &exists,
                           std::vector<TerrainSurfaceDiagnostic> &out,
                           std::string &error) {
  if (!validTerrainSurface(a, error))
    return false;
  if (!exists) {
    error = "Texture identity resolver is required";
    return false;
  }
  std::vector<TerrainSurfaceDiagnostic> diagnostics;
  refs(a, [&](AssetGuid layer, AssetGuid texture, const std::string &loc) {
    if (!exists(texture))
      diagnostics.push_back({layer, texture, loc});
  });
  out = std::move(diagnostics);
  if (!out.empty()) {
    error = "Unresolved terrain surface texture references";
    return false;
  }
  error.clear();
  return true;
}
std::vector<AssetGuid>
activeTerrainSurfaceLayers(const TerrainSurfaceAsset &a) {
  std::vector<AssetGuid> out;
  for (auto &l : a.layers)
    if (l.active)
      out.push_back(l.identity);
  return out;
}
} // namespace ae::resources
