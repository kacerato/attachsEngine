#include "renderer/frustum_visibility.h"

#include <algorithm>
#include <cmath>

namespace ae::renderer {

PerspectiveFrustum buildPerspectiveFrustum(const float cameraPosition[3], float yaw,
                                           float pitch, float aspectRatio,
                                           const PerspectiveVisibilitySettings &settings) {
  PerspectiveFrustum result{};
  if (cameraPosition == nullptr) return result;
  const float values[] = {cameraPosition[0], cameraPosition[1], cameraPosition[2], yaw,
                          pitch, aspectRatio, settings.verticalFieldOfViewRadians,
                          settings.nearPlane, settings.farPlane, settings.boundsScale,
                          settings.boundsMargin};
  for (float value : values) if (!std::isfinite(value)) return result;
  if (!settings.enabled || aspectRatio <= 0.0f || settings.nearPlane <= 0.0f ||
      settings.farPlane <= settings.nearPlane || settings.boundsScale < 1.0f ||
      settings.boundsMargin < 0.0f || settings.verticalFieldOfViewRadians <= 0.0f ||
      settings.verticalFieldOfViewRadians >= 3.14159265359f) return result;

  const float tangentHalfVertical = std::tan(settings.verticalFieldOfViewRadians * 0.5f);
  if (!std::isfinite(tangentHalfVertical) || tangentHalfVertical <= 0.0f) return result;
  std::copy(cameraPosition, cameraPosition + 3, result.cameraPosition);
  result.yaw = yaw;
  result.pitch = pitch;
  result.tangentHalfVertical = tangentHalfVertical;
  result.tangentHalfHorizontal = tangentHalfVertical * aspectRatio;
  result.nearPlane = settings.nearPlane;
  result.farPlane = settings.farPlane;
  result.boundsScale = settings.boundsScale;
  result.boundsMargin = settings.boundsMargin;
  result.valid = std::isfinite(result.tangentHalfHorizontal) &&
                 result.tangentHalfHorizontal > 0.0f;
  return result;
}

bool isSphereVisible(const PerspectiveFrustum &frustum, const float center[3], float radius) {
  if (!frustum.valid || center == nullptr || !std::isfinite(radius) || radius < 0.0f) return true;
  for (u32 axis = 0; axis < 3; ++axis) if (!std::isfinite(center[axis])) return true;

  const float deltaX = center[0] - frustum.cameraPosition[0];
  const float deltaY = center[1] - frustum.cameraPosition[1];
  const float deltaZ = center[2] - frustum.cameraPosition[2];
  const float cosineYaw = std::cos(frustum.yaw);
  const float sineYaw = std::sin(frustum.yaw);
  const float cosinePitch = std::cos(frustum.pitch);
  const float sinePitch = std::sin(frustum.pitch);

  // Exact CPU counterpart of transpose(dirtRoadCameraRotation()) in
  // dirt_road.vert. Forward is +Z in view space.
  const float yawX = cosineYaw * deltaX - sineYaw * deltaZ;
  const float yawZ = sineYaw * deltaX + cosineYaw * deltaZ;
  const float viewX = yawX;
  const float viewY = cosinePitch * deltaY + sinePitch * yawZ;
  const float viewZ = -sinePitch * deltaY + cosinePitch * yawZ;
  const float expandedRadius = radius * frustum.boundsScale + frustum.boundsMargin;

  if (viewZ + expandedRadius < frustum.nearPlane ||
      viewZ - expandedRadius > frustum.farPlane) return false;

  const float horizontalRadius = expandedRadius *
      std::sqrt(1.0f + frustum.tangentHalfHorizontal * frustum.tangentHalfHorizontal);
  if (viewX - viewZ * frustum.tangentHalfHorizontal > horizontalRadius ||
      -viewX - viewZ * frustum.tangentHalfHorizontal > horizontalRadius) return false;

  const float verticalRadius = expandedRadius *
      std::sqrt(1.0f + frustum.tangentHalfVertical * frustum.tangentHalfVertical);
  return viewY - viewZ * frustum.tangentHalfVertical <= verticalRadius &&
         -viewY - viewZ * frustum.tangentHalfVertical <= verticalRadius;
}

} // namespace ae::renderer
