#include "runtime/transform_math.h"
#include "renderer/normal_matrix.h"

#include <algorithm>
#include <cmath>

namespace ae::runtime {

void multiplyMatrix(const float a[16], const float b[16], float out[16]) {
  float value[16]{};
  for (u32 c = 0; c < 4; ++c) for (u32 r = 0; r < 4; ++r)
    for (u32 k = 0; k < 4; ++k) value[c * 4 + r] += a[k * 4 + r] * b[c * 4 + k];
  std::copy(value, value + 16, out);
}

void transformMatrix(const Transform &t, float out[16]) {
  constexpr float radians = 0.0174532925199433f;
  const float x = t.rotationDegrees[0] * radians, y = t.rotationDegrees[1] * radians, z = t.rotationDegrees[2] * radians;
  const float cx = std::cos(x), sx = std::sin(x), cy = std::cos(y), sy = std::sin(y), cz = std::cos(z), sz = std::sin(z);
  const float matrix[16]{cy * cz, cy * sz, -sy, 0, sx * sy * cz - cx * sz, sx * sy * sz + cx * cz, sx * cy, 0,
    cx * sy * cz + sx * sz, cx * sy * sz - sx * cz, cx * cy, 0, t.position[0], t.position[1], t.position[2], 1};
  std::copy(matrix, matrix + 16, out);
  for (u32 c = 0; c < 3; ++c) for (u32 r = 0; r < 3; ++r) out[c * 4 + r] *= t.scale[c];
}

void transformRotationQuaternion(const Transform &t, float out[4]) {
  constexpr float half = 0.00872664626f; // graus -> radianos, dividido por dois
  const float x = t.rotationDegrees[0] * half, y = t.rotationDegrees[1] * half, z = t.rotationDegrees[2] * half;
  const float sx = std::sin(x), cx = std::cos(x), sy = std::sin(y), cy = std::cos(y), sz = std::sin(z), cz = std::cos(z);
  out[0] = sx * cy * cz - cx * sy * sz;
  out[1] = cx * sy * cz + sx * cy * sz;
  out[2] = cx * cy * sz - sx * sy * cz;
  out[3] = cx * cy * cz + sx * sy * sz;
}

bool worldMatrix(const SceneGraph &graph, ObjectId id, float out[16]) {
  const auto *object = graph.find(id);
  if (!object) return false;
  float accumulated[16];
  transformMatrix(object->transform, accumulated);
  for (u32 depth = 0; object->parent != kInvalidObject && depth < graph.entityCount(); ++depth) {
    object = graph.find(object->parent);
    if (!object) return false;
    float parent[16];
    transformMatrix(object->transform, parent);
    multiplyMatrix(parent, accumulated, accumulated);
  }
  for (u32 i = 0; i < 16; ++i) if (!std::isfinite(accumulated[i])) return false;
  std::copy(accumulated, accumulated + 16, out);
  return true;
}

bool parentWorldMatrix(const SceneGraph &graph, ObjectId id, float out[16]) {
  const auto *object = graph.find(id);
  if (!object) return false;
  if (object->parent == kInvalidObject) {
    float identity[16]{};
    identity[0] = identity[5] = identity[10] = identity[15] = 1;
    std::copy(identity, identity + 16, out);
    return true;
  }
  return worldMatrix(graph, object->parent, out);
}

bool localTransformForWorld(const float world[16], const float parent[16], Transform &out) {
  float normal[12];
  if (!renderer::buildNormalMatrix(parent, normal)) return false;
  float inverse[16]{};
  inverse[15] = 1;
  for (u32 r = 0; r < 3; ++r) for (u32 c = 0; c < 3; ++c) inverse[c * 4 + r] = normal[r * 4 + c];
  for (u32 r = 0; r < 3; ++r) for (u32 c = 0; c < 3; ++c) inverse[12 + r] -= inverse[c * 4 + r] * parent[12 + c];
  float local[16];
  multiplyMatrix(inverse, world, local);
  Transform value;
  for (u32 c = 0; c < 3; ++c) {
    value.position[c] = local[12 + c];
    value.scale[c] = std::sqrt(local[c * 4] * local[c * 4] + local[c * 4 + 1] * local[c * 4 + 1] + local[c * 4 + 2] * local[c * 4 + 2]);
    // Small positive scales are legitimate unit/dequantization transforms.
    // Singularity is zero, not an authoring-unit threshold such as 0.001.
    if (!std::isfinite(value.scale[c]) || value.scale[c] <= 0.0f) return false;
  }
  const float pitch = std::asin(std::clamp(-local[2] / value.scale[0], -1.0f, 1.0f));
  const bool pole = std::abs(std::cos(pitch)) < .00001f;
  constexpr float degrees = 57.29577951308232f;
  value.rotationDegrees[1] = pitch * degrees;
  value.rotationDegrees[0] = (pole ? std::atan2(-local[9] / value.scale[2], local[5] / value.scale[1])
                                   : std::atan2(local[6] / value.scale[1], local[10] / value.scale[2])) * degrees;
  value.rotationDegrees[2] = pole ? 0 : std::atan2(local[1] / value.scale[0], local[0] / value.scale[0]) * degrees;
  float reconstructed[16];
  transformMatrix(value, reconstructed);
  // Reject shear/reflection instead of silently losing the old world transform.
  for (u32 i = 0; i < 16; ++i)
    if (!std::isfinite(local[i]) || std::abs(local[i] - reconstructed[i]) > .0001f * std::max(1.0f, std::abs(local[i]))) return false;
  if (!isTransformValid(value)) return false;
  out = value;
  return true;
}

} // namespace ae::runtime
