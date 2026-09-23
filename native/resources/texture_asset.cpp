#include "resources/texture_asset.h"

#include "core/sha256.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace ae::resources {
namespace {
constexpr u32 CacheMagic = 0x43544541; // AETC
enum class MipSemantic : u8 { Linear, Srgb, Normal };

bool cancelled(const std::atomic<bool> *value) {
  return value && value->load(std::memory_order_relaxed);
}

bool addBytes(u64 a, u64 b, u64 &out) {
  if (a > std::numeric_limits<u64>::max() - b) return false;
  out = a + b;
  return true;
}

u64 chainBytes(u32 width, u32 height, bool mipmaps) {
  u64 total = 0;
  for (;;) {
    u64 level = static_cast<u64>(width) * height * 4u;
    if (!addBytes(total, level, total)) return 0;
    if (!mipmaps || (width == 1 && height == 1)) return total;
    width = std::max(1u, width / 2);
    height = std::max(1u, height / 2);
  }
}

float srgbToLinear(u8 value) {
  const float c = static_cast<float>(value) / 255.0f;
  return c <= .04045f ? c / 12.92f : std::pow((c + .055f) / 1.055f, 2.4f);
}

u8 linearToSrgb(float value) {
  const float c = std::clamp(value, 0.0f, 1.0f);
  const float encoded = c <= .0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - .055f;
  return static_cast<u8>(std::lround(std::clamp(encoded, 0.0f, 1.0f) * 255.0f));
}

bool downsample(const DecodedImage &source, MipSemantic semantic,
                const std::atomic<bool> *cancel, DecodedImage &out) {
  if (!source.width || !source.height ||
      source.rgba.size() != static_cast<usize>(source.width) * source.height * 4u)
    return false;
  out = {};
  out.width = std::max(1u, source.width / 2);
  out.height = std::max(1u, source.height / 2);
  out.rgba.resize(static_cast<usize>(out.width) * out.height * 4u);
  const auto at = [&](u32 x, u32 y) {
    return source.rgba.data() + (static_cast<usize>(y) * source.width + x) * 4u;
  };
  for (u32 y = 0; y < out.height; ++y) {
    if (cancelled(cancel)) { out = {}; return false; }
    const u32 y0 = y * source.height / out.height;
    const u32 y1 = (y + 1) * source.height / out.height;
    for (u32 x = 0; x < out.width; ++x) {
      const u32 x0 = x * source.width / out.width;
      const u32 x1 = (x + 1) * source.width / out.width;
      const u32 count = (x1 - x0) * (y1 - y0);
      u8 *target = out.rgba.data() + (static_cast<usize>(y) * out.width + x) * 4u;
      if (semantic == MipSemantic::Normal) {
        float nx = 0, ny = 0, nz = 0;
        u32 alpha = 0;
        for (u32 sy = y0; sy < y1; ++sy)
          for (u32 sx = x0; sx < x1; ++sx) {
            const u8 *sample = at(sx, sy);
            nx += static_cast<float>(sample[0]) / 127.5f - 1.0f;
            ny += static_cast<float>(sample[1]) / 127.5f - 1.0f;
            nz += static_cast<float>(sample[2]) / 127.5f - 1.0f;
            alpha += sample[3];
          }
        const float length = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (length > 1.0e-8f) { nx /= length; ny /= length; nz /= length; }
        else { nx = 0; ny = 0; nz = 1; }
        const auto encode = [](float component) {
          return static_cast<u8>(std::lround(std::clamp(component * .5f + .5f, 0.0f, 1.0f) * 255.0f));
        };
        target[0] = encode(nx); target[1] = encode(ny); target[2] = encode(nz);
        target[3] = static_cast<u8>((alpha + count / 2u) / count);
      } else {
        for (u32 channel = 0; channel < 4; ++channel) {
          if (semantic == MipSemantic::Srgb && channel < 3) {
            float sum = 0;
            for (u32 sy = y0; sy < y1; ++sy)
              for (u32 sx = x0; sx < x1; ++sx) sum += srgbToLinear(at(sx, sy)[channel]);
            target[channel] = linearToSrgb(sum / static_cast<float>(count));
          } else {
            u32 sum = 0;
            for (u32 sy = y0; sy < y1; ++sy)
              for (u32 sx = x0; sx < x1; ++sx) sum += at(sx, sy)[channel];
            target[channel] = static_cast<u8>((sum + count / 2u) / count);
          }
        }
      }
    }
  }
  return true;
}

float alphaCoverage(const DecodedImage &image,float cutoff) {
  if(image.rgba.empty()) return 0;
  const u8 threshold=static_cast<u8>(std::ceil(std::clamp(cutoff,0.0f,1.0f)*255.0f));
  usize covered=0;
  for(usize at=3;at<image.rgba.size();at+=4) if(image.rgba[at]>=threshold) ++covered;
  return static_cast<float>(covered)/static_cast<float>(image.rgba.size()/4u);
}

bool preserveCoverage(DecodedImage &image,float target,float cutoff,const std::atomic<bool> *cancel) {
  if(image.rgba.empty()) return true;
  const u8 threshold=static_cast<u8>(std::ceil(std::clamp(cutoff,0.0f,1.0f)*255.0f));
  if(!threshold) return true;
  std::array<u32,256> histogram{};
  for(usize at=3;at<image.rgba.size();at+=4) {
    if((at&0x3ffffu)==3u&&cancelled(cancel)) return false;
    ++histogram[image.rgba[at]];
  }
  const float total=static_cast<float>(image.rgba.size()/4u);
  const auto coverageAt=[&](float scale) {
    u32 covered=0;
    for(u32 alpha=0;alpha<256;++alpha)
      if(std::min(255.0f,std::round(static_cast<float>(alpha)*scale))>=threshold) covered+=histogram[alpha];
    return static_cast<float>(covered)/total;
  };
  // A máscara é discreta: escolhe, entre todos os pontos em que algum valor
  // cruza o cutoff, a cobertura mais próxima. Empate preserva escala 1 para não
  // expandir nem erodir a silhueta sem necessidade.
  float bestScale=1,bestError=std::fabs(coverageAt(1)-target);
  const float emptyError=std::fabs(coverageAt(0)-target);
  if(emptyError<bestError-1e-6f) {bestScale=0;bestError=emptyError;}
  for(u32 alpha=1;alpha<256;++alpha) if(histogram[alpha]) {
    const float candidate=(static_cast<float>(threshold)-.49f)/static_cast<float>(alpha);
    const float error=std::fabs(coverageAt(candidate)-target);
    if(error<bestError-1e-6f||(std::fabs(error-bestError)<=1e-6f&&std::fabs(candidate-1)<std::fabs(bestScale-1))) {
      bestError=error;bestScale=candidate;
    }
  }
  for(usize at=3;at<image.rgba.size();at+=4) {
    if((at&0x3ffffu)==3u&&cancelled(cancel)) return false;
    image.rgba[at]=static_cast<u8>(std::min(255.0f,std::round(image.rgba[at]*bestScale)));
  }
  return !cancelled(cancel);
}

bool hexHash(std::string_view value) {
  if (value.size() != 64) return false;
  for (char c : value)
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  return true;
}

struct Writer {
  std::vector<u8> &out;
  void raw(const void *data, usize size) {
    const auto *bytes = static_cast<const u8 *>(data);
    out.insert(out.end(), bytes, bytes + size);
  }
  void u32v(u32 value) { for (u32 i = 0; i < 4; ++i) out.push_back(static_cast<u8>(value >> (i * 8u))); }
  void u64v(u64 value) { for (u32 i = 0; i < 8; ++i) out.push_back(static_cast<u8>(value >> (i * 8u))); }
  void text(std::string_view value) { u32v(static_cast<u32>(value.size())); raw(value.data(), value.size()); }
};

struct Reader {
  std::span<const u8> in;
  usize at = 0;
  bool ok = true;
  u32 u32v() {
    if (!ok || in.size() - at < 4) { ok = false; return 0; }
    u32 value = 0; for (u32 i = 0; i < 4; ++i) value |= u32{in[at++]} << (i * 8u); return value;
  }
  u64 u64v() {
    if (!ok || in.size() - at < 8) { ok = false; return 0; }
    u64 value = 0; for (u32 i = 0; i < 8; ++i) value |= u64{in[at++]} << (i * 8u); return value;
  }
  std::string text() {
    const u32 size = u32v();
    if (!ok || size > in.size() - at) { ok = false; return {}; }
    std::string result(reinterpret_cast<const char *>(in.data() + at), size); at += size; return result;
  }
};
} // namespace

bool TextureImportLimits::valid() const noexcept {
  return image.maximumDimension && image.maximumPixels && image.maximumEncodedBytes &&
         projectMaximumDimension && maximumWorkingBytes && maximumDerivedBytes;
}

bool PreparedTextureImport::valid() const noexcept {
  return texture && texture->valid() && sourceWidth && sourceHeight &&
         hexHash(sourceHash) && hexHash(cacheKey);
}

std::string textureAssetCacheKey(std::string_view sourceHash,
                                 const TextureProfile &settings,
                                 bool usageSrgb, u32 samplerFlags,
                                 const TextureImportLimits &limits) {
  const std::string input = "texture|schema=" + std::to_string(TextureAssetCacheSchema) +
      "|importer=" + std::to_string(TextureAssetImporterRevision) + "|source=" + std::string(sourceHash) +
      "|settings=" + serializeTextureProfile(settings) + "|usageSrgb=" + std::to_string(usageSrgb ? 1 : 0) +
      "|sampler=" + std::to_string(samplerFlags) + "|dimension=" + std::to_string(limits.projectMaximumDimension) +
      "|imageDimension=" + std::to_string(limits.image.maximumDimension) +
      "|pixels=" + std::to_string(limits.image.maximumPixels) +
      "|encoded=" + std::to_string(limits.image.maximumEncodedBytes) +
      "|working=" + std::to_string(limits.maximumWorkingBytes) +
      "|derived=" + std::to_string(limits.maximumDerivedBytes);
  return Sha256::hex(std::span<const u8>(reinterpret_cast<const u8 *>(input.data()), input.size()));
}

std::string textureAssetCachePath(const AssetGuid &guid) {
  return ".astra/cache/textures/" + guid.text() + ".aetc";
}

bool prepareTextureImport(std::span<const u8> source, const TextureProfile &settings,
                          bool usageSrgb, u32 samplerFlags, const TextureImportLimits &limits,
                          const std::atomic<bool> *cancel, PreparedTextureImport &out) {
  out = {};
  if (!validTextureProfile(settings) || !limits.valid()) {
    out.diagnostic = "Configuração de importação de textura inválida."; return false;
  }
  if (cancelled(cancel)) { out.diagnostic = "Importação de textura cancelada."; return false; }
  u32 width = 0, height = 0;
  if (!readImageDimensions(source, limits.image, width, height)) {
    out.diagnostic = "Imagem inválida, não suportada ou acima dos limites de importação."; return false;
  }
  const u64 sourceBytes = static_cast<u64>(width) * height * 4u;
  if (sourceBytes > limits.maximumWorkingBytes / 2u) {
    out.diagnostic = "Imagem excede o orçamento de memória de trabalho antes da decodificação."; return false;
  }
  const bool normal = settings.interpretation == TextureInterpretationNormal;
  const bool srgb = settings.interpretation == TextureInterpretationColor ? true :
                    (settings.interpretation == TextureInterpretationData || normal ? false : usageSrgb);
  const MipSemantic semantic = normal ? MipSemantic::Normal : (srgb ? MipSemantic::Srgb : MipSemantic::Linear);
  const bool preserveAlpha=settings.preserveAlphaCoverage&&semantic==MipSemantic::Srgb;
  if(preserveAlpha&&detectImageContainer(source)==ImageContainer::Ktx2) {
    out.diagnostic="Preservação de cobertura alfa exige PNG ou JPEG decodificável; o pedido para KTX2 compactado foi recusado.";
    return false;
  }
  const u32 cap = std::max(1u, settings.maximumDimension
      ? std::min(settings.maximumDimension, limits.projectMaximumDimension)
      : limits.projectMaximumDimension);
  u32 cookedWidth = width, cookedHeight = height, dropped = 0;
  while (std::max(cookedWidth, cookedHeight) > cap) {
    cookedWidth = std::max(1u, cookedWidth / 2);
    cookedHeight = std::max(1u, cookedHeight / 2);
    ++dropped;
  }
  const u64 outputBytes = chainBytes(cookedWidth, cookedHeight, settings.mipmaps);
  if (!outputBytes || outputBytes > limits.maximumDerivedBytes) {
    out.diagnostic = "Textura cozida excede o orçamento de derivados."; return false;
  }
  const u64 pixels = static_cast<u64>(width) * height;
  const auto nextLevelBytes = [](u32 levelWidth, u32 levelHeight) -> u64 {
    if (levelWidth == 1u && levelHeight == 1u) return 0;
    return static_cast<u64>(std::max(1u, levelWidth / 2u)) *
           std::max(1u, levelHeight / 2u) * 4u;
  };
  u64 reductionPeak = 0;
  if (!addBytes(sourceBytes, nextLevelBytes(width, height), reductionPeak))
    reductionPeak = std::numeric_limits<u64>::max();
  const u64 cookedBaseBytes = static_cast<u64>(cookedWidth) * cookedHeight * 4u;
  u64 cookPeak = 0;
  if (!addBytes(cookedBaseBytes, nextLevelBytes(cookedWidth, cookedHeight), cookPeak) ||
      !addBytes(cookPeak, outputBytes, cookPeak))
    cookPeak = std::numeric_limits<u64>::max();
  const u64 decodePeak = sourceBytes <= std::numeric_limits<u64>::max() / 2u
      ? sourceBytes * 2u : std::numeric_limits<u64>::max();
  u64 dilationPeak = sourceBytes;
  if (settings.dilateEdges) {
    u64 maskBytes = 0;
    if (!addBytes(pixels, pixels, maskBytes) || !addBytes(dilationPeak, maskBytes, dilationPeak))
      dilationPeak = std::numeric_limits<u64>::max();
  }
  // `reserve(outputBytes)` deliberately avoids a second output allocation, but
  // the current mip and the next downsampled mip coexist while that reserved
  // chain is alive. Reduction before the cook has a separate source+next peak.
  const u64 working = std::max({reductionPeak, cookPeak, decodePeak, dilationPeak});
  if (outputBytes > std::numeric_limits<usize>::max() || working > limits.maximumWorkingBytes) {
    out.diagnostic = "Fonte e mipmaps excedem o orçamento de memória de trabalho."; return false;
  }
  DecodedImage image;
  if (!decodeImageRgba8(source, limits.image, image, out.diagnostic)) return false;
  out.sourceWidth = image.width; out.sourceHeight = image.height;
  for (usize pixel = 0; pixel < image.rgba.size() / 4u; ++pixel)
    out.sourceHasAlpha |= image.rgba[pixel * 4u + 3u] != 255u;
  if (settings.dilateEdges) {
    dilateTransparentEdges(image, 8, 1, cancel);
    if (cancelled(cancel)) { out = {}; out.diagnostic = "Importação de textura cancelada."; return false; }
  }
  if (normal && settings.invertNormalGreen)
    for (usize pixel = 0; pixel < image.rgba.size() / 4u; ++pixel)
      image.rgba[pixel * 4u + 1u] = static_cast<u8>(255u - image.rgba[pixel * 4u + 1u]);
  const float sourceCoverage=preserveAlpha?alphaCoverage(image,settings.alphaCoverageCutoff):0;
  for (u32 level = 0; level < dropped; ++level) {
    DecodedImage next;
    if (!downsample(image, semantic, cancel, next)) {
      out = {}; out.diagnostic = cancelled(cancel) ? "Importação de textura cancelada." : "Falha ao reduzir a textura."; return false;
    }
    if(preserveAlpha&&!preserveCoverage(next,sourceCoverage,settings.alphaCoverageCutoff,cancel)) {
      out={};out.diagnostic="Importação de textura cancelada.";return false;
    }
    image = std::move(next);
  }
  auto texture = std::make_shared<renderer::AuthoringTexture>();
  texture->width = image.width; texture->height = image.height; texture->srgb = srgb;
  texture->samplerFlags = (samplerFlags & ~renderer::AuthoringTextureNoAnisotropy) |
      (settings.anisotropy ? 0u : renderer::AuthoringTextureNoAnisotropy);
  texture->mipChain.reserve(static_cast<usize>(outputBytes));
  for (;;) {
    if (cancelled(cancel)) { out = {}; out.diagnostic = "Importação de textura cancelada."; return false; }
    texture->mipChain.insert(texture->mipChain.end(), image.rgba.begin(), image.rgba.end());
    ++texture->levels;
    if (!settings.mipmaps || (image.width == 1 && image.height == 1)) break;
    DecodedImage next;
    if (!downsample(image, semantic, cancel, next)) {
      out = {}; out.diagnostic = cancelled(cancel) ? "Importação de textura cancelada." : "Falha ao gerar mipmaps."; return false;
    }
    if(preserveAlpha&&!preserveCoverage(next,sourceCoverage,settings.alphaCoverageCutoff,cancel)) {
      out={};out.diagnostic="Importação de textura cancelada.";return false;
    }
    image = std::move(next);
  }
  if (!texture->valid() || texture->mipChain.size() != outputBytes) {
    out = {}; out.diagnostic = "Textura cozida ficou inconsistente."; return false;
  }
  out.texture = std::move(texture); out.droppedMipLevels = dropped;
  out.sourceHash = Sha256::hex(source);
  out.cacheKey = textureAssetCacheKey(out.sourceHash, settings, usageSrgb, samplerFlags, limits);
  return true;
}

bool writeTextureAssetCache(const PreparedTextureImport &prepared, std::vector<u8> &out) {
  out.clear(); if (!prepared.valid()) return false;
  Writer writer{out}; const auto &texture = *prepared.texture;
  writer.u32v(CacheMagic); writer.u32v(TextureAssetCacheSchema);
  writer.text(prepared.sourceHash); writer.text(prepared.cacheKey);
  writer.u32v(prepared.sourceWidth); writer.u32v(prepared.sourceHeight);
  writer.u32v(prepared.droppedMipLevels); writer.u32v(prepared.sourceHasAlpha ? 1u : 0u);
  writer.u32v(texture.width); writer.u32v(texture.height); writer.u32v(texture.levels);
  writer.u32v(texture.srgb ? 1u : 0u); writer.u32v(texture.samplerFlags); writer.u32v(texture.format);
  writer.u64v(texture.mipChain.size()); writer.raw(texture.mipChain.data(), texture.mipChain.size());
  const std::string checksum = Sha256::hex(out); writer.raw(checksum.data(), checksum.size());
  return true;
}

bool readTextureAssetCache(std::span<const u8> bytes, std::string_view expectedKey,
                           const TextureImportLimits &limits, PreparedTextureImport &out) {
  out = {};
  if (!limits.valid() || !hexHash(expectedKey) || bytes.size() < 64u ||
      bytes.size() > limits.maximumDerivedBytes + 4096u) return false;
  const auto body = bytes.first(bytes.size() - 64u);
  const std::string_view checksum(reinterpret_cast<const char *>(bytes.data() + body.size()), 64u);
  if (Sha256::hex(body) != checksum) return false;
  Reader reader{body};
  if (reader.u32v() != CacheMagic || reader.u32v() != TextureAssetCacheSchema) return false;
  PreparedTextureImport candidate;
  candidate.sourceHash = reader.text(); candidate.cacheKey = reader.text();
  if (!reader.ok || !hexHash(candidate.sourceHash) || candidate.cacheKey != expectedKey) return false;
  candidate.sourceWidth = reader.u32v(); candidate.sourceHeight = reader.u32v();
  candidate.droppedMipLevels = reader.u32v();
  const u32 alpha = reader.u32v();
  auto texture = std::make_shared<renderer::AuthoringTexture>();
  texture->width = reader.u32v(); texture->height = reader.u32v(); texture->levels = reader.u32v();
  const u32 srgb = reader.u32v(); texture->samplerFlags = reader.u32v(); texture->format = reader.u32v();
  const u64 payload = reader.u64v();
  if (!reader.ok || alpha > 1u || srgb > 1u || !candidate.sourceWidth || !candidate.sourceHeight ||
      candidate.sourceWidth > limits.image.maximumDimension || candidate.sourceHeight > limits.image.maximumDimension ||
      static_cast<u64>(candidate.sourceWidth) * candidate.sourceHeight > limits.image.maximumPixels ||
      !texture->width || !texture->height || !texture->levels ||
      texture->levels > mipLevelCount(texture->width, texture->height) ||
      candidate.droppedMipLevels >= mipLevelCount(candidate.sourceWidth, candidate.sourceHeight) ||
      payload > limits.maximumDerivedBytes ||
      payload > body.size() - reader.at || payload > std::numeric_limits<usize>::max()) return false;
  u32 expectedWidth = candidate.sourceWidth, expectedHeight = candidate.sourceHeight;
  for (u32 level = 0; level < candidate.droppedMipLevels; ++level) {
    expectedWidth = std::max(1u, expectedWidth / 2u);
    expectedHeight = std::max(1u, expectedHeight / 2u);
  }
  if (texture->width != expectedWidth || texture->height != expectedHeight) return false;
  texture->srgb = srgb == 1u;
  texture->mipChain.assign(body.begin() + static_cast<std::ptrdiff_t>(reader.at),
                           body.begin() + static_cast<std::ptrdiff_t>(reader.at + static_cast<usize>(payload)));
  reader.at += static_cast<usize>(payload);
  candidate.sourceHasAlpha = alpha == 1u; candidate.texture = std::move(texture);
  if (reader.at != body.size() || !candidate.valid()) return false;
  out = std::move(candidate); return true;
}

} // namespace ae::resources
