#pragma once
// Atlas de sombra das luzes locais — a política e a matemática, sem Vulkan.
//
// A sombra direcional já existe em cascatas (renderer/shadow_cascades.h). O que
// falta para um interior é a luz pontual e a spot projetarem sombra: sem isso a
// lâmpada do teto atravessa a mesa e a sala inteira lê como cenário de papel.
//
// Três decisões definem se isso cabe num aparelho, e todas são aritmética:
//
// 1. **Quem recebe sombra.** Não é "todas": o orçamento é finito e gastar o
//    atlas com uma luz de dois pixels na tela tira resolução da que está na
//    cara do jogador. A importância é o tamanho do alcance da luz NA TELA — o
//    mesmo critério que a Unity usa para escolher a resolução no atlas de
//    luzes adicionais.
// 2. **Que tamanho cada mapa tem.** Potências de dois entre um piso e um teto,
//    escolhidas pela importância, com a opção do autor por cima (o Shadow
//    Resolution do Light Inspector: Low, Medium, High, Very High).
// 3. **Onde cada mapa fica.** Alocador em quadtree ("buddy"): os maiores
//    primeiro, cada um alinhado ao próprio tamanho. É o que evita fragmentar o
//    atlas e é o que permite dizer, com número, quanto sobrou.
//
// Nenhuma luz perde a sombra em silêncio: o relatório diz quantas ficaram de
// fora e por quê (distância ou orçamento), para o console publicar.
#include "renderer/punctual_lights.h"

#include <span>

namespace ae::renderer {

// Tiles que o bloco de quadro carrega para o shader. Oito luzes é o teto do
// orçamento; um pontual sozinho já pede seis mapas, então dezesseis é o ponto
// em que duas luzes de cubo e quatro spots cabem juntos sem inflar o UBO.
inline constexpr u32 MaximumLocalShadowTiles = 16;

// Resolução pedida pelo autor, no vocabulário do Light Inspector da Unity
// (Shadow Resolution). `FromQuality` deixa a decisão com a política — que é o
// que um aparelho precisa, porque o mesmo projeto roda em telas e GPUs
// diferentes.
enum class ShadowResolution : u8 { FromQuality = 0, Low = 1, Medium = 2, High = 3, VeryHigh = 4 };

// Uma luz local candidata a projetar sombra neste quadro.
struct ShadowCaster final {
  u32 lightIndex = 0; // índice na lista de luzes do quadro, para o shader reatar
  LightModality modality = LightModality::Point;
  float position[3]{};
  float direction[3]{0, -1, 0};
  float range = 10;
  float outerAngleDegrees = 35; // meio-ângulo do cone, como no componente Luz
  ShadowResolution resolution = ShadowResolution::FromQuality;
  // Near Plane do Inspector: perto demais estoura a precisão da profundidade,
  // longe demais corta o que está junto da lâmpada.
  float nearPlane = .05f;
  bool enabled = true;
};

// Um mapa dentro do atlas. Spot ocupa um; pontual ocupa seis, um por face do
// cubo, todos do mesmo tamanho — uma face em resolução diferente faria a sombra
// mudar de nitidez conforme o lado para onde o objeto anda.
struct ShadowAtlasTile final {
  u32 caster = 0;
  u32 face = 0; // spot: 0. Pontual: +X, -X, +Y, -Y, +Z, -Z nesta ordem.
  u32 x = 0, y = 0, size = 0;
  float viewProjection[16]{};
  float nearPlane = 0, farPlane = 0;
  // Unidades de mundo por texel no limite do alcance. É a escala certa para o
  // desvio de profundidade: medido em texels, não em metros, senão a luz de
  // alcance grande recebe um desvio pequeno demais e ganha acne.
  float worldUnitsPerTexelAtRange = 0;
};

struct ShadowAtlasInput final {
  // Lado do atlas em texels. É orçamento de aparelho, não gosto: 2048² de
  // profundidade custa 8 MB, 4096² custa 32 MB.
  u32 atlasResolution = 2048;
  // Área já ocupada por outro uso do mesmo atlas (as cascatas do sol, quando
  // compartilham a imagem). Os tiles locais começam depois dela, em linhas
  // inteiras, para que a política não precise conhecer o que o sol fez.
  u32 reservedRows = 0;
  u32 maximumTiles = 24;
  u32 minimumTileSize = 128, maximumTileSize = 1024;
  float cameraPosition[3]{};
  float verticalFovRadians = 1.0472f;
  // Distância máxima em que luz local projeta sombra, como o Max Distance das
  // sombras da Unity. Além disso a luz continua iluminando, sem ocluir.
  float shadowDistance = 60;
};

struct ShadowAtlasReport final {
  u32 tiles = 0;
  u32 castersWithShadow = 0;
  u32 droppedByDistance = 0;
  u32 droppedByBudget = 0;
  u32 usedTexels = 0;
  float occupancy = 0; // fração da área do atlas ocupada pelos tiles locais
};

// Resolve o atlas. Devolve quantos tiles foram escritos em `out`.
//
// Falha fechada: entrada inválida devolve 0 e um relatório zerado — sem atlas,
// quem chama desenha sem sombra local, em vez de com sombra errada.
u32 buildShadowAtlas(const ShadowAtlasInput &input, std::span<const ShadowCaster> casters,
                     std::span<ShadowAtlasTile> out, ShadowAtlasReport &report);

// Tamanho de tile que a política daria a este caster, isolado. Existe para o
// Inspector poder dizer ao autor o que "Automática" significa hoje, sem montar
// o atlas inteiro.
u32 shadowTileSizeFor(const ShadowAtlasInput &input, const ShadowCaster &caster);

} // namespace ae::renderer
