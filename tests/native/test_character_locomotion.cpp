// N05: locomocao do Personagem. Aceleracao e frenagem, controle no ar,
// voo, nado dentro da agua da cena e agachar com consulta de teto; o
// comportamento anterior (taxas zero, controle 1, modo Solo) fica identico.
#include "harness.h"
#include "runtime/scene_physics.h"
#include "runtime/transform_math.h"
#include "runtime/game_world.h"
#include "scene/character.h"
#include "scene/physics_body.h"
#include "scene/collider.h"
#include <cmath>
#include <sstream>

using namespace ae;
using namespace ae::runtime;
namespace {
struct Course {
  SceneGraph g;ObjectId actor=0,floor=0;
  explicit Course(void (*tune)(scene::Character &)=nullptr) {
    actor=g.createEntity(g.root(),ObjectKind::Folder,"Ator");auto v=*g.find(actor);
    auto *c=static_cast<scene::Character*>(v.components.add(scene::Character::descriptor));c->speed=4;c->control.source=scene::MotorControlSource::Script;
    if(tune)tune(*c);
    g.applyEntityValues(actor,v);
    floor=g.createEntity(g.root(),ObjectKind::Folder,"Chao");auto f=*g.find(floor);f.transform.position[1]=-.5f;
    f.components.add(scene::PhysicsBody::descriptor);auto *box=static_cast<scene::Collider*>(f.components.add(scene::Collider::descriptor));box->halfX=box->halfZ=60;box->halfY=.5f;
    g.applyEntityValues(floor,f);
  }
  ObjectId box(float x,float y,float z,float hx,float hy,float hz) {
    const auto id=g.createEntity(g.root(),ObjectKind::Folder,"Bloco");auto v=*g.find(id);v.transform.position[0]=x;v.transform.position[1]=y;v.transform.position[2]=z;
    v.components.add(scene::PhysicsBody::descriptor);auto *c=static_cast<scene::Collider*>(v.components.add(scene::Collider::descriptor));c->halfX=hx;c->halfY=hy;c->halfZ=hz;
    g.applyEntityValues(id,v);return id;
  }
};
float pos(const GameWorld &w,ObjectId id,u32 axis) {float m[16];worldMatrix(w.graph(),id,m);return m[12+axis];}
// Um quadro: comando de script (frente) e passo fisico.
void drive(GameWorld &w,ScenePhysics &p,ObjectId id,float forward,u32 frames=1) {
  for(u32 i=0;i<frames;++i) {p.beginScriptInputFrame();p.setCharacterScriptMove(id,0,forward,0);w.beginFrame(1/60.);
    if(!p.advance(1/60.,w)) {AE_EXPECT_TRUE(false,p.error().c_str());return;}}
}
}

AE_TEST(character_acceleration_and_braking_ramp_the_planar_speed) {
  Course instant;GameWorld w0;w0.load(instant.g);ScenePhysics p0;AE_EXPECT_TRUE(p0.start(w0),p0.error().c_str());
  drive(w0,p0,instant.actor,0,20);const float z0=pos(w0,instant.actor,2);drive(w0,p0,instant.actor,1,6);
  AE_EXPECT_TRUE(pos(w0,instant.actor,2)-z0>.35f,"zero rates keep the previous instant response");
  p0.stop();
  Course ramp([](scene::Character &c){c.acceleration=4;c.braking=8;});GameWorld w;w.load(ramp.g);ScenePhysics p;AE_EXPECT_TRUE(p.start(w),p.error().c_str());
  drive(w,p,ramp.actor,0,20);const float z=pos(w,ramp.actor,2);drive(w,p,ramp.actor,1,6);
  const float early=pos(w,ramp.actor,2)-z;
  AE_EXPECT_TRUE(early>.005f&&early<.2f,"acceleration ramps instead of jumping to full speed");
  drive(w,p,ramp.actor,1,90);
  physics::CharacterMotor::RuntimeState state;AE_EXPECT_TRUE(p.characterState(w,ramp.actor,state)==WorldStatus::Ok,"state");
  AE_EXPECT_TRUE(std::abs(std::hypot(state.velocity.x,state.velocity.z)-4)<.2f,"reaches the authored speed");
  const float stopStart=pos(w,ramp.actor,2);drive(w,p,ramp.actor,0,60);
  const float slide=pos(w,ramp.actor,2)-stopStart;
  AE_EXPECT_TRUE(slide>.5f&&slide<1.6f,"braking decelerates over distance (v^2/2b = 1 m)");
  p.stop();
}

AE_TEST(character_air_control_zero_keeps_jump_momentum) {
  Course c([](scene::Character &ch){ch.airControl=0;ch.jumpSpeed=6;});GameWorld w;w.load(c.g);ScenePhysics p;AE_EXPECT_TRUE(p.start(w),p.error().c_str());
  drive(w,p,c.actor,0,20);drive(w,p,c.actor,1,30);
  p.beginScriptInputFrame();p.setCharacterScriptMove(c.actor,0,1,0);AE_EXPECT_TRUE(p.jumpCharacter(c.actor,&w),"jump queued");w.beginFrame(1/60.);p.advance(1/60.,w);
  drive(w,p,c.actor,1,3);const float z=pos(w,c.actor,2);
  drive(w,p,c.actor,-1,10);
  AE_EXPECT_TRUE(pos(w,c.actor,2)>z+.3f,"reversing input in the air does not stop the jump momentum");
  p.stop();
}

AE_TEST(character_fly_and_swim_modes_use_the_vertical_axis_and_water) {
  Course fly([](scene::Character &ch){ch.movementMode=1;});GameWorld w;w.load(fly.g);ScenePhysics p;AE_EXPECT_TRUE(p.start(w),p.error().c_str());
  drive(w,p,fly.actor,0,10);const float y=pos(w,fly.actor,1);
  for(u32 i=0;i<30;++i){AE_EXPECT_TRUE(p.setCharacterVertical(fly.actor,1),"vertical axis");drive(w,p,fly.actor,0);}
  AE_EXPECT_TRUE(pos(w,fly.actor,1)>y+1.5f,"flying climbs with the vertical axis");
  const float hover=pos(w,fly.actor,1);drive(w,p,fly.actor,0,30);
  AE_EXPECT_TRUE(std::abs(pos(w,fly.actor,1)-hover)<.01f,"flying ignores gravity");
  p.stop();
  // Nado: agua de 3 m acima do chao; sem comando o corpo sobe ate a superficie.
  Course swim([](scene::Character &ch){ch.movementMode=2;});GameWorld ws;ws.load(swim.g);ScenePhysics ps;AE_EXPECT_TRUE(ps.start(ws),ps.error().c_str());
  ps.setWaterQuery([](float,float,float &surface){surface=3;return true;});
  drive(ws,ps,swim.actor,0,240);
  bool swimming=false;AE_EXPECT_TRUE(ps.characterInWater(swim.actor,swimming)&&swimming,"inside the water volume");
  AE_EXPECT_TRUE(pos(ws,swim.actor,1)>1.5f,"buoyant drift toward the surface");
  const float z=pos(ws,swim.actor,2);drive(ws,ps,swim.actor,1,60);
  const float swum=pos(ws,swim.actor,2)-z;
  AE_EXPECT_TRUE(swum>1.8f&&swum<2.8f,"swim speed is the authored fraction (0.6 x 4 m/s)");
  ps.setWaterQuery({});drive(ws,ps,swim.actor,0,120);
  AE_EXPECT_TRUE(ps.characterInWater(swim.actor,swimming)&&!swimming&&pos(ws,swim.actor,1)<.5f,"without water the character falls back to ground mode");
  ps.stop();
}

AE_TEST(character_crouch_refuses_to_stand_under_a_low_ceiling) {
  Course c;c.box(0,1.9f,3,2,.2f,1);GameWorld w;w.load(c.g);ScenePhysics p;AE_EXPECT_TRUE(p.start(w),p.error().c_str());
  drive(w,p,c.actor,0,20);
  AE_EXPECT_TRUE(p.setCharacterCrouching(c.actor,true),"crouch");
  drive(w,p,c.actor,1,45);
  AE_EXPECT_TRUE(pos(w,c.actor,2)>2.5f,"crouched capsule passes under the low ceiling");
  AE_EXPECT_TRUE(!p.setCharacterCrouching(c.actor,false),"standing up is refused without headroom");
  drive(w,p,c.actor,1,45);
  AE_EXPECT_TRUE(p.setCharacterCrouching(c.actor,false),"stands up once clear");
  p.stop();
}

AE_TEST(character_version_5_scenes_keep_their_behavior) {
  scene::Character current;std::ostringstream out;
  for(u32 i=0;i<14;++i)out<<scene::Character::descriptor.numbers[i].read(current)<<' ';
  out<<0<<' '<<0;
  scene::Character legacy;std::istringstream in(out.str());
  AE_EXPECT_TRUE(legacy.read(in,5)&&legacy.acceleration==0&&legacy.braking==0&&legacy.airControl==1&&legacy.movementMode==0,"v5 reads with instant response and ground mode");
  scene::Character v6;v6.acceleration=3;v6.movementMode=2;std::ostringstream o6;v6.write(o6);
  scene::Character back;std::istringstream i6(o6.str());
  AE_EXPECT_TRUE(back.read(i6,6)&&back.acceleration==3&&back.movementMode==2,"v6 round trip");
}

AE_TEST(character_drive_mode_turns_with_speed_and_never_strafes) {
  Course c([](scene::Character &ch){ch.movementMode=3;ch.turnRate=90;ch.acceleration=6;ch.braking=8;});
  GameWorld w;w.load(c.g);ScenePhysics p;AE_EXPECT_TRUE(p.start(w),p.error().c_str());
  drive(w,p,c.actor,0,20);
  const float x0=pos(w,c.actor,0),z0=pos(w,c.actor,2);
  for(u32 i=0;i<30;++i){p.beginScriptInputFrame();p.setCharacterScriptMove(c.actor,1,0,0);w.beginFrame(1/60.);p.advance(1/60.,w);}
  AE_EXPECT_TRUE(std::hypot(pos(w,c.actor,0)-x0,pos(w,c.actor,2)-z0)<.01f,"steering at standstill neither strafes nor turns in place");
  drive(w,p,c.actor,1,60);
  AE_EXPECT_TRUE(pos(w,c.actor,2)>z0+1,"throttle drives forward");
  for(u32 i=0;i<60;++i){p.beginScriptInputFrame();p.setCharacterScriptMove(c.actor,1,1,0);w.beginFrame(1/60.);p.advance(1/60.,w);}
  float m[16];worldMatrix(w.graph(),c.actor,m);
  const float yaw=std::atan2(m[8],m[10])*180/3.14159265f;
  AE_EXPECT_TRUE(std::abs(yaw)>30,"vehicle heading turned while moving");
  AE_EXPECT_TRUE(pos(w,c.actor,0)>x0+.3f,"path curves toward the steering side");
  p.stop();
}
