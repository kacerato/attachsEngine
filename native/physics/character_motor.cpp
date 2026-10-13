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
      settings.maximumSlopeRadians,settings.fixedStepSeconds,settings.stepHeight,settings.floorSnapLength,spawnEyePosition.x,
      spawnEyePosition.y,spawnEyePosition.z};
  for(float value:values) if(!std::isfinite(value)) return false;
  if(settings.radius<=0||settings.standingHalfHeight<=0||
      settings.eyeHeight<=settings.radius||settings.movementUnitsPerSecond<=0||
      settings.gravityUnitsPerSecondSquared<0||settings.gravityUnitsPerSecondSquared>1000||settings.stepHeight<0||settings.stepHeight>10||settings.floorSnapLength<0||settings.floorSnapLength>10||settings.maximumSlopeRadians<=0||
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
  pendingJump_=0;platformCarry_={};resolvedVelocity_={};hasMeasuredStep_=false;
}

bool CharacterMotor::configureMotion(float speed,float gravity,float stepHeight,float floorSnapLength,bool inheritPlatformHorizontal) {
  if(!isReady()||!std::isfinite(speed)||speed<=0||speed>100||!std::isfinite(gravity)||gravity<0||gravity>1000||!std::isfinite(stepHeight)||stepHeight<0||stepHeight>10||!std::isfinite(floorSnapLength)||floorSnapLength<0||floorSnapLength>10)return false;
  settings_.movementUnitsPerSecond=speed;settings_.gravityUnitsPerSecondSquared=gravity;
  settings_.stepHeight=stepHeight;settings_.floorSnapLength=floorSnapLength;settings_.inheritPlatformHorizontal=inheritPlatformHorizontal;
  if(!inheritPlatformHorizontal)platformCarry_={};
  return true;
}
bool CharacterMotor::configureLocomotion(const CharacterLocomotion &l) {
  if(!std::isfinite(l.acceleration)||!std::isfinite(l.braking)||!std::isfinite(l.airControl)||!std::isfinite(l.swimSpeed)||
     !std::isfinite(l.turnRate)||l.turnRate<0||l.turnRate>720||
     l.acceleration<0||l.braking<0||l.airControl<0||l.airControl>1||l.swimSpeed<=0||l.swimSpeed>1||l.mode>3)return false;
  locomotion_=l;return true;
}
void CharacterMotor::requestVelocity(AetherVec3 v,bool vertical) {
  if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z))return;
  requested_=v;hasRequest_=true;requestVertical_=vertical;
}
void CharacterMotor::setVerticalAxis(float axis) {if(std::isfinite(axis))vertical_=std::clamp(axis,-1.f,1.f);}
void CharacterMotor::setWater(bool present,float surface) {waterPresent_=present&&std::isfinite(surface);waterSurface_=waterPresent_?surface:0;}
bool CharacterMotor::setCrouching(bool crouching) {
  if(!isReady())return false;
  if(crouching==crouching_)return true;
  if(!AetherPhysics_SetCharacterCrouching(world_,character_,crouching?1:0))return false;
  crouching_=crouching;return true;
}
bool CharacterMotor::jump(float speed) {
  if(!isReady()||!std::isfinite(speed)||speed<=0||speed>100||pendingJump_>0||groundState()!=AetherCharacterGroundState::OnGround) return false;
  pendingJump_=speed;return true;
}

CharacterMotor::MotionState CharacterMotor::motionState() const {
  return isReady()?MotionState{AetherPhysics_GetCharacterVelocity(world_,character_),pendingJump_,accumulator_,resolvedVelocity_,hasMeasuredStep_,platformCarry_}:MotionState{};
}
bool CharacterMotor::restoreMotionState(const MotionState &state) {
  if(!isReady()||!std::isfinite(state.velocity.x)||!std::isfinite(state.velocity.y)||!std::isfinite(state.velocity.z)||
    !std::isfinite(state.resolvedVelocity.x)||!std::isfinite(state.resolvedVelocity.y)||!std::isfinite(state.resolvedVelocity.z)||
    !std::isfinite(state.platformCarry.x)||!std::isfinite(state.platformCarry.y)||!std::isfinite(state.platformCarry.z)||state.platformCarry.y!=0||
    !std::isfinite(state.pendingJump)||state.pendingJump<0||state.pendingJump>100||
    !std::isfinite(state.accumulator)||state.accumulator<0||state.accumulator>.1f)return false;
  AetherPhysics_SetCharacterVelocity(world_,character_,state.velocity);
  if(!AetherPhysics_RefreshCharacterContacts(world_,character_,ownsWorld_?AetherQueryLayerMask::Static:AetherQueryLayerMask::All,AetherBodyHandle_Invalid))return false;
  pendingJump_=state.pendingJump;accumulator_=state.accumulator;resolvedVelocity_=state.resolvedVelocity;hasMeasuredStep_=state.hasMeasuredStep;platformCarry_=settings_.inheritPlatformHorizontal?state.platformCarry:AetherVec3{};return true;
}

bool CharacterMotor::update(float moveRight,float moveForward,float yawRadians,
                            float deltaSeconds){
  if(!isReady()||!std::isfinite(moveRight)||!std::isfinite(moveForward)||
      !std::isfinite(yawRadians)||!std::isfinite(deltaSeconds)||deltaSeconds<0) return false;
  float magnitude=std::hypot(moveRight,moveForward);
  if(magnitude>1){moveRight/=magnitude;moveForward/=magnitude;}
  const float sine=std::sin(yawRadians),cosine=std::cos(yawRadians);
  // Dentro da agua (modo nado) a velocidade no plano usa a fracao de nado.
  AetherVec3 base{};AetherPhysics_GetCharacterTransform(world_,character_,&base,nullptr);
  swimming_=locomotion_.mode==2&&waterPresent_&&base.y+settings_.standingHalfHeight<waterSurface_+settings_.radius;
  const float speed=settings_.movementUnitsPerSecond*(swimming_?locomotion_.swimSpeed:1.f);
  float desiredX=(cosine*moveRight+sine*moveForward)*speed;
  float desiredZ=(-sine*moveRight+cosine*moveForward)*speed;
  if(locomotion_.mode==3&&!hasRequest_) {
    // Dirigir: direita/esquerda viram giro, frente/tras viram acelerador; o
    // giro cresce com a velocidade (parado nao gira) e inverte de re.
    const float forwardSpeed=planar_.x*std::sin(heading_)+planar_.z*std::cos(heading_);
    const float grip=std::clamp(std::abs(forwardSpeed)/std::max(.1f,.25f*settings_.movementUnitsPerSecond),0.f,1.f)*(forwardSpeed<0?-1.f:1.f);
    heading_+=moveRight*locomotion_.turnRate*0.0174532925f*std::min(deltaSeconds,0.1f)*grip;
    desiredX=std::sin(heading_)*moveForward*speed;desiredZ=std::cos(heading_)*moveForward*speed;
  }
  if(hasRequest_) {desiredX=requested_.x;desiredZ=requested_.z;}
  accumulator_=std::min(accumulator_+std::min(deltaSeconds,0.1f),0.1f);
  while(accumulator_>=settings_.fixedStepSeconds){
    const float dt=settings_.fixedStepSeconds;
    AetherVec3 velocity=AetherPhysics_GetCharacterVelocity(world_,character_);
    const AetherCharacterGroundState state=AetherPhysics_GetCharacterGroundState(world_,character_);
    // A steep contact is a wall/slope, not support. Treating it as ground
    // cancels gravity and lets the capsule stick to or climb vertical faces.
    const bool supported=state==AetherCharacterGroundState::OnGround;
    const bool flying=locomotion_.mode==1,floating=flying||swimming_;
    const AetherVec3 ground=supported
        ? AetherPhysics_GetCharacterGroundVelocity(world_,character_):AetherVec3{};
    if(supported)platformCarry_=settings_.inheritPlatformHorizontal?AetherVec3{ground.x,0,ground.z}:AetherVec3{};
    // Aceleracao/frenagem no plano, com controle reduzido no ar. Root motion
    // e uma velocidade pedida exata: nao passa pela rampa de aceleracao.
    if(hasRequest_) planar_={desiredX,0,desiredZ};
    else {
      const float dx=desiredX-planar_.x,dz=desiredZ-planar_.z,gap=std::hypot(dx,dz);
      const bool speeding=desiredX*desiredX+desiredZ*desiredZ>=planar_.x*planar_.x+planar_.z*planar_.z&&desiredX*planar_.x+desiredZ*planar_.z>=0;
      // Taxa zero responde na hora. No ar, controle abaixo de 1 limita a taxa
      // (30 m/s² de referencia quando a taxa no chao e instantanea); zero
      // conserva o impulso.
      float rate=speeding?locomotion_.acceleration:locomotion_.braking;bool instant=rate<=0;
      if(!supported&&!floating&&locomotion_.airControl<1) {rate=(instant?30.f:rate)*locomotion_.airControl;instant=false;}
      if(instant||(rate>0&&gap<=rate*dt)) planar_={desiredX,0,desiredZ};
      else if(rate>0&&gap>0) {planar_.x+=dx/gap*rate*dt;planar_.z+=dz/gap*rate*dt;}
    }
    velocity.x=planar_.x+(supported?ground.x:platformCarry_.x);
    velocity.z=planar_.z+(supported?ground.z:platformCarry_.z);
    if(hasRequest_&&requestVertical_) velocity.y=requested_.y;
    else if(flying) velocity.y=vertical_*speed;
    else if(swimming_) {
      // Sem comando, flutua ate a superficie; com comando, sobe/desce.
      const float center=base.y+settings_.standingHalfHeight;
      velocity.y=vertical_!=0?vertical_*speed:std::clamp((waterSurface_-center)*2.f,-1.f,1.f);
    }
    else if(supported&&pendingJump_>0) velocity.y=ground.y+pendingJump_;
    else if(supported&&velocity.y<ground.y) velocity.y=ground.y-0.1f;
    else velocity.y-=settings_.gravityUnitsPerSecondSquared*dt;
    pendingJump_=0;
    AetherPhysics_SetCharacterVelocity(world_,character_,velocity);
    const AetherVec3 gravity{0,floating?0.f:-settings_.gravityUnitsPerSecondSquared,0};
    AetherVec3 before{};AetherPhysics_GetCharacterTransform(world_,character_,&before,nullptr);
    if(!AetherPhysics_UpdateCharacterEx(world_,character_,settings_.fixedStepSeconds,gravity,
      ownsWorld_?AetherQueryLayerMask::Static:AetherQueryLayerMask::All,AetherBodyHandle_Invalid,settings_.stepHeight,settings_.floorSnapLength))return false;
    AetherVec3 after{};AetherPhysics_GetCharacterTransform(world_,character_,&after,nullptr);
    const float inverse=1.f/settings_.fixedStepSeconds;
    resolvedVelocity_={(after.x-before.x)*inverse,(after.y-before.y)*inverse,(after.z-before.z)*inverse};hasMeasuredStep_=true;
    // The owned world is static and CharacterVirtual performs its own broad/
    // narrow-phase queries in ExtendedUpdate. PhysicsSystem::Update would only
    // wake/synchronize the Jolt worker pool with no dynamic body to integrate.
    accumulator_-=settings_.fixedStepSeconds;
  }
  hasRequest_=false;vertical_=0;
  return true;
}

bool CharacterMotor::runtimeState(RuntimeState &out) const {
  if(!isReady())return false;
  out={};AetherPhysics_GetCharacterTransform(world_,character_,&out.position,nullptr);
  out.velocity=resolvedVelocity_;out.hasMeasuredStep=hasMeasuredStep_;out.motorVelocity=AetherPhysics_GetCharacterVelocity(world_,character_);
  out.hasGroundPoint=AetherPhysics_TryGetCharacterGroundPointV1(world_,character_,&out.groundPoint)!=0;
  out.groundBody=AetherPhysics_GetCharacterGroundBodyV1(world_,character_);
  out.hasCapsule=AetherPhysics_TryGetCharacterCapsuleV1(world_,character_,&out.capsuleBottom,&out.capsuleTop,&out.capsuleRadius)!=0;
  out.groundState=AetherPhysics_GetCharacterGroundState(world_,character_);out.groundVelocity=AetherPhysics_GetCharacterGroundVelocity(world_,character_);out.groundNormal=AetherPhysics_GetCharacterGroundNormal(world_,character_);return true;
}

bool CharacterMotor::teleportEye(AetherVec3 eye){
  if(!isReady()||!std::isfinite(eye.x)||!std::isfinite(eye.y)||!std::isfinite(eye.z)) return false;
  AetherPhysics_SetCharacterPositionV1(world_,character_,{eye.x,eye.y-settings_.eyeHeight,eye.z});
  platformCarry_={};return true;
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
