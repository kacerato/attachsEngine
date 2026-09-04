#include "renderer/lod_selection.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace ae::renderer {
namespace {
constexpr float kPi = 3.14159265358979323846f;
}

float computeScreenSpaceError(float geometricError, float distance, float fovYRadians,
                              float viewportHeightPx) {
  if (!std::isfinite(geometricError) || geometricError < 0.0f || !std::isfinite(distance) ||
      distance <= 0.0f || !std::isfinite(fovYRadians) || fovYRadians <= 0.0f || fovYRadians >= kPi ||
      !std::isfinite(viewportHeightPx) || viewportHeightPx <= 0.0f) {
    return std::numeric_limits<float>::infinity();
  }
  if (geometricError == 0.0f) return 0.0f;
  const float tangentHalfFov = std::tan(fovYRadians * 0.5f);
  if (!std::isfinite(tangentHalfFov) || tangentHalfFov <= 0.0f)
    return std::numeric_limits<float>::infinity();
  return geometricError * viewportHeightPx / (2.0f * distance * tangentHalfFov);
}

LodDitherPair encodeLodDither(float factor) {
  if (!std::isfinite(factor)) return {};
  const float safe = std::clamp(factor, 0.0f, 1.0f);
  return {safe, -safe};
}

LodSelection selectLodLevel(const LodLevelInfo *levels, u32 levelCount, float distance,
                            float fovYRadians, float viewportHeightPx, float pixelErrorBudget,
                            float hysteresisBandRatio, LodHysteresisState &state) {
  LodSelection result{};
  if (levels == nullptr || levelCount == 0 || !std::isfinite(pixelErrorBudget) ||
      pixelErrorBudget <= 0.0f || !std::isfinite(hysteresisBandRatio) || hysteresisBandRatio <= 0.0f ||
      hysteresisBandRatio > 1.0f) {
    return result; // Fail safe: finest level, no dither, state untouched.
  }

  auto errorAt = [&](u32 index) {
    return computeScreenSpaceError(levels[index].geometricError, distance, fovYRadians, viewportHeightPx);
  };
  // Largest index whose error still fits `budget`. Full scan (not an
  // early-break binary search) so a caller that violates the "non-decreasing
  // geometricError" contract degrades gracefully instead of picking a wildly
  // wrong level from a single out-of-order entry.
  auto coarsestFitting = [&](float budget) {
    u32 best = 0;
    for (u32 index = 0; index < levelCount; ++index) {
      if (errorAt(index) <= budget) best = std::max(best, index);
    }
    return best;
  };

  const u32 clampedCurrent = std::min(state.currentLevel, levelCount - 1);
  const float currentError = errorAt(clampedCurrent);
  u32 nextLevel;
  if (currentError > pixelErrorBudget) {
    // Current level reads as too coarse for this (closer) distance --
    // refine immediately. Quality never lags a frame behind distance.
    nextLevel = coarsestFitting(pixelErrorBudget);
  } else if (clampedCurrent + 1 < levelCount) {
    // Already within budget; only simplify further with extra margin, so an
    // object hovering exactly at the switch distance does not flicker.
    const u32 candidate = coarsestFitting(pixelErrorBudget * hysteresisBandRatio);
    nextLevel = candidate > clampedCurrent ? candidate : clampedCurrent;
  } else {
    nextLevel = clampedCurrent;
  }
  state.currentLevel = nextLevel;
  result.level = nextLevel;
  const float bandLow = pixelErrorBudget * hysteresisBandRatio;
  const float bandHigh = pixelErrorBudget;
  const float span = std::max(bandHigh - bandLow, 1.0e-6f);

  // When moving closer, hysteresis intentionally keeps a coarse level active
  // until its own error reaches bandHigh. Rendering only that coarse level in
  // the band made the reverse transition pop in one frame. Represent the same
  // fine/coarse pair and the same blend factor in either travel direction;
  // state.currentLevel still retains the hysteresis decision.
  if (nextLevel > 0) {
    const float selectedError = errorAt(nextLevel);
    if (selectedError >= bandLow && selectedError <= bandHigh) {
      result.level = nextLevel - 1;
      result.ditherToCoarserFactor =
          std::clamp((bandHigh - selectedError) / span, 0.0f, 1.0f);
      return result;
    }
  }

  if (nextLevel + 1 < levelCount) {
    // The neighbor becomes the active level once its OWN error drops to (or
    // below) bandLow -- errors shrink as distance grows, so moving farther
    // away is what makes a coarser level eventually "safe" to switch to.
    // The ramp therefore runs opposite to neighborError: high error (well
    // above bandHigh, object still relatively close) means "not
    // transitioning yet" (0); error at/under bandLow (object far enough
    // that the switch is about to happen or just did) means "fully
    // committed to the neighbor" (1).
    const float neighborError = errorAt(nextLevel + 1);
    result.ditherToCoarserFactor = std::clamp((bandHigh - neighborError) / span, 0.0f, 1.0f);
  }
  return result;
}

bool buildLodRenderGroups(std::span<const MapDrawRecord> draws,
                          std::span<const u32> candidateDrawIndices,
                          std::vector<LodRenderGroup> &outGroups,
                          std::vector<u32> &outUngrouped) {
  std::vector<LodRenderGroup> groups;
  std::vector<u32> ungrouped;
  std::map<u32, LodRenderGroup> byGroup;
  for (u32 drawIndex : candidateDrawIndices) {
    if (drawIndex >= draws.size()) return false;
    const MapDrawRecord &draw = draws[drawIndex];
    if (draw.lodLevel >= LodMaximumLevelsPerGroup ||
        !std::isfinite(draw.geometricError) || draw.geometricError < 0.0f ||
        !std::isfinite(draw.boundsRadius) || draw.boundsRadius < 0.0f ||
        !std::all_of(draw.boundsCenter, draw.boundsCenter + 3,
                     [](float value) { return std::isfinite(value); })) return false;
    LodRenderLevel &level = byGroup[draw.lodGroupId].levels[draw.lodLevel];
    if (level.drawIndices.empty()) {
      level.selection = {draw.lodLevel, draw.geometricError};
    } else if (level.selection.geometricError != draw.geometricError) {
      return false;
    }
    level.drawIndices.push_back(drawIndex);
  }

  groups.reserve(byGroup.size());
  ungrouped.reserve(candidateDrawIndices.size());
  for (auto &entry : byGroup) {
    LodRenderGroup &group = entry.second;
    while (group.levelCount < LodMaximumLevelsPerGroup &&
           !group.levels[group.levelCount].drawIndices.empty()) ++group.levelCount;
    for (u32 level = group.levelCount; level < LodMaximumLevelsPerGroup; ++level)
      if (!group.levels[level].drawIndices.empty()) return false;

    if (group.levelCount < 2) {
      ungrouped.insert(ungrouped.end(), group.levels[0].drawIndices.begin(),
                       group.levels[0].drawIndices.end());
      continue;
    }
    for (u32 level = 1; level < group.levelCount; ++level)
      if (group.levels[level].selection.geometricError <=
          group.levels[level - 1].selection.geometricError) return false;

    float minimum[3]{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                     std::numeric_limits<float>::max()};
    float maximum[3]{-minimum[0], -minimum[1], -minimum[2]};
    for (u32 level = 0; level < group.levelCount; ++level) {
      for (u32 drawIndex : group.levels[level].drawIndices) {
        const MapDrawRecord &draw = draws[drawIndex];
        for (u32 axis = 0; axis < 3; ++axis) {
          minimum[axis] = std::min(minimum[axis], draw.boundsCenter[axis] - draw.boundsRadius);
          maximum[axis] = std::max(maximum[axis], draw.boundsCenter[axis] + draw.boundsRadius);
        }
      }
    }
    for (u32 axis = 0; axis < 3; ++axis)
      group.boundsCenter[axis] = (minimum[axis] + maximum[axis]) * 0.5f;
    for (u32 level = 0; level < group.levelCount; ++level) {
      for (u32 drawIndex : group.levels[level].drawIndices) {
        const MapDrawRecord &draw = draws[drawIndex];
        float distanceSquared = 0.0f;
        for (u32 axis = 0; axis < 3; ++axis) {
          const float delta = draw.boundsCenter[axis] - group.boundsCenter[axis];
          distanceSquared += delta * delta;
        }
        group.boundsRadius = std::max(group.boundsRadius,
                                      std::sqrt(distanceSquared) + draw.boundsRadius);
      }
    }
    groups.push_back(std::move(group));
  }
  outGroups = std::move(groups);
  outUngrouped = std::move(ungrouped);
  return true;
}

void buildLodLevelZeroDrawOrder(std::span<const LodRenderGroup> groups,
                                std::span<const u32> ungrouped,
                                std::vector<u32> &outDrawOrder) {
  std::vector<u32> levelZero;
  usize groupedDrawCount = 0;
  for (const LodRenderGroup &group : groups)
    groupedDrawCount += group.levels[0].drawIndices.size();
  levelZero.reserve(ungrouped.size() + groupedDrawCount);
  levelZero.insert(levelZero.end(), ungrouped.begin(), ungrouped.end());
  for (const LodRenderGroup &group : groups) {
    const auto &draws = group.levels[0].drawIndices;
    levelZero.insert(levelZero.end(), draws.begin(), draws.end());
  }
  outDrawOrder = std::move(levelZero);
}

} // namespace ae::renderer
