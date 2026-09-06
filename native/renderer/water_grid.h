#pragma once

#include "core/base.h"

namespace ae::renderer {

// Contrato da malha camera-relative da água.
//
// A grade já existia, mas só como constante dentro de tools/build-ocean-demo.py
// (`segments = 256, extent = 8000, near_extent = 384`), assada num .aemap que o
// runtime desenha sem saber o que ela é. Isso torna a densidade da água uma
// decisão de cozimento, não de dispositivo: um aparelho de entrada e um de topo
// recebem os mesmos 131 mil triângulos, e nenhum perfil de qualidade consegue
// mudar isso.
//
// Trazer o contrato para cá é o que permite escolher a densidade em runtime. A
// matemática é a mesma de tools/water_geometry.py, deliberadamente — as duas
// precisam concordar enquanto a malha assada existir, e os testes comparam
// contra os mesmos valores.
struct WaterGridSettings final {
  // Múltiplo de 4: a transição cúbica é espelhada em torno do centro e das
  // metades, e um número que não divide por 4 desalinha a costura.
  u32 segments = 256;
  float nearExtent = 384.0f;   // metade central, de espaçamento constante
  float farExtent = 8000.0f;   // alcance total do tapete de água
};

inline constexpr u32 MinimumWaterGridSegments = 8;
inline constexpr u32 MaximumWaterGridSegments = 512;

bool validateWaterGrid(const WaterGridSettings &settings) noexcept;

// Posição, em metros e relativa à câmera, do índice de eixo pedido.
// Fora do intervalo devolve 0 em vez de ler memória alheia.
float waterGridAxisPosition(const WaterGridSettings &settings, u32 index) noexcept;

// Espaçamento incidente conservador: a maior das duas arestas que tocam o
// vértice. É o que um seletor de LOD ou um limitador de banda precisa saber,
// e é sempre o lado longo — subestimar aqui produz aliasing, não economia.
float waterGridAxisSpacing(const WaterGridSettings &settings, u32 index) noexcept;

inline constexpr u32 waterGridVertexCount(const WaterGridSettings &settings) noexcept {
  return (settings.segments + 1u) * (settings.segments + 1u);
}
inline constexpr u32 waterGridTriangleCount(const WaterGridSettings &settings) noexcept {
  return settings.segments * settings.segments * 2u;
}

// Perfis de malha, na mesma escada que os plugins de referência expõem. A
// ASTRA tinha `clipmapLevels` e nada mais; sem uma escada declarada, "reduzir a
// malha em aparelho fraco" não tem onde ser escrito.
// `Inherit` é o valor 0 pela mesma razão dos outros eixos de qualidade: ele é
// serializado no projeto, e um sentinela dentro do enum sobrevive ao round-trip
// sem um campo de presença paralelo que pode dessincronizar.
enum class WaterMeshQuality : u32 {
  Inherit = 0, Ultra = 1, High = 2, Medium = 3, Low = 4, VeryLow = 5, Count = 6,
};

inline constexpr WaterMeshQuality sanitizeWaterMeshQuality(u32 value) noexcept {
  return value < static_cast<u32>(WaterMeshQuality::Count)
             ? static_cast<WaterMeshQuality>(value) : WaterMeshQuality::Inherit;
}
const char *waterMeshQualityName(WaterMeshQuality quality) noexcept;

// Vento acima do qual a malha não densifica mais. O KWS2 usa 15 m/s como teto
// do seu próprio controle de vento, e acima disso o espectro satura de qualquer
// forma: mais triângulo não compra mais onda.
inline constexpr float MaximumWaterGridWindSpeed = 15.0f;

// Densidade escolhida por perfil e por estado de mar.
//
// Mar calmo não precisa de malha de tempestade — é a mesma observação que faz o
// KWS2 escalar o LOD do chunk pelo vento. A diferença aqui é o sentido do
// controle: lá o vento afrouxa o LOD, aqui ele densifica a grade, porque a
// ASTRA parte de uma grade única em vez de uma quadtree de chunks.
//
// farExtent é preservado: encurtar o alcance move a linha do horizonte e é
// mudança de composição, não de desempenho.
//
// `Inherit` aqui significa que ninguém resolveu o eixo antes de chegar na
// geometria, o que é um defeito de integração e não uma escolha do autor. A
// função trata isso como Medium em vez de recusar: um mar sem malha é uma falha
// pior que um mar de densidade média.
WaterGridSettings selectWaterGrid(WaterMeshQuality quality, float windSpeed,
                                  float farExtent) noexcept;

} // namespace ae::renderer
