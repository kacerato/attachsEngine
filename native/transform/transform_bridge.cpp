#include "transform/transform_bridge.h"

#include <cmath>
#include <cstddef>
#include <type_traits>

static_assert(std::is_standard_layout_v<AetherTransform>);
static_assert(sizeof(AetherTransform) == 40);
static_assert(offsetof(AetherTransform, position) == 0);
static_assert(offsetof(AetherTransform, rotation) == 12);
static_assert(offsetof(AetherTransform, scale) == 28);
static_assert(std::is_standard_layout_v<AetherTransformPlanEntry>);
static_assert(sizeof(AetherTransformPlanEntry) == sizeof(void *) * 2 + 8);
static_assert(offsetof(AetherTransformPlanEntry, local) == 0);
static_assert(offsetof(AetherTransformPlanEntry, world) == sizeof(void *));
static_assert(offsetof(AetherTransformPlanEntry, parent) == sizeof(void *) * 2);

namespace {

inline bool isIdentityRotation(const float *q) {
  return q[0] == 0.0F && q[1] == 0.0F && q[2] == 0.0F && q[3] == 1.0F;
}

inline void normalize(const float *q, float *result) {
  const float lengthSquared =
      q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];
  if (lengthSquared < 1.0e-24F) {
    result[0] = 0.0F;
    result[1] = 0.0F;
    result[2] = 0.0F;
    result[3] = 1.0F;
    return;
  }
  if (std::abs(lengthSquared - 1.0F) <= 1.0e-6F) {
    result[0] = q[0];
    result[1] = q[1];
    result[2] = q[2];
    result[3] = q[3];
    return;
  }
  const float inverseLength = 1.0F / std::sqrt(lengthSquared);
  result[0] = q[0] * inverseLength;
  result[1] = q[1] * inverseLength;
  result[2] = q[2] * inverseLength;
  result[3] = q[3] * inverseLength;
}

inline void multiplyQuaternion(const float *a, const float *b, float *result) {
  const float product[4] = {
      a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
      a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
      a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
      a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2],
  };
  normalize(product, result);
}

inline void rotateVector(const float *q, const float *v, float *result) {
  // Mesma forma de quaternion * float3 usada em Aether.Core: v + w*t + cross(u,t),
  // onde t = 2*cross(u,v). Evita construir matrizes temporárias.
  const float tx = 2.0F * (q[1] * v[2] - q[2] * v[1]);
  const float ty = 2.0F * (q[2] * v[0] - q[0] * v[2]);
  const float tz = 2.0F * (q[0] * v[1] - q[1] * v[0]);
  result[0] = v[0] + q[3] * tx + (q[1] * tz - q[2] * ty);
  result[1] = v[1] + q[3] * ty + (q[2] * tx - q[0] * tz);
  result[2] = v[2] + q[3] * tz + (q[0] * ty - q[1] * tx);
}

inline void compose(const AetherTransform &parent, const AetherTransform &child,
                    AetherTransform &result) {
  const float scaledPosition[3] = {
      parent.scale[0] * child.position[0],
      parent.scale[1] * child.position[1],
      parent.scale[2] * child.position[2],
  };
  float rotatedPosition[3];
  if (isIdentityRotation(parent.rotation)) {
    rotatedPosition[0] = scaledPosition[0];
    rotatedPosition[1] = scaledPosition[1];
    rotatedPosition[2] = scaledPosition[2];
    normalize(child.rotation, result.rotation);
  } else {
    rotateVector(parent.rotation, scaledPosition, rotatedPosition);
    multiplyQuaternion(parent.rotation, child.rotation, result.rotation);
  }

  result.position[0] = parent.position[0] + rotatedPosition[0];
  result.position[1] = parent.position[1] + rotatedPosition[1];
  result.position[2] = parent.position[2] + rotatedPosition[2];
  result.scale[0] = parent.scale[0] * child.scale[0];
  result.scale[1] = parent.scale[1] * child.scale[1];
  result.scale[2] = parent.scale[2] * child.scale[2];
}

} // namespace

std::int32_t AetherTransform_Propagate(const AetherTransform *local,
                                       const std::int32_t *parents,
                                       AetherTransform *world,
                                       std::int32_t count) {
  if (count < 0 || (count > 0 && (local == nullptr || parents == nullptr || world == nullptr)))
    return 0;

  for (std::int32_t i = 0; i < count; ++i) {
    const std::int32_t parent = parents[i];
    if (parent < -1 || parent >= i)
      return 0;
    if (parent < 0) {
      world[i] = local[i];
    } else {
      compose(world[parent], local[i], world[i]);
    }
  }
  return 1;
}

std::int32_t
AetherTransform_PropagatePlan(const AetherTransformPlanEntry *entries,
                              std::int32_t count) {
  if (count < 0 || (count > 0 && entries == nullptr))
    return 0;

  for (std::int32_t i = 0; i < count; ++i) {
    const AetherTransformPlanEntry &entry = entries[i];
    if (entry.local == nullptr || entry.world == nullptr || entry.parent < -1 ||
        entry.parent >= i)
      return 0;
    if (entry.parent < 0) {
      *entry.world = *entry.local;
    } else {
      const AetherTransform *parentWorld = entries[entry.parent].world;
      if (parentWorld == nullptr)
        return 0;
      compose(*parentWorld, *entry.local, *entry.world);
    }
  }
  return 1;
}
