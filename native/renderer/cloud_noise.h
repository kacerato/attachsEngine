#pragma once

// G4d: ruídos 3D das Volumetric Clouds (HDRP 17), no modelo de Schneider,
// "The Real-time Volumetric Cloudscapes of Horizon Zero Dawn" (SIGGRAPH 2015),
// que é a base das nuvens do HDRP. Matemática pura, testável no host.
//
// Forma (64³ RGBA8, repetível nos três eixos):
//   R  Perlin-Worley: fBm de Perlin remapeado pelo Worley de baixa frequência
//      (os "rolos" arredondados dos cúmulos)
//   G,B,A Worley invertido em 3 frequências crescentes (fBm de forma)
// Erosão (32³ RGBA8): Worley invertido em 3 frequências, corrói as bordas.

#include "core/base.h"
#include <vector>

namespace ae::renderer {

inline constexpr u32 CloudShapeNoiseSize = 64;
inline constexpr u32 CloudErosionNoiseSize = 32;

bool buildCloudShapeNoise(u32 size, u32 seed, std::vector<u8> &output);
bool buildCloudErosionNoise(u32 size, u32 seed, std::vector<u8> &output);

// Cadeia de mips de um volume RGBA8 cúbico e repetível (caixa 2x2x2): o nível
// 0 seguido dos menores, até 1x1x1, no layout que vkCmdCopyBufferToImage lê
// nível a nível. Sem mips o ruído longe vira moiré.
bool buildCloudNoiseMips(const std::vector<u8> &level0, u32 size, std::vector<u8> &output);
inline u32 cloudNoiseMipCount(u32 size) noexcept { u32 n = 1; while (size > 1) { size >>= 1; ++n; } return n; }

// Worley repetível (1 = sobre um ponto de célula, 0 = longe), período `cells`.
float tileableWorley(float x, float y, float z, u32 cells, u32 seed) noexcept;

} // namespace ae::renderer
