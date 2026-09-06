#include "harness.h"
#include "physics/water_buoyancy.h"
#include "physics/water_field_adapter.h"
#include "physics/water_runtime.h"
#include "physics/water_simulation.h"

#include <algorithm>
#include <cmath>

using namespace ae::physics;

AE_TEST(Water_simulation_runs_the_same_fixed_clock_at_different_render_rates) {
  for (int rate : {30, 60, 120}) {
    AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0, -9.81f, 0}, 8);
    AE_EXPECT_TRUE(world != nullptr, "world");
    ae::renderer::WaterWorld water;
    WaterSimulation simulation;
    ae::u32 steps = 0;
    WaterSimulationFrame result;
    for (int frame = 0; frame < rate; ++frame) {
      result = simulation.advance(world, water, 1.0 / rate);
      AE_EXPECT_TRUE(result.error == WaterSimulationError::None, "frame");
      steps += result.steps;
    }
    AE_EXPECT_TRUE(steps == 60 && std::abs(result.simulationTime - 1) < 1e-9, "same clock");
    AE_EXPECT_TRUE(result.interpolation < 1e-8, "no remainder drift");
    AetherPhysics_DestroyWorld(world);
  }
}

AE_TEST(Water_simulation_pause_slow_motion_and_catch_up_have_explicit_budgets) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0, -9.81f, 0}, 8);
  ae::renderer::WaterWorld water;
  WaterSimulation simulation;
  AE_EXPECT_TRUE(simulation.advance(world, water, 10, 1, true).steps == 0, "paused time not accumulated");
  auto frame = simulation.advance(world, water, 1.0 / 60, .5);
  AE_EXPECT_TRUE(frame.steps == 0 && std::abs(frame.interpolation - .5) < 1e-8, "half speed");
  frame = simulation.advance(world, water, 1.0 / 60, .5);
  AE_EXPECT_TRUE(frame.steps == 1, "second half completes step");
  frame = simulation.advance(world, water, 1);
  AE_EXPECT_TRUE(frame.steps == 8, "catch-up limited");
  AE_EXPECT_TRUE(frame.droppedSimulationTime > .8, "time loss is reported");
  const double time = frame.simulationTime;
  frame = simulation.advance(world, water, std::nan(""));
  AE_EXPECT_TRUE(frame.error == WaterSimulationError::InvalidInput && frame.simulationTime == time,
                 "bad frame preserves clock");
  AE_EXPECT_TRUE(!simulation.configure({}), "fixed clock cannot change while running");
  simulation.reset();
  AE_EXPECT_TRUE(simulation.configure({}), "reset permits configuration");
  AetherPhysics_DestroyWorld(world);
}

AE_TEST(Water_simulation_consumes_events_per_substep_and_retries_failed_provider) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0, -9.81f, 0}, 8);
  ae::renderer::WaterWorld water;
  AE_EXPECT_TRUE(water.setVolume(1, {}), "water");
  AetherBodyDesc desc{};
  desc.shape.kind = AetherShapeKind::Box;
  desc.shape.boxHalfExtent = {.5f, .5f, .5f};
  desc.position = {0, -.25f, 0};
  desc.rotation = {0, 0, 0, 1};
  desc.motionType = AetherMotionType::Dynamic;
  const auto body = AetherPhysics_CreateBody(world, &desc);
  WaterSimulation simulation;
  AE_EXPECT_TRUE(simulation.bind({body, {}, 1}), "body");
  struct Observer { bool ready = false; ae::u32 steps = 0, entries = 0; } observer;
  WaterSimulationHooks hooks;
  hooks.context = &observer;
  hooks.beforeStep = [](void *context, double) { return static_cast<Observer *>(context)->ready; };
  hooks.afterStep = [](void *context, std::span<const WaterContactEvent> events) {
    auto &state = *static_cast<Observer *>(context);
    ++state.steps;
    for (auto &event : events) if (event.phase == WaterContactPhase::Enter) ++state.entries;
  };
  auto result = simulation.advance(world, water, 1.0 / 30, 1, false, hooks);
  AE_EXPECT_TRUE(result.error == WaterSimulationError::Provider && result.steps == 0, "provider not ready");
  observer.ready = true;
  result = simulation.advance(world, water, 0, 1, false, hooks);
  AE_EXPECT_TRUE(result.steps == 2 && observer.steps == 2, "retry both pending steps");
  AE_EXPECT_TRUE(observer.entries == 1, "entry not lost to second substep");
  AetherPhysics_DestroyWorld(world);
}

AE_TEST(Water_runtime_queries_applies_and_emits_contact_with_real_jolt_bodies) {
  AetherPhysicsWorld *physics = AetherPhysics_CreateWorld({0, -9.81f, 0}, 8);
  AE_EXPECT_TRUE(physics != nullptr, "physics world");
  AetherBodyDesc desc{};
  desc.shape.kind = AetherShapeKind::Box;
  desc.shape.boxHalfExtent = {.5f, .5f, .5f};
  desc.rotation = {0, 0, 0, 1};
  desc.motionType = AetherMotionType::Dynamic;
  desc.position = {0, -.25f, 0};
  const auto body = AetherPhysics_CreateBody(physics, &desc);
  ae::renderer::WaterWorld water;
  ae::renderer::WaterFieldSetup setup{};
  setup.currents.uniform = {2, 0};
  AE_EXPECT_TRUE(water.setVolume(77, setup), "water");
  WaterRuntime runtime;
  AE_EXPECT_TRUE(runtime.bind({body, {}, 1}), "bind body");
  AE_EXPECT_TRUE(runtime.apply(physics, water, 0), "apply first fixed tick");
  AE_EXPECT_TRUE(runtime.events().size() == 1 && runtime.events()[0].phase == WaterContactPhase::Enter,
                 "entry event");
  AE_EXPECT_TRUE(runtime.stats().forces.bodiesSubmerged == 1, "force submitted");
  AE_EXPECT_TRUE(!runtime.apply(physics, water, 0), "reject duplicate tick before adding forces twice");
  AetherPhysics_Step(physics, 1.0f / 60, 1);
  AE_EXPECT_TRUE(AetherPhysics_GetLinearVelocity(physics, body).x > 0, "current accelerates real body");
  AE_EXPECT_TRUE(runtime.apply(physics, water, 1.0 / 60), "next tick");
  AE_EXPECT_TRUE(runtime.events().empty(), "no repeated enter");
  AetherPhysics_Step(physics, 1.0f / 60, 1);
  water.clear();
  AE_EXPECT_TRUE(runtime.apply(physics, water, 2.0 / 60), "water unload");
  AE_EXPECT_TRUE(runtime.events().size() == 1 && runtime.events()[0].phase == WaterContactPhase::Exit,
                 "unload exits contact");
  AE_EXPECT_TRUE(runtime.stats().forces.bodiesSubmerged == 0, "unloaded water applies no force");
  AE_EXPECT_TRUE(runtime.unbind(body), "scene lifecycle unbind");
  AetherPhysics_DestroyWorld(physics);
}

AE_TEST(Water_runtime_equilibrium_is_consistent_at_60_and_120_fixed_steps) {
  float finalHeight[2]{};
  for (int run = 0; run < 2; ++run) {
    const int frequency = run == 0 ? 60 : 120;
    AetherPhysicsWorld *physics = AetherPhysics_CreateWorld({0, -9.81f, 0}, 8);
    AE_EXPECT_TRUE(physics != nullptr, "world");
    AetherBodyDesc desc{};
    desc.shape.kind = AetherShapeKind::Box;
    desc.shape.boxHalfExtent = {.5f, .5f, .5f};
    desc.rotation = {0, 0, 0, 1};
    desc.position = {0, 1, 0};
    desc.motionType = AetherMotionType::Dynamic;
    const auto body = AetherPhysics_CreateBody(physics, &desc);
    // Convergence and sleeping are independent contracts. The existing sleep
    // regression stays enabled; here prevent the solver from freezing an early
    // low-velocity pose on opposite sides of equilibrium at different dt.
    AE_EXPECT_TRUE(AetherPhysics_SetAllowSleepingV2(physics, body, 0), "disable sleep for convergence");
    ae::renderer::WaterWorld water;
    AE_EXPECT_TRUE(water.setVolume(1, {}), "water");
    WaterRuntime runtime;
    AE_EXPECT_TRUE(runtime.bind({body, {}, 1}), "body");
    WaterRuntimeSettings settings;
    settings.forces.fluidDensity = 2000;
    for (int tick = 0; tick < frequency * 12; ++tick) {
      AE_EXPECT_TRUE(runtime.apply(physics, water, static_cast<double>(tick) / frequency, settings), "tick");
      AetherPhysics_Step(physics, 1.0f / frequency, 1);
    }
    AetherVec3 position{};
    AE_EXPECT_TRUE(AetherPhysics_TryGetBodyPoseV2(physics, body, &position, nullptr), "pose");
    finalHeight[run] = position.y;
    AE_EXPECT_TRUE(std::abs(position.y) < .05f, "physical equilibrium");
    AE_EXPECT_TRUE(runtime.stats().forces.bodiesClamped == 0, "no saturation");
    AetherPhysics_DestroyBody(physics, body);
    position.y = 123;
    AE_EXPECT_TRUE(!AetherPhysics_TryGetBodyPoseV2(physics, body, &position, nullptr), "stale handle");
    AE_EXPECT_TRUE(position.y == 123, "failure preserves output");
    AE_EXPECT_TRUE(runtime.apply(physics, water, 13, settings), "stale binding safely skipped");
    AE_EXPECT_TRUE(runtime.stats().unavailableBodies == 1, "observable stale binding");
    AetherPhysics_DestroyWorld(physics);
  }
  if (std::abs(finalHeight[0] - finalHeight[1]) >= .05f)
    std::fprintf(stderr, "Water equilibrium: 60Hz=%f 120Hz=%f\n", finalHeight[0], finalHeight[1]);
  AE_EXPECT_TRUE(std::abs(finalHeight[0] - finalHeight[1]) < .05f, "timestep convergence");
}

AE_TEST(Water_field_adapter_drives_buoyancy_with_current_and_orbital_velocity) {
  ae::renderer::WaterFieldSetup setup{};
  setup.currents.uniform = {2, 0};
  ae::renderer::WaterField field;
  AE_EXPECT_TRUE(field.configure(setup), "field configured");
  const ae::renderer::WaterVec2 position{0, 0};
  ae::renderer::WaterFieldSample sampled{};
  AE_EXPECT_TRUE(field.sample({&position, 1}, 0, {&sampled, 1}), "field queried");
  AetherWaterBodySample body{};
  AE_EXPECT_TRUE(makeWaterBodySample(1, {}, position, sampled, 1, body), "adapter");
  BuoyancyInput input{};
  input.mass = 1000;
  input.submerged = submergedVolume({}, {0, -.25f, 0}, {0, 0, 0, 1},
                                    {body.planeNormal, body.planeOffset});
  input.waterVelocity = body.waterVelocity;
  AE_EXPECT_TRUE(evaluateBuoyancy(input, {}).force.x > 0, "current pushes body");
  sampled.flags |= static_cast<ae::u32>(ae::renderer::WaterFieldFlag::Excluded);
  AE_EXPECT_TRUE(!makeWaterBodySample(2, {}, position, sampled, 1, body), "dry region rejected");
  AE_EXPECT_TRUE(body.body == 1, "failed conversion preserves output");
}

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

namespace {
AetherWaterBodySample boxSample(AetherBodyHandle body, AetherVec3 halfExtent) {
  AetherWaterBodySample sample{};
  sample.body = body;
  sample.shapeKind = static_cast<ae::u32>(BuoyantShapeKind::Box);
  sample.halfExtent = halfExtent;
  sample.planeNormal = {0.0f, 1.0f, 0.0f};
  sample.planeOffset = 0.0f;
  sample.referenceArea = 1.0f;
  return sample;
}
} // namespace

AE_TEST(Water_forces_boundary_rejects_a_call_it_cannot_honour) {
  BuoyancySettings settings{};
  AetherWaterForceStats stats{};
  stats.bodiesConsidered = 7;
  AE_EXPECT_TRUE(AetherPhysics_ApplyWaterForces(nullptr, nullptr, 0, &settings, &stats) == -1,
                 "no world, no forces");
  AE_EXPECT_TRUE(stats.bodiesConsidered == 0, "a rejected call still clears the statistics");

  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 8);
  AE_EXPECT_TRUE(world != nullptr, "world");
  AetherWaterBodySample sample = boxSample(AetherBodyHandle_Invalid, {0.5f, 0.5f, 0.5f});
  AE_EXPECT_TRUE(AetherPhysics_ApplyWaterForces(world, &sample, -1, &settings, nullptr) == -1,
                 "a negative count is a programming error, not an empty batch");
  AE_EXPECT_TRUE(AetherPhysics_ApplyWaterForces(world, nullptr, 3, &settings, nullptr) == -1,
                 "a null batch with a positive count is rejected");
  settings.fluidDensity = 0.0f;
  AE_EXPECT_TRUE(AetherPhysics_ApplyWaterForces(world, &sample, 1, &settings, nullptr) == -1,
                 "invalid settings apply nothing");
  AetherPhysics_DestroyWorld(world);
}

AE_TEST(Water_forces_float_a_body_at_its_density_draught_and_let_it_sleep) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 8);
  AE_EXPECT_TRUE(world != nullptr, "world");

  // Jolt derives mass from the shape at 1000 kg/m^3, so a fluid at 2000 makes
  // the body half as dense as the water and it must settle half submerged.
  AetherBodyDesc desc{};
  desc.shape.kind = AetherShapeKind::Box;
  desc.shape.boxHalfExtent = {0.5f, 0.5f, 0.5f};
  desc.position = {0.0f, 4.0f, 0.0f};
  desc.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
  desc.motionType = AetherMotionType::Dynamic;
  desc.friction = 0.5f;
  const AetherBodyHandle body = AetherPhysics_CreateBody(world, &desc);
  AE_EXPECT_TRUE(body != AetherBodyHandle_Invalid, "body");

  BuoyancySettings settings{};
  settings.fluidDensity = 2000.0f;
  AetherWaterBodySample sample = boxSample(body, desc.shape.boxHalfExtent);
  AetherWaterForceStats stats{};
  bool everSubmerged = false;
  for (int tick = 0; tick < 900; ++tick) {
    AetherPhysics_ApplyWaterForces(world, &sample, 1, &settings, &stats);
    if (stats.bodiesSubmerged > 0) everSubmerged = true;
    AetherPhysics_Step(world, 1.0f / 60.0f, 1);
  }
  AE_EXPECT_TRUE(everSubmerged, "the body must actually reach the water");
  AE_EXPECT_TRUE(stats.bodiesClamped == 0, "a plausible body never saturates the clamp");

  AetherVec3 position{};
  AetherPhysics_GetTransform(world, body, &position, nullptr);
  AE_EXPECT_TRUE(std::abs(position.y) < 0.05f,
                 "a body half the density of the fluid floats with half its height wet");
  // Water that keeps every floating body awake is a battery drain, and it is
  // why the boundary skips sleeping bodies instead of forcing them active.
  AE_EXPECT_TRUE(AetherPhysics_IsActive(world, body) == 0,
                 "a settled body is allowed to sleep");
  AetherPhysics_DestroyWorld(world);
}
