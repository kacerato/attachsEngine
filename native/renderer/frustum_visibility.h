#pragma once

#include "core/base.h"

namespace ae::renderer {

// Backend-independent camera data used by scene visibility. The renderer
// builds this once per frame and every backend can consume the same result.
// Invalid input deliberately fails open: uncertain objects remain visible.
struct PerspectiveVisibilitySettings final {
  float verticalFieldOfViewRadians = 1.0471975512f; // 60 degrees.
  float nearPlane = 0.1f;
  float farPlane = 1000.0f;
  float boundsScale = 1.05f;
  float boundsMargin = 0.5f;
  bool enabled = true;
};

struct PerspectiveFrustum final {
  float cameraPosition[3]{};
  float yaw = 0.0f;
  float pitch = 0.0f;
  float tangentHalfHorizontal = 1.0f;
  float tangentHalfVertical = 1.0f;
  float nearPlane = 0.1f;
  float farPlane = 1000.0f;
  float boundsScale = 1.05f;
  float boundsMargin = 0.5f;
  bool valid = false;
};

PerspectiveFrustum buildPerspectiveFrustum(const float cameraPosition[3], float yaw,
                                           float pitch, float aspectRatio,
                                           const PerspectiveVisibilitySettings &settings = {});

// Tests a world-space bounding sphere against all six normalized frustum
// planes. The radius is expanded by the configured scale/margin to avoid
// visible popping from importer precision or animation drift.
bool isSphereVisible(const PerspectiveFrustum &frustum, const float center[3], float radius);

struct VisibilityTelemetry final {
  u32 candidateDraws = 0;
  u32 visibleDraws = 0;
  u32 culledDraws = 0;
  u32 submittedDrawCalls = 0;
  u64 candidateTriangles = 0;
  u64 visibleTriangles = 0;
  u64 submittedTriangles = 0;
};

} // namespace ae::renderer
