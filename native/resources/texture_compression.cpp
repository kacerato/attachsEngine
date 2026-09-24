#include "resources/texture_compression.h"

#include <astcenc.h>

namespace ae::resources {
bool compressTexture(const renderer::AuthoringTexture &rgba, TextureCompression compression,
                     renderer::AuthoringTexture &out, std::string &diagnostic) {
  out = {};
  diagnostic.clear();
  if (rgba.format != renderer::AuthoringTextureRgba8 || !rgba.valid()) {
    diagnostic = "Compressão de textura precisa de uma cadeia RGBA8 válida.";
    return false;
  }
  const u32 block = static_cast<u32>(compression);
  if (compression == TextureCompression::None) {
    out = rgba;
    return true;
  }
  if (!validTextureCompression(block)) {
    diagnostic = "Formato de compressão desconhecido.";
    return false;
  }
  // Cor em sRGB, dados (normal, metálico/rugosidade, oclusão) lineares. Normal
  // continua RGB comum: o shader lê XYZ, e o modo "normal map" do encoder (X/Y
  // em RG, Z reconstruído) exigiria outro caminho de amostragem.
  astcenc_config config{};
  const float quality = ASTCENC_PRE_FAST; // importação no aparelho: tempo manda
  if (astcenc_config_init(rgba.srgb ? ASTCENC_PRF_LDR_SRGB : ASTCENC_PRF_LDR, block, block, 1, quality, 0, &config) !=
      ASTCENC_SUCCESS) {
    diagnostic = "Configuração ASTC recusada pelo encoder.";
    return false;
  }
  astcenc_context *context = nullptr;
  if (astcenc_context_alloc(&config, 1, &context, nullptr) != ASTCENC_SUCCESS || !context) {
    diagnostic = "Não foi possível criar o contexto do encoder ASTC.";
    return false;
  }
  const astcenc_swizzle swizzle{ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A};
  renderer::AuthoringTexture made;
  made.width = rgba.width;
  made.height = rgba.height;
  made.levels = rgba.levels;
  made.srgb = rgba.srgb;
  made.samplerFlags = rgba.samplerFlags;
  made.format = authoringFormatFor(compression);
  made.mipChain.resize(static_cast<usize>(made.expectedBytes()));
  usize source = 0, target = 0;
  bool ok = true;
  for (u32 level = 0, w = rgba.width, h = rgba.height; ok && level < rgba.levels;
       ++level, w = w > 1 ? w / 2 : 1, h = h > 1 ? h / 2 : 1) {
    // O encoder lê fatias por ponteiro; uma fatia 2D por nível.
    void *slice = const_cast<u8 *>(rgba.mipChain.data() + source);
    astcenc_image image{};
    image.dim_x = w;
    image.dim_y = h;
    image.dim_z = 1;
    image.data_type = ASTCENC_TYPE_U8;
    image.data = &slice;
    const auto bytes = static_cast<usize>(renderer::authoringTextureLevelBytes(made.format, w, h));
    ok = astcenc_compress_image(context, &image, &swizzle, made.mipChain.data() + target, bytes, 0) == ASTCENC_SUCCESS &&
         astcenc_compress_reset(context) == ASTCENC_SUCCESS;
    source += static_cast<usize>(w) * h * 4;
    target += bytes;
  }
  astcenc_context_free(context);
  if (!ok || !made.valid()) {
    diagnostic = "O encoder ASTC falhou em um nível da textura.";
    return false;
  }
  out = std::move(made);
  return true;
}
} // namespace ae::resources
