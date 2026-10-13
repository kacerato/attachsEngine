#pragma once

#include "physics/jolt_bridge.h"
#include "renderer/static_collision_mesh.h"

#include <cmath>
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
// Locomocao (N05): zero em aceleracao/frenagem responde na hora, como antes.
struct CharacterLocomotion {
  float acceleration=0,braking=0,airControl=1,swimSpeed=.6f,turnRate=90;
  u32 mode=0; // 0 solo, 1 voo, 2 nado quando ha agua, 3 dirigir
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
  // Navegação (Warp, travessia de link): move o olho para `eye` sem simular o caminho.
  bool teleportEye(AetherVec3 eye);
  bool configureMotion(float speed,float gravity,float stepHeight,float floorSnapLength,bool inheritPlatformHorizontal=false);
  bool configureLocomotion(const CharacterLocomotion &locomotion);
  // Velocidade pedida por root motion neste passo (mundo, m/s): substitui o
  // comando no plano e, se `vertical`, tambem no eixo Y. A colisao decide o
  // deslocamento realizado; consumido pelo proximo update.
  void requestVelocity(AetherVec3 velocity,bool vertical);
  // Eixo vertical (-1..1) do voo/nado; consumido pelo proximo update.
  void setVerticalAxis(float axis);
  // Superficie da agua neste ponto (altura do mundo); sem agua, `present` falso.
  void setWater(bool present,float surfaceHeight);
  bool inWater() const noexcept {return swimming_;}
  // Cápsula baixa; levantar falha quando nao ha espaco acima.
  bool setCrouching(bool crouching);
  bool crouching() const noexcept {return crouching_;}
  // Dirigir: o motor guarda a propria direcao (radianos); a camera nao gira o veiculo.
  void setHeading(float radians) {if(std::isfinite(radians))heading_=radians;}
  float heading() const noexcept {return heading_;}
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
  CharacterLocomotion locomotion_{};
  AetherVec3 planar_{},requested_{};bool hasRequest_=false,requestVertical_=false;
  float vertical_=0,waterSurface_=0,heading_=0;bool waterPresent_=false,swimming_=false,crouching_=false;
};

} // namespace ae::physics
