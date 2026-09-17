// Matemática das sombras em cascata.
//
// Os três artefatos clássicos de CSM têm origem aritmética, e é aqui que eles são
// trancados: distribuição de resolução entre cascatas, volume que pulsa quando a
// câmera gira, e borda que "nada" com movimento sub-texel.
#include "harness.h"
#include "renderer/shadow_cascades.h"

#include <cmath>

using namespace ae;
using namespace ae::test;
using namespace ae::renderer;

namespace {

ShadowCascadeInput baseInput() {
  ShadowCascadeInput input{};
  input.cameraPosition[0] = 10.0f;
  input.cameraPosition[1] = 2.0f;
  input.cameraPosition[2] = -30.0f;
  input.cameraForward[0] = 0.0f;
  input.cameraForward[1] = 0.0f;
  input.cameraForward[2] = 1.0f;
  input.cameraUp[1] = 1.0f;
  input.verticalFovRadians = 1.0472f;
  input.aspectRatio = 2772.0f / 1280.0f;
  input.nearPlane = 0.1f;
  input.shadowDistance = 200.0f;
  // Sol a 45 graus, iluminando de cima para baixo e para frente.
  input.lightDirection[0] = 0.0f;
  input.lightDirection[1] = -0.7071f;
  input.lightDirection[2] = 0.7071f;
  input.casterExtrusion = 200.0f;
  input.cascadeResolution = 1024;
  return input;
}

// Transforma um ponto de mundo por uma matriz column-major e devolve o clip.
void transform(const float m[16], const float p[3], float out[4]) {
  for (u32 row = 0; row < 4; ++row) {
    out[row] = m[row] * p[0] + m[4 + row] * p[1] + m[8 + row] * p[2] + m[12 + row];
  }
}

} // namespace

AE_TEST(orthographic_shadow_receivers_cover_parallel_near_and_far_faces) {
  auto input=baseInput();input.orthographicHalfHeight=12;input.aspectRatio=2;
  input.verticalFovRadians=0;input.nearPlane=1;input.shadowDistance=30;
  ShadowCascade cascades[4]{};
  AE_EXPECT_EQ(computeShadowCascades(input,4,.75f,cascades),4u,"orthographic does not need FOV");
  for(const auto &cascade:cascades) for(float depth:{cascade.nearDistance,cascade.farDistance})
    for(float x:{-24.f,24.f}) for(float y:{-12.f,12.f}) {
      const float point[]{input.cameraPosition[0]+x,input.cameraPosition[1]+y,input.cameraPosition[2]+depth};float clip[4];
      transform(cascade.viewProjection,point,clip);
      AE_EXPECT_TRUE(std::abs(clip[0])<=1.003f&&std::abs(clip[1])<=1.003f&&clip[2]>=0&&clip[2]<=1.003f,"parallel corners stay inside light volume including texel snap");
    }
}

AE_TEST(cascade_splits_crescem_e_terminam_na_distancia_pedida) {
  float splits[MaximumShadowCascades]{};
  AE_EXPECT_EQ(computeCascadeSplits(0.1f, 200.0f, 4, DefaultCascadeSplitLambda, splits), 4u,
               "quatro cortes escritos");
  for (u32 i = 1; i < 4; ++i) {
    AE_EXPECT_TRUE(splits[i] > splits[i - 1], "cortes precisam ser estritamente crescentes");
  }
  // Uma faixa sem sombra no limite do alcance seria visível como uma borda reta
  // atravessando o chão.
  AE_EXPECT_EQ(splits[3], 200.0f, "a ultima cascata termina exatamente no alcance");
}

AE_TEST(cascade_splits_misturam_uniforme_e_logaritmico) {
  float uniform[MaximumShadowCascades]{};
  float logarithmic[MaximumShadowCascades]{};
  float blended[MaximumShadowCascades]{};
  computeCascadeSplits(1.0f, 100.0f, 4, 0.0f, uniform);
  computeCascadeSplits(1.0f, 100.0f, 4, 1.0f, logarithmic);
  computeCascadeSplits(1.0f, 100.0f, 4, 0.5f, blended);
  // Log concentra resolução perto da câmera: seus cortes intermediários ficam
  // antes dos uniformes, e a mistura no meio dos dois.
  AE_EXPECT_TRUE(logarithmic[0] < uniform[0], "log corta mais cedo que uniforme");
  AE_EXPECT_TRUE(blended[0] > logarithmic[0] && blended[0] < uniform[0], "mistura fica no meio");
}

AE_TEST(cascade_splits_recusam_entrada_invalida) {
  float splits[MaximumShadowCascades]{};
  AE_EXPECT_EQ(computeCascadeSplits(0.1f, 200.0f, 0, .75f, splits), 0u, "zero cascatas");
  AE_EXPECT_EQ(computeCascadeSplits(0.1f, 200.0f, 9, .75f, splits), 0u, "acima do maximo");
  AE_EXPECT_EQ(computeCascadeSplits(0.0f, 200.0f, 4, .75f, splits), 0u, "near invalido");
  AE_EXPECT_EQ(computeCascadeSplits(10.0f, 5.0f, 4, .75f, splits), 0u, "alcance menor que near");
  AE_EXPECT_EQ(computeCascadeSplits(0.1f, 200.0f, 4, .75f, nullptr), 0u, "saida nula");
}

AE_TEST(cascade_cobre_a_fatia_do_frustum_que_promete) {
  // O canto mais distante da fatia precisa cair dentro do volume da cascata; se
  // não cair, aparece um buraco de sombra bem no meio da tela.
  const auto input = baseInput();
  ShadowCascade cascades[MaximumShadowCascades]{};
  const u32 count = computeShadowCascades(input, 3, DefaultCascadeSplitLambda, cascades);
  AE_EXPECT_EQ(count, 3u, "tres cascatas resolvidas");
  for (u32 index = 0; index < count; ++index) {
    const auto &cascade = cascades[index];
    const float far = cascade.farDistance;
    const float tanV = std::tan(input.verticalFovRadians * 0.5f);
    const float corner[3] = {input.cameraPosition[0] + far * tanV * input.aspectRatio,
                             input.cameraPosition[1] + far * tanV,
                             input.cameraPosition[2] + far};
    float clip[4]{};
    transform(cascade.viewProjection, corner, clip);
    AE_EXPECT_TRUE(std::fabs(clip[0]) <= 1.001f, "canto dentro de X da cascata");
    AE_EXPECT_TRUE(std::fabs(clip[1]) <= 1.001f, "canto dentro de Y da cascata");
    AE_EXPECT_TRUE(clip[2] >= -0.001f && clip[2] <= 1.001f, "canto dentro da profundidade");
  }
}

AE_TEST(cascade_mantem_o_volume_quando_a_camera_apenas_gira) {
  // A razão de usar a esfera circunscrita em vez da caixa alinhada à luz: girar a
  // câmera no lugar não pode mudar o tamanho do volume, senão a resolução efetiva
  // da sombra pulsa enquanto o jogador olha em volta.
  auto input = baseInput();
  ShadowCascade straight[MaximumShadowCascades]{};
  computeShadowCascades(input, 3, DefaultCascadeSplitLambda, straight);

  input.cameraForward[0] = 0.7071f;
  input.cameraForward[2] = 0.7071f;
  ShadowCascade turned[MaximumShadowCascades]{};
  computeShadowCascades(input, 3, DefaultCascadeSplitLambda, turned);

  for (u32 index = 0; index < 3; ++index) {
    const float difference = std::fabs(straight[index].radiusWorld - turned[index].radiusWorld);
    AE_EXPECT_TRUE(difference < 1e-3f, "o raio da cascata nao pode depender do yaw");
  }
}

AE_TEST(cascade_crossfade_covers_previous_receiver_slice) {
  auto input = baseInput();
  input.verticalFovRadians = 0.08f;
  input.aspectRatio = 1.0f;
  input.cascadeBlendRatio = 0.5f;
  input.lightDirection[0] = 1.0f;
  input.lightDirection[1] = input.lightDirection[2] = 0.0f;
  ShadowCascade cascades[MaximumShadowCascades]{};
  AE_EXPECT_EQ(computeShadowCascades(input, 4, .75f, cascades), 4u, "four cascades");
  for (u32 index = 1; index < 4; ++index) {
    const float previousStart = index > 1 ? cascades[index - 2].farDistance : 0.0f;
    const float split = cascades[index - 1].farDistance;
    const float start = split - (split - previousStart) * input.cascadeBlendRatio;
    for (int step = 0; step <= 8; ++step) {
      const float depth = start + (split - start) * static_cast<float>(step) / 8.0f;
      const float halfWidth = depth * std::tan(input.verticalFovRadians * 0.5f);
      for (int x = -1; x <= 1; x += 2) {
        for (int y = -1; y <= 1; y += 2) {
          const float point[3] = {input.cameraPosition[0] + x * halfWidth,
                                 input.cameraPosition[1] + y * halfWidth,
                                 input.cameraPosition[2] + depth};
          float clip[4]{};
          transform(cascades[index].viewProjection, point, clip);
          AE_EXPECT_TRUE(std::fabs(clip[0]) <= 1.002f && std::fabs(clip[1]) <= 1.002f &&
                         clip[2] >= -0.002f && clip[2] <= 1.002f,
                         "the incoming cascade covers every blended receiver");
        }
      }
    }
    AE_EXPECT_EQ(cascades[index].nearDistance, split, "nominal split stays unchanged");
  }
}

AE_TEST(cascade_ancora_o_centro_em_texels_inteiros) {
  // Sem ancoragem, cada movimento sub-texel reamostra a sombra num ponto diferente
  // e a borda ferve. O teste move a câmera menos de um texel e exige que o centro
  // da cascata não se mexa.
  auto input = baseInput();
  ShadowCascade before[MaximumShadowCascades]{};
  AE_EXPECT_EQ(computeShadowCascades(input, 1, DefaultCascadeSplitLambda, before), 1u, "resolvida");

  const float texel = before[0].worldUnitsPerTexel;
  AE_EXPECT_TRUE(texel > 0.0f, "texel precisa ter tamanho de mundo conhecido");
  input.cameraPosition[0] += texel * 0.01f;
  ShadowCascade after[MaximumShadowCascades]{};
  computeShadowCascades(input, 1, DefaultCascadeSplitLambda, after);

  const float moved = std::fabs(after[0].centerWorld[0] - before[0].centerWorld[0]) +
                      std::fabs(after[0].centerWorld[1] - before[0].centerWorld[1]) +
                      std::fabs(after[0].centerWorld[2] - before[0].centerWorld[2]);
  AE_EXPECT_TRUE(moved < texel * 0.5f, "movimento sub-texel nao pode deslocar a cascata");
}

AE_TEST(cascade_texel_cresce_com_a_distancia_e_encolhe_com_a_resolucao) {
  auto input = baseInput();
  ShadowCascade cascades[MaximumShadowCascades]{};
  computeShadowCascades(input, 3, DefaultCascadeSplitLambda, cascades);
  for (u32 index = 1; index < 3; ++index) {
    AE_EXPECT_TRUE(cascades[index].worldUnitsPerTexel > cascades[index - 1].worldUnitsPerTexel,
                   "cascata distante cobre mais mundo por texel");
  }
  ShadowCascade dense[MaximumShadowCascades]{};
  input.cascadeResolution = 2048;
  computeShadowCascades(input, 3, DefaultCascadeSplitLambda, dense);
  AE_EXPECT_TRUE(dense[0].worldUnitsPerTexel < cascades[0].worldUnitsPerTexel,
                 "dobrar a resolucao reduz o texel de mundo");
}

AE_TEST(cascade_falha_fechada_em_entrada_degenerada) {
  // Falhar fechado é desenhar sem sombra; falhar aberto seria desenhar com sombra
  // errada, que é pior porque parece intencional.
  ShadowCascade cascades[MaximumShadowCascades]{};
  auto zeroLight = baseInput();
  zeroLight.lightDirection[0] = zeroLight.lightDirection[1] = zeroLight.lightDirection[2] = 0.0f;
  AE_EXPECT_EQ(computeShadowCascades(zeroLight, 3, .75f, cascades), 0u, "luz degenerada");

  auto parallelUp = baseInput();
  parallelUp.cameraUp[0] = 0.0f; parallelUp.cameraUp[1] = 0.0f; parallelUp.cameraUp[2] = 1.0f;
  AE_EXPECT_EQ(computeShadowCascades(parallelUp, 3, .75f, cascades), 0u, "up paralelo ao forward");

  auto zeroResolution = baseInput();
  zeroResolution.cascadeResolution = 0;
  AE_EXPECT_EQ(computeShadowCascades(zeroResolution, 3, .75f, cascades), 0u, "resolucao zero");

  auto badFov = baseInput();
  badFov.verticalFovRadians = 0.0f;
  AE_EXPECT_EQ(computeShadowCascades(badFov, 3, .75f, cascades), 0u, "fov invalido");
}

AE_TEST(cascade_sol_a_pino_nao_colapsa_a_base_da_luz) {
  // Direção da luz paralela ao "up" auxiliar colapsaria o produto vetorial; o
  // módulo troca o auxiliar. Sol a pino é meio-dia, não um caso exótico.
  auto input = baseInput();
  input.lightDirection[0] = 0.0f;
  input.lightDirection[1] = -1.0f;
  input.lightDirection[2] = 0.0f;
  ShadowCascade cascades[MaximumShadowCascades]{};
  AE_EXPECT_EQ(computeShadowCascades(input, 2, DefaultCascadeSplitLambda, cascades), 2u,
               "sol a pino precisa resolver");
  for (u32 index = 0; index < 2; ++index) {
    for (u32 element = 0; element < 16; ++element) {
      AE_EXPECT_TRUE(std::isfinite(cascades[index].viewProjection[element]),
                     "matriz precisa ser finita");
    }
  }
}

AE_TEST(cascade_cula_somente_casters_que_nao_podem_afetar_receptores) {
  const auto input = baseInput();
  ShadowCascade cascade{};
  AE_EXPECT_EQ(computeShadowCascades(input, 1, DefaultCascadeSplitLambda, &cascade), 1u,
               "cascata resolvida");

  const float inside[3] = {cascade.centerWorld[0], cascade.centerWorld[1],
                           cascade.centerWorld[2]};
  AE_EXPECT_TRUE(isShadowCasterVisible(cascade, inside, 1.0f),
                 "caster dentro do receptor permanece");

  const float lateral[3] = {cascade.centerWorld[0] + cascade.radiusWorld * 3.0f,
                            cascade.centerWorld[1], cascade.centerWorld[2]};
  AE_EXPECT_TRUE(!isShadowCasterVisible(cascade, lateral, 1.0f),
                 "caster lateral remoto nao pode sombrear a cascata");

  const float behindLight[3] = {
      cascade.centerWorld[0] - cascade.lightDirection[0] * (cascade.casterExtrusionWorld + 10.0f),
      cascade.centerWorld[1] - cascade.lightDirection[1] * (cascade.casterExtrusionWorld + 10.0f),
      cascade.centerWorld[2] - cascade.lightDirection[2] * (cascade.casterExtrusionWorld + 10.0f)};
  AE_EXPECT_TRUE(!isShadowCasterVisible(cascade, behindLight, 1.0f),
                 "caster atras do olho da luz e removido");

  const float invalid[3] = {NAN, 0.0f, 0.0f};
  AE_EXPECT_TRUE(isShadowCasterVisible(cascade, invalid, 1.0f),
                 "bounds invalidos falham abertos");
}

AE_TEST(cascade_cache_reusa_movimento_dentro_da_margem_e_invalida_fora) {
  auto input = baseInput();
  input.receiverGuardBandRatio = 1.10f;
  ShadowCascade cached{};
  AE_EXPECT_EQ(computeShadowCascades(input, 1, DefaultCascadeSplitLambda, &cached), 1u,
               "cascata cacheada");

  input.cameraPosition[0] += cached.radiusWorld * 0.02f;
  ShadowCascade nearby{};
  computeShadowCascades(input, 1, DefaultCascadeSplitLambda, &nearby);
  AE_EXPECT_TRUE(canReuseStaticShadowCascade(cached, nearby, 1.10f),
                 "movimento pequeno permanece no guard band");

  input.cameraPosition[0] += cached.radiusWorld * 0.20f;
  ShadowCascade distant{};
  computeShadowCascades(input, 1, DefaultCascadeSplitLambda, &distant);
  AE_EXPECT_TRUE(!canReuseStaticShadowCascade(cached, distant, 1.10f),
                 "movimento alem da margem invalida");

  distant.lightDirection[0] = 1.0f;
  distant.lightDirection[1] = 0.0f;
  distant.lightDirection[2] = 0.0f;
  AE_EXPECT_TRUE(!canReuseStaticShadowCascade(cached, distant, 1.10f),
                 "mudanca do sol sempre invalida");
}
