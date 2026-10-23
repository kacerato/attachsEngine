#pragma once

// G4b: Cloud Layer (HDRP 17) para mobile. Matemática pura, testável no host.
//
// O mapa de nuvens faz o papel do DefaultCloudMap da Unity, mas é gerado: um
// RGBA8 repetível nos dois eixos, com cadeia de mips, montado uma vez na CPU.
//   R  cúmulos: fBm de ruído de gradiente periódico (5 oitavas)
//   G  cirros: o mesmo ruído esticado ao longo de X (faixas)
//   B  detalhe: fBm de frequência alta que corrói as bordas dos cúmulos
// O shader (rhi/shaders/cloud_layer.glsl) e cloudLayerAlpha abaixo usam a mesma
// regra de cobertura, para que o teste de host diga o que a GPU desenha.

#include "core/base.h"
#include "renderer/scene_environment.h"
#include <vector>

namespace ae::renderer {

inline constexpr u32 CloudMapSize = 256;

// Cadeia completa (256² até 1²) em RGBA8 linear.
bool buildCloudMapTexture(u32 size, u32 seed, std::vector<u8> &output);

// Leitura bilinear repetível do nível 0 (uv em repetições do mapa).
void sampleCloudMap(const std::vector<u8> &map, u32 size, float u, float v, float out[3]) noexcept;

// Opacidade de uma amostra do mapa (cúmulo, cirro, detalhe já em 0..1).
float cloudLayerAlpha(const CloudLayer &clouds, float cumulus, float cirrus, float detail) noexcept;

// Linhas do bloco de quadro (DirtRoadFrameUniform, offset 3536):
//   parameters  opacidade (0 sem nuvens), cobertura, cirros, densidade
//   shape       1/tamanho, altitude, deslocamento do vento x, z (m, < tamanho)
//   tint        tom * 2^exposição, atenuação do ambiente
//   shadow      força da sombra (0 sem sombra), espessura m, tipo (1 volumétrica), passos
//   volume      (offset 4112) fator de forma, erosão, reservados
// O vento entra como deslocamento já reduzido ao tamanho do mapa (o mapa se
// repete), calculado em double: o ponto flutuante do shader não perde precisão
// depois de horas de Play.
struct CloudLayerRows final {
  float parameters[4]{};
  float shape[4]{};
  float tint[4]{};
  float shadow[4]{};
  float volume[4]{};
};
CloudLayerRows packCloudLayer(const CloudLayer &clouds, bool environmentActive, double timeSeconds) noexcept;

// A camada é uma casca esférica concêntrica ao planeta (raio da Terra), como
// as nuvens do HDRP: a distância até ela cresce até ~sqrt(2Rh) no horizonte
// (160 km a 2 km de altura) em vez de ir ao infinito como num plano, e o
// tamanho aparente das nuvens diminui até virar uma faixa densa lá embaixo.
// Forma estável (sem cancelamento perto do zênite); o shader usa a mesma.
inline constexpr float CloudPlanetRadiusMeters = 6371000.0f;
float cloudShellDistance(float heightMeters, float viewSine) noexcept;

// Quanto do céu a camada esconde (0..1): o ambiente do céu passa ao cinza do
// encoberto nessa proporção, como a sonda de ambiente do HDRP que vê as nuvens.
float cloudSkyOcclusion(const CloudLayer &clouds) noexcept;

} // namespace ae::renderer
