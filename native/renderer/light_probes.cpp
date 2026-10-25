#include "renderer/light_probes.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <tuple>

namespace ae::renderer {
namespace {
constexpr double Pi = 3.14159265358979323846;

struct Vec3d {
  double x = 0, y = 0, z = 0;
};
Vec3d sub(const Vec3d &a, const Vec3d &b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
double dot(const Vec3d &a, const Vec3d &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3d cross(const Vec3d &a, const Vec3d &b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
double orient(const Vec3d &a, const Vec3d &b, const Vec3d &c, const Vec3d &d) {
  return dot(sub(b, a), cross(sub(c, a), sub(d, a)));
}

// Esfera circunscrita de um tetraedro (centro e raio ao quadrado).
bool circumsphere(const Vec3d &a, const Vec3d &b, const Vec3d &c, const Vec3d &d, Vec3d &center, double &radius2) {
  const Vec3d ba = sub(b, a), ca = sub(c, a), da = sub(d, a);
  const double det = 2.0 * dot(ba, cross(ca, da));
  if (std::abs(det) < 1e-300) return false;
  const double lb = dot(ba, ba), lc = dot(ca, ca), ld = dot(da, da);
  const Vec3d t1 = cross(ca, da), t2 = cross(da, ba), t3 = cross(ba, ca);
  const Vec3d offset{(lb * t1.x + lc * t2.x + ld * t3.x) / det, (lb * t1.y + lc * t2.y + ld * t3.y) / det,
                     (lb * t1.z + lc * t2.z + ld * t3.z) / det};
  center = {a.x + offset.x, a.y + offset.y, a.z + offset.z};
  radius2 = dot(offset, offset);
  return std::isfinite(radius2);
}

// Coordenadas baricêntricas de p no tetraedro (a,b,c,d), em double.
bool barycentric(const Vec3d &a, const Vec3d &b, const Vec3d &c, const Vec3d &d, const Vec3d &p, double out[4]) {
  const double v = orient(a, b, c, d);
  if (std::abs(v) < 1e-300) return false;
  out[0] = orient(p, b, c, d) / v;
  out[1] = orient(a, p, c, d) / v;
  out[2] = orient(a, b, p, d) / v;
  out[3] = orient(a, b, c, p) / v;
  return true;
}

// Face de um tetraedro identificada pelos três vértices em ordem crescente.
using FaceKey = std::array<u32, 3>;
FaceKey faceKey(u32 a, u32 b, u32 c) {
  FaceKey key{a, b, c};
  std::sort(key.begin(), key.end());
  return key;
}
} // namespace

void shL2Basis(const float d[3], float out[9]) noexcept {
  const float x = d[0], y = d[1], z = d[2];
  out[0] = 0.2820947918f;
  out[1] = 0.4886025119f * y;
  out[2] = 0.4886025119f * z;
  out[3] = 0.4886025119f * x;
  out[4] = 1.0925484306f * x * y;
  out[5] = 1.0925484306f * y * z;
  out[6] = 0.3153915653f * (3.0f * y * y - 1.0f);
  out[7] = 1.0925484306f * x * z;
  out[8] = 0.5462742153f * (x * x - z * z);
}

void evaluateShIrradiance(const ShL2 &sh, const float direction[3], float out[3]) noexcept {
  float basis[9];
  shL2Basis(direction, basis);
  for (u32 k = 0; k < 3; ++k) {
    float sum = 0;
    for (u32 i = 0; i < 9; ++i) sum += sh.c[i][k] * basis[i];
    out[k] = std::max(sum, 0.0f);
  }
}

void cubeTexelDirection(u32 face, float u, float v, float out[3]) noexcept {
  // Tabela 16 da especificação Vulkan: sc, tc e eixo maior por face.
  const float s = 2.0f * u - 1.0f, t = 2.0f * v - 1.0f;
  switch (face) {
  case 0: out[0] = 1; out[1] = -t; out[2] = -s; break;  // +X
  case 1: out[0] = -1; out[1] = -t; out[2] = s; break;  // -X
  case 2: out[0] = s; out[1] = 1; out[2] = t; break;    // +Y
  case 3: out[0] = s; out[1] = -1; out[2] = -t; break;  // -Y
  case 4: out[0] = s; out[1] = -t; out[2] = 1; break;   // +Z
  default: out[0] = -s; out[1] = -t; out[2] = -1; break; // -Z
  }
}

bool projectCubeToIrradiance(const CubeFaces &faces, ShL2 &out) noexcept {
  out = {};
  if (!faces.valid()) return false;
  const u32 n = faces.size;
  double accum[9][3]{};
  double totalWeight = 0;
  for (u32 face = 0; face < 6; ++face)
    for (u32 y = 0; y < n; ++y)
      for (u32 x = 0; x < n; ++x) {
        const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(n);
        const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(n);
        float d[3];
        cubeTexelDirection(face, u, v, d);
        const float s = 2.0f * u - 1.0f, t = 2.0f * v - 1.0f;
        // Ângulo sólido do texel: área (2/n)² projetada na esfera unitária.
        const double weight = (4.0 / (static_cast<double>(n) * n)) / std::pow(1.0 + s * s + t * t, 1.5);
        const float length = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        for (float &c : d) c /= length;
        float basis[9];
        shL2Basis(d, basis);
        const float *rgb = &faces.rgb[((static_cast<usize>(face) * n + y) * n + x) * 3];
        for (u32 k = 0; k < 3; ++k) {
          const float radiance = std::isfinite(rgb[k]) ? std::max(rgb[k], 0.0f) : 0.0f;
          for (u32 i = 0; i < 9; ++i) accum[i][k] += static_cast<double>(radiance) * basis[i] * weight;
        }
        totalWeight += weight;
      }
  if (!(totalWeight > 0)) return false;
  // A soma discreta dos ângulos sólidos difere de 4π por O(1/n²); normalizar
  // tira esse viés para que um céu constante dê exatamente π·L de irradiância.
  const double normalize = 4.0 * Pi / totalWeight;
  constexpr double band[3]{Pi, 2.0 * Pi / 3.0, Pi / 4.0};
  for (u32 i = 0; i < 9; ++i) {
    const u32 l = i == 0 ? 0 : (i < 4 ? 1 : 2);
    for (u32 k = 0; k < 3; ++k) out.c[i][k] = static_cast<float>(accum[i][k] * normalize * band[l]);
  }
  return true;
}

void deringShL2(ShL2 &sh) noexcept {
  // Janela de Hanning de largura 4: banda 1 a 85%, banda 2 a 50%.
  constexpr float window[3]{1.0f, 0.8535534f, 0.5f};
  for (u32 i = 1; i < 9; ++i) {
    const float w = window[i < 4 ? 1 : 2];
    for (auto &channel : sh.c[i]) channel *= w;
  }
  for (auto &channel : sh.c[0]) channel = std::max(channel, 0.0f);
  // A janela sozinha não garante irradiância não negativa com uma fonte muito
  // forte. Procura em 256 direções (espiral de Fibonacci: o anel negativo de
  // uma fonte pequena fica a ~145° dela) e reduz as bandas 1 e 2 até não haver
  // lobo negativo — o efeito visível que o "Remove Ringing" da Unity resolve.
  const auto minimum = [&](const ShL2 &value) {
    float lowest = 1e30f;
    for (u32 n = 0; n < 256; ++n) {
      const float z = 1 - 2 * (static_cast<float>(n) + .5f) / 256, r = std::sqrt(std::max(0.f, 1 - z * z));
      const float phi = 2.39996323f * static_cast<float>(n);
      const float d[3]{r * std::cos(phi), z, r * std::sin(phi)};
      float basis[9];
      shL2Basis(d, basis);
      for (u32 k = 0; k < 3; ++k) {
        float sum = 0;
        for (u32 i = 0; i < 9; ++i) sum += value.c[i][k] * basis[i];
        lowest = std::min(lowest, sum);
      }
    }
    return lowest;
  };
  if (minimum(sh) >= 0) return;
  float lo = 0, hi = 1;
  for (u32 step = 0; step < 16; ++step) {
    const float mid = (lo + hi) * 0.5f;
    ShL2 trial = sh;
    for (u32 i = 1; i < 9; ++i) for (auto &channel : trial.c[i]) channel *= mid;
    (minimum(trial) >= 0 ? lo : hi) = mid;
  }
  for (u32 i = 1; i < 9; ++i) for (auto &channel : sh.c[i]) channel *= lo;
}

bool LightProbeSet::tetrahedralize() {
  tetrahedra.clear();
  const usize count = positions.size();
  // Vazio ou menos de 4 sondas é válido: sem tetraedro, a busca usa a mais próxima.
  if (count < 4 || count > MaximumLightProbes || sh.size() != count) return sh.size() == count && count <= MaximumLightProbes;
  // Perturbação determinística minúscula: grades regulares (o cubo de 8 sondas
  // que nasce do menu) são cosféricas e deixariam o teste da esfera ambíguo.
  double minimum[3]{1e300, 1e300, 1e300}, maximum[3]{-1e300, -1e300, -1e300};
  for (const auto &p : positions)
    for (u32 k = 0; k < 3; ++k) {
      minimum[k] = std::min(minimum[k], static_cast<double>(p[k]));
      maximum[k] = std::max(maximum[k], static_cast<double>(p[k]));
    }
  const double extent = std::max({maximum[0] - minimum[0], maximum[1] - minimum[1], maximum[2] - minimum[2], 1e-3});
  std::vector<Vec3d> points(count + 4);
  for (usize i = 0; i < count; ++i) {
    u32 hash = static_cast<u32>(i) * 2654435761u + 0x9e3779b9u;
    double jitter[3];
    for (double &j : jitter) {
      hash ^= hash << 13; hash ^= hash >> 17; hash ^= hash << 5;
      j = (static_cast<double>(hash & 0xffffu) / 65535.0 - 0.5) * extent * 2e-6;
    }
    points[i] = {positions[i][0] + jitter[0], positions[i][1] + jitter[1], positions[i][2] + jitter[2]};
  }
  // Super-tetraedro muito maior que o conjunto.
  const Vec3d c{(minimum[0] + maximum[0]) * 0.5, (minimum[1] + maximum[1]) * 0.5, (minimum[2] + maximum[2]) * 0.5};
  const double r = extent * 1000.0;
  points[count + 0] = {c.x - r, c.y - r, c.z - r};
  points[count + 1] = {c.x + 3 * r, c.y - r, c.z - r};
  points[count + 2] = {c.x - r, c.y + 3 * r, c.z - r};
  points[count + 3] = {c.x - r, c.y - r, c.z + 3 * r};

  struct Cell {
    std::array<u32, 4> v;
    Vec3d center;
    double radius2 = 0;
    bool alive = true;
  };
  std::vector<Cell> cells;
  auto addCell = [&](u32 a, u32 b, u32 cc, u32 d) {
    if (orient(points[a], points[b], points[cc], points[d]) < 0) std::swap(a, b);
    Cell cell{{a, b, cc, d}, {}, 0, true};
    if (!circumsphere(points[a], points[b], points[cc], points[d], cell.center, cell.radius2)) return false;
    cells.push_back(cell);
    return true;
  };
  addCell(static_cast<u32>(count), static_cast<u32>(count + 1), static_cast<u32>(count + 2), static_cast<u32>(count + 3));
  for (u32 i = 0; i < count; ++i) {
    const Vec3d &p = points[i];
    std::map<FaceKey, u32> boundary; // face -> ocorrências entre as células removidas
    std::vector<FaceKey> order;
    for (auto &cell : cells) {
      if (!cell.alive) continue;
      const Vec3d d = sub(p, cell.center);
      if (dot(d, d) >= cell.radius2) continue;
      cell.alive = false;
      constexpr u32 faces[4][3]{{1, 2, 3}, {0, 2, 3}, {0, 1, 3}, {0, 1, 2}};
      for (const auto &f : faces) {
        const auto key = faceKey(cell.v[f[0]], cell.v[f[1]], cell.v[f[2]]);
        if (boundary[key]++ == 0) order.push_back(key);
      }
    }
    for (const auto &key : order)
      if (boundary[key] == 1 && std::abs(orient(points[key[0]], points[key[1]], points[key[2]], p)) > 1e-18 * extent * extent * extent)
        addCell(key[0], key[1], key[2], i);
    // Compacta de vez em quando para a varredura não crescer sem limite.
    if (cells.size() > 4096) cells.erase(std::remove_if(cells.begin(), cells.end(), [](const Cell &cc) { return !cc.alive; }), cells.end());
  }
  // O volume é medido nas posições REAIS: quatro cantos de uma mesma face de um
  // cubo de sondas formam, com a perturbação, um tetraedro achatado que tem
  // volume na conta perturbada e zero na busca. Esse tetraedro não serve.
  const double minimumVolume = 1e-7 * extent * extent * extent;
  const auto real = [&](u32 index) { return Vec3d{positions[index][0], positions[index][1], positions[index][2]}; };
  for (const auto &cell : cells) {
    if (!cell.alive) continue;
    if (cell.v[0] >= count || cell.v[1] >= count || cell.v[2] >= count || cell.v[3] >= count) continue;
    if (std::abs(orient(real(cell.v[0]), real(cell.v[1]), real(cell.v[2]), real(cell.v[3]))) < minimumVolume) continue;
    ProbeTetrahedron t;
    t.vertex = cell.v;
    tetrahedra.push_back(t);
  }
  // Vizinhança: duas células que compartilham uma face.
  std::map<FaceKey, std::pair<i32, u32>> open;
  for (u32 index = 0; index < tetrahedra.size(); ++index) {
    auto &t = tetrahedra[index];
    for (u32 f = 0; f < 4; ++f) {
      const auto key = faceKey(t.vertex[(f + 1) % 4], t.vertex[(f + 2) % 4], t.vertex[(f + 3) % 4]);
      const auto found = open.find(key);
      if (found == open.end()) { open.emplace(key, std::make_pair(static_cast<i32>(index), f)); continue; }
      t.neighbor[f] = found->second.first;
      tetrahedra[static_cast<usize>(found->second.first)].neighbor[found->second.second] = static_cast<i32>(index);
      open.erase(found);
    }
  }
  return true;
}

ProbeInterpolation locateProbes(const LightProbeSet &set, const float point[3], i32 hint) noexcept {
  ProbeInterpolation result;
  if (set.positions.empty()) return result;
  const Vec3d p{point[0], point[1], point[2]};
  if (set.tetrahedra.empty()) {
    // Sem volume: a sonda mais próxima, como a Unity faz com uma sonda só.
    double best = 1e300;
    for (u32 i = 0; i < set.positions.size(); ++i) {
      const Vec3d q{set.positions[i][0], set.positions[i][1], set.positions[i][2]};
      const Vec3d d = sub(p, q);
      if (dot(d, d) < best) { best = dot(d, d); result.vertex[0] = i; }
    }
    result.weight[0] = 1;
    result.count = 1;
    return result;
  }
  const auto vertex = [&](u32 index) {
    const auto &v = set.positions[index];
    return Vec3d{v[0], v[1], v[2]};
  };
  i32 current = hint >= 0 && static_cast<usize>(hint) < set.tetrahedra.size() ? hint : 0;
  double lambda[4]{};
  i32 lastValid = -1;
  double lastLambda[4]{};
  const usize limit = std::min<usize>(set.tetrahedra.size() + 4, 4096);
  for (usize step = 0; step < limit; ++step) {
    const auto &t = set.tetrahedra[static_cast<usize>(current)];
    if (!barycentric(vertex(t.vertex[0]), vertex(t.vertex[1]), vertex(t.vertex[2]), vertex(t.vertex[3]), p, lambda)) break;
    lastValid = current;
    std::copy(lambda, lambda + 4, lastLambda);
    u32 worst = 0;
    for (u32 k = 1; k < 4; ++k) if (lambda[k] < lambda[worst]) worst = k;
    if (lambda[worst] >= -1e-7) {
      result.inside = true;
      break;
    }
    const i32 next = t.neighbor[worst];
    if (next < 0) break; // saiu pelo casco
    current = next;
  }
  if (lastValid < 0) return result;
  if (!result.inside) {
    // A caminhada pode parar numa face de borda que não é a mais próxima;
    // confirma contra todos os tetraedros de borda pelo menor desvio negativo.
    double bestScore = 0;
    for (u32 k = 0; k < 4; ++k) bestScore += std::min(lastLambda[k], 0.0);
    for (u32 index = 0; index < set.tetrahedra.size(); ++index) {
      const auto &t = set.tetrahedra[index];
      if (t.neighbor[0] >= 0 && t.neighbor[1] >= 0 && t.neighbor[2] >= 0 && t.neighbor[3] >= 0) continue;
      double l[4];
      if (!barycentric(vertex(t.vertex[0]), vertex(t.vertex[1]), vertex(t.vertex[2]), vertex(t.vertex[3]), p, l)) continue;
      double score = 0;
      for (u32 k = 0; k < 4; ++k) score += std::min(l[k], 0.0);
      if (score > bestScore) { bestScore = score; lastValid = static_cast<i32>(index); std::copy(l, l + 4, lastLambda); }
    }
  }
  const auto &t = set.tetrahedra[static_cast<usize>(lastValid)];
  double sum = 0;
  for (u32 k = 0; k < 4; ++k) {
    lastLambda[k] = std::clamp(lastLambda[k], 0.0, 1.0);
    sum += lastLambda[k];
  }
  if (!(sum > 0)) { lastLambda[0] = 1; sum = 1; }
  result.tetrahedron = lastValid;
  result.vertex = t.vertex;
  for (u32 k = 0; k < 4; ++k) result.weight[k] = static_cast<float>(lastLambda[k] / sum);
  result.count = 4;
  return result;
}

float evaluateProbeSky(const ProbeSkyVisibility &sky, const float d[3]) noexcept {
  // Mesma base de shL2Basis nas quatro primeiras funções: (0,0), y, z, x.
  return std::max(0.0f, sky[0] * 0.2820947918f + 0.4886025119f * (sky[1] * d[1] + sky[2] * d[2] + sky[3] * d[0]));
}

ProbeSkyVisibility interpolateProbeSky(const LightProbeSet &set, const ProbeInterpolation &where) noexcept {
  ProbeSkyVisibility out{};
  for (u32 k = 0; k < where.count; ++k)
    if (where.vertex[k] < set.skyVisibility.size())
      for (u32 i = 0; i < 4; ++i) out[i] += set.skyVisibility[where.vertex[k]][i] * where.weight[k];
  return out;
}

ShL2 interpolateProbes(const LightProbeSet &set, const ProbeInterpolation &where) noexcept {
  ShL2 out;
  for (u32 k = 0; k < where.count; ++k)
    if (where.vertex[k] < set.sh.size()) out.add(set.sh[where.vertex[k]], where.weight[k]);
  return out;
}

u64 lightProbePositionsHash(std::span<const std::array<float, 3>> positions, std::span<const u64> ids) noexcept {
  u64 key = 1469598103934665603ull;
  const auto mix = [&](u64 v) { key = (key ^ v) * 1099511628211ull; };
  mix(positions.size());
  for (usize i = 0; i < positions.size(); ++i) {
    if (i < ids.size()) mix(ids[i]);
    // Milímetro: arredondar evita que ruído de float marque o bake como velho.
    for (float v : positions[i]) mix(static_cast<u64>(static_cast<i64>(std::lround(static_cast<double>(v) * 1000.0))));
  }
  return key;
}

bool LightProbeBakeData::valid() const noexcept {
  if (positions.size() != sh.size() || positions.size() != pointIds.size() || positions.size() > MaximumLightProbes) return false;
  for (const auto &p : positions) for (float v : p) if (!std::isfinite(v)) return false;
  for (const auto &s : sh) for (const auto &c : s.c) for (float v : c) if (!std::isfinite(v)) return false;
  if (!skyVisibility.empty() && skyVisibility.size() != positions.size()) return false;
  for (const auto &s : skyVisibility) for (float v : s) if (!std::isfinite(v)) return false;
  return true;
}

std::vector<u8> LightProbeBakeData::serialize() const {
  std::vector<u8> out;
  const auto put32 = [&](u32 v) { for (u32 i = 0; i < 4; ++i) out.push_back(static_cast<u8>(v >> (i * 8))); };
  const auto put64 = [&](u64 v) { put32(static_cast<u32>(v)); put32(static_cast<u32>(v >> 32)); };
  const auto putFloat = [&](float v) { u32 bits; std::memcpy(&bits, &v, 4); put32(bits); };
  out.insert(out.end(), {'A', 'E', 'L', 'P'});
  put32(Version);
  put64(guidHigh); put64(guidLow); put64(sourceHash);
  putFloat(bakeSeconds);
  const bool sky = !skyVisibility.empty();
  put32(sky ? 1u : 0u); // v2: bit 0 visibilidade do céu por ponto
  put32(static_cast<u32>(positions.size()));
  for (usize i = 0; i < positions.size(); ++i) {
    put64(pointIds[i]);
    for (float v : positions[i]) putFloat(v);
    for (const auto &c : sh[i].c) for (float v : c) putFloat(v);
    if (sky) for (float v : skyVisibility[i]) putFloat(v);
  }
  return out;
}

bool LightProbeBakeData::deserialize(std::span<const u8> bytes, LightProbeBakeData &out, std::string *error) {
  const auto fail = [&](const char *message) { if (error) *error = message; return false; };
  if (bytes.size() < 44 || bytes.size() > MaximumBytes || std::memcmp(bytes.data(), "AELP", 4) != 0)
    return fail("arquivo de sondas de luz inválido");
  usize at = 4;
  const auto get32 = [&]() { u32 v = 0; for (u32 i = 0; i < 4; ++i) v |= static_cast<u32>(bytes[at + i]) << (i * 8); at += 4; return v; };
  const auto get64 = [&]() { const u64 lo = get32(); return lo | (static_cast<u64>(get32()) << 32); };
  const auto getFloat = [&]() { const u32 bits = get32(); float v; std::memcpy(&v, &bits, 4); return v; };
  const u32 version = get32();
  if (version != 1 && version != Version) return fail("versão de sondas de luz desconhecida");
  LightProbeBakeData value;
  value.guidHigh = get64(); value.guidLow = get64(); value.sourceHash = get64();
  value.bakeSeconds = getFloat();
  const bool sky = version >= 2 && bytes.size() - at >= 8 && (get32() & 1u) != 0;
  const u32 count = get32();
  const usize record = 8 + 12 + 27 * 4 + (sky ? 16 : 0);
  if (count > MaximumLightProbes || bytes.size() - at != static_cast<usize>(count) * record) return fail("sondas de luz truncadas");
  value.pointIds.resize(count); value.positions.resize(count); value.sh.resize(count);
  if (sky) value.skyVisibility.resize(count);
  for (u32 i = 0; i < count; ++i) {
    value.pointIds[i] = get64();
    for (float &v : value.positions[i]) v = getFloat();
    for (auto &c : value.sh[i].c) for (float &v : c) v = getFloat();
    if (sky) for (float &v : value.skyVisibility[i]) v = getFloat();
  }
  if (!value.valid()) return fail("valores não finitos nas sondas de luz");
  out = std::move(value);
  return true;
}

} // namespace ae::renderer
