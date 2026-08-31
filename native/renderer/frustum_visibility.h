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
  // HZB occlusion stage (see renderer/hzb_visibility.h), applied only to
  // draws that already survived frustum culling above -- hzbOccludedDraws is
  // therefore already counted inside culledDraws, not additional to it.
  // Zero on every frame HZB occlusion is disabled or not yet initialized.
  u32 hzbTestedDraws = 0;
  u32 hzbOccludedDraws = 0;
  // Objects whose occluded streak reset to zero this frame (an instant
  // revive, never delayed -- see updateHzbHysteresis). A large count relative
  // to hzbTestedDraws would flag thrashing at the hysteresis boundary.
  u32 hzbRevivedDraws = 0;
  // Temporal CPU-readback HZB is only conservative when it represents the
  // same camera pose. Until a same-frame GPU culling path exists, moving
  // camera frames fail open and report how many candidates skipped HZB.
  u32 hzbSkippedCameraMotionDraws = 0;
  // Candidates deliberately left visible because the current workload is
  // below the global fixed-cost threshold for CPU-readback HZB.
  u32 hzbSkippedBudgetDraws = 0;
};

} // namespace ae::renderer
