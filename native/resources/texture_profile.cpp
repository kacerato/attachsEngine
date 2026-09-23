#include "resources/texture_profile.h"
#include "resources/json_reader.h"

#include <algorithm>
#include <cmath>

namespace ae::resources {
bool sameTextureProfile(const TextureProfile &a, const TextureProfile &b) noexcept {
  return a.interpretation == b.interpretation && a.maximumDimension == b.maximumDimension && a.mipmaps == b.mipmaps &&
         a.dilateEdges == b.dilateEdges && a.anisotropy == b.anisotropy &&
         a.invertNormalGreen == b.invertNormalGreen && a.preserveAlphaCoverage == b.preserveAlphaCoverage &&
         std::fabs(a.alphaCoverageCutoff-b.alphaCoverageCutoff)<.0005f;
}

bool validTextureProfile(const TextureProfile &profile) noexcept {
  return profile.interpretation <= TextureInterpretationNormal &&
         std::isfinite(profile.alphaCoverageCutoff) && profile.alphaCoverageCutoff >= 0 && profile.alphaCoverageCutoff <= 1 &&
         std::find(TextureDimensionSteps.begin(), TextureDimensionSteps.end(), profile.maximumDimension) != TextureDimensionSteps.end();
}

std::string serializeTextureProfile(const TextureProfile &profile) {
  return "{\"schema\":" + std::to_string(TextureProfileSchema) + ",\"interpretation\":" + std::to_string(profile.interpretation) +
         ",\"maximumDimension\":" + std::to_string(profile.maximumDimension) + ",\"mipmaps\":" + (profile.mipmaps ? "1" : "0") +
         ",\"dilateEdges\":" + (profile.dilateEdges ? "1" : "0") + ",\"anisotropy\":" + (profile.anisotropy ? "1" : "0") +
         ",\"invertNormalGreen\":" + (profile.invertNormalGreen ? "1" : "0") +
         ",\"preserveAlphaCoverage\":" + (profile.preserveAlphaCoverage ? "1" : "0") +
         ",\"alphaCoverageCutoff\":" + std::to_string(static_cast<u32>(std::lround(profile.alphaCoverageCutoff*1000.0f))) + "}\n";
}

bool parseTextureProfile(std::string_view text, TextureProfile &out) {
  out = {};
  JsonDocument document;
  if (!JsonDocument::parse(text, document) || !document.root() || document.root()->kind != JsonDocument::Kind::Object)
    return false;
  const auto &root = *document.root();
  const auto schema = document.index(root, "schema");
  if (schema < 1 || schema > static_cast<i64>(TextureProfileSchema)) return false;
  const auto interpretation = document.index(root, "interpretation");
  const auto dimension = document.index(root, "maximumDimension");
  const auto mipmaps = document.index(root, "mipmaps");
  const auto edges = document.index(root, "dilateEdges");
  const auto anisotropy = document.index(root, "anisotropy");
  const auto invertNormalGreen = schema >= 2 ? document.index(root, "invertNormalGreen") : 0;
  const auto preserveAlphaCoverage = schema >= 3 ? document.index(root, "preserveAlphaCoverage") : 0;
  const auto alphaCoverageCutoff = schema >= 3 ? document.index(root, "alphaCoverageCutoff") : 500;
  const auto flag = [](i64 value) { return value == 0 || value == 1; };
  // O valor 3 só passou a significar mapa normal no schema 2. Aceitá-lo em um
  // perfil v1 mudaria o sentido de dados antigos/corrompidos durante a migração.
  if (interpretation < 0 || interpretation > 255 ||
      (schema == 1 && interpretation > TextureInterpretationData) || dimension < 0 ||
      !flag(mipmaps) || !flag(edges) ||
      !flag(anisotropy) || !flag(invertNormalGreen) || !flag(preserveAlphaCoverage) ||
      alphaCoverageCutoff < 0 || alphaCoverageCutoff > 1000) return false;
  TextureProfile parsed;
  parsed.interpretation = static_cast<u8>(interpretation);
  parsed.maximumDimension = static_cast<u32>(std::min<i64>(dimension, 1 << 20));
  parsed.mipmaps = mipmaps == 1;
  parsed.dilateEdges = edges == 1;
  parsed.anisotropy = anisotropy == 1;
  parsed.invertNormalGreen = invertNormalGreen == 1;
  parsed.preserveAlphaCoverage = preserveAlphaCoverage == 1;
  parsed.alphaCoverageCutoff = static_cast<float>(alphaCoverageCutoff)/1000.0f;
  if (!validTextureProfile(parsed)) return false;
  out = parsed;
  return true;
}

std::string textureProfilePath(const AssetGuid &texture) { return ".astra/textures/" + texture.text() + ".profile"; }

u32 dilateTransparentEdges(DecodedImage &image, u32 passes, u8 threshold,
                           const std::atomic<bool> *cancel) {
  const usize count = static_cast<usize>(image.width) * image.height;
  if (!image.width || !image.height || image.rgba.size() != count * 4) return 0;
  std::vector<u8> filled(count);
  for (usize i = 0; i < count; ++i) filled[i] = image.rgba[i * 4 + 3] >= threshold;
  u32 changed = 0;
  std::vector<u8> next;
  for (u32 pass = 0; pass < passes; ++pass) {
    // Cada passo lê só o que já tinha cor no passo anterior: a borda cresce um
    // texel por vez e não arrasta a primeira cor encontrada pela linha inteira.
    next = filled;
    bool any = false;
    for (u32 y = 0; y < image.height; ++y) {
      if (cancel && cancel->load(std::memory_order_relaxed)) return changed;
      for (u32 x = 0; x < image.width; ++x) {
        const usize at = static_cast<usize>(y) * image.width + x;
        if (filled[at]) continue;
        u32 sum[3]{}, samples = 0;
        const auto take = [&](u32 nx, u32 ny) {
          const usize neighbour = static_cast<usize>(ny) * image.width + nx;
          if (!filled[neighbour]) return;
          for (u32 c = 0; c < 3; ++c) sum[c] += image.rgba[neighbour * 4 + c];
          ++samples;
        };
        if (x > 0) take(x - 1, y);
        if (x + 1 < image.width) take(x + 1, y);
        if (y > 0) take(x, y - 1);
        if (y + 1 < image.height) take(x, y + 1);
        if (!samples) continue;
        for (u32 c = 0; c < 3; ++c) image.rgba[at * 4 + c] = static_cast<u8>(sum[c] / samples);
        next[at] = 1;
        ++changed;
        any = true;
      }
    }
    filled.swap(next);
    if (!any) break;
  }
  return changed;
}
} // namespace ae::resources
