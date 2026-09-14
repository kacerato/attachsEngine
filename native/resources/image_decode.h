#pragma once
#include "core/base.h"
#include <span>
#include <string>
#include <vector>

namespace ae::resources {
// Decodificação de imagens embutidas em fontes importadas (M09.1, Entrega 3).
//
// Backend: stb_image v2.30 (domínio público / MIT), fixado no commit
// 2c980bb59875b0d32144a71867fbdebb2f77cd20 — ver
// `native/third_party/stb/VERSION.txt`. Escolhido por decodificar igual no
// aparelho e no host: o decodificador do sistema (`AImageDecoder`) só existe a
// partir do Android 11 e o mínimo da Astra é o 8 (API 26).
//
// Só PNG e JPEG. Os limites valem ANTES de alocar: as dimensões são lidas do
// cabeçalho e recusadas primeiro, porque o tamanho comprimido de um GLB não diz
// quanta memória uma imagem pede depois de aberta.
struct ImageDecodeLimits {
  u32 maximumDimension = 8192;
  u64 maximumPixels = 16ull * 1024 * 1024;   // 16 MP por imagem
  u64 maximumEncodedBytes = 64ull << 20;     // bytes da imagem dentro da fonte
};

enum class ImageContainer : u8 { Unknown, Png, Jpeg };

struct DecodedImage {
  u32 width = 0, height = 0;
  std::vector<u8> rgba; // RGBA8, linha 0 no topo, 4 bytes por pixel
};

ImageContainer detectImageContainer(std::span<const u8> bytes);

// Só o cabeçalho: dimensões de uma imagem que `decodeImageRgba8` aceitaria.
// Serve para planejar memória antes de decodificar qualquer pixel.
bool readImageDimensions(std::span<const u8> bytes, const ImageDecodeLimits &limits, u32 &width, u32 &height);

// Falha fechada: em qualquer erro `out` volta vazio e `diagnostic` diz o motivo.
bool decodeImageRgba8(std::span<const u8> bytes, const ImageDecodeLimits &limits, DecodedImage &out,
                      std::string &diagnostic);

// Cadeia completa de mipmaps (níveis concatenados, maior primeiro) por média
// 2x2. Com `srgb`, a média é feita em espaço linear e volta a sRGB — média
// direta de valores sRGB escurece a textura ao longe. Alfa e mapas de dados
// (normal, metálico/rugosidade, oclusão) são médios diretos.
u32 mipLevelCount(u32 width, u32 height);
bool buildMipChain(const DecodedImage &base, bool srgb, std::vector<u8> &chain, u32 &levels);
} // namespace ae::resources
