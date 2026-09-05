#include "harness.h"
#include "physics/water_buoyancy.h"

#include <algorithm>
#include <cmath>

using namespace ae::physics;

namespace {
constexpr float Pi = 3.14159265358979323846f;
const AetherQuat Identity{0.0f, 0.0f, 0.0f, 1.0f};

WaterPlane flatSurface(float height = 0.0f) { return {{0.0f, 1.0f, 0.0f}, -height}; }

AetherQuat rotationAboutZ(float radians) {
  return {0.0f, 0.0f, std::sin(radians * 0.5f), std::cos(radians * 0.5f)};
}

float relativeError(float value, float reference) {
  return std::abs(value - reference) / std::max(std::abs(reference), 1.0e-6f);
}
} // namespace

AE_TEST(Buoyancy_box_draught_matches_the_closed_form_at_every_depth) {
  BuoyantShape box{};
  box.kind = BuoyantShapeKind::Box;
  box.halfExtent = {1.0f, 0.5f, 2.0f};  // 2 x 1 x 4 m, volume 8 m^3

  AE_EXPECT_TRUE(relativeError(buoyantShapeVolume(box), 8.0f) < 1.0e-5f, "box volume");

  for (int step = 0; step <= 20; ++step) {
    const float centreHeight = -0.5f + static_cast<float>(step) * 0.05f;  // -0.5 .. 0.5
    const SubmergedVolume submerged =
        submergedVolume(box, {0.0f, centreHeight, 0.0f}, Identity, flatSurface());
    const float wetHeight = std::clamp(0.5f - centreHeight, 0.0f, 1.0f);
    const float expected = wetHeight * 2.0f * 4.0f;
    AE_EXPECT_TRUE(std::abs(submerged.volume - expected) < 1.0e-4f,
                   "an axis-aligned box wets exactly its submerged slab");
    if (expected > 1.0e-4f) {
      const float expectedCentroid = centreHeight - 0.5f + wetHeight * 0.5f;
      AE_EXPECT_TRUE(std::abs(submerged.centroid.y - expectedCentroid) < 1.0e-4f,
                     "the centre of buoyancy is the centroid of the wet slab");
    }
  }
}

AE_TEST(Buoyancy_tilted_box_conserves_volume_and_moves_its_centre_sideways) {
  // A tilted box half out of the water is the case a voxel approximation gets
  // wrong: the wet volume is still half, but its centroid moves off the axis,
  // and that offset is the entire righting moment.
  BuoyantShape box{};
  box.halfExtent = {1.0f, 1.0f, 1.0f};
  const SubmergedVolume level = submergedVolume(box, {0, 0, 0}, Identity, flatSurface());
  AE_EXPECT_TRUE(relativeError(level.volume, 4.0f) < 1.0e-4f, "half of an 8 m^3 cube");
  AE_EXPECT_TRUE(std::abs(level.centroid.x) < 1.0e-5f, "level box is centred");

  const SubmergedVolume tilted =
      submergedVolume(box, {0, 0, 0}, rotationAboutZ(0.5f), flatSurface());
  // A convex body centred on the plane is cut in half whatever its orientation.
  AE_EXPECT_TRUE(relativeError(tilted.volume, 4.0f) < 1.0e-3f,
                 "rotating about the surface centre cannot change the wet volume");
  AE_EXPECT_TRUE(std::abs(tilted.centroid.x) > 0.01f,
                 "a tilted body must move its centre of buoyancy off the axis");
}

AE_TEST(Buoyancy_sphere_cap_matches_the_analytic_volume) {
  BuoyantShape sphere{};
  sphere.kind = BuoyantShapeKind::Sphere;
  sphere.halfExtent = {1.5f, 0.0f, 0.0f};
  const float full = 4.0f / 3.0f * Pi * 1.5f * 1.5f * 1.5f;
  AE_EXPECT_TRUE(relativeError(buoyantShapeVolume(sphere), full) < 1.0e-5f, "sphere volume");

  AE_EXPECT_TRUE(submergedVolume(sphere, {0, 2.0f, 0}, Identity, flatSurface()).volume == 0.0f,
                 "a sphere clear of the surface displaces nothing");
  const SubmergedVolume sunk = submergedVolume(sphere, {0, -2.0f, 0}, Identity, flatSurface());
  AE_EXPECT_TRUE(relativeError(sunk.volume, full) < 1.0e-5f, "a fully sunk sphere displaces all");
  AE_EXPECT_TRUE(std::abs(sunk.centroid.y + 2.0f) < 1.0e-5f, "and is centred on itself");

  // Exactly half submerged: half the volume, centroid at 3r/8 below the surface.
  const SubmergedVolume half = submergedVolume(sphere, {0, 0, 0}, Identity, flatSurface());
  AE_EXPECT_TRUE(relativeError(half.volume, full * 0.5f) < 1.0e-4f, "half sphere is half volume");
  AE_EXPECT_TRUE(std::abs(half.centroid.y + 3.0f * 1.5f / 8.0f) < 1.0e-4f,
                 "hemisphere centroid sits at three eighths of the radius");
}

AE_TEST(Buoyancy_shapes_reject_degenerate_input_instead_of_returning_a_guess) {
  BuoyantShape box{};
  box.halfExtent = {0.0f, 1.0f, 1.0f};
  AE_EXPECT_TRUE(buoyantShapeVolume(box) == 0.0f, "a box with no width has no volume");
  AE_EXPECT_TRUE(submergedVolume(box, {0, 0, 0}, Identity, flatSurface()).volume == 0.0f,
                 "and displaces nothing");

  BuoyantShape good{};
  WaterPlane degenerate{{0.0f, 0.0f, 0.0f}, 0.0f};
  AE_EXPECT_TRUE(submergedVolume(good, {0, 0, 0}, Identity, degenerate).volume == 0.0f,
                 "a plane without a normal is not a surface");
  const AetherVec3 nan{std::nanf(""), 0.0f, 0.0f};
  AE_EXPECT_TRUE(submergedVolume(good, nan, Identity, flatSurface()).volume == 0.0f,
                 "a non-finite position produces no force");
}

AE_TEST(Buoyancy_body_settles_at_the_draught_its_density_predicts) {
  // A cube of half the water's density must float with half its height wet.
  // This is the single number a player notices, so it is pinned directly.
  BuoyantShape box{};
  box.halfExtent = {0.5f, 0.5f, 0.5f};
  BuoyancySettings settings{};
  const float bodyVolume = buoyantShapeVolume(box);
  const float mass = 500.0f * bodyVolume;

  float height = 1.0f;
  float velocity = 0.0f;
  const float step = 1.0f / 240.0f;
  for (int tick = 0; tick < 6000; ++tick) {
    BuoyancyInput input{};
    input.submerged = submergedVolume(box, {0.0f, height, 0.0f}, Identity, flatSurface());
    input.mass = mass;
    input.bodyVelocity = {0.0f, velocity, 0.0f};
    input.referenceArea = 1.0f;
    const BuoyancyForces forces = evaluateBuoyancy(input, settings);
    AE_EXPECT_TRUE(!forces.clamped, "a plausible body must never saturate the clamp");
    const float netForce = forces.force.y - mass * settings.gravity;
    velocity += netForce / forces.effectiveMass * step;
    height += velocity * step;
  }
  AE_EXPECT_TRUE(std::abs(height) < 0.02f,
                 "a half-density cube settles with half its height submerged");
  AE_EXPECT_TRUE(std::abs(velocity) < 0.05f, "and stops moving instead of oscillating forever");
}

AE_TEST(Buoyancy_drag_follows_the_water_and_not_the_world) {
  BuoyancySettings settings{};
  BuoyantShape box{};
  BuoyancyInput input{};
  input.submerged = submergedVolume(box, {0, -1.0f, 0}, Identity, flatSurface());
  input.mass = 400.0f;
  input.referenceArea = 1.0f;

  // A body drifting with the water feels no drag; the same body standing still
  // in a moving current is pushed. Using world velocity would invert both.
  input.bodyVelocity = {3.0f, 0.0f, 0.0f};
  input.waterVelocity = {3.0f, 0.0f, 0.0f};
  const BuoyancyForces drifting = evaluateBuoyancy(input, settings);
  AE_EXPECT_TRUE(std::abs(drifting.force.x) < 1.0e-4f, "moving with the water is not drag");

  input.bodyVelocity = {0.0f, 0.0f, 0.0f};
  const BuoyancyForces held = evaluateBuoyancy(input, settings);
  AE_EXPECT_TRUE(held.force.x > 1.0f, "a current pushes a body that is holding still");
}

AE_TEST(Buoyancy_added_mass_is_reported_and_never_folded_into_the_force) {
  BuoyancySettings settings{};
  settings.addedMassCoefficient = 0.5f;
  BuoyantShape box{};
  BuoyancyInput input{};
  input.submerged = submergedVolume(box, {0, -1.0f, 0}, Identity, flatSurface());
  input.mass = 100.0f;
  const BuoyancyForces forces = evaluateBuoyancy(input, settings);
  const float displaced = settings.fluidDensity * input.submerged.volume;
  AE_EXPECT_TRUE(std::abs(forces.effectiveMass - (100.0f + 0.5f * displaced)) < 1.0e-3f,
                 "added mass belongs to the integrator, not to the applied force");

  settings.addedMassCoefficient = 0.0f;
  const BuoyancyForces without = evaluateBuoyancy(input, settings);
  AE_EXPECT_TRUE(std::abs(without.force.y - forces.force.y) < 1.0e-3f,
                 "changing added mass must not change the force itself");
}

AE_TEST(Buoyancy_clamp_reports_itself_instead_of_hiding_a_bad_configuration) {
  BuoyancySettings settings{};
  settings.maximumAccelerationGravities = 1.0f;
  BuoyantShape box{};
  box.halfExtent = {5.0f, 5.0f, 5.0f};
  BuoyancyInput input{};
  input.submerged = submergedVolume(box, {0, -10.0f, 0}, Identity, flatSurface());
  input.mass = 1.0f;  // a cork the size of a room
  const BuoyancyForces forces = evaluateBuoyancy(input, settings);
  AE_EXPECT_TRUE(forces.clamped, "a saturated force must say so");
  AE_EXPECT_TRUE(std::abs(forces.force.y - settings.gravity) < 1.0e-3f,
                 "and must be limited to the configured acceleration");
}

AE_TEST(Buoyancy_settings_outside_their_envelope_produce_no_force) {
  BuoyancySettings settings{};
  settings.fluidDensity = -1.0f;
  AE_EXPECT_TRUE(!validateBuoyancySettings(settings), "negative density is rejected");
  BuoyantShape box{};
  BuoyancyInput input{};
  input.submerged = submergedVolume(box, {0, -1.0f, 0}, Identity, flatSurface());
  input.mass = 10.0f;
  const BuoyancyForces forces = evaluateBuoyancy(input, settings);
  AE_EXPECT_TRUE(forces.force.y == 0.0f, "invalid settings apply nothing at all");
  AE_EXPECT_TRUE(forces.effectiveMass == input.mass, "and leave the integrator's mass untouched");
}
