#pragma once

#include "core/base.h"
#include "renderer/map_package.h"

#include <array>
#include <span>
#include <vector>

namespace ae::renderer {

// The most levels one lodGroupId can ever have: tools/cook-gltf-map.py's
// MAX_LOD_LEVELS simplification levels plus the baked impostor level that
// tools/bake-foliage-impostors.py appends (see MapMaximumLodLevels). Callers
// size fixed-capacity per-group buffers (LodLevelInfo arrays, etc.) with this
// instead of allocating per frame.
inline constexpr u32 LodMaximumLevelsPerGroup = MapMaximumLodLevels;

// LOD selection by projected screen-space error -- the same metric used by
// 3D Tiles/Cesium and Simplygon: a mesh's geometric simplification error (a
// world-space distance, produced once at import time -- see
// tools/cook-gltf-map.py) is projected through the camera to estimate how
// many PIXELS of visual error that simplification introduces at the current
// distance. Runtime selection never touches geometry or triangle counts
// directly; it only compares this one projected number against a budget.
//
// screenSpaceError = geometricError * viewportHeightPx / (2 * distance * tan(fovY/2))
//
// Fails safe throughout: degenerate input (non-finite/non-positive distance,
// FOV outside (0,pi), non-positive viewport/budget) returns +infinity from
// computeScreenSpaceError (i.e. "treat as maximally coarse, prefer the
// finest level") and level 0 with zero dither from selectLodLevel, matching
// legacy (pre-LOD) content, which always decodes with geometricError=0.

float computeScreenSpaceError(float geometricError, float distance, float fovYRadians,
                              float viewportHeightPx);

struct LodLevelInfo final {
  u32 level = 0;
  // Must be non-decreasing as this array's index increases -- index 0 is
  // conventionally the finest level (geometricError == 0 for legacy/no-LOD
  // content). Enforced at import time by tools/cook-gltf-map.py's build-time
  // checks, not re-validated per frame here (this is a hot path).
  float geometricError = 0.0f;
};

struct LodHysteresisState final {
  u32 currentLevel = 0;
};

struct LodSelection final {
  u32 level = 0;
  // [0,1] blend factor toward levels[selectedIndex + 1] (the next coarser
  // neighbor), 0 when not near a transition. The caller encodes +factor on
  // the outgoing geometry and -factor on the incoming geometry so the shader
  // produces complementary Bayer coverage with no holes or double shading.
  float ditherToCoarserFactor = 0.0f;
};

struct LodDitherPair final {
  float outgoing = 0.0f;
  float incoming = 0.0f;
};

// Signed shader encoding for a transition. Positive outgoing and negative
// incoming values use complementary halves of the same Bayer threshold.
LodDitherPair encodeLodDither(float factor);

// Selects a level from `levels` (levelCount > 0, sorted ascending by
// geometricError, index 0 the finest) for the given camera distance, with an
// asymmetric hysteresis band around pixelErrorBudget: refining to a finer
// level happens immediately whenever the current level's own error exceeds
// budget (quality never lags), but simplifying to a coarser level requires
// the candidate's error to fit within
// pixelErrorBudget * hysteresisBandRatio (hysteresisBandRatio in (0,1]) --
// extra margin that prevents flicker exactly at the switch distance.
// Mutates state.currentLevel to persist the selection across frames; the
// same state must be reused for the same lodGroupId every frame (see
// MapDrawRecord::lodGroupId).
LodSelection selectLodLevel(const LodLevelInfo *levels, u32 levelCount, float distance,
                            float fovYRadians, float viewportHeightPx, float pixelErrorBudget,
                            float hysteresisBandRatio, LodHysteresisState &state);

struct LodRenderLevel final {
  LodLevelInfo selection{};
  std::vector<u32> drawIndices;
};

struct LodRenderGroup final {
  std::array<LodRenderLevel, LodMaximumLevelsPerGroup> levels{};
  u32 levelCount = 0;
  float boundsCenter[3]{};
  float boundsRadius = 0.0f;
  LodHysteresisState hysteresis{};
};

// Builds backend-independent runtime groups after spatial chunking. One
// imported LOD level may contain many render chunks; all of them must switch
// together while remaining individually cullable. Single-level/legacy draws
// are returned in outUngrouped and incur no per-frame LOD selection.
bool buildLodRenderGroups(std::span<const MapDrawRecord> draws,
                          std::span<const u32> candidateDrawIndices,
                          std::vector<LodRenderGroup> &outGroups,
                          std::vector<u32> &outUngrouped);

// Builds the maximum-quality candidate list for a package containing multiple
// discrete levels. Disabling adaptive LOD selects this list; it must never
// fall back to every package draw, which would render duplicate surfaces.
void buildLodLevelZeroDrawOrder(std::span<const LodRenderGroup> groups,
                                std::span<const u32> ungrouped,
                                std::vector<u32> &outDrawOrder);

} // namespace ae::renderer
