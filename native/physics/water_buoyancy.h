#pragma once

#include "physics/jolt_bridge.h"

#include <span>

namespace ae::physics {

// The plane a body is weighed against: signed height of a point is
// dot(normal, point) + offset, negative below the surface. A local plane fitted
// from the water field is what turns a wave into a force; the field itself
// stays out of this header so the maths can be tested without a renderer.
struct WaterPlane final {
  AetherVec3 normal{0.0f, 1.0f, 0.0f};
  float offset = 0.0f;
};

struct SubmergedVolume final {
  float volume = 0.0f;
  AetherVec3 centroid{};  // centre of buoyancy; meaningless when volume is zero
};

// Exact, not sampled. Voxel approximations of the submerged part get the draught
// wrong by whole percent and put the centre of buoyancy in the wrong place,
// which is what makes a boat lean the wrong way instead of righting itself.
SubmergedVolume submergedTetrahedron(const AetherVec3 corners[4], const WaterPlane &plane) noexcept;

enum class BuoyantShapeKind : ae::u32 { Sphere = 0, Box = 1 };

struct BuoyantShape final {
  BuoyantShapeKind kind = BuoyantShapeKind::Box;
  // Box: half extents. Sphere: radius in x, the rest ignored.
  AetherVec3 halfExtent{0.5f, 0.5f, 0.5f};
};

float buoyantShapeVolume(const BuoyantShape &shape) noexcept;

SubmergedVolume submergedVolume(const BuoyantShape &shape, AetherVec3 position,
                                AetherQuat rotation, const WaterPlane &plane) noexcept;

struct BuoyancySettings final {
  float fluidDensity = 1000.0f;      // kg/m^3
  float gravity = 9.81f;             // m/s^2, magnitude
  float linearDrag = 0.6f;           // quadratic coefficient along the flow
  // Viscous resistance, proportional to the displaced mass and to speed. Without
  // it a floating body is a spring with only quadratic damping, which decays
  // like 1/t: it rings for tens of seconds at small amplitude. Real hulls lose
  // that energy to viscosity and wave making, and so does this one.
  float viscousDrag = 1.0f;          // per second
  float angularDrag = 0.4f;
  float addedMassCoefficient = 0.5f; // fraction of displaced mass that resists acceleration
  // A saturated force is a tuning failure, not a result. The clamp keeps a bad
  // configuration from launching a body, and the caller is told it happened.
  float maximumAccelerationGravities = 8.0f;
};

bool validateBuoyancySettings(const BuoyancySettings &settings) noexcept;

struct BuoyancyInput final {
  SubmergedVolume submerged{};
  AetherVec3 centreOfMass{};
  AetherVec3 bodyVelocity{};   // at the centre of buoyancy, not the centre of mass
  AetherVec3 waterVelocity{};  // orbital plus flow: the body feels the difference
  AetherVec3 angularVelocity{};
  float mass = 1.0f;
  float referenceArea = 1.0f;  // projected area used by the quadratic drag term
};

struct BuoyancyForces final {
  AetherVec3 force{};
  AetherVec3 torque{};
  AetherVec3 applicationPoint{};
  float effectiveMass = 1.0f;  // mass plus added mass, for the caller's integrator
  bool clamped = false;
};

// Pure function of its input: no state, no allocation, safe from any job.
BuoyancyForces evaluateBuoyancy(const BuoyancyInput &input,
                                const BuoyancySettings &settings) noexcept;

} // namespace ae::physics
