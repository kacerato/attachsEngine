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
  AE_EXPECT_TRUE(back.read(i6,scene::Character::descriptor.version)&&back.acceleration==3&&back.movementMode==2,"current version round trip");
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

namespace {
float bodyYawDegrees(const GameWorld &w,ObjectId id) {float m[16];worldMatrix(w.graph(),id,m);return std::atan2(m[8],m[10])*57.2957795f;}
void steer(GameWorld &w,ScenePhysics &p,ObjectId id,float right,float forward,float cameraYaw,u32 frames=1) {
  for(u32 i=0;i<frames;++i) {p.beginScriptInputFrame();p.setCharacterScriptMove(id,right,forward,cameraYaw);w.beginFrame(1/60.);
    if(!p.advance(1/60.,w)) {AE_EXPECT_TRUE(false,p.error().c_str());return;}}
}
}

// Starter Assets ThirdPersonController (Unity): o corpo gira suavemente para a
// direcao do comando somada ao yaw da camera (RotationSmoothTime 0,12 s).
AE_TEST(character_faces_the_movement_direction_relative_to_the_camera) {
  Course c([](scene::Character &ch){ch.faceMovement=true;ch.rotationSmoothTime=.12f;ch.faceTurnSpeed=720;});
  GameWorld w;w.load(c.g);ScenePhysics p;AE_EXPECT_TRUE(p.start(w),p.error().c_str());
  drive(w,p,c.actor,0,20);
  AE_EXPECT_TRUE(std::abs(bodyYawDegrees(w,c.actor))<.01f,"parado o corpo nao gira");
  steer(w,p,c.actor,1,0,0);
  const float first=bodyYawDegrees(w,c.actor);
  AE_EXPECT_TRUE(first>.5f&&first<30,"o primeiro passo gira pouco: suavizado, sem salto");
  steer(w,p,c.actor,1,0,0,60);
  AE_EXPECT_TRUE(std::abs(bodyYawDegrees(w,c.actor)-90)<1,"direita com a camera em +Z: o corpo olha para +X");
  // Camera girada 90 graus: "frente" do comando vira +X do mundo.
  steer(w,p,c.actor,0,1,3.14159265f,60);
  AE_EXPECT_TRUE(std::abs(std::abs(bodyYawDegrees(w,c.actor))-180)<1,"frente com a camera virada para -Z: o corpo olha para -Z");
  const float facing=bodyYawDegrees(w,c.actor);
  steer(w,p,c.actor,0,0,0,30);
  AE_EXPECT_TRUE(std::abs(bodyYawDegrees(w,c.actor)-facing)<.01f,"ao soltar o comando o corpo fica na ultima direcao");
  p.stop();
}

AE_TEST(character_facing_takes_the_short_way_and_follows_a_circle) {
  Course c([](scene::Character &ch){ch.faceMovement=true;ch.rotationSmoothTime=.12f;ch.faceTurnSpeed=720;ch.speed=2;});
  GameWorld w;w.load(c.g);ScenePhysics p;AE_EXPECT_TRUE(p.start(w),p.error().c_str());
  drive(w,p,c.actor,0,20);
  steer(w,p,c.actor,0,1,3.0f,60);   // ~172 graus
  const float before=bodyYawDegrees(w,c.actor);
  steer(w,p,c.actor,0,1,-3.0f,1);   // ~-172 graus: 16 graus pelo lado de 180, nao 344 pelo zero
  const float after=bodyYawDegrees(w,c.actor);
  const float travelled=std::remainder(after-before,360.0f);
  AE_EXPECT_TRUE(travelled>0&&travelled<16.5f,"cruza 180 graus pelo caminho curto");
  // Andando em circulo (camera girando 90 graus/s), o corpo acompanha com atraso pequeno.
  float cameraYaw=0,worst=0;
  for(u32 i=0;i<240;++i) {
    cameraYaw+=1.5707963f/60;steer(w,p,c.actor,0,1,cameraYaw);
    if(i>60)worst=std::max(worst,std::abs(std::remainder(bodyYawDegrees(w,c.actor)-cameraYaw*57.2957795f,360.0f)));
  }
  AE_EXPECT_TRUE(worst<20,"no circulo o corpo segue a direcao com atraso menor que 20 graus");
  p.stop();
}

AE_TEST(character_facing_off_and_world_reference_keep_their_meaning) {
  Course off;GameWorld w;w.load(off.g);ScenePhysics p;AE_EXPECT_TRUE(p.start(w),p.error().c_str());
  drive(w,p,off.actor,0,20);steer(w,p,off.actor,1,0,0,60);
  AE_EXPECT_TRUE(std::abs(bodyYawDegrees(w,off.actor))<.01f,"desligado o corpo nao gira (comportamento anterior)");
  AE_EXPECT_TRUE(pos(w,off.actor,0)>1,"e continua andando para a direita");
  p.stop();
  Course world([](scene::Character &ch){ch.cameraRelative=false;});
  GameWorld w2;w2.load(world.g);ScenePhysics p2;AE_EXPECT_TRUE(p2.start(w2),p2.error().c_str());
  drive(w2,p2,world.actor,0,20);const float x0=pos(w2,world.actor,0),z0=pos(w2,world.actor,2);
  steer(w2,p2,world.actor,0,1,1.5707963f,40);
  AE_EXPECT_TRUE(pos(w2,world.actor,2)-z0>1&&std::abs(pos(w2,world.actor,0)-x0)<.05f,"sem camera, frente e +Z mesmo com a camera virada");
  p2.stop();
}

AE_TEST(character_version_6_scenes_read_without_facing) {
  scene::Character current;std::ostringstream out;
  for(u32 i=0;i<19;++i)out<<scene::Character::descriptor.numbers[i].read(current)<<' ';
  out<<0<<' '<<0<<' '<<0;
  scene::Character legacy;legacy.faceMovement=true;legacy.cameraRelative=false;std::istringstream in(out.str());
  AE_EXPECT_TRUE(legacy.read(in,6)&&!legacy.faceMovement&&legacy.cameraRelative&&legacy.rotationSmoothTime==.12f,"v6 le sem giro e relativo a camera");
  scene::Character v7;v7.faceMovement=true;v7.cameraRelative=false;v7.rotationSmoothTime=.3f;v7.faceTurnSpeed=360;std::ostringstream o7;v7.write(o7);
  scene::Character back;std::istringstream i7(o7.str());
  AE_EXPECT_TRUE(back.read(i7,7)&&back.faceMovement&&!back.cameraRelative&&back.rotationSmoothTime==.3f&&back.faceTurnSpeed==360,"v7 ida e volta");
  AE_EXPECT_TRUE(scene::Character::descriptor.version==7,"Personagem v7");
}
