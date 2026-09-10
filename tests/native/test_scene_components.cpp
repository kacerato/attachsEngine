// This translation unit deliberately includes no editor, renderer or UI header.
#include "scene/physics_body.h"
#include "scene/character.h"
#include "scene/camera_look.h"
#include "harness.h"

using namespace ae;
AE_TEST(scene_components_load_and_clone_without_editor_dependency) {
  const scene::ComponentType *registry[]{&scene::PhysicsBody::descriptor,&scene::Character::descriptor,&scene::CameraLook::descriptor};
  scene::Components authored;
  auto *body=static_cast<scene::PhysicsBody*>(authored.edit(scene::PhysicsBody::descriptor));
  body->mass=17;body->motion=scene::BodyMotion::Dynamic;
  auto *actor=static_cast<scene::Character*>(authored.edit(scene::Character::descriptor));
  actor->jumpSpeed=6;
  auto *look=static_cast<scene::CameraLook*>(authored.edit(scene::CameraLook::descriptor));
  look->pitchLimit=70;
  std::ostringstream out;AE_EXPECT_TRUE(authored.write(out),"write shared contract");
  scene::Components execution;std::istringstream input(out.str());
  AE_EXPECT_TRUE(execution.read(input,registry),"load without editor");
  auto *runtimeBody=static_cast<scene::PhysicsBody*>(execution.edit(scene::PhysicsBody::descriptor));
  AE_EXPECT_EQ(runtimeBody->mass,17.0f,"physical data preserved");
  runtimeBody->mass=2;
  AE_EXPECT_EQ(body->mass,17.0f,"execution ownership independent");
  AE_EXPECT_EQ(static_cast<const scene::Character*>(execution.find(scene::Character::descriptor))->jumpSpeed,6.0f,"character data preserved");
  AE_EXPECT_EQ(static_cast<const scene::CameraLook*>(execution.find(scene::CameraLook::descriptor))->pitchLimit,70.0f,"camera data preserved");
  const auto saved=out.str();
  std::istringstream unknown("1 \"unknown.component\" 1 \"\"");
  AE_EXPECT_TRUE(!execution.read(unknown,registry),"unsupported type rejected transactionally");
  AE_EXPECT_EQ(runtimeBody->mass,2.0f,"failed load retains existing data");
  std::ostringstream unchanged;AE_EXPECT_TRUE(authored.write(unchanged),"authoring remains serializable");
  AE_EXPECT_EQ(unchanged.str(),saved,"authoring unchanged");
}
