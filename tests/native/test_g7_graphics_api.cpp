#include "harness.h"
#include "editor/editor_archive.h"
#include "editor/editor_document.h"
#include "editor/editor_map_scene.h"
#include "editor/editor_play_scene.h"
#include "renderer/time_of_day.h"
#include "renderer/volume_parameters.h"
#include "resources/environment_profile.h"
#include "runtime/component_operations.h"
#include "runtime/scene_event_connections.h"
#include "runtime/transform_math.h"
#include "scene/component_api_csharp.h"
#include "scene/environment.h"
#include "scene/event_connection.h"
#include "scene/light.h"
#include "scene/mesh_renderer.h"
#include "scene/player_input.h"
#include "scene/time_of_day.h"
#include "scene/timer.h"

#include <cmath>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

using namespace ae;
using namespace ae::editor;

// G7: a API gráfica universal pelo caminho real do Play — o relógio da Hora do
// dia, os eventos dele numa Conexão de evento, os métodos do Volume (cópia do
// perfil, caixas por parâmetro, peso) e a fachada C# gerada com nomes da Unity.
namespace {
EditorEntityId sun(EditorDocument &doc,float hour,float dayLength) {
  const auto id=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Sol");
  auto values=*doc.find(id);
  auto *light=static_cast<scene::Light*>(values.components.add(scene::Light::descriptor));
  auto *time=static_cast<scene::TimeOfDay*>(values.components.add(scene::TimeOfDay::descriptor));
  light->kind=scene::LightKind::Directional;
  time->hour=hour;time->dayLengthMinutes=dayLength;time->latitude=0;time->dayOfYear=80;
  return doc.applyEntityValues(id,values)?id:0;
}
EditorEntityId connection(EditorDocument &doc,EditorEntityId id,u32 event,u32 action,u32 method,u64 receiver,float argument=0) {
  auto values=*doc.find(id);
  auto *c=static_cast<scene::EventConnection*>(values.components.add(scene::EventConnection::descriptor));
  c->event=event;c->action=action;c->method=method;c->receiver=receiver;c->argument=argument;
  return doc.applyEntityValues(id,values)?id:0;
}
const scene::TimeOfDay *worldTime(EditorPlayScene &play,EditorEntityId id) {
  const auto *object=play.world().graph().find(id);
  return object?static_cast<const scene::TimeOfDay*>(object->components.find(scene::TimeOfDay::descriptor)):nullptr;
}
const scene::Environment *worldVolume(EditorPlayScene &play,EditorEntityId id) {
  const auto *object=play.world().graph().find(id);
  return object?static_cast<const scene::Environment*>(object->components.find(scene::Environment::descriptor)):nullptr;
}
} // namespace

AE_TEST(g7_time_of_day_clock_advances_the_world_hour_and_obeys_script_writes) {
  EditorDocument doc;
  // Dia de 1 min: 24 h em 60 s, 0,1 h por quadro de 0,25 s (o máximo do relógio).
  const auto id=sun(doc,17.0f,1.0f);
  const auto saved=serializeEditorDocument(doc,0);
  EditorMapScene resources;EditorPlayScene play;AE_EXPECT_TRUE(play.start(doc,resources),"Play");
  AE_EXPECT_TRUE(play.advance(.25),"quadro");
  AE_EXPECT_TRUE(std::abs(worldTime(play,id)->hour-17.1f)<1e-3f,"a hora do mundo avança pela duração do dia");
  // Como TimeOfDay.Hour = 6 num script: o relógio continua dali.
  const auto component=play.world().findComponent(play.world().handle(id),scene::TimeOfDay::descriptor.id);
  AE_EXPECT_TRUE(play.world().setProperty(component,"hour",scene::ComponentPropertyValue{6.0f})==runtime::WorldStatus::Ok,"escrita do script");
  AE_EXPECT_TRUE(play.advance(.25),"quadro");
  AE_EXPECT_TRUE(std::abs(worldTime(play,id)->hour-6.1f)<1e-3f,"a hora escrita vale no quadro seguinte");
  // O sol gira pela hora do mundo, sem relógio paralelo.
  float matrix[16];AE_EXPECT_TRUE(runtime::worldMatrix(play.world().graph(),id,matrix),"matriz");
  renderer::TimeOfDayInput input;input.hour=worldTime(play,id)->hour;input.latitude=0;input.dayOfYear=80;
  const auto state=renderer::evaluateTimeOfDay(input);
  float alignment=0;for(int a=0;a<3;++a) alignment+=matrix[8+a]*-state.toSun[a];
  AE_EXPECT_TRUE(alignment>.999f,"a luz aponta para longe do sol da hora atual");
  // Virada do dia: 23,95 h + 0,1 h = 0,05 h do dia 81.
  AE_EXPECT_TRUE(play.world().setProperty(component,"hour",scene::ComponentPropertyValue{23.95f})==runtime::WorldStatus::Ok,"23,95 h");
  AE_EXPECT_TRUE(play.advance(.25),"quadro");
  AE_EXPECT_TRUE(std::abs(worldTime(play,id)->hour-.05f)<1e-3f&&worldTime(play,id)->dayOfYear==81,"passa da meia-noite e avança o dia");
  play.stop();
  AE_EXPECT_EQ(serializeEditorDocument(doc,0),saved,"o Play não muda a hora autoral");
}

AE_TEST(g7_time_of_day_events_light_the_lamps_at_sunset_without_script) {
  EditorDocument doc;
  const auto id=sun(doc,17.5f,1.0f);
  // Postes começam apagados; o pôr do sol os acende, o nascer apaga.
  const auto lamps=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Postes");
  auto values=*doc.find(lamps);values.active=false;doc.applyEntityValues(lamps,values);
  connection(doc,id,41,1,0,lamps);
  connection(doc,id,40,2,0,lamps);
  EditorMapScene resources;EditorPlayScene play;AE_EXPECT_TRUE(play.start(doc,resources),"Play");
  auto &events=play.events();events.attach(runtime::ComponentEventQueue::Consumer::Scripts,true);
  bool lit=false,darkAgain=false;u32 hours=0,sunsets=0,sunrises=0,days=0;
  // 60 s de Play = um dia inteiro.
  for(u32 frame=0;frame<240;++frame) {
    AE_EXPECT_TRUE(play.advance(.25),"quadro");
    events.consume(runtime::ComponentEventQueue::Consumer::Scripts,[&](const runtime::ComponentEventRecord &record,u64) {
      if(record.type!=&scene::TimeOfDay::descriptor) return;
      const auto name=record.type->events[record.event].id;
      if(name=="hour_changed") ++hours;
      if(name=="sunset") ++sunsets;
      if(name=="sunrise") ++sunrises;
      if(name=="day_changed") {++days;AE_EXPECT_TRUE(record.values[0].integer==81,"payload do dia");}
    });
    const bool active=play.world().activeSelf(play.world().handle(lamps));
    if(active) lit=true;
    if(lit&&!active) darkAgain=true;
  }
  AE_EXPECT_EQ(hours,24u,"uma hora inteira por evento");
  AE_EXPECT_TRUE(sunsets==1&&sunrises==1&&days==1,"um pôr, um nascer e uma virada por dia");
  AE_EXPECT_TRUE(lit&&darkAgain,"a Conexão acende no pôr do sol e apaga no nascer");
  play.stop();
}

AE_TEST(g7_volume_methods_copy_the_shared_profile_and_edit_override_boxes) {
  EditorDocument doc;
  const auto id=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Volume");
  auto values=*doc.find(id);
  auto *volume=static_cast<scene::Environment*>(values.components.add(scene::Environment::descriptor));
  resources::EnvironmentProfile profile;
  profile.guid={0x1111,0x2222};profile.name="Cinema";
  profile.values=volume->values;profile.values.bloomIntensity=1.5f;
  profile.overrides=renderer::VolumeOverrides::empty();profile.overrides.addGroup(renderer::VolumeGroup::Bloom);
  const auto intensity=renderer::volumeParameterIndex("bloom_intensity");
  profile.overrides.setOverridden(intensity,true);
  volume->profile=profile.guid;
  doc.applyEntityValues(id,values);
  EditorMapScene resources;EditorPlayScene play;AE_EXPECT_TRUE(play.start(doc,resources),"Play");
  auto services=play.operationServices();
  const std::vector<resources::EnvironmentProfile> profiles{profile};
  services.environmentProfiles=&profiles;
  const auto handle=play.world().findComponent(play.world().handle(id),scene::Environment::descriptor.id);
  scene::ComponentOperationValue result;
  AE_EXPECT_TRUE(runtime::invokeComponentMethod(services,handle,"has_instantiated_profile",{},result)==runtime::WorldStatus::Ok&&
                 result.boolean==0,"com perfil compartilhado não há cópia própria");
  const auto index=scene::ComponentOperationValue::makeInteger(intensity);
  AE_EXPECT_TRUE(runtime::invokeComponentMethod(services,handle,"parameter_overridden",std::span(&index,1),result)==runtime::WorldStatus::Ok&&
                 result.boolean==1,"a caixa lida é a do perfil compartilhado");
  const auto copied=runtime::invokeComponentMethod(services,handle,"instantiate_profile",{},result);
  AE_EXPECT_TRUE(copied==runtime::WorldStatus::Ok,(std::string("Volume.profile: ")+runtime::worldStatusMessage(copied)).c_str());
  AE_EXPECT_TRUE(!worldVolume(play,id)->profile.valid()&&worldVolume(play,id)->values.bloomIntensity==1.5f&&
                 worldVolume(play,id)->overrideBloom,"a instância recebe valores e overrides do perfil");
  const std::array off{scene::ComponentOperationValue::makeInteger(intensity),scene::ComponentOperationValue::makeBoolean(false)};
  AE_EXPECT_TRUE(runtime::invokeComponentMethod(services,handle,"set_parameter_overridden",off,result)==runtime::WorldStatus::Ok&&
                 !worldVolume(play,id)->volumeOverrides().overridden(intensity),"overrideState desmarcado");
  const std::array inactive{scene::ComponentOperationValue::makeInteger(static_cast<i64>(renderer::VolumeGroup::Bloom)),
                            scene::ComponentOperationValue::makeBoolean(false)};
  AE_EXPECT_TRUE(runtime::invokeComponentMethod(services,handle,"set_component_active",inactive,result)==runtime::WorldStatus::Ok&&
                 !worldVolume(play,id)->volumeOverrides().groupActive(renderer::VolumeGroup::Bloom),"VolumeComponent.active");
  const auto weight=scene::ComponentOperationValue::makeNumber(.25);
  AE_EXPECT_TRUE(runtime::invokeComponentMethod(services,handle,"set_weight",std::span(&weight,1),result)==runtime::WorldStatus::Ok&&
                 worldVolume(play,id)->weight==.25f,"Volume.weight");
  const auto heavy=scene::ComponentOperationValue::makeNumber(2);
  AE_EXPECT_TRUE(runtime::invokeComponentMethod(services,handle,"set_weight",std::span(&heavy,1),result)==runtime::WorldStatus::InvalidArgument,
                 "peso fora de 0..1 recusado");
  const auto outside=scene::ComponentOperationValue::makeInteger(9999);
  AE_EXPECT_TRUE(runtime::invokeComponentMethod(services,handle,"parameter_overridden",std::span(&outside,1),result)==runtime::WorldStatus::InvalidArgument,
                 "índice fora da tabela recusado");
  // Sem biblioteca de perfis, trocar de perfil não tem como aplicar valores.
  AE_EXPECT_TRUE(runtime::invokeComponentMethod(play.operationServices(),handle,"set_shared_profile",
                 std::array{scene::ComponentOperationValue::makeInteger(1),scene::ComponentOperationValue::makeInteger(2)},result)!=runtime::WorldStatus::Ok,
                 "perfil desconhecido não é aceito");
  play.stop();
}

AE_TEST(g7_event_connection_v3_carries_the_asset_and_reads_v2) {
  scene::EventConnection authored;authored.event=1;authored.action=scene::kEventConnectionCallMethod;authored.method=39;
  authored.asset={0xabc,0xdef};
  std::ostringstream out;out.imbue(std::locale::classic());authored.write(out);
  scene::EventConnection reread;std::istringstream in(out.str());in.imbue(std::locale::classic());
  AE_EXPECT_TRUE(reread.read(in,3)&&reread.asset==authored.asset&&reread.method==39,"material da conexão atravessa a v3");
  std::istringstream old("1 1 4 6 0.5 0 0 0 -1");old.imbue(std::locale::classic());
  scene::EventConnection legacy;
  AE_EXPECT_TRUE(legacy.read(old,2)&&!legacy.asset.valid()&&legacy.method==6,"v2 lida sem recurso");
  const auto &type=scene::EventConnection::descriptor;
  AE_EXPECT_EQ(type.version,3u,"versão 3");
  AE_EXPECT_TRUE(type.resourceBindings[1].slotCount(authored)==1&&type.resourceBindings[0].slotCount(authored)==0,
                 "só o seletor de Material aparece para Malha: trocar material");
  authored.method=37;
  AE_EXPECT_TRUE(type.resourceBindings[0].slotCount(authored)==1&&type.resourceBindings[1].slotCount(authored)==0,
                 "só o seletor de Perfil aparece para Volume: trocar perfil");
  AE_EXPECT_TRUE(scene::eventConnectionTakesNumber(36)&&scene::eventConnectionTakesAsset(37)&&scene::eventConnectionTakesAsset(39),
                 "peso leva número; perfil e material levam recurso");
}

AE_TEST(g7_event_connection_sets_volume_weight_from_a_timer) {
  EditorDocument doc;
  const auto clock=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Relogio");auto values=*doc.find(clock);
  auto *timer=static_cast<scene::Timer*>(values.components.add(scene::Timer::descriptor));timer->intervalSeconds=.1f;timer->repeat=false;
  doc.applyEntityValues(clock,values);
  const auto id=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Volume DoF");values=*doc.find(id);
  values.components.add(scene::Environment::descriptor);doc.applyEntityValues(id,values);
  connection(doc,clock,1,scene::kEventConnectionCallMethod,36,id,.4f);
  EditorMapScene resources;EditorPlayScene play;AE_EXPECT_TRUE(play.start(doc,resources),"Play");
  for(u32 frame=0;frame<20;++frame) AE_EXPECT_TRUE(play.advance(.02),"quadro");
  AE_EXPECT_TRUE(std::abs(worldVolume(play,id)->weight-.4f)<1e-6f,"Timer → Volume: definir peso");
  AE_EXPECT_TRUE(play.eventConnections().diagnostic().empty(),play.eventConnections().diagnostic().c_str());
  play.stop();
}

AE_TEST(g7_generated_facade_is_partial_with_asset_methods_and_parameter_table) {
  const auto api=scene::componentCSharpApi();
  AE_EXPECT_TRUE(api.find("public readonly partial struct EnvironmentVolume : IComponentFacade<EnvironmentVolume>")!=std::string::npos,
                 "fachada parcial para os nomes da Unity");
  AE_EXPECT_TRUE(api.find("public void SetSharedProfile(AssetGuid asset) => Component.Invoke(\"set_shared_profile\"")!=std::string::npos,
                 "recurso em duas metades vira AssetGuid");
  AE_EXPECT_TRUE(api.find("public void SetSharedMaterial(AssetGuid asset)")!=std::string::npos,"Renderer.sharedMaterial pela fachada");
  AE_EXPECT_TRUE(api.find("public ComponentSubscription OnHourChanged(Behavior owner")!=std::string::npos&&
                 api.find("public ComponentSubscription OnSunset(Behavior owner")!=std::string::npos,"eventos da Hora do dia");
  AE_EXPECT_TRUE(api.find("public static class VolumeParameterTable")!=std::string::npos&&
                 api.find("\"override_lens_distortion\",")!=std::string::npos,"tabela de parâmetros e overrides");
  AE_EXPECT_TRUE(api.find("public bool ParameterOverridden(long parameter)")!=std::string::npos,"overrideState por índice");
  AE_EXPECT_TRUE(api.find("public long ProbeCount() => Component.Invoke(\"probe_count\").AsInteger();")!=std::string::npos,
                 "probePositions.Length do Light Probe Group");
}

AE_TEST(g7_player_input_turns_actions_into_events_filtered_by_action) {
  EditorDocument doc;
  // Mapa com duas ações de tecla; a Conexão só reage à segunda ("Mirar").
  auto map=doc.inputActions();
  const auto base=static_cast<u32>(map.actions().size());
  runtime::InputAction fire;fire.id="Disparar";fire.bindings={{runtime::InputSource::Key,62,0,0,1,false}};
  runtime::InputAction aim;aim.id="Mirar";aim.bindings={{runtime::InputSource::Key,29,0,0,1,false}};
  AE_EXPECT_TRUE(map.add(fire)&&map.add(aim)&&doc.setInputActions(map),"mapa");
  const auto player=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Jogador");
  const auto lamp=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Luz da mira");
  auto values=*doc.find(lamp);values.active=false;doc.applyEntityValues(lamp,values);
  values=*doc.find(player);
  values.components.add(scene::PlayerInput::descriptor);
  auto *pressed=static_cast<scene::EventConnection*>(values.components.add(scene::EventConnection::descriptor));
  pressed->event=43;pressed->action=1;pressed->receiver=lamp;pressed->inputAction=base+2;
  auto *released=static_cast<scene::EventConnection*>(values.components.add(scene::EventConnection::descriptor));
  released->event=44;released->action=2;released->receiver=lamp;released->inputAction=base+2;
  AE_EXPECT_TRUE(doc.applyEntityValues(player,values),"entrada e conexões");
  // Ida e volta: o filtro pela ação persiste na v3.
  EditorDocument reopened;AE_EXPECT_TRUE(deserializeEditorDocument(serializeEditorDocument(doc,0),0,reopened),"ida e volta");
  AE_EXPECT_TRUE(reopened.find(player)->components.find(scene::PlayerInput::descriptor)!=nullptr,"Entrada do jogador persiste");

  EditorMapScene resources;EditorPlayScene play;AE_EXPECT_TRUE(play.start(doc,resources),"Play");
  const auto frame=[&](std::vector<u32> keys) {
    runtime::InputDeviceState state;state.keys=std::move(keys);
    play.input().submit(state,1.0/60);
    return play.advance(1.0/60);
  };
  AE_EXPECT_TRUE(frame({62}),"Disparar");
  AE_EXPECT_TRUE(!play.world().activeSelf(play.world().handle(lamp)),"outra ação não passa no filtro");
  AE_EXPECT_TRUE(frame({62,29}),"Mirar");
  AE_EXPECT_TRUE(play.world().activeSelf(play.world().handle(lamp)),"ação executada acende");
  AE_EXPECT_TRUE(frame({62,29}),"segurando");
  AE_EXPECT_TRUE(play.world().activeSelf(play.world().handle(lamp)),"segurar não repete");
  AE_EXPECT_TRUE(frame({}),"soltar");
  AE_EXPECT_TRUE(!play.world().activeSelf(play.world().handle(lamp)),"ação cancelada apaga");
  play.stop();
  const auto api=scene::componentCSharpApi();
  AE_EXPECT_TRUE(api.find("public ComponentSubscription OnPerformed(Behavior owner")!=std::string::npos,"script assina ação executada");
}

// G7: cena gerada pelo g7_api_scene_author rodando no Play com física real
// (G7_SCENE aponta para scenes/editor.aescene; sem a variável o teste só passa).
AE_TEST(g7_generated_scene_plays_with_physics_and_connections) {
  const char *path=std::getenv("G7_SCENE");
  if(!path) return;
  EditorDocument doc;AE_EXPECT_TRUE(loadEditorDocument(path,0,doc),"cena gerada carrega");
  EditorMapScene resources;EditorPlayScene play;
  AE_EXPECT_TRUE(play.start(doc,resources),("Play: "+play.physicsError()+" | "+play.frameError()).c_str());
  for(u32 frame=0;frame<360;++frame) if(!play.advance(1.0/60)) {AE_EXPECT_TRUE(false,play.frameError().c_str());break;}
  AE_EXPECT_TRUE(play.eventConnections().deliveries()>0,play.eventConnections().diagnostic().c_str());
  play.stop();
}
