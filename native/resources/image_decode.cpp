#include "resources/image_decode.h"
#include "resources/gltf_codecs.h"

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
#define STBI_ONLY_HDR
#define STBI_NO_STDIO
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
  if (isKtx2(bytes)) return ImageContainer::Ktx2;
  static constexpr char radiance[] = "#?RADIANCE";
  static constexpr char rgbe[] = "#?RGBE";
  if ((bytes.size() >= sizeof(radiance)-1 && std::memcmp(bytes.data(),radiance,sizeof(radiance)-1)==0) ||
      (bytes.size() >= sizeof(rgbe)-1 && std::memcmp(bytes.data(),rgbe,sizeof(rgbe)-1)==0))
    return ImageContainer::RadianceHdr;
  return ImageContainer::Unknown;
}

bool readImageDimensions(std::span<const u8> bytes, const ImageDecodeLimits &limits, u32 &width, u32 &height) {
  width = height = 0;
  if (bytes.empty() || bytes.size() > limits.maximumEncodedBytes || bytes.size() > 0x7fffffffu ||
      (detectImageContainer(bytes) != ImageContainer::Png &&
       detectImageContainer(bytes) != ImageContainer::Jpeg &&
       detectImageContainer(bytes) != ImageContainer::Ktx2))
    return false;
  const auto withinLimits = [&](u32 w, u32 h) {
    return w <= limits.maximumDimension && h <= limits.maximumDimension &&
           static_cast<u64>(w) * static_cast<u64>(h) <= limits.maximumPixels;
  };
  if (detectImageContainer(bytes) == ImageContainer::Ktx2) {
    if (!readKtx2Dimensions(bytes, width, height) || !withinLimits(width, height)) {
      width = height = 0;
      return false;
    }
    return true;
  }
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
  if (detectImageContainer(bytes) != ImageContainer::Png &&
      detectImageContainer(bytes) != ImageContainer::Jpeg &&
      detectImageContainer(bytes) != ImageContainer::Ktx2) {
    diagnostic = "Formato de imagem não suportado neste perfil (PNG, JPEG e KTX2/BasisU).";
    return false;
  }
  if (detectImageContainer(bytes) == ImageContainer::Ktx2) {
    // Cabeçalho primeiro, como nos outros formatos: nada é transcodificado
    // antes de o tamanho ser aceito.
    u32 width = 0, height = 0;
    if (!readImageDimensions(bytes, limits, width, height)) {
      diagnostic = "KTX2 com cabeçalho inválido ou maior que o limite desta importação.";
      return false;
    }
    u32 decodedWidth = 0, decodedHeight = 0;
    if (!transcodeKtx2Rgba8(bytes, decodedWidth, decodedHeight, out.rgba, diagnostic)) return false;
    if (decodedWidth != width || decodedHeight != height) {
      out = {};
      diagnostic = "KTX2 com dimensões divergentes entre cabeçalho e dados.";
      return false;
    }
    out.width = width;
    out.height = height;
    return true;
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

bool decodeRadianceHdrRgba32f(std::span<const u8> bytes,const HdrImageDecodeLimits &limits,
                              DecodedHdrImage &out,std::string &diagnostic) {
  out={};diagnostic.clear();
  if(bytes.empty()||bytes.size()>limits.maximumEncodedBytes||bytes.size()>0x7fffffffu) {
    diagnostic="Radiance HDR vazio ou maior que o limite de bytes da importação.";return false;
  }
  if(detectImageContainer(bytes)!=ImageContainer::RadianceHdr) {
    diagnostic="Formato HDR não suportado. Use Radiance RGBE (.hdr); OpenEXR não está disponível.";return false;
  }
  int width=0,height=0,channels=0;
  if(!stbi_info_from_memory(bytes.data(),static_cast<int>(bytes.size()),&width,&height,&channels)||
     width<=0||height<=0) {
    diagnostic=std::string("Cabeçalho Radiance HDR inválido: ")+
        (stbi_failure_reason()?stbi_failure_reason():"desconhecido");return false;
  }
  if(static_cast<u32>(width)>limits.maximumDimension||static_cast<u32>(height)>limits.maximumDimension) {
    diagnostic="Radiance HDR excede os limites de dimensão ou memória decodificada.";return false;
  }
  const u64 pixels=static_cast<u64>(width)*static_cast<u64>(height);
  if(pixels>limits.maximumPixels||pixels>limits.maximumDecodedBytes/(2u*4u*sizeof(float))) {
    diagnostic="Radiance HDR excede os limites de dimensão ou memória decodificada.";return false;
  }
  int decodedWidth=0,decodedHeight=0,decodedChannels=0;
  float *pixels32=stbi_loadf_from_memory(bytes.data(),static_cast<int>(bytes.size()),&decodedWidth,
                                         &decodedHeight,&decodedChannels,4);
  if(!pixels32) {
    diagnostic=std::string("Falha ao decodificar Radiance HDR: ")+
        (stbi_failure_reason()?stbi_failure_reason():"desconhecido");return false;
  }
  if(decodedWidth!=width||decodedHeight!=height) {
    stbi_image_free(pixels32);diagnostic="Radiance HDR divergiu do próprio cabeçalho.";return false;
  }
  const usize count=static_cast<usize>(pixels)*4u;
  bool valid=true;
  for(usize index=0;index<count;++index)
    if(!std::isfinite(pixels32[index])||pixels32[index]<0.0f) {valid=false;break;}
  if(!valid) {
    stbi_image_free(pixels32);diagnostic="Radiance HDR contém radiância negativa ou não finita.";return false;
  }
  out.width=static_cast<u32>(width);out.height=static_cast<u32>(height);
  out.rgba.assign(pixels32,pixels32+count);stbi_image_free(pixels32);return true;
}

u32 mipLevelCount(u32 width, u32 height) {
  u32 levels = 1;
  for (u32 size = std::max(width, height); size > 1; size >>= 1) ++levels;
  return levels;
}

namespace {
// Gera os níveis por média de caixa, nível a nível, e guarda só a partir de
// `firstLevel`: os níveis de cima (os que o orçamento descarta) passam por um
// buffer rolante e nunca entram na cadeia. `reduce` escreve um texel de destino
// a partir do retângulo [x0,x1)x[y0,y1) do nível anterior.
template <class Reduce>
bool buildChain(const DecodedImage &base, u32 firstLevel, std::vector<u8> &chain, u32 &levels, Reduce reduce) {
  chain.clear();
  levels = 0;
  if (!base.width || !base.height || base.rgba.size() != static_cast<usize>(base.width) * base.height * 4u) return false;
  const u32 total = mipLevelCount(base.width, base.height);
  if (firstLevel >= total) return false;
  usize kept = 0;
  for (u32 level = 0, w = base.width, h = base.height; level < total; ++level, w = std::max(1u, w / 2), h = std::max(1u, h / 2))
    if (level >= firstLevel) kept += static_cast<usize>(w) * h * 4u;
  chain.reserve(kept);
  if (firstLevel == 0) chain.insert(chain.end(), base.rgba.begin(), base.rgba.end());
  std::vector<u8> rolling[2];
  const u8 *previous = base.rgba.data();
  u32 previousWidth = base.width, previousHeight = base.height;
  for (u32 level = 1; level < total; ++level) {
    const u32 width = std::max(1u, previousWidth / 2), height = std::max(1u, previousHeight / 2);
    auto &target = rolling[level & 1u];
    target.resize(static_cast<usize>(width) * height * 4u);
    for (u32 y = 0; y < height; ++y)
      for (u32 x = 0; x < width; ++x) {
        // Particiona toda a origem entre os texels de destino. Em 3 -> 1, por
        // exemplo, os três texels participam; o antigo 2x2 descartava a última
        // coluna/linha de dimensões NPOT.
        const u32 x0 = x * previousWidth / width, x1 = (x + 1) * previousWidth / width;
        const u32 y0 = y * previousHeight / height, y1 = (y + 1) * previousHeight / height;
        reduce(previous, previousWidth, x0, x1, y0, y1, target.data() + (static_cast<usize>(y) * width + x) * 4u);
      }
    if (level >= firstLevel) chain.insert(chain.end(), target.begin(), target.end());
    previous = target.data();
    previousWidth = width;
    previousHeight = height;
  }
  levels = total - firstLevel;
  return true;
}
} // namespace

bool buildMipChain(const DecodedImage &base, bool srgb, std::vector<u8> &chain, u32 &levels, u32 firstLevel) {
  const auto &toLinear = srgbToLinearTable();
  return buildChain(base, firstLevel, chain, levels,
                    [&](const u8 *source, u32 stride, u32 x0, u32 x1, u32 y0, u32 y1, u8 *target) {
    const auto at = [&](u32 sx, u32 sy) { return source + (static_cast<usize>(sy) * stride + sx) * 4u; };
    const u32 samples = (x1 - x0) * (y1 - y0);
    for (u32 c = 0; c < 4; ++c) {
      if (srgb && c < 3) {
        float sum = 0;
        for (u32 sy = y0; sy < y1; ++sy)
          for (u32 sx = x0; sx < x1; ++sx) sum += toLinear[at(sx, sy)[c]];
        target[c] = linearToSrgb(sum / static_cast<float>(samples));
      } else {
        u32 sum = 0;
        for (u32 sy = y0; sy < y1; ++sy)
          for (u32 sx = x0; sx < x1; ++sx) sum += at(sx, sy)[c];
        target[c] = static_cast<u8>((sum + samples / 2) / samples);
      }
    }
  });
}

bool buildNormalMipChain(const DecodedImage &base, std::vector<u8> &chain, u32 &levels, u32 firstLevel) {
  return buildChain(base, firstLevel, chain, levels,
                    [](const u8 *source, u32 stride, u32 x0, u32 x1, u32 y0, u32 y1, u8 *target) {
    const u32 count = (x1 - x0) * (y1 - y0);
    float nx = 0, ny = 0, nz = 0;
    u32 alpha = 0;
    for (u32 sy = y0; sy < y1; ++sy)
      for (u32 sx = x0; sx < x1; ++sx) {
        const u8 *sample = source + (static_cast<usize>(sy) * stride + sx) * 4u;
        nx += static_cast<float>(sample[0]) / 127.5f - 1.0f;
        ny += static_cast<float>(sample[1]) / 127.5f - 1.0f;
        nz += static_cast<float>(sample[2]) / 127.5f - 1.0f;
        alpha += sample[3];
      }
    const float length = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (length > 1.0e-8f) { nx /= length; ny /= length; nz /= length; }
    else { nx = 0; ny = 0; nz = 1; }
    const auto encode = [](float value) {
      return static_cast<u8>(std::lround(std::clamp(value * .5f + .5f, 0.0f, 1.0f) * 255.0f));
    };
    target[0] = encode(nx); target[1] = encode(ny); target[2] = encode(nz);
    target[3] = static_cast<u8>((alpha + count / 2u) / count);
  });
}
} // namespace ae::resources
