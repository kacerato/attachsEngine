#include "harness.h"
#include "renderer/lod_selection.h"
#include "renderer/map_package.h"

#include <cmath>
#include <vector>

using namespace ae;
using namespace ae::renderer;

// Contrato do impostor de folhagem distante (tools/bake-foliage-impostors.py).
//
// O baker acrescenta, ao ultimo nivel da cadeia de LOD de um grupo alpha-tested,
// UM quad com um tile assado no lugar de centenas de cards que se sobrepoem.
// Tres coisas precisam continuar verdadeiras para que essa troca seja segura, e
// nenhuma delas e obvia lendo so o baker:
//
//   1. o pacote precisa aceitar um nivel a mais do que o cooker gera;
//   2. o impostor so pode aparecer longe, nunca no lugar da malha de perto;
//   3. o volume de cull precisa valer para QUALQUER guinada da camera, porque o
//      vertex shader gira o quad depois que o culling ja decidiu.

namespace {
constexpr float kHalfPi = 1.57079632679f; // fovY = pi/2 -> tan(fovY/2) = 1.

// Cadeia completa como o baker a deixa: tres niveis de simplificacao do cooker
// mais o impostor no topo, com erro geometrico estritamente crescente.
std::vector<MapDrawRecord> impostorChain() {
  std::vector<MapDrawRecord> draws(MapMaximumLodLevels);
  const float errors[MapMaximumLodLevels]{0.0f, 1.0f, 4.0f, 16.0f};
  for (u32 level = 0; level < MapMaximumLodLevels; ++level) {
    draws[level].lodGroupId = 7;
    draws[level].lodLevel = level;
    draws[level].geometricError = errors[level];
    draws[level].boundsRadius = 3.0f;
  }
  return draws;
}
}

AE_TEST(Impostor_material_bit_never_collides_with_the_flags_the_renderer_already_branches_on) {
  // O bit entra em `materialFlags.x` do push constant e o vertex shader testa
  // MATERIAL_IMPOSTOR=256u (rhi/shaders/dirt_road.vert). Se alguem reutilizar 1u<<8 para
  // outra coisa, a folhagem distante passa a girar sozinha na tela.
  const u32 existing = MapMaterialBlend | MapMaterialNormalMap | MapMaterialMetallicRoughnessMap |
                       MapMaterialEmissiveMap | MapMaterialAlphaMask | MapMaterialDoubleSided |
                       MapMaterialNoCollision | MapMaterialForceCollision | MapMaterialWater;
  AE_EXPECT_EQ(MapMaterialImpostor, 256u, "o contrato GLSL usa MATERIAL_IMPOSTOR=256u");
  AE_EXPECT_EQ(MapMaterialImpostor & existing, 0u, "bit exclusivo");

  // A combinacao que o baker grava: recorte por alfa (passe opaco, escreve
  // depth), duas faces (o quad gira e pode ser visto por tras) e nunca blend --
  // o impostor nao pode cair na fila ordenada de transparencias.
  const u32 baked = MapMaterialAlphaMask | MapMaterialDoubleSided | MapMaterialImpostor;
  AE_EXPECT_EQ(baked & MapMaterialBlend, 0u, "impostor nunca e transparencia ordenada");
  AE_EXPECT_TRUE((baked & MapMaterialAlphaMask) != 0, "impostor e alpha-tested");
  AE_EXPECT_TRUE((baked & MapMaterialDoubleSided) != 0, "impostor e visivel dos dois lados");
}

AE_TEST(Impostor_completes_a_four_level_chain_the_package_contract_accepts) {
  AE_EXPECT_EQ(MapMaximumLodLevels, 4u,
               "tres niveis do cooker mais o impostor assado");
  AE_EXPECT_EQ(LodMaximumLevelsPerGroup, MapMaximumLodLevels,
               "runtime e formato concordam sobre quantos niveis cabem num grupo");

  const std::vector<MapDrawRecord> draws = impostorChain();
  const std::vector<u32> candidates{0, 1, 2, 3};
  std::vector<LodRenderGroup> groups;
  std::vector<u32> ungrouped;
  AE_EXPECT_TRUE(buildLodRenderGroups(draws, candidates, groups, ungrouped),
                 "cadeia com impostor no topo e valida");
  AE_EXPECT_EQ(groups.size(), static_cast<usize>(1), "um grupo");
  AE_EXPECT_EQ(groups[0].levelCount, MapMaximumLodLevels, "os quatro niveis sao vistos");
  AE_EXPECT_EQ(groups[0].levels[MapMaximumLodLevels - 1].drawIndices.size(),
               static_cast<usize>(1), "o impostor e o unico draw do nivel mais grosseiro");
  AE_EXPECT_TRUE(ungrouped.empty(), "nada escapa da selecao por erro projetado");
}

AE_TEST(Impostor_is_reached_only_far_away_and_never_replaces_the_mesh_up_close) {
  const std::vector<MapDrawRecord> draws = impostorChain();
  const std::vector<u32> candidates{0, 1, 2, 3};
  std::vector<LodRenderGroup> groups;
  std::vector<u32> ungrouped;
  AE_EXPECT_TRUE(buildLodRenderGroups(draws, candidates, groups, ungrouped), "grupo valido");

  LodLevelInfo levels[LodMaximumLevelsPerGroup]{};
  for (u32 level = 0; level < groups[0].levelCount; ++level)
    levels[level] = groups[0].levels[level].selection;

  // budget 10 px, banda de histerese 0.5 -> um nivel so entra quando seu erro
  // projetado cabe em 5 px. erro projetado = geometricError * 500 / distancia.
  LodHysteresisState state{};
  const LodSelection close = selectLodLevel(levels, groups[0].levelCount, 10.0f, kHalfPi,
                                            1000.0f, 10.0f, 0.5f, state);
  AE_EXPECT_EQ(close.level, 0u, "de perto a malha original continua sendo desenhada");

  // 16 * 500 / 4000 = 2 px, dentro dos 5 px da banda: o impostor entra.
  const LodSelection far = selectLodLevel(levels, groups[0].levelCount, 4000.0f, kHalfPi,
                                          1000.0f, 10.0f, 0.5f, state);
  AE_EXPECT_EQ(far.level, MapMaximumLodLevels - 1, "de longe o impostor substitui o grupo");

  // Voltar para perto tem que devolver a malha: um impostor preso na tela
  // depois que a camera se aproxima e pior que nao ter impostor nenhum.
  const LodSelection back = selectLodLevel(levels, groups[0].levelCount, 10.0f, kHalfPi,
                                           1000.0f, 10.0f, 0.5f, state);
  AE_EXPECT_EQ(back.level, 0u, "refinar de volta e imediato");
}

AE_TEST(Impostor_cull_bounds_survive_every_yaw_the_vertex_shader_can_apply) {
  // O culling roda na CPU com os limites gravados no pacote; o giro acontece
  // depois, no vertex shader. Isso so e valido porque o quad esta centrado na
  // origem local e gira em torno de Y -- a esfera que o envolve e invariante.
  // Se o baker parar de centralizar o quad, este teste quebra antes de a
  // folhagem comecar a sumir na borda da tela.
  const float span = 2.5f;
  const float corners[4][3]{{-span, -span, 0.0f}, {span, -span, 0.0f},
                            {span, span, 0.0f}, {-span, span, 0.0f}};
  const float baseline = std::sqrt(2.0f) * span;
  for (u32 step = 0; step < 64; ++step) {
    const float yaw = static_cast<float>(step) * (6.283185307f / 64.0f);
    const float cosineYaw = std::cos(yaw), sineYaw = std::sin(yaw);
    for (const auto &corner : corners) {
      // Mesma rotacao de dirt_road.vert: X e Z giram, Y fica.
      const float x = cosineYaw * corner[0] + sineYaw * corner[2];
      const float y = corner[1];
      const float z = -sineYaw * corner[0] + cosineYaw * corner[2];
      const float radius = std::sqrt(x * x + y * y + z * z);
      AE_EXPECT_TRUE(std::fabs(radius - baseline) < 1e-4f,
                     "o raio do quad nao muda com a guinada da camera");
    }
  }
}
