#include "resources/image_decode.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

// stb_image é código de terceiros: os avisos dele não são avisos da Astra, e a
// Astra compila com -Werror. Silenciados só em volta da inclusão.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#pragma GCC diagnostic ignored "-Wtype-limits"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#pragma GCC diagnostic ignored "-Wcast-function-type"
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif
#define STB_IMAGE_IMPLEMENTATION
// Sem STB_IMAGE_STATIC: funções estáticas não usadas geram -Wunused-function no
// FIM da unidade, depois do pop dos pragmas. Esta é a única unidade que define a
// implementação, então não há símbolo duplicado.
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_NO_THREAD_LOCALS
#include "third_party/stb/stb_image.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace ae::resources {
namespace {
const std::array<float, 256> &srgbToLinearTable() {
  static const auto table = [] {
    std::array<float, 256> values{};
    for (u32 i = 0; i < 256; ++i) {
      const float c = static_cast<float>(i) / 255.0f;
      values[i] = c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
    }
    return values;
  }();
  return table;
}
u8 linearToSrgb(float linear) {
  const float c = std::clamp(linear, 0.0f, 1.0f);
  const float s = c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
  return static_cast<u8>(std::lround(std::clamp(s, 0.0f, 1.0f) * 255.0f));
}
} // namespace

ImageContainer detectImageContainer(std::span<const u8> bytes) {
  static constexpr u8 png[8]{0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
  if (bytes.size() >= 8 && std::memcmp(bytes.data(), png, 8) == 0) return ImageContainer::Png;
  if (bytes.size() >= 3 && bytes[0] == 0xff && bytes[1] == 0xd8 && bytes[2] == 0xff) return ImageContainer::Jpeg;
  return ImageContainer::Unknown;
}

bool readImageDimensions(std::span<const u8> bytes, const ImageDecodeLimits &limits, u32 &width, u32 &height) {
  width = height = 0;
  if (bytes.empty() || bytes.size() > limits.maximumEncodedBytes || bytes.size() > 0x7fffffffu ||
      detectImageContainer(bytes) == ImageContainer::Unknown)
    return false;
  int w = 0, h = 0, channels = 0;
  if (!stbi_info_from_memory(bytes.data(), static_cast<int>(bytes.size()), &w, &h, &channels) || w <= 0 || h <= 0)
    return false;
  if (static_cast<u32>(w) > limits.maximumDimension || static_cast<u32>(h) > limits.maximumDimension ||
      static_cast<u64>(w) * static_cast<u64>(h) > limits.maximumPixels)
    return false;
  width = static_cast<u32>(w);
  height = static_cast<u32>(h);
  return true;
}

bool decodeImageRgba8(std::span<const u8> bytes, const ImageDecodeLimits &limits, DecodedImage &out,
                      std::string &diagnostic) {
  out = {};
  diagnostic.clear();
  if (bytes.empty() || bytes.size() > limits.maximumEncodedBytes || bytes.size() > 0x7fffffffu) {
    diagnostic = "Imagem vazia ou maior que o limite de bytes da importação.";
    return false;
  }
  if (detectImageContainer(bytes) == ImageContainer::Unknown) {
    diagnostic = "Formato de imagem não suportado neste perfil (só PNG e JPEG).";
    return false;
  }
  int width = 0, height = 0, channels = 0;
  // Cabeçalho primeiro: nenhuma alocação de pixels antes de aceitar o tamanho.
  if (!stbi_info_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height, &channels) || width <= 0 ||
      height <= 0) {
    diagnostic = std::string("Cabeçalho de imagem inválido: ") + (stbi_failure_reason() ? stbi_failure_reason() : "desconhecido");
    return false;
  }
  if (static_cast<u32>(width) > limits.maximumDimension || static_cast<u32>(height) > limits.maximumDimension ||
      static_cast<u64>(width) * static_cast<u64>(height) > limits.maximumPixels) {
    diagnostic = "Imagem de " + std::to_string(width) + "x" + std::to_string(height) + " passa do limite desta importação.";
    return false;
  }
  int decodedWidth = 0, decodedHeight = 0, decodedChannels = 0;
  stbi_uc *pixels = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &decodedWidth, &decodedHeight,
                                          &decodedChannels, 4);
  if (!pixels) {
    diagnostic = std::string("Falha ao decodificar imagem: ") + (stbi_failure_reason() ? stbi_failure_reason() : "desconhecido");
    return false;
  }
  if (decodedWidth != width || decodedHeight != height) {
    stbi_image_free(pixels);
    diagnostic = "Imagem com dimensões divergentes entre cabeçalho e dados.";
    return false;
  }
  const usize count = static_cast<usize>(width) * static_cast<usize>(height) * 4;
  out.width = static_cast<u32>(width);
  out.height = static_cast<u32>(height);
  out.rgba.assign(pixels, pixels + count);
  stbi_image_free(pixels);
  return true;
}

u32 mipLevelCount(u32 width, u32 height) {
  u32 levels = 1;
  for (u32 size = std::max(width, height); size > 1; size >>= 1) ++levels;
  return levels;
}

bool buildMipChain(const DecodedImage &base, bool srgb, std::vector<u8> &chain, u32 &levels) {
  chain.clear();
  levels = 0;
  if (!base.width || !base.height || base.rgba.size() != static_cast<usize>(base.width) * base.height * 4) return false;
  levels = mipLevelCount(base.width, base.height);
  usize total = 0;
  for (u32 level = 0, w = base.width, h = base.height; level < levels; ++level, w = std::max(1u, w / 2), h = std::max(1u, h / 2))
    total += static_cast<usize>(w) * h * 4;
  chain.resize(total);
  std::memcpy(chain.data(), base.rgba.data(), base.rgba.size());
  const auto &toLinear = srgbToLinearTable();
  usize previousOffset = 0, offset = base.rgba.size();
  u32 previousWidth = base.width, previousHeight = base.height;
  for (u32 level = 1; level < levels; ++level) {
    const u32 width = std::max(1u, previousWidth / 2), height = std::max(1u, previousHeight / 2);
    for (u32 y = 0; y < height; ++y)
      for (u32 x = 0; x < width; ++x) {
        // Até 2x2 texels da origem; em dimensão ímpar ou 1, repete a borda.
        const u32 x0 = std::min(x * 2, previousWidth - 1), x1 = std::min(x * 2 + 1, previousWidth - 1);
        const u32 y0 = std::min(y * 2, previousHeight - 1), y1 = std::min(y * 2 + 1, previousHeight - 1);
        const auto at = [&](u32 sx, u32 sy) { return chain.data() + previousOffset + (static_cast<usize>(sy) * previousWidth + sx) * 4; };
        const u8 *samples[4]{at(x0, y0), at(x1, y0), at(x0, y1), at(x1, y1)};
        u8 *target = chain.data() + offset + (static_cast<usize>(y) * width + x) * 4;
        for (u32 c = 0; c < 4; ++c) {
          if (srgb && c < 3) {
            float sum = 0;
            for (const auto *sample : samples) sum += toLinear[sample[c]];
            target[c] = linearToSrgb(sum * 0.25f);
          } else {
            u32 sum = 0;
            for (const auto *sample : samples) sum += sample[c];
            target[c] = static_cast<u8>((sum + 2) / 4);
          }
        }
      }
    previousOffset = offset;
    offset += static_cast<usize>(width) * height * 4;
    previousWidth = width;
    previousHeight = height;
  }
  return true;
}
} // namespace ae::resources
