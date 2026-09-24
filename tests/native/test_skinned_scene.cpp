// Skin e animação na cena (G6-B): componentes, instanciação da importação,
// paleta na extração e reprodução no Play, sem GPU.
#include "harness.h"
#include "skinned_glb_fixture.h"
#include "editor/editor_play_scene.h"
#include "editor/editor_session.h"
#include "renderer/authoring_geometry.h"
#include "scene/animation.h"
#include "scene/skinned_mesh.h"

#include <cmath>
#include <sstream>

using namespace ae;
using namespace ae::editor;

namespace {
bool near(float a, float b, float tolerance = 1e-3f) { return std::fabs(a - b) <= tolerance; }

// O consumidor gráfico sem Vulkan: primitivas internas e depois a biblioteca,
// como o renderer. Guarda o que recebeu de skin durante a publicação.
struct SkinningPublisher {
  std::vector<u8> vertices;
  std::vector<u32> indices;
  std::vector<renderer::MapDrawRecord> draws;
  std::vector<renderer::MapMaterialRecord> materials;
  std::vector<u32> drawJoints;
  usize influenceBytes = 0;
};

void startSession(EditorSession &session, SkinningPublisher &gpu) {
  std::vector<u8> vertices;
  std::vector<u32> indices;
  std::vector<renderer::MapDrawRecord> draws;
  std::vector<renderer::MapMaterialRecord> materials;
  AE_EXPECT_TRUE(renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride, vertices, indices, draws, materials),
                 "primitivas internas");
  AE_EXPECT_TRUE(session.importMap(draws, materials, false, vertices, indices, 0), "biblioteca inicial");
  session.setGeometryPublisher([&gpu, &session](std::span<const u8> v, std::span<const u32> i,
                                                std::span<const renderer::MapDrawRecord> d,
                                                std::span<const renderer::MapMaterialRecord> m,
                                                std::span<const renderer::SharedAuthoringTexture>,
                                                EditorSession::PublishedGeometry &out) {
    gpu.vertices.clear(); gpu.indices.clear(); gpu.draws.clear(); gpu.materials.clear();
    if (!renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride, gpu.vertices, gpu.indices, gpu.draws, gpu.materials))
      return false;
    const auto vertexBase = static_cast<u32>(gpu.vertices.size() / renderer::MapVertexStride);
    const auto indexBase = static_cast<u32>(gpu.indices.size());
    const auto materialBase = static_cast<u32>(gpu.materials.size());
    gpu.vertices.insert(gpu.vertices.end(), v.begin(), v.end());
    gpu.indices.insert(gpu.indices.end(), i.begin(), i.end());
    gpu.materials.insert(gpu.materials.end(), m.begin(), m.end());
    for (auto draw : d) {
      draw.firstIndex += indexBase; draw.vertexOffset += vertexBase; draw.materialIndex += materialBase;
      draw.lodGroupId = static_cast<u32>(gpu.draws.size());
      gpu.draws.push_back(draw);
    }
    const auto &skin = session.skinningPublication();
    gpu.drawJoints.assign(skin.drawJoints.begin(), skin.drawJoints.end());
    gpu.influenceBytes = skin.influences.size();
    out = {gpu.draws, gpu.materials, gpu.vertices, gpu.indices};
    return true;
  });
}

EditorEntityId named(const runtime::SceneGraph &graph, const char *name) {
  std::vector<EditorEntityId> ids;
  graph.collectSubtree(graph.root(), ids);
  for (const auto id : ids) if (std::string(graph.find(id)->name) == name) return id;
  return kInvalidEntity;
}

const renderer::MapDrawState *drawOf(const std::vector<renderer::MapDrawState> &draws, EditorEntityId id) {
  for (const auto &draw : draws) if (draw.objectId == id) return &draw;
  return nullptr;
}
} // namespace

AE_TEST(skinned_mesh_and_animation_components_round_trip_without_runtime_state) {
  scene::SkinnedMesh skinned;
  skinned.bones = {5, 0, 7};
  skinned.quality = scene::SkinQuality::Two;
  skinned.skinnedMotionVectors = false;
  std::stringstream text;
  skinned.write(text);
  scene::SkinnedMesh restored;
  AE_EXPECT_TRUE(restored.read(text, 1) && restored.bones == skinned.bones && restored.influences() == 2 &&
                 !restored.skinnedMotionVectors, "ossos, qualidade e vetor de movimento voltam");
  std::stringstream broken("3 1 0");
  AE_EXPECT_TRUE(!restored.read(broken, 1), "qualidade fora de Auto/1/2/4 recusada");

  scene::Animation animation;
  animation.clip = 2;
  animation.playAutomatically = false;
  animation.wrapMode = resources::AnimationWrapMode::PingPong;
  animation.speed = -.5f;
  animation.playing = true;
  animation.time = 3;
  std::stringstream saved;
  animation.write(saved);
  scene::Animation loaded;
  AE_EXPECT_TRUE(loaded.read(saved, 1) && loaded.clipIndex() == 2 && !loaded.playAutomatically &&
                 loaded.wrapMode == resources::AnimationWrapMode::PingPong && near(loaded.speed, -.5f),
                 "clipe, início automático, repetição e velocidade gravados");
  AE_EXPECT_TRUE(!loaded.playing && loaded.time == 0, "tocando e tempo são execução: nunca vão ao arquivo");
  loaded.clip = 1.5f;
  AE_EXPECT_TRUE(!loaded.valid(), "índice de clipe fracionário recusado");
}

AE_TEST(imported_skinned_glb_instantiates_bones_and_animation_and_plays_in_play) {
  EditorSession session;
  SkinningPublisher gpu;
  startSession(session, gpu);
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(session.importModel(test::skinnedAnimatedGlb(), "Fontes/rig.glb", {}, report), report.diagnostic.c_str());
  AE_EXPECT_TRUE(report.skinnedMeshes == 1 && report.animations == 1, "malha com esqueleto e animação criadas");
  AE_EXPECT_TRUE(gpu.influenceBytes == 4 * resources::SkinInfluenceStride,
                 "o publicador recebe as influências dos 4 vértices do corpo, paralelas à biblioteca");
  AE_EXPECT_TRUE(!gpu.drawJoints.empty() && gpu.drawJoints.back() == 2, "o desenho do corpo publica 2 juntas");

  const auto &doc = session.document();
  const auto rig = named(doc, "Rig"), hip = named(doc, "Hip"), arm = named(doc, "Arm"), body = named(doc, "Body");
  AE_EXPECT_TRUE(rig && hip && arm && body, "a árvore do arquivo virou objetos");
  const auto *skinned = static_cast<const scene::SkinnedMesh *>(doc.find(body)->components.find(scene::SkinnedMesh::descriptor));
  AE_EXPECT_TRUE(skinned && skinned->bones.size() == 2 && skinned->bones[0] == hip && skinned->bones[1] == arm,
                 "ossos ligados aos objetos das juntas, na ordem do skin");
  AE_EXPECT_TRUE(doc.find(rig)->components.find(scene::Animation::descriptor) != nullptr, "a raiz da instância toca o clipe");

  std::vector<renderer::MapDrawState> rest;
  AE_EXPECT_TRUE(session.extractMap(rest), "extração do documento");
  const auto *restDraw = drawOf(rest, body);
  AE_EXPECT_TRUE(restDraw && restDraw->skinPalette && restDraw->skinPalette->size() == 32, "paleta de duas juntas");
  if (!restDraw || !restDraw->skinPalette) return;
  bool identity = true;
  for (u32 j = 0; j < 2; ++j)
    for (u32 k = 0; k < 16; ++k)
      identity = identity && near((*restDraw->skinPalette)[j * 16 + k], (k % 5 == 0) ? 1.0f : 0.0f);
  AE_EXPECT_TRUE(identity, "na pose de bind a paleta é identidade");
  const float restCenterZ = restDraw->pose.draw.boundsCenter[2];

  EditorPlayScene play;
  AE_EXPECT_TRUE(play.start(session.document(), session.mapScene()), "Play inicia");
  // `advance` limita o passo a 0,25 s: duas passadas = 0,5 s do clipe.
  AE_EXPECT_TRUE(play.advance(.25) && play.advance(.25), "dois passos");
  const auto &live = play.document();
  AE_EXPECT_TRUE(near(live.find(arm)->transform.rotationDegrees[2], 45, .05f), "Arm a 45° no meio do LINEAR");
  AE_EXPECT_TRUE(near(live.find(hip)->transform.position[2], 2), "Hip saltou no STEP em 0,5 s");
  AE_EXPECT_TRUE(near(live.find(hip)->transform.scale[0], 1.5f), "escala CUBICSPLINE no meio");
  AE_EXPECT_TRUE(play.animator().playingCount() == 1, "um clipe tocando");
  std::vector<renderer::MapDrawState> posed;
  AE_EXPECT_TRUE(play.extract(session.mapScene(), posed), "extração do Play");
  const auto *posedDraw = drawOf(posed, body);
  AE_EXPECT_TRUE(posedDraw && posedDraw->skinPalette && !near((*posedDraw->skinPalette)[16], 1.0f),
                 "a paleta do Arm segue a rotação");
  AE_EXPECT_TRUE(posedDraw && posedDraw->pose.draw.boundsCenter[2] > restCenterZ + 1.5f,
                 "os limites acompanham o corpo deformado");

  // Stop por script: a pose fica onde está.
  auto &world = play.world();
  const auto owner = world.findComponent(world.handle(rig), "astra.animation");
  AE_EXPECT_TRUE(world.setProperty(owner, "playing", scene::ComponentPropertyValue{false}) == runtime::WorldStatus::Ok,
                 "Stop pela API de propriedades");
  const float frozen = live.find(arm)->transform.rotationDegrees[2];
  AE_EXPECT_TRUE(play.advance(.25), "passo parado");
  AE_EXPECT_TRUE(near(live.find(arm)->transform.rotationDegrees[2], frozen), "parado não anima");

  // Once: passa do fim, para e volta ao início (WrapMode.Once da Unity).
  AE_EXPECT_TRUE(world.setProperty(owner, "wrap_mode", scene::ComponentPropertyValue{0u}) == runtime::WorldStatus::Ok &&
                 world.setProperty(owner, "time", scene::ComponentPropertyValue{.9f}) == runtime::WorldStatus::Ok &&
                 world.setProperty(owner, "playing", scene::ComponentPropertyValue{true}) == runtime::WorldStatus::Ok,
                 "Once a partir de 0,9 s");
  AE_EXPECT_TRUE(play.advance(.25), "passa do fim");
  scene::ComponentPropertyValue playing, time;
  AE_EXPECT_TRUE(world.getProperty(owner, "playing", playing) == runtime::WorldStatus::Ok &&
                 world.getProperty(owner, "time", time) == runtime::WorldStatus::Ok &&
                 !std::get<bool>(playing) && std::get<float>(time) == 0, "Once terminou e rebobinou");
  AE_EXPECT_TRUE(near(live.find(arm)->transform.rotationDegrees[2], 90, .05f), "a última pose amostrada é a do fim");

  play.stop();
  AE_EXPECT_TRUE(near(session.document().find(arm)->transform.rotationDegrees[2], 0) &&
                 near(session.document().find(hip)->transform.position[2], 0), "o documento autoral não mudou");
}

AE_TEST(a_missing_bone_keeps_its_vertices_in_the_bind_pose) {
  EditorSession session;
  SkinningPublisher gpu;
  startSession(session, gpu);
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(session.importModel(test::skinnedAnimatedGlb(), "Fontes/rig.glb", {}, report), report.diagnostic.c_str());
  auto &doc = session.document();
  const auto body = named(doc, "Body"), arm = named(doc, "Arm");
  auto values = *doc.find(body);
  auto *skinned = static_cast<scene::SkinnedMesh *>(values.components.edit(scene::SkinnedMesh::descriptor));
  AE_EXPECT_TRUE(skinned != nullptr, "componente presente");
  if (!skinned) return;
  skinned->bones[1] = 0;
  AE_EXPECT_TRUE(session.history().applyValues(doc, body, values), "osso desligado");
  auto armValues = *doc.find(arm);
  armValues.transform.rotationDegrees[2] = 90;
  AE_EXPECT_TRUE(session.history().applyValues(doc, arm, armValues), "Arm girado no editor");
  std::vector<renderer::MapDrawState> draws;
  AE_EXPECT_TRUE(session.extractMap(draws), "extração");
  const auto *draw = drawOf(draws, body);
  AE_EXPECT_TRUE(draw && draw->skinPalette && near((*draw->skinPalette)[16], 1) && near((*draw->skinPalette)[16 + 12], 0),
                 "junta sem osso usa a paleta identidade, não a pose do objeto solto");
  if (!draw) return;
  u32 missing = 0;
  std::vector<float> palette;
  float center[3], radius = 0;
  AE_EXPECT_TRUE(session.mapScene().skinPose(doc, *skinned, draw->sourceDrawIndex, draw->pose.draw.model,
                                             palette, center, radius, &missing) && missing == 1,
                 "a ausência é contada para o Inspector");
}
