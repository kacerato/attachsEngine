#include "harness.h"
#include "editor/editor_document.h"
#include "editor/editor_physics_body.h"
#include "editor/editor_play_scene.h"
#include "editor/editor_archive.h"
#include "editor/editor_creation_catalog.h"
#include "editor/editor_session.h"
#include "runtime/scene_event_connections.h"
#include "scene/component_reflection.h"
#include "scene/event_connection.h"
#include "scene/audio.h"
#include "runtime/component_operations.h"
#include "scene/component_api_csharp.h"
#include "scene/script_behavior.h"
#include "scene/script_extensions.h"
#include "scene/timer.h"
#include "scene/transform_tween.h"

#include <functional>
#include <string>

using namespace ae;
using namespace ae::editor;

// Métodos e eventos de componente pelo caminho real do Play: EditorPlayScene ->
// ScriptBridge -> família `astra.component.operations` da ABI42 -> serviços
// de timer/tween/física. O duplo do runtime gerenciado só registra o que a ABI
// entrega; nenhum serviço nativo é simulado.
namespace {
struct OperationsRuntime {
  static scene::ScriptSceneAccess access;
  static std::function<void()> onStart;
  static int start(const u8 *,int,const u8 *,int,const scene::ScriptSceneAccess *value) {
    access=*value;if(onStart)onStart();return value->available()?0:1;
  }
  static scene::ScriptRuntimeApi api() {
    scene::ScriptRuntimeApi a{};
    a.start=&start;a.update=[](float){return 0;};a.fixedUpdate=a.update;a.lateUpdate=a.update;a.stop=[]{};
    a.copyDiagnostics=[](u8*,int){return 0;};a.trigger=[](u64,u64,u32){return 0;};
    a.contact=[](u64,u64,u32,const float*){return 0;};a.timer=[](u64,u64,u32){return 0;};a.lifecycle=[](u32,u32){return 0;};
    return a;
  }
  static const scene::ScriptComponentOperations *operations(u32 *version=nullptr,u32 *size=nullptr) {
    const std::string_view name=scene::kScriptComponentOperations;
    return static_cast<const scene::ScriptComponentOperations*>(
      access.extension(access.context,reinterpret_cast<const u8*>(name.data()),static_cast<int>(name.size()),version,size));
  }
  static int invoke(u64 object,u64 instance,std::string_view method,std::span<const scene::ComponentOperationValue> args,scene::ComponentOperationValue &result) {
    const auto *ops=operations();
    const auto handle=access.generation(access.context,object);
    return ops->invoke(access.context,object,access.worldId(access.context),handle,instance,
                       reinterpret_cast<const u8*>(method.data()),static_cast<int>(method.size()),args.data(),static_cast<int>(args.size()),&result);
  }
  static std::string eventName(const scene::ScriptComponentEvent &event) {
    char buffer[128]{};const int length=operations()->eventName(access.context,event.type,event.event,reinterpret_cast<u8*>(buffer),sizeof(buffer));
    return length>0?std::string(buffer,static_cast<usize>(length)):std::string();
  }
};
scene::ScriptSceneAccess OperationsRuntime::access{};
std::function<void()> OperationsRuntime::onStart{};

void attach(EditorDocument &doc,EditorEntityId id) {
  auto values=*doc.find(id);
  auto *script=static_cast<scene::ScriptBehavior*>(values.components.add(scene::ScriptBehavior::descriptor));
  script->scriptType="project.Probe";script->source="Probe.cs";doc.applyEntityValues(id,values);
}
EditorEntityId body(EditorDocument &doc,const char *name,float y,scene::BodyMotion motion,bool sensor) {
  const auto id=doc.createEntity(doc.root(),EditorEntityKind::Folder,name);auto values=*doc.find(id);
  values.transform.position[1]=y;auto *b=editPhysicsBody(values);b->motion=motion;b->sensor=sensor;
  auto *c=editCollider(values);c->halfX=c->halfY=c->halfZ=.5f;
  return doc.applyEntityValues(id,values)?id:0;
}
u32 lastStatus() {return OperationsRuntime::access.lastStatus(OperationsRuntime::access.context);}
} // namespace

AE_TEST(component_operations_every_declared_method_has_one_runtime_function) {
  const auto issues=runtime::auditComponentOperations();
  std::string joined;for(const auto &issue:issues) joined+=issue+"; ";
  AE_EXPECT_TRUE(issues.empty(),(std::string("method and event contracts audited: ")+joined).c_str());
  AE_EXPECT_EQ(runtime::componentMethodBindings().size(),usize{25},"timer 6 + tween 5 + sequence 5 + audio 5 + path follow 4");
  AE_EXPECT_TRUE(scene::findComponentEvent(scene::Timer::descriptor,"elapsed")&&
                 scene::findComponentEvent(scene::Collider::descriptor,"trigger_enter")&&
                 scene::findComponentEvent(scene::Collider2D::descriptor,"collision_exit")&&
                 scene::findComponentEvent(scene::TransformTween::descriptor,"completed"),"declared events are discoverable by id");
  const auto api=scene::componentCSharpApi();
  AE_EXPECT_TRUE(api.find("public void Start(double interval) => Component.Invoke(\"start\", ComponentValue.Number(interval));")!=std::string::npos,
                 "generated facade wraps typed method arguments");
  AE_EXPECT_TRUE(api.find("public double Remaining() => Component.Invoke(\"remaining\").AsNumber();")!=std::string::npos,"generated facade unwraps result");
  AE_EXPECT_TRUE(api.find("public ComponentSubscription OnTriggerEnter(Behavior owner, Action<ComponentEventArgs> handler)")!=std::string::npos,
                 "generated facade exposes event subscription");
}

AE_TEST(component_operations_abi42_family_invokes_real_timer_and_delivers_elapsed_event) {
  EditorDocument doc;const auto id=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Timer");auto value=*doc.find(id);
  auto *timer=static_cast<scene::Timer*>(value.components.add(scene::Timer::descriptor));
  timer->autoStart=false;timer->repeat=false;timer->intervalSeconds=.5f;const auto instance=timer->instanceId();
  doc.applyEntityValues(id,value);attach(doc,id);
  bool resolved=false;u32 version=0,size=0;
  OperationsRuntime::onStart=[&]{
    resolved=OperationsRuntime::operations(&version,&size)!=nullptr;
    const std::string_view unknown="astra.unknown.family";
    resolved=resolved&&!OperationsRuntime::access.extension(OperationsRuntime::access.context,reinterpret_cast<const u8*>(unknown.data()),
                                                            static_cast<int>(unknown.size()),nullptr,nullptr);
  };
  EditorMapScene resources;EditorPlayScene play;play.setScriptRuntime(OperationsRuntime::api(),"/project");
  AE_EXPECT_TRUE(play.start(doc,resources)&&resolved,"family resolved by name; unknown family is null, not a failed session");
  AE_EXPECT_TRUE(version==1&&size==sizeof(scene::ScriptComponentOperations),"family carries its own version and size");
  AE_EXPECT_TRUE(OperationsRuntime::access.version==45&&OperationsRuntime::access.available(),"core frozen at v45 with extension resolver");
  auto incomplete=OperationsRuntime::access;incomplete.extension=nullptr;AE_EXPECT_TRUE(!incomplete.available(),"resolver is part of the core");

  scene::ComponentOperationValue result;
  const auto interval=scene::ComponentOperationValue::makeNumber(.1);
  AE_EXPECT_EQ(OperationsRuntime::invoke(id,instance,"start",std::span(&interval,1),result),1,"start through declared method");
  AE_EXPECT_TRUE(OperationsRuntime::invoke(id,instance,"running",{},result)==1&&result.valueKind()==scene::ComponentValueKind::Boolean&&result.boolean==1,
                 "result typed by descriptor");
  AE_EXPECT_TRUE(OperationsRuntime::invoke(id,instance,"remaining",{},result)==1&&std::abs(result.number-.1)<1e-6,"runtime interval reaches scheduler");
  AE_EXPECT_TRUE(play.timers().state(id,instance)->interval==.1f,"same scheduler state as the timer ABI");
  const auto wrong=scene::ComponentOperationValue::makeBoolean(true);
  AE_EXPECT_TRUE(OperationsRuntime::invoke(id,instance,"start",std::span(&wrong,1),result)==0&&
                 lastStatus()==static_cast<u32>(runtime::WorldStatus::InvalidArgument),"argument kind checked before the function");
  AE_EXPECT_TRUE(OperationsRuntime::invoke(id,instance,"play",{},result)==0&&
                 lastStatus()==static_cast<u32>(runtime::WorldStatus::UnknownOperation),"method of another type is refused by name");
  AE_EXPECT_TRUE(OperationsRuntime::invoke(id,instance+99,"stop",{},result)==0&&
                 lastStatus()==static_cast<u32>(runtime::WorldStatus::ComponentMissing),"missing instance refused");

  AE_EXPECT_TRUE(play.advance(.2),"frame fires the timer");
  scene::ScriptComponentEvent events[8]{};const auto *ops=OperationsRuntime::operations();
  const int count=ops->pollEvents(OperationsRuntime::access.context,events,8);
  AE_EXPECT_EQ(count,1,"one aggregated elapsed event");
  AE_EXPECT_TRUE(count==1&&OperationsRuntime::eventName(events[0])=="astra.time.timer/elapsed"&&events[0].object==id&&events[0].instance==instance&&
                 events[0].count==1&&events[0].values[0].valueKind()==scene::ComponentValueKind::Integer&&events[0].values[0].integer==1,"payload and identity");
  AE_EXPECT_EQ(ops->pollEvents(OperationsRuntime::access.context,events,8),0,"cursor advanced; no duplicate delivery");
  const std::string_view declared="astra.time.timer/elapsed",undeclared="astra.time.timer/finished";
  AE_EXPECT_TRUE(ops->declaresEvent(OperationsRuntime::access.context,reinterpret_cast<const u8*>(declared.data()),static_cast<int>(declared.size()))==1&&
                 ops->declaresEvent(OperationsRuntime::access.context,reinterpret_cast<const u8*>(undeclared.data()),static_cast<int>(undeclared.size()))==0,
                 "subscriptions validated against descriptors");
  play.stop();
  AE_EXPECT_TRUE(ops->invoke(OperationsRuntime::access.context,id,0,0,instance,reinterpret_cast<const u8*>("stop"),4,nullptr,0,&result)==0&&
                 lastStatus()==static_cast<u32>(runtime::WorldStatus::NotRunning),"retained table refuses after Stop");
  OperationsRuntime::onStart={};
}

AE_TEST(component_operations_tween_completion_and_jolt_contacts_reach_the_event_queue) {
  EditorDocument doc;
  const auto sensor=body(doc,"Sensor",0,scene::BodyMotion::Static,true);
  const auto falling=body(doc,"Corpo",3,scene::BodyMotion::Dynamic,false);
  const auto mover=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Tween");auto value=*doc.find(mover);
  auto *tween=static_cast<scene::TransformTween*>(value.components.add(scene::TransformTween::descriptor));
  tween->duration=.1f;tween->loops=1;tween->position=true;tween->destination[0]=2;const auto tweenInstance=tween->instanceId();
  doc.applyEntityValues(mover,value);
  EditorMapScene resources;EditorPlayScene play;AE_EXPECT_TRUE(play.start(doc,resources),"Play without scripts");
  // Sem scripts na cena, o cursor Scripts está livre para o teste observar a
  // fila; o cursor Connections pertence às Conexões de evento do próprio Play.
  play.events().attach(runtime::ComponentEventQueue::Consumer::Scripts,true);
  u32 completed=0,enter=0;runtime::ObjectId other=0;
  for(u32 frame=0;frame<150;++frame) {
    AE_EXPECT_TRUE(play.advance(.02f),"frame");
    play.events().consume(runtime::ComponentEventQueue::Consumer::Scripts,[&](const runtime::ComponentEventRecord &record,u64) {
      const auto &event=record.type->events[record.event];
      if(record.type==&scene::TransformTween::descriptor&&event.id=="completed"&&record.object.id==mover&&record.instance==tweenInstance) ++completed;
      if(record.type==&scene::Collider::descriptor&&event.id=="trigger_enter"&&record.object.id==sensor) {++enter;other=static_cast<runtime::ObjectId>(record.values[0].object);}
    });
  }
  AE_EXPECT_EQ(completed,1u,"finite tween emits completed exactly once");
  AE_EXPECT_TRUE(enter>=1&&other==falling,"Jolt sensor enter carries the other object");
  play.stop();
  AE_EXPECT_TRUE(!play.events().attached(runtime::ComponentEventQueue::Consumer::Scripts)&&play.events().pending(runtime::ComponentEventQueue::Consumer::Scripts)==0,
                 "Stop discards the queue and its consumers");
}

AE_TEST(component_event_queue_bounds_reports_loss_and_holds_nothing_without_consumers) {
  EditorDocument doc;const auto id=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Emissor");auto value=*doc.find(id);
  const auto instance=value.components.add(scene::Timer::descriptor)->instanceId();doc.applyEntityValues(id,value);
  runtime::GameWorld world;AE_EXPECT_TRUE(world.load(doc),"world");runtime::ComponentEventQueue queue;
  const auto one=scene::ComponentOperationValue::makeInteger(1);
  AE_EXPECT_TRUE(queue.emit(world,id,instance,scene::Timer::descriptor,"elapsed",std::span(&one,1))&&queue.pending(runtime::ComponentEventQueue::Consumer::Scripts)==0,
                 "without consumers nothing accumulates");
  queue.attach(runtime::ComponentEventQueue::Consumer::Scripts,true);
  AE_EXPECT_TRUE(!queue.emit(world,id,instance,scene::Timer::descriptor,"finished",std::span(&one,1)),"undeclared event refused");
  const auto wrong=scene::ComponentOperationValue::makeNumber(1);
  AE_EXPECT_TRUE(!queue.emit(world,id,instance,scene::Timer::descriptor,"elapsed",std::span(&wrong,1))&&
                 !queue.emit(world,id,instance,scene::Timer::descriptor,"elapsed"),"payload must match the descriptor");
  for(usize i=0;i<runtime::ComponentEventQueue::Capacity+5;++i) queue.emit(world,id,instance,scene::Timer::descriptor,"elapsed",std::span(&one,1));
  usize delivered=0;u64 firstLost=0;
  const auto lost=queue.consume(runtime::ComponentEventQueue::Consumer::Scripts,[&](const runtime::ComponentEventRecord &,u64 l){if(!delivered)firstLost=l;++delivered;});
  AE_EXPECT_TRUE(lost==5&&firstLost==5&&delivered==runtime::ComponentEventQueue::Capacity&&queue.dropped()==5,"overflow is reported, never silent");
  queue.attach(runtime::ComponentEventQueue::Consumer::Connections,true);
  queue.emit(world,id,instance,scene::Timer::descriptor,"elapsed",std::span(&one,1));
  delivered=0;queue.consume(runtime::ComponentEventQueue::Consumer::Scripts,[&](const runtime::ComponentEventRecord &,u64){++delivered;},0);
  AE_EXPECT_TRUE(delivered==0&&queue.pending(runtime::ComponentEventQueue::Consumer::Scripts)==1,"limit zero delivers nothing and keeps the cursor");
  AE_EXPECT_TRUE(world.destroyObject(world.handle(id))==runtime::WorldStatus::Ok,"owner removal queued");world.flush(nullptr);
  AE_EXPECT_TRUE(!queue.emit(world,id,instance,scene::Timer::descriptor,"elapsed",std::span(&one,1)),"dead emitter refused");
}

// --- Bloco B: Conexão de evento -------------------------------------------
namespace {
EditorEntityId connectionOn(EditorDocument &doc,EditorEntityId id,u32 event,u32 action,u32 method,u64 receiver,float argument=0,bool once=false,u64 filter=0) {
  auto values=*doc.find(id);
  auto *c=static_cast<scene::EventConnection*>(values.components.add(scene::EventConnection::descriptor));
  c->event=event;c->action=action;c->method=method;c->receiver=receiver;c->argument=argument;c->once=once;c->otherFilter=filter;
  return doc.applyEntityValues(id,values)?id:0;
}
} // namespace

AE_TEST(event_connection_catalog_covers_every_declared_event_and_command) {
  const auto issues=runtime::auditEventConnectionCatalog();
  std::string joined;for(const auto &issue:issues) joined+=issue+"; ";
  AE_EXPECT_TRUE(issues.empty(),(std::string("connection catalog: ")+joined).c_str());
  AE_EXPECT_TRUE(scene::auditComponentContracts().empty(),"new schema declares consumer and capability");
  AE_EXPECT_TRUE(scene::findComponentSchema("astra.logic.event_connection")!=nullptr,"registered in the logic family");
}

AE_TEST(event_connection_persists_validates_and_clones_with_remapped_receiver) {
  EditorDocument doc;const auto emitter=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Emissor");
  const auto receiver=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Receptor");
  AE_EXPECT_TRUE(connectionOn(doc,emitter,3,scene::kEventConnectionCallMethod,6,receiver,.25f,true,receiver)!=0,"authored");
  const auto archive=serializeEditorDocument(doc,0);EditorDocument reopened;
  AE_EXPECT_TRUE(deserializeEditorDocument(archive,0,reopened),"roundtrip");
  const auto *c=static_cast<const scene::EventConnection*>(reopened.find(emitter)->components.find(scene::EventConnection::descriptor));
  AE_EXPECT_TRUE(c&&c->event==3&&c->action==4&&c->method==6&&c->receiver==receiver&&c->argument==.25f&&c->once&&c->otherFilter==receiver,"every field persisted");
  scene::EventConnection bad;bad.method=99;AE_EXPECT_TRUE(!bad.valid(),"unknown method identity refused");
  bad.method=0;bad.event=42;AE_EXPECT_TRUE(!bad.valid(),"unknown event identity refused");
  bad.event=0;bad.argument=-1;AE_EXPECT_TRUE(!bad.valid(),"argument domain enforced");
  // Visibilidade condicional dirigida pelos mesmos predicados da validação.
  const auto &type=scene::EventConnection::descriptor;scene::EventConnection draft;
  AE_EXPECT_TRUE(!type.enums[2].presentation.isVisible(draft)&&!type.references[1].presentation.isVisible(draft),"method and filter hidden until relevant");
  draft.action=4;draft.method=6;draft.event=3;
  AE_EXPECT_TRUE(type.enums[2].presentation.isVisible(draft)&&type.numbers[0].presentation.isVisible(draft)&&type.references[1].presentation.isVisible(draft),
                 "method, numeric argument and contact filter visible once they have a consumer");
  draft.method=1;AE_EXPECT_TRUE(!type.numbers[0].presentation.isVisible(draft),"Play needs no argument");
  runtime::ObjectCloneMap mapping;const auto copy=reopened.cloneSubtree(emitter,reopened.root(),mapping);
  const auto *cloned=static_cast<const scene::EventConnection*>(reopened.find(copy)->components.find(scene::EventConnection::descriptor));
  AE_EXPECT_TRUE(cloned&&cloned->receiver==receiver,"external receiver preserved on clone");
}

AE_TEST(event_connection_timer_and_jolt_sensor_drive_activation_and_declared_methods) {
  EditorDocument doc;
  // Timer -> ativa um objeto inativo.
  const auto clock=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Relogio");auto value=*doc.find(clock);
  auto *timer=static_cast<scene::Timer*>(value.components.add(scene::Timer::descriptor));timer->intervalSeconds=.1f;timer->repeat=false;doc.applyEntityValues(clock,value);
  const auto lamp=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Lampada");value=*doc.find(lamp);value.active=false;doc.applyEntityValues(lamp,value);
  connectionOn(doc,clock,1,1,0,lamp);
  // Sensor Jolt -> inicia (uma vez) o timer de outro objeto com intervalo de Play.
  const auto sensor=body(doc,"Sensor",0,scene::BodyMotion::Static,true);
  body(doc,"Corpo",3,scene::BodyMotion::Dynamic,false);
  const auto target=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Alvo");value=*doc.find(target);
  auto *remote=static_cast<scene::Timer*>(value.components.add(scene::Timer::descriptor));remote->autoStart=false;remote->repeat=false;
  const auto remoteInstance=remote->instanceId();doc.applyEntityValues(target,value);
  connectionOn(doc,sensor,3,scene::kEventConnectionCallMethod,6,target,.5f,true);
  // Filtro por outro objeto que nunca entra: não pode disparar.
  const auto never=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Nunca");
  value=*doc.find(never);value.active=false;doc.applyEntityValues(never,value);
  connectionOn(doc,sensor,3,1,0,never,0,false,lamp);
  // Receptor sem o componente do método: diagnóstico, não sucesso.
  const auto empty=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Sem audio");
  connectionOn(doc,sensor,3,scene::kEventConnectionCallMethod,1,empty);
  const auto saved=serializeEditorDocument(doc,0);

  EditorMapScene resources;EditorPlayScene play;AE_EXPECT_TRUE(play.start(doc,resources),"Play without scripts");
  bool lit=false,started=false;std::string diagnostic;
  for(u32 frame=0;frame<150;++frame) {
    AE_EXPECT_TRUE(play.advance(.02f),"frame");
    lit=lit||play.world().activeSelf(play.world().handle(lamp));
    if(const auto *state=play.timers().state(target,remoteInstance);state&&state->running) started=true;
    if(!play.eventConnections().diagnostic().empty()) diagnostic=play.eventConnections().diagnostic();
  }
  AE_EXPECT_TRUE(lit,"timer elapsed activates the receiver without script");
  AE_EXPECT_TRUE(started&&play.timers().state(target,remoteInstance)->interval==.5f,"sensor enter calls Timer.start with the authored argument");
  AE_EXPECT_TRUE(!play.world().activeSelf(play.world().handle(never)),"other-object filter rejects unmatched contact");
  AE_EXPECT_TRUE(play.eventConnections().deliveries()>=2,"deliveries counted");
  AE_EXPECT_TRUE(diagnostic.find(runtime::worldStatusMessage(runtime::WorldStatus::ComponentMissing))!=std::string::npos,"missing component reported, not silent");
  play.stop();
  AE_EXPECT_EQ(serializeEditorDocument(doc,0),saved,"Play never mutates authoring");
  AE_EXPECT_TRUE(play.eventConnections().deliveries()==0,"Stop resets connection runtime");
}

AE_TEST(event_connection_once_fires_a_single_time_per_play_session) {
  EditorDocument doc;const auto clock=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Relogio");auto value=*doc.find(clock);
  auto *timer=static_cast<scene::Timer*>(value.components.add(scene::Timer::descriptor));timer->intervalSeconds=.1f;timer->repeat=true;doc.applyEntityValues(clock,value);
  const auto lamp=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Lampada");
  connectionOn(doc,clock,1,3,0,lamp,0,true);
  EditorMapScene resources;EditorPlayScene play;
  for(u32 session=0;session<2;++session) {
    AE_EXPECT_TRUE(play.start(doc,resources),"Play");
    for(u32 frame=0;frame<30;++frame) AE_EXPECT_TRUE(play.advance(.05f),"repeating timer fires many times");
    AE_EXPECT_TRUE(!play.world().activeSelf(play.world().handle(lamp))&&play.eventConnections().deliveries()==1,"toggle happened exactly once");
    play.stop();
  }
}

AE_TEST(event_connection_recipes_compose_real_types_in_one_undo_step) {
  EditorSession session;auto &doc=session.document();
  u32 recipe=0;AE_EXPECT_TRUE(findCreationRecipe("gameplay.sound_trigger",&recipe),"sound trigger recipe");
  session.history().clear();const auto id=session.createRecipe(recipe,doc.root());AE_EXPECT_TRUE(id!=0,"created");
  const auto *entity=doc.find(id);
  const auto *c=entity?static_cast<const scene::EventConnection*>(entity->components.find(scene::EventConnection::descriptor)):nullptr;
  AE_EXPECT_TRUE(c&&c->event==3&&c->action==4&&c->method==1&&c->receiver==0,"connection plays the own sound");
  const auto *physics=entity?static_cast<const scene::PhysicsBody*>(entity->components.find(scene::PhysicsBody::descriptor)):nullptr;
  AE_EXPECT_TRUE(entity&&entity->components.find(scene::AudioSource::descriptor)&&physics&&physics->sensor,"audio and sensor body composed");
  AE_EXPECT_TRUE(session.history().undo(doc)&&!doc.exists(id),"one undo removes the composition");
  u32 plain=0;AE_EXPECT_TRUE(findCreationRecipe("gameplay.event_connection",&plain),"plain connection recipe");
}
