#include "resources/terrain_surface_asset.h"
#include "resources/animation_curve_domain.h"
#include "resources/terrain_surface_map_visitor.h"
#include <algorithm>
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
template <class A> void fields(A &a, AnimationCurveKey &v){
  a(v.id,v.time,v.value,v.inSlope,v.outSlope,v.inWeight,v.outWeight,v.incoming,v.outgoing,v.broken,v.weightedIn,v.weightedOut);
}
template <class A> void fields(A &a, AnimationCurve &v){a(v.keys);}
template <class A> void fields(A &a, TerrainGeoSettings &v){
  a(v.mode,v.albedo,v.normal,v.blend,v.blendStrength,v.normalStrength,v.scaleMeters,v.offsetMeters,
    v.rangeEnabled,v.range,v.curveEnabled,v.curve,v.curveScaleMeters,v.curveOffsetMeters,v.curveRotationDegrees,v.heightContrast);
}
template <class A> void fields(A &a, TerrainGlobalMap &v){
  a(v.enabled,v.map,v.intensity,v.fadeStart,v.fadeEnd,v.nearStrength,v.farStrength,v.scaleX,v.scaleY,v.offsetX,v.offsetY,
    v.blend,v.normalBlend,v.smoothness,v.ao,v.metallic,v.mask);
}
template<class A> void fields(A &a,TerrainSnowSettings &v){
  a(v.mode,v.diffuse,v.height,v.normal,v.smoothness,v.ao,v.normalNoiseMap,v.mask,v.sparkleNoiseMap,
    v.upVector,v.tint,v.uvScales,v.amount,v.heightAngleRange,v.parameters,v.normalNoiseParams,v.distanceResample,
    v.stochasticScale,v.stochasticContrast,v.normalNoiseEnabled,v.distanceResampleEnabled,v.perLayerEnabled,
    v.maskEnabled,v.worldSpaceUV,v.stochasticEnabled,v.triplanarEnabled,v.useGlobalLevel,v.useGlobalHeight,
    v.sssEnabled,v.rimEnabled,v.sparkleEnabled,v.sssTint,v.rimColor,v.sparkleTint,v.rimPower,
    v.sparkleEmission,v.sparkleStrength,v.sparkleSize,v.sparkleDensity,v.sparkleNoiseDensity,
    v.sparkleNoiseAmplitude,v.sparkleViewDependency,v.sourcePropertiesJson,v.sourceKeywordsJson);
}
template<class A> void fields(A &a,TerrainTextureResidency &v){a(v.mode,v.maxSide);}
template <class A> void extensionLayer(A &a,TerrainSurfaceLayer &v){
  a(v.geoParticipation,v.geoHeightFilterEnabled,v.geoHeightThreshold,v.geoHeightStrength,v.globalMapParticipation);
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
  void one(AnimationCurve &v){
    u32 count=0;one(count);
    if(!ok||count>AnimationCurve::MaximumKeys){ok=false;return;}
    v.keys.resize(count);for(auto &key:v.keys)one(key);
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
  if (!(u32(m.channel) <= 7 && validTerrainTextureResidency(m.textureResidency) && range(m.factor, 0, 64) &&
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
  const auto visitMap=[&](const TerrainSurfaceMap &m,const std::string &location){if(m.texture.valid())f({},m.texture,location);};
  visitMap(a.geo.albedo,"geo.albedo");visitMap(a.geo.normal,"geo.normal");
  visitMap(a.snow.diffuse,"snow.diffuse");visitMap(a.snow.height,"snow.height");visitMap(a.snow.normal,"snow.normal");
  visitMap(a.snow.smoothness,"snow.smoothness");visitMap(a.snow.ao,"snow.ao");visitMap(a.snow.normalNoiseMap,"snow.normalNoise");
  visitMap(a.snow.mask,"snow.mask");visitMap(a.snow.sparkleNoiseMap,"snow.sparkleNoise");
  for(usize i=0;i<a.globalMaps.size();++i){
    const auto &g=a.globalMaps[i];const auto location="globalMap."+std::to_string(i);
    visitMap(g.map,location);visitMap(g.smoothness,location+".smoothness");visitMap(g.ao,location+".ao");visitMap(g.metallic,location+".metallic");visitMap(g.mask,location+".mask");
  }
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
  if(!validTerrainTextureResidency(a.textureResidency,true))return fail("Invalid terrain texture residency policy");
  const auto &snow=a.snow;
  if(u32(snow.mode)>2)return fail("Invalid Snow mode");
  bool snowMaps=true;for(const auto *m:{&snow.diffuse,&snow.height,&snow.normal,&snow.smoothness,&snow.ao,&snow.normalNoiseMap,&snow.mask,&snow.sparkleNoiseMap})snowMaps=snowMaps&&map(*m);
  if(!snowMaps)return fail("Invalid Snow channel mapping");
  for(const auto *v:{&snow.upVector,&snow.tint,&snow.uvScales,&snow.heightAngleRange,&snow.parameters,&snow.normalNoiseParams,&snow.distanceResample,&snow.sssTint,&snow.rimColor,&snow.sparkleTint})
    for(float x:*v)if(!range(x,-1e8f,1e8f))return fail("Invalid Snow vector");
  const float upLength=snow.upVector[0]*snow.upVector[0]+snow.upVector[1]*snow.upVector[1]+snow.upVector[2]*snow.upVector[2];
  if(upLength<1e-12f||!std::isfinite(upLength)||!range(snow.amount,0,1)||snow.heightAngleRange[1]<=snow.heightAngleRange[0]||
    snow.heightAngleRange[3]<snow.heightAngleRange[2]||snow.distanceResample[3]<=snow.distanceResample[2]||
    !range(snow.stochasticScale,.0001f,65536)||!range(snow.stochasticContrast,0,64)||!range(snow.rimPower,0,128))return fail("Invalid Snow direction/transition");
  for(float x:snow.parameters)if(!range(x,0,1))return fail("Snow clearing/age/melt must be in [0,1]");
  for(const auto *color:{&snow.tint,&snow.sssTint,&snow.rimColor,&snow.sparkleTint})for(float x:*color)if(!range(x,0,64))return fail("Snow colors must be nonnegative and bounded");
  if(!range(snow.normalNoiseParams[0],-65536,65536)||!range(snow.normalNoiseParams[1],0,64)||
     !range(snow.distanceResample[0],-65536,65536)||!range(snow.distanceResample[1],0,1)||!range(snow.distanceResample[2],0,1e8f)||
     !range(snow.heightAngleRange[2],-1,1)||!range(snow.heightAngleRange[3],-1,1))return fail("Invalid Snow noise/resampling or angle range");
  for(float x:{snow.sparkleEmission,snow.sparkleStrength,snow.sparkleSize,snow.sparkleDensity,snow.sparkleNoiseDensity,snow.sparkleNoiseAmplitude,snow.sparkleViewDependency})
    if(!range(x,0,65536))return fail("Invalid Snow sparkle parameter");
  for(const auto *raw:{&snow.sourcePropertiesJson,&snow.sourceKeywordsJson})if(raw->size()>64*1024||raw->find('\0')!=std::string::npos)return fail("Invalid Snow source provenance");
  if(snow.mode!=TerrainSnowMode::Off){
    for(const auto *m:{&snow.diffuse,&snow.height,&snow.normal,&snow.smoothness,&snow.ao})if(!m->texture.valid())return fail("Snow requires diffuse/height/normal/smoothness/AO sources");
    if(snow.normal.channel!=TerrainSurfaceChannel::NormalRGB&&snow.normal.channel!=TerrainSurfaceChannel::NormalRG&&snow.normal.channel!=TerrainSurfaceChannel::NormalGA)return fail("Snow requires explicit RGB/RG/GA normal packing");
    if((snow.normalNoiseEnabled&&!snow.normalNoiseMap.texture.valid())||(snow.maskEnabled&&!snow.mask.texture.valid())||(snow.sparkleEnabled&&!snow.sparkleNoiseMap.texture.valid()))return fail("Enabled Snow detail requires source");
  }
  const auto &geo=a.geo;
  if(u32(geo.mode)>2||!map(geo.albedo)||!map(geo.normal)||
    (geo.blend!=TerrainColorBlend::Multiply2x&&geo.blend!=TerrainColorBlend::LighterColor)||
    !range(geo.blendStrength,0,1)||!range(geo.normalStrength,0,64)||!range(geo.scaleMeters,.0001f,1e8f)||!range(geo.offsetMeters,-1e8f,1e8f)||
    !range(geo.curveScaleMeters,.0001f,1e8f)||!range(geo.curveOffsetMeters,-1e8f,1e8f)||!range(geo.curveRotationDegrees,-36000,36000)||!range(geo.heightContrast,0,65536))return fail("Invalid Geo parameters");
  if(geo.mode!=TerrainGeoMode::Off&&!geo.albedo.texture.valid())return fail("Geo requires albedo source");
  if(geo.mode==TerrainGeoMode::AlbedoNormal&&(!geo.normal.texture.valid()||
    (geo.normal.channel!=TerrainSurfaceChannel::NormalRGB&&geo.normal.channel!=TerrainSurfaceChannel::NormalRG&&geo.normal.channel!=TerrainSurfaceChannel::NormalGA)))return fail("Geo requires RGB/RG/GA normal source");
  for(usize i=0;i<geo.range.size();++i)if(!range(geo.range[i],-1e8f,1e8f)||(i&&geo.range[i]<geo.range[i-1]))return fail("Invalid Geo height range");
  if(!geo.curve.keys.empty()&&!validAnimationCurveDomain(geo.curve,AnimationCurveDomain::Signed))return fail("Invalid signed Geo curve");
  std::set<u64> curveIds;
  for(const auto &key:geo.curve.keys)if(!curveIds.insert(key.id).second||!range(key.value,-1e8f,1e8f)||!range(key.inSlope,-1e8f,1e8f)||!range(key.outSlope,-1e8f,1e8f))return fail("Invalid Geo curve key identity/value");
  for(usize role=0;role<a.globalMaps.size();++role){
    const auto &g=a.globalMaps[role];
    if(!map(g.map)||!map(g.smoothness)||!map(g.ao)||!map(g.metallic)||!map(g.mask)||u32(g.blend)>3||u32(g.normalBlend)>1||
      !range(g.intensity,0,64)||!range(g.fadeStart,0,1e8f)||!range(g.fadeEnd,0,1e8f)||g.fadeEnd<=g.fadeStart||
      !range(g.nearStrength,0,64)||!range(g.farStrength,0,64)||!range(g.scaleX,-65536,65536)||!range(g.scaleY,-65536,65536)||
      !range(g.offsetX,-1e8f,1e8f)||!range(g.offsetY,-1e8f,1e8f))return fail("Invalid global map parameters");
    if(g.enabled&&role!=2&&!g.map.texture.valid())return fail("Enabled global map requires source");
    if(g.enabled&&role==2&&!g.smoothness.texture.valid()&&!g.ao.texture.valid()&&!g.metallic.texture.valid())return fail("Enabled global SAOM requires source");
    if(g.enabled&&role==1&&g.map.channel!=TerrainSurfaceChannel::NormalRGB&&g.map.channel!=TerrainSurfaceChannel::NormalRG&&g.map.channel!=TerrainSurfaceChannel::NormalGA)return fail("Global normal requires RGB/RG/GA packing");
  }
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
    if(!range(l.snowParticipation,0,1)||!range(l.snowPorosity,0,1))return fail("Invalid Snow layer participation/porosity");
    if (!l.identity.valid() || !ids.insert(l.identity).second ||
        l.name.size() > 128 || l.name.find('\0') != std::string::npos)
      return fail("Invalid or duplicate surface layer identity/name");
    if(!range(l.geoParticipation,0,1)||!range(l.geoHeightThreshold,-1,1)||!range(l.geoHeightStrength,0,1))return fail("Invalid layer Geo participation/filter");
    for(usize i=0;i<l.globalMapParticipation.size();++i)if(!range(l.globalMapParticipation[i],0,i==0?1.f:2.f))return fail("Invalid layer global map participation");
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
  w.s << "AETERRAINSURFACE3 ";
  w.one(copy);
  u32 extensionVersion=1,count=u32(copy.layers.size());w.one(extensionVersion);w.one(copy.geo);w.one(copy.globalMaps);w.one(count);
  for(auto &layer:copy.layers){w.one(layer.identity);extensionLayer(w,layer);}
  u32 snowVersion=1;w.one(snowVersion);w.one(copy.snow);w.one(copy.textureResidency);
  for(auto &layer:copy.layers){w.one(layer.identity);w.one(layer.snowParticipation);w.one(layer.snowPorosity);}
  u32 mapCount=0;visitTerrainSurfaceMaps(copy,[&](auto &){++mapCount;});w.one(mapCount);
  visitTerrainSurfaceMaps(copy,[&](auto &m){w.one(m.texture);w.one(m.textureResidency);});
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
  if (version != "AETERRAINSURFACE1"&&version!="AETERRAINSURFACE2"&&version!="AETERRAINSURFACE3") {
    error = "Unsupported terrain surface version";
    return false;
  }
  TerrainSurfaceAsset a;
  r.one(a);
  if(version!="AETERRAINSURFACE1"){
    u32 extensionVersion=0,count=0;r.one(extensionVersion);
    if(!r.ok||extensionVersion!=1){error="Unsupported terrain surface extension version";return false;}
    r.one(a.geo);r.one(a.globalMaps);r.one(count);
    if(!r.ok||count!=a.layers.size()){error="Invalid terrain extension layer coverage";return false;}
    std::set<AssetGuid> visited;
    for(u32 i=0;i<count;++i){
      AssetGuid id;r.one(id);auto found=std::find_if(a.layers.begin(),a.layers.end(),[&](const auto &layer){return layer.identity==id;});
      if(!r.ok||!visited.insert(id).second||found==a.layers.end()){error="Invalid terrain extension layer identity";return false;}
      extensionLayer(r,*found);
    }
  }
  if(version=="AETERRAINSURFACE3"){
    u32 snowVersion=0;r.one(snowVersion);if(!r.ok||snowVersion!=1){error="Unsupported Snow extension version";return false;}
    r.one(a.snow);r.one(a.textureResidency);
    std::set<AssetGuid> visited;
    for(usize i=0;i<a.layers.size();++i){AssetGuid id;r.one(id);auto it=std::find_if(a.layers.begin(),a.layers.end(),[&](auto &l){return l.identity==id;});
      if(!r.ok||it==a.layers.end()||!visited.insert(id).second){error="Invalid Snow layer coverage";return false;}r.one(it->snowParticipation);r.one(it->snowPorosity);}
    u32 mapCount=0,expected=0;r.one(mapCount);visitTerrainSurfaceMaps(a,[&](auto &){++expected;});
    if(!r.ok||mapCount!=expected){error="Invalid texture residency map coverage";return false;}
    visitTerrainSurfaceMaps(a,[&](auto &m){AssetGuid id;r.one(id);r.one(m.textureResidency);if(id!=m.texture)r.ok=false;});
  }
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
