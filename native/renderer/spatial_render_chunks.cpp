#include "renderer/spatial_render_chunks.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace ae::renderer {
namespace {

struct Point final { float x, y, z; };
struct TriangleRef final {
  u32 sourceOffset = 0;
  u32 morton = 0;
  Point centroid{};
};

bool vertexPosition(const MapPackageView &package, const MapDrawRecord &draw,
                    u32 sourceIndex, Point &out) {
  const u64 vertexIndex = static_cast<u64>(sourceIndex) + draw.vertexOffset;
  if (vertexIndex >= package.header.vertexCount) return false;
  float local[3]{};
  std::memcpy(local, package.vertices.data() + vertexIndex * package.header.vertexStride,
              sizeof(local));
  out.x = draw.model[0] * local[0] + draw.model[4] * local[1] +
          draw.model[8] * local[2] + draw.model[12];
  out.y = draw.model[1] * local[0] + draw.model[5] * local[1] +
          draw.model[9] * local[2] + draw.model[13];
  out.z = draw.model[2] * local[0] + draw.model[6] * local[1] +
          draw.model[10] * local[2] + draw.model[14];
  return std::isfinite(out.x) && std::isfinite(out.y) && std::isfinite(out.z);
}

u32 quantize(float value, float minimum, float maximum) {
  const float extent = maximum - minimum;
  if (!(extent > 1.0e-6f)) return 0;
  return static_cast<u32>(std::clamp((value - minimum) / extent, 0.0f, 1.0f) *
                          1023.0f + 0.5f);
}

u32 morton3(u32 x, u32 y, u32 z) {
  u32 code = 0;
  for (u32 bit = 0; bit < 10; ++bit) {
    code |= ((x >> bit) & 1u) << (bit * 3u);
    code |= ((y >> bit) & 1u) << (bit * 3u + 1u);
    code |= ((z >> bit) & 1u) << (bit * 3u + 2u);
  }
  return code;
}

void expand(Point point, Point &minimum, Point &maximum) {
  minimum.x = std::min(minimum.x, point.x); minimum.y = std::min(minimum.y, point.y);
  minimum.z = std::min(minimum.z, point.z); maximum.x = std::max(maximum.x, point.x);
  maximum.y = std::max(maximum.y, point.y); maximum.z = std::max(maximum.z, point.z);
}

bool appendChunk(const MapPackageView &package, const MapDrawRecord &source,
                 const TriangleRef *triangles, u32 triangleCount,
                 float waterDisplacementAllowance, SpatialRenderChunks &result) {
  const u32 firstIndex = static_cast<u32>(result.indices.size());
  Point minimum{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max()};
  Point maximum{-minimum.x, -minimum.y, -minimum.z};
  for (u32 triangle = 0; triangle < triangleCount; ++triangle) {
    const u32 offset = source.firstIndex + triangles[triangle].sourceOffset;
    for (u32 corner = 0; corner < 3; ++corner) {
      const u32 index = package.indices[offset + corner];
      Point world{};
      if (!vertexPosition(package, source, index, world)) return false;
      result.indices.push_back(index);
      expand(world, minimum, maximum);
    }
  }
  MapDrawRecord chunk = source;
  chunk.firstIndex = firstIndex;
  chunk.indexCount = triangleCount * 3;
  chunk.boundsCenter[0] = (minimum.x + maximum.x) * 0.5f;
  chunk.boundsCenter[1] = (minimum.y + maximum.y) * 0.5f;
  chunk.boundsCenter[2] = (minimum.z + maximum.z) * 0.5f;
  float radiusSquared = 0.0f;
  for (u32 index = firstIndex; index < result.indices.size(); ++index) {
    Point world{};
    if (!vertexPosition(package, source, result.indices[index], world)) return false;
    const float dx = world.x - chunk.boundsCenter[0];
    const float dy = world.y - chunk.boundsCenter[1];
    const float dz = world.z - chunk.boundsCenter[2];
    radiusSquared = std::max(radiusSquared, dx * dx + dy * dy + dz * dz);
  }
  chunk.boundsRadius = std::sqrt(radiusSquared);
  const u32 materialFlags = package.materials[source.materialIndex].flags;
  if ((materialFlags & MapMaterialWater) != 0)
    chunk.boundsRadius += waterDisplacementAllowance;
  if ((materialFlags & MapMaterialImpostor) != 0) {
    // Shader-rotated geometry needs swept bounds, including nonuniform scale.
    float localRadiusSquared = 0.0f;
    for (u32 index = firstIndex; index < result.indices.size(); ++index) {
      const u64 vertex = static_cast<u64>(result.indices[index]) + source.vertexOffset;
      float p[3]{};
      std::memcpy(p, package.vertices.data() + vertex * package.header.vertexStride, sizeof(p));
      localRadiusSquared = std::max(localRadiusSquared, p[0]*p[0] + p[1]*p[1] + p[2]*p[2]);
    }
    // sqrt(||M||_1 * ||M||_inf) bounds the spectral norm, even with shear.
    float maximumRow = 0.0f, maximumColumn = 0.0f;
    for (u32 axis = 0; axis < 3; ++axis) {
      float row = 0.0f, column = 0.0f;
      for (u32 other = 0; other < 3; ++other) {
        row += std::abs(source.model[other * 4 + axis]);
        column += std::abs(source.model[axis * 4 + other]);
      }
      maximumRow = std::max(maximumRow, row);
      maximumColumn = std::max(maximumColumn, column);
      chunk.boundsCenter[axis] = source.model[12 + axis];
    }
    chunk.boundsRadius = std::sqrt(localRadiusSquared * maximumRow * maximumColumn);
  }
  result.draws.push_back(chunk);
  return true;
}

} // namespace

bool buildSpatialRenderChunks(const MapPackageView &package,
                              const SpatialRenderChunkSettings &settings,
                              SpatialRenderChunks &out) {
  if (settings.opaqueTrianglesPerChunk == 0 || settings.coverageTrianglesPerChunk == 0 ||
      !std::isfinite(settings.waterDisplacementAllowance) ||
      settings.waterDisplacementAllowance < 0.0f ||
      package.draws.empty() ||
      package.indices.empty() || package.vertices.empty()) return false;
  SpatialRenderChunks prepared;
  prepared.indices.reserve(package.indices.size());
  prepared.draws.reserve(package.draws.size() +
                         package.header.triangleCount /
                             std::min(settings.opaqueTrianglesPerChunk,
                                      settings.coverageTrianglesPerChunk));
  std::vector<TriangleRef> triangles;
  for (const MapDrawRecord &draw : package.draws) {
    if (draw.indexCount % 3 != 0 || draw.materialIndex >= package.materials.size()) return false;
    const u32 triangleCount = draw.indexCount / 3;
    const u32 materialFlags = package.materials[draw.materialIndex].flags;
    const bool blended = (materialFlags & MapMaterialBlend) != 0;
    // Static spatial bounds cannot cull camera-relative grids. Splitting them
    // only adds draws and reorders an already coherent mesh without visibility
    // savings. Finite water remains eligible for normal spatial partitioning.
    const bool cameraWater = (materialFlags & (MapMaterialWater | MapMaterialWaterCameraGrid)) ==
                            (MapMaterialWater | MapMaterialWaterCameraGrid);
    const bool preservePrimitive = blended || cameraWater;
    const bool coverage = (materialFlags & MapMaterialAlphaMask) != 0;
    const u32 targetTriangles = coverage ? settings.coverageTrianglesPerChunk
                                         : settings.opaqueTrianglesPerChunk;
    triangles.clear();
    triangles.resize(triangleCount);
    Point minimum{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::max()};
    Point maximum{-minimum.x, -minimum.y, -minimum.z};
    for (u32 triangle = 0; triangle < triangleCount; ++triangle) {
      TriangleRef &ref = triangles[triangle];
      ref.sourceOffset = triangle * 3;
      Point corners[3]{};
      for (u32 corner = 0; corner < 3; ++corner) {
        if (!vertexPosition(package, draw,
                            package.indices[draw.firstIndex + ref.sourceOffset + corner],
                            corners[corner])) return false;
      }
      ref.centroid = {(corners[0].x + corners[1].x + corners[2].x) / 3.0f,
                      (corners[0].y + corners[1].y + corners[2].y) / 3.0f,
                      (corners[0].z + corners[1].z + corners[2].z) / 3.0f};
      expand(ref.centroid, minimum, maximum);
    }
    if (!preservePrimitive && triangleCount > targetTriangles) {
      for (TriangleRef &ref : triangles) {
        ref.morton = morton3(quantize(ref.centroid.x, minimum.x, maximum.x),
                             quantize(ref.centroid.y, minimum.y, maximum.y),
                             quantize(ref.centroid.z, minimum.z, maximum.z));
      }
      std::sort(triangles.begin(), triangles.end(), [](const TriangleRef &left,
                                                        const TriangleRef &right) {
        return left.morton != right.morton ? left.morton < right.morton
                                           : left.sourceOffset < right.sourceOffset;
      });
    }
    for (u32 first = 0; first < triangleCount;) {
      const u32 count = preservePrimitive ? triangleCount :
          std::min(targetTriangles, triangleCount - first);
      if (!appendChunk(package, draw, triangles.data() + first, count,
                       settings.waterDisplacementAllowance, prepared)) return false;
      first += count;
    }
  }
  if (prepared.indices.size() != package.indices.size() || prepared.draws.empty()) return false;
  out = std::move(prepared);
  return true;
}

} // namespace ae::renderer
