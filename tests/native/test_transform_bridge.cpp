#include "harness.h"
#include "transform/transform_bridge.h"

#include <cmath>

namespace {

bool near(float a, float b) { return std::abs(a - b) <= 1.0e-5F; }

AetherTransform translated(float x, float y, float z) {
  return {{x, y, z}, {0.0F, 0.0F, 0.0F, 1.0F}, {1.0F, 1.0F, 1.0F}};
}

} // namespace

AE_TEST(transform_batch_propaga_raizes_e_cadeia_topologica) {
  const AetherTransform local[] = {
      translated(10.0F, 0.0F, 0.0F),
      translated(0.0F, 2.0F, 0.0F),
      translated(0.0F, 0.0F, 3.0F),
      translated(-4.0F, 0.0F, 0.0F),
  };
  const std::int32_t parents[] = {-1, 0, 1, -1};
  AetherTransform world[4]{};

  AE_EXPECT_TRUE(AetherTransform_Propagate(local, parents, world, 4) == 1,
                 "um plano topologico valido deve ser aceito");
  AE_EXPECT_TRUE(near(world[2].position[0], 10.0F) &&
                     near(world[2].position[1], 2.0F) &&
                     near(world[2].position[2], 3.0F),
                 "a composicao deve acumular toda a cadeia");
  AE_EXPECT_TRUE(near(world[3].position[0], -4.0F),
                 "cada raiz deve preservar seu transform local");
}

AE_TEST(transform_batch_aplica_escala_rotacao_e_normalizacao) {
  constexpr float halfRoot = 0.70710678118F;
  const AetherTransform local[] = {
      {{1.0F, 2.0F, 3.0F}, {0.0F, 0.0F, halfRoot, halfRoot}, {2.0F, 3.0F, 4.0F}},
      {{1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F, 2.0F}, {5.0F, 6.0F, 7.0F}},
  };
  const std::int32_t parents[] = {-1, 0};
  AetherTransform world[2]{};

  AE_EXPECT_TRUE(AetherTransform_Propagate(local, parents, world, 2) == 1,
                 "o lote deve aceitar rotacao e escala nao uniforme");
  AE_EXPECT_TRUE(near(world[1].position[0], 1.0F) &&
                     near(world[1].position[1], 4.0F) &&
                     near(world[1].position[2], 3.0F),
                 "a posicao deve aplicar escala e depois rotacao do pai");
  AE_EXPECT_TRUE(near(world[1].scale[0], 10.0F) &&
                     near(world[1].scale[1], 18.0F) &&
                     near(world[1].scale[2], 28.0F),
                 "a escala deve ser composta componente a componente");
  const float rotationLength = std::sqrt(
      world[1].rotation[0] * world[1].rotation[0] +
      world[1].rotation[1] * world[1].rotation[1] +
      world[1].rotation[2] * world[1].rotation[2] +
      world[1].rotation[3] * world[1].rotation[3]);
  AE_EXPECT_TRUE(near(rotationLength, 1.0F),
                 "a rotacao composta deve permanecer normalizada");
}

AE_TEST(transform_batch_rejeita_parent_fora_da_ordem_topologica) {
  const AetherTransform local[] = {translated(0.0F, 0.0F, 0.0F)};
  const std::int32_t parents[] = {0};
  AetherTransform world[1]{};
  AE_EXPECT_TRUE(AetherTransform_Propagate(local, parents, world, 1) == 0,
                 "um filho nao pode apontar para si ou para uma entrada futura");
}

AE_TEST(transform_plan_compone_diretamente_nos_enderecos_do_ecs) {
  AetherTransform local[] = {
      translated(2.0F, 0.0F, 0.0F),
      translated(0.0F, 3.0F, 0.0F),
      translated(0.0F, 0.0F, 4.0F),
  };
  AetherTransform world[3]{};
  const AetherTransformPlanEntry entries[] = {
      {&local[0], &world[0], -1, 0},
      {&local[1], &world[1], 0, 0},
      {&local[2], &world[2], 1, 0},
  };

  AE_EXPECT_TRUE(AetherTransform_PropagatePlan(entries, 3) == 1,
                 "o plano por enderecos deve ser aceito");
  AE_EXPECT_TRUE(near(world[2].position[0], 2.0F) &&
                     near(world[2].position[1], 3.0F) &&
                     near(world[2].position[2], 4.0F),
                 "o kernel deve ler e escrever diretamente os componentes indicados");
}
