#pragma once
#include "core/base.h"
#include "renderer/authoring_texture.h"
#include <string>

namespace ae::resources {
// Compressão de textura na importação (S1 do plano Sponza).
//
// Referência: Texture Importer da Unity 6000.0 — "Format"/"Compression" por
// plataforma; no Android o padrão da Unity é ASTC 6x6. Aqui a mesma escolha é
// autoral no perfil de importação e o encoder é o da Arm (astc-encoder 5.7.0),
// o mesmo que a Unity e o SDK do Android usam para ASTC.
//
// A cadeia de mips é gerada ANTES, em RGBA8, pelos mesmos filtros de sempre
// (sRGB em espaço linear, normais renormalizadas); cada nível é então codificado
// isoladamente. Codificar não muda largura, altura nem número de níveis — só o
// formato e os bytes —, então orçamento, cache e upload continuam sabendo o que
// cada nível ocupa por `authoringTextureLevelBytes`.
enum class TextureCompression : u8 {
  None = 0,    // RGBA8 (4 bytes/texel)
  Astc4x4 = 4, // 8 bits/texel
  Astc6x6 = 6, // 3,56 bits/texel — padrão da Unity no Android
  Astc8x8 = 8, // 2 bits/texel
};
inline constexpr u32 authoringFormatFor(TextureCompression compression) {
  return compression == TextureCompression::Astc4x4 ? renderer::AuthoringTextureAstc4x4
       : compression == TextureCompression::Astc6x6 ? renderer::AuthoringTextureAstc6x6
       : compression == TextureCompression::Astc8x8 ? renderer::AuthoringTextureAstc8x8
                                                    : renderer::AuthoringTextureRgba8;
}
inline constexpr bool validTextureCompression(u32 value) {
  return value == 0 || value == 4 || value == 6 || value == 8;
}

// Codifica `rgba` (RGBA8, cadeia completa ou a partir de um nível) no ASTC LDR
// pedido; `out` sai com o mesmo tamanho, níveis, espaço de cor e sampler. Um
// contexto por chamada: pode rodar em vários trabalhadores ao mesmo tempo.
// Falha fechada com o motivo em `diagnostic`; `out` fica vazio.
bool compressTexture(const renderer::AuthoringTexture &rgba, TextureCompression compression,
                     renderer::AuthoringTexture &out, std::string &diagnostic);

// Bloco F: prévia do que vai para a GPU. Cadeia RGBA8 a partir do primeiro
// nível com o maior lado até `maximumDimension`, decodificando ASTC quando for o
// caso (em sRGB para texturas de cor) e lendo do derivado em disco os níveis
// que não estão na memória. `firstLevel` diz qual nível da textura virou o 0 da
// prévia. Falha fechada com o motivo em `diagnostic`.
bool previewTexture(const renderer::AuthoringTexture &texture, u32 maximumDimension, renderer::AuthoringTexture &out,
                    u32 &firstLevel, std::string &diagnostic);
} // namespace ae::resources
