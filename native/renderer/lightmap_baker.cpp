#include "renderer/lightmap_baker.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <functional>
#include <thread>

#if defined(__linux__) || defined(__ANDROID__)
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace ae::renderer {

const char *lightmapBakeStageName(LightmapBakeStage stage) noexcept {
  switch (stage) {
  case LightmapBakeStage::Preparing: return "Preparando a cena";
  case LightmapBakeStage::Bvh: return "Montando a BVH";
  case LightmapBakeStage::Layout: return "Distribuindo os lightmaps";
  case LightmapBakeStage::Direct: return "Luz direta";
  case LightmapBakeStage::Indirect: return "Luz indireta e céu";
  case LightmapBakeStage::Filtering: return "Denoise e dilatação";
  case LightmapBakeStage::Encoding: return "Comprimindo";
  case LightmapBakeStage::Done: return "Concluído";
  }
  return "";
}

namespace {
constexpr float Pi = 3.14159265358979f;

struct V3 {
  float x = 0, y = 0, z = 0;
  V3() = default;
  constexpr V3(float a, float b, float c) : x(a), y(b), z(c) {}
  explicit V3(const float *p) : x(p[0]), y(p[1]), z(p[2]) {}
  float operator[](u32 i) const { return i == 0 ? x : i == 1 ? y : z; }
  float &operator[](u32 i) { return i == 0 ? x : i == 1 ? y : z; }
};
inline V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V3 operator*(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline V3 mul(V3 a, V3 b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float length(V3 a) { return std::sqrt(dot(a, a)); }
inline V3 normalize(V3 a) { const float l = length(a); return l > 1e-20f ? a * (1.0f / l) : V3{0, 1, 0}; }
inline V3 vmin(V3 a, V3 b) { return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)}; }
inline V3 vmax(V3 a, V3 b) { return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)}; }
inline float luminance(V3 c) { return .2126f * c.x + .7152f * c.y + .0722f * c.z; }

// Base ortonormal em volta da normal (Duff et al. 2017).
inline void basis(V3 n, V3 &t, V3 &b) {
  const float sign = std::copysign(1.0f, n.z);
  const float a = -1.0f / (sign + n.z);
  const float c = n.x * n.y * a;
  t = {1.0f + sign * n.x * n.x * a, sign * c, -sign * n.x};
  b = {c, sign + n.y * n.y * a, -n.y};
}
inline float radicalInverse(u32 bits) {
  bits = (bits << 16u) | (bits >> 16u);
  bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
  bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
  bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
  bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
  return static_cast<float>(bits) * 2.3283064365386963e-10f;
}
inline u32 hash32(u32 x) {
  x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
  return x;
}
inline float hashFloat(u32 x) { return static_cast<float>(hash32(x) >> 8) * (1.0f / 16777216.0f); }
// Hemisfério em cosseno, Hammersley com rotação de Cranley-Patterson por texel:
// estratificado e sem padrão repetido entre texels vizinhos.
inline V3 cosineSample(V3 n, u32 index, u32 count, float r0, float r1) {
  float u = (static_cast<float>(index) + .5f) / static_cast<float>(count) + r0;
  float v = radicalInverse(index) + r1;
  u -= std::floor(u); v -= std::floor(v);
  const float radius = std::sqrt(u), phi = 2.0f * Pi * v;
  V3 t, b;
  basis(n, t, b);
  const float z = std::sqrt(std::max(0.0f, 1.0f - u));
  return normalize(t * (radius * std::cos(phi)) + b * (radius * std::sin(phi)) + n * z);
}

// --- Pool de trabalhadores -------------------------------------------------
class Workers {
public:
  Workers(u32 count, bool lowPriority) : count_(std::max<u32>(count, 1)), lowPriority_(lowPriority) {}
  // fn(início, fim) em blocos de `chunk`; devolve falso se cancelado.
  bool parallelFor(u64 total, u64 chunk, const std::atomic<bool> &cancel, const std::function<void(u64, u64)> &fn,
                   const std::function<void(u64)> &onChunk = {}) {
    if (!total) return !cancel.load();
    std::atomic<u64> next{0}, done{0};
    auto body = [&]() {
#if defined(__linux__) || defined(__ANDROID__)
      if (lowPriority_) setpriority(PRIO_PROCESS, static_cast<id_t>(syscall(SYS_gettid)), 10);
#endif
      for (;;) {
        if (cancel.load(std::memory_order_relaxed)) return;
        const u64 begin = next.fetch_add(chunk);
        if (begin >= total) return;
        const u64 end = std::min(total, begin + chunk);
        fn(begin, end);
        const u64 finished = done.fetch_add(end - begin) + (end - begin);
        if (onChunk) onChunk(finished);
      }
    };
    std::vector<std::thread> threads;
    threads.reserve(count_ - 1);
    for (u32 i = 1; i < count_; ++i) threads.emplace_back(body);
    body();
    for (auto &thread : threads) thread.join();
    return !cancel.load();
  }
  u32 count() const { return count_; }
private:
  u32 count_;
  bool lowPriority_;
};

// --- Cena de mundo e BVH ---------------------------------------------------
struct WorldTriangle {
  V3 v0, e1, e2;
  u32 instance = 0, local = 0; // instância e triângulo local da malha
  u32 flags = 0;
};
// TriFlipped: o enrolamento contradiz a normal dos vértices (malhas como o
// Sponza misturam as duas convenções); a frente é a da normal, como no
// lightmapper da Unity, que usa a normal da malha para decidir a face.
constexpr u32 TriAlphaTest = 1, TriDoubleSided = 2, TriNoShadow = 4, TriMirrored = 8, TriFlipped = 16;

struct BvhNode {
  V3 lo, hi;
  u32 first = 0; // filho esquerdo (interior) ou primeiro triângulo (folha)
  u32 count = 0; // 0 interior
};

struct Hit {
  float t = 1e30f, u = 0, v = 0;
  u32 triangle = ~0u;
  bool back = false;
};

class BakeWorld {
public:
  const LightmapBakeScene &scene;
  std::vector<WorldTriangle> triangles;
  std::vector<BvhNode> nodes;
  // Por instância: normais de mundo (matriz normal) e início dos triângulos.
  std::vector<std::array<float, 9>> normalMatrix;
  std::vector<std::array<float, 12>> modelRows;
  // Escala negativa (espelho) inverte o enrolamento: a frente do triângulo de
  // mundo é o lado oposto ao do produto vetorial das arestas.
  std::vector<u8> mirrored;
  // Albedo linear com Albedo Boost, por textura, já em float.
  float albedoBoost = 1;

  explicit BakeWorld(const LightmapBakeScene &s) : scene(s) {}

  V3 transformPoint(u32 instance, V3 p) const {
    const auto &m = modelRows[instance];
    return {m[0] * p.x + m[1] * p.y + m[2] * p.z + m[3], m[4] * p.x + m[5] * p.y + m[6] * p.z + m[7],
            m[8] * p.x + m[9] * p.y + m[10] * p.z + m[11]};
  }
  V3 transformNormal(u32 instance, V3 n) const {
    const auto &m = normalMatrix[instance];
    return normalize({m[0] * n.x + m[1] * n.y + m[2] * n.z, m[3] * n.x + m[4] * n.y + m[5] * n.z,
                      m[6] * n.x + m[7] * n.y + m[8] * n.z});
  }

  void prepareInstances() {
    modelRows.resize(scene.instances.size());
    normalMatrix.resize(scene.instances.size());
    mirrored.assign(scene.instances.size(), 0);
    for (usize i = 0; i < scene.instances.size(); ++i) {
      const float *m = scene.instances[i].model; // coluna principal
      auto &r = modelRows[i];
      for (u32 row = 0; row < 3; ++row)
        for (u32 col = 0; col < 4; ++col) r[row * 4 + col] = m[col * 4 + row];
      // Inversa transposta da 3x3: a matriz dos cofatores dividida pelo
      // determinante (a inversa seria a transposta dela).
      const float a = r[0], b = r[1], c = r[2], d = r[4], e = r[5], f = r[6], g = r[8], h = r[9], k = r[10];
      const float A = e * k - f * h, B = -(d * k - f * g), C = d * h - e * g;
      const float D = -(b * k - c * h), E = a * k - c * g, F = -(a * h - b * g);
      const float G = b * f - c * e, H = -(a * f - c * d), K = a * e - b * d;
      const float det = a * A + b * B + c * C;
      mirrored[i] = det < 0 ? 1 : 0;
      const float s = std::fabs(det) > 1e-30f ? 1.0f / det : 1.0f;
      normalMatrix[i] = {A * s, B * s, C * s, D * s, E * s, F * s, G * s, H * s, K * s};
    }
  }

  void gatherTriangles() {
    for (u32 i = 0; i < scene.instances.size(); ++i) {
      const auto &instance = scene.instances[i];
      if (!instance.contributes || instance.mesh >= scene.meshes.size()) continue;
      const auto &mesh = scene.meshes[instance.mesh];
      const auto &material = instance.material < scene.materials.size() ? scene.materials[instance.material] : LightmapBakeMaterial{};
      const u32 vertexCount = static_cast<u32>(mesh.positions.size() / 3);
      for (u32 t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const u32 a = mesh.indices[t], b = mesh.indices[t + 1], c = mesh.indices[t + 2];
        if (a >= vertexCount || b >= vertexCount || c >= vertexCount) continue;
        const V3 p0 = transformPoint(i, V3(&mesh.positions[a * 3])), p1 = transformPoint(i, V3(&mesh.positions[b * 3])),
                 p2 = transformPoint(i, V3(&mesh.positions[c * 3]));
        WorldTriangle tri;
        tri.v0 = p0; tri.e1 = p1 - p0; tri.e2 = p2 - p0;
        if (length(cross(tri.e1, tri.e2)) < 1e-12f) continue;
        tri.instance = i; tri.local = t / 3;
        tri.flags = (material.alphaTest ? TriAlphaTest : 0) | (material.doubleSided ? TriDoubleSided : 0) |
                    (instance.castsShadows ? 0 : TriNoShadow) | (mirrored[i] ? TriMirrored : 0);
        if (mesh.normals.size() == mesh.positions.size()) {
          const V3 shading = transformNormal(i, V3(&mesh.normals[a * 3]) + V3(&mesh.normals[b * 3]) + V3(&mesh.normals[c * 3]));
          V3 face = cross(tri.e1, tri.e2);
          if (mirrored[i]) face = face * -1.0f;
          if (dot(face, shading) < 0) tri.flags |= TriFlipped;
        }
        triangles.push_back(tri);
      }
    }
  }

  // BVH binada por SAH (12 caixas), folhas de até 4 triângulos.
  void build() {
    nodes.clear();
    if (triangles.empty()) return;
    const usize count = triangles.size();
    std::vector<V3> centroid(count), lo(count), hi(count);
    for (usize i = 0; i < count; ++i) {
      const auto &t = triangles[i];
      const V3 a = t.v0, b = t.v0 + t.e1, c = t.v0 + t.e2;
      lo[i] = vmin(a, vmin(b, c)); hi[i] = vmax(a, vmax(b, c));
      centroid[i] = (lo[i] + hi[i]) * .5f;
    }
    std::vector<u32> order(count);
    for (u32 i = 0; i < count; ++i) order[i] = i;
    nodes.reserve(count * 2 / 3 + 8);
    nodes.push_back({});
    struct Task { u32 node, first, count; };
    std::vector<Task> stack{{0, 0, static_cast<u32>(count)}};
    constexpr u32 Bins = 12;
    while (!stack.empty()) {
      const Task task = stack.back();
      stack.pop_back();
      V3 boxLo{1e30f, 1e30f, 1e30f}, boxHi{-1e30f, -1e30f, -1e30f}, cLo = boxLo, cHi = boxHi;
      for (u32 i = task.first; i < task.first + task.count; ++i) {
        const u32 t = order[i];
        boxLo = vmin(boxLo, lo[t]); boxHi = vmax(boxHi, hi[t]);
        cLo = vmin(cLo, centroid[t]); cHi = vmax(cHi, centroid[t]);
      }
      nodes[task.node].lo = boxLo; nodes[task.node].hi = boxHi;
      if (task.count <= 4) { nodes[task.node].first = task.first; nodes[task.node].count = task.count; continue; }
      float bestCost = 1e30f;
      u32 bestAxis = 0, bestSplit = 0;
      for (u32 axis = 0; axis < 3; ++axis) {
        const float extent = cHi[axis] - cLo[axis];
        if (extent < 1e-12f) continue;
        u32 binCount[Bins]{};
        V3 binLo[Bins], binHi[Bins];
        for (u32 b = 0; b < Bins; ++b) { binLo[b] = {1e30f, 1e30f, 1e30f}; binHi[b] = {-1e30f, -1e30f, -1e30f}; }
        for (u32 i = task.first; i < task.first + task.count; ++i) {
          const u32 t = order[i];
          const u32 b = std::min(Bins - 1, static_cast<u32>((centroid[t][axis] - cLo[axis]) / extent * Bins));
          ++binCount[b]; binLo[b] = vmin(binLo[b], lo[t]); binHi[b] = vmax(binHi[b], hi[t]);
        }
        const auto area = [](V3 a, V3 b) { const V3 d = b - a; return d.x < 0 ? 0.0f : 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x); };
        float leftArea[Bins], rightArea[Bins];
        u32 leftCount[Bins], rightCount[Bins];
        V3 l0{1e30f, 1e30f, 1e30f}, l1{-1e30f, -1e30f, -1e30f};
        u32 n = 0;
        for (u32 b = 0; b < Bins; ++b) {
          n += binCount[b]; leftCount[b] = n;
          if (binCount[b]) { l0 = vmin(l0, binLo[b]); l1 = vmax(l1, binHi[b]); }
          leftArea[b] = n ? area(l0, l1) : 0;
        }
        V3 r0{1e30f, 1e30f, 1e30f}, r1{-1e30f, -1e30f, -1e30f};
        n = 0;
        for (u32 b = Bins; b-- > 0;) {
          n += binCount[b]; rightCount[b] = n;
          if (binCount[b]) { r0 = vmin(r0, binLo[b]); r1 = vmax(r1, binHi[b]); }
          rightArea[b] = n ? area(r0, r1) : 0;
        }
        for (u32 b = 0; b + 1 < Bins; ++b) {
          if (!leftCount[b] || !rightCount[b + 1]) continue;
          const float cost = leftArea[b] * leftCount[b] + rightArea[b + 1] * rightCount[b + 1];
          if (cost < bestCost) { bestCost = cost; bestAxis = axis; bestSplit = b; }
        }
      }
      u32 mid = task.first;
      if (bestCost < 1e30f) {
        const float extent = cHi[bestAxis] - cLo[bestAxis];
        auto *begin = order.data() + task.first, *end = begin + task.count;
        auto *split = std::partition(begin, end, [&](u32 t) {
          return std::min(Bins - 1, static_cast<u32>((centroid[t][bestAxis] - cLo[bestAxis]) / extent * Bins)) <= bestSplit;
        });
        mid = static_cast<u32>(split - order.data());
      }
      if (mid == task.first || mid == task.first + task.count) mid = task.first + task.count / 2;
      const u32 left = static_cast<u32>(nodes.size());
      nodes.push_back({});
      nodes.push_back({});
      nodes[task.node].first = left;
      nodes[task.node].count = 0;
      stack.push_back({left, task.first, mid - task.first});
      stack.push_back({left + 1, mid, task.first + task.count - mid});
    }
    std::vector<WorldTriangle> sorted(count);
    for (usize i = 0; i < count; ++i) sorted[i] = triangles[order[i]];
    triangles = std::move(sorted);
  }

  // Albedo (RGB linear) e alfa no triângulo, pela UV0.
  void surface(u32 triangle, float u, float v, V3 &albedo, float &alpha) const {
    const auto &tri = triangles[triangle];
    const auto &instance = scene.instances[tri.instance];
    const auto &mesh = scene.meshes[instance.mesh];
    static const LightmapBakeMaterial fallback{};
    const auto &material = instance.material < scene.materials.size() ? scene.materials[instance.material] : fallback;
    albedo = {material.baseColor[0], material.baseColor[1], material.baseColor[2]};
    alpha = material.baseColor[3];
    if (material.baseTexture >= 0 && static_cast<usize>(material.baseTexture) < scene.textures.size() &&
        mesh.uv0.size() >= mesh.positions.size() / 3 * 2) {
      const auto &texture = scene.textures[material.baseTexture];
      if (texture.width && texture.height && texture.rgba.size() >= usize(texture.width) * texture.height * 4) {
        const u32 a = mesh.indices[tri.local * 3], b = mesh.indices[tri.local * 3 + 1], c = mesh.indices[tri.local * 3 + 2];
        const float w = 1.0f - u - v;
        const float s = mesh.uv0[a * 2] * w + mesh.uv0[b * 2] * u + mesh.uv0[c * 2] * v;
        const float t = mesh.uv0[a * 2 + 1] * w + mesh.uv0[b * 2 + 1] * u + mesh.uv0[c * 2 + 1] * v;
        const float *m = material.uvTransform;
        float x = m[0] * s + m[1] * t + m[2], y = m[3] * s + m[4] * t + m[5];
        x -= std::floor(x); y -= std::floor(y);
        const u32 px = std::min(texture.width - 1, static_cast<u32>(x * texture.width));
        const u32 py = std::min(texture.height - 1, static_cast<u32>(y * texture.height));
        const u8 *texel = texture.rgba.data() + (usize(py) * texture.width + px) * 4;
        V3 color{texel[0] / 255.0f, texel[1] / 255.0f, texel[2] / 255.0f};
        if (texture.srgb)
          for (u32 k = 0; k < 3; ++k) color[k] = color[k] <= .04045f ? color[k] / 12.92f : std::pow((color[k] + .055f) / 1.055f, 2.4f);
        albedo = mul(albedo, color);
        alpha *= texel[3] / 255.0f;
      }
    }
    if (albedoBoost > 1.0f)
      for (u32 k = 0; k < 3; ++k) albedo[k] = std::pow(std::max(albedo[k], 0.0f), 1.0f / albedoBoost);
    // Nenhuma superfície real devolve mais que ~95%: acima disso os rebotes
    // não convergem e o resultado depende do número de passes.
    for (u32 k = 0; k < 3; ++k) albedo[k] = std::clamp(albedo[k], 0.0f, .95f);
  }

  bool alphaRejects(u32 triangle, float u, float v) const {
    const auto &tri = triangles[triangle];
    if (!(tri.flags & TriAlphaTest)) return false;
    const auto &instance = scene.instances[tri.instance];
    const float cutoff = instance.material < scene.materials.size() ? scene.materials[instance.material].alphaCutoff : .5f;
    V3 albedo;
    float alpha = 1;
    surface(triangle, u, v, albedo, alpha);
    return alpha < cutoff;
  }

  // Möller–Trumbore; `shadow` aceita qualquer acerto (e pula quem não projeta).
  bool trace(V3 origin, V3 direction, float maximum, bool shadow, Hit &hit) const {
    if (nodes.empty()) return false;
    const V3 inverse{1.0f / (std::fabs(direction.x) > 1e-20f ? direction.x : 1e-20f),
                     1.0f / (std::fabs(direction.y) > 1e-20f ? direction.y : 1e-20f),
                     1.0f / (std::fabs(direction.z) > 1e-20f ? direction.z : 1e-20f)};
    u32 stack[64];
    u32 top = 0;
    stack[top++] = 0;
    hit.t = maximum;
    bool found = false;
    while (top) {
      const BvhNode &node = nodes[stack[--top]];
      float t0 = 0, t1 = hit.t;
      bool miss = false;
      for (u32 axis = 0; axis < 3 && !miss; ++axis) {
        float a = (node.lo[axis] - origin[axis]) * inverse[axis], b = (node.hi[axis] - origin[axis]) * inverse[axis];
        if (a > b) std::swap(a, b);
        t0 = std::max(t0, a);
        t1 = std::min(t1, b);
        miss = t0 > t1;
      }
      if (miss) continue;
      if (node.count == 0) {
        if (top + 2 > 64) continue;
        stack[top++] = node.first;
        stack[top++] = node.first + 1;
        continue;
      }
      for (u32 i = node.first; i < node.first + node.count; ++i) {
        const auto &tri = triangles[i];
        if (shadow && (tri.flags & TriNoShadow)) continue;
        const V3 p = cross(direction, tri.e2);
        const float det = dot(tri.e1, p);
        if (std::fabs(det) < 1e-14f) continue;
        const float inv = 1.0f / det;
        const V3 s = origin - tri.v0;
        const float u = dot(s, p) * inv;
        if (u < 0 || u > 1) continue;
        const V3 q = cross(s, tri.e1);
        const float v = dot(direction, q) * inv;
        if (v < 0 || u + v > 1) continue;
        const float t = dot(tri.e2, q) * inv;
        if (t <= 0 || t >= hit.t) continue;
        if (alphaRejects(i, u, v)) continue;
        hit.t = t; hit.u = u; hit.v = v; hit.triangle = i;
        const bool inverted = ((tri.flags & TriMirrored) != 0) != ((tri.flags & TriFlipped) != 0);
        hit.back = ((det < 0) != inverted) && !(tri.flags & TriDoubleSided);
        found = true;
        if (shadow) return true;
      }
    }
    return found;
  }
};

// --- Texels ----------------------------------------------------------------
struct Rect { u32 instance = 0, page = 0, x = 0, y = 0, size = 0; };
struct Texel {
  V3 position, normal, geometric;
  u32 instance = 0;
  float footprint = 0; // lado do texel em metros
  u8 quality = 0;      // 2 centro dentro do triângulo, 1 conservador
  float distance = 1e30f;
  bool valid = true;
};
struct Page {
  u32 size = 0;
  std::vector<i32> record; // texel -> índice em `texels`, -1 vazio
  // Irradiância local (RGB) e do céu (escalar) do passe anterior, dilatadas,
  // para a busca de quem o raio acertou.
  std::vector<float> local, sky;
};

void lightRadiance(const LightmapBakeLight &light, V3 p, V3 &toLight, float &distance, V3 &radiance) {
  if (light.kind == 0) {
    toLight = normalize(V3(light.direction) * -1.0f);
    distance = 1e30f;
    radiance = V3(light.radiance);
    return;
  }
  const V3 delta = V3(light.position) - p;
  const float squared = std::max(dot(delta, delta), 1e-4f);
  distance = std::sqrt(squared);
  toLight = delta * (1.0f / distance);
  // Mesma janela de alcance e cone do fragmento (dirt_road_shading.glsl).
  const float ratio = dot(delta, delta) / std::max(light.range * light.range, 1e-4f);
  float window = std::clamp(1.0f - ratio * ratio, 0.0f, 1.0f);
  window *= window;
  float cone = std::clamp(dot(normalize(V3(light.direction)), toLight * -1.0f) * light.coneScale + light.coneOffset, 0.0f, 1.0f);
  cone *= cone;
  radiance = V3(light.radiance) * (window * cone / squared);
}

u64 mixHash(u64 key, u64 value) { return (key ^ value) * 1099511628211ull; }
u64 floatBits(float v) { u32 b; std::memcpy(&b, &v, 4); return b; }
} // namespace

u64 lightmapSceneHash(const LightmapBakeScene &scene, const LightmapBakeSettings &settings) noexcept {
  u64 key = 1469598103934665603ull;
  const auto mixF = [&](float v) { key = mixHash(key, floatBits(v)); };
  const auto mixU = [&](u64 v) { key = mixHash(key, v); };
  mixF(settings.texelsPerUnit); mixU(settings.padding); mixU(settings.maxSize); mixU(settings.directSamples);
  mixU(settings.indirectSamples); mixU(settings.environmentSamples); mixU(settings.bounces); mixU(settings.denoise);
  mixU(settings.compress); mixF(settings.indirectIntensity); mixF(settings.albedoBoost); mixU(settings.directionalMode);
  mixU(scene.meshes.size());
  for (const auto &mesh : scene.meshes) {
    mixU(mesh.positions.size()); mixU(mesh.indices.size()); mixU(mesh.uv1Valid);
    // Amostra da geometria: o bastante para notar reimportação, sem varrer tudo.
    const usize step = std::max<usize>(1, mesh.positions.size() / 64);
    for (usize i = 0; i < mesh.positions.size(); i += step) mixF(mesh.positions[i]);
    const usize uvStep = std::max<usize>(1, mesh.uv1.size() / 64);
    for (usize i = 0; i < mesh.uv1.size(); i += uvStep) mixF(mesh.uv1[i]);
  }
  for (const auto &m : scene.materials) {
    for (float v : m.baseColor) mixF(v);
    for (float v : m.emission) mixF(v);
    mixU(static_cast<u64>(m.baseTexture + 1)); mixU(m.alphaTest); mixU(m.doubleSided); mixF(m.alphaCutoff);
  }
  for (const auto &i : scene.instances) {
    mixU(i.objectId); mixU(i.slot); mixU(i.mesh); mixU(i.material);
    for (float v : i.model) mixF(std::round(v * 10000.0f));
    mixU(i.contributes); mixU(i.receives); mixU(i.castsShadows); mixF(i.scaleInLightmap);
  }
  for (const auto &l : scene.lights) {
    mixU(l.kind);
    for (float v : l.position) mixF(std::round(v * 1000.0f));
    for (float v : l.direction) mixF(std::round(v * 10000.0f));
    for (float v : l.radiance) mixF(v);
    mixF(l.range); mixF(l.coneScale); mixF(l.coneOffset); mixF(l.shadowRadius); mixU(l.bakeDirect); mixU(l.castsShadows);
  }
  return key;
}

bool lightmapDebugTrace(const LightmapBakeScene &scene, const LightmapBakeRay &ray, float &distance, u32 &instance) {
  BakeWorld world(scene);
  world.prepareInstances();
  world.gatherTriangles();
  world.build();
  Hit hit;
  if (!world.trace(V3(ray.origin), normalize(V3(ray.direction)), ray.maximum, false, hit)) return false;
  distance = hit.t;
  instance = world.triangles[hit.triangle].instance;
  return true;
}

struct LightmapTracer::Impl {
  BakeWorld world;
  explicit Impl(const LightmapBakeScene &scene) : world(scene) {
    world.prepareInstances();
    world.gatherTriangles();
    world.build();
  }
};
LightmapTracer::LightmapTracer(const LightmapBakeScene &scene) : impl_(std::make_unique<Impl>(scene)) {}
LightmapTracer::~LightmapTracer() = default;
LightmapTracer::Result LightmapTracer::trace(const float origin[3], const float direction[3], float maximum) const {
  Result result;
  Hit hit;
  if (!impl_->world.trace(V3(origin), normalize(V3(direction)), maximum, false, hit)) return result;
  result.hit = true;
  result.back = hit.back;
  result.distance = hit.t;
  const auto &world = impl_->world;
  const auto &tri = world.triangles[hit.triangle];
  result.instance = tri.instance;
  V3 n = normalize(cross(tri.e1, tri.e2));
  if (tri.flags & TriMirrored) n = n * -1.0f;
  if (tri.flags & TriFlipped) n = n * -1.0f;
  if (dot(n, V3(direction)) > 0) n = n * -1.0f; // a face voltada para o raio
  V3 albedo;
  float alpha;
  world.surface(hit.triangle, hit.u, hit.v, albedo, alpha);
  const auto &mesh = world.scene.meshes[world.scene.instances[tri.instance].mesh];
  if (mesh.uv1.size() >= mesh.positions.size() / 3 * 2) {
    const u32 a = mesh.indices[tri.local * 3], b = mesh.indices[tri.local * 3 + 1], c = mesh.indices[tri.local * 3 + 2];
    const float w = 1 - hit.u - hit.v;
    result.uv1[0] = mesh.uv1[a * 2] * w + mesh.uv1[b * 2] * hit.u + mesh.uv1[c * 2] * hit.v;
    result.uv1[1] = mesh.uv1[a * 2 + 1] * w + mesh.uv1[b * 2 + 1] * hit.u + mesh.uv1[c * 2 + 1] * hit.v;
  }
  for (u32 k = 0; k < 3; ++k) { result.normal[k] = n[k]; result.albedo[k] = albedo[k]; }
  return result;
}

bool bakeLightmaps(const LightmapBakeScene &scene, const LightmapBakeSettings &settings, const LightmapBakeOptions &options,
                   LightmapBakeProgress &progress, LightmapData &out, std::string &error) {
  const auto start = std::chrono::steady_clock::now();
  const auto setStage = [&](LightmapBakeStage stage, float fraction) {
    progress.stage.store(static_cast<u32>(stage));
    progress.fraction.store(fraction);
  };
  const auto cancelled = [&]() {
    if (!progress.cancel.load()) return false;
    error = "Bake cancelado";
    return true;
  };
  if (!settings.valid()) { error = "Configurações de lightmap inválidas"; return false; }
  setStage(LightmapBakeStage::Preparing, 0);
  const u32 threadCount = options.threads ? options.threads : std::max<u32>(1, std::thread::hardware_concurrency() > 1 ? std::thread::hardware_concurrency() - 1 : 1);
  Workers workers(threadCount, options.lowPriority);

  BakeWorld world(scene);
  world.albedoBoost = settings.albedoBoost;
  world.prepareInstances();
  setStage(LightmapBakeStage::Bvh, .01f);
  world.gatherTriangles();
  world.build();
  if (cancelled()) return false;
  LightmapStats stats;
  stats.triangles = static_cast<u32>(world.triangles.size());
  for (const auto &instance : scene.instances) stats.contributors += instance.contributes ? 1u : 0u;
  stats.lights = static_cast<u32>(scene.lights.size());
  // Desvio da origem do raio: proporcional ao tamanho da cena, com piso de 1 mm.
  V3 sceneLo{1e30f, 1e30f, 1e30f}, sceneHi{-1e30f, -1e30f, -1e30f};
  if (!world.nodes.empty()) { sceneLo = world.nodes[0].lo; sceneHi = world.nodes[0].hi; }
  const float sceneSize = world.nodes.empty() ? 1.0f : length(sceneHi - sceneLo);
  const float bias = std::max(1e-3f, sceneSize * 2e-5f);

  // --- Retângulos e páginas -----------------------------------------------
  setStage(LightmapBakeStage::Layout, .04f);
  struct Receiver { u32 instance; float side; };
  std::vector<Receiver> receivers;
  for (u32 i = 0; i < scene.instances.size(); ++i) {
    const auto &instance = scene.instances[i];
    if (!instance.receives || instance.mesh >= scene.meshes.size()) continue;
    const auto &mesh = scene.meshes[instance.mesh];
    if (!mesh.uv1Valid || mesh.uv1.size() != mesh.positions.size() / 3 * 2) { ++stats.rejectedReceivers; continue; }
    double worldArea = 0, uvArea = 0;
    for (usize t = 0; t + 2 < mesh.indices.size(); t += 3) {
      const u32 a = mesh.indices[t], b = mesh.indices[t + 1], c = mesh.indices[t + 2];
      const V3 p0 = world.transformPoint(i, V3(&mesh.positions[a * 3])), p1 = world.transformPoint(i, V3(&mesh.positions[b * 3])),
               p2 = world.transformPoint(i, V3(&mesh.positions[c * 3]));
      worldArea += .5 * length(cross(p1 - p0, p2 - p0));
      uvArea += .5 * std::fabs((mesh.uv1[b * 2] - mesh.uv1[a * 2]) * (mesh.uv1[c * 2 + 1] - mesh.uv1[a * 2 + 1]) -
                               (mesh.uv1[c * 2] - mesh.uv1[a * 2]) * (mesh.uv1[b * 2 + 1] - mesh.uv1[a * 2 + 1]));
    }
    if (worldArea <= 1e-10 || uvArea <= 1e-10) { ++stats.rejectedReceivers; continue; }
    const float side = settings.texelsPerUnit * std::max(instance.scaleInLightmap, 0.0f) * static_cast<float>(std::sqrt(worldArea / uvArea));
    if (!(side > 0)) continue;
    receivers.push_back({i, side});
  }
  stats.receivers = static_cast<u32>(receivers.size());
  std::vector<Rect> rects;
  u32 pageCount = 0, pageSize = 0;
  {
    const u32 maximum = settings.maxSize, pad = settings.padding;
    float scale = 1.0f;
    for (u32 attempt = 0; attempt < 40; ++attempt) {
      rects.clear();
      std::vector<u32> order(receivers.size());
      for (u32 i = 0; i < order.size(); ++i) order[i] = i;
      std::vector<u32> sides(receivers.size());
      for (u32 i = 0; i < receivers.size(); ++i)
        sides[i] = std::clamp<u32>(static_cast<u32>(std::ceil(receivers[i].side * scale)), 2, maximum - 2 * pad);
      std::stable_sort(order.begin(), order.end(), [&](u32 a, u32 b) { return sides[a] > sides[b]; });
      // Prateleiras: maior primeiro, da esquerda para a direita.
      u32 page = 0, x = 0, y = 0, shelf = 0;
      for (u32 index : order) {
        const u32 cell = sides[index] + 2 * pad;
        if (x + cell > maximum) { x = 0; y += shelf; shelf = 0; }
        if (y + cell > maximum) { ++page; x = 0; y = 0; shelf = 0; }
        rects.push_back({receivers[index].instance, page, x, y, sides[index]});
        x += cell;
        shelf = std::max(shelf, cell);
      }
      pageCount = receivers.empty() ? 0 : page + 1;
      if (pageCount <= MaximumLightmapPages) break;
      scale *= .8f;
    }
    if (pageCount > MaximumLightmapPages) { error = "Lightmaps demais: reduza a Resolução do lightmap"; return false; }
    // Todas as páginas do mesmo tamanho (um array na GPU); com uma página só,
    // ela encolhe para o que de fato foi usado.
    u32 used = 0;
    for (const auto &rect : rects) used = std::max({used, rect.x + rect.size + 2 * pad, rect.y + rect.size + 2 * pad});
    pageSize = pageCount > 1 ? maximum : std::min(maximum, std::max<u32>(64, (used + 63) / 64 * 64));
  }
  std::vector<Page> pages(pageCount);
  for (auto &page : pages) {
    page.size = pageSize;
    page.record.assign(usize(pageSize) * pageSize, -1);
  }
  // Rasterização da UV2 nos retângulos.
  std::vector<Texel> texels;
  std::vector<i32> rectOfInstance(scene.instances.size(), -1);
  for (u32 r = 0; r < rects.size(); ++r) rectOfInstance[rects[r].instance] = static_cast<i32>(r);
  for (const auto &rect : rects) {
    const auto &instance = scene.instances[rect.instance];
    const auto &mesh = scene.meshes[instance.mesh];
    auto &page = pages[rect.page];
    const float originX = static_cast<float>(rect.x + settings.padding), originY = static_cast<float>(rect.y + settings.padding);
    const float side = static_cast<float>(rect.size);
    for (usize t = 0; t + 2 < mesh.indices.size(); t += 3) {
      const u32 idx[3]{mesh.indices[t], mesh.indices[t + 1], mesh.indices[t + 2]};
      float px[3], py[3];
      V3 wp[3], wn[3];
      for (u32 k = 0; k < 3; ++k) {
        px[k] = originX + mesh.uv1[idx[k] * 2] * side;
        py[k] = originY + mesh.uv1[idx[k] * 2 + 1] * side;
        wp[k] = world.transformPoint(rect.instance, V3(&mesh.positions[idx[k] * 3]));
        wn[k] = mesh.normals.size() == mesh.positions.size() ? world.transformNormal(rect.instance, V3(&mesh.normals[idx[k] * 3])) : V3{};
      }
      V3 geometric = normalize(cross(wp[1] - wp[0], wp[2] - wp[0]));
      if (world.mirrored[rect.instance]) geometric = geometric * -1.0f;
      const float area = (px[1] - px[0]) * (py[2] - py[0]) - (px[2] - px[0]) * (py[1] - py[0]);
      if (std::fabs(area) < 1e-12f) continue;
      const float worldArea = .5f * length(cross(wp[1] - wp[0], wp[2] - wp[0]));
      const float footprint = std::sqrt(worldArea / (.5f * std::fabs(area)));
      const i32 x0 = std::max<i32>(static_cast<i32>(rect.x), static_cast<i32>(std::floor(std::min({px[0], px[1], px[2]}) - 1)));
      const i32 x1 = std::min<i32>(static_cast<i32>(rect.x + rect.size + 2 * settings.padding) - 1, static_cast<i32>(std::ceil(std::max({px[0], px[1], px[2]}) + 1)));
      const i32 y0 = std::max<i32>(static_cast<i32>(rect.y), static_cast<i32>(std::floor(std::min({py[0], py[1], py[2]}) - 1)));
      const i32 y1 = std::min<i32>(static_cast<i32>(rect.y + rect.size + 2 * settings.padding) - 1, static_cast<i32>(std::ceil(std::max({py[0], py[1], py[2]}) + 1)));
      for (i32 y = y0; y <= y1; ++y)
        for (i32 x = x0; x <= x1; ++x) {
          const float cx = x + .5f, cy = y + .5f;
          float b1 = ((cx - px[0]) * (py[2] - py[0]) - (px[2] - px[0]) * (cy - py[0])) / area;
          float b2 = ((px[1] - px[0]) * (cy - py[0]) - (cx - px[0]) * (py[1] - py[0])) / area;
          float b0 = 1 - b1 - b2;
          u8 quality = 2;
          float distance = 0;
          if (b0 < 0 || b1 < 0 || b2 < 0) {
            // Fora: o ponto mais próximo do triângulo, se estiver a menos de
            // 0,75 texel, ainda representa este texel (cobertura conservadora).
            b0 = std::max(b0, 0.0f); b1 = std::max(b1, 0.0f); b2 = std::max(b2, 0.0f);
            const float sum = b0 + b1 + b2;
            b0 /= sum; b1 /= sum; b2 /= sum;
            const float qx = px[0] * b0 + px[1] * b1 + px[2] * b2, qy = py[0] * b0 + py[1] * b1 + py[2] * b2;
            distance = std::hypot(qx - cx, qy - cy);
            if (distance > .75f) continue;
            quality = 1;
          }
          const usize at = usize(y) * pageSize + x;
          i32 &slot = page.record[at];
          if (slot >= 0) {
            const auto &existing = texels[slot];
            if (existing.quality > quality || (existing.quality == quality && existing.distance <= distance)) continue;
          }
          Texel texel;
          texel.position = wp[0] * b0 + wp[1] * b1 + wp[2] * b2;
          V3 n = wn[0] * b0 + wn[1] * b1 + wn[2] * b2;
          texel.normal = length(n) > 1e-6f ? normalize(n) : geometric;
          // A face que recebe é a da normal do vértice; o enrolamento só dá a direção do plano.
          texel.geometric = dot(texel.normal, geometric) < 0 ? geometric * -1.0f : geometric;
          texel.instance = rect.instance;
          texel.footprint = footprint;
          texel.quality = quality;
          texel.distance = distance;
          if (slot >= 0) texels[slot] = texel;
          else { slot = static_cast<i32>(texels.size()); texels.push_back(texel); }
        }
    }
    if (cancelled()) return false;
  }
  stats.texels = texels.size();
  u64 coveredTexels = 0;
  for (const auto &rect : rects) coveredTexels += u64(rect.size) * rect.size;
  stats.occupancy = pageCount ? static_cast<float>(static_cast<double>(coveredTexels) / (double(pageCount) * pageSize * pageSize)) : 0;
  // Página e posição de cada texel, para escrever o resultado de volta.
  std::vector<u32> texelPage(texels.size()), texelAt(texels.size());
  for (u32 p = 0; p < pages.size(); ++p)
    for (u32 i = 0; i < pages[p].record.size(); ++i)
      if (pages[p].record[i] >= 0) { texelPage[pages[p].record[i]] = p; texelAt[pages[p].record[i]] = i; }

  std::atomic<u64> rays{0};
  // --- Lado de superfície de dois lados ------------------------------------
  // Material double-sided (Double Sided GI da Unity) é visto pelos dois lados,
  // mas o texel guarda um valor só. Malha fechada com normais para dentro (a
  // copa de árvore da Vila) integraria o próprio interior e sairia preta: se
  // o lado da normal está fechado pelo próprio objeto e o outro lado vê
  // claramente mais céu, o texel passa a receber pelo outro lado.
  {
    constexpr u32 probe = 16;
    if (!workers.parallelFor(texels.size(), 256, progress.cancel, [&](u64 begin, u64 end) {
          u64 traced = 0;
          for (u64 i = begin; i < end; ++i) {
            auto &texel = texels[i];
            const auto &instance = scene.instances[texel.instance];
            if (instance.material >= scene.materials.size() || !scene.materials[instance.material].doubleSided) continue;
            u32 self[2]{}, open[2]{};
            for (u32 side = 0; side < 2; ++side) {
              const float sign = side ? -1.0f : 1.0f;
              const V3 normal = texel.normal * sign, geometric = texel.geometric * sign;
              const V3 origin = texel.position + geometric * bias;
              for (u32 s = 0; s < probe; ++s) {
                const V3 direction = cosineSample(normal, s, probe, hashFloat(static_cast<u32>(i) * 747796405u + side), .5f);
                if (dot(direction, geometric) <= 0) continue;
                ++traced;
                Hit hit;
                if (!world.trace(origin, direction, 1e30f, false, hit)) ++open[side];
                else if (world.triangles[hit.triangle].instance == texel.instance) ++self[side];
              }
            }
            if (self[0] * 4 >= probe * 3 && open[1] * 4 >= open[0] * 4 + probe) {
              texel.normal = texel.normal * -1.0f;
              texel.geometric = texel.geometric * -1.0f;
            }
          }
          rays.fetch_add(traced);
        }, [](u64) {}))
      return cancelled(), false;
  }

  // --- Luz direta ----------------------------------------------------------
  setStage(LightmapBakeStage::Direct, .08f);
  std::vector<V3> directBaked(texels.size()), directAll(texels.size());
  const float directWeight = .12f;
  const auto directAt = [&](V3 position, V3 normal, V3 geometric, u32 seed, bool onlyBaked, V3 &baked, V3 &all, u64 &traced) {
    baked = all = {};
    for (u32 l = 0; l < scene.lights.size(); ++l) {
      const auto &light = scene.lights[l];
      if (onlyBaked && !light.bakeDirect) continue;
      V3 toLight, radiance;
      float distance;
      lightRadiance(light, position, toLight, distance, radiance);
      const float nl = dot(normal, toLight);
      if (nl <= 0 || luminance(radiance) <= 0) continue;
      const u32 samples = light.shadowRadius > 0 ? std::max<u32>(1, settings.directSamples) : 1;
      float visible = 0;
      const V3 origin = position + geometric * bias;
      for (u32 s = 0; s < samples; ++s) {
        V3 direction = toLight;
        float maximum = distance - bias * 2;
        if (light.shadowRadius > 0) {
          V3 t, b;
          basis(toLight, t, b);
          const float r0 = hashFloat(seed * 9781u + s * 6271u + l), r1 = hashFloat(seed * 7919u + s * 104729u + l * 31u);
          const float radius = std::sqrt((s + r0) / samples), phi = 2 * Pi * radicalInverse(s) + r1 * 2 * Pi;
          if (light.kind == 0) {
            const float spread = std::tan(light.shadowRadius) * radius;
            direction = normalize(toLight + t * (spread * std::cos(phi)) + b * (spread * std::sin(phi)));
          } else {
            const V3 target = V3(light.position) + t * (light.shadowRadius * radius * std::cos(phi)) + b * (light.shadowRadius * radius * std::sin(phi));
            const V3 delta = target - origin;
            maximum = length(delta) - bias * 2;
            direction = normalize(delta);
          }
        }
        ++traced;
        Hit hit;
        if (!light.castsShadows || !world.trace(origin, direction, maximum, true, hit)) visible += 1;
      }
      const V3 e = radiance * (nl * visible / samples);
      all = all + e;
      if (light.bakeDirect) baked = baked + e;
    }
  };
  const u64 chunk = 256;
  if (!workers.parallelFor(texels.size(), chunk, progress.cancel, [&](u64 begin, u64 end) {
        u64 traced = 0;
        for (u64 i = begin; i < end; ++i) {
          const auto &texel = texels[i];
          directAt(texel.position, texel.normal, texel.geometric, static_cast<u32>(i), false, directBaked[i], directAll[i], traced);
        }
        rays.fetch_add(traced);
      }, [&](u64 done) { progress.fraction.store(.08f + directWeight * static_cast<float>(done) / std::max<u64>(1, texels.size())); }))
    return cancelled(), false;

  // --- Rebotes e céu -------------------------------------------------------
  const u32 passes = std::max<u32>(1, settings.bounces);
  progress.passes.store(passes);
  std::vector<V3> indirect(texels.size());
  std::vector<float> skyIrradiance(texels.size());
  std::vector<u32> backHits(texels.size()), cast(texels.size());
  // Página da etapa anterior: o que o raio encontra quando acerta um receptor.
  const auto publish = [&](const std::vector<V3> &local, const std::vector<float> &sky) {
    for (auto &page : pages) {
      page.local.assign(usize(page.size) * page.size * 3, 0.0f);
      page.sky.assign(usize(page.size) * page.size, -1.0f); // -1: sem valor
    }
    for (u32 i = 0; i < texels.size(); ++i) {
      if (!texels[i].valid) continue;
      auto &page = pages[texelPage[i]];
      const u32 at = texelAt[i];
      for (u32 k = 0; k < 3; ++k) page.local[at * 3 + k] = local[i][k];
      page.sky[at] = sky[i];
    }
    // Dilata para a busca bilinear não puxar preto da borda das ilhas.
    for (auto &page : pages)
      for (u32 iteration = 0; iteration < settings.padding + 2; ++iteration) {
        std::vector<float> nextSky = page.sky, nextLocal = page.local;
        const i32 size = static_cast<i32>(page.size);
        for (i32 y = 0; y < size; ++y)
          for (i32 x = 0; x < size; ++x) {
            const usize at = usize(y) * size + x;
            if (page.sky[at] >= 0) continue;
            float sum = 0, local3[3]{};
            u32 count = 0;
            for (i32 dy = -1; dy <= 1; ++dy)
              for (i32 dx = -1; dx <= 1; ++dx) {
                const i32 nx = x + dx, ny = y + dy;
                if (nx < 0 || ny < 0 || nx >= size || ny >= size) continue;
                const usize n = usize(ny) * size + nx;
                if (page.sky[n] < 0) continue;
                sum += page.sky[n];
                for (u32 k = 0; k < 3; ++k) local3[k] += page.local[n * 3 + k];
                ++count;
              }
            if (!count) continue;
            nextSky[at] = sum / count;
            for (u32 k = 0; k < 3; ++k) nextLocal[at * 3 + k] = local3[k] / count;
          }
        page.sky = std::move(nextSky);
        page.local = std::move(nextLocal);
      }
  };
  // Irradiância do passe anterior no ponto que o raio acertou.
  const auto previousAt = [&](const Hit &hit, V3 &local, float &sky, u64 &traced) {
    const auto &tri = world.triangles[hit.triangle];
    const i32 rect = rectOfInstance[tri.instance];
    const auto &instance = scene.instances[tri.instance];
    const auto &mesh = scene.meshes[instance.mesh];
    if (rect >= 0) {
      const auto &r = rects[rect];
      const auto &page = pages[r.page];
      const u32 a = mesh.indices[tri.local * 3], b = mesh.indices[tri.local * 3 + 1], c = mesh.indices[tri.local * 3 + 2];
      const float w = 1 - hit.u - hit.v;
      const float u = mesh.uv1[a * 2] * w + mesh.uv1[b * 2] * hit.u + mesh.uv1[c * 2] * hit.v;
      const float v = mesh.uv1[a * 2 + 1] * w + mesh.uv1[b * 2 + 1] * hit.u + mesh.uv1[c * 2 + 1] * hit.v;
      const float fx = r.x + settings.padding + u * r.size - .5f, fy = r.y + settings.padding + v * r.size - .5f;
      const i32 ix = static_cast<i32>(std::floor(fx)), iy = static_cast<i32>(std::floor(fy));
      const float tx = fx - ix, ty = fy - iy;
      float weight = 0, skySum = 0;
      V3 localSum{};
      for (u32 k = 0; k < 4; ++k) {
        const i32 x = ix + static_cast<i32>(k & 1), y = iy + static_cast<i32>(k >> 1);
        if (x < 0 || y < 0 || x >= static_cast<i32>(page.size) || y >= static_cast<i32>(page.size)) continue;
        const usize at = usize(y) * page.size + x;
        if (page.sky[at] < 0) continue;
        const float wk = ((k & 1) ? tx : 1 - tx) * ((k >> 1) ? ty : 1 - ty) + 1e-6f;
        weight += wk;
        skySum += page.sky[at] * wk;
        localSum = localSum + V3(&page.local[at * 3]) * wk;
      }
      if (weight > 0) { local = localSum * (1.0f / weight); sky = skySum / weight; return; }
    }
    // Contribui sem receber (sem UV2 ou Receive GI = Light Probes): direta na
    // hora e o céu pela inclinação (face para cima vê metade do céu aberto,
    // face para baixo -- o verso de um telhado -- não vê céu nenhum).
    const V3 p0 = tri.v0, position = p0 + tri.e1 * hit.u + tri.e2 * hit.v;
    V3 n = normalize(cross(tri.e1, tri.e2));
    if (tri.flags & TriMirrored) n = n * -1.0f;
    if (tri.flags & TriFlipped) n = n * -1.0f;
    if (hit.back) n = n * -1.0f;
    V3 baked, all;
    directAt(position, n, n, hit.triangle, false, baked, all, traced);
    local = all;
    sky = Pi * .25f * (1.0f + n.y);
  };
  std::vector<V3> passLocal(texels.size());
  std::vector<float> passSky(texels.size());
  {
    std::vector<V3> seedLocal = directAll;
    std::vector<float> seedSky(texels.size(), 0.0f);
    publish(seedLocal, seedSky);
  }
  const float passWeight = .68f / passes;
  for (u32 pass = 0; pass < passes; ++pass) {
    progress.pass.store(pass + 1);
    setStage(LightmapBakeStage::Indirect, .20f + passWeight * pass);
    const u32 samples = pass == 0 ? std::max(settings.indirectSamples, settings.environmentSamples) : settings.indirectSamples;
    if (!workers.parallelFor(texels.size(), 64, progress.cancel, [&](u64 begin, u64 end) {
          u64 traced = 0;
          for (u64 i = begin; i < end; ++i) {
            const auto &texel = texels[i];
            if (!texel.valid) continue;
            const V3 origin = texel.position + texel.geometric * bias;
            const float r0 = hashFloat(static_cast<u32>(i) * 2654435761u + pass * 97u), r1 = hashFloat(static_cast<u32>(i) * 40503u + pass * 7919u + 1u);
            V3 local{};
            float sky = 0;
            u32 back = 0;
            for (u32 s = 0; s < samples; ++s) {
              const V3 direction = cosineSample(texel.normal, s, samples, r0, r1);
              // Raio que vai para dentro da própria superfície (normal
              // interpolada) não tem o que devolver.
              if (dot(direction, texel.geometric) <= 0) continue;
              ++traced;
              Hit hit;
              if (!world.trace(origin, direction, 1e30f, false, hit)) { sky += 1.0f; continue; }
              if (hit.back) { ++back; continue; }
              V3 albedo;
              float alpha;
              world.surface(hit.triangle, hit.u, hit.v, albedo, alpha);
              V3 previousLocal;
              float previousSky;
              previousAt(hit, previousLocal, previousSky, traced);
              const auto &instance = scene.instances[world.triangles[hit.triangle].instance];
              V3 emission{};
              if (instance.material < scene.materials.size()) emission = V3(scene.materials[instance.material].emission);
              local = local + mul(albedo, previousLocal) * (1.0f / Pi) + emission;
              sky += luminance(albedo) * previousSky * (1.0f / Pi);
            }
            passLocal[i] = local * (Pi / samples);
            passSky[i] = sky * (Pi / samples);
            if (pass == 0) { backHits[i] = back; cast[i] = samples; }
          }
          rays.fetch_add(traced);
        }, [&](u64 done) {
          progress.fraction.store(.20f + passWeight * (pass + static_cast<float>(done) / std::max<u64>(1, texels.size())));
        }))
      return cancelled(), false;
    // Dentro de parede: mais da metade dos raios bateu no verso de uma face.
    if (pass == 0)
      for (u32 i = 0; i < texels.size(); ++i)
        if (cast[i] && backHits[i] * 2 > cast[i]) { texels[i].valid = false; ++stats.invalidTexels; }
    indirect = passLocal;
    skyIrradiance = passSky;
    if (pass + 1 < passes) {
      // O próximo passe vê a direta mais o rebote deste.
      std::vector<V3> seed(texels.size());
      for (u32 i = 0; i < texels.size(); ++i) seed[i] = directAll[i] + indirect[i];
      publish(seed, skyIrradiance);
    }
  }
  if (settings.bounces == 0) std::fill(indirect.begin(), indirect.end(), V3{});
  // --- Sondas de luz -------------------------------------------------------
  // O Generate Lighting da Unity assa as Light Probes junto, com as mesmas
  // luzes. Cada sonda integra a esfera inteira sobre as páginas finais (direta
  // de todas as luzes + rebote, a mesma radiância que um texel vê) e soma a
  // direta das luzes Assadas em forma fechada; o céu fica à parte, em L1, para
  // a hora do dia continuar mandando (fragmento: SH/π + céu × V(n)).
  std::vector<ShL2> probeSh(scene.probePositions.size());
  std::vector<ProbeSkyVisibility> probeSky(scene.probePositions.size());
  if (!scene.probePositions.empty()) {
    setStage(LightmapBakeStage::Indirect, .88f);
    std::vector<V3> seed(texels.size());
    for (u32 i = 0; i < texels.size(); ++i) seed[i] = directAll[i] + indirect[i];
    publish(seed, skyIrradiance);
    const u32 samples = std::max<u32>(256, settings.indirectSamples);
    const float band[9]{Pi, 2 * Pi / 3, 2 * Pi / 3, 2 * Pi / 3, Pi / 4, Pi / 4, Pi / 4, Pi / 4, Pi / 4};
    if (!workers.parallelFor(scene.probePositions.size(), 1, progress.cancel, [&](u64 begin, u64 end) {
          u64 traced = 0;
          for (u64 i = begin; i < end; ++i) {
            const V3 origin(scene.probePositions[i].data());
            float local[9][3]{}, sky[4]{};
            const float r = hashFloat(static_cast<u32>(i) * 2246822519u + 7u);
            for (u32 s = 0; s < samples; ++s) {
              // Esfera uniforme (Hammersley girado por sonda).
              const float z = 1 - 2 * (s + .5f) / samples, ring = std::sqrt(std::max(0.0f, 1 - z * z));
              const float phi = 2 * Pi * (radicalInverse(s) + r);
              const V3 direction(ring * std::cos(phi), z, ring * std::sin(phi));
              float y[9];
              const float d[3]{direction[0], direction[1], direction[2]};
              shL2Basis(d, y);
              ++traced;
              Hit hit;
              V3 radiance{};
              float skyRadiance = 0;
              if (!world.trace(origin, direction, 1e30f, false, hit)) skyRadiance = 1;
              else if (!hit.back) {
                V3 albedo, previousLocal;
                float alpha, previousSky;
                world.surface(hit.triangle, hit.u, hit.v, albedo, alpha);
                previousAt(hit, previousLocal, previousSky, traced);
                const auto &instance = scene.instances[world.triangles[hit.triangle].instance];
                V3 emission{};
                if (instance.material < scene.materials.size()) emission = V3(scene.materials[instance.material].emission);
                radiance = mul(albedo, previousLocal) * (1.0f / Pi) + emission;
                skyRadiance = luminance(albedo) * previousSky * (1.0f / Pi);
              }
              for (u32 b = 0; b < 9; ++b) for (u32 k = 0; k < 3; ++k) local[b][k] += radiance[k] * y[b];
              for (u32 b = 0; b < 4; ++b) sky[b] += skyRadiance * y[b];
            }
            const float weight = 4 * Pi / samples;
            auto &sh = probeSh[i];
            for (u32 b = 0; b < 9; ++b) for (u32 k = 0; k < 3; ++k) sh.c[b][k] = local[b][k] * weight * band[b];
            // Direta das Assadas: luz pontual é um delta, E_lm = A_l · E · Y_lm(l).
            for (const auto &light : scene.lights) {
              if (!light.bakeDirect) continue;
              V3 toLight, lightRadiance3;
              float distance;
              lightRadiance(light, origin, toLight, distance, lightRadiance3);
              if (luminance(lightRadiance3) <= 0) continue;
              Hit blocked;
              ++traced;
              if (light.castsShadows && world.trace(origin, toLight, distance - bias * 2, true, blocked)) continue;
              float y[9];
              const float d[3]{toLight[0], toLight[1], toLight[2]};
              shL2Basis(d, y);
              for (u32 b = 0; b < 9; ++b) for (u32 k = 0; k < 3; ++k) sh.c[b][k] += band[b] * lightRadiance3[k] * y[b];
            }
            // Céu: V(n) = E_céu(n)/π; 1 numa sonda em campo aberto.
            for (u32 b = 0; b < 4; ++b) probeSky[i][b] = sky[b] * weight * band[b] / Pi * settings.indirectIntensity;
          }
          rays.fetch_add(traced);
        }, [](u64) {}))
      return cancelled(), false;
  }
  stats.rays = rays.load();

  // --- Denoise, dilatação e codificação -----------------------------------
  setStage(LightmapBakeStage::Filtering, .89f);
  // Planos de página com o resultado em float: indireto+céu filtráveis e a direta.
  struct Plane { std::vector<float> indirect, direct, sky; std::vector<u8> valid; };
  std::vector<Plane> planes(pages.size());
  for (u32 p = 0; p < pages.size(); ++p) {
    const usize count = usize(pages[p].size) * pages[p].size;
    planes[p].indirect.assign(count * 3, 0.0f);
    planes[p].direct.assign(count * 3, 0.0f);
    planes[p].sky.assign(count, 0.0f);
    planes[p].valid.assign(count, 0);
  }
  for (u32 i = 0; i < texels.size(); ++i) {
    if (!texels[i].valid) continue;
    auto &plane = planes[texelPage[i]];
    const u32 at = texelAt[i];
    for (u32 k = 0; k < 3; ++k) {
      plane.indirect[at * 3 + k] = indirect[i][k] * settings.indirectIntensity;
      plane.direct[at * 3 + k] = directBaked[i][k];
    }
    plane.sky[at] = std::clamp(skyIrradiance[i] / Pi * settings.indirectIntensity, 0.0f, 4.0f);
    plane.valid[at] = 1;
  }
  if (settings.denoise) {
    // À-trous (Dammertz et al. 2010) com núcleo B3, passos 1, 2 e 4, parado
    // por normal, posição e objeto: suaviza o ruído do Monte Carlo sem
    // atravessar quina nem vazar de um objeto para outro.
    static constexpr float Kernel[5]{1.0f / 16, 1.0f / 4, 3.0f / 8, 1.0f / 4, 1.0f / 16};
    for (u32 p = 0; p < pages.size(); ++p) {
      auto &plane = planes[p];
      const auto &page = pages[p];
      const i32 size = static_cast<i32>(page.size);
      for (u32 step = 1; step <= 4; step *= 2) {
        std::vector<float> nextIndirect = plane.indirect, nextSky = plane.sky;
        workers.parallelFor(static_cast<u64>(size), 8, progress.cancel, [&](u64 begin, u64 end) {
          for (i32 y = static_cast<i32>(begin); y < static_cast<i32>(end); ++y)
            for (i32 x = 0; x < size; ++x) {
              const usize at = usize(y) * size + x;
              const i32 record = page.record[at];
              if (record < 0 || !plane.valid[at]) continue;
              const auto &center = texels[record];
              const float sigma = std::max(center.footprint * 2.0f * step, 1e-4f);
              float weight = 0, sky = 0;
              V3 sum{};
              for (i32 j = -2; j <= 2; ++j)
                for (i32 i = -2; i <= 2; ++i) {
                  const i32 nx = x + i * static_cast<i32>(step), ny = y + j * static_cast<i32>(step);
                  if (nx < 0 || ny < 0 || nx >= size || ny >= size) continue;
                  const usize n = usize(ny) * size + nx;
                  const i32 other = page.record[n];
                  if (other < 0 || !plane.valid[n] || texels[other].instance != center.instance) continue;
                  const auto &texel = texels[other];
                  const float normalWeight = std::pow(std::max(0.0f, dot(center.normal, texel.normal)), 32.0f);
                  const V3 delta = texel.position - center.position;
                  const float positionWeight = std::exp(-dot(delta, delta) / (2 * sigma * sigma));
                  const float w = Kernel[i + 2] * Kernel[j + 2] * normalWeight * positionWeight;
                  if (w <= 0) continue;
                  weight += w;
                  sum = sum + V3(&plane.indirect[n * 3]) * w;
                  sky += plane.sky[n] * w;
                }
              if (weight <= 0) continue;
              for (u32 k = 0; k < 3; ++k) nextIndirect[at * 3 + k] = sum[k] / weight;
              nextSky[at] = sky / weight;
            }
        });
        plane.indirect = std::move(nextIndirect);
        plane.sky = std::move(nextSky);
        if (cancelled()) return false;
      }
    }
  }
  // Dilatação: os texels vazios (fora das ilhas e dentro de parede) recebem a
  // média dos vizinhos válidos, passo a passo, até cobrir o padding.
  for (u32 p = 0; p < pages.size(); ++p) {
    auto &plane = planes[p];
    const i32 size = static_cast<i32>(pages[p].size);
    for (u32 iteration = 0; iteration < settings.padding * 2 + 4; ++iteration) {
      std::vector<u8> nextValid = plane.valid;
      bool changed = false;
      for (i32 y = 0; y < size; ++y)
        for (i32 x = 0; x < size; ++x) {
          const usize at = usize(y) * size + x;
          if (plane.valid[at]) continue;
          float sum[7]{};
          u32 count = 0;
          for (i32 dy = -1; dy <= 1; ++dy)
            for (i32 dx = -1; dx <= 1; ++dx) {
              const i32 nx = x + dx, ny = y + dy;
              if (nx < 0 || ny < 0 || nx >= size || ny >= size) continue;
              const usize n = usize(ny) * size + nx;
              if (!plane.valid[n]) continue;
              for (u32 k = 0; k < 3; ++k) { sum[k] += plane.indirect[n * 3 + k]; sum[3 + k] += plane.direct[n * 3 + k]; }
              sum[6] += plane.sky[n];
              ++count;
            }
          if (!count) continue;
          for (u32 k = 0; k < 3; ++k) { plane.indirect[at * 3 + k] = sum[k] / count; plane.direct[at * 3 + k] = sum[3 + k] / count; }
          plane.sky[at] = sum[6] / count;
          nextValid[at] = 2;
          changed = true;
        }
      plane.valid = std::move(nextValid);
      if (!changed) break;
    }
  }
  setStage(LightmapBakeStage::Encoding, .96f);
  LightmapData result;
  result.settings = settings;
  result.pages.resize(pages.size());
  for (u32 p = 0; p < pages.size(); ++p) {
    auto &page = result.pages[p];
    const auto &plane = planes[p];
    page.width = page.height = pages[p].size;
    const usize count = usize(page.width) * page.height;
    page.format = settings.compress ? LightmapPage::FormatSqrtRgba8 : LightmapPage::FormatHalf;
    if (settings.compress) {
      // Faixa da página no percentil 99,9 do canal mais forte: um texel
      // colado a uma lâmpada não achata o resto da página em 8 bits.
      std::vector<float> peaks;
      peaks.reserve(count);
      for (usize i = 0; i < count; ++i) {
        float peak = 0;
        for (u32 k = 0; k < 3; ++k) peak = std::max(peak, plane.indirect[i * 3 + k] + plane.direct[i * 3 + k]);
        if (plane.valid[i]) peaks.push_back(peak);
      }
      float range = 1e-4f;
      if (!peaks.empty()) {
        const usize nth = std::min(peaks.size() - 1, static_cast<usize>(peaks.size() * .999));
        std::nth_element(peaks.begin(), peaks.begin() + nth, peaks.end());
        range = std::max(range, peaks[nth]);
      }
      page.range = range;
      page.texels.resize(count * 4);
      for (usize i = 0; i < count; ++i) {
        for (u32 k = 0; k < 3; ++k) {
          const float e = std::clamp((plane.indirect[i * 3 + k] + plane.direct[i * 3 + k]) / range, 0.0f, 1.0f);
          page.texels[i * 4 + k] = static_cast<u8>(std::lround(std::sqrt(e) * 255.0f));
        }
        page.texels[i * 4 + 3] = static_cast<u8>(std::lround(std::sqrt(std::clamp(plane.sky[i], 0.0f, 1.0f)) * 255.0f));
      }
    } else {
      page.range = 1;
      page.texels.resize(count * 8);
      for (usize i = 0; i < count; ++i) {
        u16 h[4];
        for (u32 k = 0; k < 3; ++k) h[k] = floatToHalf(plane.indirect[i * 3 + k] + plane.direct[i * 3 + k]);
        h[3] = floatToHalf(plane.sky[i]);
        std::memcpy(page.texels.data() + i * 8, h, 8);
      }
    }
  }
  for (const auto &rect : rects) {
    const auto &instance = scene.instances[rect.instance];
    LightmapRendererEntry entry;
    entry.objectId = instance.objectId;
    entry.slot = instance.slot;
    entry.page = rect.page;
    const float size = static_cast<float>(pageSize);
    entry.scaleOffset[0] = entry.scaleOffset[1] = rect.size / size;
    entry.scaleOffset[2] = (rect.x + settings.padding) / size;
    entry.scaleOffset[3] = (rect.y + settings.padding) / size;
    result.entries.push_back(entry);
  }
  result.stats = stats;
  result.sourceHash = lightmapSceneHash(scene, settings);
  result.bakeSeconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();
  if (!result.valid()) { error = "Resultado do bake inválido"; return false; }
  setStage(LightmapBakeStage::Done, 1.0f);
  result.probeSh = std::move(probeSh);
  result.probeSky = std::move(probeSky);
  out = std::move(result);
  return true;
}

} // namespace ae::renderer
