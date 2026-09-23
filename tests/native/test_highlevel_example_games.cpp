#include "editor/editor_archive.h"
#include "runtime/scene_physics.h"
#include "scene/physics_body.h"
#include "scene/script_behavior.h"
#include "harness.h"

#include <fstream>
#include <sstream>

using namespace ae;
using namespace ae::editor;

namespace {
bool loadGame(const char *slug,EditorDocument &document) {
  const std::string path=std::string(AETHER_REPOSITORY_ROOT)+
    "/android/app/src/main/assets/astra/example-projects/"+slug+"/scenes/editor.aescene";
  std::ifstream input(path,std::ios::binary);std::ostringstream text;text<<input.rdbuf();
  return (input.good()||input.eof())&&deserializeEditorDocument(text.str(),0,document);
}
const EditorEntity *named(const EditorDocument &document,std::string_view name) {
  std::vector<EditorEntityId> ids;document.collectSubtree(document.root(),ids);
  for(const auto id:ids) {const auto *entity=document.find(id);if(entity&&std::string_view(entity->name)==name)return entity;}
  return nullptr;
}
void expectMotion(const EditorDocument &document,std::string_view name,scene::BodyMotion motion) {
  const auto objectMessage=std::string("object ")+std::string(name);
  const auto *entity=named(document,name);AE_EXPECT_TRUE(entity!=nullptr,objectMessage.c_str());
  const auto *body=entity?static_cast<const scene::PhysicsBody*>(entity->components.find(scene::PhysicsBody::descriptor)):nullptr;
  const auto motionMessage=std::string("motion ")+std::string(name);
  AE_EXPECT_TRUE(body&&body->motion==motion,motionMessage.c_str());
}
void expectScript(const EditorDocument &document,std::string_view name,std::string_view type) {
  const auto objectMessage=std::string("script object ")+std::string(name);
  const auto *entity=named(document,name);AE_EXPECT_TRUE(entity!=nullptr,objectMessage.c_str());
  const auto *script=entity?scene::scriptBehavior(entity->components.find(scene::ScriptBehavior::descriptor)):nullptr;
  const auto typeMessage=std::string("script type ")+std::string(type);
  AE_EXPECT_TRUE(script&&script->scriptType==type,typeMessage.c_str());
}
void expectPhysics(const EditorDocument &document,u32 minimumBodies) {
  runtime::GameWorld world;runtime::ScenePhysics physics;
  AE_EXPECT_TRUE(world.load(document),"load play world");
  AE_EXPECT_TRUE(physics.start(world),physics.error().c_str());
  AE_EXPECT_TRUE(physics.bodyCount()>=minimumBodies,"physical world populated");
  AE_EXPECT_TRUE(physics.advance(1.0/60.0,world),"physical world advances");
}
}

AE_TEST(highlevel_example_games_load_with_physical_holding_and_ai_contracts) {
  EditorDocument quarantine;AE_EXPECT_TRUE(loadGame("quarentena",quarantine),"parse quarantine");
  AE_EXPECT_TRUE(quarantine.entityCount()>=200,"quarantine authored slice");
  AE_EXPECT_TRUE(quarantine.inputActions().find("Interagir")!=nullptr,"quarantine interaction action");
  expectMotion(quarantine,"Fusível 1",scene::BodyMotion::Dynamic);
  expectMotion(quarantine,"MIRA · drone de manutenção",scene::BodyMotion::Kinematic);
  expectScript(quarantine,"MIRA · drone de manutenção","project.MaintenanceDrone");
  expectPhysics(quarantine,20);

  EditorDocument rescue;AE_EXPECT_TRUE(loadGame("resgate",rescue),"parse rescue");
  AE_EXPECT_TRUE(rescue.entityCount()>=230,"rescue authored slice");
  AE_EXPECT_TRUE(rescue.inputActions().find("Interagir")!=nullptr,"rescue interaction action");
  expectMotion(rescue,"Kit médico real 1",scene::BodyMotion::Dynamic);
  expectMotion(rescue,"Sobrevivente 1",scene::BodyMotion::Kinematic);
  expectScript(rescue,"Sobrevivente 1","project.SurvivorAgent");
  expectPhysics(rescue,25);

  EditorDocument perimeter;AE_EXPECT_TRUE(loadGame("perimetro",perimeter),"parse perimeter");
  AE_EXPECT_TRUE(perimeter.entityCount()>=360,"perimeter authored slice");
  AE_EXPECT_TRUE(perimeter.inputActions().find("Ação")!=nullptr,"perimeter contextual action");
  expectMotion(perimeter,"Caixote militar real 1",scene::BodyMotion::Dynamic);
  expectMotion(perimeter,"Sentinela 1",scene::BodyMotion::Kinematic);
  expectScript(perimeter,"Caixote militar real 1","project.ThrowableCrate");
  expectScript(perimeter,"Sentinela 1","project.SentinelAgent");
  expectPhysics(perimeter,45);

  EditorDocument market;AE_EXPECT_TRUE(loadGame("mercado-nexus",market),"parse market");
  AE_EXPECT_TRUE(market.entityCount()>=180,"market authored slice");
  AE_EXPECT_TRUE(market.inputActions().find("Entregar")!=nullptr,"market delivery action");
  expectMotion(market,"Carga Âmbar 1",scene::BodyMotion::Dynamic);
  expectMotion(market,"Mercador Âmbar",scene::BodyMotion::Kinematic);
  expectScript(market,"Mercador Âmbar","project.VendorAgent");
  expectScript(market,"NIX · drone de logística","project.CourierGuide");
  expectPhysics(market,18);

  EditorDocument abyss;AE_EXPECT_TRUE(loadGame("farol-abissal",abyss),"parse abyss");
  AE_EXPECT_TRUE(abyss.entityCount()>=140,"abyss authored slice");
  AE_EXPECT_TRUE(abyss.inputActions().find("Pulso")!=nullptr,"abyss pulse action");
  expectMotion(abyss,"Célula de pressão 1",scene::BodyMotion::Dynamic);
  expectMotion(abyss,"Predador abissal 1",scene::BodyMotion::Kinematic);
  expectScript(abyss,"Predador abissal 1","project.AbyssStalker");
  expectScript(abyss,"LUME · drone faroleiro","project.BeaconDrone");
  expectPhysics(abyss,16);

  EditorDocument titan;AE_EXPECT_TRUE(loadGame("expresso-tita",titan),"parse titan");
  AE_EXPECT_TRUE(titan.entityCount()>=180,"titan authored slice");
  AE_EXPECT_TRUE(titan.inputActions().find("Ação")!=nullptr,"titan contextual action");
  expectMotion(titan,"Cápsula de carvão 1",scene::BodyMotion::Dynamic);
  expectMotion(titan,"Saqueador 1",scene::BodyMotion::Kinematic);
  expectScript(titan,"Cápsula de carvão 1","project.ThrownCargo");
  expectScript(titan,"Saqueador 1","project.RaiderAgent");
  expectScript(titan,"CONDUTOR 7","project.ConductorAgent");
  expectPhysics(titan,27);
}
