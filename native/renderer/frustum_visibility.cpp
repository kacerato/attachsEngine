#include "renderer/frustum_visibility.h"
#include "renderer/camera_ray.h"

#include <algorithm>
#include <cmath>

namespace ae::renderer {

PerspectiveFrustum buildPerspectiveFrustum(const float cameraPosition[3], float yaw,
                                           float pitch, float aspectRatio,
                                           const PerspectiveVisibilitySettings &settings) {
  PerspectiveFrustum result{};
  if (cameraPosition == nullptr) return result;
  const float values[] = {cameraPosition[0], cameraPosition[1], cameraPosition[2], yaw,
                          pitch, aspectRatio,
                          settings.nearPlane, settings.farPlane, settings.boundsScale,
                          settings.boundsMargin,settings.roll};
  for (float value : values) if (!std::isfinite(value)) return result;
  if(settings.projection!=CameraProjection::Perspective && settings.projection!=CameraProjection::Orthographic) return result;
  if(settings.projection==CameraProjection::Orthographic &&
     (!std::isfinite(settings.orthographicHalfHeight)||settings.orthographicHalfHeight<=0)) return result;
  if (!settings.enabled || aspectRatio <= 0.0f || settings.nearPlane <= 0.0f ||
      settings.farPlane <= settings.nearPlane || settings.boundsScale < 1.0f ||
      settings.boundsMargin < 0.0f) return result;
  if(settings.projection==CameraProjection::Perspective &&
     (!std::isfinite(settings.verticalFieldOfViewRadians)||settings.verticalFieldOfViewRadians<=0 ||
      settings.verticalFieldOfViewRadians>=3.14159265359f)) return result;

  const float tangentHalfVertical = settings.projection==CameraProjection::Orthographic?1.f:
      std::tan(settings.verticalFieldOfViewRadians * 0.5f);
  if (!std::isfinite(tangentHalfVertical) || tangentHalfVertical <= 0.0f) return result;
  std::copy(cameraPosition, cameraPosition + 3, result.cameraPosition);
  result.yaw = yaw;
  result.pitch = pitch;
  result.roll = settings.roll;
  result.projection=settings.projection;
  result.orthographicHalfHeight=settings.orthographicHalfHeight;
  result.orthographicHalfWidth=settings.orthographicHalfHeight*aspectRatio;
  if(settings.projection==CameraProjection::Orthographic && !std::isfinite(result.orthographicHalfWidth)) return result;
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
  const float unrolledY=cosinePitch * deltaY + sinePitch * yawZ;
  const float viewX = std::cos(frustum.roll)*yawX+std::sin(frustum.roll)*unrolledY;
  const float viewY = -std::sin(frustum.roll)*yawX+std::cos(frustum.roll)*unrolledY;
  const float viewZ = -sinePitch * deltaY + cosinePitch * yawZ;
  const float expandedRadius = radius * frustum.boundsScale + frustum.boundsMargin;

  if (viewZ + expandedRadius < frustum.nearPlane ||
      viewZ - expandedRadius > frustum.farPlane) return false;

  if(isOrthographic(frustum))
    return std::abs(viewX)<=frustum.orthographicHalfWidth+expandedRadius &&
           std::abs(viewY)<=frustum.orthographicHalfHeight+expandedRadius;

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
