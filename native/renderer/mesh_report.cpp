#include "renderer/mesh_report.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace ae::renderer {
namespace {

// Deslocamentos do vértice de 48 bytes (renderer/authoring_geometry.h descreve
// a mesma estrutura do lado de quem escreve).
constexpr u32 kNormalOffset = 12, kTangentOffset = 20, kUv0Offset = 28, kUv1Offset = 36, kColorOffset = 44;

float unpackSnorm(i16 value) { return std::max(-1.0f, static_cast<float>(value) / 32767.0f); }

float triangleArea(const float a[3], const float b[3], const float c[3]) {
  const float u[3]{b[0] - a[0], b[1] - a[1], b[2] - a[2]};
  const float v[3]{c[0] - a[0], c[1] - a[1], c[2] - a[2]};
  const float cross[3]{u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
  return .5f * std::sqrt(cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2]);
}

float uvTriangleArea(const float a[2], const float b[2], const float c[2]) {
  return .5f * std::fabs((b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1]));
}

// Percentil sobre triângulos já ordenados, ponderado pela ÁREA: mil triângulos
// minúsculos de um parafuso não podem definir a densidade de uma parede.
float weightedPercentile(const std::vector<std::pair<float, float>> &sorted, float total, float fraction) {
  if (sorted.empty() || total <= 0) return 0;
  const float target = total * fraction;
  float accumulated = 0;
  for (const auto &entry : sorted) {
    accumulated += entry.second;
    if (accumulated >= target) return entry.first;
  }
  return sorted.back().first;
}

} // namespace

MapVertexView readMapVertex(std::span<const u8> vertices, u64 index) {
  MapVertexView view;
  const u64 offset = index * MapVertexStride;
  if (offset + MapVertexStride > vertices.size()) return view;
  const u8 *base = vertices.data() + offset;
  std::memcpy(view.position, base, sizeof(view.position));
  i16 packed[4];
  std::memcpy(packed, base + kNormalOffset, sizeof(packed));
  for (u32 axis = 0; axis < 3; ++axis) view.normal[axis] = unpackSnorm(packed[axis]);
  std::memcpy(packed, base + kTangentOffset, sizeof(packed));
  for (u32 axis = 0; axis < 3; ++axis) view.tangent[axis] = unpackSnorm(packed[axis]);
  view.tangentSign = unpackSnorm(packed[3]);
  std::memcpy(view.uv0, base + kUv0Offset, sizeof(view.uv0));
  std::memcpy(view.uv1, base + kUv1Offset, sizeof(view.uv1));
  u32 color = 0;
  std::memcpy(&color, base + kColorOffset, sizeof(color));
  for (u32 channel = 0; channel < 4; ++channel)
    view.color[channel] = static_cast<float>((color >> (channel * 8)) & 0xffu) / 255.0f;
  return view;
}

bool buildMeshDataReport(std::span<const u8> vertices, std::span<const u32> indices,
                         const MapDrawRecord &draw, float uniformScale, MeshDataReport &out) {
  if (!(uniformScale > 0) || !std::isfinite(uniformScale)) return false;
  if (vertices.size() % MapVertexStride || !draw.indexCount || draw.indexCount % 3) return false;
  if (static_cast<u64>(draw.firstIndex) + draw.indexCount > indices.size()) return false;
  const u64 vertexTotal = vertices.size() / MapVertexStride;

  MeshDataReport report;
  report.triangleCount = draw.indexCount / 3;
  // Vértices DESTE desenho, não do pacote: um GLB inteiro num buffer só faria
  // toda malha relatar o mesmo número.
  u64 lowestVertex = vertexTotal, highestVertex = 0;
  bool first = true;
  u32 outsideUnit = 0, uvVertices = 0;
  std::vector<std::pair<float, float>> densities;
  densities.reserve(report.triangleCount);
  float densityWeight = 0;

  for (u32 triangle = 0; triangle < report.triangleCount; ++triangle) {
    MapVertexView corner[3];
    for (u32 point = 0; point < 3; ++point) {
      const u64 index = static_cast<u64>(draw.vertexOffset) + indices[draw.firstIndex + triangle * 3 + point];
      if (index >= vertexTotal) return false;
      lowestVertex = std::min(lowestVertex, index);
      highestVertex = std::max(highestVertex, index);
      corner[point] = readMapVertex(vertices, index);
    }
    for (u32 point = 0; point < 3; ++point) {
      const auto &vertex = corner[point];
      for (u32 axis = 0; axis < 3; ++axis) {
        const float value = vertex.position[axis] * uniformScale;
        if (first) {
          report.boundsMinimum[axis] = report.boundsMaximum[axis] = value;
        } else {
          report.boundsMinimum[axis] = std::min(report.boundsMinimum[axis], value);
          report.boundsMaximum[axis] = std::max(report.boundsMaximum[axis], value);
        }
      }
      const float normalLength = std::sqrt(vertex.normal[0] * vertex.normal[0] + vertex.normal[1] * vertex.normal[1] +
                                           vertex.normal[2] * vertex.normal[2]);
      if (normalLength > .5f) report.hasNormals = true;
      else ++report.zeroNormals;
      const float tangentLength = std::sqrt(vertex.tangent[0] * vertex.tangent[0] +
                                            vertex.tangent[1] * vertex.tangent[1] +
                                            vertex.tangent[2] * vertex.tangent[2]);
      if (tangentLength > .5f) {
        report.hasTangents = true;
        // O sinal é o handedness do espaço tangente: negativo espelha o mapa
        // normal, zero não é sinal nenhum e deixa a base sem orientação.
        if (vertex.tangentSign < -.5f) ++report.mirroredTangents;
        else if (vertex.tangentSign < .5f) ++report.invalidTangents;
      } else {
        ++report.invalidTangents;
      }
      if (vertex.uv0[0] != 0 || vertex.uv0[1] != 0) report.hasUv0 = true;
      if (vertex.uv1[0] != 0 || vertex.uv1[1] != 0) report.hasUv1 = true;
      if (vertex.color[0] < 1 || vertex.color[1] < 1 || vertex.color[2] < 1 || vertex.color[3] < 1)
        report.hasColors = true;
      for (u32 axis = 0; axis < 2; ++axis) {
        if (first) {
          report.uvMinimum[axis] = report.uvMaximum[axis] = vertex.uv0[axis];
        } else {
          report.uvMinimum[axis] = std::min(report.uvMinimum[axis], vertex.uv0[axis]);
          report.uvMaximum[axis] = std::max(report.uvMaximum[axis], vertex.uv0[axis]);
        }
      }
      ++uvVertices;
      if (vertex.uv0[0] < -1e-4f || vertex.uv0[0] > 1 + 1e-4f || vertex.uv0[1] < -1e-4f || vertex.uv0[1] > 1 + 1e-4f)
        ++outsideUnit;
      first = false;
    }

    float scaled[3][3];
    for (u32 point = 0; point < 3; ++point)
      for (u32 axis = 0; axis < 3; ++axis) scaled[point][axis] = corner[point].position[axis] * uniformScale;
    const float area = triangleArea(scaled[0], scaled[1], scaled[2]);
    const float uv = uvTriangleArea(corner[0].uv0, corner[1].uv0, corner[2].uv0);
    report.surfaceArea += area;
    report.uvArea += uv;
    if (area <= 1e-12f) {
      ++report.degenerateTriangles;
      continue;
    }
    if (uv <= 1e-12f) {
      ++report.zeroAreaUvTriangles;
      continue;
    }
    // Texels por metro por pixel de textura: a raiz da razão entre a área de UV
    // e a área no mundo. Vale para textura quadrada; retangular entra pela
    // resolução que quem chama escolhe.
    densities.emplace_back(std::sqrt(uv / area), area);
    densityWeight += area;
  }

  report.vertexCount = highestVertex >= lowestVertex ? static_cast<u32>(highestVertex - lowestVertex + 1) : 0;
  report.outsideUnitRatio = uvVertices ? static_cast<float>(outsideUnit) / static_cast<float>(uvVertices) : 0;
  if (!report.hasUv0) {
    report.uvMinimum[0] = report.uvMinimum[1] = report.uvMaximum[0] = report.uvMaximum[1] = 0;
    report.outsideUnitRatio = 0;
  }
  std::sort(densities.begin(), densities.end(),
            [](const std::pair<float, float> &a, const std::pair<float, float> &b) { return a.first < b.first; });
  report.densityLow = weightedPercentile(densities, densityWeight, .1f);
  report.densityMedian = weightedPercentile(densities, densityWeight, .5f);
  report.densityHigh = weightedPercentile(densities, densityWeight, .9f);
  report.stretchRatio = report.densityLow > 0 ? report.densityHigh / report.densityLow : 0;
  out = report;
  return true;
}

} // namespace ae::renderer
