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
AE_TEST(character_ground_step_height_changes_real_stair_traversal_without_replacing_capsule) {
  auto*world=AetherPhysics_CreateWorld({0,-9.81f,0},64);AE_EXPECT_TRUE(world,"Jolt world");
  AetherBodyDesc floor{};floor.shape.kind=AetherShapeKind::Box;floor.shape.boxHalfExtent={20,.5f,20};floor.position={0,-.5f,0};floor.rotation={0,0,0,1};floor.motionType=AetherMotionType::Static;floor.friction=.5f;
  AE_EXPECT_TRUE(AetherPhysics_CreateBody(world,&floor)!=AetherBodyHandle_Invalid,"floor");auto stair=floor;stair.position={3,.15f,0};stair.shape.boxHalfExtent={1.5f,.15f,10};AE_EXPECT_TRUE(AetherPhysics_CreateBody(world,&stair)!=AetherBodyHandle_Invalid,"30cm stair");
  ae::physics::CharacterMotorSettings settings;settings.radius=.3f;settings.standingHalfHeight=.5f;settings.eyeHeight=1.4f;settings.movementUnitsPerSecond=2;settings.stepHeight=0;
  ae::physics::CharacterMotor low,high;AE_EXPECT_TRUE(low.initializeInWorld(world,{0,3,-2},settings)&&high.initializeInWorld(world,{0,3,2},settings),"independent capsules");
  for(int i=0;i<120;++i){low.update(0,0,0,1.f/60);high.update(0,0,0,1.f/60);}
  AE_EXPECT_TRUE(high.configureMotion(2,24,.4f,.5f),"live motion settings keep capsule");float highest=0;
  for(int i=0;i<180;++i){AE_EXPECT_TRUE(low.update(1,0,0,1.f/60)&&high.update(1,0,0,1.f/60),"actual stair movement");highest=std::max(highest,high.eyePosition().y);}
  std::printf("CHARACTER_STEP: disabled x=%.3f, enabled x=%.3f, max eye=%.3f\n",low.eyePosition().x,high.eyePosition().x,highest);
  AE_EXPECT_TRUE(low.eyePosition().x<1.5f&&high.eyePosition().x>4.5f&&highest>1.65f,"step setting changes collision result");
  const auto before=high.eyePosition();AE_EXPECT_TRUE(!high.configureMotion(2,24,11,.5f)&&high.eyePosition().x==before.x,"out-of-domain step leaves motor unchanged");
  low.shutdown();high.shutdown();AetherPhysics_DestroyWorld(world);
}
AE_TEST(character_ground_snap_and_zero_gravity_change_real_ledge_descent) {
  auto*world=AetherPhysics_CreateWorld({0,-9.81f,0},64);AE_EXPECT_TRUE(world,"Jolt world");AetherBodyDesc floor{};floor.shape.kind=AetherShapeKind::Box;floor.shape.boxHalfExtent={20,.5f,20};floor.position={0,-.5f,0};floor.rotation={0,0,0,1};floor.motionType=AetherMotionType::Static;floor.friction=.5f;AetherPhysics_CreateBody(world,&floor);
  auto ledge=floor;ledge.position={-2,.15f,0};ledge.shape.boxHalfExtent={3,.15f,10};AetherPhysics_CreateBody(world,&ledge);
  ae::physics::CharacterMotorSettings settings;settings.radius=.3f;settings.standingHalfHeight=.5f;settings.eyeHeight=1.4f;settings.movementUnitsPerSecond=2;settings.stepHeight=0;
  ae::physics::CharacterMotor free,snap;AE_EXPECT_TRUE(free.initializeInWorld(world,{0,3,-2},settings)&&snap.initializeInWorld(world,{0,3,2},settings),"capsules");for(int i=0;i<120;++i){free.update(0,0,0,1.f/60);snap.update(0,0,0,1.f/60);}
  AE_EXPECT_TRUE(free.configureMotion(2,0,0,0)&&snap.configureMotion(2,0,0,.5f),"zero gravity and separate floor snap");for(int i=0;i<60;++i){free.update(1,0,0,1.f/60);snap.update(1,0,0,1.f/60);}
  std::printf("CHARACTER_SNAP: disabled eye=%.3f, enabled eye=%.3f\n",free.eyePosition().y,snap.eyePosition().y);
  AE_EXPECT_TRUE(free.eyePosition().y>snap.eyePosition().y+.15f&&snap.groundState()==AetherCharacterGroundState::OnGround,"snap produces support on lower floor while zero-gravity capsule remains above it");
  const auto before=free.eyePosition();AE_EXPECT_TRUE(!AetherPhysics_UpdateCharacterEx(world,AetherCharacterHandle_Invalid,.016f,{0,-9.81f,0},AetherQueryLayerMask::All,AetherBodyHandle_Invalid,.4f,.5f)&&free.eyePosition().y==before.y,"invalid bridge handle cannot advance");
  free.shutdown();snap.shutdown();AetherPhysics_DestroyWorld(world);
}

AE_TEST(character_state_measures_resolved_displacement_and_support_lifecycle) {
  auto *world=AetherPhysics_CreateWorld({0,-9.81f,0},64);AE_EXPECT_TRUE(world,"real world");
  AetherBodyDesc floor{};floor.shape.kind=AetherShapeKind::Box;floor.shape.boxHalfExtent={20,.5f,20};floor.position={0,-.5f,0};floor.rotation={0,0,0,1};floor.motionType=AetherMotionType::Static;
  AE_EXPECT_TRUE(AetherPhysics_CreateBody(world,&floor)!=AetherBodyHandle_Invalid,"floor");auto wall=floor;wall.position={2,5,0};wall.shape.boxHalfExtent={.25f,5,10};AE_EXPECT_TRUE(AetherPhysics_CreateBody(world,&wall)!=AetherBodyHandle_Invalid,"wall");
  ae::physics::CharacterMotor motor;AE_EXPECT_TRUE(motor.initializeInWorld(world,{0,4,0}),"motor");ae::physics::CharacterMotor::RuntimeState state;
  AE_EXPECT_TRUE(motor.runtimeState(state)&&!state.hasMeasuredStep,"no fabricated initial sample");
  for(int i=0;i<180;++i)AE_EXPECT_TRUE(motor.update(1,0,0,1.f/60),"collide");
  AE_EXPECT_TRUE(motor.runtimeState(state)&&state.hasMeasuredStep&&state.groundState==AetherCharacterGroundState::OnGround&&state.groundNormal.y>.9f,"actual support normal");
  AE_EXPECT_TRUE(std::abs(state.velocity.x)<.01f&&state.motorVelocity.x>7,"wall resolves requested velocity to stopped displacement");
  const auto before=state.position;AE_EXPECT_TRUE(motor.jump(5)&&motor.update(0,0,0,1.f/60)&&motor.runtimeState(state),"jump and query");
  AE_EXPECT_TRUE(state.position.y>before.y&&state.velocity.y>0&&state.groundState!=AetherCharacterGroundState::OnGround,"upward resolved step is airborne");
  const auto snapshot=motor.motionState();AE_EXPECT_TRUE(snapshot.hasMeasuredStep&&snapshot.resolvedVelocity.y==state.velocity.y,"rebuild snapshot carries measured sample");
  motor.shutdown();AE_EXPECT_TRUE(!motor.runtimeState(state),"ended motor rejects query");AetherPhysics_DestroyWorld(world);
}

AE_TEST(character_platform_follows_kinematic_support_translation_and_reports_surface_velocity) {
  for(int axis=0;axis<2;++axis) {
    auto *world=AetherPhysics_CreateWorld({0,-9.81f,0},64);AE_EXPECT_TRUE(world,"real platform world");
    AetherBodyDesc platform{};platform.shape.kind=AetherShapeKind::Box;platform.shape.boxHalfExtent={5,.5f,5};platform.position={0,-.5f,0};platform.rotation={0,0,0,1};platform.motionType=AetherMotionType::Kinematic;
    const auto body=AetherPhysics_CreateBody(world,&platform);AE_EXPECT_TRUE(body!=AetherBodyHandle_Invalid,"kinematic support");ae::physics::CharacterMotor motor;AE_EXPECT_TRUE(motor.initializeInWorld(world,{0,4,0}),"actual capsule");
    for(int i=0;i<180;++i){AE_EXPECT_TRUE(motor.update(0,0,0,1.f/60),"settle");AE_EXPECT_TRUE(AetherPhysics_StepV2(world,1.f/60,1)==0,"scene tick");}
    const auto before=motor.eyePosition();
    for(int i=1;i<=120;++i){const float distance=i/60.f;AetherVec3 target{axis==0?distance:0,axis==1?distance-.5f:-.5f,0};
      AE_EXPECT_TRUE(AetherPhysics_MoveKinematicV2(world,body,target,{0,0,0,1},1.f/60),"move support through real solver");
      AE_EXPECT_TRUE(motor.update(0,0,0,1.f/60)&&AetherPhysics_StepV2(world,1.f/60,1)==0,"same ordering as ScenePhysics");
    }
    ae::physics::CharacterMotor::RuntimeState state;AE_EXPECT_TRUE(motor.runtimeState(state),"actual final snapshot");const float distance=axis==0?motor.eyePosition().x-before.x:motor.eyePosition().y-before.y;const float surface=axis==0?state.groundVelocity.x:state.groundVelocity.y;
    std::printf("CHARACTER_PLATFORM: axis=%d displacement=%.3f surface=%.3f state=%u\n",axis,distance,surface,static_cast<ae::u32>(state.groundState));
    AE_EXPECT_TRUE(distance>1.8f&&distance<2.2f&&surface>.9f&&surface<1.1f&&state.groundState==AetherCharacterGroundState::OnGround,"support carries character and reports point velocity");
    motor.shutdown();AetherPhysics_DestroyWorld(world);
  }
}

AE_TEST(character_platform_rotating_support_carries_capsule_with_contact_point_velocity) {
  auto *world=AetherPhysics_CreateWorld({0,-9.81f,0},64);AE_EXPECT_TRUE(world,"world");
  AetherBodyDesc platform{};platform.shape.kind=AetherShapeKind::Box;platform.shape.boxHalfExtent={5,.5f,5};platform.position={0,-.5f,0};platform.rotation={0,0,0,1};platform.motionType=AetherMotionType::Kinematic;
  const auto body=AetherPhysics_CreateBody(world,&platform);ae::physics::CharacterMotor motor;AE_EXPECT_TRUE(body!=AetherBodyHandle_Invalid&&motor.initializeInWorld(world,{2,4,0}),"rotating platform and capsule");
  for(int i=0;i<180;++i){motor.update(0,0,0,1.f/60);AetherPhysics_StepV2(world,1.f/60,1);}
  for(int i=1;i<=120;++i){const float angle=i*.5f/60;AE_EXPECT_TRUE(AetherPhysics_MoveKinematicV2(world,body,{0,-.5f,0},{0,std::sin(angle*.5f),0,std::cos(angle*.5f)},1.f/60),"native rotation command");AE_EXPECT_TRUE(motor.update(0,0,0,1.f/60)&&AetherPhysics_StepV2(world,1.f/60,1)==0,"physics tick");}
  ae::physics::CharacterMotor::RuntimeState state;AE_EXPECT_TRUE(motor.runtimeState(state),"snapshot");
  std::printf("CHARACTER_PLATFORM_ROTATION: position=(%.3f,%.3f) support=(%.3f,%.3f)\n",state.position.x,state.position.z,state.groundVelocity.x,state.groundVelocity.z);
  AE_EXPECT_TRUE(state.groundState==AetherCharacterGroundState::OnGround&&state.position.x>.9f&&state.position.x<1.3f&&state.position.z<-1.5f,"rotation transports offset capsule");
  AE_EXPECT_TRUE(std::abs(state.groundVelocity.x-.5f*state.position.z)<.04f&&std::abs(state.groundVelocity.z+.5f*state.position.x)<.04f,"support velocity includes angular motion at contact point");
  motor.shutdown();AetherPhysics_DestroyWorld(world);
}

AE_TEST(character_platform_carry_horizontal_option_changes_airborne_motion_and_rebuild_state) {
  float travelled[2]{};
  for(int inherit=0;inherit<2;++inherit) {
    auto *world=AetherPhysics_CreateWorld({0,-9.81f,0},64);AetherBodyDesc platform{};platform.shape.kind=AetherShapeKind::Box;platform.shape.boxHalfExtent={5,.5f,5};platform.position={0,-.5f,0};platform.rotation={0,0,0,1};platform.motionType=AetherMotionType::Kinematic;const auto body=AetherPhysics_CreateBody(world,&platform);
    ae::physics::CharacterMotorSettings settings;settings.inheritPlatformHorizontal=inherit!=0;ae::physics::CharacterMotor motor;AE_EXPECT_TRUE(body!=AetherBodyHandle_Invalid&&motor.initializeInWorld(world,{0,4,0},settings),"real platform and policy");
    for(int i=0;i<180;++i){motor.update(0,0,0,1.f/60);AetherPhysics_StepV2(world,1.f/60,1);}
    for(int i=1;i<=60;++i){AetherPhysics_MoveKinematicV2(world,body,{i/60.f,-.5f,0},{0,0,0,1},1.f/60);motor.update(0,0,0,1.f/60);AetherPhysics_StepV2(world,1.f/60,1);}
    const auto before=motor.eyePosition();AE_EXPECT_TRUE(motor.jump(8),"supported jump");
    for(int i=61;i<=90;++i){AetherPhysics_MoveKinematicV2(world,body,{i/60.f,-.5f,0},{0,0,0,1},1.f/60);AE_EXPECT_TRUE(motor.update(0,0,0,1.f/60)&&AetherPhysics_StepV2(world,1.f/60,1)==0,"airborne tick");}
    travelled[inherit]=motor.eyePosition().x-before.x;
    if(inherit){const auto eye=motor.eyePosition();const auto saved=motor.motionState();AE_EXPECT_TRUE(saved.platformCarry.x>.9f&&motor.groundState()!=AetherCharacterGroundState::OnGround,"carry is runtime scalar");motor.shutdown();AE_EXPECT_TRUE(motor.initializeInWorld(world,eye,settings)&&motor.restoreMotionState(saved)&&motor.update(0,0,0,1.f/60),"compatible rebuild restores carry");AE_EXPECT_TRUE(motor.eyePosition().x>eye.x+.01f,"restored carry affects next resolved step");const auto now=motor.eyePosition();AE_EXPECT_TRUE(motor.configureMotion(8,24,.4f,.5f,false)&&motor.update(0,0,0,1.f/60)&&std::abs(motor.eyePosition().x-now.x)<.001f,"live disable clears only horizontal platform carry");}
    motor.shutdown();AetherPhysics_DestroyWorld(world);
  }
  std::printf("CHARACTER_PLATFORM_CARRY: legacy=%.3f inherit=%.3f\n",travelled[0],travelled[1]);AE_EXPECT_TRUE(travelled[0]<.04f&&travelled[1]>.45f,"option conserves horizontal support movement in air");
}
