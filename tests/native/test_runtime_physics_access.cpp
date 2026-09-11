#include "harness.h"
#include "editor/editor_document.h"
#include "editor/editor_physics_body.h"
#include "runtime/scene_physics.h"

#include <cmath>

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
