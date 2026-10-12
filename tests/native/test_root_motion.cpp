// N05: root motion. O passo do osso raiz anda com o personagem pela rota que
// o objeto ja tem (transformacao, Personagem ou corpo dinamico); o osso fica
// no lugar; o laco do clipe nao da tranco.
#include "harness.h"
#include "runtime/scene_root_motion.h"
#include "runtime/scene_physics.h"
#include "runtime/transform_math.h"
#include "runtime/game_world.h"
#include "scene/root_motion.h"
#include "scene/physics_body.h"
#include "scene/collider.h"
#include "scene/character.h"
#include <cmath>
#include <sstream>

using namespace ae;
using namespace ae::runtime;
namespace {
struct Walker {
  SceneGraph g;ObjectId owner=0,hips=0;
  explicit Walker(bool vertical=false) {
    owner=g.createEntity(g.root(),ObjectKind::Folder,"Andador");hips=g.createEntity(owner,ObjectKind::Folder,"Quadril");
    Transform t;t.position[1]=1;g.setTransform(hips,t);
    auto v=*g.find(owner);auto *c=static_cast<scene::RootMotion*>(v.components.add(scene::RootMotion::descriptor));c->bone=hips;c->vertical=vertical;
    g.applyEntityValues(owner,v);
  }
  scene::RootMotion &motion() {return *static_cast<scene::RootMotion*>(g.editComponents(owner)->edit(scene::RootMotion::descriptor));}
};
// "Animacao": escreve o quadril como um clipe faria.
void pose(GameWorld &w,ObjectId hips,float z,float yaw=0,float y=1) {
  Transform t;t.position[1]=y;t.position[2]=z;t.rotationDegrees[1]=yaw;w.setLocalTransform(w.handle(hips),t);
}
float at(const GameWorld &w,ObjectId id,u32 axis) {float m[16];worldMatrix(w.graph(),id,m);return m[12+axis];}
}

AE_TEST(root_motion_moves_the_character_and_keeps_the_bone_in_place) {
  Walker k;k.motion().loopJump=.3f;GameWorld world;AE_EXPECT_TRUE(world.load(k.g),"world");SceneRootMotion rm;
  for(u32 f=0;f<=10;++f) {world.beginFrame(1/60.);pose(world,k.hips,.05f*float(f));AE_EXPECT_TRUE(rm.advance(world,1/60.f,nullptr),"advance");}
  AE_EXPECT_TRUE(std::abs(at(world,k.owner,2)-.5f)<1e-3f,"owner advanced by the animated distance");
  AE_EXPECT_TRUE(std::abs(at(world,k.hips,2)-at(world,k.owner,2))<1e-3f,"hips stay over the owner");
  AE_EXPECT_TRUE(!rm.reports().empty()&&rm.reports()[0].route==SceneRootMotion::Route::Transform,"transform route without physics");
  // Laco: o clipe volta ao inicio; o personagem segue com o passo anterior.
  world.beginFrame(1/60.);pose(world,k.hips,0);rm.advance(world,1/60.f,nullptr);
  AE_EXPECT_TRUE(rm.reports()[0].loop&&std::abs(at(world,k.owner,2)-.55f)<1e-3f,"loop jump replaced by the previous step");
  world.beginFrame(1/60.);pose(world,k.hips,.05f);rm.advance(world,1/60.f,nullptr);
  AE_EXPECT_TRUE(std::abs(at(world,k.owner,2)-.6f)<1e-3f,"cycle continues after the loop");
}

AE_TEST(root_motion_turns_with_the_bone_and_respects_axes_and_weight) {
  Walker k;k.motion().weight=.5f;GameWorld world;world.load(k.g);SceneRootMotion rm;
  for(u32 f=0;f<=10;++f) {world.beginFrame(1/60.);pose(world,k.hips,0,3.f*float(f));rm.advance(world,1/60.f,nullptr);}
  float m[16];worldMatrix(world.graph(),k.owner,m);
  const float yaw=std::atan2(m[8],m[10])*180/3.14159265f;
  AE_EXPECT_TRUE(std::abs(yaw-15)<.2f,"half of the 30 degree turn goes to the owner");
  float h[16];worldMatrix(world.graph(),k.hips,h);
  const float hipsYaw=std::atan2(h[8],h[10])*180/3.14159265f;
  AE_EXPECT_TRUE(std::abs(hipsYaw-30)<.3f,"world facing matches the animation (owner + remaining bone turn)");
  Walker v(true);v.motion().horizontal=false;GameWorld w2;w2.load(v.g);SceneRootMotion r2;
  for(u32 f=0;f<=4;++f) {w2.beginFrame(1/60.);pose(w2,v.hips,.1f*float(f),0,1+.05f*float(f));r2.advance(w2,1/60.f,nullptr);}
  AE_EXPECT_TRUE(std::abs(at(w2,v.owner,1)-.2f)<1e-3f&&std::abs(at(w2,v.owner,2))<1e-4f,"only the vertical axis is applied");
  AE_EXPECT_TRUE(std::abs(at(w2,v.hips,2)-.4f)<1e-3f,"unapplied axis stays animated on the bone");
}

AE_TEST(root_motion_drives_physics_body_velocity_and_character) {
  {
    Walker k;auto v=*k.g.find(k.owner);
    auto *body=static_cast<scene::PhysicsBody*>(v.components.add(scene::PhysicsBody::descriptor));body->motion=scene::BodyMotion::Dynamic;body->gravityFactor=0;body->linearDamping=0;
    v.components.add(scene::Collider::descriptor);k.g.applyEntityValues(k.owner,v);
    GameWorld world;world.load(k.g);ScenePhysics physics;AE_EXPECT_TRUE(physics.start(world),physics.error().c_str());SceneRootMotion rm;
    for(u32 f=0;f<=5;++f) {world.beginFrame(1/60.);pose(world,k.hips,.05f*float(f));AE_EXPECT_TRUE(rm.advance(world,1/60.f,&physics),"advance");physics.advance(1/60.,world);}
    float velocity[3];physics.getBodyVelocity(k.owner,velocity);
    AE_EXPECT_TRUE(rm.reports()[0].route==SceneRootMotion::Route::Body&&std::abs(velocity[2]-3)<.05f,"animated step becomes body velocity (0.05 m per 1/60 s)");
    AE_EXPECT_TRUE(at(world,k.owner,2)>.1f,"body moved through the solver");
    physics.stop();
  }
  {
    Walker k;auto v=*k.g.find(k.owner);v.components.add(scene::Character::descriptor);k.g.applyEntityValues(k.owner,v);
    const auto floor=k.g.createEntity(k.g.root(),ObjectKind::Folder,"Chao");auto support=*k.g.find(floor);support.transform.position[1]=-.5f;
    support.components.add(scene::PhysicsBody::descriptor);auto *shape=static_cast<scene::Collider*>(support.components.add(scene::Collider::descriptor));shape->halfX=shape->halfZ=30;shape->halfY=.5f;k.g.applyEntityValues(floor,support);
    GameWorld world;world.load(k.g);ScenePhysics physics;AE_EXPECT_TRUE(physics.start(world),physics.error().c_str());SceneRootMotion rm;
    const float start=at(world,k.owner,2);
    for(u32 f=0;f<=10;++f) {world.beginFrame(1/60.);pose(world,k.hips,.05f*float(f));rm.advance(world,1/60.f,&physics);physics.advance(1/60.,world);}
    AE_EXPECT_TRUE(rm.reports()[0].route==SceneRootMotion::Route::Character,"character route");
    AE_EXPECT_TRUE(std::abs(at(world,k.owner,2)-start-.5f)<2e-2f,"character walked the animated distance");
    physics.stop();
  }
}

AE_TEST(root_motion_reports_missing_bone_and_round_trips) {
  Walker k;k.motion().bone=0;GameWorld world;world.load(k.g);SceneRootMotion rm;world.beginFrame(1/60.);rm.advance(world,1/60.f,nullptr);
  AE_EXPECT_TRUE(!rm.diagnostics().empty(),"missing bone diagnosed");
  scene::RootMotion value;value.bone=7;value.vertical=true;value.loopJump=.8f;std::ostringstream out;value.write(out);
  scene::RootMotion copy;std::istringstream in(out.str());
  AE_EXPECT_TRUE(copy.read(in,1)&&copy.bone==7&&copy.vertical&&std::abs(copy.loopJump-.8f)<1e-6f,"persists");
}
