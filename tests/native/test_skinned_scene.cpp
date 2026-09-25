// Deformação e animação na cena (G6-B): componentes, instanciação e
// reimportação da fonte, pose deformada na extração e na seleção, estados de
// animação com cross-fade e camadas, blend shapes e a ABI de animação — tudo
// sem GPU, com o GLB sintético e três modelos oficiais da Khronos.
#include "harness.h"
#include "skinned_glb_fixture.h"
#include "editor/editor_play_scene.h"
#include "editor/editor_archive.h"
#include "editor/editor_session.h"
#include "renderer/authoring_geometry.h"
#include "runtime/transform_math.h"
#include "scene/animation.h"
#include "scene/script_behavior.h"
#include "scene/skinned_mesh.h"
#include "resources/import_cache.h"

#include <cmath>
#include <fstream>
#include <iterator>
#include <sstream>

using namespace ae;
using namespace ae::editor;

namespace {
bool near(float a, float b, float tolerance = 1e-3f) { return std::fabs(a - b) <= tolerance; }

std::vector<u8> fixture(const char *name) {
  std::ifstream input(std::string(AETHER_REPOSITORY_ROOT) + "/tests/native/fixtures/gltf/" + name, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

// O consumidor gráfico sem Vulkan: primitivas internas e depois a biblioteca,
// como o renderer. Guarda o que recebeu de deformação durante a publicação.
struct DeformationPublisher {
  std::vector<u8> vertices;
  std::vector<u32> indices;
  std::vector<renderer::MapDrawRecord> draws;
  std::vector<renderer::MapMaterialRecord> materials;
  std::vector<u32> drawJoints, drawMorphTargets, drawMorphOffsets;
  usize influenceBytes = 0, morphFloats = 0;
};

void startSession(EditorSession &session, DeformationPublisher &gpu) {
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
    gpu.drawMorphTargets.assign(skin.drawMorphTargets.begin(), skin.drawMorphTargets.end());
    gpu.drawMorphOffsets.assign(skin.drawMorphOffsets.begin(), skin.drawMorphOffsets.end());
    gpu.influenceBytes = skin.influences.size();
    gpu.morphFloats = skin.morphDeltas.size();
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

template <class T> const T *component(const runtime::SceneGraph &graph, EditorEntityId id) {
  const auto *object = graph.find(id);
  return object ? static_cast<const T *>(object->components.find(T::descriptor)) : nullptr;
}

EditorEntityId withComponent(const runtime::SceneGraph &graph, const scene::ComponentType &type) {
  std::vector<EditorEntityId> ids;
  graph.collectSubtree(graph.root(), ids);
  for (const auto id : ids) if (graph.find(id)->components.find(type)) return id;
  return kInvalidEntity;
}

const renderer::MapDrawState *drawOf(const std::vector<renderer::MapDrawState> &draws, EditorEntityId id) {
  for (const auto &draw : draws) if (draw.objectId == id) return &draw;
  return nullptr;
}

float quaternionAngle(const float a[4], const float b[4]) {
  const float dot = std::fabs(a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]);
  return 2 * std::acos(std::min(dot, 1.0f));
}
void rotationOf(const runtime::SceneGraph &graph, EditorEntityId id, float q[4]) {
  runtime::transformRotationQuaternion(graph.find(id)->transform, q);
}
} // namespace

AE_TEST(deformation_and_animation_components_round_trip_and_migrate_v1) {
  scene::SkinnedMesh mesh;
  mesh.bones = {5, 0, 7};
  mesh.blendShapeWeights = {0, 37.5f, 100};
  mesh.quality = scene::SkinQuality::Two;
  mesh.skinnedMotionVectors = false;
  std::stringstream text;
  mesh.write(text);
  scene::SkinnedMesh restored;
  AE_EXPECT_TRUE(restored.read(text, 2) && restored.bones == mesh.bones && restored.blendShapeWeights == mesh.blendShapeWeights &&
                 restored.influences() == 2 && !restored.skinnedMotionVectors, "ossos, blend shapes e qualidade voltam");
  std::stringstream v1("4 1 2 9 10");
  AE_EXPECT_TRUE(restored.read(v1, 1) && restored.bones.size() == 2 && restored.blendShapeWeights.empty(),
                 "v1 (só ossos) continua legível");
  std::stringstream broken("3 1 0 0");
  AE_EXPECT_TRUE(!restored.read(broken, 2), "qualidade fora de Auto/1/2/4 recusada");
  mesh.blendShapeWeights[1] = std::nanf("");
  AE_EXPECT_TRUE(!mesh.valid(), "peso não finito recusado");

  scene::Animation animation;
  const resources::AssetGuid walk{1, 2}, run{3, 4};
  animation.appendClip(walk);animation.appendClip(run);
  animation.clip = run;
  animation.playAutomatically = false;
  animation.wrapMode = resources::AnimationWrapMode::PingPong;
  animation.speed = -.5f;
  std::stringstream saved;
  animation.write(saved);
  scene::Animation loaded;
  AE_EXPECT_TRUE(loaded.read(saved, 3) && loaded.clips == animation.clips && loaded.clip == run && !loaded.playAutomatically &&
                 loaded.wrapMode == resources::AnimationWrapMode::PingPong && near(loaded.speed, -.5f),
                 "lista de clipes, padrão, repetição e velocidade");
  const auto runElement=loaded.clips[1].id;
  AE_EXPECT_TRUE(loaded.moveClip(runElement,0) && loaded.clips[0].id==runElement && loaded.clips[0].asset==run,
                 "reordenação conserva a identidade e o recurso da entrada");
  std::stringstream reordered;loaded.write(reordered);
  scene::Animation reopened;
  AE_EXPECT_TRUE(reopened.read(reordered,3) && reopened.clips==loaded.clips,"arquivo v3 conserva ordem e IDs");
  AE_EXPECT_TRUE(reopened.removeClip(runElement) && reopened.appendClip()>runElement,
                 "remoção não reutiliza identidade antiga");
  std::stringstream v2;v2<<run.text()<<" 0 2 -0.5 2 "<<walk.text()<<' '<<run.text();
  scene::Animation migrated;
  AE_EXPECT_TRUE(migrated.read(v2,2) && migrated.clips.size()==2 && migrated.clips[0].id==1 &&
                 migrated.clips[1].id==2 && migrated.clips[1].asset==run,"arquivo v2 ganha IDs estáveis");
  std::stringstream duplicate;duplicate<<run.text()<<" 0 2 -0.5 2 3 1 "<<walk.text()<<" 1 "<<run.text();
  scene::Animation invalid;
  AE_EXPECT_TRUE(!invalid.read(duplicate,3),"arquivo v3 com IDs duplicados é recusado");
  std::stringstream legacy("2 1 1 1");
  AE_EXPECT_TRUE(loaded.read(legacy, 1) && loaded.legacyClipIndex == 2 && loaded.clips.empty(),
                 "v1 guarda o índice até o editor traduzi-lo");
  // O tamanho da lista é uma propriedade: aumentar abre entradas vazias.
  scene::Components components;
  components.add(scene::Animation::descriptor);
  AE_EXPECT_TRUE(scene::setComponentProperty(components, "astra.animation", "clip_count", scene::ComponentPropertyValue{3u}) ==
                     scene::ComponentPropertyStatus::Applied &&
                 static_cast<const scene::Animation *>(components.find(scene::Animation::descriptor))->clips.size() == 3,
                 "clip_count redimensiona a lista");
}

AE_TEST(animation_clip_element_ids_survive_scene_save_and_reload) {
  EditorDocument document;
  const auto owner=document.createEntity(document.root(),EditorEntityKind::Folder,"Animated");
  AE_EXPECT_TRUE(owner!=kInvalidEntity,"objeto criado");
  if(owner==kInvalidEntity) return;
  auto values=*document.find(owner);
  auto *animation=static_cast<scene::Animation *>(values.components.add(scene::Animation::descriptor));
  AE_EXPECT_TRUE(animation!=nullptr,"componente criado");
  if(!animation) return;
  const resources::AssetGuid first{11,12},second{21,22};
  const u64 firstId=animation->appendClip(first),removedId=animation->appendClip(second);
  AE_EXPECT_TRUE(animation->removeClip(removedId),"entrada removida");
  const u64 newId=animation->appendClip(second);
  AE_EXPECT_TRUE(newId>removedId && animation->moveClip(newId,0),"ID novo reordenado");
  AE_EXPECT_TRUE(document.applyEntityValues(owner,values),"componente aplicado ao documento");
  EditorDocument restored;
  AE_EXPECT_TRUE(deserializeEditorDocument(serializeEditorDocument(document,17),17,restored),"cena reaberta");
  const auto *saved=component<scene::Animation>(restored,owner);
  AE_EXPECT_TRUE(saved && saved->clips.size()==2 && saved->clips[0].id==newId && saved->clips[0].asset==second &&
                 saved->clips[1].id==firstId && saved->clips[1].asset==first,
                 "cena conserva ordem, IDs e referências");
  if(saved) {
    auto next=*saved;
    AE_EXPECT_TRUE(next.appendClip()>newId,"cena reaberta não reutiliza ID removido");
  }
}

AE_TEST(imported_rig_binds_bones_and_clip_identity_and_plays) {
  EditorSession session;
  DeformationPublisher gpu;
  startSession(session, gpu);
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(session.importModel(test::skinnedAnimatedGlb(), "Fontes/rig.glb", {}, report), report.diagnostic.c_str());
  AE_EXPECT_EQ(report.skinnedMeshes, 1u, "malha deformável criada");
  AE_EXPECT_EQ(report.animations, 1u, "animação criada");
  AE_EXPECT_TRUE(gpu.influenceBytes == 4 * resources::SkinInfluenceStride && !gpu.drawJoints.empty() &&
                 gpu.drawJoints.back() == 2, "o publicador recebe influências e juntas do corpo");

  const auto &doc = session.document();
  const auto rig = named(doc, "Rig"), hip = named(doc, "Hip"), arm = named(doc, "Arm"), body = named(doc, "Body");
  const auto *mesh = component<scene::SkinnedMesh>(doc, body);
  AE_EXPECT_TRUE(mesh && mesh->bones.size() == 2 && mesh->bones[0] == hip && mesh->bones[1] == arm,
                 "ossos ligados aos objetos das juntas, na ordem do skin");
  const auto *animation = component<scene::Animation>(doc, rig);
  const auto wave = resources::animationClipGuid(report.source, "Wave", 0);
  AE_EXPECT_TRUE(animation && animation->clips.size() == 1 && animation->clips[0].asset == wave && animation->clip == wave,
                 "o clipe é referenciado pela identidade derivada da fonte e do nome");
  runtime::AnimationClipView view;
  AE_EXPECT_TRUE(session.mapScene().findClip(wave, view) && view.name == "Wave" && near(view.clip->duration, 1),
                 "a biblioteca resolve o clipe pela identidade");

  std::vector<renderer::MapDrawState> rest;
  AE_EXPECT_TRUE(session.extractMap(rest), "extração do documento");
  const auto *restDraw = drawOf(rest, body);
  AE_EXPECT_TRUE(restDraw && restDraw->skinPalette && restDraw->skinPalette->size() == 32 && !restDraw->morphWeights,
                 "paleta de duas juntas, sem blend shapes");
  if (!restDraw || !restDraw->skinPalette) return;
  const float restCenterZ = restDraw->pose.draw.boundsCenter[2];

  EditorPlayScene play;
  AE_EXPECT_TRUE(play.start(session.document(), session.mapScene()), "Play inicia");
  AE_EXPECT_TRUE(play.advance(.25) && play.advance(.25), "dois passos (0,5 s do clipe)");
  const auto &live = play.document();
  AE_EXPECT_TRUE(near(live.find(arm)->transform.rotationDegrees[2], 45, .05f), "Arm a 45° no meio do LINEAR");
  AE_EXPECT_TRUE(near(live.find(hip)->transform.position[2], 2), "Hip saltou no STEP em 0,5 s");
  AE_EXPECT_TRUE(near(live.find(hip)->transform.scale[0], 1.5f), "escala CUBICSPLINE no meio");
  std::vector<renderer::MapDrawState> posed;
  AE_EXPECT_TRUE(play.extract(session.mapScene(), posed), "extração do Play");
  const auto *posedDraw = drawOf(posed, body);
  AE_EXPECT_TRUE(posedDraw && posedDraw->pose.draw.boundsCenter[2] > restCenterZ + 1.5f, "limites acompanham o corpo");

  const auto instance = animation->instanceId();
  auto &animator = const_cast<runtime::SceneAnimator &>(play.animator());
  AE_EXPECT_TRUE(animator.stop(rig, instance, wave) == runtime::AnimationCommandStatus::Ok, "Stop pelo avaliador");
  const float frozen = live.find(arm)->transform.rotationDegrees[2];
  AE_EXPECT_TRUE(play.advance(.25) && near(live.find(arm)->transform.rotationDegrees[2], frozen), "parado não anima");

  // Once: passa do fim, para e rebobina; a última pose amostrada fica.
  runtime::AnimationStateView state;
  state.clip = wave;
  state.enabled = true;
  state.time = .9f;
  state.weight = 1;
  state.wrapMode = resources::AnimationWrapMode::Once;
  AE_EXPECT_TRUE(animator.setState(rig, instance, state) == runtime::AnimationCommandStatus::Ok, "estado escrito");
  AE_EXPECT_TRUE(play.advance(.25), "passa do fim");
  AE_EXPECT_TRUE(animator.state(rig, instance, wave, state) == runtime::AnimationCommandStatus::Ok && !state.enabled &&
                 state.time == 0, "Once terminou e rebobinou");
  AE_EXPECT_TRUE(near(live.find(arm)->transform.rotationDegrees[2], 90, .05f), "a última pose é a do fim");
  const resources::AssetGuid foreign{9, 9};
  AE_EXPECT_TRUE(animator.play(rig, instance, foreign) == runtime::AnimationCommandStatus::ClipNotInComponent,
                 "clipe fora da lista do componente é recusado");
  play.stop();
  AE_EXPECT_TRUE(near(session.document().find(arm)->transform.rotationDegrees[2], 0) &&
                 near(session.document().find(hip)->transform.position[2], 0), "o documento autoral não mudou");
}

AE_TEST(fox_cross_fades_and_layers_blend_three_clips_on_one_skeleton) {
  const auto bytes = fixture("Fox.glb");
  AE_EXPECT_TRUE(!bytes.empty(), "fixture Fox.glb");
  EditorSession session;
  DeformationPublisher gpu;
  startSession(session, gpu);
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(session.importModel(bytes, "Fontes/Fox.glb", {}, report), report.diagnostic.c_str());
  const auto &doc = session.document();
  const auto owner = withComponent(doc, scene::Animation::descriptor);
  const auto *animation = component<scene::Animation>(doc, owner);
  AE_EXPECT_TRUE(animation && animation->clips.size() == 3, "Survey, Walk e Run na lista");
  if (!animation || animation->clips.size() != 3) return;
  const auto survey = animation->clips[0].asset, walk = animation->clips[1].asset, run = animation->clips[2].asset;
  runtime::AnimationClipView view;
  AE_EXPECT_TRUE(session.mapScene().findClip(walk, view) && view.name == "Walk", "nomes do arquivo");
  const auto *mesh = component<scene::SkinnedMesh>(doc, withComponent(doc, scene::SkinnedMesh::descriptor));
  AE_EXPECT_TRUE(mesh && mesh->bones.size() == 24, "24 ossos ligados");
  if (mesh) for (const auto bone : mesh->bones) AE_EXPECT_TRUE(bone && doc.find(static_cast<EditorEntityId>(bone)), "todo osso existe");
  // Um osso que os três clipes giram: a cabeça.
  const auto head = named(doc, "b_Head_05");
  AE_EXPECT_TRUE(head != kInvalidEntity, "osso da cabeça");

  EditorPlayScene play;
  AE_EXPECT_TRUE(play.start(session.document(), session.mapScene()), "Play inicia");
  auto &animator = const_cast<runtime::SceneAnimator &>(play.animator());
  const auto instance = animation->instanceId();
  AE_EXPECT_TRUE(play.advance(.1), "primeiro quadro");
  AE_EXPECT_TRUE(animator.isPlaying(owner, instance, survey), "Tocar ao iniciar tocou o clipe padrão");

  // Cross-fade de 0,5 s: no meio, metade de cada; no fim, só Walk e Survey parado.
  AE_EXPECT_TRUE(animator.crossFade(owner, instance, walk, .5f) == runtime::AnimationCommandStatus::Ok, "cross-fade");
  AE_EXPECT_TRUE(play.advance(.25), "meio do fade");
  runtime::AnimationStateView a, b;
  animator.state(owner, instance, survey, a);
  animator.state(owner, instance, walk, b);
  AE_EXPECT_TRUE(near(a.weight, .5f, .01f) && near(b.weight, .5f, .01f) && a.enabled && b.enabled, "pesos 0,5 / 0,5");
  AE_EXPECT_TRUE(play.advance(.25) && play.advance(.05), "fim do fade");
  animator.state(owner, instance, survey, a);
  animator.state(owner, instance, walk, b);
  AE_EXPECT_TRUE(!a.enabled && a.time == 0 && near(b.weight, 1), "quem saiu parou e rebobinou");

  // Camadas: Run na camada 1 com peso 1 domina a propriedade; com 0,5 divide com Walk.
  const auto poseAt = [&](const resources::AssetGuid &clip, float time, float q[4]) {
    animator.stop(owner, instance, {});
    runtime::AnimationStateView only;
    only.clip = clip; only.enabled = true; only.time = time; only.weight = 1; only.speed = 0;
    animator.setState(owner, instance, only);
    play.advance(0);
    rotationOf(play.document(), head, q);
  };
  float walkPose[4], runPose[4], layered[4], half[4];
  poseAt(walk, .3f, walkPose);
  poseAt(run, .3f, runPose);
  AE_EXPECT_TRUE(quaternionAngle(walkPose, runPose) > .02f, "Walk e Run dão poses diferentes na cabeça");
  animator.stop(owner, instance, {});
  runtime::AnimationStateView base, top;
  base.clip = walk; base.enabled = true; base.time = .3f; base.weight = 1; base.speed = 0;
  top.clip = run; top.enabled = true; top.time = .3f; top.weight = 1; top.speed = 0; top.layer = 1;
  animator.setState(owner, instance, base);
  animator.setState(owner, instance, top);
  play.advance(0);
  rotationOf(play.document(), head, layered);
  AE_EXPECT_TRUE(quaternionAngle(layered, runPose) < 1e-3f, "camada de cima com peso 1 toma a propriedade");
  top.weight = .5f;
  animator.setState(owner, instance, top);
  play.advance(0);
  rotationOf(play.document(), head, half);
  const float total = quaternionAngle(walkPose, runPose);
  AE_EXPECT_TRUE(near(quaternionAngle(half, walkPose), total * .5f, total * .1f) &&
                 near(quaternionAngle(half, runPose), total * .5f, total * .1f), "peso 0,5 na camada de cima fica no meio");
  play.stop();
}

AE_TEST(morph_cube_blend_shapes_animate_extract_and_bound) {
  const auto bytes = fixture("AnimatedMorphCube.glb");
  AE_EXPECT_TRUE(!bytes.empty(), "fixture AnimatedMorphCube.glb");
  resources::GltfImport model;
  AE_EXPECT_TRUE(resources::importGlb(bytes, {}, {}, model), model.diagnostic.c_str());
  AE_EXPECT_TRUE(model.morphs.size() == 1 && model.morphs[0].targetCount == 2 && model.drawMorphs.size() == model.draws.size() &&
                 model.drawMorphs[0] == 0, "dois alvos no desenho do cubo");
  AE_EXPECT_TRUE(model.animations.size() == 1 && model.animations[0].channels.size() == 1 &&
                 model.animations[0].channels[0].path == resources::AnimationPath::Weights &&
                 model.animations[0].channels[0].weightCount == 2 && model.unsupportedAnimationChannels == 0,
                 "o canal de pesos é importado, não descartado");
  AE_EXPECT_TRUE(model.morphs[0].maximumDisplacement[0] > 0 && model.morphs[0].maximumDisplacement[1] > 0,
                 "deslocamento máximo de cada alvo medido");
  std::vector<u8> cache;
  resources::GltfImport restored;
  AE_EXPECT_TRUE(resources::writeImportCache(model, "morph", cache) && resources::readImportCache(cache, "morph", restored) &&
                 restored.morphs.size() == 1 && restored.morphs[0].deltas == model.morphs[0].deltas &&
                 restored.drawMorphs == model.drawMorphs && restored.animations[0].channels[0].weightCount == 2,
                 "blend shapes e canal de pesos sobrevivem ao cache");

  EditorSession session;
  DeformationPublisher gpu;
  startSession(session, gpu);
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(session.importModel(bytes, "Fontes/cubo.glb", {}, report), report.diagnostic.c_str());
  AE_EXPECT_TRUE(gpu.morphFloats == model.morphs[0].deltas.size() && gpu.drawMorphTargets.back() == 2,
                 "deltas e alvos chegam ao consumidor gráfico");
  const auto &doc = session.document();
  const auto cube = withComponent(doc, scene::SkinnedMesh::descriptor);
  const auto *mesh = component<scene::SkinnedMesh>(doc, cube);
  AE_EXPECT_TRUE(mesh && mesh->bones.empty() && mesh->blendShapeWeights.size() == 2 && mesh->blendShapeWeights[0] == 0,
                 "malha deformável só com blend shapes, pesos iniciais da malha");
  std::vector<renderer::MapDrawState> rest;
  AE_EXPECT_TRUE(session.extractMap(rest), "extração");
  const auto *restDraw = drawOf(rest, cube);
  AE_EXPECT_TRUE(restDraw && restDraw->morphWeights && restDraw->morphWeights->size() == 2 && !restDraw->skinPalette,
                 "pesos no desenho, sem paleta");
  const float restRadius = restDraw ? restDraw->pose.draw.boundsRadius : 0;

  EditorPlayScene play;
  AE_EXPECT_TRUE(play.start(session.document(), session.mapScene()), "Play");
  AE_EXPECT_TRUE(play.advance(.25) && play.advance(.25) && play.advance(.25), "o clipe anda");
  const auto *live = component<scene::SkinnedMesh>(play.document(), cube);
  AE_EXPECT_TRUE(live && (std::fabs(live->blendShapeWeights[0]) + std::fabs(live->blendShapeWeights[1])) > 1,
                 "o canal weights escreve os pesos do blend shape (0..100)");
  std::vector<renderer::MapDrawState> posed;
  AE_EXPECT_TRUE(play.extract(session.mapScene(), posed), "extração do Play");
  const auto *posedDraw = drawOf(posed, cube);
  AE_EXPECT_TRUE(posedDraw && posedDraw->morphWeights && posedDraw->pose.draw.boundsRadius > restRadius,
                 "pesos 0..1 no desenho e limites maiores que a forma base");
  play.stop();

  // Seleção pela forma deformada: com o alvo 0 inteiro, o toque segue a forma que se vê.
  auto values = *doc.find(cube);
  static_cast<scene::SkinnedMesh *>(values.components.edit(scene::SkinnedMesh::descriptor))->blendShapeWeights[0] = 100;
  AE_EXPECT_TRUE(session.history().applyValues(session.document(), cube, values), "peso autoral");
  EditorPickCandidate base, deformed;
  AE_EXPECT_TRUE(session.mapScene().pickSlotGeometry(session.document(), cube, 0, deformed) && deformed.geometry(),
                 "malha de seleção deformada");
  const auto slot = component<scene::MeshRenderer>(doc, cube)->slotMesh(0);
  std::span<const EditorPickMesh::Triangle> original;
  float relative[16];
  AE_EXPECT_TRUE(session.mapScene().localGeometry(slot, original, relative), "forma base");
  bool moved = false;
  const auto triangles = deformed.geometry()->triangles();
  for (usize t = 0; t < triangles.size() && t < original.size(); ++t)
    for (u32 k = 0; k < 9; ++k) moved = moved || !near(triangles[t][k], original[t][k], 1e-4f);
  AE_EXPECT_TRUE(moved, "a malha de seleção não é mais a forma base");
  (void)base;
}

AE_TEST(cesium_man_imports_nineteen_bones_and_walks) {
  const auto bytes = fixture("CesiumMan.glb");
  AE_EXPECT_TRUE(!bytes.empty(), "fixture CesiumMan.glb");
  EditorSession session;
  DeformationPublisher gpu;
  startSession(session, gpu);
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(session.importModel(bytes, "Fontes/CesiumMan.glb", {}, report), report.diagnostic.c_str());
  AE_EXPECT_TRUE(report.skinnedMeshes == 1 && report.animations == 1, "corpo deformável e animação");
  const auto &doc = session.document();
  const auto body = withComponent(doc, scene::SkinnedMesh::descriptor);
  const auto *mesh = component<scene::SkinnedMesh>(doc, body);
  AE_EXPECT_TRUE(mesh && mesh->bones.size() == 19, "19 ossos");
  u32 linked = 0;
  if (mesh) for (const auto bone : mesh->bones) linked += bone && doc.find(static_cast<EditorEntityId>(bone)) ? 1u : 0u;
  AE_EXPECT_EQ(linked, 19u, "todos ligados");
  EditorPlayScene play;
  AE_EXPECT_TRUE(play.start(session.document(), session.mapScene()), "Play");
  std::vector<renderer::MapDrawState> first, second;
  AE_EXPECT_TRUE(play.advance(.1) && play.extract(session.mapScene(), first), "quadro 1");
  AE_EXPECT_TRUE(play.advance(.2) && play.extract(session.mapScene(), second), "quadro 2");
  const auto *a = drawOf(first, body), *b = drawOf(second, body);
  AE_EXPECT_TRUE(a && b && a->skinPalette && b->skinPalette && *a->skinPalette != *b->skinPalette,
                 "a caminhada muda a paleta entre quadros");
  AE_EXPECT_TRUE(b && std::isfinite(b->pose.draw.boundsRadius) && b->pose.draw.boundsRadius > 0, "limites finitos");
  play.stop();
}

AE_TEST(a_missing_bone_keeps_its_vertices_in_the_bind_pose_and_picking_follows_the_pose) {
  EditorSession session;
  DeformationPublisher gpu;
  startSession(session, gpu);
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(session.importModel(test::skinnedAnimatedGlb(), "Fontes/rig.glb", {}, report), report.diagnostic.c_str());
  auto &doc = session.document();
  const auto body = named(doc, "Body"), arm = named(doc, "Arm");
  // Arm girado 90° no editor: o toque acerta o braço onde ele está.
  auto armValues = *doc.find(arm);
  armValues.transform.rotationDegrees[2] = 90;
  AE_EXPECT_TRUE(session.history().applyValues(doc, arm, armValues), "Arm girado no editor");
  EditorPickCandidate pick;
  AE_EXPECT_TRUE(session.mapScene().pickSlotGeometry(doc, body, 0, pick) && pick.geometry(), "malha de seleção");
  bool rotatedVertex = false;
  // (0,2,0) segue o Arm com 0,75 e o Hip com 0,25: 0,75·(-1,1,0) + 0,25·(0,2,0).
  for (const auto &t : pick.geometry()->triangles())
    for (u32 k = 0; k < 3; ++k) rotatedVertex = rotatedVertex || (near(t[k * 3], -.75f) && near(t[k * 3 + 1], 1.25f));
  AE_EXPECT_TRUE(rotatedVertex, "o vértice (0,2,0) do braço está em (-0,75; 1,25) na malha de seleção");
  float center[3], radius = 0;
  AE_EXPECT_TRUE(session.mapScene().bounds(doc, body, center, radius) &&
                 std::hypot(center[0] + .75f, center[1] - 1.25f, center[2]) <= radius, "os limites contêm o braço girado");

  auto values = *doc.find(body);
  auto *mesh = static_cast<scene::SkinnedMesh *>(values.components.edit(scene::SkinnedMesh::descriptor));
  if (!mesh) return;
  mesh->bones[1] = 0;
  AE_EXPECT_TRUE(session.history().applyValues(doc, body, values), "osso desligado");
  std::vector<renderer::MapDrawState> draws;
  AE_EXPECT_TRUE(session.extractMap(draws), "extração");
  const auto *draw = drawOf(draws, body);
  AE_EXPECT_TRUE(draw && draw->skinPalette && near((*draw->skinPalette)[16], 1) && near((*draw->skinPalette)[16 + 12], 0),
                 "junta sem osso usa a paleta identidade");
  EditorMapScene::DeformedPose pose;
  AE_EXPECT_TRUE(draw && session.mapScene().deformedPose(doc, *mesh, draw->sourceDrawIndex, draw->pose.draw.model, pose) &&
                 pose.missingBones == 1, "a ausência é contada para o Inspector");
}

AE_TEST(reimport_rebinds_missing_bones_and_translates_v1_clip_index) {
  EditorSession session;
  DeformationPublisher gpu;
  startSession(session, gpu);
  EditorSession::ModelImportReport report;
  const auto bytes = test::skinnedAnimatedGlb();
  AE_EXPECT_TRUE(session.importModel(bytes, "Fontes/rig.glb", {}, report), report.diagnostic.c_str());
  auto &doc = session.document();
  const auto body = named(doc, "Body"), rig = named(doc, "Rig"), arm = named(doc, "Arm");
  // Estado que uma cena antiga ou uma edição deixaria: osso solto e Animação v1.
  auto values = *doc.find(body);
  static_cast<scene::SkinnedMesh *>(values.components.edit(scene::SkinnedMesh::descriptor))->bones[1] = 0;
  AE_EXPECT_TRUE(session.history().applyValues(doc, body, values), "osso solto");
  auto rigValues = *doc.find(rig);
  auto *animation = static_cast<scene::Animation *>(rigValues.components.edit(scene::Animation::descriptor));
  std::stringstream v1("0 1 1 1");
  AE_EXPECT_TRUE(animation && animation->read(v1, 1), "Animação lida como v1");
  AE_EXPECT_TRUE(session.history().applyValues(doc, rig, rigValues), "v1 na cena");

  EditorSession::ModelImportReport again;
  AE_EXPECT_TRUE(session.importModel(bytes, "Fontes/rig.glb", {}, again), again.diagnostic.c_str());
  AE_EXPECT_TRUE(again.reimported, "mesma fonte: reimportação");
  const auto *mesh = component<scene::SkinnedMesh>(session.document(), body);
  AE_EXPECT_TRUE(mesh && mesh->bones.size() == 2 && mesh->bones[1] == arm, "osso religado ao objeto da junta");
  const auto *migrated = component<scene::Animation>(session.document(), rig);
  const auto wave = resources::animationClipGuid(again.source, "Wave", 0);
  AE_EXPECT_TRUE(migrated && migrated->clip == wave && migrated->contains(wave) && migrated->legacyClipIndex == ~0u,
                 "o índice v1 virou a identidade do clipe");
}

namespace {
// Runtime de scripts falso: o suficiente para a ABI v13 chegar e ser usada.
struct AnimationRuntime {
  static scene::ScriptSceneAccess access;
  static int start(const u8 *, int, const u8 *, int, const scene::ScriptSceneAccess *value) {
    if (value) access = *value;
    return value && value->available() ? 0 : 1;
  }
  static int update(float) { return 0; }
  static int fixedUpdate(float) { return 0; }
  static void stop() {}
  static int copyDiagnostics(u8 *, int) { return 0; }
  static int trigger(u64, u64, u32) { return 0; }
  static int contact(u64, u64, u32, const float *) { return 0; }
  static scene::ScriptRuntimeApi api() {
    scene::ScriptRuntimeApi value{};
    value.start = &start; value.update = &update; value.fixedUpdate = &fixedUpdate; value.stop = &stop;
    value.copyDiagnostics = &copyDiagnostics; value.trigger = &trigger; value.contact = &contact;
    return value;
  }
};
scene::ScriptSceneAccess AnimationRuntime::access{};
} // namespace

AE_TEST(script_abi_v13_plays_blends_and_edits_clip_entries) {
  const auto bytes = fixture("Fox.glb");
  EditorSession session;
  DeformationPublisher gpu;
  startSession(session, gpu);
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(session.importModel(bytes, "Fontes/Fox.glb", {}, report), report.diagnostic.c_str());
  auto &doc = session.document();
  const auto owner = withComponent(doc, scene::Animation::descriptor);
  auto values = *doc.find(owner);
  auto *script = static_cast<scene::ScriptBehavior *>(values.components.add(scene::ScriptBehavior::descriptor));
  script->scriptType = "project.Raposa";
  script->source = "Raposa.cs";
  AE_EXPECT_TRUE(session.history().applyValues(doc, owner, values), "script na raposa");
  const auto instance = component<scene::Animation>(doc, owner)->instanceId();

  EditorPlayScene play;
  play.setScriptRuntime(AnimationRuntime::api(), "/projeto");
  resources::AssetRegistry assets;
  std::vector<resources::EnvironmentProfile> profiles;
  renderer::ProjectRenderingSettings settings;
  renderer::RenderingCapabilities capabilities;
  const auto policy=renderer::resolveRenderingPolicy(settings,capabilities,renderer::ThermalPressure::None);
  play.configureScriptRendering(settings,capabilities,renderer::ThermalPressure::None,policy,{},&assets,&profiles);
  play.setScriptResourceAvailability([&](resources::AssetGuid guid,resources::AssetType kind,std::string_view,
                                           u32,scene::ComponentValue &) {
    runtime::AnimationClipView view;
    return kind==resources::AssetType::AnimationClip && session.mapScene().findClip(guid,view);
  });
  AE_EXPECT_TRUE(play.start(doc, session.mapScene()), "Play com o runtime falso");
  auto &abi = AnimationRuntime::access;
  AE_EXPECT_TRUE(abi.version == 13 && abi.available(), "ABI v13 completa");
  AE_EXPECT_TRUE(!abi.setParentWithPolicy(abi.context,owner,doc.root(),0,99),
                 "política de pose inválida recusada pela ABI");
  u64 ticket = 0;
  AE_EXPECT_TRUE(abi.queueStructuralOperation(abi.context,1,owner,doc.root(),0,0,&ticket) && ticket,
                 "ponte enfileira troca rastreada");
  u32 operationState = 99, operationResult = 99;
  AE_EXPECT_TRUE(abi.queryOperation(abi.context,play.world().worldId(),ticket,&operationState,&operationResult) && operationState == 0,
                 "ponte consulta pendência");
  AE_EXPECT_EQ(play.world().flush(),1u,"ponto seguro aplica troca rastreada");
  AE_EXPECT_TRUE(abi.queryOperation(abi.context,play.world().worldId(),ticket,&operationState,&operationResult) &&
                 operationState == 1 && operationResult == (u32)runtime::WorldStatus::Ok,"ponte consulta resultado aplicado");
  u8 name[64]{};
  scene::ScriptAssetGuid run{};
  AE_EXPECT_EQ(abi.animationClipAt(abi.context, owner, instance, 2, &run, name, sizeof name), 3, "três clipes");
  AE_EXPECT_TRUE(std::string(reinterpret_cast<const char *>(name)) == "Run", "nome do terceiro clipe");
  const u8 property[]{'c','l','i','p','s'};
  u64 element=0;
  AE_EXPECT_EQ(abi.resourceElementId(abi.context,owner,instance,property,5,2,&element),1,"ID do terceiro elemento");
  AE_EXPECT_TRUE(element!=0,"ID persistente não é posição");
  scene::ScriptAssetGuid byId{};
  AE_EXPECT_EQ(abi.getResourceByElementId(abi.context,owner,instance,property,5,element,&byId),1,"recurso por ID");
  AE_EXPECT_TRUE(byId.high==run.high && byId.low==run.low,"ID resolve Run");
  AE_EXPECT_EQ(abi.setResourceByElementId(abi.context,owner,instance,property,5,element,run),1,"escrita por ID usa consumidor");
  AE_EXPECT_EQ(abi.getResourceByElementId(abi.context,owner,instance,property,5,element+999,&byId),0,"ID ausente recusado");
  AE_EXPECT_EQ(abi.lastStatus(abi.context),static_cast<u32>(runtime::WorldStatus::UnknownElement),"erro de elemento ausente");
  scene::ScriptAnimationCommand fade{};
  fade.op = 1; fade.clip = run; fade.seconds = .4f;
  AE_EXPECT_EQ(abi.animationCommand(abi.context, owner, instance, &fade), 1, "CrossFade pela ABI");
  AE_EXPECT_TRUE(play.advance(.2), "metade do fade");
  scene::ScriptAnimationState state{};
  AE_EXPECT_EQ(abi.getAnimationState(abi.context, owner, instance, run, &state), 1, "estado pela ABI");
  AE_EXPECT_TRUE(state.enabled, "Run ligado");
  AE_EXPECT_TRUE(near(state.weight, .5f, .02f), ("peso no meio do fade: " + std::to_string(state.weight)).c_str());
  AE_EXPECT_TRUE(state.length > 0, "duração do clipe");
  state.speed = 2; state.layer = 1;
  AE_EXPECT_EQ(abi.setAnimationState(abi.context, owner, instance, &state), 1, "escrita do estado");
  scene::ScriptAnimationCommand bogus = fade;
  bogus.clip = {7, 7};
  AE_EXPECT_EQ(abi.animationCommand(abi.context, owner, instance, &bogus), 0, "clipe estranho recusado");
  AE_EXPECT_EQ(abi.lastStatus(abi.context), static_cast<u32>(runtime::WorldStatus::ClipNotInComponent), "com o motivo");
  AE_EXPECT_EQ(abi.animationCommand(abi.context, owner, instance + 999, &fade), 0, "componente inexistente recusado");
  scene::ScriptAssetGuid survey{};
  AE_EXPECT_EQ(abi.animationClipAt(abi.context,owner,instance,0,&survey,nullptr,0),3,"clipe inicial disponível");
  AE_EXPECT_EQ(abi.setResourceByElementId(abi.context,owner,instance,property,5,element,survey),1,
               "troca por ID conserva o elemento");
  AE_EXPECT_TRUE(play.advance(.01),"avaliador sincroniza a lista editada");
  AE_EXPECT_EQ(abi.getAnimationState(abi.context,owner,instance,run,&state),0,"clipe removido deixa de tocar");
  AE_EXPECT_EQ(abi.lastStatus(abi.context),static_cast<u32>(runtime::WorldStatus::ClipNotInComponent),
               "estado antigo é recusado");
  u64 added=0;
  AE_EXPECT_EQ(abi.appendAnimationClip(abi.context,owner,instance,run,&added),1,"adiciona Run no mundo Play");
  AE_EXPECT_TRUE(added>element,"nova entrada não reutiliza ID existente");
  AE_EXPECT_EQ(abi.animationClipAt(abi.context,owner,instance,0,nullptr,nullptr,0),4,"lista cresceu");
  AE_EXPECT_EQ(abi.getResourceByElementId(abi.context,owner,instance,property,5,added,&byId),1,"nova entrada resolve por ID");
  AE_EXPECT_TRUE(byId.high==run.high && byId.low==run.low,"recurso adicionado é Run");
  AE_EXPECT_EQ(abi.moveAnimationClip(abi.context,owner,instance,added,0),1,"move entrada sem recriar ID");
  u64 first=0;
  AE_EXPECT_EQ(abi.resourceElementId(abi.context,owner,instance,property,5,0,&first),1,"primeiro elemento");
  AE_EXPECT_EQ(first,added,"identidade preservada na reordenação");
  AE_EXPECT_EQ(abi.animationCommand(abi.context,owner,instance,&fade),1,"clipe adicionado pode tocar");
  AE_EXPECT_EQ(abi.appendAnimationClip(abi.context,owner,instance,{7,7},&first),0,"recurso ausente não entra");
  AE_EXPECT_EQ(abi.lastStatus(abi.context),static_cast<u32>(runtime::WorldStatus::ComponentUnavailable),
               "consumidor informa recurso indisponível");
  AE_EXPECT_EQ(abi.animationClipAt(abi.context,owner,instance,0,nullptr,nullptr,0),4,"falha não altera lista");
  AE_EXPECT_EQ(abi.moveAnimationClip(abi.context,owner,instance,added,4),0,"índice fora da lista recusado");
  AE_EXPECT_EQ(abi.lastStatus(abi.context),static_cast<u32>(runtime::WorldStatus::InvalidArgument),"motivo do índice");
  AE_EXPECT_EQ(abi.removeAnimationClip(abi.context,owner,instance,added),1,"remove por ID");
  AE_EXPECT_EQ(abi.removeAnimationClip(abi.context,owner,instance,added),0,"ID vencido recusado");
  AE_EXPECT_EQ(abi.lastStatus(abi.context),static_cast<u32>(runtime::WorldStatus::UnknownElement),"motivo do ID vencido");
  AE_EXPECT_EQ(abi.animationClipAt(abi.context,owner,instance,0,nullptr,nullptr,0),3,"lista voltou ao tamanho inicial");
  AE_EXPECT_TRUE(play.advance(.01),"avaliador retira estado do clipe removido");
  AE_EXPECT_EQ(abi.getAnimationState(abi.context,owner,instance,run,&state),0,"Run removido não permanece tocando");
  for(usize i=3;i<scene::Animation::MaximumClips;++i)
    AE_EXPECT_EQ(abi.appendAnimationClip(abi.context,owner,instance,run,&added),1,"entrada dentro do limite");
  AE_EXPECT_EQ(abi.appendAnimationClip(abi.context,owner,instance,run,&added),0,"limite de clipes recusado");
  AE_EXPECT_EQ(abi.lastStatus(abi.context),static_cast<u32>(runtime::WorldStatus::LimitReached),"motivo do limite");
  AE_EXPECT_EQ(added,0,"falha não devolve ID novo");
  AE_EXPECT_EQ(abi.animationClipAt(abi.context,owner,instance,0,nullptr,nullptr,0),
               static_cast<int>(scene::Animation::MaximumClips),"limite não ultrapassado");
  AE_EXPECT_EQ(component<scene::Animation>(doc,owner)->clips.size(),3,"Play não alterou o documento autoral");
  play.stop();
}

AE_TEST(inspector_clip_resource_command_accepts_loaded_clips_refuses_unknown_and_undoes) {
  const auto bytes = fixture("Fox.glb");
  EditorSession session;
  DeformationPublisher gpu;
  startSession(session, gpu);
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(session.importModel(bytes, "Fontes/Fox.glb", {}, report), report.diagnostic.c_str());
  auto &doc = session.document();
  const auto owner = withComponent(doc, scene::Animation::descriptor);
  const auto *animation = component<scene::Animation>(doc, owner);
  if (!animation) return;
  const auto instance = animation->instanceId();
  const auto run = animation->clips[2].asset;
  const auto catalog = session.mapScene().clipCatalog();
  AE_EXPECT_TRUE(catalog.size() == 3 && catalog[2].name == "Run" && catalog[2].duration > 0, "catálogo do seletor");
  EditorActionRequest request;
  request.version = session.sceneVersion();
  request.entity = owner;
  request.action = EditorAction::ComponentResource;
  request.componentInstance = instance;
  request.componentProperty = "clip";
  request.componentResource = run;
  AE_EXPECT_TRUE(session.dispatch(request).status == EditorActionStatus::Applied, "clipe padrão trocado para Run");
  AE_EXPECT_TRUE(component<scene::Animation>(doc, owner)->clip == run, "identidade gravada");
  request.version = session.sceneVersion();
  request.componentResource = {42, 42};
  AE_EXPECT_TRUE(session.dispatch(request).status != EditorActionStatus::Applied, "clipe fora das fontes carregadas recusado");
  request.version = session.sceneVersion();
  request.action = EditorAction::Undo;
  AE_EXPECT_TRUE(session.dispatch(request).status == EditorActionStatus::Applied &&
                 component<scene::Animation>(doc, owner)->clip == animation->clips[0].asset, "um desfazer volta ao Survey");
}

// O documento salvo guarda um peso por blend shape: reabrir não pode zerar os endereços.
AE_TEST(morph_cube_blend_shape_weights_survive_save_and_load) {
  const auto bytes = fixture("AnimatedMorphCube.glb");
  EditorSession session;
  DeformationPublisher gpu;
  startSession(session, gpu);
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(session.importModel(bytes, "Fontes/cubo.glb", {}, report), report.diagnostic.c_str());
  const auto cube = withComponent(session.document(), scene::SkinnedMesh::descriptor);
  const auto path = std::string(AETHER_REPOSITORY_ROOT) + "/build/test-morph-weights.aescene";
  AE_EXPECT_TRUE(session.save(path.c_str(), 0), "cena salva");
  EditorSession reopened;
  AE_EXPECT_TRUE(reopened.load(path.c_str(), 0), "cena reaberta");
  const auto *mesh = component<scene::SkinnedMesh>(reopened.document(), cube);
  AE_EXPECT_TRUE(mesh && mesh->blendShapeWeights.size() == 2, "dois endereços de peso depois de reabrir");
  std::remove(path.c_str());
}
