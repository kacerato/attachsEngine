// Skin e animação por nós (G6-B): importação, amostragem das três
// interpolações do glTF, paleta de juntas, limites deformados e cache.
#include "harness.h"
#include "skinned_glb_fixture.h"
#include "resources/gltf_import.h"
#include "resources/import_cache.h"
#include "resources/skeletal_animation.h"

#include <cmath>
#include <cstring>

using namespace ae;
using namespace ae::resources;

namespace {
bool near(float a, float b, float tolerance = 1e-4f) { return std::fabs(a - b) <= tolerance; }
const AnimationChannel *channelFor(const AnimationClip &clip, u32 node, AnimationPath path) {
  for (const auto &channel : clip.channels)
    if (channel.node == node && channel.path == path) return &channel;
  return nullptr;
}
} // namespace

AE_TEST(skinned_glb_imports_joints_weights_bind_matrices_and_clips) {
  GltfImport model;
  AE_EXPECT_TRUE(importGlb(test::skinnedAnimatedGlb(), {}, {}, model), model.diagnostic.c_str());
  AE_EXPECT_EQ(model.nodes.size(), usize{4}, "Rig, Hip, Arm e Body viram nós");
  AE_EXPECT_EQ(model.skins.size(), usize{1}, "um skin");
  const auto &skin = model.skins[0];
  AE_EXPECT_TRUE(skin.joints.size() == 2 && model.nodes[skin.joints[0]].name == "Hip" &&
                 model.nodes[skin.joints[1]].name == "Arm", "juntas apontam para os nós emitidos");
  AE_EXPECT_TRUE(near(skin.inverseBind[16 + 13], -1.0f), "bind inversa do Arm lida do acessor MAT4");
  AE_EXPECT_TRUE(model.drawSkins.size() == 1 && model.drawSkins[0] == 0, "desenho do Body deformado pelo skin 0");
  AE_EXPECT_EQ(model.skippedSkins, 0u, "skin suportado não é contado como ignorado");
  AE_EXPECT_EQ(model.skinInfluences.size(), usize{4} * SkinInfluenceStride, "influências paralelas aos vértices");
  u16 top[8];
  std::memcpy(top, model.skinInfluences.data() + 2 * SkinInfluenceStride, 16);
  AE_EXPECT_TRUE(top[0] == 1 && top[1] == 0, "4 maiores influências em ordem de peso");
  AE_EXPECT_TRUE(std::abs(static_cast<int>(top[4]) - 49151) <= 1 && top[4] + top[5] == 65535,
                 "pesos 0,6/0,2 renormalizados para 0,75/0,25 e soma exata");
  // Esfera do Arm: vértices (0,2,0) e (1,2,0) no espaço da junta = (0,1,0), (1,1,0).
  const float *arm = skin.jointSpheres.data() + 4;
  AE_EXPECT_TRUE(near(arm[0], .5f) && near(arm[1], 1.0f) && near(arm[3], .5f), "esfera por junta no espaço da junta");
  AE_EXPECT_EQ(model.animations.size(), usize{1}, "clipe com canais suportados");
  const auto &clip = model.animations[0];
  AE_EXPECT_TRUE(clip.name == "Wave" && near(clip.duration, 1.0f) && clip.channels.size() == 3, "três canais TRS");
  AE_EXPECT_EQ(model.unsupportedAnimationChannels, 1u, "canal de pesos de morph contado, não fingido");
  AE_EXPECT_EQ(model.skippedAnimations, 0u, "clipe importado não é ignorado");
}

AE_TEST(animation_sampling_follows_step_linear_and_cubicspline) {
  GltfImport model;
  AE_EXPECT_TRUE(importGlb(test::skinnedAnimatedGlb(), {}, {}, model), model.diagnostic.c_str());
  const auto &clip = model.animations[0];
  const u32 hip = model.skins[0].joints[0], arm = model.skins[0].joints[1];
  const auto *rotation = channelFor(clip, arm, AnimationPath::Rotation);
  const auto *translation = channelFor(clip, hip, AnimationPath::Translation);
  const auto *scale = channelFor(clip, hip, AnimationPath::Scale);
  AE_EXPECT_TRUE(rotation && translation && scale, "canais endereçam os nós emitidos");
  float value[4];
  AE_EXPECT_TRUE(sampleAnimationChannel(*rotation, .5f, value), "rotação amostrada");
  AE_EXPECT_TRUE(near(value[2], std::sin(0.3926991f)) && near(value[3], std::cos(0.3926991f)),
                 "LINEAR em rotação é slerp: 45° no meio");
  AE_EXPECT_TRUE(sampleAnimationChannel(*translation, .49f, value) && near(value[2], 0),
                 "STEP mantém a chave anterior");
  AE_EXPECT_TRUE(sampleAnimationChannel(*translation, .5f, value) && near(value[2], 2),
                 "STEP troca exatamente no tempo da chave");
  AE_EXPECT_TRUE(sampleAnimationChannel(*scale, .5f, value) && near(value[0], 1.5f),
                 "CUBICSPLINE com tangentes zero é Hermite: 1,5 no meio");
  AE_EXPECT_TRUE(sampleAnimationChannel(*scale, 2.0f, value) && near(value[1], 2.0f), "depois do fim, última chave");
  bool finished = false;
  AE_EXPECT_TRUE(near(wrapAnimationTime(2.25f, 1, AnimationWrapMode::Loop, finished), .25f) && !finished, "Loop");
  AE_EXPECT_TRUE(near(wrapAnimationTime(1.25f, 1, AnimationWrapMode::PingPong, finished), .75f), "PingPong volta");
  AE_EXPECT_TRUE(near(wrapAnimationTime(3, 1, AnimationWrapMode::Once, finished), 1) && finished, "Once termina");
  AE_EXPECT_TRUE(near(wrapAnimationTime(3, 1, AnimationWrapMode::ClampForever, finished), 1) && !finished,
                 "ClampForever segura sem terminar");
  AnimationChannel broken = *rotation;
  broken.times = {1, 0};
  AE_EXPECT_TRUE(!validAnimationChannel(broken) && !sampleAnimationChannel(broken, 0, value),
                 "tempos fora de ordem são recusados");
}

AE_TEST(skin_palette_deforms_in_draw_space_and_bounds_follow_the_joints) {
  GltfImport model;
  AE_EXPECT_TRUE(importGlb(test::skinnedAnimatedGlb(), {}, {}, model), model.diagnostic.c_str());
  const auto &skin = model.skins[0];
  const float identity[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  // Arm girado 90° em Z em torno da própria origem (0,1,0).
  const float t[3]{0, 1, 0}, q[4]{0, 0, 0.70710678f, 0.70710678f}, s[3]{1, 1, 1};
  float armWorld[16];
  composeTransform(t, q, s, armWorld);
  std::vector<float> joints(identity, identity + 16);
  joints.insert(joints.end(), armWorld, armWorld + 16);
  std::vector<float> palette;
  AE_EXPECT_TRUE(computeSkinPalette(skin, identity, joints, palette), "paleta calculada");
  const float *m = palette.data() + 16;
  const float p[3]{0, 2, 0};
  float moved[3];
  for (u32 axis = 0; axis < 3; ++axis) moved[axis] = m[axis] * p[0] + m[4 + axis] * p[1] + m[8 + axis] * p[2] + m[12 + axis];
  AE_EXPECT_TRUE(near(moved[0], -1) && near(moved[1], 1) && near(moved[2], 0), "vértice do braço gira com a junta");
  // Modelo do desenho transladado: a paleta compensa, o mundo continua o mesmo.
  float model10[16];
  std::copy(identity, identity + 16, model10);
  model10[12] = 10;
  std::vector<float> relative;
  AE_EXPECT_TRUE(computeSkinPalette(skin, model10, joints, relative), "paleta relativa ao modelo");
  AE_EXPECT_TRUE(near(relative[16 + 12], palette[16 + 12] - 10), "inversa do modelo aplicada à esquerda");
  float center[3], radius = 0;
  AE_EXPECT_TRUE(skinnedLocalBounds(skin, palette, center, radius), "limites deformados");
  AE_EXPECT_TRUE(center[0] - radius <= -1.0f && center[1] + radius >= 2.0f - 1e-3f,
                 "a esfera contém o braço girado e a base");
  std::vector<float> wrongSize(16);
  AE_EXPECT_TRUE(!computeSkinPalette(skin, identity, wrongSize, palette), "contagem de juntas errada recusada");
}

AE_TEST(skin_and_animation_survive_the_import_cache) {
  GltfImport model;
  AE_EXPECT_TRUE(importGlb(test::skinnedAnimatedGlb(), {}, {}, model), model.diagnostic.c_str());
  std::vector<u8> bytes;
  AE_EXPECT_TRUE(writeImportCache(model, "chave", bytes), "cache gravado");
  GltfImport restored;
  AE_EXPECT_TRUE(readImportCache(bytes, "chave", restored), "cache lido");
  AE_EXPECT_TRUE(restored.skins.size() == 1 && restored.skins[0].joints == model.skins[0].joints &&
                 restored.skins[0].inverseBind == model.skins[0].inverseBind &&
                 restored.skins[0].jointSpheres == model.skins[0].jointSpheres, "skin idêntico");
  AE_EXPECT_TRUE(restored.drawSkins == model.drawSkins && restored.skinInfluences == model.skinInfluences,
                 "influências e skin por desenho idênticos");
  AE_EXPECT_TRUE(restored.animations.size() == 1 && restored.animations[0].channels.size() == 3 &&
                 restored.animations[0].channels[2].values == model.animations[0].channels[2].values &&
                 restored.unsupportedAnimationChannels == 1, "clipe idêntico, com o canal recusado contado");
  // Um índice de junta fora da árvore é cache corrompido.
  GltfImport corrupt = model;
  corrupt.skins[0].joints[1] = 99;
  AE_EXPECT_TRUE(writeImportCache(corrupt, "chave", bytes) && !readImportCache(bytes, "chave", restored),
                 "junta fora da árvore recusa o cache");
}

AE_TEST(skin_with_a_joint_outside_the_scene_is_refused_with_reason_and_draws_static) {
  GltfImport model;
  AE_EXPECT_TRUE(importGlb(test::skinnedAnimatedGlb(true), {}, {}, model), model.diagnostic.c_str());
  AE_EXPECT_TRUE(model.skins.size() == 1 && model.skins[0].joints.empty(), "skin recusado fica vazio");
  AE_EXPECT_TRUE(model.drawSkins.size() == 1 && model.drawSkins[0] == -1, "o desenho fica estático");
  AE_EXPECT_EQ(model.skippedSkins, 1u, "a recusa é contada");
  bool reason = false;
  for (const auto &note : model.notes) reason = reason || note.find("junta fora da cena") != std::string::npos;
  AE_EXPECT_TRUE(reason && model.anythingSkipped(), "o motivo aparece no relatório");
}
