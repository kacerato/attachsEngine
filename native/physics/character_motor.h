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
  float stepHeight=.4f,floorSnapLength=.5f;
  bool inheritPlatformHorizontal=false;
};

// Runtime character primitive over the engine physics facade. It owns one
// private PhysicsWorld or borrows a scene world, plus one CharacterVirtual capsule.
// Touch/camera APIs do not enter this layer: callers provide normalized actions.
class CharacterMotor final {
public:
  CharacterMotor() = default;
  ~CharacterMotor();
  CharacterMotor(const CharacterMotor &)=delete;
  CharacterMotor &operator=(const CharacterMotor &)=delete;

  bool initialize(std::span<const renderer::CollisionVertex> vertices,std::span<const u32> indices,
                  AetherVec3 spawnEyePosition,const CharacterMotorSettings &settings={});
  // The scene owns and steps this world. Destroy the motor before the world.
  // Call update once per scene fixed tick; this never steps rigid bodies.
  bool initializeInWorld(AetherPhysicsWorld *world,AetherVec3 spawnEyePosition,
                         const CharacterMotorSettings &settings={});
  void shutdown();
  bool update(float moveRight,float moveForward,float yawRadians,float deltaSeconds);
  bool jump(float speed);
  bool configureMotion(float speed,float gravity,float stepHeight,float floorSnapLength,bool inheritPlatformHorizontal=false);
  // Scalar state only: never carries old-world Jolt contacts, bodies or handles.
  struct MotionState {AetherVec3 velocity{};float pendingJump=0,accumulator=0;AetherVec3 resolvedVelocity{};bool hasMeasuredStep=false;AetherVec3 platformCarry{};};
  struct RuntimeState {AetherVec3 position{},velocity{},motorVelocity{},groundVelocity{},groundNormal{},groundPoint{},capsuleBottom{},capsuleTop{};float capsuleRadius=0;AetherCharacterGroundState groundState=AetherCharacterGroundState::InAir;bool hasMeasuredStep=false,hasGroundPoint=false,hasCapsule=false;AetherBodyHandle groundBody=AetherBodyHandle_Invalid;};
  bool runtimeState(RuntimeState &out) const;
  MotionState motionState() const;
  bool restoreMotionState(const MotionState &state);

  bool isReady() const { return world_!=nullptr&&character_!=AetherCharacterHandle_Invalid; }
  AetherCharacterHandle handle() const { return character_; }
  AetherPhysicsWorld *physicsWorld() const { return world_; }
  AetherVec3 eyePosition() const;
  AetherCharacterGroundState groundState() const;

private:
  bool initializeImpl(std::span<const renderer::CollisionVertex> vertices,std::span<const u32> indices,
                      AetherVec3 spawnEyePosition,const CharacterMotorSettings &settings,AetherPhysicsWorld *sceneWorld);
  bool ownsWorld_=false;
  AetherPhysicsWorld *world_=nullptr;
  AetherBodyHandle staticWorld_=AetherBodyHandle_Invalid;
  AetherCharacterHandle character_=AetherCharacterHandle_Invalid;
  CharacterMotorSettings settings_{};
  float accumulator_=0.0f;
  float pendingJump_=0;
  AetherVec3 resolvedVelocity_{},platformCarry_{};
  bool hasMeasuredStep_=false;
};

} // namespace ae::physics
