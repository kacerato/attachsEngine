#include "harness.h"
#include "renderer/hzb_visibility.h"

#include <cmath>
#include <limits>
#include <vector>

using namespace ae;
using namespace ae::renderer;

AE_TEST(Hzb_pyramid_max_reduction_is_exact_across_levels) {
  const float base[16] = {
      0, 1, 2, 3,
      4, 5, 6, 7,
      8, 9, 10, 11,
      12, 13, 14, 15,
  };
  HzbPyramid pyramid;
  AE_EXPECT_TRUE(buildHzbPyramid(base, 4, 4, pyramid), "constrói pirâmide válida");
  AE_EXPECT_TRUE(pyramid.valid, "pirâmide marcada válida");
  AE_EXPECT_EQ(pyramid.mips.size(), static_cast<usize>(3), "4x4 -> 2x2 -> 1x1 são três níveis");
  AE_EXPECT_EQ(pyramid.mips[0].width, 4u, "nível base preserva largura");
  AE_EXPECT_EQ(pyramid.mips[1].width, 2u, "nível 1 reduzido pela metade");
  AE_EXPECT_EQ(pyramid.mips[2].width, 1u, "nível 2 chega a 1x1");

  const HzbMipLevel &mip1 = pyramid.mips[1];
  AE_EXPECT_TRUE(pyramid.texels[mip1.offset + 0] == 5.0f, "bloco superior-esquerdo: max(0,1,4,5)=5");
  AE_EXPECT_TRUE(pyramid.texels[mip1.offset + 1] == 7.0f, "bloco superior-direito: max(2,3,6,7)=7");
  AE_EXPECT_TRUE(pyramid.texels[mip1.offset + 2] == 13.0f, "bloco inferior-esquerdo: max(8,9,12,13)=13");
  AE_EXPECT_TRUE(pyramid.texels[mip1.offset + 3] == 15.0f, "bloco inferior-direito: max(10,11,14,15)=15");

  const HzbMipLevel &mip2 = pyramid.mips[2];
  AE_EXPECT_TRUE(pyramid.texels[mip2.offset] == 15.0f, "topo da pirâmide é o máximo global");
}

AE_TEST(Hzb_pyramid_rejects_invalid_input) {
  HzbPyramid pyramid;
  AE_EXPECT_TRUE(!buildHzbPyramid(nullptr, 4, 4, pyramid), "ponteiro nulo rejeitado");
  AE_EXPECT_TRUE(!pyramid.valid, "pirâmide de saída não fica válida em falha");
  const float base[4] = {0, 1, 2, 3};
  AE_EXPECT_TRUE(!buildHzbPyramid(base, 0, 4, pyramid), "largura zero rejeitada");
  AE_EXPECT_TRUE(!buildHzbPyramid(base, 4, 0, pyramid), "altura zero rejeitada");
  const float withNan[4] = {0, 1, std::numeric_limits<float>::quiet_NaN(), 3};
  AE_EXPECT_TRUE(!buildHzbPyramid(withNan, 2, 2, pyramid), "NaN no nível base rejeitado");
}

AE_TEST(Hzb_block_max_downsamples_non_power_of_two_exactly) {
  // 6x4 real depth, reduzido para 3x2: cada célula de saída cobre exatamente
  // um bloco 2x2 da fonte, então o resultado deve ser idêntico à redução
  // 2x2 comum, mesmo sem potência de dois na entrada.
  const float full[24] = {
      1, 2, 3, 4, 5, 6,
      7, 8, 9, 10, 11, 12,
      13, 14, 15, 16, 17, 18,
      19, 20, 21, 22, 23, 24,
  };
  std::vector<float> base;
  AE_EXPECT_TRUE(buildHzbBaseLevelBlockMax(full, 6, 4, 3, 2, base), "block-max válido");
  AE_EXPECT_EQ(base.size(), static_cast<usize>(6), "3x2 = 6 texels de saída");
  // Bloco (0,0): linhas0-1,cols0-1 -> {1,2,7,8} max=8
  AE_EXPECT_TRUE(base[0] == 8.0f, "primeiro bloco 2x2");
  // Bloco (1,0): linhas0-1,cols2-3 -> {3,4,9,10} max=10
  AE_EXPECT_TRUE(base[1] == 10.0f, "segundo bloco 2x2");
  // Bloco (2,1): linhas2-3,cols4-5 -> {17,18,23,24} max=24
  AE_EXPECT_TRUE(base[5] == 24.0f, "último bloco cobre o canto inferior direito");
}

AE_TEST(Hzb_block_max_rejects_invalid_input) {
  std::vector<float> base;
  AE_EXPECT_TRUE(!buildHzbBaseLevelBlockMax(nullptr, 6, 4, 3, 2, base), "ponteiro nulo rejeitado");
  const float full[4] = {1, 2, 3, 4};
  AE_EXPECT_TRUE(!buildHzbBaseLevelBlockMax(full, 2, 2, 0, 2, base), "largura de destino zero rejeitada");
  const float withInf[4] = {1, std::numeric_limits<float>::infinity(), 3, 4};
  AE_EXPECT_TRUE(!buildHzbBaseLevelBlockMax(withInf, 2, 2, 1, 1, base), "infinito na fonte rejeitado");
}

AE_TEST(Hzb_level_offsets_are_tightly_packed_finest_first) {
  const HzbLevelDims dims[3] = {{4, 4}, {2, 2}, {1, 1}};
  std::vector<HzbMipLevel> mips;
  computeHzbLevelOffsets(dims, 3, mips);
  AE_EXPECT_EQ(mips.size(), static_cast<usize>(3), "três níveis produzem três descritores");
  AE_EXPECT_EQ(mips[0].offset, 0u, "primeiro nível começa em zero");
  AE_EXPECT_EQ(mips[1].offset, 16u, "segundo nível começa após 4x4=16 texels");
  AE_EXPECT_EQ(mips[2].offset, 20u, "terceiro nível começa após 16+2x2=20 texels");
}

AE_TEST(Hzb_pyramid_from_levels_round_trips_gpu_readback_layout) {
  const HzbLevelDims dims[2] = {{2, 2}, {1, 1}};
  const float texels[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
  HzbPyramid pyramid;
  AE_EXPECT_TRUE(hzbPyramidFromLevels(texels, 5, dims, 2, pyramid), "reconstrói pirâmide de dois níveis");
  AE_EXPECT_TRUE(pyramid.valid, "marcada válida");
  AE_EXPECT_EQ(pyramid.mips.size(), static_cast<usize>(2), "dois níveis preservados");
  AE_EXPECT_TRUE(pyramid.texels[pyramid.mips[1].offset] == 5.0f, "nível grosseiro lido do offset correto");

  AE_EXPECT_TRUE(!hzbPyramidFromLevels(texels, 4, dims, 2, pyramid), "contagem de floats divergente é rejeitada");
  AE_EXPECT_TRUE(!pyramid.valid, "saída não fica válida em falha");
  const float withNan[5] = {1.0f, 2.0f, 3.0f, std::numeric_limits<float>::quiet_NaN(), 5.0f};
  AE_EXPECT_TRUE(!hzbPyramidFromLevels(withNan, 5, dims, 2, pyramid), "NaN no readback é rejeitado");
  AE_EXPECT_TRUE(!hzbPyramidFromLevels(nullptr, 5, dims, 2, pyramid), "ponteiro nulo rejeitado");
}

AE_TEST(Hzb_reduction_chain_valida_maximo_2x2_e_detecta_corrupcao) {
  const float base[] = {
      0.1f, 0.7f, 0.2f, 0.3f,
      0.4f, 0.5f, 0.9f, 0.1f,
      0.2f, 0.6f, 0.3f, 0.8f,
  };
  HzbPyramid pyramid{};
  AE_EXPECT_TRUE(buildHzbPyramid(base, 4, 3, pyramid), "referencia HZB deve ser criada");
  AE_EXPECT_TRUE(validateHzbMaxReductionChain(pyramid),
                 "cadeia produzida pela referencia deve obedecer max 2x2");
  pyramid.texels[pyramid.mips[1].offset] -= 0.25f;
  AE_EXPECT_TRUE(!validateHzbMaxReductionChain(pyramid),
                 "um texel reduzido incorreto precisa invalidar a cadeia");
}

AE_TEST(Hzb_screen_rect_centers_forward_sphere_and_rejects_near_plane_straddle) {
  const float camera[3]{0.0f, 0.0f, 0.0f};
  PerspectiveVisibilitySettings settings{};
  settings.boundsScale = 1.0f;
  settings.boundsMargin = 0.0f;
  settings.nearPlane = 1.0f;
  settings.farPlane = 100.0f;
  const PerspectiveFrustum frustum = buildPerspectiveFrustum(camera, 0.0f, 0.0f, 1.0f, settings);

  const float centered[3]{0.0f, 0.0f, 10.0f};
  const HzbScreenRect rect = projectBoundsToHzbScreenRect(frustum, centered, 1.0f);
  AE_EXPECT_TRUE(rect.valid, "esfera à frente produz retângulo válido");
  AE_EXPECT_TRUE(rect.nearDepth > 0.897f && rect.nearDepth < 0.899f,
                 "profundidade próxima usa o mesmo depth Vulkan normalizado do attachment");
  AE_EXPECT_TRUE(rect.minU < 0.5f && rect.maxU > 0.5f, "retângulo cobre o centro horizontal da tela");
  AE_EXPECT_TRUE(rect.minV < 0.5f && rect.maxV > 0.5f, "retângulo cobre o centro vertical da tela");

  const float straddlingNear[3]{0.0f, 0.0f, 1.0f};
  const HzbScreenRect straddling = projectBoundsToHzbScreenRect(frustum, straddlingNear, 2.0f);
  AE_EXPECT_TRUE(!straddling.valid, "esfera cruzando o near plane é recusada, nunca subdimensionada");

  const float behindCamera[3]{0.0f, 0.0f, -5.0f};
  const HzbScreenRect behind = projectBoundsToHzbScreenRect(frustum, behindCamera, 1.0f);
  AE_EXPECT_TRUE(!behind.valid, "esfera atrás da câmera é recusada");

  const float farOffscreen[3]{500.0f, 0.0f, 10.0f};
  const HzbScreenRect offscreen = projectBoundsToHzbScreenRect(frustum, farOffscreen, 0.1f);
  AE_EXPECT_TRUE(!offscreen.valid, "esfera totalmente fora da tela após recorte é recusada");
}

AE_TEST(Hzb_screen_rect_applies_surface_pre_rotation_before_sampling) {
  const float camera[3]{0.0f, 0.0f, 0.0f};
  PerspectiveVisibilitySettings settings{};
  settings.boundsScale = 1.0f;
  settings.boundsMargin = 0.0f;
  const PerspectiveFrustum frustum = buildPerspectiveFrustum(camera, 0.0f, 0.0f, 2.0f, settings);
  const float rightOfCamera[3]{4.0f, 0.0f, 10.0f};
  const HzbScreenRect identity = projectBoundsToHzbScreenRect(frustum, rightOfCamera, 0.5f);
  const HzbScreenTransform rotate90{0.0f, -1.0f, 1.0f, 0.0f};
  const HzbScreenRect rotated = projectBoundsToHzbScreenRect(frustum, rightOfCamera, 0.5f, rotate90);
  AE_EXPECT_TRUE(identity.valid && rotated.valid, "ambas projeções são válidas");
  const float identityCenterU = (identity.minU + identity.maxU) * 0.5f;
  const float identityCenterV = (identity.minV + identity.maxV) * 0.5f;
  const float rotatedCenterU = (rotated.minU + rotated.maxU) * 0.5f;
  const float rotatedCenterV = (rotated.minV + rotated.maxV) * 0.5f;
  AE_EXPECT_TRUE(identityCenterU > 0.5f && std::abs(identityCenterV - 0.5f) < 1.0e-4f,
                 "sem rotação o objeto está à direita");
  AE_EXPECT_TRUE(std::abs(rotatedCenterU - 0.5f) < 1.0e-4f && rotatedCenterV > 0.5f,
                 "pré-rotação de 90 graus move direita para baixo no attachment");
}

AE_TEST(Hzb_screen_rect_fails_open_for_invalid_frustum_or_bounds) {
  const float camera[3]{0.0f, 0.0f, 0.0f};
  PerspectiveVisibilitySettings disabled{};
  disabled.enabled = false;
  const PerspectiveFrustum invalidFrustum = buildPerspectiveFrustum(camera, 0.0f, 0.0f, 1.0f, disabled);
  const float somewhere[3]{0.0f, 0.0f, 10.0f};
  AE_EXPECT_TRUE(!projectBoundsToHzbScreenRect(invalidFrustum, somewhere, 1.0f).valid,
                 "frustum inválido nunca produz retângulo");

  PerspectiveVisibilitySettings settings{};
  const PerspectiveFrustum frustum = buildPerspectiveFrustum(camera, 0.0f, 0.0f, 1.0f, settings);
  const float nanCenter[3]{std::numeric_limits<float>::quiet_NaN(), 0.0f, 10.0f};
  AE_EXPECT_TRUE(!projectBoundsToHzbScreenRect(frustum, nanCenter, 1.0f).valid, "centro não finito recusado");
  AE_EXPECT_TRUE(!projectBoundsToHzbScreenRect(frustum, somewhere, -1.0f).valid, "raio negativo recusado");
}

AE_TEST(Hzb_occlusion_test_is_conservative_and_fails_open) {
  // Pirâmide 2x2 sintética no mesmo espaço de depth normalizado usado pelo
  // attachment: metade esquerda perto (0,2), metade direita longe (0,9).
  const float base[4] = {0.2f, 0.9f, 0.2f, 0.9f};
  HzbPyramid pyramid;
  AE_EXPECT_TRUE(buildHzbPyramid(base, 2, 2, pyramid), "pirâmide sintética construída");

  HzbScreenRect leftRect{};
  leftRect.minU = 0.0f; leftRect.maxU = 0.4f; leftRect.minV = 0.0f; leftRect.maxV = 1.0f;
  leftRect.nearDepth = 0.8f; // além do max=0,2 registrado -> ocluído
  leftRect.valid = true;
  AE_EXPECT_TRUE(isOccludedByHzb(pyramid, leftRect), "candidato mais distante que o oclusor é ocluído");

  HzbScreenRect closeRect = leftRect;
  closeRect.nearDepth = 0.1f; // mais perto que o max=0,2 -> não ocluído
  AE_EXPECT_TRUE(!isOccludedByHzb(pyramid, closeRect), "candidato mais próximo que o oclusor não é ocluído");

  HzbScreenRect spanningRect{};
  spanningRect.minU = 0.0f; spanningRect.maxU = 1.0f; spanningRect.minV = 0.0f; spanningRect.maxV = 1.0f;
  spanningRect.nearDepth = 0.8f; // além da esquerda, mas não do max=0,9 da direita
  spanningRect.valid = true;
  AE_EXPECT_TRUE(!isOccludedByHzb(pyramid, spanningRect),
                 "um único texel não ocluído no footprint mantém o candidato visível");

  HzbPyramid invalidPyramid;
  AE_EXPECT_TRUE(!isOccludedByHzb(invalidPyramid, leftRect), "pirâmide inválida falha aberta");
  HzbScreenRect invalidRect{};
  AE_EXPECT_TRUE(!isOccludedByHzb(pyramid, invalidRect), "retângulo inválido falha aberta");

  leftRect.nearDepth = 0.20001f;
  AE_EXPECT_TRUE(isOccludedByHzb(pyramid, leftRect, 0.0f), "sem bias a diferença mínima oclui");
  AE_EXPECT_TRUE(!isOccludedByHzb(pyramid, leftRect, 0.0001f),
                 "bias normalizado preserva candidato quase coplanar");
  AE_EXPECT_TRUE(!isOccludedByHzb(pyramid, leftRect, -1.0f), "bias inválido falha aberto");
}

AE_TEST(Hzb_hysteresis_reports_instant_revive_and_delayed_cull) {
  HzbHysteresisState state{};
  AE_EXPECT_TRUE(updateHzbHysteresis(state, false, 3), "nunca testado / não ocluído permanece visível");
  AE_EXPECT_EQ(state.occludedStreak, 0u, "streak zerado sem oclusão");

  AE_EXPECT_TRUE(updateHzbHysteresis(state, true, 3), "primeiro frame ocluído ainda dentro da carência");
  AE_EXPECT_TRUE(updateHzbHysteresis(state, true, 3), "segundo frame ocluído ainda dentro da carência");
  AE_EXPECT_TRUE(!updateHzbHysteresis(state, true, 3), "terceiro frame consecutivo ocluído cula o objeto");
  AE_EXPECT_EQ(state.occludedStreak, 3u, "streak reflete três frames ocluídos consecutivos");

  AE_EXPECT_TRUE(updateHzbHysteresis(state, false, 3), "um único frame não ocluído revive instantaneamente");
  AE_EXPECT_EQ(state.occludedStreak, 0u, "streak zera na revivência, sem atraso");
}

AE_TEST(Hzb_hysteresis_zero_budget_culls_on_first_occluded_test) {
  HzbHysteresisState state{};
  AE_EXPECT_TRUE(!updateHzbHysteresis(state, true, 0), "carência zero cula no primeiro teste ocluído");
}

AE_TEST(Hzb_hysteresis_saturates_instead_of_wrapping_to_visible) {
  HzbHysteresisState state{};
  state.occludedStreak = std::numeric_limits<u32>::max();
  AE_EXPECT_TRUE(!updateHzbHysteresis(state, true, 3), "streak saturado continua ocluído");
  AE_EXPECT_EQ(state.occludedStreak, std::numeric_limits<u32>::max(), "contador não volta a zero por overflow");
}

AE_TEST(Hzb_workload_budget_skips_small_or_empty_candidate_sets) {
  AE_EXPECT_TRUE(!shouldRunHzb(0, 0), "workload vazio nunca paga HZB");
  AE_EXPECT_TRUE(!shouldRunHzb(63, 64), "abaixo do orçamento global falha aberto");
  AE_EXPECT_TRUE(shouldRunHzb(64, 64), "limiar inclusivo ativa o estágio");
  AE_EXPECT_TRUE(shouldRunHzb(1, 0), "zero permite execução explícita de validação");
}
