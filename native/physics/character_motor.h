#pragma once

#include "physics/jolt_bridge.h"
#include "renderer/static_collision_mesh.h"

#include <span>

namespace ae::physics {

struct CharacterMotorSettings {
  float radius = 0.45f;
  float standingHalfHeight = 0.55f;
  float eyeHeight = 1.65f;
  float movementUnitsPerSecond = 8.0f;
  float gravityUnitsPerSecondSquared = 24.0f;
  float maximumSlopeRadians = 0.78539816339f;
  float fixedStepSeconds = 1.0f/60.0f;
};

// Runtime character primitive over the engine physics facade. It owns one
// PhysicsWorld, one static cooked world mesh and one CharacterVirtual capsule.
// Touch/camera APIs do not enter this layer: callers provide normalized actions.
class CharacterMotor final {
public:
  CharacterMotor() = default;
  ~CharacterMotor();
  CharacterMotor(const CharacterMotor &)=delete;
  CharacterMotor &operator=(const CharacterMotor &)=delete;

  bool initialize(std::span<const renderer::CollisionVertex> vertices,std::span<const u32> indices,
                  AetherVec3 spawnEyePosition,const CharacterMotorSettings &settings={});
  void shutdown();
  bool update(float moveRight,float moveForward,float yawRadians,float deltaSeconds);

  bool isReady() const { return world_!=nullptr&&character_!=AetherCharacterHandle_Invalid; }
  AetherVec3 eyePosition() const;
  AetherCharacterGroundState groundState() const;

private:
  AetherPhysicsWorld *world_=nullptr;
  AetherBodyHandle staticWorld_=AetherBodyHandle_Invalid;
  AetherCharacterHandle character_=AetherCharacterHandle_Invalid;
  CharacterMotorSettings settings_{};
  float accumulator_=0.0f;
};

} // namespace ae::physics
