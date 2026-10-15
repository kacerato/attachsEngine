#include "resources/terrain_asset.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

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
} // namespace
float terrainBrushDistance(const TerrainStroke &s, float dx, float dy, float dz, bool volume) {
  const float a=s.rotation*.01745329252f, c=std::cos(a), t=std::sin(a);
  const float x=(dx*c+dz*t)/s.aspect, z=-dx*t+dz*c;
  if (s.shape==TerrainBrushShape::Box)
    return std::max({std::abs(x),std::abs(z),volume?std::abs(dy):0.f})/s.radius;
  if (volume && s.shape==TerrainBrushShape::Hemisphere && dy<0) return 2;
  if (volume && s.shape==TerrainBrushShape::Cone) {
    if(dy<0 || dy>s.radius*2) return 2;
    return std::sqrt(x*x+z*z)/std::max(.001f,s.radius-dy*.5f);
  }
  return std::sqrt(x*x+z*z+(volume?dy*dy:0))/s.radius;
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
  const float values[]{r.size, r.height, r.base, r.frequency, r.textureScale,
                       r.baseDepth, r.macroVariation};
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
  if (r.baseDepth < .1f || r.baseDepth > 512) {
    error = "Espessura mínima da base: 0,1–512 unidades.";
    return false;
  }
  if (r.mode != TerrainMode::Heightfield &&
      u64(r.cells + 1) * (r.cells + 1) * (r.verticalCells + 1) > 600000) {
    error = "Volume excede 600 mil amostras; reduza a resolução.";
    return false;
  }
  for (const auto &s : r.strokes) {
    const float v[]{s.x, s.y, s.z, s.radius, s.strength, s.hardness, s.rotation, s.aspect};
    for (float n : v)
      if (!std::isfinite(n)) {
        error = "Pincel não finito.";
        return false;
      }
    if (u32(s.tool) > 12 || u32(s.shape)>3 || s.hardness<0 || s.hardness>1 || s.aspect<.1f || s.aspect>10 || std::abs(s.rotation)>360 || s.layer > 3 || s.radius < .1f || s.radius > 2048 ||
        s.strength < 0 || s.strength > 128 || std::abs(s.x) > 4096 ||
        std::abs(s.z) > 4096 || std::abs(s.y) > 2048) {
      error = "Pincel fora dos limites.";
      return false;
    }
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
  s << std::setprecision(9) << "AETERRAIN3 " << u32(r.mode) << ' ' << r.seed
    << ' ' << r.cells << ' ' << r.verticalCells << ' ' << r.octaves << ' '
    << r.size << ' ' << r.height << ' ' << r.base << ' ' << r.frequency << ' '
    << r.textureScale << ' ' << r.island << ' ' << r.collision << ' '
    << r.solidBase << ' ' << r.baseDepth << ' ' << r.blendedSurface << ' ' << r.macroVariation << ' ' << r.strokes.size();
  for (const auto &v : r.strokes)
    s << ' ' << u32(v.tool) << ' ' << v.x << ' ' << v.y << ' ' << v.z << ' '
      << v.radius << ' ' << v.strength << ' ' << v.layer << ' ' << u32(v.shape) << ' ' << v.hardness << ' ' << v.rotation << ' ' << v.aspect << ' ' << v.gesture;
  return s.str();
}
bool deserializeTerrainRecipe(std::string_view text, TerrainRecipe &out,
                              std::string &error) {
  if (text.size() > 1048576) {
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
      (magic != "AETERRAIN1" && magic != "AETERRAIN2" && magic != "AETERRAIN3")) {
    error = "Receita de terreno inválida.";
    return false;
  }
  r.blendedSurface = false;
  if ((magic != "AETERRAIN1" && !(s >> r.solidBase >> r.baseDepth)) ||
      (magic == "AETERRAIN3" && !(s >> r.blendedSurface >> r.macroVariation)) ||
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
    if (magic == "AETERRAIN3") {
      u32 shape=0;
      if (!(s >> shape >> v.hardness >> v.rotation >> v.aspect >> v.gesture)) { error="Pincel truncado.";return false; }
      v.shape=TerrainBrushShape(shape);
    }
    v.tool = TerrainTool(tool);
    if(magic!="AETERRAIN3" && (v.tool==TerrainTool::Dig || v.tool==TerrainTool::Add))v.hardness=1;
  }
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
bool generateTerrain(const TerrainRecipe &r, TerrainMesh &out,
                     std::string &error,
                     const std::function<bool(float)> &progress) {
  if (!validTerrainRecipe(r, error))
    return false;
  const u32 n = r.cells + 1,
            ny = r.mode == TerrainMode::Heightfield ? 1 : r.verticalCells + 1;
  const float step = r.size / float(r.cells),
              bottom = -std::max(4.f, r.height * .25f),
              top = r.base + r.height + std::max(4.f, r.height * .25f),
              dy = (top - bottom) / float(r.verticalCells);
  std::vector<float> heights(usize(n) * n);
  std::vector<u8> layers(heights.size()), holes(heights.size());
  std::vector<TerrainStroke> strokes;
  for (usize i = 0; i < r.strokes.size(); ++i) {
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
  for (u32 z = 0; z < n; ++z)
    for (u32 x = 0; x < n; ++x)
      heights[hidx(x, z)] = height(r, float(x) * step - r.size * .5f,
                                   float(z) * step - r.size * .5f);
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
      for (u32 z = 0; z < n; ++z)
        for (u32 x = 0; x < n; ++x) {
          float dx = float(x) * step - r.size * .5f - s.x,
                dz = float(z) * step - r.size * .5f - s.z;
          float w=terrainBrushWeight(s,terrainBrushDistance(s,dx,0,dz,false));
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
            for (i32 oz = -1; oz <= 1; ++oz)
              for (i32 ox = -1; ox <= 1; ++ox) {
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
            h=std::lerp(h,height(r,float(x)*step-r.size*.5f,float(z)*step-r.size*.5f),std::min(1.f,w*s.strength));
            if(w*s.strength>.5f){holes[hidx(x,z)]=0;layers[hidx(x,z)]=0;}break;
          case TerrainTool::Hole:
            if(w*s.strength>.5f)holes[hidx(x,z)]=1;break;
          default:
            break;
          }
          if ((s.tool == TerrainTool::Paint || s.tool == TerrainTool::Path) &&
              w * s.strength > .01f)
            layers[hidx(x, z)] = u8(s.layer + 1);
        }
    }
  const bool closedHeightfield = ny == 1 && r.solidBase;
  const float solidBottom =
      closedHeightfield
          ? *std::min_element(heights.begin(), heights.end()) - r.baseDepth
          : 0.f;
  const auto nearest = [&](float p) {
    return u32(
        std::clamp(std::lround((p + r.size * .5f) / step), 0l, long(n - 1)));
  };
  const auto index = [&](u32 x, u32 y, u32 z) {
    return (usize(z) * ny + y) * n + x;
  };
  std::vector<float> field;
  if (ny > 1) {
    field.resize(usize(n) * n * ny);
    for (u32 z = 0; z < n; ++z) {
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
    for (const auto &s : r.strokes) {
      if(s.strength == 0) continue;
      if (s.tool != TerrainTool::Dig && s.tool != TerrainTool::Add &&
          s.tool != TerrainTool::Smooth && s.tool != TerrainTool::Sharpen && s.tool != TerrainTool::Restore)
        continue;
      const auto previous =
          (s.tool == TerrainTool::Smooth || s.tool == TerrainTool::Sharpen) ? field : std::vector<float>{};
      for (u32 z = 0; z < n; ++z) {
        if (progress && !progress(.3f)) {
          error = "Geração cancelada.";
          return false;
        }
        for (u32 y = 0; y < ny; ++y)
          for (u32 x = 0; x < n; ++x) {
            V p{float(x) * step - r.size * .5f, bottom + float(y) * dy,
                float(z) * step - r.size * .5f};
            V q = p - V{s.x, s.y, s.z};
            float distance = terrainBrushDistance(s,q.x,q.y,q.z,true)*s.radius;
            auto &d = field[index(x, y, z)];
            if (s.tool == TerrainTool::Restore) {
              float w=terrainBrushWeight(s,distance/s.radius);
              d=std::lerp(d,p.y-height(r,p.x,p.z),std::min(1.f,w*s.strength));
            } else if (s.tool == TerrainTool::Smooth || s.tool == TerrainTool::Sharpen) {
              if (distance >= s.radius)
                continue;
              float sum = 0;
              u32 count = 0;
              for (i32 a = -1; a <= 1; ++a)
                for (i32 b = -1; b <= 1; ++b)
                  for (i32 c = -1; c <= 1; ++c) {
                    sum += previous[index(
                        u32(std::clamp(i32(x) + a, 0, i32(n) - 1)),
                        u32(std::clamp(i32(y) + b, 0, i32(ny) - 1)),
                        u32(std::clamp(i32(z) + c, 0, i32(n) - 1)))];
                    ++count;
                  }
              d = std::lerp(
                  d, s.tool==TerrainTool::Sharpen?2*d-sum/float(count):sum/float(count),
                  std::clamp(terrainBrushWeight(s,distance/s.radius) * s.strength, 0.f,
                             1.f));
            } else {
              float sphere = distance - s.radius;
              float changed = s.tool == TerrainTool::Dig ? std::max(d, -sphere)
                                                         : std::min(d, sphere);
              d = std::lerp(d, changed, std::clamp(s.strength*terrainBrushWeight(s,distance/s.radius),0.f,1.f));
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
  TerrainMesh mesh;
  mesh.samples = u32(heights.size() + field.size());
  bool budget = false;
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
        // Smooth automatic strata, evaluated per vertex so chunk boundaries
        // have identical weights. Painting is local in 3D for cave surfaces.
        float rock=fade(std::clamp((1-std::abs(nm.y)-.12f)/.5f,0.f,1.f));
        float soil=(1-rock)*(.10f+.25f*noise(p.x*.13f,p.z*.13f,r.seed+73));
        v.weights[0]=1-rock-soil;v.weights[1]=rock;v.weights[2]=soil;v.weights[3]=0;
        if(fixedLayer<4) {std::fill_n(v.weights,4,0);v.weights[fixedLayer]=1;}
        else for(const auto &brush:strokes) {
          if(brush.tool!=TerrainTool::Paint && brush.tool!=TerrainTool::Path && brush.tool!=TerrainTool::Restore)continue;
          const float d=terrainBrushDistance(brush,p.x-brush.x,p.y-brush.y,p.z-brush.z,ny>1 && brush.tool!=TerrainTool::Path);
          const float w=std::clamp(terrainBrushWeight(brush,d)*brush.strength,0.f,1.f);
          if(w<=0)continue;
          const float natural[]{1-rock-soil,rock,soil,0};
          for(u32 l=0;l<4;++l)v.weights[l]=std::lerp(v.weights[l],brush.tool==TerrainTool::Restore?natural[l]:(l==brush.layer?1.f:0.f),w);
        }
        v.uv[0]=p.x/r.textureScale;v.uv[1]=p.z/r.textureScale;
        v.uv1[0]=p.y/r.textureScale;v.uv1[1]=r.macroVariation;
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
bool writeTerrainGlb(const TerrainRecipe &r, const TerrainMesh &mesh,
                     const std::array<std::vector<u8>, 4> &textures,
                     std::vector<u8> &out, std::string &error) {
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
    if(r.blendedSurface && std::all_of(textures.begin(),textures.end(),[](const auto &t){return !t.empty();}))
      materials << ",\"extras\":{\"astraTerrainSurface\":1,\"grassTexture\":{\"index\":0},\"rockTexture\":{\"index\":1},\"soilTexture\":{\"index\":2},\"pathTexture\":{\"index\":3}}";
    materials << "}";
  }
  for (usize i = 0; i < mesh.parts.size(); ++i) {
    const auto &p = mesh.parts[i];
    if (p.vertices.empty() || p.layer > 3) {
      error = "Parte de terreno inválida.";
      return false;
    }
    u32 v = view(p.vertices.data(), p.vertices.size() * sizeof(TerrainVertex),
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
                << ",\"componentType\":5126,\"count\":" << p.vertices.size()
                << ",\"type\":\"" << (attr == 4 ? "VEC4" : attr>=2 ? "VEC2" : "VEC3") << '"';
      if (attr == 0)
        accessors << ",\"min\":[" << lo[0] << ',' << lo[1] << ',' << lo[2]
                  << "],\"max\":[" << hi[0] << ',' << hi[1] << ',' << hi[2]
                  << ']';
      accessors << '}';
    }
    if (i) {
      nodes << ',';
      meshes << ',';
    }
    nodes << "{\"name\":\"Chunk_" << p.x << '_' << p.y << '_' << p.z << '_'
          << p.layer << "\",\"mesh\":" << i << '}';
    meshes << "{\"primitives\":[{\"attributes\":{\"POSITION\":" << first
           << ",\"NORMAL\":" << first + 1 << ",\"TEXCOORD_0\":" << first + 2
           << ",\"TEXCOORD_1\":" << first+3 << ",\"COLOR_0\":" << first+4
           << "},\"material\":" << p.layer << "}]}";
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
            "10497,\"wrapT\":10497}]";
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
