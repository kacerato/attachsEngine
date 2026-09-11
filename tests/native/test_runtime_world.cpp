#include "harness.h"
#include "editor/editor_play_scene.h"
#include "editor/editor_physics_body.h"
#include "runtime/game_world.h"
#include "runtime/scene_components.h"
#include "scene/component_schema.h"

using namespace ae;
using namespace ae::editor;
using ae::runtime::GameWorld;
using ae::runtime::ObjectHandle;
using ae::runtime::TransformAuthority;
using ae::runtime::WorldStatus;

namespace {
EditorEntityId named(EditorDocument &doc, EditorEntityId parent, const char *name) {
  return doc.createEntity(parent, EditorEntityKind::Folder, name);
}
}

AE_TEST(runtime_world_loads_scene_and_keeps_authoring_document_untouched) {
  EditorDocument doc;
  const auto child = named(doc, doc.root(), "Filho");
  auto values = *doc.find(child);
  values.transform.position[0] = 4;
  AE_EXPECT_TRUE(doc.applyEntityValues(child, values), "autoria inicial");
  const auto revision = doc.revision();

  GameWorld world;
  AE_EXPECT_TRUE(world.load(doc), "carga da cena");
  const auto handle = world.handle(child);
  AE_EXPECT_TRUE(handle.valid() && world.alive(handle), "handle do objeto carregado");

  runtime::Transform moved;
  AE_EXPECT_EQ(static_cast<u32>(world.localTransform(handle, moved)), 0u, "leitura de transform");
  moved.position[1] = 9;
  AE_EXPECT_EQ(static_cast<u32>(world.setLocalTransform(handle, moved)), 0u, "escrita no mundo");
  AE_EXPECT_EQ(world.graph().find(child)->transform.position[1], 9.f, "mundo recebeu a pose");
  AE_EXPECT_EQ(doc.find(child)->transform.position[1], 0.f, "documento autoral intacto");
  AE_EXPECT_EQ(doc.revision(), revision, "nenhuma revisão do documento durante a execução");
}

AE_TEST(runtime_world_rejects_handles_from_another_play_session) {
  EditorDocument doc;
  const auto child = named(doc, doc.root(), "Filho");
  GameWorld first, second;
  AE_EXPECT_TRUE(first.load(doc) && second.load(doc), "duas sessões");
  AE_EXPECT_TRUE(first.worldId() != second.worldId(), "sessões têm identidades distintas");
  const auto foreign = first.handle(child);
  AE_EXPECT_EQ(static_cast<u32>(second.validate(foreign)), static_cast<u32>(WorldStatus::ForeignWorld),
               "handle de outra execução é recusado, não reinterpretado");
}

AE_TEST(runtime_world_destroy_expires_handle_immediately_and_frees_at_safe_point) {
  EditorDocument doc;
  const auto parent = named(doc, doc.root(), "Pai");
  const auto child = named(doc, parent, "Filho");
  GameWorld world;
  AE_EXPECT_TRUE(world.load(doc), "carga");
  const auto parentHandle = world.handle(parent);
  const auto childHandle = world.handle(child);

  AE_EXPECT_EQ(static_cast<u32>(world.destroyObject(parentHandle)), 0u, "remoção enfileirada");
  // A referência vence AGORA, ainda dentro do mesmo callback.
  AE_EXPECT_EQ(static_cast<u32>(world.validate(parentHandle)), static_cast<u32>(WorldStatus::StaleHandle),
               "handle do removido é recusado imediatamente");
  AE_EXPECT_EQ(static_cast<u32>(world.validate(childHandle)), static_cast<u32>(WorldStatus::StaleHandle),
               "a subárvore inteira vence junto");
  runtime::Transform ignored;
  AE_EXPECT_EQ(static_cast<u32>(world.localTransform(parentHandle, ignored)), static_cast<u32>(WorldStatus::StaleHandle),
               "leitura por referência vencida é recusada");
  // O armazenamento ainda existe: a iteração em curso não é corrompida.
  AE_EXPECT_TRUE(world.graph().exists(parent), "remoção só toca o armazenamento no ponto seguro");
  AE_EXPECT_EQ(world.pendingCommandCount(), 1u, "um comando pendente");

  std::vector<runtime::ObjectId> destroyed;
  AE_EXPECT_EQ(world.flush(&destroyed), 1u, "ponto seguro aplica a fila");
  AE_EXPECT_EQ(static_cast<u32>(destroyed.size()), 2u, "pai e filho relatados ao consumidor");
  AE_EXPECT_TRUE(!world.graph().exists(parent) && !world.graph().exists(child), "armazenamento liberado");
  AE_EXPECT_EQ(world.pendingCommandCount(), 0u, "fila esvaziada");
}

AE_TEST(runtime_world_creates_objects_immediately_without_breaking_iteration) {
  EditorDocument doc;
  GameWorld world;
  AE_EXPECT_TRUE(world.load(doc), "carga");
  const auto root = world.root();
  WorldStatus status = WorldStatus::Rejected;

  // Percorre os filhos criando um objeto por iteração: o vetor de ids é copiado
  // por quem itera, então crescer a hierarquia aqui não invalida nada.
  const auto created = world.createObject(root, "Criado", status);
  AE_EXPECT_EQ(static_cast<u32>(status), 0u, "criação aceita");
  AE_EXPECT_TRUE(created.valid() && world.alive(created), "handle utilizável na mesma linha");
  AE_EXPECT_EQ(world.childCount(root), 1u, "criação é imediata na hierarquia");
  AE_EXPECT_EQ(static_cast<u32>(world.pendingCommandCount()), 0u, "criar não enfileira comando");

  const auto nested = world.createObject(created, "Neto", status);
  AE_EXPECT_TRUE(nested.valid(), "criação encadeada");
  AE_EXPECT_TRUE(world.parentOf(nested) == created, "pai correto");
  AE_EXPECT_TRUE(world.findChildByName(root, "Neto", true) == nested, "busca recursiva encontra o novo objeto");
  AE_EXPECT_TRUE(!world.findChildByName(root, "Neto", false).valid(), "busca direta não atravessa níveis");
}

AE_TEST(runtime_world_component_api_follows_the_shared_schema) {
  EditorDocument doc;
  const auto id = named(doc, doc.root(), "Objeto");
  GameWorld world;
  AE_EXPECT_TRUE(world.load(doc), "carga");
  const auto handle = world.handle(id);
  WorldStatus status = WorldStatus::Rejected;

  // Câmera é declarada mutável em Play; corpo físico não é.
  const auto camera = world.addComponent(handle, "astra.camera", status);
  AE_EXPECT_EQ(static_cast<u32>(status), 0u, "câmera anexável em Play");
  AE_EXPECT_TRUE(camera.valid(), "instância devolvida");
  world.addComponent(handle, "astra.physics.body", status);
  AE_EXPECT_EQ(static_cast<u32>(status), static_cast<u32>(WorldStatus::NotMutableInPlay),
               "corpo físico não entra durante a execução");
  world.addComponent(handle, "astra.inexistente", status);
  AE_EXPECT_EQ(static_cast<u32>(status), static_cast<u32>(WorldStatus::UnknownComponent), "tipo desconhecido");
  world.addComponent(handle, "astra.camera", status);
  AE_EXPECT_EQ(static_cast<u32>(status), static_cast<u32>(WorldStatus::ComponentUnavailable),
               "componente singular não duplica");

  // Olhar exige Câmera: a regra vem do schema, não de um caso especial aqui.
  const auto look = world.addComponent(handle, "astra.camera.look", status);
  AE_EXPECT_EQ(static_cast<u32>(status), 0u, "olhar com câmera presente");
  AE_EXPECT_EQ(static_cast<u32>(world.removeComponent(camera)), static_cast<u32>(WorldStatus::ComponentInUse),
               "remover a câmera quebraria a exigência do olhar");
  AE_EXPECT_EQ(static_cast<u32>(world.removeComponent(look)), 0u, "remoção do olhar enfileirada");
  AE_EXPECT_EQ(world.flush(), 1u, "ponto seguro remove o componente");
  AE_EXPECT_EQ(static_cast<u32>(world.removeComponent(camera)), 0u, "câmera liberada depois");
  world.flush();
  AE_EXPECT_EQ(world.componentCount(handle), 0u, "objeto sem componentes");
}

AE_TEST(runtime_world_property_access_is_typed_and_addresses_each_instance) {
  EditorDocument doc;
  const auto id = named(doc, doc.root(), "Objeto");
  auto values = *doc.find(id);
  auto *first = static_cast<scene::Collider *>(values.components.add(scene::Collider::descriptor));
  auto *second = static_cast<scene::Collider *>(values.components.add(scene::Collider::descriptor));
  AE_EXPECT_TRUE(first && second, "dois colisores autorados");
  second->halfX = 2;
  const auto secondInstance = second->instanceId();
  AE_EXPECT_TRUE(doc.applyEntityValues(id, values), "autoria aceita");

  GameWorld world;
  AE_EXPECT_TRUE(world.load(doc), "carga");
  const auto handle = world.handle(id);
  const auto a = world.findComponent(handle, "astra.physics.collider", 0);
  const auto b = world.findComponent(handle, "astra.physics.collider", 1);
  AE_EXPECT_TRUE(a.valid() && b.valid() && a.instance != b.instance, "instâncias distintas endereçáveis");
  AE_EXPECT_EQ(b.instance, secondInstance, "ordinal segue a ordem autorada");

  scene::ComponentPropertyValue value;
  AE_EXPECT_EQ(static_cast<u32>(world.getProperty(b, "half_x", value)), 0u, "leitura por instância");
  AE_EXPECT_EQ(std::get<float>(value), 2.f, "valor da segunda instância");
  AE_EXPECT_EQ(static_cast<u32>(world.setProperty(a, "half_x", 3.0f)), 0u, "escrita por instância");
  AE_EXPECT_EQ(static_cast<u32>(world.getProperty(a, "half_x", value)), 0u, "releitura");
  AE_EXPECT_EQ(std::get<float>(value), 3.f, "somente a primeira mudou");
  AE_EXPECT_EQ(static_cast<u32>(world.getProperty(b, "half_x", value)), 0u, "releitura da segunda");
  AE_EXPECT_EQ(std::get<float>(value), 2.f, "segunda preservada");

  AE_EXPECT_EQ(static_cast<u32>(world.setProperty(a, "half_x", true)), static_cast<u32>(WorldStatus::InvalidArgument),
               "tipo incompatível é recusado");
  AE_EXPECT_EQ(static_cast<u32>(world.setProperty(a, "half_x", 1e9f)), static_cast<u32>(WorldStatus::Rejected),
               "valor fora dos limites do descritor é recusado");
  AE_EXPECT_EQ(static_cast<u32>(world.setProperty(a, "nao_existe", 1.0f)), static_cast<u32>(WorldStatus::InvalidArgument),
               "propriedade desconhecida");
  AE_EXPECT_EQ(static_cast<u32>(world.setProperty(a, "owner", scene::ObjectReference{999})),
               static_cast<u32>(WorldStatus::UnknownObject), "referência para objeto inexistente é recusada");
}

AE_TEST(runtime_world_transform_authority_protects_simulated_poses) {
  EditorDocument doc;
  const auto id = named(doc, doc.root(), "Corpo");
  const auto child = named(doc, id, "Marca");
  GameWorld world;
  AE_EXPECT_TRUE(world.load(doc), "carga");
  const auto handle = world.handle(id);
  runtime::Transform value;
  AE_EXPECT_EQ(static_cast<u32>(world.localTransform(handle, value)), 0u, "leitura");
  AE_EXPECT_EQ(static_cast<u32>(world.setLocalTransform(handle, value)), 0u, "objeto livre aceita escrita");

  world.setAuthority(child, TransformAuthority::PhysicsBody);
  AE_EXPECT_EQ(static_cast<u32>(world.setLocalTransform(handle, value)),
               static_cast<u32>(WorldStatus::TransformOwnedByPhysics),
               "mover um ancestral de pose simulada é recusado com motivo");
  world.clearAuthorities();
  AE_EXPECT_EQ(static_cast<u32>(world.setLocalTransform(handle, value)), 0u, "autoridade liberada");
}

AE_TEST(runtime_world_world_transform_composes_and_decomposes_through_the_hierarchy) {
  EditorDocument doc;
  const auto parent = named(doc, doc.root(), "Pai");
  auto values = *doc.find(parent);
  values.transform.position[0] = 5;
  values.transform.rotationDegrees[1] = 90;
  AE_EXPECT_TRUE(doc.applyEntityValues(parent, values), "pai posicionado");
  const auto child = named(doc, parent, "Filho");
  values = *doc.find(child);
  values.transform.position[2] = 2;
  AE_EXPECT_TRUE(doc.applyEntityValues(child, values), "filho deslocado");

  GameWorld world;
  AE_EXPECT_TRUE(world.load(doc), "carga");
  const auto handle = world.handle(child);
  runtime::Transform world1;
  AE_EXPECT_EQ(static_cast<u32>(world.worldTransform(handle, world1)), 0u, "transform de mundo");
  AE_EXPECT_TRUE(std::abs(world1.position[0] - 7) < .001f, "rotação do pai aplicada ao deslocamento do filho");

  runtime::Transform target = world1;
  target.position[0] = 10;
  AE_EXPECT_EQ(static_cast<u32>(world.setWorldTransform(handle, target)), 0u, "escrita em espaço de mundo");
  AE_EXPECT_EQ(static_cast<u32>(world.worldTransform(handle, world1)), 0u, "releitura");
  AE_EXPECT_TRUE(std::abs(world1.position[0] - 10) < .001f, "ida e volta preserva o alvo de mundo");
}

AE_TEST(runtime_play_scene_runs_on_the_world_and_stop_preserves_authoring) {
  EditorDocument doc;
  const auto id = named(doc, doc.root(), "Caixa");
  auto values = *doc.find(id);
  values.transform.position[1] = 6;
  editPhysicsBody(values)->motion = scene::BodyMotion::Dynamic;
  editCollider(values);
  AE_EXPECT_TRUE(doc.applyEntityValues(id, values), "corpo dinâmico autorado");
  const auto revision = doc.revision();

  EditorMapScene resources;
  EditorPlayScene play;
  AE_EXPECT_TRUE(play.start(doc, resources), "Play inicia sobre o mundo de execução");
  AE_EXPECT_TRUE(play.world().running() && play.world().worldId() != 0, "mundo ativo");
  for (int step = 0; step < 30; ++step) AE_EXPECT_TRUE(play.advance(1. / 60), "passo de simulação");
  AE_EXPECT_TRUE(play.document().find(id)->transform.position[1] < 6.f, "a queda aparece no mundo");
  AE_EXPECT_EQ(doc.find(id)->transform.position[1], 6.f, "documento autoral intacto durante o Play");
  AE_EXPECT_EQ(doc.revision(), revision, "nenhuma revisão autoral durante o Play");

  const auto worldId = play.world().worldId();
  play.stop();
  AE_EXPECT_TRUE(!play.active() && !play.executionGraph(), "Stop encerra o mundo");
  AE_EXPECT_EQ(doc.find(id)->transform.position[1], 6.f, "autoria preservada depois do Stop");
  AE_EXPECT_TRUE(play.start(doc, resources), "novo Play");
  AE_EXPECT_TRUE(play.world().worldId() != worldId, "o segundo Play tem identidade própria");
}

AE_TEST(runtime_play_scene_releases_physics_bodies_destroyed_at_the_safe_point) {
  EditorDocument doc;
  const auto id = named(doc, doc.root(), "Caixa");
  auto values = *doc.find(id);
  values.transform.position[1] = 6;
  editPhysicsBody(values)->motion = scene::BodyMotion::Dynamic;
  editCollider(values);
  AE_EXPECT_TRUE(doc.applyEntityValues(id, values), "corpo dinâmico");

  EditorMapScene resources;
  EditorPlayScene play;
  AE_EXPECT_TRUE(play.start(doc, resources), "Play");
  auto &world = play.world();
  const auto handle = world.handle(id);
  AE_EXPECT_EQ(static_cast<u32>(world.destroyObject(handle)), 0u, "remoção durante a execução");
  AE_EXPECT_TRUE(play.advance(1. / 60), "o passo seguinte aplica o ponto seguro");
  AE_EXPECT_TRUE(!world.graph().exists(id), "objeto removido");
  for (int step = 0; step < 10; ++step)
    AE_EXPECT_TRUE(play.advance(1. / 60), "simulação continua sem o corpo removido");
}

AE_TEST(runtime_schema_matches_the_editor_catalog_rules) {
  // O catálogo do inspetor e a API C# leem o MESMO schema: se esta checagem
  // falhar, uma das duas listas voltou a existir em separado.
  for (const auto &schema : scene::componentSchemas) {
    AE_EXPECT_TRUE(schema.type && !schema.type->id.empty(), "entrada com descritor");
    AE_EXPECT_TRUE(scene::findComponentSchema(schema.type->id) == &schema, "busca por id devolve a própria entrada");
  }
  scene::Components components;
  const auto *look = scene::findComponentSchema("astra.camera.look");
  const auto *camera = scene::findComponentSchema("astra.camera");
  const auto *body = scene::findComponentSchema("astra.physics.body");
  const auto *character = scene::findComponentSchema("astra.physics.character");
  AE_EXPECT_TRUE(look && camera && body && character, "entradas nativas presentes");
  AE_EXPECT_TRUE(scene::componentUnavailableReason(*look, components), "olhar exige câmera");
  AE_EXPECT_TRUE(!scene::componentUnavailableReason(*camera, components), "câmera sempre anexável");
  AE_EXPECT_TRUE(components.add(*camera->type), "câmera adicionada");
  AE_EXPECT_TRUE(!scene::componentUnavailableReason(*look, components), "olhar liberado com câmera");
  AE_EXPECT_TRUE(components.add(*character->type), "personagem adicionado");
  AE_EXPECT_TRUE(scene::componentUnavailableReason(*body, components), "corpo é incompatível com personagem");
}
