#include "resources/terrain_asset.h"
#include "resources/terrain_vertex_index.h"
#include "resources/shared_decoded_texture.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <unordered_map>

namespace ae::resources {
namespace {
struct V {
  float x, y, z;
  V operator+(V b) const { return {x + b.x, y + b.y, z + b.z}; }
  V operator-(V b) const { return {x - b.x, y - b.y, z - b.z}; }
  V operator*(float s) const { return {x * s, y * s, z * s}; }
};
float dot(V a, V b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V cross(V a, V b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
V normal(V a) {
  const float l = std::sqrt(dot(a, a));
  return l > 1e-12f ? a * (1 / l) : V{0, 1, 0};
}
float fade(float t) { return t * t * (3 - 2 * t); }
float hash(i32 x, i32 z, u32 seed) {
  u32 v = u32(x) * 374761393u + u32(z) * 668265263u + seed * 2246822519u;
  v = (v ^ (v >> 13)) * 1274126177u;
  return float(v ^ (v >> 16)) / float(std::numeric_limits<u32>::max());
}
float noise(float x, float z, u32 seed) {
  const auto ix = i32(std::floor(x)), iz = i32(std::floor(z));
  const float fx = fade(x - float(ix)), fz = fade(z - float(iz));
  return std::lerp(
      std::lerp(hash(ix, iz, seed), hash(ix + 1, iz, seed), fx),
      std::lerp(hash(ix, iz + 1, seed), hash(ix + 1, iz + 1, seed), fx), fz);
}
float height(const TerrainRecipe &r, float x, float z) {
  float f = r.frequency / r.size, a = 1, sum = 0, weight = 0;
  for (u32 i = 0; i < r.octaves; ++i) {
    sum += noise(x * f, z * f, r.seed + i) * a;
    weight += a;
    f *= 2;
    a *= .5f;
  }
  float h = r.base + r.height * sum / weight;
  if (r.island) {
    float d = std::max(std::abs(x), std::abs(z)) / (r.size * .5f);
    h *= 1 - fade(std::clamp((d - .45f) / .55f, 0.f, 1.f));
  }
  return h;
}
void word(std::vector<u8> &b, u32 w) {
  for (u32 i = 0; i < 4; ++i)
    b.push_back(u8(w >> (i * 8)));
}
u32 word(std::span<const u8> b, usize p) {
  return u32(b[p]) | u32(b[p + 1]) << 8 | u32(b[p + 2]) << 16 |
         u32(b[p + 3]) << 24;
}
std::string pathNameToken(std::string_view name) {
  constexpr char hex[]="0123456789abcdef";std::string result;
  for(unsigned char c:name){result+=hex[c>>4];result+=hex[c&15];}return result.empty()?"-":result;
}
bool readPathName(std::string_view token,std::string &name) {
  if(token=="-"){name.clear();return true;}if(token.size()>512 || token.size()%2)return false;
  const auto digit=[](char c)->int{return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};
  name.clear();for(usize i=0;i<token.size();i+=2){const int a=digit(token[i]),b=digit(token[i+1]);if(a<0||b<0)return false;name+=char(a*16+b);}return true;
}
float pathCurve(const std::vector<TerrainCurvePoint> &curve,float t) {
  t=std::clamp(t,0.f,1.f);
  for(usize i=1;i<curve.size();++i)if(t<=curve[i].x) {
    const auto &a=curve[i-1],&b=curve[i];return std::lerp(a.y,b.y,(t-a.x)/(b.x-a.x));
  }
  return curve.back().y;
}
struct PathSample {V p;float length;};
std::vector<PathSample> samplePath(const TerrainPath &path,float step) {
  std::vector<PathSample> samples;if(!path.enabled || path.points.size()<2)return samples;
  const auto point=[&](usize i){const auto &p=path.points[i];return V{p.x,p.y,p.z};};
  float length=0;
  for(usize i=1;i<path.points.size();++i) {
    const auto a=point(i-1),b=point(i),prev=point(i>1?i-2:0),next=point(std::min(i+1,path.points.size()-1));
    const u32 count=std::min(2048u,std::max(1u,u32(std::ceil(std::sqrt(dot(b-a,b-a))/std::max(.1f,step)))));
    for(u32 j=i==1?0:1;j<=count;++j) {
      const float t=float(j)/count;V p=a*(1-t)+b*t;
      if(path.points[i-1].curved) p=(a*2+(b-prev)*t+(prev*2-a*5+b*4-next)*(t*t)+(a*3-b*3+next-prev)*(t*t*t))*.5f;
      if(!samples.empty()){const auto d=p-samples.back().p;length+=std::sqrt(d.x*d.x+d.z*d.z);}
      samples.push_back({p,length});
    }
  }
  return samples;
}
struct PathHit {float distance=INFINITY,y=0,t=0;};
PathHit pathHit(const TerrainPath &path,const std::vector<PathSample> &samples,float x,float z) {
  PathHit hit;
  bool beyondCap=false;
  if(samples.size()<2)return hit;
  usize first=1,last=samples.size()-1;
  const auto horizontalLength2=[&](usize i) {
    const auto d=samples[i].p-samples[i-1].p;return d.x*d.x+d.z*d.z;
  };
  while(first<=last && horizontalLength2(first)<1e-10f)++first;
  while(last>first && horizontalLength2(last)<1e-10f)--last;
  for(usize i=first;i<=last;++i) {
    const auto &a=samples[i-1],&b=samples[i];const auto d=b.p-a.p;
    const float denominator=d.x*d.x+d.z*d.z;if(denominator<1e-10f)continue;
    const float raw=((x-a.p.x)*d.x+(z-a.p.z)*d.z)/denominator;
    const float extra=path.cap==TerrainPathCap::Square?path.width*.5f/std::sqrt(denominator):0;
    const float t=std::clamp(raw,i==first?-extra:0.f,i==last?1+extra:1.f);
    const float dx=x-a.p.x-d.x*t,dz=z-a.p.z-d.z*t,distance=std::sqrt(dx*dx+dz*dz);
    if(distance<hit.distance) {
      hit={distance,std::lerp(a.p.y,b.p.y,std::clamp(t,0.f,1.f)),
        std::lerp(a.length,b.length,std::clamp(t,0.f,1.f))/std::max(.0001f,samples.back().length)};
      // Pick the nearest segment BEFORE clipping the terminal plane. Skipping
      // it in the search lets the next tessellated segment paint a round end.
      beyondCap=path.cap!=TerrainPathCap::Round &&
        ((i==first && raw < -extra)||(i==last && raw > 1+extra));
    }
  }
  if(beyondCap)hit.distance=INFINITY;
  return hit;
}
} // namespace
namespace {
V brushRotation(V p, const TerrainStroke &s, bool inverse) {
  const auto rotate=[](float &a,float &b,float degrees) {
    const float angle=degrees*.01745329252f,c=std::cos(angle),t=std::sin(angle);
    const float x=a*c-b*t;b=a*t+b*c;a=x;
  };
  if(inverse) { rotate(p.x,p.z,-s.rotation);rotate(p.y,p.z,-s.pitch);rotate(p.x,p.y,-s.roll); }
  else { rotate(p.x,p.y,s.roll);rotate(p.y,p.z,s.pitch);rotate(p.x,p.z,s.rotation); }
  return p;
}
}
float terrainBrushDistance(const TerrainStroke &s, float dx, float dy, float dz, bool volume) {
  // A planar brush never gains a vertical dependency from pitch or roll.
  auto orientation=s;
  if(!volume)orientation.pitch=orientation.roll=0;
  const auto q=brushRotation({dx,volume?dy:0,dz},orientation,true);
  const float x=q.x/(s.radius*s.aspect),z=q.z/(s.radius*s.depthAspect);
  const float y=volume?q.y/(s.radius*s.verticalAspect)*(s.inverted?-1.f:1.f):0;
  if(s.shape==TerrainBrushShape::Box)return std::max({std::abs(x),std::abs(z),std::abs(y)});
  if(s.shape==TerrainBrushShape::RoundedBox) {
    const float r=s.roundness;
    const float ax=std::abs(x)-(1-r),ay=volume?std::abs(y)-(1-r):-1.f,az=std::abs(z)-(1-r);
    const float outside=std::sqrt(std::max(0.f,ax)*std::max(0.f,ax)+std::max(0.f,ay)*std::max(0.f,ay)+std::max(0.f,az)*std::max(0.f,az));
    return 1+outside+std::min(0.f,std::max({ax,ay,az}))-r;
  }
  if(volume && s.shape==TerrainBrushShape::Hemisphere && y<0)return 2;
  if(volume && s.shape==TerrainBrushShape::Cone) {
    if(y<0 || y>2)return 2;
    return std::sqrt(x*x+z*z)/std::max(.00001f,1-y*.5f);
  }
  return std::sqrt(x*x+y*y+z*z);
}
std::array<float,3> terrainBrushOutline(const TerrainStroke &s,u32 ring,float phase) {
  const float theta=phase*6.28318530718f;
  float x=std::cos(theta),z=std::sin(theta),y=0;
  if(ring==1){y=z;z=0;}else if(ring==2){y=x;x=0;}
  if(s.shape==TerrainBrushShape::Box) {
    const float m=std::max({std::abs(x),std::abs(y),std::abs(z)});x/=m;y/=m;z/=m;
  } else if(s.shape==TerrainBrushShape::RoundedBox) {
    // Intersect each radial ray with the exact rounded-box distance field.
    auto unit=s;unit.radius=unit.aspect=unit.verticalAspect=unit.depthAspect=1;
    unit.rotation=unit.pitch=unit.roll=0;unit.inverted=false;
    float lo=0,hi=1.733f;
    for(int i=0;i<18;++i){const float t=(lo+hi)*.5f;if(terrainBrushDistance(unit,x*t,y*t,z*t,true)<1)lo=t;else hi=t;}
    const float t=(lo+hi)*.5f;x*=t;y*=t;z*=t;
  }
  if(s.shape==TerrainBrushShape::Hemisphere)y=std::abs(y);
  if(s.shape==TerrainBrushShape::Cone && ring>0) {
    const float t=phase*3;float radial;
    if(t<1){radial=1-t;y=2*t;}else if(t<2){radial=1-t;y=2*(2-t);}else{radial=2*t-5;y=0;}
    x=ring==1?radial:0;z=ring==2?radial:0;
  }
  const auto q=brushRotation({x*s.radius*s.aspect,y*s.radius*s.verticalAspect*(s.inverted?-1.f:1.f),z*s.radius*s.depthAspect},s,false);
  return {q.x,q.y,q.z};
}
bool validTerrainStroke(const TerrainStroke &s,std::string &error) {
  const float values[]{s.x,s.y,s.z,s.radius,s.strength,s.hardness,s.rotation,s.aspect,s.verticalAspect,s.depthAspect,s.pitch,s.roll,s.roundness,s.opacity,s.surfaceDepth};
  for(float v:values)if(!std::isfinite(v)){error="Pincel não finito.";return false;}
  if(u32(s.tool)>12 || u32(s.shape)>4 || u32(s.application)>2 || s.hardness<0 || s.hardness>1 ||
     s.aspect<.1f || s.aspect>10 || s.verticalAspect<.1f || s.verticalAspect>10 || s.depthAspect<.1f || s.depthAspect>10 ||
     std::abs(s.rotation)>360 || std::abs(s.pitch)>360 || std::abs(s.roll)>360 || s.roundness<0 || s.roundness>1 ||
     s.opacity<0 || s.opacity>1 || std::abs(s.surfaceDepth)>2048 || s.filterRadius<1 || s.filterRadius>4 || s.layer>3 || s.radius<.1f || s.radius>2048 ||
     s.strength<0 || s.strength>128 || std::abs(s.x)>4096 || std::abs(s.z)>4096 || std::abs(s.y)>2048) {
    error="Pincel fora dos limites.";return false;
  }
  return true;
}
float terrainBrushWeight(const TerrainStroke &s,float d) {
  if(d>=1) return 0;
  return fade(std::clamp((1-d)/std::max(.001f,1-s.hardness),0.f,1.f));
}
bool validTerrainRecipe(const TerrainRecipe &r, std::string &error) {
  if (u32(r.mode) > 2 || r.cells < 8 || r.cells > 128 || r.cells % 8 ||
      r.verticalCells < 8 || r.verticalCells > 64 || r.octaves < 1 ||
      r.octaves > 8 || r.strokes.size() > 4096) {
    error =
        "Resolução: 8–128 (múltiplos de 8); volume: 8–64; até 4096 amostras.";
    return false;
  }
  if(r.controlCells && (r.controlCells<8 || r.controlCells>512 || r.controlCells%8)) {error="Mapa de materiais: 0 ou 8–512, múltiplos de 8.";return false;}
  const float values[]{r.size, r.height, r.base, r.frequency, r.textureScale,
                       r.baseDepth, r.macroVariation, r.lodRelativeError};
  for (float v : values)
    if (!std::isfinite(v)) {
      error = "Parâmetro não finito.";
      return false;
    }
  if (r.size < 8 || r.size > 2048 || r.height < 0 || r.height > 512 ||
      r.base < 0 || r.base > 256 || r.frequency < .1f || r.frequency > 32 ||
      r.textureScale < .1f || r.textureScale > 256) {
    error = "Dimensões, frequência ou escala de textura fora do limite.";
    return false;
  }
  if (r.macroVariation < 0 || r.macroVariation > 1) { error="Variação deve estar entre 0 e 1."; return false; }
  if (r.lodLevels < 1 || r.lodLevels > 4 || r.lodRelativeError < .0001f || r.lodRelativeError > .1f) {
    error="LOD: 1–4 níveis; erro relativo entre 0,0001 e 0,1.";return false;
  }
  if (r.baseDepth < .1f || r.baseDepth > 512) {
    error = "Espessura mínima da base: 0,1–512 unidades.";
    return false;
  }
  if (r.mode != TerrainMode::Heightfield &&
      u64(r.cells + 1) * (r.cells + 1) * (r.verticalCells + 1) > 600000) {
    error = "Volume excede 600 mil amostras; reduza a resolução.";
    return false;
  }
  if(r.paths.size()>64){error="Limite de 64 caminhos por terreno.";return false;}
  std::vector<u32> pathIds;
  for(const auto &p:r.paths) {
    const float values[]{p.width,p.shoulderWidth,p.elevation,p.slopeLimit,p.uniformRamp,p.definedHeight,p.textureOpacity,p.shoulderOpacity,p.textureTransition,p.vegetationTransition};
    for(float v:values)if(!std::isfinite(v)){error="Parâmetro não finito no caminho.";return false;}
    if(!p.id || std::find(pathIds.begin(),pathIds.end(),p.id)!=pathIds.end() || p.name.size()>256 || p.points.size()>1024 ||
      p.width<.1f || p.shoulderWidth<p.width || p.shoulderWidth>2048 || p.slopeLimit<0 || p.slopeLimit>89.9f ||
      std::abs(p.elevation)>512 || std::abs(p.definedHeight)>2048 || p.uniformRamp<0 || p.uniformRamp>1 ||
      p.textureOpacity<0 || p.textureOpacity>1 || p.shoulderOpacity<0 || p.shoulderOpacity>1 ||
      p.textureTransition<0 || p.textureTransition>512 || p.vegetationTransition<0 || p.vegetationTransition>512 ||
      p.surfaceLayer>3 || p.shoulderLayer>3 || u32(p.cap)>2) {error="Propriedade do caminho fora do limite.";return false;}
    pathIds.push_back(p.id);
    for(const auto &point:p.points)if(!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z)||std::abs(point.x)>4096||std::abs(point.z)>4096||std::abs(point.y)>2048){error="Ponto de caminho inválido.";return false;}
    for(const auto *curve:{&p.profile,&p.textureProfile,&p.vegetationProfile}) {
      if(curve->size()<2 || curve->size()>32 || curve->front().x!=0 || curve->back().x!=1){error="Curva precisa cobrir 0–1 com 2–32 pontos.";return false;}
      float last=-1;
      const bool geometry=curve==&p.profile;
      for(const auto &v:*curve){if(!std::isfinite(v.x)||!std::isfinite(v.y)||v.x<=last||v.x<0||v.x>1||v.y<(geometry?-1.f:0.f)||v.y>(geometry?2.f:1.f)){error="Curva de transição inválida.";return false;}last=v.x;}
    }
  }
  for (const auto &s : r.strokes) {
    if(!validTerrainStroke(s,error))return false;
    if (r.mode == TerrainMode::Heightfield &&
        (s.tool == TerrainTool::Dig || s.tool == TerrainTool::Add)) {
      error = "Escavar/adicionar volume exige modo voxel.";
      return false;
    }
    if (r.mode != TerrainMode::Heightfield && s.tool == TerrainTool::Hole) {
      error = "Buracos de superfície exigem relevo; use Escavar no voxel.";
      return false;
    }
  }
  error.clear();
  return true;
}
std::string serializeTerrainRecipe(const TerrainRecipe &r) {
  std::ostringstream s;
  s.imbue(std::locale::classic());
  s << std::setprecision(9) << "AETERRAIN7 " << u32(r.mode) << ' ' << r.seed
    << ' ' << r.cells << ' ' << r.verticalCells << ' ' << r.octaves << ' '
    << r.size << ' ' << r.height << ' ' << r.base << ' ' << r.frequency << ' '
    << r.textureScale << ' ' << r.island << ' ' << r.collision << ' '
    << r.solidBase << ' ' << r.baseDepth << ' ' << r.blendedSurface << ' ' << r.macroVariation << ' ' << r.strokes.size();
  for (const auto &v : r.strokes)
    s << ' ' << u32(v.tool) << ' ' << v.x << ' ' << v.y << ' ' << v.z << ' '
      << v.radius << ' ' << v.strength << ' ' << v.layer << ' ' << u32(v.shape) << ' ' << v.hardness << ' ' << v.rotation << ' ' << v.aspect << ' ' << v.gesture
      << ' ' << v.verticalAspect << ' ' << v.depthAspect << ' ' << v.pitch << ' ' << v.roll
      << ' ' << v.roundness << ' ' << v.opacity << ' ' << v.filterRadius << ' ' << u32(v.application)
      << ' ' << v.inverted << ' ' << v.restoreGeometry << ' ' << v.restoreWeights
      << ' ' << v.surfaceDepth << ' ' << v.lockHeight << ' ' << v.dimensionsLinked;
  s << ' ' << r.paths.size();
  for(const auto &p:r.paths) {
    s << ' ' << p.id << ' ' << pathNameToken(p.name) << ' ' << p.width << ' ' << p.shoulderWidth << ' ' << p.elevation
      << ' ' << p.slopeLimit << ' ' << p.uniformRamp << ' ' << p.definedHeight << ' ' << p.textureOpacity << ' ' << p.shoulderOpacity
      << ' ' << p.textureTransition << ' ' << p.vegetationTransition << ' ' << p.surfaceLayer << ' ' << p.shoulderLayer
      << ' ' << p.enabled << ' ' << p.shape << ' ' << p.texture << ' ' << p.keepWidthRatio << ' ' << p.autoRamp
      << ' ' << p.useDefinedHeight << ' ' << p.reverseRamp << ' ' << p.smartPaint << ' ' << p.clearGrass << ' ' << p.clearTrees
      << ' ' << p.showVegetation << ' ' << u32(p.cap) << ' ' << p.points.size();
    for(const auto &point:p.points)s << ' ' << point.x << ' ' << point.y << ' ' << point.z << ' ' << point.curved;
    for(const auto *curve:{&p.profile,&p.textureProfile,&p.vegetationProfile}) {
      s << ' ' << curve->size();for(const auto &v:*curve)s << ' ' << v.x << ' ' << v.y;
    }
    s << ' ' << p.followTerrain;
  }
  s << ' ' << r.lodLevels << ' ' << r.lodRelativeError << ' ' << r.controlCells;
  return s.str();
}
bool deserializeTerrainRecipe(std::string_view text, TerrainRecipe &out,
                              std::string &error) {
  // 64 paths with 1024 full-precision points plus 4096 brush samples.
  if (text.size() > 8 * 1048576) {
    error = "Receita muito grande.";
    return false;
  }
  std::istringstream s{std::string(text)};
  s.imbue(std::locale::classic());
  std::string magic;
  TerrainRecipe r;
  u32 mode = 0, count = 0;
  if (!(s >> magic >> mode >> r.seed >> r.cells >> r.verticalCells >>
        r.octaves >> r.size >> r.height >> r.base >> r.frequency >>
        r.textureScale >> r.island >> r.collision) ||
      (magic.size()!=10 || magic.substr(0,9)!="AETERRAIN" || magic[9]<'1' || magic[9]>'7')) {
    error = "Receita de terreno inválida.";
    return false;
  }
  const u32 version=u32(magic[9]-'0');
  r.blendedSurface = false;
  if ((version>=2 && !(s >> r.solidBase >> r.baseDepth)) ||
      (version>=3 && !(s >> r.blendedSurface >> r.macroVariation)) ||
      !(s >> count) || count > 4096) {
    error = "Receita de terreno inválida.";
    return false;
  }
  r.mode = TerrainMode(mode);
  r.strokes.resize(count);
  for (auto &v : r.strokes) {
    u32 tool = 0;
    if (!(s >> tool >> v.x >> v.y >> v.z >> v.radius >> v.strength >>
          v.layer)) {
      error = "Operação truncada.";
      return false;
    }
    if (version>=3) {
      u32 shape=0;
      if (!(s >> shape >> v.hardness >> v.rotation >> v.aspect >> v.gesture)) { error="Pincel truncado.";return false; }
      v.shape=TerrainBrushShape(shape);
    }
    if(version>=4) {
      u32 application=0;
      if(!(s >> v.verticalAspect >> v.depthAspect >> v.pitch >> v.roll >> v.roundness >> v.opacity >> v.filterRadius >> application >> v.inverted >> v.restoreGeometry >> v.restoreWeights >> v.surfaceDepth >> v.lockHeight >> v.dimensionsLinked)) {error="Propriedades do pincel truncadas.";return false;}
      v.application=TerrainBrushApplication(application);
    }
    v.tool = TerrainTool(tool);
    if(version<3 && (v.tool==TerrainTool::Dig || v.tool==TerrainTool::Add))v.hardness=1;
  }
  if(version>=4) {
    u32 paths=0;if(!(s>>paths)||paths>64){error="Lista de caminhos inválida.";return false;}
    r.paths.resize(paths);
    for(auto &p:r.paths) {
      std::string name;u32 cap=0,points=0;
      if(!(s >> p.id >> name >> p.width >> p.shoulderWidth >> p.elevation >> p.slopeLimit >> p.uniformRamp >> p.definedHeight
        >> p.textureOpacity >> p.shoulderOpacity >> p.textureTransition >> p.vegetationTransition >> p.surfaceLayer >> p.shoulderLayer
        >> p.enabled >> p.shape >> p.texture >> p.keepWidthRatio >> p.autoRamp >> p.useDefinedHeight >> p.reverseRamp >> p.smartPaint
        >> p.clearGrass >> p.clearTrees >> p.showVegetation >> cap >> points) || points>1024 || !readPathName(name,p.name)) {error="Caminho truncado.";return false;}
      p.cap=TerrainPathCap(cap);p.points.resize(points);
      for(auto &point:p.points)if(!(s>>point.x>>point.y>>point.z>>point.curved)){error="Pontos truncados.";return false;}
      for(auto *curve:{&p.profile,&p.textureProfile,&p.vegetationProfile}) {
        u32 count=0;if(!(s>>count)||count<2||count>32){error="Curva inválida.";return false;}curve->resize(count);
        for(auto &v:*curve)if(!(s>>v.x>>v.y)){error="Curva truncada.";return false;}
      }
      if(version>=5 && !(s>>p.followTerrain)){error="Modo de altura truncado.";return false;}
    }
  }
  if(version>=6 && !(s>>r.lodLevels>>r.lodRelativeError)){error="Configuração de LOD truncada.";return false;}
  if(version>=7 && !(s>>r.controlCells)){error="Resolução do mapa truncada.";return false;}
  s >> std::ws;
  if (!s.eof()) {
    error = "Dados extras na receita.";
    return false;
  }
  if (!validTerrainRecipe(r, error))
    return false;
  out = std::move(r);
  return true;
}
std::vector<std::array<float,3>> terrainPathPolyline(const TerrainPath &path,float step) {
  std::vector<std::array<float,3>> result;
  if(!std::isfinite(step) || step<=0)return result;
  for(const auto &sample:samplePath(path,step))result.push_back({sample.p.x,sample.p.y,sample.p.z});
  return result;
}
float terrainFieldHeight(const TerrainField &field,float x,float z) {
  if(field.width<2 || field.depth<2 || field.step<=0 || field.heights.size()!=usize(field.width)*field.depth)return 0;
  const float gx=std::clamp(x/field.step+(field.width-1)*.5f,0.f,float(field.width-1));
  const float gz=std::clamp(z/field.step+(field.depth-1)*.5f,0.f,float(field.depth-1));
  const u32 ix=std::min(u32(gx),field.width-2),iz=std::min(u32(gz),field.depth-2);
  const auto at=[&](u32 a,u32 b){return field.heights[usize(b)*field.width+a];};
  return std::lerp(std::lerp(at(ix,iz),at(ix+1,iz),gx-ix),std::lerp(at(ix,iz+1),at(ix+1,iz+1),gx-ix),gz-iz);
}
static std::array<float,4> evaluateTerrainWeights(const TerrainRecipe &r,V p,V nm,
    bool volume,u32 fixedLayer,const std::vector<const TerrainStroke *> &localPaint,
    const std::vector<std::vector<PathSample>> &pathSamples) {
  TerrainVertex v;
  // Smooth automatic strata, evaluated per vertex so chunk boundaries
  // have identical weights. Painting is local in 3D for cave surfaces.
  float rock=fade(std::clamp((1-std::abs(nm.y)-.12f)/.5f,0.f,1.f));
  float soil=(1-rock)*(.10f+.25f*noise(p.x*.13f,p.z*.13f,r.seed+73));
  v.weights[0]=1-rock-soil;v.weights[1]=rock;v.weights[2]=soil;v.weights[3]=0;
  if(fixedLayer<4) {std::fill_n(v.weights,4,0);v.weights[fixedLayer]=1;}
  else for(const auto *candidate:localPaint) {
    const auto &brush=*candidate;
    if(brush.tool==TerrainTool::Restore && !brush.restoreWeights)continue;
    if(brush.tool!=TerrainTool::Paint && brush.tool!=TerrainTool::Path && brush.tool!=TerrainTool::Restore)continue;
    const float d=terrainBrushDistance(brush,p.x-brush.x,p.y-brush.y,p.z-brush.z,volume && brush.tool!=TerrainTool::Path);
    const float w=std::clamp(terrainBrushWeight(brush,d)*brush.strength,0.f,1.f);
    if(w<=0)continue;
    const float natural[]{1-rock-soil,rock,soil,0};
    if(brush.tool!=TerrainTool::Restore && brush.application==TerrainBrushApplication::Target) {
      // Target weight is absolute, with smooth spatial edges. Repeated stamps
      // at the same location cannot keep driving the selected weight to 1.
      const float target=brush.opacity*terrainBrushWeight(brush,d);
      const float old=v.weights[brush.layer],other=1-old;
      for(u32 l=0;l<4;++l)if(l!=brush.layer)v.weights[l]=other>.000001f?v.weights[l]*((1-target)/other):(1-target)/3;
      v.weights[brush.layer]=target;continue;
    }
    for(u32 l=0;l<4;++l)v.weights[l]=std::lerp(v.weights[l],brush.tool==TerrainTool::Restore?natural[l]:(l==brush.layer?1.f:0.f),w*brush.opacity);
  }
  const TerrainPath *strongestPath=nullptr;
  float strongestLane=0,strongestShoulder=0,strongestTotal=0;
  const auto paintPath=[&](const TerrainPath &path,float lane,float shoulder) {
    for(u32 l=0;l<4;++l)v.weights[l]*=1-lane-shoulder;
    v.weights[path.surfaceLayer]+=lane;v.weights[path.shoulderLayer]+=shoulder;
  };
  const auto flushSmart=[&] {
    if(strongestPath)paintPath(*strongestPath,strongestLane,strongestShoulder);
    strongestPath=nullptr;strongestTotal=0;
  };
  for(usize i=0;i<r.paths.size();++i) {
    const auto &path=r.paths[i];if(!path.enabled || !path.texture)continue;
    const auto hit=pathHit(path,pathSamples[i],p.x,p.z);
    const float half=path.width*.5f,outer=path.shoulderWidth*.5f+path.textureTransition;
    if(hit.distance>outer)continue;
    const float lane=1-fade(std::clamp((hit.distance-half)/std::max(.001f,path.textureTransition),0.f,1.f));
    const float shoulder=pathCurve(path.textureProfile,std::clamp((hit.distance-half)/std::max(.001f,outer-half),0.f,1.f));
    const float laneWeight=lane*path.textureOpacity,shoulderWeight=(1-lane)*shoulder*path.shoulderOpacity;
    const float total=laneWeight+shoulderWeight;
    if(path.smartPaint) {
      if(total>strongestTotal || (total==strongestTotal && strongestPath && path.id<strongestPath->id)) {
        strongestPath=&path;strongestTotal=total;strongestLane=laneWeight;strongestShoulder=shoulderWeight;
      }
    } else {flushSmart();paintPath(path,laneWeight,shoulderWeight);}
  }
  flushSmart();
  return {v.weights[0],v.weights[1],v.weights[2],v.weights[3]};
}
std::array<float,4> terrainFieldWeights(const TerrainField &f,float x,float z) {
  if(!std::isfinite(x) || !std::isfinite(z) || !std::isfinite(f.controlStep) ||
     f.controlWidth<2 || f.controlStep<=0 || f.controlWeights.size()!=usize(f.controlWidth)*f.controlWidth)return {1,0,0,0};
  const float gx=std::clamp(x/f.controlStep+(f.controlWidth-1)*.5f,0.f,float(f.controlWidth-1));
  const float gz=std::clamp(z/f.controlStep+(f.controlWidth-1)*.5f,0.f,float(f.controlWidth-1));
  const u32 ix=std::min(u32(gx),f.controlWidth-2),iz=std::min(u32(gz),f.controlWidth-2);
  std::array<float,4> result;
  for(u32 l=0;l<4;++l) {
    const auto at=[&](u32 a,u32 b){return f.controlWeights[usize(b)*f.controlWidth+a][l];};
    result[l]=std::lerp(std::lerp(at(ix,iz),at(ix+1,iz),gx-ix),std::lerp(at(ix,iz+1),at(ix+1,iz+1),gx-ix),gz-iz);
  }
  return result;
}
static bool buildTerrainField(const TerrainRecipe &r, TerrainField &out,
                     std::string &error,const std::function<bool(float)> &progress,
                     const TerrainField *seed=nullptr,usize firstStroke=0) {
  if (!validTerrainRecipe(r, error))
    return false;
  const u32 n = r.cells + 1,
            ny = r.mode == TerrainMode::Heightfield ? 1 : r.verticalCells + 1;
  const float step = r.size / float(r.cells),
              bottom = -std::max(4.f, r.height * .25f),
              top = r.base + r.height + std::max(4.f, r.height * .25f),
              dy = (top - bottom) / float(r.verticalCells);
  std::vector<float> heights=seed?seed->heights:std::vector<float>(usize(n)*n);
  std::vector<u8> layers=seed?seed->legacyLayers:std::vector<u8>(heights.size());
  std::vector<u8> holes=seed?seed->holes:std::vector<u8>(heights.size());
  std::vector<TerrainStroke> strokes;
  for (usize i = firstStroke; i < r.strokes.size(); ++i) {
    const auto &s = r.strokes[i];
    if(s.strength == 0) continue;
    if (i && s.tool == TerrainTool::Path &&
        r.strokes[i - 1].tool == TerrainTool::Path && r.strokes[i - 1].gesture == s.gesture) {
      const auto &a = r.strokes[i - 1];
      float dx = s.x - a.x, dz = s.z - a.z;
      u32 steps = u32(
          std::ceil(std::sqrt(dx * dx + dz * dz) /
                    std::max(step * .5f, std::min(a.radius, s.radius) * .35f)));
      if (strokes.size() + steps > 4096) {
        error = "Caminho excede o orçamento de amostras; aumente o raio.";
        return false;
      }
      for (u32 j = 1; j < steps; ++j) {
        float t = float(j) / float(steps);
        auto sample = s;
        sample.x = std::lerp(a.x, s.x, t);
        sample.y = std::lerp(a.y, s.y, t);
        sample.z = std::lerp(a.z, s.z, t);
        sample.radius = std::lerp(a.radius, s.radius, t);
        strokes.push_back(sample);
      }
    }
    strokes.push_back(s);
  }
  const auto hidx = [&](u32 x, u32 z) { return usize(z) * n + x; };
  if(!seed)for (u32 z = 0; z < n; ++z)
    for (u32 x = 0; x < n; ++x)
      heights[hidx(x, z)] = height(r, float(x) * step - r.size * .5f,
                                   float(z) * step - r.size * .5f);
  const auto bounds=[&](const TerrainStroke &s,bool volume) {
    const float radius=s.radius*std::max({s.aspect,s.verticalAspect,s.depthAspect})*2;
    const auto low=[](float p,float origin,float step,u32 limit){return u32(std::clamp(std::floor((p-origin)/step),0.f,float(limit)));};
    const auto high=[](float p,float origin,float step,u32 limit){return u32(std::clamp(std::ceil((p-origin)/step)+1,0.f,float(limit)));};
    return std::array<u32,6>{low(s.x-radius,-r.size*.5f,step,n),high(s.x+radius,-r.size*.5f,step,n),
      volume?low(s.y-radius,bottom,dy,ny):0,volume?high(s.y+radius,bottom,dy,ny):1,
      low(s.z-radius,-r.size*.5f,step,n),high(s.z+radius,-r.size*.5f,step,n)};
  };
  // Shared boundary samples ensure adjacent chunks evaluate identical geometry.
  for (const auto &s : strokes)
    if (s.tool != TerrainTool::Dig && s.tool != TerrainTool::Add) {
      if(ny>1 && s.tool==TerrainTool::Restore)continue;
      if (progress && !progress(0)) {
        error = "Geração cancelada.";
        return false;
      }
      const auto previous =
          (s.tool == TerrainTool::Smooth || s.tool == TerrainTool::Sharpen) ? heights : std::vector<float>{};
      const auto region=bounds(s,false);
      for (u32 z = region[4]; z < region[5]; ++z)
        for (u32 x = region[0]; x < region[1]; ++x) {
          float dx = float(x) * step - r.size * .5f - s.x,
                dz = float(z) * step - r.size * .5f - s.z;
          float w=s.opacity*terrainBrushWeight(s,terrainBrushDistance(s,dx,0,dz,false));
          if(w<=0) continue;
          auto &h = heights[hidx(x, z)];
          switch (s.tool) {
          case TerrainTool::Raise:
            h += w * s.strength;
            break;
          case TerrainTool::Lower:
            h -= w * s.strength;
            break;
          case TerrainTool::Flatten:
          case TerrainTool::Path:
            h = std::lerp(h, s.y, std::clamp(w * s.strength, 0.f, 1.f));
            break;
          case TerrainTool::Smooth:
          case TerrainTool::Sharpen: {
            if (ny > 1)
              break; // Voxel smoothing operates on density, including cave
                     // walls.
            float sum = 0;
            u32 count = 0;
            for (i32 oz = -i32(s.filterRadius); oz <= i32(s.filterRadius); ++oz)
              for (i32 ox = -i32(s.filterRadius); ox <= i32(s.filterRadius); ++ox) {
                const auto xx = u32(std::clamp(i32(x) + ox, 0, i32(n) - 1)),
                           zz = u32(std::clamp(i32(z) + oz, 0, i32(n) - 1));
                sum += previous[hidx(xx, zz)];
                ++count;
              }
            const float target=s.tool==TerrainTool::Sharpen?2*h-sum/float(count):sum/float(count);
            h = std::lerp(h, target,
                          std::clamp(w * s.strength, 0.f, 1.f));
            break;
          }
          case TerrainTool::Noise:
            h += w*s.strength*(noise(float(x)*.37f,float(z)*.37f,r.seed+s.gesture)*2-1);break;
          case TerrainTool::Terrace: {
            const float interval=std::max(.25f,s.strength);
            h=std::lerp(h,std::round(h/interval)*interval,w);break;
          }
          case TerrainTool::Restore:
            if(s.restoreGeometry)h=std::lerp(h,height(r,float(x)*step-r.size*.5f,float(z)*step-r.size*.5f),std::min(1.f,w*s.strength));
            if(w*s.strength>.5f){if(s.restoreGeometry)holes[hidx(x,z)]=0;if(s.restoreWeights)layers[hidx(x,z)]=0;}break;
          case TerrainTool::Hole:
            if(w*s.strength>.5f)holes[hidx(x,z)]=1;
            break;
          default:
            break;
          }
          if ((s.tool == TerrainTool::Paint || s.tool == TerrainTool::Path) &&
              w * s.strength > .01f)
            layers[hidx(x, z)] = u8(s.layer + 1);
        }
    }
  // Paths remain separate authored objects. Every rebuild starts from the
  // sculpted base; changing a point never bakes the old road into that base.
  const auto sculpted=heights;
  const auto ground=[&](float x,float z) {
    const float gx=std::clamp((x+r.size*.5f)/step,0.f,float(n-1)),gz=std::clamp((z+r.size*.5f)/step,0.f,float(n-1));
    const u32 ix=std::min(u32(gx),n-2),iz=std::min(u32(gz),n-2);
    return std::lerp(std::lerp(sculpted[hidx(ix,iz)],sculpted[hidx(ix+1,iz)],gx-ix),
                     std::lerp(sculpted[hidx(ix,iz+1)],sculpted[hidx(ix+1,iz+1)],gx-ix),gz-iz);
  };
  for(const auto &path:r.paths)if(path.enabled && path.shape && path.points.size()>1) {
    auto samples=samplePath(path,step*.5f);
    if(samples.empty())continue;
    float first=path.useDefinedHeight?path.definedHeight:path.points.front().y;
    float last=path.useDefinedHeight?path.definedHeight:path.points.back().y;
    if(path.reverseRamp)std::swap(first,last);
    const float maximum=std::tan(path.slopeLimit*.01745329252f);
    if(path.autoRamp)last=first+std::clamp(last-first,-maximum*samples.back().length,maximum*samples.back().length);
    for(auto &sample:samples) {
      const float t=sample.length/std::max(.0001f,samples.back().length);
      const float follow=path.useDefinedHeight?path.definedHeight:path.followTerrain?ground(sample.p.x,sample.p.z):sample.p.y;
      sample.p.y=std::lerp(follow,std::lerp(first,last,t),path.uniformRamp)+path.elevation;
    }
    if(path.autoRamp)for(usize i=1;i<samples.size();++i) {
      const float maximumDelta=maximum*(samples[i].length-samples[i-1].length);
      samples[i].p.y=std::clamp(samples[i].p.y,samples[i-1].p.y-maximumDelta,samples[i-1].p.y+maximumDelta);
    }
    for(u32 z=0;z<n;++z)for(u32 x=0;x<n;++x) {
      const auto hit=pathHit(path,samples,x*step-r.size*.5f,z*step-r.size*.5f);
      const float half=path.width*.5f,shoulder=path.shoulderWidth*.5f;
      if(hit.distance>shoulder)continue;
      const float influence=hit.distance<=half?1:pathCurve(path.profile,(hit.distance-half)/std::max(.0001f,shoulder-half));
      auto &h=heights[hidx(x,z)];h=std::lerp(h,hit.y,influence);
    }
  }
  const bool closedHeightfield = ny == 1 && r.solidBase;
  const float solidBottom =
      closedHeightfield
          ? *std::min_element(heights.begin(), heights.end()) - r.baseDepth
          : 0.f;
  const auto index = [&](u32 x, u32 y, u32 z) {
    return (usize(z) * ny + y) * n + x;
  };
  std::vector<float> field=seed?seed->density:std::vector<float>{};
  if (ny > 1) {
    field.resize(usize(n) * n * ny);
    if(!seed)for (u32 z = 0; z < n; ++z) {
      if (progress && !progress(float(z) / float(n) * .35f)) {
        error = "Geração cancelada.";
        return false;
      }
      for (u32 y = 0; y < ny; ++y)
        for (u32 x = 0; x < n; ++x) {
          V p{float(x) * step - r.size * .5f, bottom + float(y) * dy,
              float(z) * step - r.size * .5f};
          field[index(x, y, z)] = p.y - heights[hidx(x, z)];
        }
    }
    std::vector<float> gestureField;
    u32 densityGesture=~0u;
    for (const auto &s : strokes) {
      if(s.gesture!=densityGesture || !s.gesture){gestureField=field;densityGesture=s.gesture;}
      if(s.strength == 0) continue;
      if (s.tool != TerrainTool::Dig && s.tool != TerrainTool::Add &&
          s.tool != TerrainTool::Smooth && s.tool != TerrainTool::Sharpen && s.tool != TerrainTool::Restore)
        continue;
      const auto previous =
          (s.tool == TerrainTool::Smooth || s.tool == TerrainTool::Sharpen) ? field : std::vector<float>{};
      const auto region=bounds(s,true);
      for (u32 z = region[4]; z < region[5]; ++z) {
        if (progress && !progress(.3f)) {
          error = "Geração cancelada.";
          return false;
        }
        for (u32 y = region[2]; y < region[3]; ++y)
          for (u32 x = region[0]; x < region[1]; ++x) {
            V p{float(x) * step - r.size * .5f, bottom + float(y) * dy,
                float(z) * step - r.size * .5f};
            V q = p - V{s.x, s.y, s.z};
            float distance = terrainBrushDistance(s,q.x,q.y,q.z,true)*s.radius;
            auto &d = field[index(x, y, z)];
            if (s.tool == TerrainTool::Restore) {
              float w=terrainBrushWeight(s,distance/s.radius);
              if(s.restoreGeometry)d=std::lerp(d,p.y-height(r,p.x,p.z),std::min(1.f,w*s.strength*s.opacity));
            } else if (s.tool == TerrainTool::Smooth || s.tool == TerrainTool::Sharpen) {
              if (distance >= s.radius)
                continue;
              float sum = 0;
              u32 count = 0;
              for (i32 a = -i32(s.filterRadius); a <= i32(s.filterRadius); ++a)
                for (i32 b = -i32(s.filterRadius); b <= i32(s.filterRadius); ++b)
                  for (i32 c = -i32(s.filterRadius); c <= i32(s.filterRadius); ++c) {
                    sum += previous[index(
                        u32(std::clamp(i32(x) + a, 0, i32(n) - 1)),
                        u32(std::clamp(i32(y) + b, 0, i32(ny) - 1)),
                        u32(std::clamp(i32(z) + c, 0, i32(n) - 1)))];
                    ++count;
                  }
              d = std::lerp(
                  d, s.tool==TerrainTool::Sharpen?2*d-sum/float(count):sum/float(count),
                  std::clamp(terrainBrushWeight(s,distance/s.radius) * s.strength*s.opacity, 0.f,
                             1.f));
            } else {
              if(s.application==TerrainBrushApplication::Accumulate) {
                const float amount=std::max(0.f,s.radius-distance)*s.strength*s.opacity*terrainBrushWeight(s,distance/s.radius);
                d+=s.tool==TerrainTool::Dig?amount:-amount;continue;
              }
              float sphere = distance - s.radius;
              const float before=s.application==TerrainBrushApplication::Target?gestureField[index(x,y,z)]:d;
              float changed = s.tool == TerrainTool::Dig ? std::max(before, -sphere) : std::min(before, sphere);
              if(s.application==TerrainBrushApplication::Target) {
                const float target=std::lerp(before,changed,std::clamp(s.opacity*s.strength*terrainBrushWeight(s,distance/s.radius),0.f,1.f));
                d=s.tool==TerrainTool::Dig?std::max(d,target):std::min(d,target);continue;
              }
              d = std::lerp(d, changed, std::clamp(s.strength*s.opacity*terrainBrushWeight(s,distance/s.radius),0.f,1.f));
            }
          }
      }
    }
    // A finite smooth volume is closed on all six sides, including its bottom.
    if (r.mode == TerrainMode::SmoothVoxel)
      for (u32 z = 0; z < n; ++z)
        for (u32 y = 0; y < ny; ++y)
          for (u32 x = 0; x < n; ++x) {
            float edge = std::min({float(x) * step, float(n - 1 - x) * step,
                                   float(z) * step, float(n - 1 - z) * step,
                                   float(y) * dy, float(ny - 1 - y) * dy});
            auto &d = field[index(x, y, z)];
            d = std::max(d, std::min(step, dy) * .01f - edge);
          }
  }
  TerrainField result;
  result.width=result.depth=n;result.height=ny;result.step=step;result.verticalStep=dy;
  result.bottom=bottom;result.solidBottom=solidBottom;
  result.heights=std::move(heights);result.density=std::move(field);
  result.legacyLayers=std::move(layers);result.holes=std::move(holes);
  if(seed)result.paintSamples=seed->paintSamples;
  result.paintSamples.insert(result.paintSamples.end(),strokes.begin(),strokes.end());
  if(r.mode==TerrainMode::Heightfield && r.controlCells && r.blendedSurface) {
    // Replay the authoritative recipe on a material grid. Never resample a prior
    // map: editing mesh cells cannot alter paint stamps or path tessellation.
    auto materialRecipe=r;materialRecipe.controlCells=0;materialRecipe.cells=std::min(128u,r.controlCells);
    TerrainField material;
    if(!buildTerrainField(materialRecipe,material,error,progress))return false;
    result.controlWidth=r.controlCells+1;result.controlStep=r.size/r.controlCells;
    result.controlWeights.resize(usize(result.controlWidth)*result.controlWidth);
    constexpr u32 paintTileSize=16;
    const u32 paintTiles=(result.controlWidth+paintTileSize-1)/paintTileSize;
    std::vector<std::vector<const TerrainStroke *>> paintBins(usize(paintTiles)*paintTiles);
    // Insert in recipe order: culling must never reorder Target/Restore stamps.
    // The enclosing rotated rectangle covers every planar shape; pitch/roll
    // deliberately have no influence on heightfield painting.
    for(const auto &stroke:material.paintSamples) {
      if(progress && !progress(0)){error="Geração cancelada.";return false;}
      if(stroke.tool!=TerrainTool::Paint && stroke.tool!=TerrainTool::Path &&
         !(stroke.tool==TerrainTool::Restore && stroke.restoreWeights))continue;
      const float extent=stroke.radius*(stroke.aspect+stroke.depthAspect);
      const float x0=(stroke.x-extent+r.size*.5f)/result.controlStep;
      const float x1=(stroke.x+extent+r.size*.5f)/result.controlStep;
      const float z0=(stroke.z-extent+r.size*.5f)/result.controlStep;
      const float z1=(stroke.z+extent+r.size*.5f)/result.controlStep;
      if(x1<0 || z1<0 || x0>float(result.controlWidth-1) || z0>float(result.controlWidth-1))continue;
      const u32 tx0=u32(std::max(0.f,std::floor(x0)))/paintTileSize;
      const u32 tz0=u32(std::max(0.f,std::floor(z0)))/paintTileSize;
      const u32 tx1=u32(std::min(float(result.controlWidth-1),std::ceil(x1)))/paintTileSize;
      const u32 tz1=u32(std::min(float(result.controlWidth-1),std::ceil(z1)))/paintTileSize;
      for(u32 tz=tz0;tz<=tz1;++tz)for(u32 tx=tx0;tx<=tx1;++tx)
        paintBins[usize(tz)*paintTiles+tx].push_back(&stroke);
    }
    std::vector<std::vector<PathSample>> paths;
    for(const auto &path:r.paths)paths.push_back(samplePath(path,result.controlStep*.5f));
    for(u32 z=0;z<result.controlWidth;++z) {
      if(progress && !progress(float(z)/result.controlWidth)){error="Geração cancelada.";return false;}
      for(u32 x=0;x<result.controlWidth;++x) {
        if(x%paintTileSize==0 && progress && !progress(float(usize(z)*result.controlWidth+x)/result.controlWeights.size())) {
          error="Geração cancelada.";return false;
        }
        const auto &paint=paintBins[usize(z/paintTileSize)*paintTiles+x/paintTileSize];
        const float px=x*result.controlStep-r.size*.5f,pz=z*result.controlStep-r.size*.5f;
        const float d=material.step;
        const V nm=normal(V{(terrainFieldHeight(material,px-d,pz)-terrainFieldHeight(material,px+d,pz))/(2*d),1,
          (terrainFieldHeight(material,px,pz-d)-terrainFieldHeight(material,px,pz+d))/(2*d)});
        result.controlWeights[usize(z)*result.controlWidth+x]=evaluateTerrainWeights(r,{px,terrainFieldHeight(material,px,pz),pz},nm,false,4,paint,paths);
      }
    }
  }
  out=std::move(result);error.clear();return true;
}
bool generateTerrainField(const TerrainRecipe &r,TerrainField &out,std::string &error,
                          const std::function<bool(float)> &progress) {
  return buildTerrainField(r,out,error,progress);
}
bool meshTerrainField(const TerrainRecipe &r,const TerrainField &data,TerrainMesh &out,
                      std::string &error,const std::function<bool(float)> &progress,
                      const TerrainMesh *previous,std::span<const TerrainChunkKey> dirty) {
  if(!validTerrainRecipe(r,error))return false;
  const u32 n=r.cells+1,ny=r.mode==TerrainMode::Heightfield?1:r.verticalCells+1;
  if(data.width!=n || data.depth!=n || data.height!=ny || data.heights.size()!=usize(n)*n ||
     data.legacyLayers.size()!=usize(n)*n || data.holes.size()!=usize(n)*n ||
     data.density.size()!=(ny>1?usize(n)*n*ny:0)) {error="Dados do terreno incompatíveis com a malha.";return false;}
  const u32 expectedControl=(r.mode==TerrainMode::Heightfield && r.blendedSurface && r.controlCells)?r.controlCells+1:0;
  if(data.controlWidth!=expectedControl || data.controlWeights.size()!=usize(expectedControl)*expectedControl) {error="Mapa de materiais incompatível com a receita.";return false;}
  if(expectedControl) {
    if(!std::isfinite(data.controlStep) || data.controlStep!=r.size/r.controlCells) {
      error="Espaçamento do mapa de materiais inválido.";return false;
    }
    for(const auto &weights:data.controlWeights) {
      float total=0;
      for(float value:weights) {
        if(!std::isfinite(value) || value<0 || value>1) {
          error="Peso do mapa de materiais inválido.";return false;
        }
        total+=value;
      }
      if(std::abs(total-1)>1e-4f) {error="Pesos do mapa de materiais não normalizados.";return false;}
    }
  }
  const auto &heights=data.heights;const auto &field=data.density;
  const auto &layers=data.legacyLayers;const auto &holes=data.holes;const auto &strokes=data.paintSamples;
  const float step=data.step,dy=data.verticalStep,bottom=data.bottom,solidBottom=data.solidBottom;
  const bool closedHeightfield=ny==1 && r.solidBase;
  const auto hidx=[&](u32 x,u32 z){return usize(z)*n+x;};
  const auto index=[&](u32 x,u32 y,u32 z){return (usize(z)*ny+y)*n+x;};
  const auto nearest=[&](float p){return u32(std::clamp(std::lround((p+r.size*.5f)/step),0l,long(n-1)));};
  const auto sample = [&](i32 x, i32 y, i32 z) {
    return field[index(u32(std::clamp(x, 0, i32(n) - 1)),
                       u32(std::clamp(y, 0, i32(ny) - 1)),
                       u32(std::clamp(z, 0, i32(n) - 1)))];
  };
  const auto gradient = [&](V p) {
    i32 x = i32(nearest(p.x)), z = i32(nearest(p.z));
    if (ny == 1) {
      return normal(
          V{(heights[hidx(u32(std::max(0, x - 1)), u32(z))] -
             heights[hidx(u32(std::min(i32(n) - 1, x + 1)), u32(z))]) /
                (2 * step),
            1,
            (heights[hidx(u32(x), u32(std::max(0, z - 1)))] -
             heights[hidx(u32(x), u32(std::min(i32(n) - 1, z + 1)))]) /
                (2 * step)});
    }
    const i32 y = i32(std::lround((p.y - bottom) / dy));
    return normal(V{(sample(x + 1, y, z) - sample(x - 1, y, z)) / (2 * step),
                    (sample(x, y + 1, z) - sample(x, y - 1, z)) / (2 * dy),
                    (sample(x, y, z + 1) - sample(x, y, z - 1)) / (2 * step)});
  };
  std::vector<std::vector<PathSample>> pathSamples;
  for(const auto &path:r.paths)pathSamples.push_back(samplePath(path,step*.5f));
  TerrainMesh mesh;
  mesh.samples = u32(heights.size() + field.size() + data.controlWeights.size());
  bool budget = false;
  std::vector<const TerrainStroke *> localPaint;
  const auto emit = [&](std::array<TerrainPart, 4> &parts, V a, V b, V c,
                        V wanted, bool flat, u32 fixedLayer = 4) {
    V face = cross(b - a, c - a);
    if (dot(face, face) < 1e-12f)
      return;
    if (dot(face, wanted) < 0) {
      std::swap(b, c);
      face = face * -1;
    }
    face = normal(face);
    V center = (a + b + c) * (1.f / 3);
    u32 layer = face.y < .65f                        ? 1u
                : center.y < r.base + r.height * .2f ? 2u
                                                     : 0u;
    const u8 painted = layers[hidx(nearest(center.x), nearest(center.z))];
    if (fixedLayer < 4)
      layer = fixedLayer;
    else if (painted)
      layer = painted - 1;
    if (++mesh.triangles > 200000) {
      budget = true;
      return;
    }
    for (V p : {a, b, c}) {
      V nm = flat ? face : gradient(p);
      TerrainVertex v{{p.x, p.y, p.z}, {nm.x, nm.y, nm.z}, {}};
      if(!r.blendedSurface)std::fill_n(v.weights,4,1.f);
      if (std::abs(face.y) >= std::max(std::abs(face.x), std::abs(face.z))) {
        v.uv[0] = p.x / r.textureScale;
        v.uv[1] = p.z / r.textureScale;
      } else if (std::abs(face.x) > std::abs(face.z)) {
        v.uv[0] = p.z / r.textureScale;
        v.uv[1] = p.y / r.textureScale;
      } else {
        v.uv[0] = p.x / r.textureScale;
        v.uv[1] = p.y / r.textureScale;
      }
      if(r.blendedSurface) {
        const auto weights=(ny==1 && fixedLayer>=4 && data.controlWidth)?terrainFieldWeights(data,p.x,p.z):
          evaluateTerrainWeights(r,p,nm,ny>1,fixedLayer,localPaint,pathSamples);
        std::copy(weights.begin(),weights.end(),v.weights);
        v.uv[0]=p.x/r.textureScale;v.uv[1]=p.z/r.textureScale;
        v.uv1[0]=p.y/r.textureScale;
        // Negative macro encoding marks closed sides/bottom as ineligible for
        // a heightfield control texture while preserving the exact variation.
        v.uv1[1]=ny==1 && r.controlCells && fixedLayer<4?-(r.macroVariation+1):r.macroVariation;
      }
      parts[layer].vertices.push_back(v);
    }
  };
  const u32 ycells = ny == 1 ? 1 : r.verticalCells;
  u32 done = 0;
  const u32 count =
      ((r.cells + 15) / 16) * ((r.cells + 15) / 16) * ((ycells + 15) / 16);
  const int corner[8][3]{{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
                         {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}};
  const int tetra[6][4]{{0, 5, 1, 6}, {0, 1, 2, 6}, {0, 2, 3, 6},
                        {0, 3, 7, 6}, {0, 7, 4, 6}, {0, 4, 5, 6}};
  for (u32 cz = 0; cz < r.cells; cz += 16)
    for (u32 cy = 0; cy < ycells; cy += 16)
      for (u32 cx = 0; cx < r.cells; cx += 16) {
        if (progress && !progress(.35f + .65f * float(done++) / float(count))) {
          error = "Geração cancelada.";
          return false;
        }
        const TerrainChunkKey key{cx/16,cy/16,cz/16};
        // Reject non-overlapping paint volumes once per chunk, preserving the
        // exact stroke order and the full-resolution samples. The conservative
        // radius also contains rotated boxes and ellipsoids. A vertex shared
        // by two chunks therefore receives the same relevant operations.
        localPaint.clear();
        if(r.blendedSurface)for(const auto &brush:strokes) {
          if(brush.tool!=TerrainTool::Paint && brush.tool!=TerrainTool::Path &&
             !(brush.tool==TerrainTool::Restore && brush.restoreWeights))continue;
          const float extent=2*brush.radius*std::max({brush.aspect,brush.verticalAspect,brush.depthAspect})+.0001f;
          if(brush.x+extent<cx*step-r.size*.5f || brush.x-extent>std::min(cx+16,r.cells)*step-r.size*.5f ||
             brush.z+extent<cz*step-r.size*.5f || brush.z-extent>std::min(cz+16,r.cells)*step-r.size*.5f)continue;
          if(ny>1 && brush.tool!=TerrainTool::Path &&
             (brush.y+extent<bottom+cy*dy || brush.y-extent>bottom+std::min(cy+16,r.verticalCells)*dy))continue;
          localPaint.push_back(&brush);
        }
        if(previous && std::find(dirty.begin(),dirty.end(),key)==dirty.end()) {
          bool nonempty=false;
          for(const auto &part:previous->parts)if(part.x==key.x && part.y==key.y && part.z==key.z) {
            mesh.parts.push_back(part);mesh.triangles+=u32(part.vertices.size()/3);nonempty=true;
          }
          if(nonempty)++mesh.chunks;
          ++mesh.reusedChunks;continue;
        }
        ++mesh.rebuiltChunks;
        std::array<TerrainPart, 4> parts;
        for (u32 i = 0; i < 4; ++i) {
          parts[i].x = cx / 16;
          parts[i].y = cy / 16;
          parts[i].z = cz / 16;
          parts[i].layer = i;
        }
        for (u32 z = cz; z < std::min(cz + 16, r.cells); ++z)
          for (u32 y = cy; y < std::min(cy + 16, ycells); ++y)
            for (u32 x = cx; x < std::min(cx + 16, r.cells); ++x) {
              if (budget) {
                error = "Malha excede 200 mil triângulos; reduza a resolução.";
                return false;
              }
              if (ny == 1) {
                if(holes[hidx(x,z)]) continue;
                const auto p = [&](u32 xx, u32 zz) {
                  return V{float(xx) * step - r.size * .5f,
                           heights[hidx(xx, zz)],
                           float(zz) * step - r.size * .5f};
                };
                V a = p(x, z), b = p(x + 1, z), c = p(x + 1, z + 1),
                  d = p(x, z + 1);
                emit(parts, a, c, b, {0, 1, 0}, false);
                emit(parts, a, d, c, {0, 1, 0}, false);
                if (closedHeightfield) {
                  const V aa{a.x, solidBottom, a.z},
                      bb{b.x, solidBottom, b.z},
                      cc{c.x, solidBottom, c.z},
                      dd{d.x, solidBottom, d.z};
                  // Subdivide the floor like the roof so that chunk seams and
                  // perimeter edges share exact endpoints, with no T junctions.
                  emit(parts, aa, bb, cc, {0, -1, 0}, true, 1);
                  emit(parts, aa, cc, dd, {0, -1, 0}, true, 1);
                  const auto wall = [&](V p0, V p1, V q0, V q1, V outward) {
                    emit(parts, p0, q0, q1, outward, true, 1);
                    emit(parts, p0, q1, p1, outward, true, 1);
                  };
                  // Close the footprint and hole rims, never ordinary chunk
                  // borders. A cut goes through the roof and floor together.
                  if (x == 0 || holes[hidx(x-1,z)])
                    wall(a, d, aa, dd, {-1, 0, 0});
                  if (x + 1 == r.cells || holes[hidx(x+1,z)])
                    wall(b, c, bb, cc, {1, 0, 0});
                  if (z == 0 || holes[hidx(x,z-1)])
                    wall(a, b, aa, bb, {0, 0, -1});
                  if (z + 1 == r.cells || holes[hidx(x,z+1)])
                    wall(d, c, dd, cc, {0, 0, 1});
                }
                continue;
              }
              V p[8];
              float d[8];
              for (u32 i = 0; i < 8; ++i) {
                u32 xx = x + u32(corner[i][0]), yy = y + u32(corner[i][1]),
                    zz = z + u32(corner[i][2]);
                p[i] = {float(xx) * step - r.size * .5f,
                        bottom + float(yy) * dy,
                        float(zz) * step - r.size * .5f};
                d[i] = field[index(xx, yy, zz)];
              }
              if (r.mode == TerrainMode::BlockVoxel) {
                if (d[0] >= 0)
                  continue;
                const int faces[6][4]{{0, 4, 7, 3}, {1, 2, 6, 5}, {0, 1, 5, 4},
                                      {3, 7, 6, 2}, {0, 3, 2, 1}, {4, 5, 6, 7}};
                const int dir[6][3]{{-1, 0, 0}, {1, 0, 0},  {0, -1, 0},
                                    {0, 1, 0},  {0, 0, -1}, {0, 0, 1}};
                for (u32 f = 0; f < 6; ++f) {
                  i32 xx = i32(x) + dir[f][0], yy = i32(y) + dir[f][1],
                      zz = i32(z) + dir[f][2];
                  if (xx >= 0 && xx < i32(r.cells) && yy >= 0 &&
                      yy < i32(ycells) && zz >= 0 && zz < i32(r.cells) &&
                      sample(xx, yy, zz) < 0)
                    continue;
                  const auto *q = faces[f];
                  V norm{float(dir[f][0]), float(dir[f][1]), float(dir[f][2])};
                  emit(parts, p[q[0]], p[q[1]], p[q[2]], norm, true);
                  emit(parts, p[q[0]], p[q[2]], p[q[3]], norm, true);
                }
                continue;
              }
              for (const auto &t : tetra) {
                int inside[4], outside[4], ni = 0, no = 0;
                for (int v : t) {
                  if (d[v] < 0)
                    inside[ni++] = v;
                  else
                    outside[no++] = v;
                }
                if (!ni || !no)
                  continue;
                const auto lerp = [&](int a, int b) {
                  return p[a] + (p[b] - p[a]) * (d[a] / (d[a] - d[b]));
                };
                V direction{};
                for (int i = 0; i < no; ++i)
                  direction = direction + p[outside[i]] * (1.f / float(no));
                for (int i = 0; i < ni; ++i)
                  direction = direction - p[inside[i]] * (1.f / float(ni));
                if (ni == 1) {
                  emit(parts, lerp(inside[0], outside[0]),
                       lerp(inside[0], outside[1]), lerp(inside[0], outside[2]),
                       direction, false);
                } else if (ni == 3) {
                  emit(parts, lerp(outside[0], inside[0]),
                       lerp(outside[0], inside[1]), lerp(outside[0], inside[2]),
                       direction, false);
                } else {
                  V a = lerp(inside[0], outside[0]),
                    b = lerp(inside[0], outside[1]),
                    c = lerp(inside[1], outside[0]),
                    e = lerp(inside[1], outside[1]);
                  emit(parts, a, b, c, direction, false);
                  emit(parts, b, e, c, direction, false);
                }
              }
            }
        bool nonempty = false;
        for (auto &p : parts)
          if (!p.vertices.empty()) {
            nonempty = true;
            mesh.parts.push_back(std::move(p));
          }
        if (nonempty)
          ++mesh.chunks;
      }
  if (budget || mesh.parts.empty()) {
    error = "Volume vazio ou limite de malha excedido.";
    return false;
  }
  out = std::move(mesh);
  error.clear();
  return true;
}
bool generateTerrain(const TerrainRecipe &r,TerrainMesh &out,std::string &error,
                     const std::function<bool(float)> &progress) {
  TerrainField field;
  return generateTerrainField(r,field,error,progress) && meshTerrainField(r,field,out,error,progress);
}
bool updateTerrain(const TerrainRecipe &r,const TerrainBuild *previous,TerrainBuild &out,
                   std::string &error,const std::function<bool(float)> &progress) {
  TerrainBuild next;next.recipe=r;
  bool incremental=false;
  if(previous && r.strokes.size()>=previous->recipe.strokes.size()) {
    auto prefix=r;prefix.strokes.resize(previous->recipe.strokes.size());
    incremental=r.paths.empty() && serializeTerrainRecipe(prefix)==serializeTerrainRecipe(previous->recipe);
    // Changing height under an existing carved volume requires replay; material
    // and density-only edits keep their exact ordered semantics incrementally.
    for(usize i=previous->recipe.strokes.size();incremental && i<r.strokes.size();++i) {
      const auto &stroke=r.strokes[i];const auto tool=stroke.tool;
      if(r.mode!=TerrainMode::Heightfield && tool!=TerrainTool::Paint && tool!=TerrainTool::Dig && tool!=TerrainTool::Add &&
         tool!=TerrainTool::Smooth && tool!=TerrainTool::Sharpen && tool!=TerrainTool::Restore)incremental=false;
      // A target uses its pre-gesture field. Replaying that gesture preserves
      // the cap when the previous preview ended halfway through the gesture.
      if(stroke.application==TerrainBrushApplication::Target && i && stroke.gesture==r.strokes[i-1].gesture)incremental=false;
      // Stored smooth-volume fields have a closed finite outer boundary.
      // Any density operator touching it must read pre-closure values (even
      // partial-opacity CSG can differ). Replay that field; interior density
      // edits and regional mesh invalidation keep their incremental path.
      if(r.mode==TerrainMode::SmoothVoxel && tool!=TerrainTool::Paint) {
        const auto &field=previous->field;
        const float radius=stroke.radius*std::max({stroke.aspect,stroke.verticalAspect,stroke.depthAspect})*2;
        const float halo=(tool==TerrainTool::Smooth || tool==TerrainTool::Sharpen)?float(stroke.filterRadius):0;
        const float horizontal=radius+halo*field.step;
        const float vertical=radius+halo*field.verticalStep;
        if(std::abs(stroke.x)+horizontal>=r.size*.5f || std::abs(stroke.z)+horizontal>=r.size*.5f ||
           stroke.y-vertical<=field.bottom || stroke.y+vertical>=field.bottom+(field.height-1)*field.verticalStep)
          incremental=false;
      }
    }
  }
  if(!buildTerrainField(r,next.field,error,progress,incremental?&previous->field:nullptr,
                       incremental?previous->recipe.strokes.size():0))return false;
  bool compatible=previous!=nullptr;
  if(previous) {
    auto a=r,b=previous->recipe;a.strokes.clear();b.strokes.clear();
    // Path geometry is covered by field differences below. Material-only
    // changes additionally dirty the old AND new path footprints. Keeping
    // paths here used to invalidate every chunk on any point/profile edit.
    a.paths.clear();b.paths.clear();
    a.lodLevels=b.lodLevels=1;a.lodRelativeError=b.lodRelativeError=.01f;
    compatible=compatible && serializeTerrainRecipe(a)==serializeTerrainRecipe(b) &&
      next.field.solidBottom==previous->field.solidBottom && next.field.heights.size()==previous->field.heights.size() &&
      next.field.density.size()==previous->field.density.size();
    compatible=compatible && next.field.controlWidth==previous->field.controlWidth &&
      next.field.controlWeights.size()==previous->field.controlWeights.size();
  }
  const u32 nx=(r.cells+15)/16,ny=r.mode==TerrainMode::Heightfield?1:(r.verticalCells+15)/16;
  std::vector<u8> changed(usize(nx)*nx*ny,!compatible);
  const auto mark=[&](i32 x,i32 y,i32 z) {
    if(x<0 || z<0 || y<0 || x>=i32(r.cells) || z>=i32(r.cells) || y>=i32(r.mode==TerrainMode::Heightfield?1:r.verticalCells))return;
    changed[(usize(z/16)*ny+u32(y/16))*nx+u32(x/16)]=1;
  };
  if(compatible) {
    const auto &a=next.field,&b=previous->field;
    // Bilinear sampling spreads a changed texel over both adjacent intervals.
    // Convert those intervals to mesh cells and include shared-vertex halos.
    for(u32 z=0;z<a.controlWidth;++z)for(u32 x=0;x<a.controlWidth;++x) {
      const usize i=usize(z)*a.controlWidth+x;
      if(a.controlWeights[i]==b.controlWeights[i])continue;
      const float ratio=a.controlStep/a.step;
      const i32 x0=i32(std::floor((float(x)-1)*ratio))-1;
      const i32 x1=i32(std::ceil((float(x)+1)*ratio))+1;
      const i32 z0=i32(std::floor((float(z)-1)*ratio))-1;
      const i32 z1=i32(std::ceil((float(z)+1)*ratio))+1;
      for(i32 mz=std::max(0,z0);mz<=std::min(i32(r.cells)-1,z1);++mz)
        for(i32 mx=std::max(0,x0);mx<=std::min(i32(r.cells)-1,x1);++mx)mark(mx,0,mz);
    }
    // One-cell halo accounts for central-difference normals and cells that
    // reference a changed corner on the other side of a chunk boundary.
    for(u32 z=0;z<a.depth;++z)for(u32 x=0;x<a.width;++x) {
      const usize i=usize(z)*a.width+x;
      if(a.height==1 && (a.heights[i]!=b.heights[i] || a.holes[i]!=b.holes[i] || a.legacyLayers[i]!=b.legacyLayers[i]))
        for(i32 dz=-2;dz<=1;++dz)for(i32 dx=-2;dx<=1;++dx)mark(i32(x)+dx,0,i32(z)+dz);
      if(a.height>1)for(u32 y=0;y<a.height;++y) {
        const usize j=(usize(z)*a.height+y)*a.width+x;
        if(a.density[j]!=b.density[j])for(i32 dz=-2;dz<=1;++dz)for(i32 dy=-2;dy<=1;++dy)for(i32 dx=-2;dx<=1;++dx)
          mark(i32(x)+dx,i32(y)+dy,i32(z)+dz);
      }
    }
    // Material operations do not necessarily move density. Mark both old and
    // new influence when a stroke was edited, removed, reordered or appended.
    const auto same=[](const TerrainStroke &a,const TerrainStroke &b) {
      TerrainRecipe x,y;x.strokes={a};y.strokes={b};return serializeTerrainRecipe(x)==serializeTerrainRecipe(y);
    };
    usize prefix=0;while(prefix<r.strokes.size() && prefix<previous->recipe.strokes.size() && same(r.strokes[prefix],previous->recipe.strokes[prefix]))++prefix;
    const auto paintRegions=[&](const std::vector<TerrainStroke> &strokes) {
      for(usize i=prefix;i<strokes.size();++i) {
        const auto &brush=strokes[i];
        if(brush.tool!=TerrainTool::Paint && brush.tool!=TerrainTool::Path && brush.tool!=TerrainTool::Restore)continue;
        const float radius=brush.radius*std::max({brush.aspect,brush.verticalAspect,brush.depthAspect})*2;
        const i32 x0=i32(std::floor((brush.x-radius+r.size*.5f)/a.step))-2,x1=i32(std::ceil((brush.x+radius+r.size*.5f)/a.step))+1;
        const i32 z0=i32(std::floor((brush.z-radius+r.size*.5f)/a.step))-2,z1=i32(std::ceil((brush.z+radius+r.size*.5f)/a.step))+1;
        for(i32 z=std::max(0,z0);z<=std::min(i32(r.cells)-1,z1);++z)
          for(i32 x=std::max(0,x0);x<=std::min(i32(r.cells)-1,x1);++x)
            for(u32 y=0;y<ny;++y)mark(x,i32(y*16),z);
      }
    };
    paintRegions(r.strokes);paintRegions(previous->recipe.strokes);
    usize pathPrefix=0;
    while(pathPrefix<r.paths.size() && pathPrefix<previous->recipe.paths.size() &&
          r.paths[pathPrefix]==previous->recipe.paths[pathPrefix])++pathPrefix;
    const auto pathRegions=[&](const std::vector<TerrainPath> &paths) {
      // Reordering may change ordered overpaint and Smart group composition.
      // Include the whole changed suffix instead of matching only path IDs.
      for(usize i=pathPrefix;i<paths.size();++i) {
        const auto &path=paths[i];
        if(!path.enabled || !path.texture || path.points.size()<2)continue;
        const auto samples=samplePath(path,a.step*.5f);
        if(samples.empty())continue;
        float xmin=INFINITY,xmax=-INFINITY,zmin=INFINITY,zmax=-INFINITY;
        for(const auto &sample:samples) {
          xmin=std::min(xmin,sample.p.x);xmax=std::max(xmax,sample.p.x);
          zmin=std::min(zmin,sample.p.z);zmax=std::max(zmax,sample.p.z);
        }
        // Includes square-cap extension, shoulder and texture falloff. An
        // extra cell covers shared chunk vertices exactly on the footprint.
        const float radius=path.width*.5f+path.shoulderWidth*.5f+path.textureTransition;
        const i32 x0=i32(std::floor((xmin-radius+r.size*.5f)/a.step))-2;
        const i32 x1=i32(std::ceil((xmax+radius+r.size*.5f)/a.step))+1;
        const i32 z0=i32(std::floor((zmin-radius+r.size*.5f)/a.step))-2;
        const i32 z1=i32(std::ceil((zmax+radius+r.size*.5f)/a.step))+1;
        for(i32 z=std::max(0,z0);z<=std::min(i32(r.cells)-1,z1);++z)
          for(i32 x=std::max(0,x0);x<=std::min(i32(r.cells)-1,x1);++x)
            for(u32 y=0;y<ny;++y)mark(x,i32(y*16),z);
      }
    };
    pathRegions(r.paths);pathRegions(previous->recipe.paths);
  }
  for(u32 z=0;z<nx;++z)for(u32 y=0;y<ny;++y)for(u32 x=0;x<nx;++x)
    if(changed[(usize(z)*ny+y)*nx+x])next.dirty.push_back({x,y,z});
  if(!meshTerrainField(r,next.field,next.mesh,error,progress,compatible?&previous->mesh:nullptr,next.dirty))return false;
  out=std::move(next);return true;
}
// PNG RGBA8 with stored DEFLATE blocks: deterministic, lossless, no codec
// dependency in the terrain-only target. CRC and Adler cover encoded bytes.
bool makeTerrainControlTexture(const TerrainField &field,renderer::SharedAuthoringTexture &out,std::string &error) {
  if(field.height!=1 || field.controlWidth<9 || field.controlWidth>513 || (field.controlWidth-1)%8 ||
     !std::isfinite(field.controlStep) || field.controlStep<=0 ||
     field.controlWeights.size()!=usize(field.controlWidth)*field.controlWidth) {error="Campo do mapa de pintura inválido.";return false;}
  renderer::AuthoringTexture texture;texture.width=texture.height=field.controlWidth;texture.levels=1;
  texture.srgb=false;texture.samplerFlags=renderer::encodeGltfTextureSampler(9729,9729)|renderer::AuthoringTextureNoAnisotropy;
  texture.mipChain.reserve(field.controlWeights.size()*4);
  for(const auto &weights:field.controlWeights) {
    double sum=0;
    for(float weight:weights) {
      if(!std::isfinite(weight) || weight<0 || weight>1){error="Peso do mapa de pintura inválido.";return false;}
      sum+=weight;
    }
    if(std::abs(sum-1)>.001){error="Os pesos do mapa de pintura devem somar 1.";return false;}
    std::array<u8,4> quantized{};std::array<double,4> remainder{};u32 total=0;
    for(u32 i=0;i<4;++i) {
      const double scaled=weights[i]/sum*255;
      quantized[i]=u8(std::floor(scaled));total+=quantized[i];remainder[i]=scaled-quantized[i];
    }
    // Largest remainder quantization preserves exactly 255 units per texel.
    while(total<255) {
      const auto best=std::max_element(remainder.begin(),remainder.end());
      ++quantized[usize(best-remainder.begin())];*best=-1;++total;
    }
    texture.mipChain.insert(texture.mipChain.end(),quantized.begin(),quantized.end());
  }
  out=shareDecodedTexture(std::move(texture));error.clear();return true;
}
renderer::TerrainControlBinding terrainControlBinding(const TerrainRecipe &r,const TerrainField &field,u32 index) {
  renderer::TerrainControlBinding binding;
  if(r.mode!=TerrainMode::Heightfield || !r.blendedSurface || !r.controlCells ||
     field.height!=1 || field.controlWidth!=r.controlCells+1 || !std::isfinite(r.size) || r.size<=0 ||
     !std::isfinite(r.textureScale) || r.textureScale<=0)return binding;
  binding.textureIndex=index;
  binding.uvScale[0]=binding.uvScale[1]=r.textureScale*float(field.controlWidth-1)/(float(field.controlWidth)*r.size);
  binding.uvOffset[0]=binding.uvOffset[1]=.5f;return binding;
}
static std::vector<u8> terrainControlPng(const renderer::AuthoringTexture &texture) {
  std::vector<u8> raw;
  for(u32 y=0;y<texture.height;++y) {
    raw.push_back(0);
    const auto start=texture.mipChain.begin()+usize(y)*texture.width*4;
    raw.insert(raw.end(),start,start+usize(texture.width)*4);
  }
  std::vector<u8> png{137,80,78,71,13,10,26,10};
  const auto be=[](std::vector<u8> &b,u32 v){for(int shift=24;shift>=0;shift-=8)b.push_back(u8(v>>shift));};
  const auto chunk=[&](const char *type,const std::vector<u8> &data) {
    be(png,u32(data.size()));const usize start=png.size();
    png.insert(png.end(),type,type+4);png.insert(png.end(),data.begin(),data.end());
    u32 crc=0xffffffffu;
    for(usize i=start;i<png.size();++i){crc^=png[i];for(u32 bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320u&u32(-i32(crc&1)));}
    be(png,crc^0xffffffffu);
  };
  std::vector<u8> header;be(header,texture.width);be(header,texture.height);
  header.insert(header.end(),{8,6,0,0,0});chunk("IHDR",header);
  std::vector<u8> compressed{0x78,0x01};
  for(usize offset=0;offset<raw.size();) {
    const u32 count=u32(std::min<usize>(65535,raw.size()-offset));
    compressed.push_back(offset+count==raw.size()?1:0);
    compressed.push_back(u8(count));compressed.push_back(u8(count>>8));
    compressed.push_back(u8(~count));compressed.push_back(u8((~count)>>8));
    compressed.insert(compressed.end(),raw.begin()+offset,raw.begin()+offset+count);offset+=count;
  }
  u32 a=1,b=0;for(u8 v:raw){a=(a+v)%65521;b=(b+a)%65521;}
  be(compressed,(b<<16)|a);chunk("IDAT",compressed);chunk("IEND",{});return png;
}
static bool writeTerrainGlbImpl(const TerrainRecipe &r, const TerrainMesh &mesh,
                     const std::array<std::vector<u8>, 4> &textures,
                     std::vector<u8> &out, std::string &error,const TerrainField *prepared) {
  if (!validTerrainRecipe(r, error) || mesh.parts.empty() ||
      mesh.triangles > 200000)
    return false;
  std::vector<u8> binary;
  std::ostringstream views, accessors, nodes, meshes, materials, images, tex;
  u32 viewCount = 0, accessorCount = 0, imageCount = 0;
  for (auto *s :
       {&views, &accessors, &nodes, &meshes, &materials, &images, &tex}) {
    s->imbue(std::locale::classic());
    *s << std::setprecision(9);
  }
  const auto view = [&](const void *data, usize bytes, u32 stride) {
    while (binary.size() % 4)
      binary.push_back(0);
    u32 i = viewCount++;
    if (i)
      views << ',';
    views << "{\"buffer\":0,\"byteOffset\":" << binary.size()
          << ",\"byteLength\":" << bytes;
    if (stride)
      views << ",\"byteStride\":" << stride;
    views << '}';
    const auto *p = static_cast<const u8 *>(data);
    binary.insert(binary.end(), p, p + bytes);
    return i;
  };
  const char *names[]{"Grama", "Rocha", "Terra", "Caminho"};
  const char *colors[]{"0.28,0.44,0.16,1", "0.48,0.46,0.42,1",
                       "0.42,0.27,0.15,1", "0.52,0.43,0.3,1"};
  const bool blended=r.blendedSurface && std::all_of(textures.begin(),textures.end(),[](const auto &t){return !t.empty();});
  const bool control=blended && r.mode==TerrainMode::Heightfield && r.controlCells;
  TerrainField generatedField;
  const TerrainField *controlField=prepared;
  if(control && prepared) {
    if(prepared->height!=1 || prepared->width!=r.cells+1 || prepared->depth!=r.cells+1 ||
       prepared->heights.size()!=usize(r.cells+1)*(r.cells+1) || prepared->controlWidth!=r.controlCells+1 ||
       prepared->step!=r.size/r.cells || prepared->controlStep!=r.size/r.controlCells) {
      error="O campo calculado não corresponde à receita do terreno.";return false;
    }
  }
  if(control && !prepared) {
    if(!generateTerrainField(r,generatedField,error))return false;
    controlField=&generatedField;
  }
  for (u32 i = 0; i < 4; ++i) {
    if (i)
      materials << ',';
    materials << "{\"name\":\"" << names[i]
              << "\",\"pbrMetallicRoughness\":{\"metallicFactor\":0,"
                 "\"roughnessFactor\":0.95";
    if (!textures[i].empty()) {
      u32 v = view(textures[i].data(), textures[i].size(), 0);
      if (imageCount) {
        images << ',';
        tex << ',';
      }
      images << "{\"bufferView\":" << v << ",\"mimeType\":\"image/png\"}";
      tex << "{\"source\":" << imageCount << ",\"sampler\":0}";
      materials << ",\"baseColorTexture\":{\"index\":" << imageCount++ << '}';
    } else
      materials << ",\"baseColorFactor\":[" << colors[i] << ']';
    materials << "}";
    if(blended) {
      materials << ",\"extras\":{\"astraTerrainSurface\":" << (control?2:1) << ",\"grassTexture\":{\"index\":0},\"rockTexture\":{\"index\":1},\"soilTexture\":{\"index\":2},\"pathTexture\":{\"index\":3}";
      if(control) {
        const float scale=r.textureScale*float(r.controlCells)/(float(r.controlCells+1)*r.size);
        materials << ",\"controlTexture\":{\"index\":4},\"controlUvScale\":[" << scale << ',' << scale
                  << "],\"controlUvOffset\":[0.5,0.5]";
      }
      materials << '}';
    }
    materials << "}";
  }
  if(control) {
    renderer::SharedAuthoringTexture texture;
    if(!makeTerrainControlTexture(*controlField,texture,error))return false;
    const auto png=terrainControlPng(*texture);const u32 v=view(png.data(),png.size(),0);
    images << ",{\"bufferView\":" << v << ",\"mimeType\":\"image/png\"}";
    tex << ",{\"source\":" << imageCount++ << ",\"sampler\":1}";
  }
  for (usize i = 0; i < mesh.parts.size(); ++i) {
    const auto &p = mesh.parts[i];
    if (p.vertices.empty() || p.layer > 3) {
      error = "Parte de terreno inválida.";
      return false;
    }
    // Share only bit-identical vertices, including normals, UVs and all
    // material weights. This reduces storage/GPU traffic without changing
    // silhouettes, texture resolution, hard edges or splat boundaries.
    std::vector<TerrainVertex> unique;
    std::vector<u32> indices;
    indexTerrainVertices(p.vertices, unique, indices);
    u32 v = view(unique.data(), unique.size() * sizeof(TerrainVertex),
                 sizeof(TerrainVertex));
    float lo[3]{INFINITY, INFINITY, INFINITY},
        hi[3]{-INFINITY, -INFINITY, -INFINITY};
    for (const auto &vertex : p.vertices)
      for (u32 a = 0; a < 3; ++a) {
        lo[a] = std::min(lo[a], vertex.position[a]);
        hi[a] = std::max(hi[a], vertex.position[a]);
      }
    u32 first = accessorCount;
    for (u32 attr = 0; attr < 5; ++attr) {
      if (accessorCount++)
        accessors << ',';
      accessors << "{\"bufferView\":" << v << ",\"byteOffset\":" << (attr==0?0:attr==1?12:attr==2?24:attr==3?32:40)
                << ",\"componentType\":5126,\"count\":" << unique.size()
                << ",\"type\":\"" << (attr == 4 ? "VEC4" : attr>=2 ? "VEC2" : "VEC3") << '"';
      if (attr == 0)
        accessors << ",\"min\":[" << lo[0] << ',' << lo[1] << ',' << lo[2]
                  << "],\"max\":[" << hi[0] << ',' << hi[1] << ',' << hi[2]
                  << ']';
      accessors << '}';
    }
    const u32 indexView=view(indices.data(),indices.size()*sizeof(u32),0);
    const u32 indexAccessor=accessorCount++;
    accessors << ",{\"bufferView\":" << indexView << ",\"componentType\":5125,\"count\":" << indices.size() << ",\"type\":\"SCALAR\"}";
    if (i) {
      nodes << ',';
      meshes << ',';
    }
    nodes << "{\"name\":\"Chunk_" << p.x << '_' << p.y << '_' << p.z << '_'
          << p.layer << "\",\"mesh\":" << i << '}';
    meshes << "{\"primitives\":[{\"attributes\":{\"POSITION\":" << first
           << ",\"NORMAL\":" << first + 1 << ",\"TEXCOORD_0\":" << first + 2
           << ",\"TEXCOORD_1\":" << first+3 << ",\"COLOR_0\":" << first+4
           << "},\"indices\":" << indexAccessor << ",\"material\":" << p.layer << "}]}";
  }
  while (binary.size() % 4)
    binary.push_back(0);
  std::ostringstream json;
  json.imbue(std::locale::classic());
  json << "{\"asset\":{\"version\":\"2.0\",\"generator\":\"Astra Terrain "
          "1\"},\"extras\":{\"astraTerrain\":\""
       << serializeTerrainRecipe(r)
       << "\"},\"scene\":0,\"scenes\":[{\"nodes\":[";
  for (usize i = 0; i < mesh.parts.size(); ++i) {
    if (i)
      json << ',';
    json << i;
  }
  json << "]}],\"nodes\":[" << nodes.str() << "],\"meshes\":[" << meshes.str()
       << "],\"materials\":[" << materials.str() << "],\"bufferViews\":["
       << views.str() << "],\"accessors\":[" << accessors.str()
       << "],\"buffers\":[{\"byteLength\":" << binary.size() << "}]";
  if (imageCount)
    json << ",\"images\":[" << images.str() << "],\"textures\":[" << tex.str()
         << "],\"samplers\":[{\"magFilter\":9729,\"minFilter\":9987,\"wrapS\":"
            "10497,\"wrapT\":10497},{\"magFilter\":9729,\"minFilter\":9729,\"wrapS\":33071,\"wrapT\":33071}]";
  json << '}';
  auto text = json.str();
  while (text.size() % 4)
    text.push_back(' ');
  std::vector<u8> bytes;
  word(bytes, 0x46546c67);
  word(bytes, 2);
  word(bytes, u32(28 + text.size() + binary.size()));
  word(bytes, u32(text.size()));
  word(bytes, 0x4e4f534a);
  bytes.insert(bytes.end(), text.begin(), text.end());
  word(bytes, u32(binary.size()));
  word(bytes, 0x004e4942);
  bytes.insert(bytes.end(), binary.begin(), binary.end());
  out = std::move(bytes);
  error.clear();
  return true;
}
bool writeTerrainGlb(const TerrainRecipe &r,const TerrainMesh &mesh,
                     const std::array<std::vector<u8>,4> &textures,std::vector<u8> &out,std::string &error) {
  return writeTerrainGlbImpl(r,mesh,textures,out,error,nullptr);
}
bool writeTerrainGlb(const TerrainBuild &build,const std::array<std::vector<u8>,4> &textures,
                     std::vector<u8> &out,std::string &error) {
  return writeTerrainGlbImpl(build.recipe,build.mesh,textures,out,error,&build.field);
}
bool terrainRecipeFromGlb(std::span<const u8> b, TerrainRecipe &out,
                          std::string &error) {
  if (b.size() < 20 || word(b, 0) != 0x46546c67 || word(b, 4) != 2 ||
      word(b, 8) != b.size() || word(b, 16) != 0x4e4f534a ||
      word(b, 12) > b.size() - 20) {
    error = "GLB de terreno inválido.";
    return false;
  }
  std::string_view json(reinterpret_cast<const char *>(b.data() + 20),
                        word(b, 12));
  constexpr std::string_view marker = "\"astraTerrain\":\"";
  auto start = json.find(marker);
  if (start == json.npos) {
    error = "Selecione um terreno gerado pela Astra.";
    return false;
  }
  start += marker.size();
  auto end = json.find('"', start);
  if (end == json.npos) {
    error = "Receita truncada.";
    return false;
  }
  return deserializeTerrainRecipe(json.substr(start, end - start), out, error);
}
} // namespace ae::resources
