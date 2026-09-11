#include "harness.h"
#include "editor/editor_document.h"
#include "editor/editor_physics_body.h"
#include "editor/editor_play_scene.h"
#include "scene/script_behavior.h"

#include <cstring>
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

  static void reset() {
    starts = updates = fixedUpdates = stops = triggers = contacts = 0;
    lastContactFirst = lastContactSecond = 0;
    lastContactPhase = 99;
    lastContactHadNormal = false;
    attachments.clear();
  }
  static int start(const u8 *, int, const u8 *json, int length, const scene::ScriptSceneAccess *access) {
    ++starts;
    attachments.assign(reinterpret_cast<const char *>(json), static_cast<usize>(length));
    // A ABI precisa chegar completa: um ponteiro faltando aqui é um campo que o
    // lado gerenciado usaria sem existir.
    return access && access->available() ? 0 : 1;
  }
  static int update(float) { ++updates; return 0; }
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

  EditorPlayScene play;
  AE_EXPECT_TRUE(play.start(doc, resources), "Play inicia");
  auto &world = play.world();
  const auto handle = world.handle(id);
  const auto mesh = world.findComponent(handle, "astra.render.mesh");
  AE_EXPECT_TRUE(mesh.valid(), "componente de malha endereçável");

  // Exatamente o que `Component.SetFloat` faz do lado C#.
  AE_EXPECT_EQ(static_cast<u32>(world.setProperty(mesh, "base_color.r", .10f)), 0u, "R");
  AE_EXPECT_EQ(static_cast<u32>(world.setProperty(mesh, "base_color.g", .90f)), 0u, "G");
  AE_EXPECT_EQ(static_cast<u32>(world.setProperty(mesh, "base_color.b", .20f)), 0u, "B");

  std::vector<renderer::MapDrawState> draws;
  AE_EXPECT_TRUE(play.extract(resources, draws), "extração do mundo em execução");
  AE_EXPECT_EQ(draws.size(), 1u, "um desenho");
  // Escrever uma propriedade de material LIGA o override: sem isso o valor
  // ficaria guardado e o desenho continuaria usando o material do pacote.
  AE_EXPECT_TRUE(draws[0].material.enabled, "override ativado pela escrita");
  AE_EXPECT_EQ(draws[0].material.baseColor[1], .90f, "verde escrito pelo código");
  const auto applied = renderer::applyMaterialOverride(material, draws[0].material);
  AE_EXPECT_EQ(applied.baseColorFactor[1], .90f, "o override chega ao registro de material do desenho");

  // E o documento autoral continua com o material do pacote.
  play.stop();
  AE_EXPECT_TRUE(!meshMaterial(*doc.find(id)).enabled, "autoria preservada depois do Stop");
}
