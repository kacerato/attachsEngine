#include "harness.h"
#include "editor/editor_document.h"
#include "editor/editor_physics_body.h"
#include "editor/editor_play_scene.h"
#include "editor/editor_play_edit.h"
#include "editor/editor_archive.h"
#include "editor/editor_history.h"
#include "runtime/scene_environment.h"
#include "runtime/scene_lights.h"
#include "scene/camera.h"
#include "scene/camera_look.h"
#include "scene/camera_follow.h"
#include "scene/character.h"
#include "scene/environment.h"
#include "scene/script_behavior.h"
#include "scene/timer.h"

#include <cmath>
#include <cstring>
#include <functional>
#include <sstream>
#include <string>
#include <vector>
#include <limits>

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
  static u32 lateUpdates, lifecycleEvents, lastLifecycle;
  static u32 triggers, contacts;
  static u32 timers, timerExpirations;
  static u32 edits;
  static std::string lastEdit;
  static u64 lastTimerObject, lastTimerInstance;
  static u64 lastContactFirst, lastContactSecond;
  static u32 lastContactPhase;
  static bool lastContactHadNormal;
  static std::string attachments;
  static scene::ScriptSceneAccess sceneAccess;
  static std::function<void()> onUpdate, onStart, onLateUpdate,onFixedUpdate;

  static void reset() {
    starts = updates = fixedUpdates = stops = triggers = contacts = timers = timerExpirations = 0;
    lateUpdates = lifecycleEvents = 0; lastLifecycle = 99;
    edits = 0; lastEdit.clear();
    lastTimerObject = lastTimerInstance = 0;
    lastContactFirst = lastContactSecond = 0;
    lastContactPhase = 99;
    lastContactHadNormal = false;
    attachments.clear();
    sceneAccess = {};
    onUpdate = {}; onStart = {}; onLateUpdate = {};onFixedUpdate={};
  }
  static int start(const u8 *, int, const u8 *json, int length, const scene::ScriptSceneAccess *access) {
    ++starts;
    attachments.assign(reinterpret_cast<const char *>(json), static_cast<usize>(length));
    if(access) sceneAccess=*access;
    if(onStart) onStart();
    // A ABI precisa chegar completa: um ponteiro faltando aqui é um campo que o
    // lado gerenciado usaria sem existir.
    return access && access->available() ? 0 : 1;
  }
  static int update(float) { ++updates; if(onUpdate) onUpdate(); return 0; }
  static int fixedUpdate(float) { ++fixedUpdates;if(onFixedUpdate)onFixedUpdate();return 0; }
  static int lateUpdate(float) { ++lateUpdates; if(onLateUpdate) onLateUpdate(); return 0; }
  static int lifecycle(u32 kind,u32 value) { ++lifecycleEvents; lastLifecycle=kind*2+value; return 0; }
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
  static int edit(u64,u64,const u8 *json,int length) {
    ++edits;lastEdit.assign(reinterpret_cast<const char *>(json),static_cast<usize>(length));return 0;
  }
  static int timer(u64 object,u64 instance,u32 count) {
    ++timers;timerExpirations+=count;lastTimerObject=object;lastTimerInstance=instance;return 0;
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
    value.timer = &timer;
    value.lateUpdate = &lateUpdate;
    value.lifecycle = &lifecycle;
    value.edit = &edit;
    return value;
  }
};
u32 FakeRuntime::starts = 0, FakeRuntime::updates = 0, FakeRuntime::fixedUpdates = 0, FakeRuntime::stops = 0;
u32 FakeRuntime::lateUpdates = 0, FakeRuntime::lifecycleEvents = 0, FakeRuntime::lastLifecycle = 99;
u32 FakeRuntime::triggers = 0, FakeRuntime::contacts = 0;
u32 FakeRuntime::timers = 0, FakeRuntime::timerExpirations = 0;
u32 FakeRuntime::edits = 0;
std::string FakeRuntime::lastEdit;
u64 FakeRuntime::lastTimerObject = 0, FakeRuntime::lastTimerInstance = 0;
u64 FakeRuntime::lastContactFirst = 0, FakeRuntime::lastContactSecond = 0;
u32 FakeRuntime::lastContactPhase = 99;
bool FakeRuntime::lastContactHadNormal = false;
std::string FakeRuntime::attachments;
scene::ScriptSceneAccess FakeRuntime::sceneAccess{};
std::function<void()> FakeRuntime::onUpdate{}, FakeRuntime::onStart{}, FakeRuntime::onLateUpdate{},FakeRuntime::onFixedUpdate{};

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

AE_TEST(play_inspection_reads_live_fields_without_authoring_writes_and_rejects_partial_snapshots) {
  FakeRuntime::reset();EditorDocument doc;EditorMapScene resources;EditorPlayScene play;
  const auto id=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Inspecionado");
  attachScript(doc,id,"test.inspection");const auto revision=doc.revision();
  static std::string report;
  auto api=FakeRuntime::api();
  api.inspectFields=[](u64,u8 *out,int capacity)->int {
    if(out && capacity>=static_cast<int>(report.size())) std::memcpy(out,report.data(),report.size());
    return static_cast<int>(report.size());
  };
  play.setScriptRuntime(api,"test");AE_EXPECT_TRUE(play.start(doc,resources),"Play real com ponte de teste");
  report="ASTRA_FIELDS 1 1 1 3 \"live\" \"int32\" 0 \"71\" \"optional\" \"array:int32\" 1 \"Nulo\" \"bad\" \"float\" 2 \"Getter falhou\"";
  std::vector<runtime::ScriptFieldIssue> issues;
  AE_EXPECT_TRUE(play.inspectFields(id,issues),"snapshot aceito");
  auto script=scene::scriptBehavior(play.document().find(id)->components.at(0));
  AE_EXPECT_TRUE(script && script->properties.size()==1 && script->properties[0].value=="71","valor atual disponível ao Inspector");
  AE_EXPECT_TRUE(issues.size()==2 && issues[0].isNull && !issues[1].isNull,"ausência separada de erro");
  report="ASTRA_FIELDS 1 1 1 2 \"live\" \"int32\" 0 \"99\"";
  AE_EXPECT_TRUE(!play.inspectFields(id,issues),"snapshot incompleto recusado");
  script=scene::scriptBehavior(play.document().find(id)->components.at(0));
  AE_EXPECT_TRUE(script && script->properties[0].value=="71","não publica metade da leitura");
  AE_EXPECT_TRUE(doc.revision()==revision && scene::scriptBehavior(doc.find(id)->components.at(0))->properties.empty(),"cena autoral intacta");
}

// Inspector em Play (editor/editor_play_edit.h): a diferença entre duas fotos do
// espelho chega ao mundo pela API pública dele — o campo do script pela ABI, a
// propriedade do corpo com o corpo recriado no solver, o objeto novo criado — e
// o que o mundo não aceita em execução volta com o nome do campo.
AE_TEST(play_edit_applies_mirror_differences_through_the_world_api) {
  EditorDocument doc;
  physical(doc, "Chão", 0, scene::BodyMotion::Static, .5f);
  const auto box = physical(doc, "Caixa", 3, scene::BodyMotion::Dynamic, .5f);
  attachScript(doc, box, "project.Mover");
  auto authored = *doc.find(box);
  for (usize i = 0; i < authored.components.size(); ++i)
    if (auto *script = const_cast<scene::ScriptBehavior *>(scene::scriptBehavior(authored.components.at(i))))
      script->setProperty("speed", "float", "2");
  AE_EXPECT_TRUE(doc.applyEntityValues(box, authored), "script com campo autorado");

  FakeRuntime::reset();EditorMapScene resources;EditorPlayScene play;
  play.setScriptRuntime(FakeRuntime::api(), "/projeto");
  AE_EXPECT_TRUE(play.start(doc, resources), "Play inicia");
  const resources::AssetRegistry assets;
  const PlayEditContext context{play, assets};
  const runtime::SceneGraph before = play.document();
  EditorDocument after;
  static_cast<runtime::SceneGraph &>(after) = play.document();
  auto values = *after.find(box);
  editPhysicsBody(values)->gravityFactor = 0;
  values.isStatic = true;
  for (usize i = 0; i < values.components.size(); ++i)
    if (auto *script = const_cast<scene::ScriptBehavior *>(scene::scriptBehavior(values.components.at(i))))
      script->setProperty("speed", "float", "5");
  AE_EXPECT_TRUE(after.applyEntityValues(box, values), "espelho editado");
  after.createEntity(after.root(), EditorEntityKind::Folder, "Marcador");

  const auto result = applyPlayEdits(before, after, context);
  AE_EXPECT_EQ(FakeRuntime::edits, 1u, "o campo do script vai à instância viva");
  AE_EXPECT_TRUE(FakeRuntime::lastEdit.find("\"speed\":5") != std::string::npos, FakeRuntime::lastEdit.c_str());
  const auto *running = play.document().find(box);
  const auto *script = [&]() -> const scene::ScriptBehavior * {
    for (usize i = 0; i < running->components.size(); ++i)
      if (const auto *value = scene::scriptBehavior(running->components.at(i))) return value;
    return nullptr;
  }();
  AE_EXPECT_TRUE(script && script->properties.size() == 1 && script->properties[0].value == "5",
                 "o mundo mostra o valor que a instância aceitou");
  AE_EXPECT_EQ(runtime::physicsBody(*running)->gravityFactor, 0.f, "propriedade do corpo aplicada no mundo");
  AE_EXPECT_TRUE(!running->isStatic && result.refusedField == "Estático",
                 "Estático não muda em execução e a recusa diz qual campo");
  AE_EXPECT_EQ(result.created.size(), 1u, "objeto criado no espelho");
  const auto *marker = result.created.empty() ? nullptr : play.document().find(result.created[0].second);
  AE_EXPECT_TRUE(marker && std::string(marker->name) == "Marcador", "o mundo ganhou o objeto novo");
  for (int frame = 0; frame < 30; ++frame) AE_EXPECT_TRUE(play.advance(1.0 / 60), "quadro");
  AE_EXPECT_TRUE(std::abs(play.document().find(box)->transform.position[1] - 3.f) < 1e-3f,
                 "o corpo recriado sem gravidade não cai");
  AE_EXPECT_EQ(std::string(doc.find(box)->name), std::string("Caixa"), "documento autoral intocado");
  AE_EXPECT_EQ(runtime::physicsBody(*doc.find(box))->gravityFactor, 1.f, "gravidade autoral intocada");
  play.stop();
}

AE_TEST(play_timer_multiple_instances_pause_edit_and_roundtrip) {
  EditorDocument doc;
  const auto id=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Relogio");
  auto values=*doc.find(id);
  auto *repeat=static_cast<scene::Timer *>(values.components.add(scene::Timer::descriptor));
  AE_EXPECT_TRUE(repeat!=nullptr,"primeiro timer anexado");
  repeat->intervalSeconds=.1f;
  const auto repeatId=repeat->instanceId();
  auto *once=static_cast<scene::Timer *>(values.components.add(scene::Timer::descriptor));
  AE_EXPECT_TRUE(once!=nullptr,"segundo timer independente");
  once->intervalSeconds=.15f;once->repeat=false;
  const auto onceId=once->instanceId();
  AE_EXPECT_TRUE(doc.applyEntityValues(id,values),"autoria válida");
  attachScript(doc,id,"project.TimerTest");
  const auto saved=serializeEditorDocument(doc,77);
  EditorDocument reopened;
  AE_EXPECT_TRUE(deserializeEditorDocument(saved,77,reopened),"ambas instâncias sobrevivem ao arquivo");
  const auto *loaded=reopened.find(id);
  AE_EXPECT_TRUE(loaded && loaded->components.findInstance(repeatId) && loaded->components.findInstance(onceId),"ids preservados");

  FakeRuntime::reset();EditorMapScene resources;EditorPlayScene play;
  play.setScriptRuntime(FakeRuntime::api(),"/projeto");
  AE_EXPECT_TRUE(play.start(reopened,resources),"Play inicia");
  AE_EXPECT_TRUE(play.advance(.25),"quadro longo");
  AE_EXPECT_EQ(FakeRuntime::timerExpirations,3u,"dois disparos repetidos e um único");
  const auto *repeatState=play.timers().state(id,repeatId);
  const auto *onceState=play.timers().state(id,onceId);
  AE_EXPECT_TRUE(repeatState && onceState && onceState->completed,"estado por instância");
  play.pause(true);
  AE_EXPECT_TRUE(play.advance(.25),"pausa não avança relógio");
  AE_EXPECT_EQ(FakeRuntime::timerExpirations,3u,"nenhum evento durante pausa");
  play.pause(false);
  auto component=play.world().findComponent(play.world().handle(id),"astra.time.timer",0);
  AE_EXPECT_TRUE(component.valid(),"API de Play encontra timer");
  AE_EXPECT_TRUE(play.world().setProperty(component,"interval_seconds",.2f)==runtime::WorldStatus::Ok,"intervalo editável no Play");
  AE_EXPECT_TRUE(play.advance(.11),"primeira metade do novo intervalo");
  AE_EXPECT_EQ(FakeRuntime::timerExpirations,3u,"edição reinicia a contagem");
  AE_EXPECT_TRUE(play.advance(.11),"segunda metade do intervalo");
  AE_EXPECT_EQ(FakeRuntime::timerExpirations,4u,"edição afeta o disparo");
  play.stop();
  AE_EXPECT_TRUE(!play.timers().state(id,repeatId),"Stop libera estado de execução");
  const auto *authored=static_cast<const scene::Timer *>(reopened.find(id)->components.findInstance(repeatId));
  AE_EXPECT_TRUE(authored && authored->intervalSeconds==.1f,"Play não altera autoria");
}

AE_TEST(play_timer_tracks_runtime_add_remove_and_object_activation) {
  EditorDocument doc;
  const auto id=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Agenda");
  runtime::GameWorld world;AE_EXPECT_TRUE(world.load(doc),"mundo inicializado");
  runtime::SceneTimers timers;
  u32 count=0;
  auto fire=[&](runtime::ObjectId,u64,u32 expirations){count+=expirations;return true;};
  AE_EXPECT_TRUE(timers.advance(world,.1,fire),"sem timers ainda");
  runtime::WorldStatus status;
  const auto component=world.addComponent(world.handle(id),"astra.time.timer",status);
  AE_EXPECT_TRUE(component.valid() && status==runtime::WorldStatus::Ok,"componente nasce em Play");
  AE_EXPECT_TRUE(world.setProperty(component,"interval_seconds",.1f)==runtime::WorldStatus::Ok,"intervalo editado");
  AE_EXPECT_TRUE(timers.advance(world,.11,fire),"novo timer descoberto");
  AE_EXPECT_EQ(count,1u,"novo componente dispara sem reiniciar Play");
  AE_EXPECT_TRUE(world.setActive(world.handle(id),false)==runtime::WorldStatus::Ok,"objeto desativado");
  AE_EXPECT_TRUE(timers.advance(world,.2,fire),"tempo avança com objeto inativo");
  AE_EXPECT_EQ(count,1u,"timer não acumula disparos inativos");
  AE_EXPECT_TRUE(world.setActive(world.handle(id),true)==runtime::WorldStatus::Ok,"objeto reativado");
  AE_EXPECT_TRUE(timers.advance(world,.11,fire),"timer retoma");
  AE_EXPECT_EQ(count,2u,"contagem preservada durante inatividade");
  AE_EXPECT_TRUE(world.removeComponent(component)==runtime::WorldStatus::Ok,"remoção enfileirada");
  world.flush();
  AE_EXPECT_TRUE(timers.advance(world,.1,fire),"scheduler limpa instância removida");
  AE_EXPECT_TRUE(!timers.state(id,component.instance),"estado runtime liberado");
  AE_EXPECT_EQ(count,2u,"sem callback depois da remoção");
}

AE_TEST(play_camera_follow_tracks_target_after_world_pose_changes) {
  EditorDocument doc;
  const auto target=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Alvo");
  const auto cameraId=doc.createEntity(doc.root(),EditorEntityKind::Camera,"Câmera seguidora");
  auto values=*doc.find(cameraId);values.transform.position[2]=-8;
  AE_EXPECT_TRUE(editCamera(values)!=nullptr,"câmera real");
  auto *follow=static_cast<scene::CameraFollow *>(values.components.add(scene::CameraFollow::descriptor));
  AE_EXPECT_TRUE(follow!=nullptr,"componente novo");
  follow->target=target;follow->offset[2]=-5;follow->dampingSeconds=0;
  AE_EXPECT_TRUE(doc.applyEntityValues(cameraId,values),"autoria válida");
  const auto archive=serializeEditorDocument(doc,81);EditorDocument reopened;
  AE_EXPECT_TRUE(deserializeEditorDocument(archive,81,reopened),"referência e propriedades persistem");
  EditorMapScene resources;EditorPlayScene play;
  AE_EXPECT_TRUE(play.start(reopened,resources),"Play inicia");
  runtime::WorldStatus creationStatus{};
  const auto cameraChild=play.world().createObject(play.world().handle(cameraId),"Filho",creationStatus);
  AE_EXPECT_TRUE(cameraChild.valid() && creationStatus==runtime::WorldStatus::Ok,"filho de câmera criado");
  const auto followHandle=play.world().findComponent(play.world().handle(cameraId),"astra.camera.follow");
  AE_EXPECT_TRUE(play.world().setProperty(followHandle,"target",scene::ObjectReference{cameraChild.id})==runtime::WorldStatus::Rejected,
                 "alvo descendente é recusado para evitar feedback");
  runtime::Transform position{};
  position.position[0]=7;position.position[1]=3;
  AE_EXPECT_TRUE(play.world().setWorldTransform(play.world().handle(target),position)==runtime::WorldStatus::Ok,"alvo move em Play");
  AE_EXPECT_TRUE(play.advance(1.0/60.0),"quadro executa follow depois de física");
  runtime::Transform actual{};
  AE_EXPECT_TRUE(play.world().worldTransform(play.world().handle(cameraId),actual)==runtime::WorldStatus::Ok,"pose da câmera");
  AE_EXPECT_EQ(actual.position[0],7.f,"X segue o alvo");
  AE_EXPECT_EQ(actual.position[1],5.f,"Y incorpora offset");
  AE_EXPECT_EQ(actual.position[2],-5.f,"Z incorpora offset");
  AE_EXPECT_EQ(reopened.find(cameraId)->transform.position[2],-8.f,"autoria intacta após Play");
  play.stop();
}

AE_TEST(primitives_runtime_creation_resolves_resources_and_real_collisions_through_abi) {
  std::vector<u8> vertices;std::vector<u32> indices;
  std::vector<renderer::MapDrawRecord> draws;std::vector<renderer::MapMaterialRecord> materials;
  AE_EXPECT_TRUE(renderer::appendPrimitiveLibrary(renderer::MapVertexStride,vertices,indices,draws,materials),"library");
  EditorDocument doc;EditorMapScene resources;
  AE_EXPECT_TRUE(resources.import(doc,draws,materials,false,vertices,indices),"resource geometry");
  const auto driver=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Driver");attachScript(doc,driver,"acceptance.primitives");
  FakeRuntime::reset();EditorPlayScene play;play.setScriptRuntime(FakeRuntime::api(),"/project");
  AE_EXPECT_TRUE(play.start(doc,resources),"Play");const auto &abi=FakeRuntime::sceneAccess;
  const auto count=play.document().entityCount();
  AE_EXPECT_EQ(abi.createPrimitive(abi.context,doc.root(),99),u64{0},"invalid enum rejected");
  AE_EXPECT_EQ(play.document().entityCount(),count,"failure leaves no partial object");
  for(u32 type=0;type<6;++type) {
    const auto id=static_cast<u32>(abi.createPrimitive(abi.context,doc.root(),type));AE_EXPECT_TRUE(id,"runtime object");
    const auto *object=play.document().find(id);AE_EXPECT_TRUE(object,"published");
    const auto *mesh=runtime::meshRenderer(*object);AE_EXPECT_TRUE(mesh && mesh->asset==resources.assetGuid(type),"real resource identity");
    AE_EXPECT_TRUE(play.commitEdits(),"new collider registered at safe point");
    const float origin[3]{0,type==5?0.f:3.f,type==5?3.f:0.f};
    const float ray[3]{0,type==5?0.f:-6.f,type==5?-6.f:0.f};
    scene::ScriptQueryFilter filter;scene::ScriptQueryHit hit;
    const auto hits=abi.rayCast(abi.context,origin,ray,&filter,&hit,1);
    // A ray exactly on the quad diagonal can report both triangles; the ABI
    // returns the total hit count even when the output has capacity one.
    AE_EXPECT_TRUE(hits>=1,"Jolt ray hits each new shape");
    AE_EXPECT_EQ(hit.object,static_cast<u64>(id),"hit resolves to the primitive");
    AE_EXPECT_EQ(abi.destroyObject(abi.context,id),1,"destroy requests removal");
    AE_EXPECT_TRUE(play.commitEdits(),"unregister collider and render object");
    AE_EXPECT_EQ(abi.rayCast(abi.context,origin,ray,&filter,&hit,1),0,"destroyed collider is absent");
  }
  play.stop();AE_EXPECT_EQ(doc.entityCount(),count,"authoring unaffected by runtime creation");
}

AE_TEST(play_scene_abi_v20_triple_assignment_rejects_partial_writes_and_reports_status) {
  FakeRuntime::reset();EditorDocument doc;EditorMapScene resources;EditorPlayScene play;
  const auto id=doc.createEntity(doc.root(),EditorEntityKind::Camera,"Camera");
  auto value=*doc.find(id);
  auto *follow=static_cast<scene::CameraFollow*>(value.components.add(scene::CameraFollow::descriptor));
  const auto instance=follow->instanceId();
  AE_EXPECT_TRUE(doc.applyEntityValues(id,value),"author real CameraFollow");
  attachScript(doc,id,"project.Teste");
  play.setScriptRuntime(FakeRuntime::api(),"/projeto");
  AE_EXPECT_TRUE(play.start(doc,resources),"bridge publishes scene access to runtime");
  const auto &abi=FakeRuntime::sceneAccess;
  AE_EXPECT_TRUE(abi.version==scene::ScriptSceneAccess{}.version && abi.size==sizeof(scene::ScriptSceneAccess) && abi.available(),"complete ABI v25 layout");
  auto incomplete=abi;incomplete.setTriple=nullptr;
  AE_EXPECT_TRUE(!incomplete.available(),"v20 requires appended triple callback");
  const auto *property=reinterpret_cast<const u8*>("offset");
  const float invalid[3]{9,std::numeric_limits<float>::quiet_NaN(),-2};
  AE_EXPECT_EQ(abi.setTriple(abi.context,id,instance,property,6,invalid),0,"bridge rejects invalid tuple");
  AE_EXPECT_EQ(abi.lastStatus(abi.context),static_cast<u32>(runtime::WorldStatus::Rejected),"bridge reports world rejection");
  const auto handle=play.world().findComponent(play.world().handle(id),"astra.camera.follow");
  const auto *unchanged=static_cast<const scene::CameraFollow*>(play.world().readComponent(handle));
  AE_EXPECT_TRUE(unchanged && unchanged->offset[0]==0 && unchanged->offset[1]==2 && unchanged->offset[2]==-5,"invalid middle channel leaves all runtime channels intact");
  const float valid[3]{9,3,-2};
  AE_EXPECT_EQ(abi.setTriple(abi.context,id,instance,property,6,valid),1,"bridge accepts complete tuple");
  AE_EXPECT_EQ(abi.lastStatus(abi.context),static_cast<u32>(runtime::WorldStatus::Ok),"success replaces prior failure status");
  const auto *changed=static_cast<const scene::CameraFollow*>(play.world().readComponent(handle));
  AE_EXPECT_TRUE(changed && changed->offset[0]==9 && changed->offset[1]==3 && changed->offset[2]==-2,"bridge publishes all three channels");
  AE_EXPECT_EQ(abi.setTriple(abi.context,id,instance,property,6,nullptr),0,"null tuple refused at ABI boundary");
  AE_EXPECT_EQ(abi.lastStatus(abi.context),static_cast<u32>(runtime::WorldStatus::InvalidArgument),"invalid ABI argument has precise status");
  AE_EXPECT_EQ(static_cast<const scene::CameraFollow*>(doc.find(id)->components.findInstance(instance))->offset[0],0.f,"runtime tuple preserves authoring");
  play.stop();
}

AE_TEST(play_active_self_persists_and_inactive_scripts_reach_the_runtime) {
  EditorDocument doc;
  const auto parent = doc.createEntity(doc.root(), EditorEntityKind::Folder, "Pai");
  const auto child = doc.createEntity(parent, EditorEntityKind::Folder, "Filho");
  attachScript(doc, child, "project.Activation");
  AE_EXPECT_TRUE(doc.setActive(parent, false), "pai desativado na autoria");
  EditorDocument reopened;
  AE_EXPECT_TRUE(deserializeEditorDocument(serializeEditorDocument(doc, 42), 42, reopened), "salva e reabre");
  AE_EXPECT_TRUE(reopened.find(child)->active && !reopened.activeInHierarchy(child), "local e herdado distintos no arquivo");
  FakeRuntime::reset(); EditorMapScene resources; EditorPlayScene play;
  play.setScriptRuntime(FakeRuntime::api(), "/projeto");
  AE_EXPECT_TRUE(play.start(reopened, resources), "Play com script inicialmente inativo");
  AE_EXPECT_TRUE(FakeRuntime::attachments.find("project.Activation") != std::string::npos,
                 "instância incluída para permitir a primeira ativação");
  const auto &abi = FakeRuntime::sceneAccess;
  AE_EXPECT_TRUE(abi.version == 24 && abi.available(), "contrato ABI completo");
  AE_EXPECT_EQ(abi.getActiveSelf(abi.context, child), 1, "estado local chega à ABI");
  AE_EXPECT_EQ(abi.getActive(abi.context, child), 0, "ancestral inativo chega à ABI");
  const auto revision = play.world().structuralRevision();
  AE_EXPECT_EQ(abi.setActive(abi.context, child, 1), 1, "escrita idempotente aceita");
  AE_EXPECT_EQ(play.world().structuralRevision(), revision, "não pede reconstrução sem mudança");
  AE_EXPECT_EQ(abi.setActive(abi.context, parent, 1), 1, "ativa pai");
  AE_EXPECT_EQ(abi.getActive(abi.context, child), 1, "filho agora ativo na hierarquia");
  AE_EXPECT_TRUE(!reopened.find(parent)->active, "Play não altera autoria");
  AE_EXPECT_EQ(abi.getActiveSelf(abi.context, (1ull << 32) + child), -1, "id largo não pode acertar outro objeto");
  AE_EXPECT_EQ(abi.destroyObject(abi.context, parent), 1, "remove subárvore");
  AE_EXPECT_EQ(abi.getActiveSelf(abi.context, child), -1, "filho vencido não vira falso silencioso");
  play.stop();
}

AE_TEST(play_paused_step_completes_late_update_before_camera_follow_and_flushes_commands) {
  FakeRuntime::reset();EditorDocument doc;EditorMapScene resources;EditorPlayScene play;
  const auto target=physical(doc,"Target",3,scene::BodyMotion::Dynamic,.5f);
  const auto disposable=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Disposable");
  const auto camera=doc.createEntity(doc.root(),EditorEntityKind::Camera,"Camera");
  attachScript(doc,target,"project.Teste");
  auto value=*doc.find(camera);
  auto *follow=static_cast<scene::CameraFollow*>(value.components.add(scene::CameraFollow::descriptor));
  follow->target=target;follow->dampingSeconds=0;follow->offset[0]=follow->offset[1]=follow->offset[2]=0;
  AE_EXPECT_TRUE(doc.applyEntityValues(camera,value),"camera follows runtime target");
  play.setScriptRuntime(FakeRuntime::api(),"/projeto");
  AE_EXPECT_TRUE(play.start(doc,resources),"Play starts");
  AE_EXPECT_TRUE(!play.step(),"step requires editor pause");
  play.pause(true);
  AE_EXPECT_TRUE(play.advance(.2),"paused advance is idle");
  AE_EXPECT_EQ(FakeRuntime::updates,0u,"pause does not run Update");
  AE_EXPECT_EQ(play.world().elapsedSeconds(),0.,"pause does not advance clock");
  bool lateSawPhysics=false,lateChangedPose=false,lateQueuedDestroy=false;
  FakeRuntime::onLateUpdate=[&] {
    lateSawPhysics=FakeRuntime::updates==1 && FakeRuntime::fixedUpdates>0;
    auto pose=play.document().find(target)->transform;pose.position[0]=7;
    lateChangedPose=play.executionGraph()->setTransform(target,pose);
    const auto &access=FakeRuntime::sceneAccess;
    lateQueuedDestroy=access.destroyObject(access.context,disposable)==1;
  };
  AE_EXPECT_TRUE(play.step(),"one complete paused frame");
  AE_EXPECT_EQ(FakeRuntime::lateUpdates,1u,"step invokes LateUpdate exactly once");
  AE_EXPECT_TRUE(lateSawPhysics && lateChangedPose && lateQueuedDestroy,"LateUpdate follows Update and physics and applies runtime changes");
  AE_EXPECT_EQ(play.document().find(camera)->transform.position[0],7.f,"CameraFollow reads LateUpdate pose in the same frame");
  AE_EXPECT_TRUE(!play.document().find(disposable) && play.pendingCommandCount()==0,"LateUpdate destruction drained before frame ends");
  AE_EXPECT_TRUE(std::abs(play.world().elapsedSeconds()-1./60)<1e-9,"step advances one fixed frame of time");
  AE_EXPECT_TRUE(play.advance(.2),"step preserves pause");
  AE_EXPECT_EQ(FakeRuntime::updates,1u,"advance after step stays idle");
  FakeRuntime::onLateUpdate={};
  play.stop();
}

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
  AE_EXPECT_EQ(FakeRuntime::lateUpdates, FakeRuntime::updates, "LateUpdate uma vez por quadro, depois da física");
  // A caixa cai sobre o piso: o contato sólido precisa atravessar
  // EditorPlayScene -> ScriptBridge -> ABI.
  AE_EXPECT_TRUE(FakeRuntime::contacts > 0, "contato sólido entregue ao runtime");
  AE_EXPECT_TRUE((FakeRuntime::lastContactFirst == box && FakeRuntime::lastContactSecond == floorId) ||
                 (FakeRuntime::lastContactFirst == floorId && FakeRuntime::lastContactSecond == box),
                 "o par entregue é piso/caixa");
  AE_EXPECT_TRUE(play.document().find(box)->transform.position[1] < 3.f, "a caixa caiu");

  // Pausa e foco do aplicativo atravessam EditorPlayScene -> ScriptBridge -> ABI.
  AE_EXPECT_TRUE(play.applicationEvent(scene::ScriptLifecycleEvent::ApplicationPause,true),"pausa entregue");
  AE_EXPECT_EQ(FakeRuntime::lastLifecycle,1u,"evento de pausa com valor verdadeiro");
  AE_EXPECT_TRUE(play.applicationEvent(scene::ScriptLifecycleEvent::ApplicationFocus,false),"perda de foco entregue");
  AE_EXPECT_EQ(FakeRuntime::lastLifecycle,2u,"evento de foco com valor falso");
  AE_EXPECT_EQ(FakeRuntime::lifecycleEvents,2u,"dois eventos do aplicativo");
  play.stop();
  AE_EXPECT_EQ(FakeRuntime::stops, 1u, "Stop encerra o runtime");
  AE_EXPECT_TRUE(play.applicationEvent(scene::ScriptLifecycleEvent::ApplicationPause,true),"sem Play, evento é ignorado");
  AE_EXPECT_EQ(FakeRuntime::lifecycleEvents,2u,"nada chega ao runtime parado");
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

AE_TEST(character_rebuild_fixedupdate_body_edit_preserves_abi_move_and_accepted_jump) {
  FakeRuntime::reset();EditorDocument doc;const auto floor=physical(doc,"Floor",-.5f,scene::BodyMotion::Static,.5f);
  const auto actor=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Actor");auto value=*doc.find(actor);value.transform.position[1]=1;auto*character=static_cast<scene::Character*>(value.components.add(scene::Character::descriptor));character->speed=2;character->jumpSpeed=5;doc.applyEntityValues(actor,value);attachScript(doc,actor,"project.Teste");
  EditorMapScene resources;EditorPlayScene play;play.setScriptRuntime(FakeRuntime::api(),"/project");AE_EXPECT_TRUE(play.start(doc,resources),"actual Play/ABI");for(u32 frame=0;frame<120;++frame)AE_EXPECT_TRUE(play.advance(1./60),"settle real capsule");const auto before=play.document().find(actor)->transform;const auto body=play.world().findComponent(play.world().handle(floor),scene::PhysicsBody::descriptor.id);
  FakeRuntime::onUpdate=[&](){const float move[3]{1,0,0};auto&abi=FakeRuntime::sceneAccess;AE_EXPECT_TRUE(abi.characterMove(abi.context,actor,move)==1&&abi.characterJump(abi.context,actor)==1,"commands accepted before FixedUpdate");};
  FakeRuntime::onFixedUpdate=[&](){const u8 property[]{'f','r','i','c','t','i','o','n'};float friction=.7f;u32 bits=0;std::memcpy(&bits,&friction,sizeof(bits));auto&abi=FakeRuntime::sceneAccess;AE_EXPECT_TRUE(abi.setProperty(abi.context,floor,body.instance,property,8,0,bits)==1,"body edit enters actual script ABI");};
  AE_EXPECT_TRUE(play.advance(1./60),"real callback safe-point reconciliation");AE_EXPECT_TRUE(!test::currentTestFailed(),"callback assertions");const auto after=play.document().find(actor)->transform;AE_EXPECT_TRUE(after.position[0]>before.position[0]+.02f&&after.position[1]>before.position[1]+.04f,"substep sees restored intentions and refreshed support");
  play.stop();FakeRuntime::reset();
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

AE_TEST(play_tags_roundtrip_bridge_activity_and_destroy) {
  EditorDocument doc;
  runtime::ObjectTags tags;AE_EXPECT_TRUE(tags.add("Alvo") && tags.add("Missão"),"catálogo UTF-8");doc.setTags(tags);
  const auto driver=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Driver");
  const auto parent=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Parent");
  const auto child=doc.createEntity(parent,EditorEntityKind::Folder,"Child");
  attachScript(doc,driver,"project.TagProbe");
  auto values=*doc.find(child);values.tag="Alvo";doc.applyEntityValues(child,values);doc.setActive(parent,false);
  EditorDocument restored;
  AE_EXPECT_TRUE(deserializeEditorDocument(serializeEditorDocument(doc,0),0,restored),"arquivo v14");
  AE_EXPECT_TRUE(restored.find(child)->tag=="Alvo","atribuição persistida");restored.setTags(tags);
  FakeRuntime::reset();EditorMapScene resources;EditorPlayScene play;
  play.setScriptRuntime(FakeRuntime::api(),"/projeto");
  AE_EXPECT_TRUE(play.start(restored,resources),"Play real");
  const auto &abi=FakeRuntime::sceneAccess;auto *context=abi.context;
  const auto *target=reinterpret_cast<const u8*>("Alvo");u64 found[2]{};
  AE_EXPECT_EQ(abi.findTagged(context,target,4,found,2,0),0,"filho de pai inativo não encontrado");
  AE_EXPECT_EQ(abi.compareTag(context,child,target,4),1,"comparar independe de ativação");
  AE_EXPECT_TRUE(abi.setActive(context,parent,1)!=0,"ativar pai");
  AE_EXPECT_EQ(abi.findTagged(context,target,4,nullptr,0,0),1,"consulta de tamanho");
  AE_EXPECT_EQ(abi.findTagged(context,target,4,found,2,1),1,"primeiro objeto");
  AE_EXPECT_EQ(found[0],static_cast<u64>(child),"busca atravessa outras subárvores");
  const std::string utf8="Missão";const auto *text=reinterpret_cast<const u8*>(utf8.data());
  AE_EXPECT_TRUE(abi.setTag(context,child,text,static_cast<int>(utf8.size()))!=0,"atribuir UTF-8");
  u8 bytes[64]{};const auto size=abi.getTag(context,child,bytes,64);
  AE_EXPECT_TRUE(std::string(reinterpret_cast<char*>(bytes),size)==utf8,"leitura exata");
  AE_EXPECT_EQ(abi.findTagged(context,target,4,found,2,0),0,"troca elimina associação anterior");
  AE_EXPECT_EQ(abi.compareTag(context,child,reinterpret_cast<const u8*>("missing"),7),-1,"tag desconhecida é erro");
  AE_EXPECT_EQ(abi.setTag(context,child,reinterpret_cast<const u8*>("missing"),7),0,"escrita desconhecida recusada");
  AE_EXPECT_EQ(abi.findTagged(context,reinterpret_cast<const u8*>("missing"),7,found,2,0),-1,"busca desconhecida é erro");
  const auto h=play.world().handle(child);play.world().destroyObject(h);
  AE_EXPECT_EQ(abi.findTagged(context,text,static_cast<int>(utf8.size()),found,2,0),0,"destruição pendente já excluída");
  AE_EXPECT_EQ(abi.getTag(context,child,bytes,64),-1,"objeto destruído não resolve");
  play.stop();AE_EXPECT_TRUE(restored.find(child)->tag=="Alvo","Play não altera autoria");
  restored.setTags({});
  AE_EXPECT_TRUE(!play.start(restored,resources),"referência sem catálogo bloqueia execução");
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

AE_TEST(play_enabled_collider_changed_in_start_reaches_physics_before_first_update) {
  EditorDocument doc;EditorMapScene resources;
  const auto id=physical(doc,"Body",0,scene::BodyMotion::Static,.5f);attachScript(doc,id,"test.enabled");
  FakeRuntime::reset();EditorPlayScene play;play.setScriptRuntime(FakeRuntime::api(),"/projeto");
  FakeRuntime::onStart=[&] {
    const auto component=play.world().findComponent(play.world().handle(id),"astra.physics.collider");
    AE_EXPECT_TRUE(play.world().setProperty(component,"enabled",false)==runtime::WorldStatus::Ok,"Start desliga o último colisor");
  };
  AE_EXPECT_TRUE(play.start(doc,resources),"Play inicia com alteração de Start");
  const float origin[3]{0,5,0},direction[3]{0,-10,0};scene::ScriptQueryHit hit;scene::ScriptQueryFilter filter;
  AE_EXPECT_TRUE(FakeRuntime::sceneAccess.rayCast(FakeRuntime::sceneAccess.context,origin,direction,&filter,&hit,1)==0,"primeiro Update já enxerga a alteração");
  play.stop();FakeRuntime::reset();
}

AE_TEST(play_abi_v16_dynamic_script_and_delayed_destroy_respect_pause_and_step) {
  EditorDocument doc;const auto driver=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Driver");
  const auto target=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Target");attachScript(doc,driver,"test.driver");
  FakeRuntime::reset();EditorMapScene resources;EditorPlayScene play;play.setScriptRuntime(FakeRuntime::api(),"/project");
  AE_EXPECT_TRUE(play.start(doc,resources),"Play");const auto &abi=FakeRuntime::sceneAccess;auto *c=abi.context;
  const auto instance=abi.addBehavior(c,target,reinterpret_cast<const u8*>("test.dynamic"),12,reinterpret_cast<const u8*>("Probe.cs"),8);
  AE_EXPECT_TRUE(instance && abi.componentCount(c,target)==1,"ABI installs actual component");
  AE_EXPECT_TRUE(abi.removeComponent(c,target,instance) && abi.componentCount(c,target)==0,"ABI removes only script");
  AE_EXPECT_TRUE(!abi.addBehavior(c,~u64(0),reinterpret_cast<const u8*>("test.dynamic"),12,reinterpret_cast<const u8*>("Probe.cs"),8),"oversized ID rejected");
  AE_EXPECT_TRUE(abi.destroyAfter(c,target,.03),"scheduled");play.pause(true);play.advance(1);
  AE_EXPECT_TRUE(play.world().alive(play.world().handle(target)),"pause freezes deadline");
  AE_EXPECT_TRUE(play.step(),"first simulated step");AE_EXPECT_TRUE(play.world().alive(play.world().handle(target)),"not yet due");
  AE_EXPECT_TRUE(play.step() && !play.world().graph().exists(target),"second step crosses deadline and drains");
  play.stop();AE_EXPECT_TRUE(doc.exists(target) && doc.find(target)->components.size()==0,"authoring remains unchanged");
}

AE_TEST(play_prefab_abi_creates_once_queries_attachments_and_can_rollback) {
  EditorDocument doc;const auto driver=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Driver");
  attachScript(doc,driver,"test.driver");
  EditorDocument source;const auto sourceRoot=source.createEntity(source.root(),EditorEntityKind::Folder,"Source");
  attachScript(source,sourceRoot,"test.prefab");
  const auto guid=resources::assetGuidFromSeed("ABI prefab");runtime::Prefab prefab;std::string error;
  AE_EXPECT_TRUE(prefab.capture(source,sourceRoot,guid,error),error.c_str());
  FakeRuntime::reset();EditorMapScene resources;EditorPlayScene play;
  play.setScriptRuntime(FakeRuntime::api(),"/project");
  play.setPrefabLoader([&](resources::AssetGuid id,runtime::Prefab &out,std::string &diagnostic) {
    if(id!=guid) {diagnostic="missing source";return false;}out=prefab;return true;
  });
  AE_EXPECT_TRUE(play.start(doc,resources),"Play");const auto &abi=FakeRuntime::sceneAccess;auto *c=abi.context;
  const auto count=play.document().entityCount();
  const auto root=abi.instantiatePrefab(c,doc.root(),{guid.high,guid.low});
  AE_EXPECT_TRUE(root && play.document().entityCount()==count+1,"exactly one creation");
  const auto size=abi.instantiationAttachments(c,root,nullptr,0);
  AE_EXPECT_TRUE(size>2 && play.document().entityCount()==count+1,"size query is read only");
  std::string json(static_cast<usize>(size),'\0');
  AE_EXPECT_EQ(abi.instantiationAttachments(c,root,reinterpret_cast<u8*>(json.data()),size),size,"copy attachment description");
  AE_EXPECT_TRUE(json.find("test.prefab")!=std::string::npos && json.find("test.driver")==std::string::npos,"only the new subtree scripts");
  AE_EXPECT_TRUE(abi.finishInstantiation(c,root,0) && play.document().entityCount()==count,"managed binding failure can rollback");
  AE_EXPECT_TRUE(!abi.instantiatePrefab(c,doc.root(),{1,2}) && play.document().entityCount()==count,"missing resource never creates objects");
  play.stop();FakeRuntime::reset();
}

AE_TEST(play_time_scale_drives_solver_timers_delayed_destroy_and_native_clock) {
  FakeRuntime::reset();EditorDocument doc;EditorMapScene resources;
  const auto body=physical(doc,"Clock body",8,scene::BodyMotion::Dynamic,.5f);
  attachScript(doc,body,"test.clock");
  auto value=*doc.find(body);auto *timer=static_cast<scene::Timer*>(value.components.add(scene::Timer::descriptor));
  timer->intervalSeconds=.05f;timer->repeat=true;
  AE_EXPECT_TRUE(doc.applyEntityValues(body,value),"timer authored");
  const auto transient=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Delayed");
  const auto authored=serializeEditorDocument(doc,0);
  EditorPlayScene play;play.setScriptRuntime(FakeRuntime::api(),"/project");
  AE_EXPECT_TRUE(play.start(doc,resources),"real Play and Jolt solver");
  const auto &abi=FakeRuntime::sceneAccess;const auto world=play.world().worldId();
  AE_EXPECT_TRUE(abi.setTimeScale(abi.context,world,.5f),"native scale mutation");
  AE_EXPECT_TRUE(play.world().destroyAfter(play.world().handle(transient),.04)==runtime::WorldStatus::Ok,"scaled destruction scheduled");
  AE_EXPECT_TRUE(play.advance(1.0/60.0) && FakeRuntime::fixedUpdates==0,"half speed defers fixed dispatch");
  AE_EXPECT_TRUE(play.advance(1.0/60.0) && FakeRuntime::fixedUpdates==1,"two frames consume one physical step");
  const auto y=play.document().find(body)->transform.position[1];
  AE_EXPECT_TRUE(y<8 && FakeRuntime::timerExpirations==0,"body consumed step; timer not due");
  AE_EXPECT_TRUE(abi.setTimeScale(abi.context,world,0) && play.advance(.1),"freeze simulation while dispatch continues");
  AE_EXPECT_TRUE(FakeRuntime::updates==3 && FakeRuntime::lateUpdates==3 && FakeRuntime::fixedUpdates==1,"Update/LateUpdate continue, FixedUpdate stops");
  AE_EXPECT_TRUE(play.document().find(body)->transform.position[1]==y && play.world().alive(play.world().handle(transient)),"pose and destruction frozen");
  scene::ScriptTimeState snapshot{};
  AE_EXPECT_TRUE(abi.timeSnapshot(abi.context,world,&snapshot),"authoritative ABI snapshot");
  AE_EXPECT_TRUE(snapshot.frameCount==3 && snapshot.delta==0 && snapshot.unscaledDelta==.1f && std::abs(snapshot.unscaledTime-(.1+1.0/30))<1e-8,"unscaled time remains observable");
  AE_EXPECT_TRUE(!abi.setTimeScale(abi.context,world,std::numeric_limits<float>::quiet_NaN()) && !abi.setTimeScale(abi.context,world,5),"invalid scale rejected");
  AE_EXPECT_TRUE(!abi.timeSnapshot(abi.context,world+1,&snapshot),"foreign session refused");
  const auto frame=play.world().clock().frameCount();
  AE_EXPECT_TRUE(!play.advance(std::numeric_limits<double>::quiet_NaN()) && play.world().clock().frameCount()==frame,"invalid delta is atomic");
  play.pause(true);AE_EXPECT_TRUE(play.step(),"editor Step overrides zero scale for one frame");
  AE_EXPECT_TRUE(play.world().clock().scale()==0 && FakeRuntime::fixedUpdates==2 && FakeRuntime::timerExpirations==0,"Step retains requested scale and consumes timers/physics");
  AE_EXPECT_TRUE(abi.setTimeScale(abi.context,world,1),"resume normal time");play.pause(false);
  AE_EXPECT_TRUE(play.advance(1.0/60) && !play.document().exists(transient) && FakeRuntime::timerExpirations==1,"timer and delayed destruction use scaled time and safe point");
  AE_EXPECT_TRUE(serializeEditorDocument(doc,0)==authored,"Play clock never mutates authoring");
  play.stop();AE_EXPECT_TRUE(!abi.timeSnapshot(abi.context,world,&snapshot),"ended session refused");
  AE_EXPECT_TRUE(play.start(doc,resources) && play.world().clock().scale()==1 && play.world().clock().frameCount()==0,"restart resets session clock");
  play.stop();FakeRuntime::reset();
}

AE_TEST(play_time_scale_changes_in_update_take_effect_on_next_whole_frame) {
  FakeRuntime::reset();EditorDocument doc;EditorMapScene resources;
  const auto body=physical(doc,"Next frame",8,scene::BodyMotion::Dynamic,.5f);attachScript(doc,body,"test.clock");
  EditorPlayScene play;play.setScriptRuntime(FakeRuntime::api(),"/project");
  AE_EXPECT_TRUE(play.start(doc,resources),"Play");
  FakeRuntime::onUpdate=[&] {const auto &abi=FakeRuntime::sceneAccess;abi.setTimeScale(abi.context,play.world().worldId(),0);};
  AE_EXPECT_TRUE(play.advance(1.0/60) && FakeRuntime::fixedUpdates==1,"scale write cannot split current frame");
  const auto y=play.document().find(body)->transform.position[1];
  AE_EXPECT_TRUE(play.advance(1.0/60) && FakeRuntime::fixedUpdates==1 && play.document().find(body)->transform.position[1]==y,"next frame freezes all physical work");
  play.stop();FakeRuntime::reset();
}

AE_TEST(play_time_scale_unscaled_timer_persists_and_dispatches_during_simulation_freeze) {
  scene::Timer legacy;legacy.ignoreTimeScale=true;std::istringstream old("0.1 1 1");
  AE_EXPECT_TRUE(legacy.read(old,1) && !legacy.ignoreTimeScale,"v1 timers migrate to scaled time");
  FakeRuntime::reset();EditorDocument doc;EditorMapScene resources;
  const auto id=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Unscaled timer");
  auto value=*doc.find(id);auto *timer=static_cast<scene::Timer*>(value.components.add(scene::Timer::descriptor));
  timer->intervalSeconds=.1f;timer->repeat=false;timer->ignoreTimeScale=true;const auto instance=timer->instanceId();
  AE_EXPECT_TRUE(doc.applyEntityValues(id,value),"unscaled timer authored");attachScript(doc,id,"test.clock");
  const auto file=serializeEditorDocument(doc,0);EditorDocument reopened;
  AE_EXPECT_TRUE(deserializeEditorDocument(file,0,reopened),"v2 archive reopens");
  AE_EXPECT_TRUE(static_cast<const scene::Timer*>(reopened.find(id)->components.findInstance(instance))->ignoreTimeScale,"clock mode persists by component identity");
  EditorPlayScene play;play.setScriptRuntime(FakeRuntime::api(),"/project");
  AE_EXPECT_TRUE(play.start(reopened,resources),"Play");
  AE_EXPECT_TRUE(play.world().setTimeScale(0)==runtime::WorldStatus::Ok && play.advance(.11),"zero scaled time with real frame");
  AE_EXPECT_TRUE(FakeRuntime::timerExpirations==1 && play.world().elapsedSeconds()==0 && FakeRuntime::fixedUpdates==0,"timer dispatch uses unscaled clock, physics stays frozen");
  const auto component=play.world().findComponent(play.world().handle(id),"astra.time.timer",0);
  AE_EXPECT_TRUE(play.world().setProperty(component,"ignore_time_scale",false)==runtime::WorldStatus::Ok,"mode is runtime editable");
  play.stop();FakeRuntime::reset();
}

AE_TEST(play_time_scale_unscaled_tween_migrates_persists_and_keeps_runtime_authority) {
  scene::TransformTween legacy;legacy.ignoreTimeScale=true;
  std::istringstream old("1 1 0 0 1 0 0 1 0 0 1 4 0 0 0 0 0 1 1 1");
  AE_EXPECT_TRUE(legacy.read(old,1) && !legacy.ignoreTimeScale,"legacy tween remains scaled");
  EditorDocument doc;EditorMapScene resources;
  const auto id=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Unscaled tween");auto value=*doc.find(id);
  auto *tween=static_cast<scene::TransformTween*>(value.components.add(scene::TransformTween::descriptor));
  tween->duration=1;tween->destination[0]=4;tween->ignoreTimeScale=true;const auto instance=tween->instanceId();
  AE_EXPECT_TRUE(doc.applyEntityValues(id,value),"author real transform tween");
  const auto file=serializeEditorDocument(doc,0);EditorDocument reopened;
  AE_EXPECT_TRUE(deserializeEditorDocument(file,0,reopened) && serializeEditorDocument(reopened,0)==file,"v2 mode roundtrips exactly");
  EditorPlayScene play;AE_EXPECT_TRUE(play.start(reopened,resources),"Play with actual tween consumer");
  AE_EXPECT_TRUE(play.world().setTimeScale(0)==runtime::WorldStatus::Ok && play.advance(.25),"unscaled dispatch");
  AE_EXPECT_TRUE(play.document().find(id)->transform.position[0]==1 && play.world().elapsedSeconds()==0,"unscaled tween changes pose during simulation freeze");
  play.pause(true);AE_EXPECT_TRUE(play.advance(.25) && play.document().find(id)->transform.position[0]==1,"editor pause stops both clocks");
  const auto component=play.world().findComponent(play.world().handle(id),"astra.tween.transform",0);
  AE_EXPECT_TRUE(play.world().setProperty(component,"ignore_time_scale",false)==runtime::WorldStatus::Ok,"live clock switch");
  play.pause(false);AE_EXPECT_TRUE(play.advance(.25) && play.document().find(id)->transform.position[0]==1,"scaled mode freezes without resetting progress");
  AE_EXPECT_TRUE(play.world().setTimeScale(1)==runtime::WorldStatus::Ok && play.advance(.25) && play.document().find(id)->transform.position[0]==2,"resume retains elapsed state");
  AE_EXPECT_TRUE(play.tweens().cancel(play.world(),id,instance) && play.advance(.25) && play.document().find(id)->transform.position[0]==2,"cancellation remains authoritative");
  play.stop();
}

AE_TEST(timer_connection_roundtrip_migration_clone_and_native_activation) {
  EditorDocument doc;
  const auto source=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Emissor");
  const auto receiver=doc.createEntity(source,EditorEntityKind::Folder,"Receptor");
  AE_EXPECT_TRUE(doc.setActive(receiver,false),"receptor autoral inativo");
  auto value=*doc.find(source);
  auto *timer=static_cast<scene::Timer*>(value.components.add(scene::Timer::descriptor));
  timer->intervalSeconds=.1f;timer->repeat=false;timer->elapsedAction=1;timer->elapsedTarget=receiver;
  const auto instance=timer->instanceId();
  EditorHistory history;
  AE_EXPECT_TRUE(history.applyValues(doc,source,value),"conexão editada pelo histórico");
  AE_EXPECT_TRUE(history.undo(doc) && !doc.find(source)->components.find(scene::Timer::descriptor),"Undo retira conexão e timer juntos");
  AE_EXPECT_TRUE(history.redo(doc),"Redo restaura autoria");
  EditorDocument loaded;
  AE_EXPECT_TRUE(deserializeEditorDocument(serializeEditorDocument(doc,0),0,loaded),"conexão persistente");
  runtime::ObjectCloneMap mapping;
  const auto clone=loaded.cloneSubtree(source,loaded.root(),mapping);
  AE_EXPECT_TRUE(clone!=0,"composição clonada");
  const auto *cloned=static_cast<const scene::Timer*>(loaded.find(clone)->components.findInstance(instance));
  AE_EXPECT_TRUE(cloned && cloned->elapsedTarget!=receiver && loaded.find(cloned->elapsedTarget)->parent==clone,"receptor remapeado dentro da composição");
  runtime::GameWorld world;runtime::SceneTimers timers;
  AE_EXPECT_TRUE(world.load(loaded),"mundo real");
  u32 calls=0;bool callbackSawActive=true;
  auto callback=[&](runtime::ObjectId object,u64,u32 count) {
    calls+=count;
    if(object==source)callbackSawActive=world.activeSelf(world.handle(receiver));
    return true;
  };
  AE_EXPECT_TRUE(timers.advance(world,.11,callback),"timeout aplica ação e entrega evento");
  AE_EXPECT_TRUE(callbackSawActive && world.activeSelf(world.handle(receiver)),"ativação precede callback");
  AE_EXPECT_EQ(calls,2u,"cada composição entrega seu evento");
  AE_EXPECT_TRUE(!loaded.find(receiver)->active,"ativação de Play não alterou autoria");
  scene::Timer legacy;legacy.elapsedAction=3;legacy.elapsedTarget=receiver;
  std::istringstream old("0.1 1 1 0");
  AE_EXPECT_TRUE(legacy.read(old,2) && legacy.elapsedAction==0 && legacy.elapsedTarget==0,"v2 migra explicitamente desconectado");
  std::istringstream invalid("0.1 1 1 0 4 2");
  AE_EXPECT_TRUE(!legacy.read(invalid,3),"arquivo não inventa operação desconhecida");
}

AE_TEST(timer_connection_aggregated_toggle_pending_removal_and_teardown) {
  EditorDocument doc;
  const auto source=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Emissor");
  const auto receiver=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Receptor");
  auto value=*doc.find(source);auto *timer=static_cast<scene::Timer*>(value.components.add(scene::Timer::descriptor));
  timer->intervalSeconds=.1f;timer->elapsedAction=3;timer->elapsedTarget=receiver;const auto instance=timer->instanceId();
  AE_EXPECT_TRUE(doc.applyEntityValues(source,value),"conexão tipada");
  runtime::GameWorld world;runtime::SceneTimers timers;AE_EXPECT_TRUE(world.load(doc),"mundo carregado");
  auto fire=[](runtime::ObjectId,u64,u32){return true;};
  AE_EXPECT_TRUE(timers.advance(world,.25,fire) && world.activeSelf(world.handle(receiver)),"dois disparos mantêm estado inicial");
  AE_EXPECT_TRUE(timers.advance(world,.1,fire) && !world.activeSelf(world.handle(receiver)),"disparo ímpar alterna estado");
  AE_EXPECT_TRUE(world.destroyObject(world.handle(receiver))==runtime::WorldStatus::Ok,"receptor removido em fila");
  AE_EXPECT_TRUE(timers.advance(world,.1,fire),"receptor pendente nunca é acionado");
  const auto *state=timers.state(source,instance);
  AE_EXPECT_TRUE(state && state->connection==runtime::SceneTimers::ConnectionStatus::MissingTarget,"ausência é inspecionável");
  world.flush();
  const auto component=world.findComponent(world.handle(source),"astra.time.timer");
  AE_EXPECT_TRUE(world.removeComponent(component)==runtime::WorldStatus::Ok,"emissor removido");world.flush();
  AE_EXPECT_TRUE(timers.advance(world,.1,fire) && !timers.state(source,instance),"teardown retira estado e conexão");
}

AE_TEST(input_profiles_abi26_reads_overrides_exports_and_imports_real_play_input) {
  FakeRuntime::reset();EditorDocument doc;EditorMapScene resources;EditorPlayScene play;
  const auto id=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Rebind probe");attachScript(doc,id,"project.Teste");
  play.setScriptRuntime(FakeRuntime::api(),"/projeto");AE_EXPECT_TRUE(play.start(doc,resources),"real bridge running");
  const auto &abi=FakeRuntime::sceneAccess;const auto *name=reinterpret_cast<const u8*>("Saltar");
  AE_EXPECT_TRUE(abi.version==scene::ScriptSceneAccess{}.version&&abi.available(),"complete ABI26");auto incomplete=abi;incomplete.inputProfile=nullptr;
  AE_EXPECT_TRUE(!incomplete.available(),"profiles are required by new ABI");
  scene::ScriptInputBinding binding;
  AE_EXPECT_TRUE(abi.inputBindingCommand(abi.context,name,6,0,4,&binding)==1&&binding.source==3,"read authored touch binding");
  binding={7,2,0,0,1,0};AE_EXPECT_EQ(abi.inputBindingCommand(abi.context,name,6,0,1,&binding),1,"apply middle mouse override");
  runtime::InputDeviceState raw;raw.mouseButtons=4;play.submitInput(raw);
  AE_EXPECT_EQ(abi.inputButton(abi.context,name,6,1),1,"override reaches ABI gameplay query");
  const int size=abi.inputProfile(abi.context,0,nullptr,0,nullptr,0);AE_EXPECT_TRUE(size>0&&size<262144,"bounded size probe");
  std::vector<u8> profile(static_cast<usize>(size));AE_EXPECT_EQ(abi.inputProfile(abi.context,0,nullptr,0,profile.data(),size),size,"copy complete profile");
  AE_EXPECT_EQ(abi.inputBindingCommand(abi.context,nullptr,0,0,3,nullptr),1,"restore all");play.submitInput(raw);
  AE_EXPECT_EQ(abi.inputButton(abi.context,name,6,0),0,"authored touch does not respond to middle mouse");
  AE_EXPECT_EQ(abi.inputProfile(abi.context,1,profile.data(),size,nullptr,0),1,"import atomically");play.submitInput(raw);
  AE_EXPECT_EQ(abi.inputButton(abi.context,name,6,1),1,"loaded profile produces press edge");
  AE_EXPECT_TRUE(doc.inputActions().isDefault(),"scene authoring remains untouched");
  play.stop();AE_EXPECT_TRUE(play.start(doc,resources),"new Play world");
  AE_EXPECT_EQ(FakeRuntime::sceneAccess.inputBindingCommand(FakeRuntime::sceneAccess.context,name,6,0,0,&binding),1,"binding available after restart");
  AE_EXPECT_EQ(binding.source,3u,"new Play uses authored defaults until explicit profile load");play.stop();
}

AE_TEST(input_runtime_capture_abi27_consumes_unbound_platform_keys_and_cancellation) {
  FakeRuntime::reset();EditorDocument doc;EditorMapScene resources;EditorPlayScene play;
  const auto id=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Capture probe");attachScript(doc,id,"project.Teste");
  play.setScriptRuntime(FakeRuntime::api(),"/projeto");AE_EXPECT_TRUE(play.start(doc,resources),"bridge running");const auto &abi=FakeRuntime::sceneAccess;
  const auto *name=reinterpret_cast<const u8*>("Saltar");auto incomplete=abi;incomplete.inputCaptureCommand=nullptr;AE_EXPECT_TRUE(!incomplete.available(),"ABI27 requires capture callback");
  AE_EXPECT_EQ(abi.inputCaptureCommand(abi.context,1,name,6,0,4,0,111),1,"begin key capture through ABI");
  runtime::InputDeviceState device;play.submitInput(device);device.keys={62};play.submitInput(device);
  AE_EXPECT_EQ(abi.inputCaptureCommand(abi.context,0,nullptr,0,0,0,0,0),2,"completed status through ABI");scene::ScriptInputBinding binding;
  AE_EXPECT_EQ(abi.inputBindingCommand(abi.context,name,6,0,0,&binding),1,"read effective captured binding");AE_EXPECT_EQ(binding.code,62u,"unbound platform key committed");
  AE_EXPECT_EQ(abi.inputButton(abi.context,name,6,0),0,"capturing event cannot jump");device.keys.clear();play.submitInput(device);device.keys={62};play.submitInput(device);
  AE_EXPECT_EQ(abi.inputButton(abi.context,name,6,1),1,"next real press reaches C# ABI consumer");
  AE_EXPECT_EQ(abi.inputCaptureCommand(abi.context,1,name,6,0,7,0,111),1,"begin mouse capture");
  AE_EXPECT_EQ(abi.inputCaptureCommand(abi.context,2,nullptr,0,0,0,0,0),3,"explicit cancel");
  AE_EXPECT_EQ(abi.inputCaptureCommand(abi.context,1,name,6,0,4,0,111),1,"begin before suspension");
  AE_EXPECT_TRUE(play.applicationEvent(scene::ScriptLifecycleEvent::ApplicationPause,true),"native pause lifecycle");
  AE_EXPECT_EQ(abi.inputCaptureCommand(abi.context,0,nullptr,0,0,0,0,0),3,"application suspension cancels capture before frames stop");play.stop();
  AE_EXPECT_EQ(abi.inputCaptureCommand(abi.context,0,nullptr,0,0,0,0,0),-1,"stopped world refuses retained capture access");
  AE_EXPECT_EQ(abi.inputProfile(abi.context,0,nullptr,0,nullptr,0),-1,"stopped world refuses retained profile access");
  AE_EXPECT_EQ(abi.inputBindingCommand(abi.context,name,6,0,0,&binding),0,"stopped world refuses retained binding access");
}

AE_TEST(physics_connections_real_jolt_sensor_activates_receiver_before_script_and_stop_restores_authoring) {
  FakeRuntime::reset();EditorDocument doc;EditorMapScene resources;EditorPlayScene play;
  const auto sensor=physical(doc,"Sensor conectado",0,scene::BodyMotion::Static,.5f);
  const auto incoming=physical(doc,"Corpo entrante",3,scene::BodyMotion::Dynamic,.5f);
  const auto receiver=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Luz receptora");auto lamp=*doc.find(receiver);
  lamp.active=false;lamp.components.add(scene::Light::descriptor);AE_EXPECT_TRUE(doc.applyEntityValues(receiver,lamp),"inactive authored light");
  auto value=*doc.find(sensor);editPhysicsBody(value)->sensor=true;
  auto *connection=static_cast<scene::PhysicsEventConnection3D*>(value.components.add(scene::PhysicsEventConnection3D::descriptor));
  connection->action=1;connection->receiver=receiver;connection->otherFilter=incoming;
  AE_EXPECT_TRUE(doc.applyEntityValues(sensor,value),"typed connection on real sensor");attachScript(doc,sensor,"project.Teste");
  static runtime::GameWorld *observedWorld;static runtime::ObjectId observedReceiver;static bool sawReady;
  observedWorld=&play.world();observedReceiver=receiver;sawReady=false;
  auto api=FakeRuntime::api();api.trigger=[](u64,u64,u32 phase)->int {
    if(phase==0)sawReady=observedWorld->activeSelf(observedWorld->handle(observedReceiver));
    return 0;
  };
  play.setScriptRuntime(api,"/projeto");AE_EXPECT_TRUE(play.start(doc,resources),"real Jolt and native connections start");
  for(u32 frame=0;frame<120&&!sawReady;++frame)AE_EXPECT_TRUE(play.advance(.02f),"physical simulation");
  AE_EXPECT_TRUE(sawReady&&play.physicsConnections().deliveries()>0,"receiver activation precedes actual trigger callback");
  std::vector<renderer::SceneLight> lights;AE_EXPECT_TRUE(runtime::collectSceneLights(play.document(),lights)&&lights.size()==1,"activation reaches render light extraction");
  AE_EXPECT_TRUE(!doc.find(receiver)->active,"authoring was not mutated");play.stop();
  AE_EXPECT_TRUE(play.physicsConnections().deliveries()==0&&play.physicsConnections().diagnostic().empty(),"Stop tears down connection diagnostics and counts");
}
AE_TEST(physics_connections_event_filter_toggle_removal_and_storage_are_one_chain) {
  EditorDocument doc;const auto first=physical(doc,"First",0,scene::BodyMotion::Static,.5f);
  const auto second=physical(doc,"Second",3,scene::BodyMotion::Dynamic,.5f);
  const auto target=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Receiver");auto receiver=*doc.find(target);receiver.active=false;doc.applyEntityValues(target,receiver);
  auto value=*doc.find(first);auto *connection=static_cast<scene::PhysicsEventConnection3D*>(value.components.add(scene::PhysicsEventConnection3D::descriptor));
  connection->event=3;connection->action=3;connection->receiver=target;connection->otherFilter=second;const auto instance=connection->instanceId();doc.applyEntityValues(first,value);
  EditorHistory history;history.clear();value=*doc.find(first);static_cast<scene::PhysicsEventConnection3D*>(value.components.editInstance(instance))->enabled=false;
  AE_EXPECT_TRUE(history.applyValues(doc,first,value)&&history.undo(doc)&&history.redo(doc)&&history.undo(doc),"property edit and history");
  const auto archive=serializeEditorDocument(doc,0);EditorDocument reopened;AE_EXPECT_TRUE(deserializeEditorDocument(archive,0,reopened),"connection roundtrip");
  runtime::ObjectCloneMap mapping;const auto duplicate=reopened.cloneSubtree(first,reopened.root(),mapping);const auto *copied=static_cast<const scene::PhysicsEventConnection3D*>(reopened.find(duplicate)->components.find(scene::PhysicsEventConnection3D::descriptor));
  AE_EXPECT_TRUE(copied&&copied->receiver==target&&copied->otherFilter==second&&duplicate!=first&&copied->instanceId()==instance,"clone preserves references and object-scoped component identity");
  runtime::GameWorld world;AE_EXPECT_TRUE(world.load(doc),"runtime loads authored data");runtime::ScenePhysicsConnections runtime;
  runtime.trigger(world,first,second,0);AE_EXPECT_TRUE(!world.activeSelf(world.handle(target)),"trigger cannot satisfy contact filter");
  runtime.contact(world,first,target,0);AE_EXPECT_TRUE(!world.activeSelf(world.handle(target)),"other-object filter rejects unmatched contact");
  runtime.contact(world,first,second,0);AE_EXPECT_TRUE(world.activeSelf(world.handle(target))&&runtime.deliveries()==1,"matching contact toggles actual receiver");
  AE_EXPECT_TRUE(world.destroyObject(world.handle(target))==runtime::WorldStatus::Ok,"pending receiver removal");runtime.contact(world,first,second,0);
  AE_EXPECT_TRUE(runtime.deliveries()==1&&!runtime.diagnostic().empty(),"pending receiver excluded and diagnosed without pointer access");
}

AE_TEST(physics_connections_reject_mismatched_sensor_event_without_starting_a_partial_world) {
  EditorDocument doc;EditorMapScene resources;EditorPlayScene play;
  const auto emitter=physical(doc,"Sensor inválido",0,scene::BodyMotion::Static,.5f);
  const auto receiver=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Receptor");
  auto value=*doc.find(emitter);auto *connection=static_cast<scene::PhysicsEventConnection3D*>(value.components.add(scene::PhysicsEventConnection3D::descriptor));
  connection->action=1;connection->receiver=receiver;doc.applyEntityValues(emitter,value);
  AE_EXPECT_TRUE(!play.start(doc,resources)&&!play.active(),"sensor event on solid body is explicitly refused");
  value=*doc.find(emitter);editPhysicsBody(value)->sensor=true;doc.applyEntityValues(emitter,value);
  AE_EXPECT_TRUE(play.start(doc,resources),"matching real sensor starts");
  const auto handle=play.world().findComponent(play.world().handle(emitter),scene::PhysicsEventConnection3D::descriptor.id);
  AE_EXPECT_TRUE(play.world().setProperty(handle,"event",u32{3})==runtime::WorldStatus::Ok&&!play.commitEdits(),"runtime incompatible event edit is diagnosed at the safe point");play.stop();
  value=*doc.find(emitter);static_cast<scene::PhysicsEventConnection3D*>(value.components.edit(scene::PhysicsEventConnection3D::descriptor))->event=3;doc.applyEntityValues(emitter,value);
  AE_EXPECT_TRUE(!play.start(doc,resources)&&!play.active(),"contact event on sensor is explicitly refused");
}

AE_TEST(physics2d_connections_real_box2d_sensor_activates_receiver_before_script_and_stop_restores_authoring) {
  FakeRuntime::reset();EditorDocument doc;EditorMapScene resources;EditorPlayScene play;
  const auto physical2D=[&](const char *name,float y,bool dynamic,bool sensor) {
    const auto id=doc.createEntity(doc.root(),EditorEntityKind::Folder,name);auto value=*doc.find(id);value.transform.position[1]=y;
    auto *body=static_cast<scene::Body2D*>(value.components.add(scene::Body2D::descriptor));body->motion=dynamic?scene::Body2DMotion::Dynamic:scene::Body2DMotion::Static;
    auto *shape=static_cast<scene::Collider2D*>(value.components.add(scene::Collider2D::descriptor));shape->sensor=sensor;shape->halfX=sensor?8:.5f;
    doc.applyEntityValues(id,value);return id;
  };
  const auto sensor=physical2D("Sensor conectado",0,false,true);
  const auto incoming=physical2D("Corpo entrante",3,true,false);
  const auto receiver=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Luz receptora");auto lamp=*doc.find(receiver);
  lamp.active=false;lamp.components.add(scene::Light::descriptor);AE_EXPECT_TRUE(doc.applyEntityValues(receiver,lamp),"inactive authored light");
  auto value=*doc.find(sensor);
  auto *connection=static_cast<scene::PhysicsEventConnection2D*>(value.components.add(scene::PhysicsEventConnection2D::descriptor));
  connection->action=1;connection->receiver=receiver;connection->otherFilter=incoming;
  auto *exit=static_cast<scene::PhysicsEventConnection2D*>(value.components.add(scene::PhysicsEventConnection2D::descriptor));exit->event=2;exit->action=2;exit->receiver=receiver;exit->otherFilter=incoming;
  AE_EXPECT_TRUE(doc.applyEntityValues(sensor,value),"typed connection on real sensor");attachScript(doc,sensor,"project.Teste");
  static runtime::GameWorld *observedWorld;static runtime::ObjectId observedReceiver;static bool sawReady;
  observedWorld=&play.world();observedReceiver=receiver;sawReady=false;
  auto api=FakeRuntime::api();api.trigger=[](u64,u64,u32 phase)->int {
    if(phase==0)sawReady=observedWorld->activeSelf(observedWorld->handle(observedReceiver));
    return 0;
  };
  play.setScriptRuntime(api,"/projeto");AE_EXPECT_TRUE(play.start(doc,resources),"real Box2D and native connections start");
  for(u32 frame=0;frame<120&&!sawReady;++frame)AE_EXPECT_TRUE(play.advance(.02f),"physical simulation");
  AE_EXPECT_TRUE(sawReady&&play.physicsConnections().deliveries()>0,"receiver activation precedes actual trigger callback");
  std::vector<renderer::SceneLight> lights;AE_EXPECT_TRUE(runtime::collectSceneLights(play.document(),lights)&&lights.size()==1,"activation reaches render light extraction");
  AE_EXPECT_TRUE(play.world().destroyObject(play.world().handle(incoming))==runtime::WorldStatus::Ok&&play.commitEdits(),"visitor removal retires real body");
  AE_EXPECT_TRUE(play.advance(.02f)&&!play.world().activeSelf(play.world().handle(receiver)),"queued exit on visitor destruction reaches surviving sensor connection");
  AE_EXPECT_TRUE(!doc.find(receiver)->active,"authoring was not mutated");play.stop();
  AE_EXPECT_TRUE(play.physicsConnections().deliveries()==0&&play.physicsConnections().diagnostic().empty(),"Stop tears down connection diagnostics and counts");
}

AE_TEST(timer_controls_native_commands_pause_restart_stop_and_lifecycle) {
  EditorDocument doc;const auto id=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Manual");auto value=*doc.find(id);
  auto *timer=static_cast<scene::Timer*>(value.components.add(scene::Timer::descriptor));timer->autoStart=false;timer->intervalSeconds=.1f;timer->repeat=false;
  const auto instance=timer->instanceId();doc.applyEntityValues(id,value);const auto saved=serializeEditorDocument(doc,0);
  runtime::GameWorld world;AE_EXPECT_TRUE(world.load(doc),"world");runtime::SceneTimers timers;runtime::SceneTimers::State state;const auto handle=runtime::ComponentHandle{world.handle(id),instance};u32 fired=0;
  const auto fire=[&](runtime::ObjectId,u64,u32 count){fired+=count;return true;};
  AE_EXPECT_TRUE(timers.advance(world,.2,fire)&&fired==0,"autostart disabled does not fire");
  AE_EXPECT_TRUE(timers.command(world,handle,3,0,state)==runtime::WorldStatus::Ok&&timers.command(world,handle,1,.15f,state)==runtime::WorldStatus::Ok&&state.running&&state.paused,"Start retains timer pause");
  const auto remaining=state.remaining;AE_EXPECT_TRUE(timers.advance(world,.2,fire)&&timers.state(id,instance)->remaining==remaining&&fired==0,"paused countdown freezes");
  AE_EXPECT_TRUE(timers.command(world,handle,4,0,state)==runtime::WorldStatus::Ok&&timers.advance(world,.2,fire)&&fired==1&&timers.state(id,instance)->completed&&!timers.state(id,instance)->running,"resume delivers exactly one timeout");
  AE_EXPECT_TRUE(world.setProperty(handle,"auto_start",true)==runtime::WorldStatus::Ok,"initial setting does not itself restart existing timer");
  AE_EXPECT_TRUE(timers.command(world,handle,1,0,state)==runtime::WorldStatus::Ok&&timers.command(world,handle,2,0,state)==runtime::WorldStatus::Ok&&state.remaining==0&&!state.running,"Stop clears countdown without timeout");
  AE_EXPECT_TRUE(timers.advance(world,.2,fire)&&fired==1,"Stop remains stopped despite enabled/autostart flags");
  AE_EXPECT_TRUE(timers.command(world,handle,1,.01f,state)==runtime::WorldStatus::InvalidArgument,"invalid interval rejected before write");
  AE_EXPECT_TRUE(world.destroyObject(world.handle(id))==runtime::WorldStatus::Ok&&timers.command(world,handle,0,0,state)!=runtime::WorldStatus::Ok,"pending owner cannot receive commands");
  AE_EXPECT_EQ(serializeEditorDocument(doc,0),saved,"runtime interval override does not mutate authoring");timers.reset();AE_EXPECT_TRUE(!timers.state(id,instance),"teardown clears countdown");
}
AE_TEST(timer_controls_abi_start_callback_snapshot_and_after_stop_rejection) {
  FakeRuntime::reset();EditorDocument doc;const auto id=doc.createEntity(doc.root(),EditorEntityKind::Folder,"ABI timer");auto value=*doc.find(id);
  auto *timer=static_cast<scene::Timer*>(value.components.add(scene::Timer::descriptor));timer->autoStart=false;timer->repeat=false;const auto instance=timer->instanceId();doc.applyEntityValues(id,value);attachScript(doc,id,"project.Teste");
  bool started=false;FakeRuntime::onStart=[&](){auto &abi=FakeRuntime::sceneAccess;scene::ScriptTimerState snapshot;
    started=abi.timerCommand(abi.context,id,instance,0,0,&snapshot)==1&&!(snapshot.flags&1)&&snapshot.remaining==0&&abi.timerCommand(abi.context,id,instance,1,.1f,&snapshot)==1&&(snapshot.flags&1);};
  EditorMapScene resources;EditorPlayScene play;play.setScriptRuntime(FakeRuntime::api(),"/project");AE_EXPECT_TRUE(play.start(doc,resources)&&started,"timer service exists before managed Start");
  const auto abi=FakeRuntime::sceneAccess;auto incomplete=abi;incomplete.timerCommand=nullptr;AE_EXPECT_TRUE(abi.version==scene::ScriptSceneAccess{}.version&&abi.available()&&!incomplete.available(),"mandatory ABI28 callback");
  AE_EXPECT_TRUE(play.advance(.2)&&FakeRuntime::timerExpirations==1,"command reaches native scheduler");scene::ScriptTimerState snapshot;
  AE_EXPECT_TRUE(abi.timerCommand(abi.context,id,instance,0,0,&snapshot)==1&&snapshot.remaining==0&&(snapshot.flags&4)&&!(snapshot.flags&1),"completion state precedes callback and remains queryable");
  snapshot.size=0;AE_EXPECT_TRUE(abi.timerCommand(abi.context,id,instance,0,0,&snapshot)==0,"snapshot layout refused");snapshot.size=sizeof(snapshot);play.stop();
  AE_EXPECT_TRUE(abi.timerCommand(abi.context,id,instance,0,0,&snapshot)==0&&abi.lastStatus(abi.context)==static_cast<u32>(runtime::WorldStatus::NotRunning),"ended session rejects retained callback safely");
}
AE_TEST(timer_controls_version_four_migrates_old_autostart_and_preserves_new_authoring) {
  for(u32 version=1;version<=3;++version){std::stringstream input;input<<"0.1 1 1";if(version>=2)input<<" 0";if(version>=3)input<<" 0 0";scene::Timer timer;AE_EXPECT_TRUE(timer.read(input,version)&&timer.autoStart,"legacy timers keep automatic behavior");}
  scene::Timer timer;timer.autoStart=false;std::stringstream payload;timer.write(payload);scene::Timer restored;AE_EXPECT_TRUE(restored.read(payload,4)&&!restored.autoStart,"explicit manual authoring survives v4");
}
AE_TEST(tween_controls_abi_start_pause_resume_cancel_and_after_stop_rejection) {
  FakeRuntime::reset();EditorDocument doc;const auto id=doc.createEntity(doc.root(),EditorEntityKind::Folder,"ABI tween");auto value=*doc.find(id);
  auto *tween=static_cast<scene::TransformTween*>(value.components.add(scene::TransformTween::descriptor));tween->autoplay=false;tween->duration=1;tween->destination[0]=4;const auto instance=tween->instanceId();doc.applyEntityValues(id,value);attachScript(doc,id,"project.Teste");
  const auto authored=serializeEditorDocument(doc,0);bool started=false;FakeRuntime::onStart=[&](){auto&abi=FakeRuntime::sceneAccess;scene::ScriptTweenState state;
    started=abi.tweenCommand(abi.context,id,instance,0,&state)==1&&state.status==0&&abi.tweenCommand(abi.context,id,instance,3,&state)==1&&(state.flags&1)&&abi.tweenCommand(abi.context,id,instance,1,&state)==1&&(state.flags&1);};
  EditorMapScene resources;EditorPlayScene play;play.setScriptRuntime(FakeRuntime::api(),"/project");AE_EXPECT_TRUE(play.start(doc,resources)&&started,"real evaluator available before script Start and restart preserves pause");
  const auto abi=FakeRuntime::sceneAccess;auto incomplete=abi;incomplete.tweenCommand=nullptr;AE_EXPECT_TRUE(abi.version==scene::ScriptSceneAccess{}.version&&abi.available()&&!incomplete.available(),"ABI29 requires tween consumer");
  AE_EXPECT_TRUE(play.advance(.25)&&play.document().find(id)->transform.position[0]==0,"local pause freezes pose");scene::ScriptTweenState state;
  AE_EXPECT_TRUE(abi.tweenCommand(abi.context,id,instance,4,&state)==1&&!(state.flags&1)&&play.advance(.25)&&std::abs(play.document().find(id)->transform.position[0]-1)<.001f,"resume drives actual transform");
  AE_EXPECT_TRUE(abi.tweenCommand(abi.context,id,instance,2,&state)==1&&state.status==4&&play.advance(.25)&&std::abs(play.document().find(id)->transform.position[0]-1)<.001f,"cancel preserves pose");
  state.size=0;AE_EXPECT_TRUE(abi.tweenCommand(abi.context,id,instance,0,&state)==0,"layout mismatch rejected");state.size=sizeof(state);state.reserved=1;AE_EXPECT_TRUE(abi.tweenCommand(abi.context,id,instance,0,&state)==0,"reserved flags rejected");state.reserved=0;
  AE_EXPECT_EQ(serializeEditorDocument(doc,0),authored,"commands never modify source");play.stop();AE_EXPECT_TRUE(abi.tweenCommand(abi.context,id,instance,0,&state)==0&&abi.lastStatus(abi.context)==static_cast<u32>(runtime::WorldStatus::NotRunning),"retained callback rejects closed session");
}
AE_TEST(number_tween_abi_start_controls_real_light_and_retained_callback_lifecycle) {
  FakeRuntime::reset();EditorDocument doc;const auto id=doc.createEntity(doc.root(),EditorEntityKind::Folder,"ABI numeric light");auto value=*doc.find(id);
  auto*light=static_cast<scene::Light*>(value.components.add(scene::Light::descriptor));light->intensity=0;const auto instance=light->instanceId();doc.applyEntityValues(id,value);attachScript(doc,id,"project.Teste");
  u64 track=0;bool started=false;const u8 property[]{'i','n','t','e','n','s','i','t','y'};
  FakeRuntime::onStart=[&](){auto&abi=FakeRuntime::sceneAccess;scene::ScriptNumberTweenParameters parameters;parameters.destination=8;parameters.duration=.1f;scene::ScriptNumberTweenState state;
    started=abi.numberTweenCreate(abi.context,id,instance,property,9,&parameters,&track)==1&&track!=0&&abi.numberTweenCommand(abi.context,track,1,&state)==1&&(state.flags&1);};
  EditorMapScene resources;EditorPlayScene play;play.setScriptRuntime(FakeRuntime::api(),"/project");AE_EXPECT_TRUE(play.start(doc,resources)&&started,"numeric scheduler exists before managed Start");
  const auto abi=FakeRuntime::sceneAccess;auto incomplete=abi;incomplete.numberTweenCreate=nullptr;AE_EXPECT_TRUE(abi.version==scene::ScriptSceneAccess{}.version&&abi.available()&&!incomplete.available(),"ABI30 requires actual creation callback");incomplete=abi;incomplete.numberTweenCommand=nullptr;AE_EXPECT_TRUE(!incomplete.available(),"control callback mandatory too");
  scene::ScriptNumberTweenParameters invalid;u64 rejected=0;invalid.size=0;AE_EXPECT_TRUE(abi.numberTweenCreate(abi.context,id,instance,property,9,&invalid,&rejected)==0,"wrong parameter layout rejected");invalid.size=sizeof(invalid);invalid.reserved=1;AE_EXPECT_TRUE(abi.numberTweenCreate(abi.context,id,instance,property,9,&invalid,&rejected)==0,"reserved rejected");invalid.reserved=0;invalid.flags=2;AE_EXPECT_TRUE(abi.numberTweenCreate(abi.context,id,instance,property,9,&invalid,&rejected)==0,"unknown flags rejected");
  scene::ScriptNumberTweenState state;AE_EXPECT_TRUE(play.advance(.2)&&abi.numberTweenCommand(abi.context,track,0,&state)==1&&state.value==0&&state.elapsed==0,"pause freezes real scheduler");
  AE_EXPECT_TRUE(abi.numberTweenCommand(abi.context,track,2,&state)==1&&play.advance(.2)&&abi.numberTweenCommand(abi.context,track,0,&state)==1&&state.status==1&&state.value==8,"resume reaches exact endpoint through Play");
  std::vector<renderer::SceneLight> lights;AE_EXPECT_TRUE(runtime::collectSceneLights(play.document(),lights)&&lights.size()==1&&lights[0].intensity>0&&light->intensity==0,"actual render extraction changes without mutating authoring");
  state.size=0;AE_EXPECT_TRUE(abi.numberTweenCommand(abi.context,track,0,&state)==0,"wrong output layout rejected");state.size=sizeof(state);
  AE_EXPECT_TRUE(abi.numberTweenCommand(abi.context,track,4,&state)==1&&abi.numberTweenCommand(abi.context,track,0,&state)==0,"release expires retained snapshot");play.stop();
  AE_EXPECT_TRUE(abi.numberTweenCommand(abi.context,track,0,&state)==0&&abi.lastStatus(abi.context)==static_cast<u32>(runtime::WorldStatus::NotRunning),"closed session callback rejected safely");
}
AE_TEST(tween_connection_version_three_clone_and_real_final_pose_activation) {
  EditorDocument doc;const auto emitter=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Tween connected");const auto receiver=doc.createEntity(emitter,EditorEntityKind::Folder,"Receiver");doc.setActive(receiver,false);auto lamp=*doc.find(receiver);lamp.components.add(scene::Light::descriptor);doc.applyEntityValues(receiver,lamp);
  auto value=*doc.find(emitter);auto *tween=static_cast<scene::TransformTween*>(value.components.add(scene::TransformTween::descriptor));tween->duration=.1f;tween->destination[0]=4;tween->finishedAction=1;tween->finishedTarget=receiver;const auto instance=tween->instanceId();doc.applyEntityValues(emitter,value);
  const auto authored=serializeEditorDocument(doc,0);EditorDocument loaded;AE_EXPECT_TRUE(deserializeEditorDocument(authored,0,loaded),"v3 scene roundtrip");runtime::ObjectCloneMap mapping;const auto clone=loaded.cloneSubtree(emitter,loaded.root(),mapping);
  const auto *copy=static_cast<const scene::TransformTween*>(loaded.find(clone)->components.findInstance(instance));AE_EXPECT_TRUE(copy&&copy->finishedTarget!=receiver&&loaded.find(copy->finishedTarget)->parent==clone,"clone remaps connection into cloned composition");
  runtime::GameWorld world;runtime::SceneTweens runtime;AE_EXPECT_TRUE(world.load(loaded)&&runtime.advance(world,.11),"real evaluator");
  AE_EXPECT_TRUE(world.activeSelf(world.handle(receiver))&&world.activeSelf(world.handle(static_cast<u32>(copy->finishedTarget)))&&world.graph().find(emitter)->transform.position[0]==4,"final pose and both actual receivers updated");
  std::vector<renderer::SceneLight> lights;AE_EXPECT_TRUE(runtime::collectSceneLights(world.graph(),lights)&&lights.size()==2,"both receiver activations reach actual light extraction");
  AE_EXPECT_TRUE(runtime.state(emitter,instance)->connectionInvoked&&runtime.state(emitter,instance)->connectionStatus==runtime::WorldStatus::Ok,"connection diagnostic records application");AE_EXPECT_EQ(serializeEditorDocument(doc,0),authored,"runtime does not change authoring");
  std::ostringstream payload;tween->write(payload);auto old=payload.str();old.resize(old.rfind(' '));old.resize(old.rfind(' '));
  for(u32 version=1;version<=2;++version){auto inputText=old;if(version==1)inputText.resize(inputText.rfind(' '));std::istringstream input(inputText);scene::TransformTween legacy;AE_EXPECT_TRUE(legacy.read(input,version)&&legacy.finishedAction==0&&legacy.finishedTarget==0,"v1/v2 explicitly migrate disconnected");}
}
AE_TEST(tween_connection_fires_once_restart_rearms_cancel_infinite_and_missing_receiver) {
  EditorDocument doc;const auto emitter=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Tween");const auto receiver=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Receiver");doc.setActive(receiver,false);
  auto value=*doc.find(emitter);auto*c=static_cast<scene::TransformTween*>(value.components.add(scene::TransformTween::descriptor));c->duration=.1f;c->finishedAction=3;c->finishedTarget=receiver;const auto instance=c->instanceId();doc.applyEntityValues(emitter,value);
  runtime::GameWorld world;runtime::SceneTweens runtime;AE_EXPECT_TRUE(world.load(doc)&&runtime.advance(world,.11)&&world.activeSelf(world.handle(receiver)),"finite completion toggles once");runtime.advance(world,.25);AE_EXPECT_TRUE(world.activeSelf(world.handle(receiver)),"completed state never emits again");
  runtime.restart(world,emitter,instance);runtime.advance(world,.11);AE_EXPECT_TRUE(!world.activeSelf(world.handle(receiver)),"explicit restart rearms one completion");runtime.restart(world,emitter,instance);runtime.cancel(world,emitter,instance);runtime.advance(world,.25);AE_EXPECT_TRUE(!world.activeSelf(world.handle(receiver)),"cancel never invokes completion");
  world.setProperty({world.handle(emitter),instance},"loops",u32{0});runtime.restart(world,emitter,instance);runtime.advance(world,.25);AE_EXPECT_TRUE(!world.activeSelf(world.handle(receiver))&&!runtime.state(emitter,instance)->connectionInvoked,"infinite never completes");
  world.setProperty({world.handle(emitter),instance},"loops",u32{1});runtime.restart(world,emitter,instance);AE_EXPECT_TRUE(world.destroyObject(world.handle(receiver))==runtime::WorldStatus::Ok,"receiver pending removal");world.flush();runtime.advance(world,.11);
  AE_EXPECT_TRUE(runtime.state(emitter,instance)->status==runtime::SceneTweens::Status::Completed&&runtime.state(emitter,instance)->connectionInvoked&&runtime.state(emitter,instance)->connectionStatus!=runtime::WorldStatus::Ok,"pose completion survives missing receiver with explicit diagnostic");
  runtime.reset();AE_EXPECT_TRUE(!runtime.state(emitter,instance),"Stop clears diagnostic and identity");
}

AE_TEST(character_state_abi_before_start_ground_jump_and_safe_after_stop) {
  FakeRuntime::reset();EditorDocument doc;physical(doc,"Floor",-.5f,scene::BodyMotion::Static,.5f);
  const auto actor=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Actor");auto value=*doc.find(actor);value.transform.position[1]=1;value.components.add(scene::Character::descriptor);doc.applyEntityValues(actor,value);attachScript(doc,actor,"project.Teste");
  bool observed=false;FakeRuntime::onStart=[&](){const auto&abi=FakeRuntime::sceneAccess;scene::ScriptCharacterState state;observed=abi.characterSnapshot(abi.context,actor,&state)==1&&!(state.flags&1);};
  EditorMapScene resources;EditorPlayScene play;play.setScriptRuntime(FakeRuntime::api(),"/project");AE_EXPECT_TRUE(play.start(doc,resources)&&observed,"native state service exists before script Start");
  const auto abi=FakeRuntime::sceneAccess;auto incomplete=abi;incomplete.characterSnapshot=nullptr;AE_EXPECT_TRUE(abi.version==scene::ScriptSceneAccess{}.version&&abi.available()&&!incomplete.available()&&sizeof(scene::ScriptCharacterState)==80,"mandatory ABI31 layout");
  for(int i=0;i<120;++i){AE_EXPECT_TRUE(play.advance(1./60),"settle");}
  scene::ScriptCharacterState state;AE_EXPECT_TRUE(abi.characterSnapshot(abi.context,actor,&state)==1&&state.groundState==0&&(state.flags&1)&&state.groundNormal[1]>.9f,"real grounded snapshot");
  state.size=0;AE_EXPECT_TRUE(!abi.characterSnapshot(abi.context,actor,&state),"reject layout");state={};state.tailReserved=1;AE_EXPECT_TRUE(!abi.characterSnapshot(abi.context,actor,&state),"reject reserved tail");state={};
  AE_EXPECT_TRUE(abi.characterJump(abi.context,actor)&&play.advance(1./60)&&abi.characterSnapshot(abi.context,actor,&state)&&state.velocity[1]>0&&state.groundState!=0,"accepted jump produces measurable native state");
  AE_EXPECT_TRUE(play.world().setActive(play.world().handle(actor),false)==runtime::WorldStatus::Ok&&!abi.characterSnapshot(abi.context,actor,&state)&&abi.lastStatus(abi.context)==static_cast<u32>(runtime::WorldStatus::ComponentUnavailable),"inactive object has no consumable state");
  play.stop();AE_EXPECT_TRUE(!abi.characterSnapshot(abi.context,actor,&state)&&abi.lastStatus(abi.context)==static_cast<u32>(runtime::WorldStatus::NotRunning),"retained callback safe after Stop");FakeRuntime::reset();
}

AE_TEST(character_platform_fixedupdate_kinematic_abi_and_native_state_follow_real_play) {
  FakeRuntime::reset();EditorDocument doc;const auto floor=physical(doc,"Piso",-.5f,scene::BodyMotion::Kinematic,.5f);auto base=*doc.find(floor);auto*collider=editCollider(base);collider->halfX=collider->halfZ=5;doc.applyEntityValues(floor,base);
  const auto actor=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Actor");auto value=*doc.find(actor);value.transform.position[1]=1;value.components.add(scene::Character::descriptor);doc.applyEntityValues(actor,value);attachScript(doc,actor,"project.Teste");
  EditorMapScene resources;EditorPlayScene play;play.setScriptRuntime(FakeRuntime::api(),"/project");AE_EXPECT_TRUE(play.start(doc,resources),"Play with real physics");for(int i=0;i<120;++i){AE_EXPECT_TRUE(play.advance(1./60),"settle");}
  float distance=0;FakeRuntime::onFixedUpdate=[&](){distance+=1.f/60;const float pose[7]{distance,-.5f,0,0,0,0,1};auto&abi=FakeRuntime::sceneAccess;AE_EXPECT_TRUE(abi.moveKinematic(abi.context,floor,pose),"actual kinematic ABI");};
  for(int i=0;i<120;++i){AE_EXPECT_TRUE(play.advance(1./60),"script, motor, solver and pose sync");}
  const auto&abi=FakeRuntime::sceneAccess;scene::ScriptCharacterState state;AE_EXPECT_TRUE(abi.characterSnapshot(abi.context,actor,&state)&&state.groundState==0&&state.position[0]>1.8f&&std::abs(state.groundVelocity[0]-1)<.02f,"native SDK snapshot reflects transported actor");
  AE_EXPECT_TRUE(std::abs(play.document().find(actor)->transform.position[0]-state.position[0])<.001f,"published scene pose matches native foot position");
  play.stop();FakeRuntime::reset();
}
