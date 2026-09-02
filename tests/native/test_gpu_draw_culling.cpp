#include "harness.h"
#include "renderer/gpu_draw_culling.h"
#include "renderer/hzb_visibility.h"

#include <cmath>
#include <cstddef>
#include <vector>

using namespace ae;
using namespace ae::renderer;

namespace {

// Pirâmide sintética: uma parede opaca ocupando a metade esquerda da tela na
// profundidade normalizada `wallDepth`, céu (profundidade máxima) no resto.
// Um candidato atrás da parede e dentro dessa metade deve ser ocluído; o mesmo
// candidato do outro lado, nunca.
HzbPyramid buildWallPyramid(u32 width, u32 height, float wallDepth) {
  std::vector<float> base(static_cast<usize>(width) * height, 1.0f);
  for (u32 y = 0; y < height; ++y)
    for (u32 x = 0; x < width / 2; ++x) base[static_cast<usize>(y) * width + x] = wallDepth;
  HzbPyramid pyramid;
  buildHzbPyramid(base.data(), width, height, pyramid);
  return pyramid;
}

PerspectiveFrustum frustumAt(float x, float y, float z, float yaw, float pitch) {
  const float position[3] = {x, y, z};
  PerspectiveVisibilitySettings settings{};
  settings.nearPlane = 0.1f;
  settings.farPlane = 500.0f;
  settings.boundsScale = 1.05f;
  settings.boundsMargin = 0.5f;
  return buildPerspectiveFrustum(position, yaw, pitch, 1.0f, settings);
}

GpuCullDrawRecord recordAt(float x, float y, float z, float radius, u32 stateIndex) {
  GpuCullDrawRecord record{};
  record.boundsCenter[0] = x;
  record.boundsCenter[1] = y;
  record.boundsCenter[2] = z;
  record.boundsRadius = radius;
  record.stateIndex = stateIndex;
  record.flags = GpuCullRecordPresent | GpuCullRecordTestable;
  return record;
}

} // namespace

AE_TEST(GpuCull_layout_matches_the_shader_contract) {
  // O kernel lê exatamente estes tamanhos. Um campo inserido no meio da struct
  // sem atualizar draw_cull.comp produziria corte silenciosamente errado, e não
  // um erro de compilação, se este teste não existisse.
  AE_EXPECT_EQ(sizeof(GpuCullDrawRecord), static_cast<usize>(32), "registro tem 32 bytes");
  AE_EXPECT_EQ(sizeof(GpuCullParameters), static_cast<usize>(112),
               "push constants cabem nos 128 bytes garantidos");
  AE_EXPECT_EQ(offsetof(GpuCullParameters, surfaceTransform), static_cast<usize>(64),
               "transformada de superfície no quinto bloco de 16 bytes");
  AE_EXPECT_EQ(offsetof(GpuCullParameters, drawCount), static_cast<usize>(80),
               "contagens no sexto bloco de 16 bytes");
  AE_EXPECT_EQ(gpuCullGroupCount(0), 0u, "nenhum candidato não despacha grupo");
  AE_EXPECT_EQ(gpuCullGroupCount(1), 1u, "um candidato ocupa um grupo");
  AE_EXPECT_EQ(gpuCullGroupCount(GpuCullLocalSizeX), 1u, "grupo cheio continua sendo um");
  AE_EXPECT_EQ(gpuCullGroupCount(GpuCullLocalSizeX + 1), 2u, "sobra abre um grupo novo");
}

AE_TEST(GpuCull_hzb_view_requires_the_ceiling_halved_chain) {
  const HzbPyramid pyramid = buildWallPyramid(16, 8, 0.5f);
  GpuCullHzbView view{};
  AE_EXPECT_TRUE(buildGpuCullHzbView(pyramid, view), "cadeia canônica é aceita");
  AE_EXPECT_EQ(view.baseWidth, 16u, "largura da base preservada");
  AE_EXPECT_EQ(view.baseHeight, 8u, "altura da base preservada");
  AE_EXPECT_EQ(view.levelCount, static_cast<u32>(pyramid.mips.size()),
               "todos os níveis ficam visíveis ao kernel");

  HzbPyramid corrupted = pyramid;
  corrupted.mips[1].width += 1; // Quebra a divisão-teto que o kernel assume.
  GpuCullHzbView rejected{};
  AE_EXPECT_TRUE(!buildGpuCullHzbView(corrupted, rejected),
                 "cadeia fora da divisão-teto é recusada");
  AE_EXPECT_TRUE(rejected.texels == nullptr, "view recusada não fica parcialmente preenchida");

  HzbPyramid invalid = pyramid;
  invalid.valid = false;
  AE_EXPECT_TRUE(!buildGpuCullHzbView(invalid, rejected), "pirâmide inválida é recusada");
}

AE_TEST(GpuCull_reference_matches_the_cpu_chain_without_motion_guard) {
  // A garantia central desta fatia: mover a decisão para a GPU não muda o que é
  // considerado visível. Com guarda zero, a referência do kernel tem de
  // reproduzir projectBoundsToHzbScreenRect + isOccludedByHzb +
  // updateHzbHysteresis candidato a candidato.
  const HzbPyramid pyramid = buildWallPyramid(64, 64, 0.30f);
  GpuCullHzbView view{};
  AE_EXPECT_TRUE(buildGpuCullHzbView(pyramid, view), "view construída");

  u32 divergences = 0;
  u32 occludedCases = 0;
  u32 visibleCases = 0;
  for (int yawStep = 0; yawStep < 8; ++yawStep) {
    const float yaw = static_cast<float>(yawStep) * 0.7853981634f;
    const PerspectiveFrustum frustum = frustumAt(0.0f, 0.0f, 0.0f, yaw, 0.15f);
    const HzbScreenTransform screenTransform{};
    GpuCullMotionGuard guard =
        buildGpuCullMotionGuard(frustum, frustum.cameraPosition, frustum.yaw, frustum.pitch);
    AE_EXPECT_TRUE(guard.valid, "guarda de pose idêntica é válida");
    GpuCullParameters parameters{};
    AE_EXPECT_TRUE(buildGpuCullParameters(frustum, screenTransform, view, guard, 1.0e-5f, 3, 1,
                                          true, parameters),
                   "parâmetros construídos");
    AE_EXPECT_TRUE(parameters.screenDilation == 0.0f && parameters.viewDepthGuard == 0.0f &&
                   parameters.translationDilationScale == 0.0f,
                   "câmera parada não produz folga nenhuma");

    for (int xStep = -6; xStep <= 6; ++xStep) {
      for (int zStep = 1; zStep <= 12; ++zStep) {
        const float localX = static_cast<float>(xStep) * 2.0f;
        const float localZ = static_cast<float>(zStep) * 9.0f;
        // Reposiciona no espaço de mundo consistente com o yaw testado.
        const float worldX = std::cos(yaw) * localX + std::sin(yaw) * localZ;
        const float worldZ = -std::sin(yaw) * localX + std::cos(yaw) * localZ;
        const GpuCullDrawRecord record = recordAt(worldX, 0.0f, worldZ, 1.5f, 0);

        const HzbScreenRect rect = projectBoundsToHzbScreenRect(
            frustum, record.boundsCenter, record.boundsRadius, screenTransform);
        const bool cpuOccluded = isOccludedByHzb(pyramid, rect, parameters.normalizedDepthBias);
        HzbHysteresisState cpuState{};
        const bool cpuVisible = updateHzbHysteresis(cpuState, cpuOccluded,
                                                    parameters.hysteresisFrames);

        u32 streak = 0;
        const GpuCullOutcome outcome =
            cullDrawRecordReference(parameters, record, view, streak);
        if (outcome.occludedThisFrame != cpuOccluded || outcome.visible != cpuVisible ||
            streak != cpuState.occludedStreak) {
          ++divergences;
        }
        if (cpuOccluded) ++occludedCases; else ++visibleCases;
      }
    }
  }
  AE_EXPECT_EQ(divergences, 0u, "referência do kernel não diverge da cadeia de CPU");
  // Sem os dois lados exercitados a igualdade acima seria trivial.
  AE_EXPECT_TRUE(occludedCases > 0, "o corpus contém candidatos ocluídos");
  AE_EXPECT_TRUE(visibleCases > 0, "o corpus contém candidatos visíveis");
}

AE_TEST(GpuCull_motion_guard_only_ever_removes_culling) {
  // Monotonicidade da guarda: nenhuma quantidade de movimento pode fazer o
  // estágio cortar MAIS do que cortaria com a câmera parada. É o que substitui
  // o penhasco `hzb_motion_skip` sem virar um corte agressivo em movimento.
  const HzbPyramid pyramid = buildWallPyramid(64, 64, 0.30f);
  GpuCullHzbView view{};
  AE_EXPECT_TRUE(buildGpuCullHzbView(pyramid, view), "view construída");
  const PerspectiveFrustum frustum = frustumAt(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  const HzbScreenTransform screenTransform{};

  u32 previousCulled = 0;
  bool firstSample = true;
  bool monotone = true;
  u32 stillCullingAtRest = 0;
  for (int motionStep = 0; motionStep < 8; ++motionStep) {
    const float offset = static_cast<float>(motionStep) * 0.75f;
    const float pyramidCamera[3] = {offset, 0.0f, -offset};
    const GpuCullMotionGuard guard =
        buildGpuCullMotionGuard(frustum, pyramidCamera, 0.05f * static_cast<float>(motionStep),
                                0.0f);
    AE_EXPECT_TRUE(guard.valid, "guarda válida para deslocamento finito");
    GpuCullParameters parameters{};
    AE_EXPECT_TRUE(buildGpuCullParameters(frustum, screenTransform, view, guard, 1.0e-5f, 0, 1,
                                          true, parameters),
                   "parâmetros construídos");

    u32 culled = 0;
    for (int xStep = -6; xStep <= 6; ++xStep) {
      for (int zStep = 1; zStep <= 12; ++zStep) {
        const GpuCullDrawRecord record = recordAt(static_cast<float>(xStep) * 2.0f, 0.0f,
                                                  static_cast<float>(zStep) * 9.0f, 1.5f, 0);
        u32 streak = 0;
        if (!cullDrawRecordReference(parameters, record, view, streak).visible) ++culled;
      }
    }
    if (motionStep == 0) stillCullingAtRest = culled;
    if (!firstSample && culled > previousCulled) monotone = false;
    previousCulled = culled;
    firstSample = false;
  }
  AE_EXPECT_TRUE(monotone, "aumentar o movimento nunca aumenta o conjunto cortado");
  AE_EXPECT_TRUE(stillCullingAtRest > 0, "com câmera parada o estágio ainda corta");
  AE_EXPECT_EQ(previousCulled, 0u, "movimento grande o bastante desliga o corte por completo");
}

AE_TEST(GpuCull_motion_guard_rejects_uncertain_poses) {
  const PerspectiveFrustum frustum = frustumAt(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  const float finitePose[3] = {0.0f, 0.0f, 0.0f};
  const float nonFinitePose[3] = {0.0f, std::nanf(""), 0.0f};
  AE_EXPECT_TRUE(!buildGpuCullMotionGuard(frustum, nonFinitePose, 0.0f, 0.0f).valid,
                 "posição não finita não produz guarda");
  AE_EXPECT_TRUE(!buildGpuCullMotionGuard(frustum, finitePose, std::nanf(""), 0.0f).valid,
                 "yaw não finito não produz guarda");
  AE_EXPECT_TRUE(!buildGpuCullMotionGuard(PerspectiveFrustum{}, finitePose, 0.0f, 0.0f).valid,
                 "frustum inválido não produz guarda");

  // Uma guarda inválida não pode virar "corte sem guarda": sem saber quanto a
  // pose mudou, a pirâmide não é utilizável neste frame.
  GpuCullHzbView view{};
  const HzbPyramid pyramid = buildWallPyramid(16, 16, 0.3f);
  AE_EXPECT_TRUE(buildGpuCullHzbView(pyramid, view), "view construída");
  GpuCullParameters parameters{};
  AE_EXPECT_TRUE(buildGpuCullParameters(frustum, HzbScreenTransform{}, view,
                                        GpuCullMotionGuard{}, 0.0f, 3, 4, true, parameters),
                 "parâmetros são construídos mesmo sem guarda");
  AE_EXPECT_EQ(parameters.flags & GpuCullPyramidUsable, 0u,
               "sem guarda válida a pirâmide não é declarada utilizável");
}

AE_TEST(GpuCull_unusable_pyramid_fails_open_and_clears_state) {
  const HzbPyramid pyramid = buildWallPyramid(32, 32, 0.2f);
  GpuCullHzbView view{};
  AE_EXPECT_TRUE(buildGpuCullHzbView(pyramid, view), "view construída");
  const PerspectiveFrustum frustum = frustumAt(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  GpuCullParameters parameters{};
  AE_EXPECT_TRUE(buildGpuCullParameters(frustum, HzbScreenTransform{}, view,
                                        buildGpuCullMotionGuard(frustum, frustum.cameraPosition,
                                                                0.0f, 0.0f),
                                        0.0f, 3, 1, false, parameters),
                 "parâmetros construídos com pirâmide declarada inutilizável");

  const GpuCullDrawRecord record = recordAt(0.0f, 0.0f, 40.0f, 1.0f, 0);
  u32 streak = 7;
  const GpuCullOutcome outcome = cullDrawRecordReference(parameters, record, view, streak);
  AE_EXPECT_TRUE(outcome.visible, "pirâmide inutilizável nunca corta");
  AE_EXPECT_TRUE(!outcome.tested, "e o candidato não é contado como testado");
  AE_EXPECT_EQ(streak, 0u, "estado acumulado de uma época anterior é descartado");

  // Um registro não marcado como testável segue o mesmo caminho.
  GpuCullParameters usableParameters{};
  AE_EXPECT_TRUE(buildGpuCullParameters(frustum, HzbScreenTransform{}, view,
                                        buildGpuCullMotionGuard(frustum, frustum.cameraPosition,
                                                                0.0f, 0.0f),
                                        0.0f, 3, 1, true, usableParameters),
                 "parâmetros utilizáveis construídos");
  GpuCullDrawRecord untestable = record;
  untestable.flags = GpuCullRecordPresent;
  streak = 5;
  const GpuCullOutcome untestedOutcome =
      cullDrawRecordReference(usableParameters, untestable, view, streak);
  AE_EXPECT_TRUE(untestedOutcome.visible && !untestedOutcome.tested,
                 "registro não testável permanece visível sem ser testado");
  AE_EXPECT_EQ(streak, 0u, "e também tem o estado zerado");
}

AE_TEST(GpuCull_hysteresis_delays_culling_and_revives_immediately) {
  // Uma parede que cobre a tela inteira: o candidato atrás dela é ocluído em
  // todo frame, então o único motivo de continuar visível é a carência.
  const HzbPyramid pyramid = buildWallPyramid(32, 32, 0.05f);
  std::vector<float> full(32u * 32u, 0.05f);
  HzbPyramid wall;
  buildHzbPyramid(full.data(), 32, 32, wall);
  GpuCullHzbView view{};
  AE_EXPECT_TRUE(buildGpuCullHzbView(wall, view), "view construída");
  const PerspectiveFrustum frustum = frustumAt(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  GpuCullParameters parameters{};
  AE_EXPECT_TRUE(buildGpuCullParameters(frustum, HzbScreenTransform{}, view,
                                        buildGpuCullMotionGuard(frustum, frustum.cameraPosition,
                                                                0.0f, 0.0f),
                                        0.0f, 3, 1, true, parameters),
                 "parâmetros construídos");
  const GpuCullDrawRecord record = recordAt(0.0f, 0.0f, 60.0f, 1.0f, 0);

  u32 streak = 0;
  GpuCullOutcome outcome = cullDrawRecordReference(parameters, record, view, streak);
  AE_EXPECT_TRUE(outcome.occludedThisFrame, "o candidato está de fato ocluído");
  AE_EXPECT_TRUE(outcome.visible, "primeiro frame ocluído ainda desenha (carência)");
  outcome = cullDrawRecordReference(parameters, record, view, streak);
  AE_EXPECT_TRUE(outcome.visible, "segundo frame ocluído ainda desenha");
  outcome = cullDrawRecordReference(parameters, record, view, streak);
  AE_EXPECT_TRUE(!outcome.visible, "terceiro frame ocluído fecha a carência e corta");
  AE_EXPECT_EQ(streak, 3u, "streak reflete os três frames");

  // Revive: um único frame não ocluído devolve o objeto imediatamente.
  GpuCullHzbView skyView{};
  AE_EXPECT_TRUE(buildGpuCullHzbView(pyramid, skyView), "view alternativa construída");
  GpuCullDrawRecord nearRecord = record;
  nearRecord.boundsCenter[2] = 1.0f; // À frente da parede: nada pode ocluí-lo.
  outcome = cullDrawRecordReference(parameters, nearRecord, view, streak);
  AE_EXPECT_TRUE(outcome.visible, "objeto exposto volta no mesmo frame");
  AE_EXPECT_TRUE(outcome.revived, "e o revive é contabilizado");
  AE_EXPECT_EQ(streak, 0u, "streak zerado no revive");
}

AE_TEST(GpuCull_padding_slots_are_never_written) {
  // O dispatch cobre a capacidade da lista de comandos, não o número de
  // comandos do frame. Um slot de sobra que escrevesse estado apagaria a
  // histerese do draw cujo índice ele carrega zerado — o draw 0.
  const HzbPyramid pyramid = buildWallPyramid(32, 32, 0.05f);
  GpuCullHzbView view{};
  AE_EXPECT_TRUE(buildGpuCullHzbView(pyramid, view), "view construída");
  const PerspectiveFrustum frustum = frustumAt(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  GpuCullParameters parameters{};
  AE_EXPECT_TRUE(buildGpuCullParameters(frustum, HzbScreenTransform{}, view,
                                        buildGpuCullMotionGuard(frustum, frustum.cameraPosition,
                                                                0.0f, 0.0f),
                                        0.0f, 3, 8, true, parameters),
                 "parâmetros construídos");
  GpuCullDrawRecord padding{};
  padding.flags = 0; // Nem presente, nem testável.
  u32 streak = 4;
  const GpuCullOutcome outcome = cullDrawRecordReference(parameters, padding, view, streak);
  AE_EXPECT_TRUE(!outcome.present, "slot de sobra é reconhecido como ausente");
  AE_EXPECT_EQ(streak, 4u, "e o estado do slot permanece intocado");
}
