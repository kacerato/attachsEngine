#include "harness.h"
#include "physics/jolt_bridge.h"
#include "runtime/scene_physics.h"
#include "scene/physics_body.h"

#include <cmath>
#include <sstream>
#include <string>
#include <vector>

using namespace ae;using namespace ae::runtime;

namespace {
AetherBodyHandle box(AetherPhysicsWorld *world,float x) {
  AetherBodyDescV2 d{};d.structSize=sizeof(d);d.apiVersion=AetherBodyApiVersionV2;d.shape.kind=AetherShapeKind::Box;d.shape.boxHalfExtent={.5f,.5f,.5f};
  d.position={x,0,0};d.rotation={0,0,0,1};d.motionType=AetherMotionType::Dynamic;d.friction=.5f;d.eventLayerMask=3;
  return AetherPhysics_CreateBodyV2(world,&d);
}
AetherBodyStateV1 state(AetherPhysicsWorld *world,AetherBodyHandle body) {AetherBodyStateV1 s{};AetherPhysics_BodyCommandV1(world,body,0,{},{},&s);return s;}
float angular(const AetherBodyStateV1 &s) {return std::sqrt(s.angular.x*s.angular.x+s.angular.y*s.angular.y+s.angular.z*s.angular.z);}
}

AE_TEST(body_center_of_mass_and_inertia_change_the_real_jolt_response) {
  AetherPhysicsWorld *world=AetherPhysics_CreateWorld({0,0,0},16);
  const auto automatic=box(world,0),shifted=box(world,5),heavy=box(world,10);
  AE_EXPECT_TRUE(AetherPhysics_SetMassV2(world,automatic,2)!=0,"massa automática");
  const AetherVec3 center{.4f,0,0},inertia{1,1,1},large{10,10,10};
  AE_EXPECT_TRUE(AetherPhysics_SetBodyMassPropertiesV1(world,shifted,2,&center,&inertia)!=0,"centro e inércia manuais");
  AE_EXPECT_TRUE(AetherPhysics_SetBodyMassPropertiesV1(world,heavy,2,&center,&large)!=0,"inércia dez vezes maior");
  const auto s=state(world,shifted);
  AE_EXPECT_TRUE(std::abs(s.centerOfMass.x-5.4f)<1e-4f&&std::abs(s.centerOfMass.y)<1e-4f,"centro de massa no ponto local autoral");
  // Impulso vertical aplicado na origem do objeto: só gira quem tem o centro deslocado.
  for(const auto body:{automatic,shifted,heavy}) {
    AetherBodyStateV1 ignored{};const auto origin=state(world,body).centerOfMass;
    const AetherVec3 at{body==automatic?0.f:body==shifted?5.f:10.f,0,0};(void)origin;
    AetherPhysics_BodyCommandV1(world,body,5,{0,1,0},at,&ignored);
  }
  const float a0=angular(state(world,automatic)),a1=angular(state(world,shifted)),a2=angular(state(world,heavy));
  AE_EXPECT_TRUE(a0<1e-4f,"centro automático no meio da caixa: o impulso não gira");
  AE_EXPECT_TRUE(a1>.1f,"centro deslocado: o mesmo impulso gira");
  AE_EXPECT_TRUE(std::abs(a1/a2-10)<.5f,"inércia dez vezes maior gira dez vezes menos");
  const AetherVec3 bad{0,0,0};
  AE_EXPECT_TRUE(AetherPhysics_SetBodyMassPropertiesV1(world,shifted,2,nullptr,&bad)==0,"inércia zero recusada");
  AetherPhysics_DestroyWorld(world);

  scene::PhysicsBody body;body.automaticCenterOfMass=false;body.centerOfMass[1]=-.3f;body.automaticInertia=false;body.inertia[2]=7;body.interpolation=1;
  std::stringstream v8;body.write(v8);scene::PhysicsBody back;
  AE_EXPECT_TRUE(back.read(v8,8)&&!back.automaticCenterOfMass&&back.centerOfMass[1]==-.3f&&back.inertia[2]==7&&back.interpolation==1,"corpo v8 relido");
  std::vector<std::string> tokens;{std::istringstream in(v8.str());std::string w;while(in>>w)tokens.push_back(w);}
  std::string v7;for(usize i=0;i+3<tokens.size();++i) if(i<16||i>=22) v7+=tokens[i]+' ';  // sem os 6 números novos nem o bloco final
  std::istringstream old(v7);scene::PhysicsBody previous;previous.interpolation=2;previous.inertia[0]=5;
  AE_EXPECT_TRUE(previous.read(old,7)&&previous.automaticCenterOfMass&&previous.automaticInertia&&previous.interpolation==0&&previous.inertia[0]==1,"corpo v7 lê com massa automática e sem interpolação");
}

AE_TEST(body_interpolation_publishes_a_pose_between_physics_steps) {
  SceneGraph g;
  const auto falling=[&](const char *name,float x,u32 interpolation){
    const auto id=g.createEntity(g.root(),ObjectKind::Folder,name);auto v=*g.find(id);v.transform.position[0]=x;v.transform.position[1]=10;
    auto *b=static_cast<scene::PhysicsBody*>(v.components.add(scene::PhysicsBody::descriptor));b->motion=scene::BodyMotion::Dynamic;b->interpolation=interpolation;
    v.components.add(scene::Collider::descriptor);g.applyEntityValues(id,v);return id;};
  const auto plain=falling("Sem interpolação",0,0),smooth=falling("Interpolado",3,1),ahead=falling("Extrapolado",6,2);
  GameWorld w;ScenePhysics physics;AE_EXPECT_TRUE(w.load(g)&&physics.start(w),physics.error().c_str());
  const auto y=[&](ObjectId id){Transform t;w.localTransform(w.handle(id),t);return t.position[1];};
  for(int i=0;i<30;++i) physics.advance(1.0/60,w);
  physics.advance(1.0/120,w);  // meio passo: nenhum passo físico novo
  const float now=y(plain),lag=y(smooth),lead=y(ahead);
  AE_EXPECT_TRUE(lag>now&&lag-now<.2f,"interpolado fica entre o passo anterior e o último (mais alto ao cair)");
  AE_EXPECT_TRUE(lead<now&&now-lead<.2f,"extrapolado projeta pela velocidade (mais baixo ao cair)");
  physics.advance(1.0/120,w);  // completa o passo: o interpolado mostra o passo anterior (atraso de um passo, como na Unity)
  const float gap=y(smooth)-y(plain);
  AE_EXPECT_TRUE(gap>0&&gap<.2f,"na fronteira do passo o atraso é de no máximo um passo");
}
