#pragma once

#include "core/base.h"

namespace ae::renderer {

// Bits 0 e 1 são o contrato histórico dos pacotes e das texturas de autoria:
// filtro linear único e interpolação linear entre mips. O marcador mantém esses
// assets com a leitura antiga; somente fontes novas usam os quatro eixos abaixo.
inline constexpr u32 TextureSamplerIndependent = 1u << 24;
inline constexpr u32 TextureSamplerMinLinear = 1u << 25;
inline constexpr u32 TextureSamplerMagLinear = 1u << 26;
inline constexpr u32 TextureSamplerMipEnabled = 1u << 27;
inline constexpr u32 TextureSamplerMipLinear = 1u << 28;

struct TextureSamplerState final {
  bool minLinear = true;
  bool magLinear = true;
  bool mipEnabled = true;
  bool mipLinear = true;
};

inline TextureSamplerState decodeTextureSampler(u32 flags) noexcept {
  if ((flags & TextureSamplerIndependent) == 0) {
    const bool linear = (flags & 1u) != 0;
    return {linear, linear, true, (flags & 2u) != 0};
  }
  return {(flags & TextureSamplerMinLinear) != 0,
          (flags & TextureSamplerMagLinear) != 0,
          (flags & TextureSamplerMipEnabled) != 0,
          (flags & TextureSamplerMipLinear) != 0};
}

// glTF 2.0, sampler.minFilter/magFilter. Campos omitidos chegam com os defaults
// de filtragem automática usados pela Astra: LINEAR_MIPMAP_LINEAR e LINEAR.
inline u32 encodeGltfTextureSampler(u32 minFilter = 9987u, u32 magFilter = 9729u) noexcept {
  u32 flags = TextureSamplerIndependent;
  if (magFilter == 9729u) flags |= TextureSamplerMagLinear;
  switch (minFilter) {
    case 9728u: break;
    case 9729u: flags |= TextureSamplerMinLinear; break;
    case 9984u: flags |= TextureSamplerMipEnabled; break;
    case 9985u: flags |= TextureSamplerMinLinear | TextureSamplerMipEnabled; break;
    case 9986u: flags |= TextureSamplerMipEnabled | TextureSamplerMipLinear; break;
    default: flags |= TextureSamplerMinLinear | TextureSamplerMipEnabled | TextureSamplerMipLinear; break;
  }
  // Aproximação para runtimes antigos, sem reatribuir o significado desses bits.
  if (magFilter == 9729u) flags |= 1u;
  if ((flags & TextureSamplerMipLinear) != 0) flags |= 2u;
  return flags;
}

} // namespace ae::renderer
