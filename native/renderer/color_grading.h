#pragma once

#include "renderer/scene_environment.h"

#include <span>

namespace ae::renderer {

// G2 (DESEMPENHO-GRAFICOS-VIVOS-2026-10-17, item 2): gradação de cor por LUT 3D.
//
// A LUT é montada aqui, na CPU, só quando os parâmetros mudam, e o pós aplica
// UMA leitura de textura 3D por pixel depois do tonemap. Referências:
// Color Lookup e Color Adjustments/White Balance/Lift Gamma Gain/Shadows
// Midtones Highlights da URP 17
// (docs.unity3d.com/Packages/com.unity.render-pipelines.universal@17.0) e
// Adjustments/Color Correction do Environment da Godot 4.5
// (docs.godotengine.org/en/4.5/tutorials/3d/environment_and_post_processing.html).
//
// Domínio: cor já mapeada para o monitor, em RGB linear. A coordenada da LUT é
// sqrt(cor), o que dá às sombras a resolução que um índice linear de 32 passos
// não daria; cada célula guarda a cor graduada, ainda linear (RGBA16F). O
// contraste e a saturação antigos entram na LUT quando ela está ligada, na
// mesma ordem do caminho sem LUT (saturação, depois contraste), então o shader
// faz uma coisa ou outra, nunca as duas.
inline constexpr u32 ColorGradingLutSize = 32;
inline constexpr u32 ColorGradingLutTexels = ColorGradingLutSize * ColorGradingLutSize * ColorGradingLutSize;

struct ColorGradingLook final {
  ColorGrading grading{};
  float contrast = 1.0f;
  float saturation = 1.0f;
  bool operator==(const ColorGradingLook &) const = default;
};

ColorGradingLook colorGradingLook(const SceneEnvironment &environment) noexcept;
// Neutro (padrão e cenas antigas): o pós segue sem LUT, com o custo de antes.
inline bool colorGradingUsesLut(const ColorGradingLook &look) noexcept { return !look.grading.neutral(); }

// A função que a LUT tabela, para um texel. Entrada e saída em RGB linear do
// monitor; a saída pode ficar abaixo de zero com contraste alto, como no
// caminho sem LUT (o shader limita no fim).
void gradeDisplayColor(const ColorGradingLook &look, const float in[3], float out[3]) noexcept;

// Ordem: r mais rápido, depois g, depois b (layout de VkImage 3D). `rgba` tem
// ColorGradingLutTexels * 4 elementos; alfa = 1.
bool buildColorGradingLut(const ColorGradingLook &look, std::span<float> rgba) noexcept;
bool buildColorGradingLutHalf(const ColorGradingLook &look, std::span<u16> rgba) noexcept;

// Mesma leitura trilinear que o shader faz (sqrt, meia célula de borda,
// limitada a [0, 1]). Existe para os testes de host compararem LUT e função.
void sampleColorGradingLut(std::span<const float> rgba, const float in[3], float out[3]) noexcept;

u16 colorGradingFloatToHalf(float value) noexcept;
float colorGradingHalfToFloat(u16 value) noexcept;

} // namespace ae::renderer
