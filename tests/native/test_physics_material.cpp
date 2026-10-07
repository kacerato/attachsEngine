#include "harness.h"
#include "physics/jolt_bridge.h"
#include "resources/physics_material.h"
#include "scene/physics_body.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

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

namespace {
// Gravação do Corpo nas versões anteriores, a partir da atual (v8): movimento,
// 15 números (os 6 de massa da v8 ficam de fora), sensor, sono, 6 travas, CCD;
// v5 acrescenta combinações e material; v6 a superfície; v7 monitoring.
std::string bodyAs(const scene::PhysicsBody &body,u32 version) {
  std::stringstream out;body.write(out);std::vector<std::string> t;{std::string w;while(out>>w) t.push_back(w);}
  const usize tail=version<=4?9:version==5?12:version==6?13:15;
  std::string text;for(usize i=0;i<16;++i) text+=t[i]+' ';
  for(usize i=22;i<22+tail;++i) text+=t[i]+' ';
  return text;
}
}

AE_TEST(physics_body_v4_reads_without_material_and_v5_keeps_the_reference) {
  scene::PhysicsBody body;body.motion=scene::BodyMotion::Dynamic;body.friction=.3f;
  body.material=resources::assetGuidFromSeed("borracha");body.restitutionCombine=4;
  std::istringstream v5(bodyAs(body,5));scene::PhysicsBody back;
  AE_EXPECT_TRUE(back.read(v5,5)&&back.material==body.material&&back.restitutionCombine==4,"v5 preserva material e combinação");
  std::istringstream v4(bodyAs(body,4));scene::PhysicsBody old;old.material=body.material;old.frictionCombine=3;
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

namespace {
// Chão composto de duas partes; só a esquerda tem material próprio que quica.
// Devolve as alturas máximas depois do pouso das esferas da esquerda e da direita.
std::pair<float,float> partBounce() {
  AetherPhysicsWorld *world=AetherPhysics_CreateWorld({0,-9.81f,0},16);
  AetherBodyDescV2 floor{};floor.structSize=sizeof(floor);floor.apiVersion=AetherBodyApiVersionV2;floor.position={0,-.5f,0};floor.rotation={0,0,0,1};
  floor.motionType=AetherMotionType::Static;floor.friction=.5f;floor.restitution=0;floor.eventLayerMask=3;
  AetherCompoundPartV3 parts[2]{};
  for(int i=0;i<2;++i){parts[i].base.shape.kind=AetherShapeKind::Box;parts[i].base.shape.boxHalfExtent={2,.5f,2};parts[i].base.position={i?2.5f:-2.5f,0,0};
    parts[i].base.rotation={0,0,0,1};parts[i].cooking=AetherMeshCookingDefaultsV1;}
  AetherBodyDynamicsV1 dynamics{sizeof(AetherBodyDynamicsV1),1,0,0,1,{0,0,0},1};
  const auto ground=AetherPhysics_CreateCompoundBodyV3(world,&floor,parts,2,&dynamics);
  const AetherPartMaterialV1 materials[2]{{.5f,.95f,0,4,1},{.5f,0,0,0,0}};
  const bool accepted=AetherPhysics_SetBodyPartMaterialsV1(world,ground,materials,2)!=0;
  AetherBodyDescV2 ball=floor;ball.shape.kind=AetherShapeKind::Sphere;ball.shape.sphereRadius=.4f;ball.motionType=AetherMotionType::Dynamic;ball.restitution=0;
  ball.position={-2.5f,2,0};const auto left=AetherPhysics_CreateBodyV2(world,&ball);
  ball.position={2.5f,2,0};const auto right=AetherPhysics_CreateBodyV2(world,&ball);
  float best[2]{};bool landed[2]{};
  for(int i=0;i<180;++i) {
    AetherPhysics_Step(world,1.f/60,1);
    const AetherBodyHandle handles[2]{left,right};
    for(int b=0;b<2;++b){AetherVec3 p{};AetherQuat q{};AetherPhysics_GetTransform(world,handles[b],&p,&q);if(p.y<.5f)landed[b]=true;if(landed[b])best[b]=std::max(best[b],p.y);}
  }
  AetherPhysics_DestroyWorld(world);
  return accepted?std::pair{best[0],best[1]}:std::pair{-1.f,-1.f};
}
}

AE_TEST(physics_material_per_collider_part_changes_only_that_part_of_the_compound) {
  const auto [left,right]=partBounce();
  AE_EXPECT_TRUE(left>1.f,"a parte com material próprio (Máximo, .95) faz quicar");
  AE_EXPECT_TRUE(right>=0&&right<.5f,"a outra parte do mesmo corpo continua sem quique");
}

AE_TEST(collider_v8_body_v5_and_material_v1_read_without_surface_data) {
  scene::Collider collider;collider.ownMaterial=true;collider.friction=.1f;collider.restitution=.7f;collider.surface=8;
  collider.material=resources::assetGuidFromSeed("gelo");collider.restitutionCombine=4;
  std::stringstream v9;collider.write(v9);scene::Collider back;
  AE_EXPECT_TRUE(back.read(v9,9)&&back.ownMaterial&&back.surface==8&&back.material==collider.material&&back.friction==.1f,"colisor v9 relido");
  // v8: forma, 13 números, vínculo e cozimento; sem atrito/restituição (números 14 e 15) nem o bloco final do material.
  scene::Collider plain;std::stringstream full;plain.write(full);
  std::vector<std::string> tokens;{std::istringstream in(full.str());std::string word;while(in>>word) tokens.push_back(word);}
  std::string text;for(usize i=0;i+5<tokens.size();++i) if(i!=14&&i!=15) text+=tokens[i]+' ';
  std::istringstream v8(text);scene::Collider old;old.ownMaterial=true;
  AE_EXPECT_TRUE(old.read(v8,8)&&!old.ownMaterial&&old.surface==0&&old.friction==.5f,"colisor v8 lê sem material próprio");
  scene::PhysicsBody body;body.surface=4;std::istringstream v6(bodyAs(body,6));scene::PhysicsBody b6;
  AE_EXPECT_TRUE(b6.read(v6,6)&&b6.surface==4,"corpo v6 com superfície");
  std::istringstream v5(bodyAs(body,5));scene::PhysicsBody b5;b5.surface=3;
  AE_EXPECT_TRUE(b5.read(v5,5)&&b5.surface==0,"corpo v5 lê com superfície padrão");
  resources::PhysicsMaterialAsset material;
  AE_EXPECT_TRUE(resources::PhysicsMaterialAsset::deserialize("ASTRA_PHYSICS_MATERIAL 1 8dd76ac7b22f2c66033af819d76c0586 1 \"Antigo\" 0.5 0.9 0 4",material)&&
                 material.surface==0&&material.restitutionCombine==4,"recurso formato 1 lê sem superfície");
}
