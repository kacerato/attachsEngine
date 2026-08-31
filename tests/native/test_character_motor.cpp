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
