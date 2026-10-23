#include "resources/lightmap_uv_generate.h"

#include "renderer/mesh_report.h"
#include "scene/lightmap_uv.h"
#include "xatlas.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <tuple>
#include <unordered_map>

namespace ae::resources {
namespace {
constexpr u32 Uv1Offset = 36;

struct Range {
  u32 firstIndex = 0, indexCount = 0, vertexOffset = 0;
  bool operator<(const Range &o) const noexcept {
    return std::tie(firstIndex, indexCount, vertexOffset) < std::tie(o.firstIndex, o.indexCount, o.vertexOffset);
  }
};
// Resultado de um desenho: vértices (cópia do original com a UV1 nova) e
// índices locais. `original[i]` é o vértice global de onde o i-ésimo saiu.
struct Unwrapped {
  bool done = false;
  std::vector<u32> original;
  std::vector<float> uv;
  std::vector<u32> indices;
  u32 charts = 0;
};

using Vec3 = std::array<float, 3>;
Vec3 sub(const Vec3 &a, const Vec3 &b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 cross(const Vec3 &a, const Vec3 &b) { return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}; }
float dot(const Vec3 &a, const Vec3 &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

// Desembrulha um desenho. Triângulos sem área no mundo saem dos índices: não
// desenham nada e, com UV nula, o lightmap recusaria a malha inteira.
bool unwrap(std::span<const u8> vertices, std::span<const u32> indices, const Range &range,
            const LightmapUvSettings &settings, Unwrapped &out, std::string &diagnostic) {
  const u64 vertexTotal = vertices.size() / renderer::MapVertexStride;
  std::vector<u32> corners;
  corners.reserve(range.indexCount);
  std::vector<Vec3> positions;
  std::vector<Vec3> faceNormals;
  std::vector<float> faceAreas;
  for (u32 t = 0; t + 2 < range.indexCount; t += 3) {
    u32 v[3];
    Vec3 p[3];
    for (u32 k = 0; k < 3; ++k) {
      const u64 g = u64(range.vertexOffset) + indices[range.firstIndex + t + k];
      if (g >= vertexTotal) { diagnostic = "Índice fora dos vértices ao gerar UV de lightmap."; return false; }
      v[k] = static_cast<u32>(g);
      std::memcpy(p[k].data(), vertices.data() + g * renderer::MapVertexStride, 12);
    }
    const auto n = cross(sub(p[1], p[0]), sub(p[2], p[0]));
    const float area2 = std::sqrt(dot(n, n));
    if (!(area2 > 1e-10f)) continue;
    for (u32 k = 0; k < 3; ++k) { corners.push_back(v[k]); positions.push_back(p[k]); }
    faceNormals.push_back({n[0] / area2, n[1] / area2, n[2] / area2});
    faceAreas.push_back(area2 * .5f);
  }
  const u32 faceCount = static_cast<u32>(faceNormals.size());
  if (!faceCount) return true; // só degenerados: o chamador conta

  // Normal por canto, quebrada nas arestas vivas: cada canto soma as faces do
  // mesmo ponto (por posição, porque o glTF duplica vértice na costura de UV0)
  // que estão a menos do Hard Angle da sua.
  float lo[3]{1e30f, 1e30f, 1e30f}, hi[3]{-1e30f, -1e30f, -1e30f};
  for (const auto &p : positions)
    for (u32 k = 0; k < 3; ++k) { lo[k] = std::min(lo[k], p[k]); hi[k] = std::max(hi[k], p[k]); }
  const float extent = std::max({hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2], 1e-6f});
  const float quantum = extent * 1e-6f;
  std::unordered_map<u64, u32> groupOf;
  std::vector<u32> cornerGroup(corners.size());
  std::vector<std::vector<u32>> groupFaces;
  for (u32 c = 0; c < corners.size(); ++c) {
    u64 key = 1469598103934665603ull;
    for (u32 k = 0; k < 3; ++k) key = (key ^ static_cast<u64>(static_cast<i64>(std::llround((positions[c][k] - lo[k]) / quantum)))) * 1099511628211ull;
    auto [it, inserted] = groupOf.emplace(key, static_cast<u32>(groupFaces.size()));
    if (inserted) groupFaces.emplace_back();
    cornerGroup[c] = it->second;
    groupFaces[it->second].push_back(c / 3);
  }
  const float cosHard = std::cos(settings.hardAngleDegrees * 3.14159265f / 180.0f);
  std::vector<Vec3> cornerNormals(corners.size());
  for (u32 c = 0; c < corners.size(); ++c) {
    const auto &own = faceNormals[c / 3];
    Vec3 sum{};
    for (u32 f : groupFaces[cornerGroup[c]]) {
      if (dot(own, faceNormals[f]) + 1e-4f < cosHard) continue;
      for (u32 k = 0; k < 3; ++k) sum[k] += faceNormals[f][k] * faceAreas[f];
    }
    const float length = std::sqrt(dot(sum, sum));
    cornerNormals[c] = length > 1e-12f ? Vec3{sum[0] / length, sum[1] / length, sum[2] / length} : own;
  }
  std::vector<float> cornerUv(corners.size() * 2);
  for (u32 c = 0; c < corners.size(); ++c) {
    const auto view = renderer::readMapVertex(vertices, corners[c]);
    cornerUv[c * 2] = view.uv0[0];
    cornerUv[c * 2 + 1] = view.uv0[1];
  }

  xatlas::Atlas *atlas = xatlas::Create();
  xatlas::MeshDecl mesh;
  mesh.vertexCount = static_cast<u32>(corners.size());
  mesh.vertexPositionData = positions.data();
  mesh.vertexPositionStride = sizeof(Vec3);
  mesh.vertexNormalData = cornerNormals.data();
  mesh.vertexNormalStride = sizeof(Vec3);
  mesh.vertexUvData = cornerUv.data();
  mesh.vertexUvStride = sizeof(float) * 2;
  mesh.faceCount = faceCount;
  mesh.epsilon = quantum;
  if (xatlas::AddMesh(atlas, mesh, 1) != xatlas::AddMeshError::Success) {
    xatlas::Destroy(atlas);
    diagnostic = "O xatlas recusou a malha ao gerar UV de lightmap.";
    return false;
  }
  xatlas::ChartOptions chart;
  // Hard Angle já está nas normais por canto; costura de normal é respeitada
  // inteira, como a aresta viva da Unity.
  chart.normalSeamWeight = 2000.0f;
  chart.textureSeamWeight = 0.5f;
  xatlas::PackOptions pack;
  pack.padding = static_cast<u32>(std::lround(settings.packMarginPixels));
  pack.resolution = 0;
  pack.texelsPerUnit = 0;
  pack.bilinear = true;
  pack.rotateCharts = true;
  xatlas::Generate(atlas, chart, pack);
  if (atlas->meshCount != 1 || !atlas->width || !atlas->height) {
    xatlas::Destroy(atlas);
    diagnostic = "O xatlas não produziu atlas para a malha.";
    return false;
  }
  const xatlas::Mesh &result = atlas->meshes[0];
  const float scale = 1.0f / static_cast<float>(std::max(atlas->width, atlas->height));
  // Solda: o mesmo vértice de origem com a mesma UV nova é um vértice só.
  std::unordered_map<u64, u32> welded;
  std::vector<u32> remap(result.vertexCount);
  for (u32 i = 0; i < result.vertexCount; ++i) {
    const auto &vertex = result.vertexArray[i];
    const u32 source = corners[vertex.xref];
    const float u = std::clamp(vertex.uv[0] * scale, 0.0f, 1.0f), v = std::clamp(vertex.uv[1] * scale, 0.0f, 1.0f);
    const u64 key = (u64(source) << 32) ^ (u64(static_cast<u32>(std::lround(u * 65535.0f))) << 16) ^
                    u64(static_cast<u32>(std::lround(v * 65535.0f)));
    auto [it, inserted] = welded.emplace(key, static_cast<u32>(out.original.size()));
    if (inserted) {
      out.original.push_back(source);
      out.uv.push_back(u);
      out.uv.push_back(v);
    }
    remap[i] = it->second;
  }
  out.indices.resize(result.indexCount);
  for (u32 i = 0; i < result.indexCount; ++i) out.indices[i] = remap[result.indexArray[i]];
  out.charts = result.chartCount;
  out.done = true;
  xatlas::Destroy(atlas);
  return true;
}
} // namespace

bool generateLightmapUvs(std::vector<u8> &vertices, std::vector<u32> &indices,
                         std::vector<renderer::MapDrawRecord> &draws, std::span<const u8> deformed,
                         const LightmapUvSettings &settings, bool compact, LightmapUvReport &report,
                         std::string &diagnostic) {
  report = {};
  if (!settings.valid()) { diagnostic = "Hard Angle ou Pack Margin fora do intervalo."; return false; }
  if (vertices.size() % renderer::MapVertexStride) { diagnostic = "Vértices fora do passo do pacote."; return false; }
  report.verticesBefore = vertices.size() / renderer::MapVertexStride;
  std::map<Range, Unwrapped> ranges;
  std::map<Range, bool> keep; // faixas que não mudam (deformadas ou sem área)
  for (usize d = 0; d < draws.size(); ++d) {
    const auto &draw = draws[d];
    const Range range{draw.firstIndex, draw.indexCount, draw.vertexOffset};
    if (u64(draw.firstIndex) + draw.indexCount > indices.size() || draw.indexCount % 3) {
      diagnostic = "Desenho fora dos índices ao gerar UV de lightmap.";
      return false;
    }
    if (d < deformed.size() && deformed[d]) { keep[range] = true; ++report.drawsDeformed; continue; }
    if (ranges.count(range) || keep.count(range)) continue;
    Unwrapped item;
    if (!unwrap(vertices, indices, range, settings, item, diagnostic)) return false;
    if (!item.done) { keep[range] = true; ++report.drawsDegenerate; continue; }
    report.charts += item.charts;
    ++report.drawsUnwrapped;
    ranges.emplace(range, std::move(item));
  }
  for (const auto &[range, value] : keep) ranges.erase(range);
  if (ranges.empty()) {
    report.verticesAfter = report.verticesBefore;
    return true;
  }

  // Monta os buffers novos. Com `compact`, cada faixa (a desembrulhada ou a
  // mantida) entra de novo do zero e o que ninguém usa some; sem, o buffer
  // original fica intacto e as faixas novas entram no fim.
  std::vector<u8> nextVertices;
  std::vector<u32> nextIndices;
  nextIndices.reserve(indices.size());
  if (!compact) nextVertices = vertices;
  std::map<Range, Range> moved;
  const auto emitUnwrapped = [&](const Range &range, const Unwrapped &item) {
    const u32 base = static_cast<u32>(nextVertices.size() / renderer::MapVertexStride);
    const usize at = nextVertices.size();
    nextVertices.resize(at + item.original.size() * renderer::MapVertexStride);
    for (usize i = 0; i < item.original.size(); ++i) {
      u8 *target = nextVertices.data() + at + i * renderer::MapVertexStride;
      std::memcpy(target, vertices.data() + usize(item.original[i]) * renderer::MapVertexStride, renderer::MapVertexStride);
      std::memcpy(target + Uv1Offset, item.uv.data() + i * 2, 8);
    }
    const u32 first = static_cast<u32>(nextIndices.size());
    nextIndices.insert(nextIndices.end(), item.indices.begin(), item.indices.end());
    moved[range] = {first, static_cast<u32>(item.indices.size()), base};
  };
  const auto emitKept = [&](const Range &range) {
    const u32 first = static_cast<u32>(nextIndices.size());
    if (!compact) {
      nextIndices.insert(nextIndices.end(), indices.begin() + range.firstIndex, indices.begin() + range.firstIndex + range.indexCount);
      moved[range] = {first, range.indexCount, range.vertexOffset};
      return;
    }
    std::unordered_map<u32, u32> local;
    const u32 base = static_cast<u32>(nextVertices.size() / renderer::MapVertexStride);
    for (u32 i = 0; i < range.indexCount; ++i) {
      const u32 g = range.vertexOffset + indices[range.firstIndex + i];
      auto [it, inserted] = local.emplace(g, static_cast<u32>(local.size()));
      if (inserted) {
        const usize at = nextVertices.size();
        nextVertices.resize(at + renderer::MapVertexStride);
        std::memcpy(nextVertices.data() + at, vertices.data() + usize(g) * renderer::MapVertexStride, renderer::MapVertexStride);
      }
      nextIndices.push_back(it->second);
    }
    moved[range] = {first, range.indexCount, base};
  };
  for (const auto &draw : draws) {
    const Range range{draw.firstIndex, draw.indexCount, draw.vertexOffset};
    if (moved.count(range)) continue;
    if (const auto found = ranges.find(range); found != ranges.end()) emitUnwrapped(range, found->second);
    else emitKept(range);
  }
  if (nextVertices.size() / renderer::MapVertexStride > 0xffffffffull) { diagnostic = "Vértices demais depois do UV de lightmap."; return false; }
  for (auto &draw : draws) {
    const auto &target = moved.at({draw.firstIndex, draw.indexCount, draw.vertexOffset});
    draw.firstIndex = target.firstIndex;
    draw.indexCount = target.indexCount;
    draw.vertexOffset = target.vertexOffset;
  }
  vertices = std::move(nextVertices);
  indices = std::move(nextIndices);
  report.verticesAfter = vertices.size() / renderer::MapVertexStride;
  return true;
}

} // namespace ae::resources
