#include "harness.h"
#include "editor/editor_document.h"
#include "editor/editor_physics_body.h"
#include "editor/editor_play_scene.h"
#include "editor/editor_play_edit.h"
#include "editor/editor_archive.h"
#include "runtime/scene_environment.h"
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
  static std::function<void()> onUpdate;

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
  static int lateUpdate(float) { ++lateUpdates; return 0; }
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
  AE_EXPECT_TRUE(abi.version == 14 && abi.available(), "contrato ABI completo");
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
