#include "harness.h"
#include "editor/editor_document.h"
#include "editor/editor_physics_body.h"
#include "editor/editor_play_scene.h"
#include "runtime/scene_environment.h"
#include "scene/camera.h"
#include "scene/camera_look.h"
#include "scene/character.h"
#include "scene/environment.h"
#include "scene/script_behavior.h"

#include <cmath>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

using namespace ae;
using namespace ae::editor;

// O caminho completo do Play com scripts — `EditorPlayScene` -> `ScriptBridge`
// -> ABI -> serviço gerenciado — não tinha teste de host: os testes de contato
// exercitavam `ScenePhysics` direto. A consequência apareceu só no aparelho,
// onde o contato sólido nunca chegava ao comportamento. Este duplo ocupa o lugar
// do runtime C# e registra o que a ABI recebe.
namespace {
struct FakeRuntime {
  static u32 starts, updates, fixedUpdates, stops;
  static u32 triggers, contacts;
  static u64 lastContactFirst, lastContactSecond;
  static u32 lastContactPhase;
  static bool lastContactHadNormal;
  static std::string attachments;
  static scene::ScriptSceneAccess sceneAccess;
  static std::function<void()> onUpdate;

  static void reset() {
    starts = updates = fixedUpdates = stops = triggers = contacts = 0;
    lastContactFirst = lastContactSecond = 0;
    lastContactPhase = 99;
    lastContactHadNormal = false;
    attachments.clear();
    sceneAccess = {};
    onUpdate = {};
  }
  static int start(const u8 *, int, const u8 *json, int length, const scene::ScriptSceneAccess *access) {
    ++starts;
    attachments.assign(reinterpret_cast<const char *>(json), static_cast<usize>(length));
    if(access) sceneAccess=*access;
    // A ABI precisa chegar completa: um ponteiro faltando aqui é um campo que o
    // lado gerenciado usaria sem existir.
    return access && access->available() ? 0 : 1;
  }
  static int update(float) { ++updates; if(onUpdate) onUpdate(); return 0; }
  static int fixedUpdate(float) { ++fixedUpdates; return 0; }
  static void stop() { ++stops; }
  static int copyDiagnostics(u8 *, int) { return 0; }
  static int trigger(u64, u64, u32) { ++triggers; return 0; }
  static int contact(u64 first, u64 second, u32 phase, const float *normal) {
    ++contacts;
    lastContactFirst = first;
    lastContactSecond = second;
    lastContactPhase = phase;
    lastContactHadNormal = normal != nullptr;
    return 0;
  }
  static scene::ScriptRuntimeApi api() {
    scene::ScriptRuntimeApi value{};
    value.start = &start;
    value.update = &update;
    value.fixedUpdate = &fixedUpdate;
    value.stop = &stop;
    value.copyDiagnostics = &copyDiagnostics;
    value.trigger = &trigger;
    value.contact = &contact;
    return value;
  }
};
u32 FakeRuntime::starts = 0, FakeRuntime::updates = 0, FakeRuntime::fixedUpdates = 0, FakeRuntime::stops = 0;
u32 FakeRuntime::triggers = 0, FakeRuntime::contacts = 0;
u64 FakeRuntime::lastContactFirst = 0, FakeRuntime::lastContactSecond = 0;
u32 FakeRuntime::lastContactPhase = 99;
bool FakeRuntime::lastContactHadNormal = false;
std::string FakeRuntime::attachments;
scene::ScriptSceneAccess FakeRuntime::sceneAccess{};
std::function<void()> FakeRuntime::onUpdate{};

EditorEntityId physical(EditorDocument &doc, const char *name, float y, scene::BodyMotion motion, float halfY) {
  const auto id = doc.createEntity(doc.root(), EditorEntityKind::Folder, name);
  auto values = *doc.find(id);
  values.transform.position[1] = y;
  auto *body = editPhysicsBody(values);
  body->motion = motion;
  auto *collider = editCollider(values);
  collider->halfX = collider->halfZ = motion == scene::BodyMotion::Static ? 8.f : .5f;
  collider->halfY = halfY;
  return doc.applyEntityValues(id, values) ? id : 0;
}

void attachScript(EditorDocument &doc, EditorEntityId id, const char *type) {
  auto values = *doc.find(id);
  auto *script = static_cast<scene::ScriptBehavior *>(values.components.add(scene::ScriptBehavior::descriptor));
  script->scriptType = type;
  script->source = "Teste.cs";
  doc.applyEntityValues(id, values);
}
} // namespace

AE_TEST(play_scene_delivers_solid_contacts_to_the_script_runtime) {
  EditorDocument doc;
  const auto floorId = physical(doc, "Piso", -1, scene::BodyMotion::Static, .5f);
  const auto box = physical(doc, "Caixa", 3, scene::BodyMotion::Dynamic, .5f);
  AE_EXPECT_TRUE(floorId && box, "cena montada");
  attachScript(doc, box, "project.Teste");

  FakeRuntime::reset();
  EditorMapScene resources;
  EditorPlayScene play;
  play.setScriptRuntime(FakeRuntime::api(), "/projeto");
  AE_EXPECT_TRUE(play.start(doc, resources), "Play inicia com script anexado");
  AE_EXPECT_EQ(FakeRuntime::starts, 1u, "o runtime recebeu start");
  AE_EXPECT_TRUE(FakeRuntime::attachments.find("project.Teste") != std::string::npos,
                 "a descrição de anexos chega ao runtime");
  scene::ScriptRenderingState graphics;
  const auto playWorld=play.world().worldId();
  AE_EXPECT_TRUE(FakeRuntime::sceneAccess.getRenderingState(
      FakeRuntime::sceneAccess.context,playWorld,&graphics)==1,"ABI v6 consulta política da sessão");
  AE_EXPECT_TRUE(graphics.world==playWorld && !graphics.pending,"estado gráfico pertence ao Play atual");
  AE_EXPECT_TRUE(FakeRuntime::sceneAccess.getRenderingState(
      FakeRuntime::sceneAccess.context,playWorld+1,&graphics)==0,"sessão gráfica velha é recusada");

  for (int step = 0; step < 180; ++step) AE_EXPECT_TRUE(play.advance(1. / 60), "quadro de Play");

  AE_EXPECT_TRUE(FakeRuntime::updates > 0 && FakeRuntime::fixedUpdates > 0, "callbacks de quadro e de passo fixo");
  // A caixa cai sobre o piso: o contato sólido precisa atravessar
  // EditorPlayScene -> ScriptBridge -> ABI.
  AE_EXPECT_TRUE(FakeRuntime::contacts > 0, "contato sólido entregue ao runtime");
  AE_EXPECT_TRUE((FakeRuntime::lastContactFirst == box && FakeRuntime::lastContactSecond == floorId) ||
                 (FakeRuntime::lastContactFirst == floorId && FakeRuntime::lastContactSecond == box),
                 "o par entregue é piso/caixa");
  AE_EXPECT_TRUE(play.document().find(box)->transform.position[1] < 3.f, "a caixa caiu");

  play.stop();
  AE_EXPECT_EQ(FakeRuntime::stops, 1u, "Stop encerra o runtime");
}

AE_TEST(play_script_character_commands_cover_all_substeps_and_preserve_authorship) {
  float distances[2]{};
  for(int run=0;run<2;++run) {
    EditorDocument doc;
    AE_EXPECT_TRUE(physical(doc,"Floor",-.5f,scene::BodyMotion::Static,.5f),"floor");
    const auto actor=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Actor");
    auto actorValue=*doc.find(actor);
    actorValue.transform.position[1]=3;
    auto *character=static_cast<scene::Character*>(actorValue.components.add(scene::Character::descriptor));
    character->speed=2;
    AE_EXPECT_TRUE(doc.applyEntityValues(actor,actorValue),"character authored");
    const auto camera=doc.createEntity(actor,EditorEntityKind::Folder,"Camera");
    auto cameraValue=*doc.find(camera);
    cameraValue.components.add(scene::Camera::descriptor);
    auto *look=static_cast<scene::CameraLook*>(cameraValue.components.add(scene::CameraLook::descriptor));
    look->yawSensitivity=180;look->pitchSensitivity=90;look->pitchLimit=60;
    cameraValue.transform.rotationDegrees[2]=15;
    AE_EXPECT_TRUE(doc.applyEntityValues(camera,cameraValue),"camera authored");
    attachScript(doc,actor,"project.Teste");
    FakeRuntime::reset();
    EditorMapScene resources;
    EditorPlayScene play;
    play.setScriptRuntime(FakeRuntime::api(),"/projeto");
    AE_EXPECT_TRUE(play.start(doc,resources),"Play starts");
    auto &api=FakeRuntime::sceneAccess;
    AE_EXPECT_EQ(api.characterJump(api.context,actor),0,"airborne jump rejected");
    AE_EXPECT_EQ(api.lastStatus(api.context),static_cast<int>(runtime::WorldStatus::Rejected),"normal refusal status");
    for(int step=0;step<120;++step) AE_EXPECT_TRUE(play.advance(1./60),"settle on real physics floor");
    const float start=play.document().find(actor)->transform.position[0];
    bool first=true;
    FakeRuntime::onUpdate=[&] {
      const float move[]{1,0,0};
      AE_EXPECT_EQ(api.characterMove(api.context,actor,move),1,"move accepted through ABI");
      if(first) {
        const float delta[]{.5f,2};
        AE_EXPECT_EQ(api.cameraLook(api.context,camera,delta),1,"look accepted through ABI");
        first=false;
      }
    };
    const int frames=run==0?30:60;
    for(int frame=0;frame<frames;++frame) AE_EXPECT_TRUE(play.advance(1./frames),"movement frame");
    AE_EXPECT_TRUE(!test::currentTestFailed(),"script callback assertions");
    distances[run]=play.document().find(actor)->transform.position[0]-start;
    AE_EXPECT_TRUE(std::abs(distances[run]-2.f)<.03f,"one second at configured two metres per second");
    const auto &rotation=play.document().find(camera)->transform.rotationDegrees;
    AE_EXPECT_TRUE(std::abs(rotation[0]-60)<.001f&&std::abs(rotation[1]-90)<.001f&&
                   std::abs(rotation[2]-15)<.001f,"look clamps pitch and preserves roll");
    FakeRuntime::onUpdate={};
    const float beforeStop=play.document().find(actor)->transform.position[0];
    AE_EXPECT_TRUE(play.advance(1./30),"next frame without script command");
    AE_EXPECT_TRUE(std::abs(play.document().find(actor)->transform.position[0]-beforeStop)<.001f,
                   "script intention does not leak into next frame");
    AE_EXPECT_EQ(api.characterJump(api.context,actor),1,"grounded jump accepted");
    AE_EXPECT_EQ(api.characterJump(api.context,actor),0,"second jump cannot repeat in air");
    AE_EXPECT_TRUE(doc.find(actor)->transform.position[0]==0&&doc.find(actor)->transform.position[1]==3&&
                   doc.find(camera)->transform.rotationDegrees[0]==0&&
                   doc.find(camera)->transform.rotationDegrees[2]==15,"authoring scene unchanged");
    play.stop();
  }
  AE_EXPECT_TRUE(std::abs(distances[0]-distances[1])<.001f,"30 and 60 Hz cover the same physical steps");
}

AE_TEST(play_scene_refuses_a_script_runtime_that_does_not_fill_the_abi) {
  EditorDocument doc;
  const auto box = physical(doc, "Caixa", 1, scene::BodyMotion::Dynamic, .5f);
  AE_EXPECT_TRUE(box, "objeto criado");
  attachScript(doc, box, "project.Teste");

  FakeRuntime::reset();
  auto incomplete = FakeRuntime::api();
  // Um hospedeiro que não resolveu `Contact` não pode ser aceito em silêncio:
  // o Play começaria e os contatos sumiriam sem nenhum aviso.
  incomplete.contact = nullptr;
  EditorMapScene resources;
  EditorPlayScene play;
  play.setScriptRuntime(incomplete, "/projeto");
  AE_EXPECT_TRUE(!play.start(doc, resources), "ABI incompleta recusa o Play");
  AE_EXPECT_EQ(FakeRuntime::starts, 0u, "o runtime nem chega a ser iniciado");
}

AE_TEST(play_script_property_bridge_updates_physical_atmosphere_consumed_by_the_frame) {
  EditorDocument doc;
  const auto id=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Atmosfera dinâmica");
  auto values=*doc.find(id);
  auto *environment=static_cast<scene::Environment*>(
      values.components.add(scene::Environment::descriptor));
  auto *script=static_cast<scene::ScriptBehavior*>(
      values.components.add(scene::ScriptBehavior::descriptor));
  AE_EXPECT_TRUE(environment&&script,"ambiente e comportamento adicionados");
  environment->values.sky=renderer::SkyModel::Atmosphere;
  const auto environmentInstance=environment->instanceId();
  script->scriptType="project.AtmosphereDriver";script->source="AtmosphereDriver.cs";
  AE_EXPECT_TRUE(doc.applyEntityValues(id,values),"cena autorada");

  FakeRuntime::reset();
  EditorMapScene resources;EditorPlayScene play;
  play.setScriptRuntime(FakeRuntime::api(),"/projeto");
  AE_EXPECT_TRUE(play.start(doc,resources),"Play inicia com bridge real");
  const auto set=[&](std::string_view property,u32 kind,u64 bits) {
    return FakeRuntime::sceneAccess.setProperty(
        FakeRuntime::sceneAccess.context,id,environmentInstance,
        reinterpret_cast<const u8*>(property.data()),static_cast<int>(property.size()),kind,bits);
  };
  AE_EXPECT_TRUE(set("sky",2,static_cast<u32>(renderer::SkyModel::PhysicalAtmosphere))!=0,
                 "enum físico atravessa a ABI");
  const float aerosolDensity=2.75f;u32 aerosolBits=0;
  std::memcpy(&aerosolBits,&aerosolDensity,sizeof(aerosolBits));
  AE_EXPECT_TRUE(set("aerosol_density",0,aerosolBits)!=0,
                 "float físico atravessa a ABI depois de selecionar o modo");
  AE_EXPECT_TRUE(set("physical_atmosphere_high_quality",1,1)!=0,
                 "qualidade física atravessa a ABI");

  renderer::SceneEnvironment resolved;
  AE_EXPECT_TRUE(runtime::collectSceneEnvironment(play.document(),resolved),
                 "coleta do frame lê o mundo em Play");
  AE_EXPECT_TRUE(resolved.sky==renderer::SkyModel::PhysicalAtmosphere&&
                 resolved.aerosolDensity==aerosolDensity&&resolved.physicalAtmosphereHighQuality,
                 "descriptor, bridge e consumidor publicam os novos valores");

  play.stop();
  const auto *authored=static_cast<const scene::Environment*>(
      doc.find(id)->components.find(scene::Environment::descriptor));
  AE_EXPECT_TRUE(authored&&authored->values.sky==renderer::SkyModel::Atmosphere&&
                 authored->values.aerosolDensity==1.0f,
                 "mudança de gameplay não altera o documento autoral");
}

AE_TEST(play_scene_material_written_by_code_reaches_the_draw_state) {
  // O que o aparelho mostrou: um comportamento escreve `base_color` e a cor não
  // muda na tela. Este teste fixa até onde a cadeia PRECISA funcionar — do
  // mundo de execução até o estado de desenho. O que estiver depois disto é
  // consumo do renderer, e não este contrato.
  renderer::MapDrawRecord draw{};
  draw.model[0] = draw.model[5] = draw.model[10] = draw.model[15] = 1;
  draw.boundsRadius = 1;
  draw.indexCount = 36;
  renderer::MapMaterialRecord material{};
  material.baseColorFactor[0] = material.baseColorFactor[1] = material.baseColorFactor[2] = 1;
  material.baseColorFactor[3] = 1;

  EditorDocument doc;
  EditorMapScene resources;
  AE_EXPECT_TRUE(resources.import(doc, {&draw, 1}, {&material, 1}), "malha importada");
  const auto id = doc.childrenOf(doc.root())[0];
  AE_EXPECT_TRUE(!meshMaterial(*doc.find(id)).enabled, "sem override ao importar");
  attachScript(doc,id,"project.MaterialDriver");

  FakeRuntime::reset();
  EditorPlayScene play;
  play.setScriptRuntime(FakeRuntime::api(),"/projeto");
  AE_EXPECT_TRUE(play.start(doc, resources), "Play inicia");
  auto &world = play.world();
  const auto handle = world.handle(id);
  const auto mesh = world.findComponent(handle, "astra.render.mesh");
  AE_EXPECT_TRUE(mesh.valid(), "componente de malha endereçável");

  // Exatamente o que `Component.SetFloat` faz do lado C#.
  AE_EXPECT_EQ(static_cast<u32>(world.setProperty(mesh, "base_color.r", .10f)), 0u, "R");
  AE_EXPECT_EQ(static_cast<u32>(world.setProperty(mesh, "base_color.g", .90f)), 0u, "G");
  AE_EXPECT_EQ(static_cast<u32>(world.setProperty(mesh, "base_color.b", .20f)), 0u, "B");
  const float roughness=.35f;u32 roughnessBits=0;std::memcpy(&roughnessBits,&roughness,sizeof(roughnessBits));
  constexpr std::string_view roughnessId="material.roughness";
  AE_EXPECT_TRUE(FakeRuntime::sceneAccess.setComponentSlotProperty(
      FakeRuntime::sceneAccess.context,id,mesh.instance,reinterpret_cast<const u8*>(roughnessId.data()),
      static_cast<int>(roughnessId.size()),0,0,roughnessBits)!=0,"slot PBR atravessa a ABI v7");
  u32 kind=99;u64 readBits=0;
  AE_EXPECT_TRUE(FakeRuntime::sceneAccess.getComponentSlotProperty(
      FakeRuntime::sceneAccess.context,id,mesh.instance,reinterpret_cast<const u8*>(roughnessId.data()),
      static_cast<int>(roughnessId.size()),0,&kind,&readBits)!=0&&kind==0&&readBits==roughnessBits,
      "slot PBR volta com tipo e bits preservados");
  const float normalScale=2.5f;u32 normalScaleBits=0;
  std::memcpy(&normalScaleBits,&normalScale,sizeof(normalScaleBits));
  constexpr std::string_view normalScaleId="sampling.normal.scale_u";
  AE_EXPECT_TRUE(FakeRuntime::sceneAccess.setComponentSlotProperty(
      FakeRuntime::sceneAccess.context,id,mesh.instance,reinterpret_cast<const u8*>(normalScaleId.data()),
      static_cast<int>(normalScaleId.size()),0,0,normalScaleBits)!=0,
      "transformação UV da normal atravessa a ABI por binding");

  std::vector<renderer::MapDrawState> draws;
  AE_EXPECT_TRUE(play.extract(resources, draws), "extração do mundo em execução");
  AE_EXPECT_EQ(draws.size(), 1u, "um desenho");
  // Escrever uma propriedade de material LIGA o override: sem isso o valor
  // ficaria guardado e o desenho continuaria usando o material do pacote.
  AE_EXPECT_TRUE(draws[0].material.enabled, "override ativado pela escrita");
  AE_EXPECT_EQ(draws[0].material.baseColor[1], .90f, "verde escrito pelo código");
  AE_EXPECT_EQ(draws[0].material.roughness,.35f,"rugosidade escrita pelo ABI por slot");
  AE_EXPECT_TRUE((draws[0].material.uvTransformMask&(1u<<1))!=0 &&
                 (draws[0].material.uvTransformMask&1u)==0 &&
                 std::fabs(draws[0].material.uvTransforms[1][0]-normalScale)<1e-4f,
                 "transformação UV alcança apenas o binding normal no estado de desenho");
  const auto applied = renderer::applyMaterialOverride(material, draws[0].material);
  AE_EXPECT_EQ(applied.baseColorFactor[1], .90f, "o override chega ao registro de material do desenho");

  // E o documento autoral continua com o material do pacote.
  play.stop();
  AE_EXPECT_TRUE(!meshMaterial(*doc.find(id)).enabled, "autoria preservada depois do Stop");
}
