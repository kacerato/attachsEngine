#include "harness.h"
#include "physics/character_motor.h"

#include <array>

using namespace ae::test;

AE_TEST(Character_motor_falls_to_ground_and_stops_at_static_wall) {
  const std::array<ae::renderer::CollisionVertex,8> vertices{{
      {-10,0,-10},{-10,0,10},{10,0,10},{10,0,-10},
      {2,0,-3},{2,10,-3},{2,10,3},{2,0,3}}};
  // Floor faces +Y; wall faces -X toward the spawn.
  const std::array<ae::u32,12> indices{{0,1,2,0,2,3,4,6,5,4,7,6}};
  ae::physics::CharacterMotor motor;
  AE_EXPECT_TRUE(motor.initialize(vertices,indices,{0,4,0}),"motor should initialize");
  for(int frame=0;frame<360;++frame) motor.update(1,0,0,1.0f/60.0f);
  const AetherVec3 eye=motor.eyePosition();
  AE_EXPECT_TRUE(eye.y>1.5f&&eye.y<1.8f,"capsule should settle on floor");
  AE_EXPECT_TRUE(eye.x<1.7f,"capsule should not cross wall");
  AE_EXPECT_TRUE(motor.groundState()==AetherCharacterGroundState::OnGround,
                 "settled capsule should report ground");
}

AE_TEST(Character_motor_borrows_scene_world_and_collides_with_kinematic_bodies) {
  auto *world=AetherPhysics_CreateWorld({0,-9.81f,0},64);
  AE_EXPECT_TRUE(world!=nullptr,"scene world");
  AetherBodyDesc floor{};floor.shape.kind=AetherShapeKind::Box;
  floor.shape.boxHalfExtent={20,.5f,20};floor.position={0,-.5f,0};floor.rotation={0,0,0,1};
  floor.motionType=AetherMotionType::Static;floor.friction=.5f;
  const auto floorHandle=AetherPhysics_CreateBody(world,&floor);
  AetherBodyDesc wall=floor;wall.position={2,5,0};wall.shape.boxHalfExtent={.25f,5,10};
  wall.motionType=AetherMotionType::Kinematic;
  const auto wallHandle=AetherPhysics_CreateBody(world,&wall);
  AE_EXPECT_TRUE(floorHandle!=AetherBodyHandle_Invalid && wallHandle!=AetherBodyHandle_Invalid,"scene colliders");
  ae::physics::CharacterMotor first,second;
  AE_EXPECT_TRUE(first.initializeInWorld(world,{0,4,-2}) && second.initializeInWorld(world,{0,4,2}),"two motors share scene");
  for(int tick=0;tick<180;++tick) {
    AE_EXPECT_TRUE(first.update(1,0,0,1.0f/60.0f) && second.update(1,0,0,1.0f/60.0f),"shared tick");
    AE_EXPECT_TRUE(AetherPhysics_StepV2(world,1.0f/60.0f,1)==0,"owner steps rigid bodies once");
  }
  AE_EXPECT_TRUE(first.eyePosition().x<1.5f && second.eyePosition().x<1.5f,"kinematic wall blocks both characters");
  AE_EXPECT_TRUE(first.groundState()==AetherCharacterGroundState::OnGround,"scene floor supports character");
  first.shutdown();
  AetherVec3 position;AetherQuat rotation;
  AE_EXPECT_TRUE(AetherPhysics_TryGetBodyPoseV2(world,wallHandle,&position,&rotation),"motor shutdown preserves scene bodies");
  AE_EXPECT_TRUE(second.update(0,0,0,1.0f/60.0f),"other character survives");
  second.shutdown();
  AE_EXPECT_TRUE(AetherPhysics_TryGetBodyPoseV2(world,floorHandle,&position,&rotation),"scene still owns floor");
  AetherPhysics_DestroyWorld(world);
}
