#include "harness.h"
#include "renderer/lod_selection.h"

#include <cmath>
#include <limits>
#include <vector>

using namespace ae;
using namespace ae::renderer;

namespace {
constexpr float kHalfPi = 1.57079632679f; // fovY = pi/2 -> tan(fovY/2) = tan(pi/4) = 1, round numbers.
}

AE_TEST(Lod_screen_space_error_matches_known_values_and_zero_error_is_always_zero) {
  // tan(fovY/2)=1, viewportHeightPx=1000 -> error = geometricError*500/distance.
  const float error = computeScreenSpaceError(1.0f, 50.0f, kHalfPi, 1000.0f);
  AE_EXPECT_TRUE(error > 9.9f && error < 10.1f, "500/50 = 10 pixels");
  AE_EXPECT_TRUE(computeScreenSpaceError(0.0f, 1.0f, kHalfPi, 1000.0f) == 0.0f,
                 "zero geometric error is always zero screen-space error");
  const float doubled = computeScreenSpaceError(1.0f, 25.0f, kHalfPi, 1000.0f);
  AE_EXPECT_TRUE(doubled > error * 1.9f && doubled < error * 2.1f, "error doubles as distance halves");
}

AE_TEST(Lod_screen_space_error_fails_safe_to_infinity_for_invalid_input) {
  const float infinity = std::numeric_limits<float>::infinity();
  AE_EXPECT_TRUE(computeScreenSpaceError(1.0f, 0.0f, kHalfPi, 1000.0f) == infinity, "zero distance");
  AE_EXPECT_TRUE(computeScreenSpaceError(1.0f, -5.0f, kHalfPi, 1000.0f) == infinity, "negative distance");
  AE_EXPECT_TRUE(computeScreenSpaceError(-1.0f, 10.0f, kHalfPi, 1000.0f) == infinity, "negative geometric error");
  AE_EXPECT_TRUE(computeScreenSpaceError(std::numeric_limits<float>::quiet_NaN(), 10.0f, kHalfPi, 1000.0f) ==
                     infinity,
                 "NaN geometric error");
  AE_EXPECT_TRUE(computeScreenSpaceError(1.0f, 10.0f, 0.0f, 1000.0f) == infinity, "zero FOV");
  AE_EXPECT_TRUE(computeScreenSpaceError(1.0f, 10.0f, kHalfPi, 0.0f) == infinity, "zero viewport height");
}

namespace {
// Three-level chain matching the worked example in the review: level 0 is
// free (geometricError=0, always finest), level 1 needs distance>=50 to fit
// a budget of 10 (or >=100 for the hysteresis-reduced budget of 5), level 2
// needs distance>=200 (or >=400 reduced).
const LodLevelInfo kThreeLevels[3] = {{0, 0.0f}, {1, 1.0f}, {2, 4.0f}};
constexpr float kBudget = 10.0f;
constexpr float kHysteresisRatio = 0.5f;
constexpr float kViewportHeight = 1000.0f;
}

AE_TEST(Lod_selection_picks_finest_level_up_close) {
  LodHysteresisState state{};
  const LodSelection selection =
      selectLodLevel(kThreeLevels, 3, 30.0f, kHalfPi, kViewportHeight, kBudget, kHysteresisRatio, state);
  AE_EXPECT_EQ(selection.level, 0u, "too close for any coarser level to clear the reduced budget");
  AE_EXPECT_EQ(state.currentLevel, 0u, "state mirrors the selection");
}

AE_TEST(Lod_selection_coarsens_once_comfortably_past_the_reduced_budget_distance) {
  LodHysteresisState state{};
  const LodSelection selection =
      selectLodLevel(kThreeLevels, 3, 150.0f, kHalfPi, kViewportHeight, kBudget, kHysteresisRatio, state);
  AE_EXPECT_EQ(selection.level, 1u, "150 clears level 1's reduced-budget distance (100) but not level 2's (400)");
}

AE_TEST(Lod_selection_hysteresis_keeps_current_level_inside_the_band) {
  LodHysteresisState state{};
  // Reach level 1 at distance 150 (as above).
  selectLodLevel(kThreeLevels, 3, 150.0f, kHalfPi, kViewportHeight, kBudget, kHysteresisRatio, state);
  // Move closer to 60: a cold selection at 60 alone would pick level 0 (60 <
  // level 1's reduced-budget distance of 100), but level 1's OWN error at 60
  // (500/60 ≈ 8.33) still fits the full (non-reduced) budget of 10, so
  // hysteresis must keep it at level 1 rather than flicker back to 0.
  const LodSelection stillCoarse =
      selectLodLevel(kThreeLevels, 3, 60.0f, kHalfPi, kViewportHeight, kBudget, kHysteresisRatio, state);
  AE_EXPECT_EQ(stillCoarse.level, 1u, "hysteresis band prevents flicker back to finer");

  // Move closer still to 45: level 1's error (500/45 ≈ 11.1) now exceeds the
  // FULL budget -- refinement is immediate, no hysteresis on the way finer.
  const LodSelection refined =
      selectLodLevel(kThreeLevels, 3, 45.0f, kHalfPi, kViewportHeight, kBudget, kHysteresisRatio, state);
  AE_EXPECT_EQ(refined.level, 0u, "refining to a finer level never waits for a margin");
}

AE_TEST(Lod_selection_dither_ramps_toward_the_coarser_neighbor_and_resets_after_switching) {
  LodHysteresisState state{};
  selectLodLevel(kThreeLevels, 3, 150.0f, kHalfPi, kViewportHeight, kBudget, kHysteresisRatio, state); // -> level 1
  const LodSelection farFromSwitch =
      selectLodLevel(kThreeLevels, 3, 150.0f, kHalfPi, kViewportHeight, kBudget, kHysteresisRatio, state);
  AE_EXPECT_TRUE(farFromSwitch.ditherToCoarserFactor == 0.0f,
                 "level 2 still needs distance 400 to enter its own band; no dither yet");

  const LodSelection approaching =
      selectLodLevel(kThreeLevels, 3, 350.0f, kHalfPi, kViewportHeight, kBudget, kHysteresisRatio, state);
  AE_EXPECT_EQ(approaching.level, 1u, "350 has not yet reached level 2's switch distance of 400");
  AE_EXPECT_TRUE(approaching.ditherToCoarserFactor > 0.8f && approaching.ditherToCoarserFactor < 1.0f,
                 "close to the switch distance: dither ramps toward 1 but has not hit it yet");

  const LodSelection switched =
      selectLodLevel(kThreeLevels, 3, 400.0f, kHalfPi, kViewportHeight, kBudget, kHysteresisRatio, state);
  AE_EXPECT_EQ(switched.level, 2u, "distance 400 is exactly level 2's switch point");
  AE_EXPECT_TRUE(switched.ditherToCoarserFactor == 0.0f, "no coarser neighbor exists past the last level");
}

AE_TEST(Lod_dither_encoding_is_signed_and_complementary) {
  const LodDitherPair pair = encodeLodDither(0.25f);
  AE_EXPECT_TRUE(pair.outgoing == 0.25f && pair.incoming == -0.25f,
                 "same threshold is encoded with opposite roles");
  const LodDitherPair clamped = encodeLodDither(2.0f);
  AE_EXPECT_TRUE(clamped.outgoing == 1.0f && clamped.incoming == -1.0f,
                 "out-of-range factor is clamped safely");
  const LodDitherPair invalid = encodeLodDither(std::numeric_limits<float>::quiet_NaN());
  AE_EXPECT_TRUE(invalid.outgoing == 0.0f && invalid.incoming == 0.0f,
                 "non-finite factor disables transition");
}

AE_TEST(Lod_selection_fails_safe_for_invalid_input_without_mutating_state) {
  LodHysteresisState state{};
  state.currentLevel = 2;
  const LodSelection nullLevels =
      selectLodLevel(nullptr, 3, 100.0f, kHalfPi, kViewportHeight, kBudget, kHysteresisRatio, state);
  AE_EXPECT_EQ(nullLevels.level, 0u, "null levels array fails safe to level 0");
  AE_EXPECT_EQ(state.currentLevel, 2u, "state untouched on failure");

  const LodSelection zeroCount =
      selectLodLevel(kThreeLevels, 0, 100.0f, kHalfPi, kViewportHeight, kBudget, kHysteresisRatio, state);
  AE_EXPECT_EQ(zeroCount.level, 0u, "zero level count fails safe");

  const LodSelection badBudget =
      selectLodLevel(kThreeLevels, 3, 100.0f, kHalfPi, kViewportHeight, 0.0f, kHysteresisRatio, state);
  AE_EXPECT_EQ(badBudget.level, 0u, "non-positive budget fails safe");

  const LodSelection badRatio =
      selectLodLevel(kThreeLevels, 3, 100.0f, kHalfPi, kViewportHeight, kBudget, 1.5f, state);
  AE_EXPECT_EQ(badRatio.level, 0u, "hysteresis ratio above 1 fails safe");
  AE_EXPECT_EQ(state.currentLevel, 2u, "none of the failures ever mutated state");
}

AE_TEST(Lod_render_groups_keep_every_spatial_chunk_in_its_level_bucket) {
  std::vector<MapDrawRecord> draws(5);
  for (u32 index = 0; index < 4; ++index) {
    draws[index].lodGroupId = 10;
    draws[index].lodLevel = index < 2 ? 0 : 1;
    draws[index].geometricError = index < 2 ? 0.0f : 1.0f;
    draws[index].boundsCenter[0] = static_cast<float>(index % 2) * 10.0f;
    draws[index].boundsRadius = 2.0f;
  }
  draws[4].lodGroupId = 20;
  draws[4].lodLevel = 0;
  draws[4].geometricError = 0.0f;
  draws[4].boundsRadius = 1.0f;
  const std::vector<u32> candidates{0, 1, 2, 3, 4};
  std::vector<LodRenderGroup> groups;
  std::vector<u32> ungrouped;
  AE_EXPECT_TRUE(buildLodRenderGroups(draws, candidates, groups, ungrouped),
                 "valid chunked LOD groups build");
  AE_EXPECT_EQ(groups.size(), static_cast<usize>(1), "only the two-level group needs runtime selection");
  AE_EXPECT_EQ(groups[0].levelCount, 2u, "two contiguous levels detected");
  AE_EXPECT_EQ(groups[0].levels[0].drawIndices.size(), static_cast<usize>(2),
               "both level-0 chunks are retained");
  AE_EXPECT_EQ(groups[0].levels[1].drawIndices.size(), static_cast<usize>(2),
               "both level-1 chunks are retained");
  AE_EXPECT_EQ(ungrouped.size(), static_cast<usize>(1), "legacy single-level draw stays ungrouped");
  AE_EXPECT_EQ(ungrouped[0], 4u, "correct legacy draw returned");
  AE_EXPECT_TRUE(groups[0].boundsRadius >= 7.0f,
                 "group bound conservatively encloses separated chunk spheres");

  std::vector<u32> maximumQuality;
  buildLodLevelZeroDrawOrder(groups, ungrouped, maximumQuality);
  AE_EXPECT_EQ(maximumQuality.size(), static_cast<usize>(3),
               "LOD disabled keeps ungrouped plus every level-zero chunk only");
  AE_EXPECT_EQ(maximumQuality[0], 4u, "ungrouped draw remains a candidate");
  AE_EXPECT_EQ(maximumQuality[1], 0u, "first level-zero chunk retained");
  AE_EXPECT_EQ(maximumQuality[2], 1u, "second level-zero chunk retained");
}

AE_TEST(Lod_render_groups_reject_level_gaps_or_chunk_error_disagreement) {
  std::vector<MapDrawRecord> draws(2);
  for (MapDrawRecord &draw : draws) {
    draw.lodGroupId = 0;
    draw.boundsRadius = 1.0f;
  }
  draws[0].lodLevel = 0;
  draws[0].geometricError = 0.0f;
  draws[1].lodLevel = 2;
  draws[1].geometricError = 2.0f;
  const std::vector<u32> candidates{0, 1};
  std::vector<LodRenderGroup> groups;
  std::vector<u32> ungrouped;
  AE_EXPECT_TRUE(!buildLodRenderGroups(draws, candidates, groups, ungrouped),
                 "level 0 followed by level 2 is rejected");

  draws[1].lodLevel = 0;
  draws[1].geometricError = 1.0f;
  AE_EXPECT_TRUE(!buildLodRenderGroups(draws, candidates, groups, ungrouped),
                 "chunks in the same level must share geometric error");
}
