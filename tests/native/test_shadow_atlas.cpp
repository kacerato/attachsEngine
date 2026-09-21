// Atlas de sombra das luzes locais (G6-B).
//
// Protegido aqui: pontual ocupa seis faces ou nenhuma; a importância na tela
// decide resolução e ordem de atendimento; o que não cabe é contado, nunca
// some em silêncio; os tiles não se sobrepõem nem invadem as linhas
// reservadas; e a projeção de cada face leva o mundo para dentro do mapa.
#include "harness.h"
#include "renderer/shadow_atlas.h"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace ae;
using namespace ae::renderer;

namespace {
ShadowCaster point(float x, float z, float range) {
  ShadowCaster caster;
  caster.modality = LightModality::Point;
  caster.position[0] = x;
  caster.position[1] = 2;
  caster.position[2] = z;
  caster.range = range;
  return caster;
}

ShadowCaster spot(float x, float z, float range, float halfAngle = 35) {
  ShadowCaster caster = point(x, z, range);
  caster.modality = LightModality::Spot;
  caster.direction[0] = 0;
  caster.direction[1] = -1;
  caster.direction[2] = 0;
  caster.outerAngleDegrees = halfAngle;
  return caster;
}

ShadowAtlasInput standard() {
  ShadowAtlasInput input;
  input.atlasResolution = 2048;
  input.minimumTileSize = 128;
  input.maximumTileSize = 512;
  input.maximumTiles = 24;
  input.shadowDistance = 60;
  return input;
}

// Leva um ponto do mundo para o quadrado do mapa, com profundidade em [0,1].
bool project(const ShadowAtlasTile &tile, const float world[3], float out[3]) {
  float clip[4]{};
  for (u32 row = 0; row < 4; ++row)
    clip[row] = tile.viewProjection[row] * world[0] + tile.viewProjection[4 + row] * world[1] +
                tile.viewProjection[8 + row] * world[2] + tile.viewProjection[12 + row];
  if (!(clip[3] > 1e-5f)) return false;
  out[0] = clip[0] / clip[3];
  out[1] = clip[1] / clip[3];
  out[2] = clip[2] / clip[3];
  return true;
}

bool overlap(const ShadowAtlasTile &a, const ShadowAtlasTile &b) {
  return a.x < b.x + b.size && b.x < a.x + a.size && a.y < b.y + b.size && b.y < a.y + a.size;
}
} // namespace

AE_TEST(a_point_light_takes_six_faces_or_none) {
  const auto input = standard();
  const ShadowCaster casters[]{point(0, 4, 8)};
  std::vector<ShadowAtlasTile> tiles(16);
  ShadowAtlasReport report;
  const u32 written = buildShadowAtlas(input, casters, tiles, report);
  AE_EXPECT_EQ(written, 6u, "as seis faces do cubo entram juntas");
  AE_EXPECT_EQ(report.castersWithShadow, 1u, "uma luz com sombra");
  for (u32 face = 0; face < 6; ++face) {
    AE_EXPECT_EQ(tiles[face].face, face, "as faces saem na ordem do cubo");
    AE_EXPECT_EQ(tiles[face].size, tiles[0].size, "todas as faces na mesma resolução");
  }
  // Meio cubo seria pior do que cubo nenhum: a sombra apareceria e sumiria
  // conforme o objeto anda em volta da lâmpada.
  std::vector<ShadowAtlasTile> tight(5);
  ShadowAtlasReport tightReport;
  AE_EXPECT_EQ(buildShadowAtlas(input, casters, tight, tightReport), 0u, "não cabe meio cubo");
  AE_EXPECT_EQ(tightReport.droppedByBudget, 1u, "e a luz recusada é contada");
  AE_EXPECT_EQ(tightReport.castersWithShadow, 0u, "sem sombra pela metade");
}

AE_TEST(screen_size_decides_resolution_and_order) {
  auto input = standard();
  input.maximumTileSize = 512;
  // Perto e longe, mesmo alcance: a de perto ocupa mais tela e merece mais
  // resolução — é o mesmo critério do nível de LOD.
  const ShadowCaster casters[]{spot(0, 60, 6), spot(0, 6, 6)};
  std::vector<ShadowAtlasTile> tiles(8);
  ShadowAtlasReport report;
  const u32 written = buildShadowAtlas(input, casters, tiles, report);
  AE_EXPECT_EQ(written, 2u, "as duas recebem mapa");
  AE_EXPECT_EQ(tiles[0].caster, 1u, "a mais próxima é atendida primeiro");
  AE_EXPECT_TRUE(tiles[0].size > tiles[1].size, "a mais próxima recebe tile maior");

  // A escolha do autor manda sobre a política, como o Shadow Resolution do
  // Light Inspector.
  ShadowCaster forced[]{spot(0, 60, 6), spot(0, 6, 6)};
  forced[0].resolution = ShadowResolution::High;
  forced[1].resolution = ShadowResolution::Low;
  std::vector<ShadowAtlasTile> chosen(8);
  ShadowAtlasReport chosenReport;
  AE_EXPECT_EQ(buildShadowAtlas(input, forced, chosen, chosenReport), 2u, "as duas continuam entrando");
  for (u32 index = 0; index < 2; ++index) {
    const auto &tile = chosen[index];
    if (tile.caster == 0) AE_EXPECT_EQ(tile.size, 512u, "Alta respeita o teto do atlas");
    else AE_EXPECT_EQ(tile.size, 256u, "Baixa é 256, mesmo estando perto");
  }
}

AE_TEST(distance_and_budget_refuse_without_silence) {
  auto input = standard();
  input.shadowDistance = 20;
  const ShadowCaster casters[]{spot(0, 200, 4), spot(0, 5, 4)};
  std::vector<ShadowAtlasTile> tiles(8);
  ShadowAtlasReport report;
  AE_EXPECT_EQ(buildShadowAtlas(input, casters, tiles, report), 1u, "só a que está dentro do alcance");
  AE_EXPECT_EQ(report.droppedByDistance, 1u, "a distante é contada, não esquecida");

  // Teto de tiles: o orçamento acaba na luz menos importante.
  input.shadowDistance = 200;
  input.maximumTiles = 2;
  std::vector<ShadowCaster> many;
  for (u32 index = 0; index < 6; ++index) many.push_back(spot(static_cast<float>(index) * 3, 6, 4));
  std::vector<ShadowAtlasTile> few(16);
  ShadowAtlasReport fewReport;
  AE_EXPECT_EQ(buildShadowAtlas(input, many, few, fewReport), 2u, "duas cabem no teto");
  AE_EXPECT_EQ(fewReport.droppedByBudget, 4u, "as quatro recusadas aparecem no relatório");
  AE_EXPECT_TRUE(fewReport.occupancy > 0 && fewReport.occupancy < 1, "a ocupação do atlas é publicada");
}

AE_TEST(tiles_never_overlap_and_respect_the_reserved_rows) {
  auto input = standard();
  // A metade de cima do atlas é do sol: as luzes locais não podem tocá-la.
  input.reservedRows = 1024;
  input.maximumTiles = 24;
  std::vector<ShadowCaster> casters;
  for (u32 index = 0; index < 4; ++index) casters.push_back(point(static_cast<float>(index) * 6, 8, 7));
  std::vector<ShadowAtlasTile> tiles(24);
  ShadowAtlasReport report;
  const u32 written = buildShadowAtlas(input, casters, tiles, report);
  AE_EXPECT_TRUE(written >= 6 && written % 6 == 0, "cubos inteiros");
  for (u32 a = 0; a < written; ++a) {
    AE_EXPECT_TRUE(tiles[a].y >= input.reservedRows, "nenhum tile invade as linhas reservadas");
    AE_EXPECT_TRUE(tiles[a].x + tiles[a].size <= input.atlasResolution &&
                   tiles[a].y + tiles[a].size <= input.atlasResolution, "tile dentro do atlas");
    AE_EXPECT_TRUE(tiles[a].x % tiles[a].size == 0 && tiles[a].y % tiles[a].size == 0,
                   "cada tile alinhado ao próprio tamanho");
    for (u32 b = a + 1; b < written; ++b)
      AE_EXPECT_TRUE(!overlap(tiles[a], tiles[b]), "dois mapas nunca dividem texel");
  }
}

AE_TEST(each_face_projects_the_world_into_its_map) {
  const auto input = standard();
  const ShadowCaster casters[]{point(0, 0, 10)};
  std::vector<ShadowAtlasTile> tiles(8);
  ShadowAtlasReport report;
  AE_EXPECT_EQ(buildShadowAtlas(input, casters, tiles, report), 6u, "cubo inteiro");
  // Um ponto a 3 m à frente de cada face cai dentro do quadrado do mapa, com
  // profundidade entre o plano próximo e o alcance.
  const float offsets[6][3]{{3, 2, 0}, {-3, 2, 0}, {0, 5, 0}, {0, -1, 0}, {0, 2, 3}, {0, 2, -3}};
  for (u32 face = 0; face < 6; ++face) {
    float projected[3]{};
    AE_EXPECT_TRUE(project(tiles[face], offsets[face], projected), "o ponto está à frente da face");
    AE_EXPECT_TRUE(std::fabs(projected[0]) <= 1.001f && std::fabs(projected[1]) <= 1.001f,
                   "dentro do quadrado do mapa");
    AE_EXPECT_TRUE(projected[2] >= 0 && projected[2] <= 1, "profundidade na convenção do Vulkan");
  }
  // O que está atrás da face não pode entrar nela.
  float behind[3]{-3, 2, 0};
  float projected[3]{};
  AE_EXPECT_TRUE(!project(tiles[0], behind, projected) || std::fabs(projected[0]) > 1,
                 "a face +X não enxerga o lado -X");

  // Spot: o cone cabe no mapa e a borda do cone ainda está dentro.
  ShadowCaster cone = spot(0, 0, 12, 30);
  const ShadowCaster spotOnly[]{cone};
  std::vector<ShadowAtlasTile> spotTiles(4);
  ShadowAtlasReport spotReport;
  AE_EXPECT_EQ(buildShadowAtlas(input, spotOnly, spotTiles, spotReport), 1u, "spot ocupa um mapa só");
  const float depth = 5.0f;
  const float edge = depth * std::tan(30.0f * 0.0174532925f);
  const float onCone[3]{edge, cone.position[1] - depth, 0};
  AE_EXPECT_TRUE(project(spotTiles[0], onCone, projected), "a borda do cone está à frente");
  AE_EXPECT_TRUE(std::fabs(projected[0]) <= 1.0f, "a borda do cone cabe no mapa");
  AE_EXPECT_TRUE(spotTiles[0].worldUnitsPerTexelAtRange > 0, "o tamanho do texel no alcance é publicado");
}
