#pragma once

#include <algorithm>
#include <cmath>

namespace ae::renderer {

// Conservatively classifies a bounded render packet for a distance-material
// variant. Invalid data fails to the near/full-quality path. The test uses the
// nearest point of the sphere, so selecting a reduced shader can never remove
// detail from a fragment inside maximumDistance.
inline bool boundsEntirelyPastDistance(const float camera[3], const float center[3],
                                       float radius, float maximumDistance) noexcept {
  if (camera == nullptr || center == nullptr || !std::isfinite(radius) || radius < 0.0f ||
      !std::isfinite(maximumDistance) || maximumDistance <= 0.0f) return false;
  float distanceSquared = 0.0f;
  for (int axis = 0; axis < 3; ++axis) {
    if (!std::isfinite(camera[axis]) || !std::isfinite(center[axis])) return false;
    const float delta = center[axis] - camera[axis];
    distanceSquared += delta * delta;
  }
  if (!std::isfinite(distanceSquared)) return false;
  const float nearest = std::max(0.0f, std::sqrt(distanceSquared) - radius);
  return nearest > maximumDistance;
}

} // namespace ae::renderer
