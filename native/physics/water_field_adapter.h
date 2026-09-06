#pragma once

#include "physics/water_buoyancy.h"
#include "renderer/water_field.h"

#include <cmath>

namespace ae::physics {

// Integration edge only: neither the Jolt backend nor WaterField depends on
// the other. Caller queries in batches at fixed-step time and supplies shape
// metadata. Invalid/dry samples leave the destination unchanged.
inline bool makeWaterBodySample(AetherBodyHandle body, const BuoyantShape &shape,
                                renderer::WaterVec2 position,
                                const renderer::WaterFieldSample &field, float referenceArea,
                                AetherWaterBodySample &out) noexcept {
  using renderer::WaterFieldFlag;
  if (body == AetherBodyHandle_Invalid ||
      !renderer::hasWaterFieldFlag(field.flags, WaterFieldFlag::Valid) ||
      renderer::hasWaterFieldFlag(field.flags, WaterFieldFlag::Excluded) ||
      !(field.coverage > 0) || !std::isfinite(field.coverage) ||
      !std::isfinite(position.x) || !std::isfinite(position.y) ||
      !std::isfinite(field.height) || !std::isfinite(referenceArea) || referenceArea < 0 ||
      (shape.kind != BuoyantShapeKind::Box && shape.kind != BuoyantShapeKind::Sphere) ||
      !(buoyantShapeVolume(shape) > 0)) return false;
  const auto &n = field.normal;
  const float length = std::hypot(n.x, n.y, n.z);
  if (!std::isfinite(length) || length < 1e-6f || n.y <= 0) return false;
  AetherWaterBodySample result{};
  result.body = body;
  result.shapeKind = static_cast<u32>(shape.kind);
  result.halfExtent = shape.halfExtent;
  result.planeNormal = {n.x / length, n.y / length, n.z / length};
  result.planeOffset = -(result.planeNormal.x * position.x +
                         result.planeNormal.y * field.height + result.planeNormal.z * position.y);
  result.waterVelocity = {field.velocity.x + field.flow.x, field.velocity.y,
                          field.velocity.z + field.flow.y};
  result.referenceArea = referenceArea;
  if (!std::isfinite(result.planeOffset) || !std::isfinite(result.waterVelocity.x) ||
      !std::isfinite(result.waterVelocity.y) || !std::isfinite(result.waterVelocity.z)) return false;
  out = result;
  return true;
}

} // namespace ae::physics
