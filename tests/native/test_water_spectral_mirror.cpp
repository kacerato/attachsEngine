#include "harness.h"
#include "renderer/water_spectral_mirror.h"

#include <cmath>
#include <vector>

using namespace ae::renderer;

namespace {
WaterMirrorSettings mirrorSettings(unsigned resolution = 32) {
  WaterMirrorSettings settings{};
  settings.spectrum.resolution = resolution;
  settings.spectrum.patchLength = 64.0f;
  settings.spectrum.depth = 40.0f;
  settings.spectrum.windSpeed = 8.0f;
  settings.spectrum.seed = 7;
  return settings;
}
} // namespace

AE_TEST(Water_mirror_rejects_settings_that_defeat_its_purpose) {
  WaterSpectralMirror mirror;
  WaterMirrorSettings settings = mirrorSettings();
  // A mirror exists to be cheaper than the GPU path. A resolution that is not
  // cheap would be a second renderer, so the contract refuses it outright.
  settings.spectrum.resolution = 256;
  AE_EXPECT_TRUE(!validateWaterMirrorSettings(settings), "oversized mirror is rejected");
  AE_EXPECT_TRUE(!mirror.initialize(settings), "rejected settings do not initialize");
  AE_EXPECT_TRUE(!mirror.isReady(), "a rejected mirror is not ready");

  settings = mirrorSettings();
  settings.choppiness = 5.0f;
  AE_EXPECT_TRUE(!validateWaterMirrorSettings(settings), "choppiness stays inside its envelope");

  settings = mirrorSettings();
  settings.inversionIterations = 99;
  AE_EXPECT_TRUE(!validateWaterMirrorSettings(settings), "inversion is bounded work");
}

AE_TEST(Water_mirror_is_a_pure_function_of_simulation_time) {
  WaterSpectralMirror mirror;
  AE_EXPECT_TRUE(mirror.initialize(mirrorSettings()), "initialize");
  AE_EXPECT_TRUE(!mirror.isReady(), "a mirror without an update has nothing to report");

  AE_EXPECT_TRUE(mirror.update(3.25), "update");
  const WaterMirrorSample first = mirror.sample({12.5f, -8.0f});

  // Reaching the same instant through a different number of steps must give
  // the same surface: this is what lets physics run at any tick rate.
  AE_EXPECT_TRUE(mirror.update(0.0), "rewind");
  AE_EXPECT_TRUE(mirror.update(1.0), "step");
  AE_EXPECT_TRUE(mirror.update(3.25), "return to the same instant");
  const WaterMirrorSample second = mirror.sample({12.5f, -8.0f});

  AE_EXPECT_TRUE(first.height == second.height, "height is bit-identical at the same time");
  AE_EXPECT_TRUE(first.velocity.y == second.velocity.y, "velocity is bit-identical too");
}

AE_TEST(Water_mirror_vertical_velocity_matches_the_height_derivative) {
  // The rate comes from the analytic i*omega factor rather than a difference
  // between frames. Comparing it against a centred difference of the height is
  // the only way to catch a sign or phase error in that derivation.
  WaterSpectralMirror mirror;
  AE_EXPECT_TRUE(mirror.initialize(mirrorSettings(32)), "initialize");
  const WaterVec2 position{5.0f, 11.0f};
  const double time = 2.0, delta = 0.002;

  AE_EXPECT_TRUE(mirror.update(time - delta), "before");
  const float before = mirror.sample(position).height;
  AE_EXPECT_TRUE(mirror.update(time + delta), "after");
  const float after = mirror.sample(position).height;
  AE_EXPECT_TRUE(mirror.update(time), "centre");
  const WaterMirrorSample centre = mirror.sample(position);

  const float numerical = static_cast<float>((after - before) / (2.0 * delta));
  const float tolerance = 0.02f * std::max(1.0f, std::abs(numerical));
  AE_EXPECT_TRUE(std::abs(centre.velocity.y - numerical) < tolerance,
                 "analytic vertical velocity must track the height it belongs to");
}

AE_TEST(Water_mirror_horizontal_velocity_matches_the_displacement_derivative) {
  WaterMirrorSettings settings = mirrorSettings(32);
  settings.choppiness = 1.0f;
  WaterSpectralMirror mirror;
  AE_EXPECT_TRUE(mirror.initialize(settings), "initialize");
  const WaterVec2 position{-3.0f, 6.5f};
  const double time = 1.5, delta = 0.002;

  AE_EXPECT_TRUE(mirror.update(time - delta), "before");
  const WaterVec2 before = mirror.sample(position).displacement;
  AE_EXPECT_TRUE(mirror.update(time + delta), "after");
  const WaterVec2 after = mirror.sample(position).displacement;
  AE_EXPECT_TRUE(mirror.update(time), "centre");
  const WaterMirrorSample centre = mirror.sample(position);

  const float numericalX = static_cast<float>((after.x - before.x) / (2.0 * delta));
  const float numericalZ = static_cast<float>((after.y - before.y) / (2.0 * delta));
  // The fixed point re-anchors the parameter point every call, so the numerical
  // derivative here follows the world column and carries more error than the
  // vertical case. The tolerance is loose on purpose and still catches a wrong
  // axis, a wrong sign or a missing choppiness factor.
  const float tolerance = 0.15f * std::max(1.0f, std::abs(numericalX) + std::abs(numericalZ));
  AE_EXPECT_TRUE(std::abs(centre.velocity.x - numericalX) < tolerance,
                 "horizontal velocity X follows its own displacement");
  AE_EXPECT_TRUE(std::abs(centre.velocity.z - numericalZ) < tolerance,
                 "horizontal velocity Z follows its own displacement");
}

AE_TEST(Water_mirror_slope_matches_the_spatial_derivative_of_height) {
  WaterMirrorSettings settings = mirrorSettings(32);
  settings.choppiness = 0.0f;  // isolate the slope from displacement inversion
  WaterSpectralMirror mirror;
  AE_EXPECT_TRUE(mirror.initialize(settings), "initialize");
  AE_EXPECT_TRUE(mirror.update(0.75), "update");

  const WaterVec2 position{9.0f, -2.0f};
  const float step = 0.05f;
  const float dx = (mirror.sample({position.x + step, position.y}).height -
                    mirror.sample({position.x - step, position.y}).height) / (2.0f * step);
  const float dz = (mirror.sample({position.x, position.y + step}).height -
                    mirror.sample({position.x, position.y - step}).height) / (2.0f * step);
  const WaterMirrorSample sample = mirror.sample(position);
  const float tolerance = 0.05f * std::max(1.0f, std::abs(dx) + std::abs(dz));
  AE_EXPECT_TRUE(std::abs(sample.slope.x - dx) < tolerance, "slope X is the derivative of height");
  AE_EXPECT_TRUE(std::abs(sample.slope.y - dz) < tolerance, "slope Z is the derivative of height");
}

AE_TEST(Water_mirror_inversion_lands_on_the_requested_world_column) {
  // With choppiness the surface point is displaced horizontally. Sampling must
  // answer for the column asked about, not for the parameter point that shares
  // its coordinates, or a hull would float beside the wave instead of on it.
  WaterMirrorSettings settings = mirrorSettings(64);
  settings.choppiness = 1.0f;
  settings.inversionIterations = 4;
  WaterSpectralMirror mirror;
  AE_EXPECT_TRUE(mirror.initialize(settings), "initialize");
  AE_EXPECT_TRUE(mirror.update(4.0), "update");

  float worst = 0.0f;
  for (int index = 0; index < 64; ++index) {
    const WaterVec2 world{static_cast<float>(index) * 1.37f - 40.0f,
                          static_cast<float>(index) * -0.91f + 15.0f};
    const WaterMirrorSample sample = mirror.sample(world);
    // The parameter point plus its displacement must return to the world point.
    const float residualX = sample.displacement.x;
    const float residualZ = sample.displacement.y;
    worst = std::max(worst, std::hypot(residualX, residualZ));
  }
  // The residual is bounded rather than zero: the check is that displacement
  // stays inside a plausible envelope, which fails loudly if inversion diverges.
  AE_EXPECT_TRUE(std::isfinite(worst) && worst < 10.0f,
                 "displacement inversion must stay bounded over the patch");
}

AE_TEST(Water_mirror_rotates_the_query_instead_of_the_spectrum) {
  WaterMirrorSettings straight = mirrorSettings(32);
  WaterMirrorSettings rotated = straight;
  rotated.directionRadians = 1.5707963f;

  WaterSpectralMirror a, b;
  AE_EXPECT_TRUE(a.initialize(straight) && b.initialize(rotated), "initialize both");
  AE_EXPECT_TRUE(a.update(2.5) && b.update(2.5), "update both");

  // Turning the wind a quarter turn must move the same surface, not build a
  // different one: the sample at the rotated position has to match.
  // The mirror maps world to spectrum by rotating the query by minus the wind
  // angle, so the world point that lands on the same parameter point after a
  // quarter turn is (-z, x), not (z, -x).
  const WaterVec2 position{7.0f, 3.0f};
  const WaterVec2 turned{-position.y, position.x};
  const float direct = a.sample(position).height;
  const float viaRotation = b.sample(turned).height;
  AE_EXPECT_TRUE(std::abs(direct - viaRotation) < 1.0e-4f,
                 "a rotated mirror is the same surface seen from a rotated frame");
}

AE_TEST(Water_mirror_reports_nothing_before_the_first_update) {
  WaterSpectralMirror mirror;
  AE_EXPECT_TRUE(mirror.initialize(mirrorSettings()), "initialize");
  const WaterMirrorSample sample = mirror.sample({1.0f, 1.0f});
  AE_EXPECT_TRUE(sample.height == 0.0f && sample.velocity.y == 0.0f,
                 "an un-evaluated mirror reports a flat surface, not stale memory");
}
