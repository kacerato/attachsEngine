#include "harness.h"
#include "editor/editor_document.h"
#include "editor/editor_physics_body.h"
#include "runtime/scene_physics.h"
#include "editor/editor_archive.h"
#include "editor/editor_session.h"
#include "editor/editor_creation_catalog.h"
#include "editor/editor_component_visuals.h"
#include "scene/constant_force.h"

#include <cmath>
#include <sstream>
#include "editor/editor_history.h"

using namespace ae;
using namespace ae::editor;
using ae::runtime::ContactEvent;
using ae::runtime::GameplayLayers;
using ae::runtime::GameWorld;
using ae::runtime::QueryFilter;
using ae::runtime::QueryHit;
using ae::runtime::QueryShapeDesc;
using ae::runtime::QueryShapeKind;
using ae::runtime::ScenePhysics;

namespace {
struct Fixture {
  GameWorld world;
  ScenePhysics physics;
  bool start(const EditorDocument &doc) { return world.load(doc) && physics.start(world); }
};

// Um corpo com um colisor; devolve o id do objeto autorado.
EditorEntityId solid(EditorDocument &doc, const char *name, float x, float y, float z,
                     scene::BodyMotion motion, float half = .5f, u32 layer = 0) {
  const auto id = doc.createEntity(doc.root(), EditorEntityKind::Folder, name);
  auto values = *doc.find(id);
  values.transform.position[0] = x;
  values.transform.position[1] = y;
  values.transform.position[2] = z;
  values.layer = layer;
  editPhysicsBody(values)->motion = motion;
  editPhysicsBody(values)->gravityFactor = 0;
  auto *collider = editCollider(values);
  collider->halfX = collider->halfY = collider->halfZ = half;
  return doc.applyEntityValues(id, values) ? id : 0;
}

// Piso estático mais uma bola dinâmica na camada 1. `separate` desliga a
// interação entre as camadas 0 e 1 no projeto. Devolve 0 se algo for recusado.
EditorEntityId fallingBallOverFloor(EditorDocument &doc, bool separate) {
  auto layers = doc.layers();
  if (!layers.setName(1, "Projétil")) return 0;
  if (separate && !layers.setInteraction(0, 1, false)) return 0;
  doc.setLayers(layers);
  const auto floorId = solid(doc, "Piso", 0, -1, 0, scene::BodyMotion::Static, 4);
  auto values = *doc.find(floorId);
  editCollider(values)->halfY = .5f;
  if (!doc.applyEntityValues(floorId, values)) return 0;
  const auto ball = solid(doc, "Bola", 0, 1, 0, scene::BodyMotion::Dynamic, .5f, 1);
  values = *doc.find(ball);
  editPhysicsBody(values)->gravityFactor = 1;
  return doc.applyEntityValues(ball, values) ? ball : 0;
}
} // namespace

AE_TEST(character_ground_version_three_archive_live_motion_and_velocity_preservation) {
  EditorDocument doc;const auto id=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Character");auto value=*doc.find(id);value.transform.position[1]=5;
  auto*character=static_cast<scene::Character*>(value.components.add(scene::Character::descriptor));character->gravity=0;character->stepHeight=.2f;character->floorSnapLength=.3f;const auto instance=character->instanceId();doc.applyEntityValues(id,value);
  const auto source=serializeEditorDocument(doc,0);EditorDocument reopened;AE_EXPECT_TRUE(deserializeEditorDocument(source,0,reopened)&&serializeEditorDocument(reopened,0)==source,"all new authoring fields roundtrip");
  for(u32 version:{1u,2u}){std::istringstream legacy(version==1?".45 .55 1.65 8 45":".45 .55 1.65 8 45 5");scene::Character old;AE_EXPECT_TRUE(old.read(legacy,version)&&old.stepHeight==.4f&&old.floorSnapLength==.5f&&old.gravity==9.81f,"legacy retains actual former Jolt and scene-gravity defaults");}
  {std::istringstream legacy(".01 .01 .02 8 45");scene::Character tiny;AE_EXPECT_TRUE(tiny.read(legacy,1)&&tiny.valid()&&tiny.stepHeight==.4f,"legacy small capsules retain previous Jolt step configuration");}
  Fixture fx;AE_EXPECT_TRUE(fx.start(reopened),fx.physics.error().c_str());AE_EXPECT_TRUE(fx.physics.advance(.25,fx.world)&&std::abs(fx.world.graph().find(id)->transform.position[1]-5)<.001f,"zero gravity holds initially stationary airborne capsule");fx.world.consumeInvalidation();
  const runtime::ComponentHandle handle{fx.world.handle(id),instance};AE_EXPECT_TRUE(fx.world.setProperty(handle,"gravity",24.f)==runtime::WorldStatus::Ok&&fx.world.setProperty(handle,"step_height",.4f)==runtime::WorldStatus::Ok&&fx.world.setProperty(handle,"floor_snap_length",0.f)==runtime::WorldStatus::Ok,"runtime controls accepted");
  AE_EXPECT_TRUE(!(fx.world.pendingInvalidation()&(scene::Invalidate::PhysicsBody|scene::Invalidate::PhysicsShape)),"locomotion does not request world or capsule rebuild");AE_EXPECT_TRUE(fx.physics.advance(.25,fx.world),"live gravity consumed by existing motor");const auto before=fx.world.graph().find(id)->transform.position[1];AE_EXPECT_TRUE(before<4.5f,"gravity changes actual physical pose");
  AE_EXPECT_TRUE(fx.world.setProperty(handle,"gravity",0.f)==runtime::WorldStatus::Ok&&fx.physics.advance(.25,fx.world)&&fx.world.graph().find(id)->transform.position[1]<before-1,"gravity edit preserves acquired downward velocity");
  AE_EXPECT_TRUE(fx.world.setProperty(handle,"step_height",11.f)==runtime::WorldStatus::Rejected,"step cannot exceed bounded query distance");AE_EXPECT_EQ(serializeEditorDocument(doc,0),source,"runtime settings preserve authored document");
  const auto visuals=collectComponentVisuals(reopened,id,1);AE_EXPECT_TRUE(visuals.size()==1&&visuals[0].icon==ui::UiIcon::PhysicsCharacterGround&&visuals[0].segments.size()>190,"actual capsule and ground distances available to viewport");
}
AE_TEST(character_rebuild_preserves_falling_velocity_but_resets_explicit_relocation) {
  EditorDocument doc;const auto floor=solid(doc,"Floor",0,-.5f,0,scene::BodyMotion::Static,10);auto floorValue=*doc.find(floor);editCollider(floorValue)->halfY=.5f;doc.applyEntityValues(floor,floorValue);
  const auto id=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Falling");auto value=*doc.find(id);value.transform.position[1]=10;auto*character=static_cast<scene::Character*>(value.components.add(scene::Character::descriptor));character->gravity=24;doc.applyEntityValues(id,value);Fixture fx;AE_EXPECT_TRUE(fx.start(doc)&&fx.physics.advance(.25,fx.world),"character acquires downward velocity");
  const float before=fx.world.graph().find(id)->transform.position[1];const auto body=fx.world.findComponent(fx.world.handle(floor),scene::PhysicsBody::descriptor.id);
  AE_EXPECT_TRUE(fx.world.setProperty(body,"friction",.7f)==runtime::WorldStatus::Ok&&fx.physics.rebuild(fx.world,nullptr)&&fx.physics.advance(.25,fx.world),"unrelated authored body edit rebuilds real world");const float after=fx.world.graph().find(id)->transform.position[1];std::printf("CHARACTER_REBUILD_FALL: before=%.3f after=%.3f displacement=%.3f\n",before,after,before-after);
  AE_EXPECT_TRUE(after<before-1.5f,"unrelated rebuild cannot erase falling velocity");
  auto destination=fx.world.graph().find(id)->transform;destination.position[1]=20;AE_EXPECT_TRUE(fx.world.placeLocalTransform(fx.world.handle(id),destination)==runtime::WorldStatus::Ok&&fx.physics.rebuild(fx.world,nullptr)&&fx.physics.advance(.25,fx.world),"explicit Inspector placement uses its reset policy");
  AE_EXPECT_TRUE(fx.world.graph().find(id)->transform.position[1]>19,"explicit relocation does not silently restore prior falling motion");
}
AE_TEST(character_rebuild_never_carries_old_motion_to_another_world_session) {
  EditorDocument doc;const auto id=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Falling");auto value=*doc.find(id);value.transform.position[1]=10;auto*character=static_cast<scene::Character*>(value.components.add(scene::Character::descriptor));character->gravity=24;doc.applyEntityValues(id,value);
  Fixture fx;AE_EXPECT_TRUE(fx.start(doc)&&fx.physics.advance(.25,fx.world),"old session acquires velocity");const float before=fx.world.graph().find(id)->transform.position[1];GameWorld second;
  AE_EXPECT_TRUE(second.load(fx.world.graph())&&second.worldId()!=fx.world.worldId()&&fx.physics.rebuild(second,nullptr)&&fx.physics.advance(.25,second),"same graph and pose but a genuinely different world");
  AE_EXPECT_TRUE(second.graph().find(id)->transform.position[1]>before-1,"new session starts from authoring, not previous session motion");
}
AE_TEST(character_rebuild_preserves_script_intention_pending_jump_and_support_contacts) {
  EditorDocument doc;const auto floor=solid(doc,"Floor",0,-.5f,0,scene::BodyMotion::Static,10);auto floorValue=*doc.find(floor);editCollider(floorValue)->halfY=.5f;doc.applyEntityValues(floor,floorValue);
  const auto id=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Grounded");auto value=*doc.find(id);value.transform.position[1]=1;auto*character=static_cast<scene::Character*>(value.components.add(scene::Character::descriptor));character->speed=2;character->jumpSpeed=5;doc.applyEntityValues(id,value);Fixture fx;AE_EXPECT_TRUE(fx.start(doc),"world");for(u32 frame=0;frame<120;++frame)AE_EXPECT_TRUE(fx.physics.advance(1./60,fx.world),"settle");const auto before=fx.world.graph().find(id)->transform;
  AE_EXPECT_TRUE(fx.physics.setCharacterScriptMove(id,1,0,0)&&fx.physics.jumpCharacter(id,&fx.world),"script move and supported jump accepted before structural flush");const auto body=fx.world.findComponent(fx.world.handle(floor),scene::PhysicsBody::descriptor.id);
  AE_EXPECT_TRUE(fx.world.setProperty(body,"friction",.7f)==runtime::WorldStatus::Ok&&fx.physics.rebuild(fx.world,nullptr)&&fx.physics.advance(1./60,fx.world),"safe-point rebuild precedes character substep");const auto after=fx.world.graph().find(id)->transform;
  std::printf("CHARACTER_REBUILD_INTENT: dx=%.3f dy=%.3f\n",after.position[0]-before.position[0],after.position[1]-before.position[1]);
  AE_EXPECT_TRUE(after.position[0]>before.position[0]+.02f&&after.position[1]>before.position[1]+.04f,"accepted movement and pending jump survive unrelated rebuild with refreshed support");
}
AE_TEST(constant_force_fixed_steps_local_orientation_and_live_properties_use_jolt) {
  EditorDocument doc;
  const auto id=solid(doc,"Propulsor",0,10,0,scene::BodyMotion::Dynamic);
  auto value=*doc.find(id);value.transform.rotationDegrees[1]=90;
  auto *body=editPhysicsBody(value);body->mass=2;body->linearDamping=body->angularDamping=0;
  auto *force=static_cast<scene::ConstantForce*>(value.components.add(scene::ConstantForce::descriptor));
  force->relativeForceZ=4;force->forceY=2;
  AE_EXPECT_TRUE(doc.applyEntityValues(id,value),"força autorada");
  Fixture fx;AE_EXPECT_TRUE(fx.start(doc),fx.physics.error().c_str());
  for(u32 step=0;step<60;++step) AE_EXPECT_TRUE(fx.physics.advance(1./60,fx.world),"subpasso físico");
  float velocity[3]{};AE_EXPECT_TRUE(fx.physics.getBodyVelocity(id,velocity),"velocidade real");
  AE_EXPECT_TRUE(std::abs(velocity[0]-2)<.02f && std::abs(velocity[1]-1)<.02f && std::abs(velocity[2])<.01f,
    "F/m integrado por1s; localZ vira mundoX após rotaçãoY90");
  const auto handle=fx.world.findComponent(fx.world.handle(id),"astra.physics.constant_force");
  AE_EXPECT_TRUE(fx.world.setProperty(handle,"enabled",false)==runtime::WorldStatus::Ok,"desliga ao vivo");
  const float before=velocity[0];
  AE_EXPECT_TRUE(fx.physics.advance(.25,fx.world) && fx.physics.getBodyVelocity(id,velocity),"15 subpassos");
  AE_EXPECT_TRUE(std::abs(velocity[0]-before)<.001f,"força desligada não altera velocidade");
  AE_EXPECT_TRUE(fx.world.setProperty(handle,"enabled",true)==runtime::WorldStatus::Ok,"religa");
  const float localZero[3]{};const float worldForce[3]{6,0,0};
  AE_EXPECT_TRUE(fx.world.setTriple(handle,"relative_force",localZero)==runtime::WorldStatus::Ok &&
    fx.world.setTriple(handle,"force",worldForce)==runtime::WorldStatus::Ok,"vetores atômicos");
  AE_EXPECT_TRUE(fx.physics.advance(.25,fx.world) && fx.physics.getBodyVelocity(id,velocity),"força nova");
  AE_EXPECT_TRUE(std::abs(velocity[0]-before-.75f)<.02f,"força nova6N atua sem rebuild");
  AE_EXPECT_TRUE(fx.world.setProperty(handle,"torque_y",2.f)==runtime::WorldStatus::Ok,"torque ao vivo");
  AE_EXPECT_TRUE(fx.physics.advance(.25,fx.world),"torque no solver");
  AE_EXPECT_TRUE(std::abs(fx.world.graph().find(id)->transform.rotationDegrees[1]-90)>.1f,"torque altera pose angular real");
  AE_EXPECT_TRUE(fx.physics.getBodyVelocity(id,velocity),"velocidade antes de remover");
  AE_EXPECT_TRUE(fx.world.removeComponent(handle)==runtime::WorldStatus::Ok,"remove por fila");fx.world.flush();
  const float after=velocity[0];
  AE_EXPECT_TRUE(fx.physics.advance(.25,fx.world) && fx.physics.getBodyVelocity(id,velocity),"após remoção");
  AE_EXPECT_TRUE(std::abs(velocity[0]-after)<.03f,"remoção não continua força");
}

AE_TEST(constant_force_creation_history_archive_gizmo_and_play_form_one_chain) {
  EditorSession session;u32 recipe=0;
  AE_EXPECT_TRUE(findCreationRecipe("physics.propulsion",&recipe),"receita catalogada");
  const auto id=session.createRecipe(recipe,session.document().root());
  AE_EXPECT_TRUE(id!=0,"cria corpo+colisor+força pelo editor");
  const auto visual=collectComponentVisuals(session.document(),id,1.5f);
  AE_EXPECT_TRUE(std::any_of(visual.begin(),visual.end(),[](const auto &v){return v.icon==ui::UiIcon::ComponentConstantForce && v.segments.size()>=3;}),"gizmo de direção real");
  const auto archive=serializeEditorDocument(session.document(),73);EditorDocument reopened;
  AE_EXPECT_TRUE(deserializeEditorDocument(archive,73,reopened),"salva/reabre");
  const auto *force=static_cast<const scene::ConstantForce*>(reopened.find(id)->components.find(scene::ConstantForce::descriptor));
  AE_EXPECT_TRUE(force && force->relativeForceZ==4 && force->enabled,"campos persistidos");
  AE_EXPECT_TRUE(session.history().undo(session.document()) && !session.document().exists(id),"undo composição completa");
  AE_EXPECT_TRUE(session.history().redo(session.document()) && session.document().exists(id),"redo composição completa");
  Fixture fx;AE_EXPECT_TRUE(fx.start(reopened),fx.physics.error().c_str());
  AE_EXPECT_TRUE(fx.physics.advance(.25,fx.world),"Play realJolt");
  float v[3]{};AE_EXPECT_TRUE(fx.physics.getBodyVelocity(id,v) && v[2]>.9f,"receita propulsora tem efeito");
  AE_EXPECT_TRUE(fx.world.setActive(fx.world.handle(id),false)==runtime::WorldStatus::Ok,"desativa objeto");
  AE_EXPECT_TRUE(fx.physics.advance(.25,fx.world),"objeto inativo");
  float stopped[3]{};AE_EXPECT_TRUE(fx.physics.getBodyVelocity(id,stopped) && stopped[2]<=v[2]+.001f,"hierarquia inativa não aplica força");
}

AE_TEST(constant_force_refuses_unsupported_motion_instead_of_silent_no_effect) {
  EditorDocument doc;const auto id=solid(doc,"Estático",0,0,0,scene::BodyMotion::Static);
  auto value=*doc.find(id);auto *force=static_cast<scene::ConstantForce*>(value.components.add(scene::ConstantForce::descriptor));
  force->forceX=4;AE_EXPECT_TRUE(doc.applyEntityValues(id,value),"autoria draft");
  Fixture fx;AE_EXPECT_TRUE(fx.world.load(doc),"grafo");
  AE_EXPECT_TRUE(!fx.physics.start(fx.world) && fx.physics.error().find("dinâmico")!=std::string::npos,"recusa contextual");
  force->enabled=false;AE_EXPECT_TRUE(doc.applyEntityValues(id,value) && fx.start(doc),"draftdesligado aceita corpoestático");
  AE_EXPECT_TRUE(scene::componentRemovalBlockedBy("astra.physics.body",doc.find(id)->components)!=nullptr,"dependência impede remoçãoisolada");
}

// Contrato Invalidate::PhysicsBody: mudar uma propriedade do corpo com o jogo
// rodando (script ou Inspector em Play) recria o corpo no solver com o valor
// novo, sem zerar o movimento que ele já tinha.
AE_TEST(runtime_body_property_change_rebuilds_the_body_and_keeps_its_motion) {
  EditorDocument doc;
  const auto ball = solid(doc, "Bola", 0, 10, 0, scene::BodyMotion::Dynamic);
  auto values = *doc.find(ball);
  editPhysicsBody(values)->velocityX = 3;
  AE_EXPECT_TRUE(doc.applyEntityValues(ball, values), "bola sem gravidade, andando em X");
  Fixture fx;
  AE_EXPECT_TRUE(fx.start(doc), fx.physics.error().c_str());
  for (int step = 0; step < 30; ++step) AE_EXPECT_TRUE(fx.physics.advance(1. / 60, fx.world), "passo");
  const float heightBefore = fx.world.graph().find(ball)->transform.position[1];
  AE_EXPECT_TRUE(std::abs(heightBefore - 10.f) < 1e-3f, "sem gravidade a altura não muda");
  float velocity[3]{};
  AE_EXPECT_TRUE(fx.physics.getBodyVelocity(ball, velocity) && velocity[0] > 2.5f, "velocidade inicial em curso");
  const float speed = velocity[0];

  const auto body = fx.world.findComponent(fx.world.handle(ball), scene::PhysicsBody::descriptor.id);
  AE_EXPECT_TRUE(fx.world.consumeInvalidation() == 0u, "nada pendente antes da mudança");
  AE_EXPECT_TRUE(fx.world.setProperty(body, "gravity_factor", 1.0f) == runtime::WorldStatus::Ok, "gravidade ligada");
  AE_EXPECT_TRUE((fx.world.consumeInvalidation() & scene::Invalidate::PhysicsBody) != 0u,
                 "a mudança declara o corpo inválido");
  AE_EXPECT_TRUE(fx.physics.rebuild(fx.world, nullptr), fx.physics.error().c_str());
  AE_EXPECT_TRUE(fx.physics.getBodyVelocity(ball, velocity) && std::abs(velocity[0] - speed) < 1e-3f,
                 "o corpo recriado continua com a velocidade que tinha, não a autorada");
  for (int step = 0; step < 30; ++step) AE_EXPECT_TRUE(fx.physics.advance(1. / 60, fx.world), "passo");
  AE_EXPECT_TRUE(fx.world.graph().find(ball)->transform.position[1] < heightBefore - .5f,
                 "a gravidade nova age no solver");
}

AE_TEST(runtime_raycast_reports_object_collider_point_and_real_surface_normal) {
  EditorDocument doc;
  const auto target = solid(doc, "Alvo", 5, 0, 0, scene::BodyMotion::Static, 1);
  Fixture fx;
  AE_EXPECT_TRUE(fx.start(doc), fx.physics.error().c_str());

  const float origin[3]{0, 0, 0}, direction[3]{10, 0, 0};
  QueryHit hit;
  AE_EXPECT_TRUE(fx.physics.rayCast(origin, direction, QueryFilter{}, hit), "raio acerta o alvo");
  AE_EXPECT_EQ(hit.object, target, "identidade de objeto, não de corpo nativo");
  AE_EXPECT_TRUE(hit.colliderInstance != 0, "instância do colisor que respondeu");
  AE_EXPECT_TRUE(std::abs(hit.point[0] - 4) < .01f, "ponto de contato na face próxima");
  AE_EXPECT_TRUE(hit.hasNormal, "normal presente");
  // A face acertada olha para a origem do raio: a normal é -X.
  AE_EXPECT_TRUE(hit.normal[0] < -.9f, "normal de superfície real, não um vetor zero");
  AE_EXPECT_TRUE(std::abs(hit.distance - 4) < .01f, "distância em unidades de mundo");
  AE_EXPECT_TRUE(hit.fraction > .39f && hit.fraction < .41f, "fração ao longo do raio");
  AE_EXPECT_TRUE(!hit.isSensor, "alvo sólido");

  // Raio de comprimento zero não tem direção: recusado em vez de devolver um
  // acerto degenerado.
  const float none[3]{0, 0, 0};
  AE_EXPECT_TRUE(!fx.physics.rayCast(origin, none, QueryFilter{}, hit), "raio de comprimento zero é recusado");

  // Ignorar o próprio objeto é o caso mais comum de quem consulta.
  QueryFilter ignoring;
  ignoring.ignore = target;
  AE_EXPECT_TRUE(!fx.physics.rayCast(origin, direction, ignoring, hit), "objeto ignorado não aparece");
}

AE_TEST(runtime_raycast_all_orders_hits_and_reports_the_real_count) {
  EditorDocument doc;
  const auto near = solid(doc, "Perto", 3, 0, 0, scene::BodyMotion::Static);
  const auto far = solid(doc, "Longe", 7, 0, 0, scene::BodyMotion::Static);
  Fixture fx;
  AE_EXPECT_TRUE(fx.start(doc), fx.physics.error().c_str());

  const float origin[3]{0, 0, 0}, direction[3]{20, 0, 0};
  QueryHit hits[4]{};
  AE_EXPECT_EQ(fx.physics.rayCastAll(origin, direction, QueryFilter{}, hits, 4), 2u, "dois acertos");
  AE_EXPECT_EQ(hits[0].object, near, "mais próximo primeiro");
  AE_EXPECT_EQ(hits[1].object, far, "depois o distante");

  // Buffer pequeno: a contagem REAL continua sendo devolvida, para que quem
  // chama saiba que houve truncamento em vez de acreditar em um resultado curto.
  QueryHit single[1]{};
  AE_EXPECT_EQ(fx.physics.rayCastAll(origin, direction, QueryFilter{}, single, 1), 2u, "contagem real mesmo truncada");
  AE_EXPECT_EQ(single[0].object, near, "prefixo é o mais próximo");
}

AE_TEST(runtime_queries_respect_gameplay_layers_and_sensor_policy) {
  EditorDocument doc;
  auto layers = doc.layers();
  AE_EXPECT_TRUE(layers.setName(1, "Cenário") && layers.setName(2, "Gatilho"), "camadas nomeadas");
  doc.setLayers(layers);
  const auto scenery = solid(doc, "Parede", 4, 0, 0, scene::BodyMotion::Static, .5f, 1);
  const auto trigger = solid(doc, "Zona", 2, 0, 0, scene::BodyMotion::Kinematic, .5f, 2);
  auto values = *doc.find(trigger);
  editPhysicsBody(values)->sensor = true;
  AE_EXPECT_TRUE(doc.applyEntityValues(trigger, values), "zona vira sensor");

  Fixture fx;
  AE_EXPECT_TRUE(fx.start(doc), fx.physics.error().c_str());
  const float origin[3]{0, 0, 0}, direction[3]{10, 0, 0};
  QueryHit hit;

  // Sensores ficam de fora por padrão: o raio de visada atravessa a zona.
  AE_EXPECT_TRUE(fx.physics.rayCast(origin, direction, QueryFilter{}, hit), "acerta algo");
  AE_EXPECT_EQ(hit.object, scenery, "sensor não interrompe a visada por padrão");

  QueryFilter withSensors;
  withSensors.includeSensors = true;
  AE_EXPECT_TRUE(fx.physics.rayCast(origin, direction, withSensors, hit), "com sensores");
  AE_EXPECT_EQ(hit.object, trigger, "agora o sensor é o mais próximo");
  AE_EXPECT_TRUE(hit.isSensor, "marcado como sensor");

  // Máscara por camada: só "Cenário".
  QueryFilter onlyScenery;
  onlyScenery.gameplayLayerMask = 1u << 1;
  onlyScenery.includeSensors = true;
  AE_EXPECT_TRUE(fx.physics.rayCast(origin, direction, onlyScenery, hit), "com máscara");
  AE_EXPECT_EQ(hit.object, scenery, "a máscara exclui a camada do gatilho");

  QueryFilter nothing;
  nothing.gameplayLayerMask = 1u << 5;
  AE_EXPECT_TRUE(!fx.physics.rayCast(origin, direction, nothing, hit), "camada vazia não acerta nada");
}

AE_TEST(runtime_layer_matrix_prevents_the_contact_in_the_solver) {
  // Interagindo: a bola para sobre o piso.
  {
    EditorDocument doc;
    const auto ball = fallingBallOverFloor(doc, false);
    AE_EXPECT_TRUE(ball, "cena montada");
    Fixture fx;
    AE_EXPECT_TRUE(fx.start(doc), fx.physics.error().c_str());
    for (int step = 0; step < 120; ++step) AE_EXPECT_TRUE(fx.physics.advance(1. / 60, fx.world), "passo");
    AE_EXPECT_TRUE(fx.world.graph().find(ball)->transform.position[1] > -1.f, "a bola repousa sobre o piso");
  }
  // Separadas: a matriz impede o par no SOLVER, e a bola atravessa.
  {
    EditorDocument doc;
    const auto ball = fallingBallOverFloor(doc, true);
    AE_EXPECT_TRUE(ball, "cena montada");
    Fixture fx;
    AE_EXPECT_TRUE(fx.start(doc), fx.physics.error().c_str());
    for (int step = 0; step < 120; ++step) AE_EXPECT_TRUE(fx.physics.advance(1. / 60, fx.world), "passo");
    AE_EXPECT_TRUE(fx.world.graph().find(ball)->transform.position[1] < -3.f,
                   "camadas que não interagem não geram contato, não só resultado filtrado");
  }
}

AE_TEST(runtime_solid_contacts_reach_the_consumer_with_normal_on_enter) {
  EditorDocument doc;
  const auto floorId = solid(doc, "Piso", 0, -1, 0, scene::BodyMotion::Static, 4);
  auto values = *doc.find(floorId);
  editCollider(values)->halfY = .5f;
  AE_EXPECT_TRUE(doc.applyEntityValues(floorId, values), "piso");
  const auto ball = solid(doc, "Caixa", 0, 1, 0, scene::BodyMotion::Dynamic, .5f);
  values = *doc.find(ball);
  editPhysicsBody(values)->gravityFactor = 1;
  AE_EXPECT_TRUE(doc.applyEntityValues(ball, values), "caixa cai");

  Fixture fx;
  AE_EXPECT_TRUE(fx.start(doc), fx.physics.error().c_str());
  struct Collected {
    u32 enter = 0, stay = 0, exit = 0;
    bool enterHadNormal = false, exitHadNormal = true, pairMatches = true;
    runtime::ObjectId floorId = 0, ball = 0;
  } collected{};
  collected.floorId = floorId;
  collected.ball = ball;
  const auto sink = [](void *opaque, const ContactEvent &event) -> bool {
    auto &c = *static_cast<Collected *>(opaque);
    const bool pair = (event.first == c.floorId && event.second == c.ball) ||
                      (event.first == c.ball && event.second == c.floorId);
    c.pairMatches = c.pairMatches && pair;
    if (event.phase == 0) { ++c.enter; c.enterHadNormal = event.hasNormal; }
    else if (event.phase == 1) ++c.stay;
    else { ++c.exit; c.exitHadNormal = event.hasNormal; }
    return true;
  };
  for (int step = 0; step < 120; ++step)
    AE_EXPECT_TRUE(fx.physics.advance(1. / 60, fx.world, nullptr, &collected, nullptr, sink), "passo com contatos");

  AE_EXPECT_EQ(collected.enter, 1u, "um Enter por par de corpos, mesmo com várias subformas");
  AE_EXPECT_TRUE(collected.stay >= 1, "Stay enquanto o contato dura");
  AE_EXPECT_TRUE(collected.enterHadNormal, "Enter traz a normal do manifold");
  AE_EXPECT_TRUE(collected.pairMatches, "o par é sempre piso/caixa");
}

AE_TEST(runtime_shape_cast_and_overlap_find_obstacles_for_a_camera) {
  EditorDocument doc;
  const auto wall = solid(doc, "Parede", 3, 0, 0, scene::BodyMotion::Static, 1);
  Fixture fx;
  AE_EXPECT_TRUE(fx.start(doc), fx.physics.error().c_str());

  QueryShapeDesc sphere;
  sphere.kind = QueryShapeKind::Sphere;
  sphere.radius = .3f;
  const float origin[3]{0, 0, 0}, direction[3]{6, 0, 0};
  QueryHit hit;
  AE_EXPECT_TRUE(fx.physics.shapeCast(sphere, origin, direction, QueryFilter{}, hit), "varredura encontra a parede");
  AE_EXPECT_EQ(hit.object, wall, "objeto correto");
  AE_EXPECT_TRUE(hit.hasNormal, "eixo de separação normalizado vira direção utilizável");
  AE_EXPECT_TRUE(hit.fraction > 0 && hit.fraction < 1, "parou antes do fim do caminho");

  const float inside[3]{3, 0, 0};
  QueryHit hits[4]{};
  AE_EXPECT_EQ(fx.physics.overlap(sphere, inside, QueryFilter{}, hits, 4), 1u, "sobreposição encontra a parede");
  AE_EXPECT_EQ(hits[0].object, wall, "objeto correto");
  AE_EXPECT_TRUE(!hits[0].hasNormal, "sobreposição parada não inventa direção de contato");

  const float empty[3]{-20, 0, 0};
  AE_EXPECT_EQ(fx.physics.overlap(sphere, empty, QueryFilter{}, hits, 4), 0u, "espaço vazio não acerta nada");
}

AE_TEST(runtime_gameplay_layers_persist_names_and_reciprocal_matrix) {
  GameplayLayers layers;
  AE_EXPECT_EQ(std::string(layers.name(0)), std::string("Padrão"), "camada padrão sempre existe");
  AE_EXPECT_TRUE(layers.interacts(0, 3), "tudo interage por padrão");
  AE_EXPECT_TRUE(layers.setName(1, "Personagem") && layers.setName(2, "Cenário"), "nomes");
  AE_EXPECT_TRUE(!layers.setName(3, "Cenário"), "nome duplicado é recusado");
  AE_EXPECT_TRUE(!layers.setName(0, ""), "a camada padrão não pode ser apagada");
  AE_EXPECT_TRUE(layers.setInteraction(1, 2, false), "separar duas camadas");
  AE_EXPECT_TRUE(!layers.interacts(1, 2) && !layers.interacts(2, 1), "a separação é recíproca dos dois lados");
  AE_EXPECT_EQ(layers.maskFor(1) & (1u << 2), 0u, "máscara reflete a matriz");

  std::ostringstream out;
  out.imbue(std::locale::classic());
  layers.write(out);
  std::istringstream in(out.str());
  in.imbue(std::locale::classic());
  GameplayLayers restored;
  AE_EXPECT_TRUE(restored.read(in), "ida e volta");
  AE_EXPECT_TRUE(restored == layers, "nomes e matriz preservados");

  // Uma matriz assimétrica editada à mão é recusada na leitura, em vez de
  // instalar um filtro que dependeria da ordem de criação dos corpos.
  std::istringstream broken(R"(2 "Padrão" 3 "Outro" 1)");
  GameplayLayers rejected;
  AE_EXPECT_TRUE(!rejected.read(broken), "matriz assimétrica é recusada");
}

AE_TEST(runtime_last_collider_disable_keeps_motion_and_removes_queries_until_reenabled) {
  EditorDocument doc;const auto id=solid(doc,"Corpo",5,0,0,scene::BodyMotion::Dynamic);
  auto values=*doc.find(id);editPhysicsBody(values)->velocityY=2;doc.applyEntityValues(id,values);
  Fixture fx;AE_EXPECT_TRUE(fx.start(doc),"corpo com forma");
  const auto collider=fx.world.findComponent(fx.world.handle(id),"astra.physics.collider");
  const float origin[3]{0,0,0},direction[3]{10,0,0};QueryHit hit;
  AE_EXPECT_TRUE(fx.physics.rayCast(origin,direction,{},hit),"forma responde antes");
  AE_EXPECT_TRUE(fx.world.setProperty(collider,"enabled",false)==runtime::WorldStatus::Ok &&
                 fx.physics.rebuild(fx.world,nullptr),fx.physics.error().c_str());
  AE_EXPECT_TRUE(!fx.physics.rayCast(origin,direction,{},hit),"último colisor não participa de consultas");
  float velocity[3]{};
  AE_EXPECT_TRUE(fx.physics.getBodyVelocity(id,velocity) && velocity[1]>1.9f,"corpo conserva velocidade");
  for(int i=0;i<6;++i) AE_EXPECT_TRUE(fx.physics.advance(1./60,fx.world),"simula sem colisão");
  AE_EXPECT_TRUE(fx.world.graph().find(id)->transform.position[1]>.15f,"dinâmica continua");
  AE_EXPECT_TRUE(fx.world.setProperty(collider,"enabled",true)==runtime::WorldStatus::Ok &&
                 fx.physics.rebuild(fx.world,nullptr),"religa sem reiniciar Play");
  const float shifted[3]{0,fx.world.graph().find(id)->transform.position[1],0};
  AE_EXPECT_TRUE(fx.physics.rayCast(shifted,direction,{},hit) && hit.colliderInstance==collider.instance,"identidade e consultas retomadas");
  values=*doc.find(id);editCollider(values)->enabled=false;doc.applyEntityValues(id,values);
  Fixture initiallyDisabled;AE_EXPECT_TRUE(initiallyDisabled.start(doc),"também inicia com forma desligada");
}

AE_TEST(character_platform_carry_version_four_migration_history_and_live_property) {
  for(u32 version=1;version<=3;++version){std::stringstream payload;payload<<".45 .55 1.65 8 45";if(version>=2)payload<<" 5";if(version>=3)payload<<" .4 .5 9.81";scene::Character value;value.inheritPlatformHorizontal=true;AE_EXPECT_TRUE(value.read(payload,version)&&!value.inheritPlatformHorizontal,"legacy projects retain no horizontal inheritance");}
  scene::Character source;source.inheritPlatformHorizontal=true;std::stringstream saved;source.write(saved);scene::Character restored;AE_EXPECT_TRUE(restored.read(saved,4)&&restored.inheritPlatformHorizontal,"v4 stores selected policy");std::stringstream invalid(".45 .55 1.65 8 45 5 .4 .5 9.81 2");AE_EXPECT_TRUE(!restored.read(invalid,4),"unknown boolean refused");
  EditorDocument doc;const auto actor=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Actor");auto original=*doc.find(actor);original.components.add(scene::Character::descriptor);doc.applyEntityValues(actor,original);EditorHistory history;auto changed=*doc.find(actor);runtime::editCharacter(changed)->inheritPlatformHorizontal=true;AE_EXPECT_TRUE(history.applyValues(doc,actor,changed)&&runtime::characterComponent(*doc.find(actor))->inheritPlatformHorizontal,"same editor history mutation as inspector");AE_EXPECT_TRUE(history.undo(doc)&&!runtime::characterComponent(*doc.find(actor))->inheritPlatformHorizontal&&history.redo(doc)&&runtime::characterComponent(*doc.find(actor))->inheritPlatformHorizontal,"Undo/Redo preserves policy");
  const auto archive=serializeEditorDocument(doc,0);EditorDocument reopened;AE_EXPECT_TRUE(deserializeEditorDocument(archive,0,reopened)&&runtime::characterComponent(*reopened.find(actor))->inheritPlatformHorizontal,"save/reopen");GameWorld world;AE_EXPECT_TRUE(world.load(reopened),"real runtime graph");world.consumeInvalidation();const auto component=world.findComponent(world.handle(actor),scene::Character::descriptor.id);AE_EXPECT_TRUE(world.setProperty(component,"inherit_platform_horizontal",false)==runtime::WorldStatus::Ok&&(world.consumeInvalidation()&(scene::Invalidate::PhysicsBody|scene::Invalidate::PhysicsShape))==0,"live toggle has no solver reconstruction");
}

#include "mechanisms_fixture.h"
#include <fstream>
#include <filesystem>

AE_TEST(mechanisms_block_authored_roundtrip_solver_queries_live_edit_and_removal) {
  ae::test::MechanismsFixture authored;AE_EXPECT_TRUE(authored.create(),"complete authored mechanisms");
  const auto text=serializeEditorDocument(authored.document,0);
  EditorDocument restored;AE_EXPECT_TRUE(deserializeEditorDocument(text,0,restored),"all mechanisms survive scene roundtrip");
  AE_EXPECT_EQ(serializeEditorDocument(restored,0),text,"stable identities and every channel persisted");
  // Keep the exact executable acceptance scene for reopening in the editor.
  std::filesystem::create_directories("docs/validacao/evidencias/mechanisms-20261001");
  std::ofstream("docs/validacao/evidencias/mechanisms-20261001/mechanisms.aescene")<<text;
  Fixture fx;AE_EXPECT_TRUE(fx.start(restored),fx.physics.error().c_str());
  AE_EXPECT_EQ(fx.physics.jointCount(),5u,"five real Jolt constraints in one world");
  for(int i=0;i<240;++i)AE_EXPECT_TRUE(fx.physics.advance(1./120,fx.world),"integrated step");
  for(u32 i=0;i<4;++i)AE_EXPECT_TRUE(std::abs(fx.world.graph().find(authored.bodies[i])->transform.position[1]-6)<.08f,"mechanism resists gravity");
  AE_EXPECT_TRUE(std::abs(fx.world.graph().find(authored.bodies[3])->transform.position[0]-18.75f)<.08f,"six DOF position drive reaches target");
  const float springY=fx.world.graph().find(authored.bodies[4])->transform.position[1];
  AE_EXPECT_TRUE(springY<6&&springY>5.85f,"spring has physical sag under gravity");
  QueryHit hit;const float top[]{0,7,0},down[]{0,-3,0};
  AE_EXPECT_TRUE(fx.physics.rayCast(top,down,{},hit)&&hit.object==authored.bodies[0],"cylinder top cap receives ray");
  AE_EXPECT_TRUE(std::abs(hit.point[1]-6.5f)<.05f,"flat cap excludes capsule hemispheres");
  QueryShapeDesc cylinder;cylinder.kind=QueryShapeKind::Cylinder;cylinder.radius=.1f;cylinder.halfHeight=.1f;
  AE_EXPECT_TRUE(fx.physics.shapeCast(cylinder,top,down,{},hit)&&hit.object==authored.bodies[0],"cylinder sweep uses the same geometry contract");
  const auto drive=fx.world.findComponent(fx.world.handle(authored.bodies[3]),"astra.physics.joint");
  AE_EXPECT_TRUE(fx.world.setProperty(drive,"linear_x_position",-.5f)==runtime::WorldStatus::Ok,"public property API updates drive");
  AE_EXPECT_TRUE(fx.physics.rebuild(fx.world,nullptr),fx.physics.error().c_str());
  for(int i=0;i<240;++i)AE_EXPECT_TRUE(fx.physics.advance(1./120,fx.world),"updated mechanism");
  AE_EXPECT_TRUE(std::abs(fx.world.graph().find(authored.bodies[3])->transform.position[0]-17.5f)<.08f,"new target consumed by rebuilt solver");
  const auto fixed=fx.world.findComponent(fx.world.handle(authored.bodies[0]),"astra.physics.joint");
  AE_EXPECT_TRUE(fx.world.removeComponent(fixed)==runtime::WorldStatus::Ok,"runtime removal queues");fx.world.flush();
  AE_EXPECT_TRUE(fx.physics.rebuild(fx.world,nullptr),"safe point removes constraint");
  AE_EXPECT_EQ(fx.physics.jointCount(),4u,"no stale native constraint");
  for(int i=0;i<60;++i)AE_EXPECT_TRUE(fx.physics.advance(1./120,fx.world),"released body steps");
  AE_EXPECT_TRUE(fx.world.graph().find(authored.bodies[0])->transform.position[1]<5.2f,"released body falls");
}

AE_TEST(mechanisms_block_migration_and_invalid_drafts_are_atomic) {
  scene::Joint old;std::ostringstream payload;payload<<3<<' '<<0<<' '<<0<<' '<<1;
  for(const auto &p:scene::jointBaseNumbers)payload<<' '<<p.read(old);
  std::istringstream legacy(payload.str());scene::Joint migrated;
  AE_EXPECT_TRUE(migrated.read(legacy,1)&&migrated.valid(),"v1 scene reads with dormant defaults");
  ae::test::MechanismsFixture authored;AE_EXPECT_TRUE(authored.create(),"authored fixture");
  GameWorld world;AE_EXPECT_TRUE(world.load(authored.document),"load");
  const auto drive=world.findComponent(world.handle(authored.bodies[3]),"astra.physics.joint");
  AE_EXPECT_TRUE(world.setProperty(drive,"linear_x_minimum",2.f)!=runtime::WorldStatus::Ok,"inverted limit rejected");
  scene::ComponentPropertyValue actual;world.getProperty(drive,"linear_x_minimum",actual);
  AE_EXPECT_TRUE(std::get<float>(actual)==-1,"bad draft did not change data");
  const float parallel[]{1,0,0};
  AE_EXPECT_TRUE(world.setTriple(drive,"normal_a",parallel)!=runtime::WorldStatus::Ok,"degenerate reference frame rejected atomically");
  AE_EXPECT_TRUE(world.setProperty(drive,"angular_y_motor",u32{1})!=runtime::WorldStatus::Ok,"locked axis cannot advertise an active motor");
}
