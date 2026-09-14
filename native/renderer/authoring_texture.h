#pragma once
#include "core/base.h"
#include <memory>
#include <vector>

namespace ae::renderer {
// Textura decodificada de uma fonte importada, pronta para subir (M09.1).
//
// Sem Vulkan: o importador produz isto num worker e o renderer só cria a imagem
// e o sampler. `mipChain` traz todos os níveis RGBA8 concatenados, do maior para
// o menor. Os bits 0..3 de `samplerFlags` têm o MESMO significado de
// `MapTextureRecord::flags` do pacote de mapa; os bits 4..5 acrescentam o
// espelhamento que o glTF permite e o pacote não usava.
inline constexpr u32 AuthoringTextureLinearFilter = 1u << 0;
inline constexpr u32 AuthoringTextureLinearMip = 1u << 1;
inline constexpr u32 AuthoringTextureRepeatU = 1u << 2;
inline constexpr u32 AuthoringTextureRepeatV = 1u << 3;
inline constexpr u32 AuthoringTextureMirrorU = 1u << 4;
inline constexpr u32 AuthoringTextureMirrorV = 1u << 5;

// Formato de `mipChain` (Entrega 4). RGBA8: 4 bytes por texel. ASTC 4x4: um bloco
// de 16 bytes por 4x4 texels (arredondado para cima) — só quando o aparelho
// amostra ASTC 4x4 e a fonte é KTX2 com a cadeia completa de mips.
inline constexpr u32 AuthoringTextureRgba8 = 0;
inline constexpr u32 AuthoringTextureAstc4x4 = 1;

struct AuthoringTexture {
  u32 width = 0, height = 0, levels = 0;
  // Cor base e emissivo são sRGB; normal e metálico/rugosidade são dados.
  bool srgb = true;
  u32 samplerFlags = AuthoringTextureLinearFilter | AuthoringTextureLinearMip | AuthoringTextureRepeatU | AuthoringTextureRepeatV;
  u32 format = AuthoringTextureRgba8;
  std::vector<u8> mipChain;
  u64 expectedBytes() const {
    if (format != AuthoringTextureRgba8 && format != AuthoringTextureAstc4x4) return 0;
    u64 total = 0;
    for (u32 level = 0, w = width, h = height; level < levels; ++level, w = w > 1 ? w / 2 : 1, h = h > 1 ? h / 2 : 1)
      total += format == AuthoringTextureAstc4x4 ? static_cast<u64>((w + 3) / 4) * ((h + 3) / 4) * 16
                                                 : static_cast<u64>(w) * h * 4;
    return total;
  }
  bool valid() const { return width && height && levels && mipChain.size() == expectedBytes(); }
};
// Compartilhada e imutável: a sessão copia blocos de fonte (candidato,
// estado anterior, reversão) e dezenas de MB de mips não podem ir junto.
using SharedAuthoringTexture = std::shared_ptr<const AuthoringTexture>;
} // namespace ae::renderer
