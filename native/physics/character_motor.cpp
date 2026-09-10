#include "physics/character_motor.h"

#include <algorithm>
#include <cmath>

namespace ae::physics {

CharacterMotor::~CharacterMotor(){shutdown();}

bool CharacterMotor::initialize(std::span<const renderer::CollisionVertex> vertices,
                                std::span<const u32> indices,
                                AetherVec3 spawnEyePosition,
                                const CharacterMotorSettings &settings){
  return initializeImpl(vertices,indices,spawnEyePosition,settings,nullptr);
}
bool CharacterMotor::initializeInWorld(AetherPhysicsWorld *world,AetherVec3 spawnEyePosition,
                                      const CharacterMotorSettings &settings) {
  if(!world) return false;
  return initializeImpl({},{},spawnEyePosition,settings,world);
}
bool CharacterMotor::initializeImpl(std::span<const renderer::CollisionVertex> vertices,
                                    std::span<const u32> indices,AetherVec3 spawnEyePosition,
                                    const CharacterMotorSettings &settings,AetherPhysicsWorld *sceneWorld) {
  shutdown();
  const float values[]={settings.radius,settings.standingHalfHeight,settings.eyeHeight,
      settings.movementUnitsPerSecond,settings.gravityUnitsPerSecondSquared,
      settings.maximumSlopeRadians,settings.fixedStepSeconds,spawnEyePosition.x,
      spawnEyePosition.y,spawnEyePosition.z};
  for(float value:values) if(!std::isfinite(value)) return false;
  if(settings.radius<=0||settings.standingHalfHeight<=0||
      settings.eyeHeight<=settings.radius||settings.movementUnitsPerSecond<=0||
      settings.gravityUnitsPerSecondSquared<=0||settings.maximumSlopeRadians<=0||
      settings.maximumSlopeRadians>=1.57079632679f||settings.fixedStepSeconds<=0||
      settings.fixedStepSeconds>0.05f||(!sceneWorld&&(vertices.empty()||indices.empty()))) return false;
  settings_=settings;
  ownsWorld_=sceneWorld==nullptr;
  world_=sceneWorld?sceneWorld:AetherPhysics_CreateWorld({0,-settings_.gravityUnitsPerSecondSquared,0},64);
  if(world_==nullptr) return false;
  if(ownsWorld_) {
  static_assert(sizeof(renderer::CollisionVertex)==sizeof(AetherVec3));
  staticWorld_=AetherPhysics_CreateStaticTriangleMesh(world_,
      reinterpret_cast<const AetherVec3 *>(vertices.data()),static_cast<u32>(vertices.size()),
      indices.data(),static_cast<u32>(indices.size()),0.8f);
  if(staticWorld_==AetherBodyHandle_Invalid){shutdown();return false;}
  }
  AetherCharacterDesc character{};
  character.radius=settings_.radius;
  character.standingHalfHeight=settings_.standingHalfHeight;
  character.crouchingHalfHeight=std::max(0.1f,settings_.standingHalfHeight*.5f);
  character.maxSlopeAngle=settings_.maximumSlopeRadians;
  character.mass=70.0f;
  character.maxStrength=100.0f;
  const AetherVec3 base{spawnEyePosition.x,spawnEyePosition.y-settings_.eyeHeight,
                        spawnEyePosition.z};
  character_=AetherPhysics_CreateCharacter(world_,&character,base,{0,0,0,1});
  if(character_==AetherCharacterHandle_Invalid){shutdown();return false;}
  accumulator_=0;
  return true;
}

void CharacterMotor::shutdown(){
  if(world_!=nullptr&&character_!=AetherCharacterHandle_Invalid)
    AetherPhysics_DestroyCharacter(world_,character_);
  character_=AetherCharacterHandle_Invalid;
  staticWorld_=AetherBodyHandle_Invalid;
  if(world_!=nullptr && ownsWorld_) AetherPhysics_DestroyWorld(world_);
  world_=nullptr;
  ownsWorld_=false;
  accumulator_=0;
  pendingJump_=0;
}

bool CharacterMotor::jump(float speed) {
  if(!isReady()||!std::isfinite(speed)||speed<=0||speed>100||pendingJump_>0||groundState()!=AetherCharacterGroundState::OnGround) return false;
  pendingJump_=speed;return true;
}

bool CharacterMotor::update(float moveRight,float moveForward,float yawRadians,
                            float deltaSeconds){
  if(!isReady()||!std::isfinite(moveRight)||!std::isfinite(moveForward)||
      !std::isfinite(yawRadians)||!std::isfinite(deltaSeconds)||deltaSeconds<0) return false;
  float magnitude=std::hypot(moveRight,moveForward);
  if(magnitude>1){moveRight/=magnitude;moveForward/=magnitude;}
  const float sine=std::sin(yawRadians),cosine=std::cos(yawRadians);
  const float desiredX=(cosine*moveRight+sine*moveForward)*settings_.movementUnitsPerSecond;
  const float desiredZ=(-sine*moveRight+cosine*moveForward)*settings_.movementUnitsPerSecond;
  accumulator_=std::min(accumulator_+std::min(deltaSeconds,0.1f),0.1f);
  while(accumulator_>=settings_.fixedStepSeconds){
    AetherVec3 velocity=AetherPhysics_GetCharacterVelocity(world_,character_);
    const AetherCharacterGroundState state=AetherPhysics_GetCharacterGroundState(world_,character_);
    // A steep contact is a wall/slope, not support. Treating it as ground
    // cancels gravity and lets the capsule stick to or climb vertical faces.
    const bool supported=state==AetherCharacterGroundState::OnGround;
    const AetherVec3 ground=supported
        ? AetherPhysics_GetCharacterGroundVelocity(world_,character_):AetherVec3{};
    velocity.x=desiredX+ground.x;
    velocity.z=desiredZ+ground.z;
    if(supported&&pendingJump_>0) velocity.y=ground.y+pendingJump_;
    else if(supported&&velocity.y<ground.y) velocity.y=ground.y-0.1f;
    else velocity.y-=settings_.gravityUnitsPerSecondSquared*settings_.fixedStepSeconds;
    pendingJump_=0;
    AetherPhysics_SetCharacterVelocity(world_,character_,velocity);
    const AetherVec3 gravity{0,-settings_.gravityUnitsPerSecondSquared,0};
    AetherPhysics_UpdateCharacter(world_,character_,settings_.fixedStepSeconds,gravity,
                                  ownsWorld_?AetherQueryLayerMask::Static:AetherQueryLayerMask::All,AetherBodyHandle_Invalid);
    // The owned world is static and CharacterVirtual performs its own broad/
    // narrow-phase queries in ExtendedUpdate. PhysicsSystem::Update would only
    // wake/synchronize the Jolt worker pool with no dynamic body to integrate.
    accumulator_-=settings_.fixedStepSeconds;
  }
  return true;
}

AetherVec3 CharacterMotor::eyePosition() const{
  if(!isReady()) return {};
  AetherVec3 base{};
  AetherPhysics_GetCharacterTransform(world_,character_,&base,nullptr);
  base.y+=settings_.eyeHeight;
  return base;
}

AetherCharacterGroundState CharacterMotor::groundState() const{
  return isReady()?AetherPhysics_GetCharacterGroundState(world_,character_)
                  :AetherCharacterGroundState::InAir;
}

} // namespace ae::physics
