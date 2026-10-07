#include "harness.h"
#include "physics/jolt_bridge.h"
#include "resources/physics_material.h"
#include "scene/physics_body.h"

#include <algorithm>
#include <cmath>
#include <sstream>

using namespace ae;

AE_TEST(physics_material_asset_round_trips_and_rejects_invalid_values) {
  resources::PhysicsMaterialAsset material;material.guid=resources::assetGuidFromSeed("gelo");material.name="Gelo \"liso\"";
  material.friction=.02f;material.restitution=.1f;material.frictionCombine=2;material.restitutionCombine=4;
  resources::PhysicsMaterialAsset loaded;
  AE_EXPECT_TRUE(resources::PhysicsMaterialAsset::deserialize(material.serialize(),loaded),"recurso relido");
  AE_EXPECT_TRUE(loaded.serialize()==material.serialize()&&loaded.name==material.name,"mesmo conteúdo, nome com aspas");
  material.restitution=1.5f;AE_EXPECT_TRUE(!material.valid(),"restituição acima de 1 recusada");
  material.restitution=.1f;material.frictionCombine=5;AE_EXPECT_TRUE(!material.valid(),"modo de combinação inexistente recusado");
  AE_EXPECT_TRUE(!resources::PhysicsMaterialAsset::deserialize("ASTRA_PHYSICS_MATERIAL 2 x",loaded),"versão desconhecida recusada");
}

AE_TEST(physics_material_combine_follows_unity_precedence) {
  AE_EXPECT_TRUE(std::abs((AetherCombinePhysicsMaterial(0,0,.25f,1,true))-(.5f))<1e-6f,"padrão do Jolt: atrito pela média geométrica");
  AE_EXPECT_TRUE(std::abs((AetherCombinePhysicsMaterial(0,0,.2f,.7f,false))-(.7f))<1e-6f,"padrão do Jolt: restituição pelo maior");
  AE_EXPECT_TRUE(std::abs((AetherCombinePhysicsMaterial(1,0,.2f,.6f,true))-(.4f))<1e-6f,"média contra padrão usa média");
  AE_EXPECT_TRUE(std::abs((AetherCombinePhysicsMaterial(1,2,.2f,.6f,true))-(.2f))<1e-6f,"mínimo vence média");
  AE_EXPECT_TRUE(std::abs((AetherCombinePhysicsMaterial(3,2,.5f,.6f,true))-(.3f))<1e-6f,"multiplicar vence mínimo");
  AE_EXPECT_TRUE(std::abs((AetherCombinePhysicsMaterial(3,4,.5f,.6f,true))-(.6f))<1e-6f,"máximo vence todos");
}

AE_TEST(physics_body_v4_reads_without_material_and_v5_keeps_the_reference) {
  scene::PhysicsBody body;body.motion=scene::BodyMotion::Dynamic;body.friction=.3f;
  std::stringstream v5;body.material=resources::assetGuidFromSeed("borracha");body.restitutionCombine=4;body.write(v5);
  scene::PhysicsBody back;AE_EXPECT_TRUE(back.read(v5,5)&&back.material==body.material&&back.restitutionCombine==4,"v5 preserva material e combinação");
  // Um arquivo v4 termina em continuousCollision.
  std::string text=v5.str();text=text.substr(0,text.rfind(' '));text=text.substr(0,text.rfind(' '));text=text.substr(0,text.rfind(' '));
  std::istringstream v4(text);scene::PhysicsBody old;old.material=body.material;old.frictionCombine=3;
  AE_EXPECT_TRUE(old.read(v4,4)&&!old.material.valid()&&old.frictionCombine==0&&old.friction==.3f,"v4 lê sem material e com padrão do motor");
}

namespace {
// Altura máxima do quique de uma esfera com restituição .9 sobre um chão sem quique.
float bounce(ae::u32 restitutionCombine) {
  AetherPhysicsWorld *world=AetherPhysics_CreateWorld({0,-9.81f,0},16);
  AetherBodyDescV2 floor{};floor.structSize=sizeof(floor);floor.apiVersion=AetherBodyApiVersionV2;
  floor.shape.kind=AetherShapeKind::Box;floor.shape.boxHalfExtent={5,.5f,5};floor.position={0,-.5f,0};floor.rotation={0,0,0,1};
  floor.motionType=AetherMotionType::Static;floor.friction=.5f;floor.restitution=0;floor.eventLayerMask=3;
  AetherBodyDescV2 ball=floor;ball.shape.kind=AetherShapeKind::Sphere;ball.shape.sphereRadius=.5f;ball.position={0,2,0};
  ball.motionType=AetherMotionType::Dynamic;ball.restitution=.9f;
  AetherPhysics_CreateBodyV2(world,&floor);const auto handle=AetherPhysics_CreateBodyV2(world,&ball);
  if(restitutionCombine) AetherPhysics_SetBodyMaterialCombineV1(world,handle,0,restitutionCombine);
  float best=0;bool landed=false;
  for(int i=0;i<180;++i) {
    AetherPhysics_Step(world,1.f/60,1);
    AetherVec3 p{};AetherQuat q{};AetherPhysics_GetTransform(world,handle,&p,&q);
    if(p.y<.6f) landed=true;
    if(landed) best=std::max(best,p.y);
  }
  AetherPhysics_DestroyWorld(world);
  return best;
}
}

AE_TEST(physics_material_combine_changes_the_real_jolt_contact) {
  const float engine=bounce(0),minimum=bounce(2);
  AE_EXPECT_TRUE(engine>1.2f,"padrão do motor: o maior valor faz quicar");
  AE_EXPECT_TRUE(minimum<.6f,"mínimo com o chão sem quique: a esfera para no chão");
}
