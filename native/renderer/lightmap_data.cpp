#include "renderer/lightmap_data.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ae::renderer {

bool LightmapBakeSettings::valid() const noexcept {
  const bool sizeStep = std::find(LightmapMaxSizeSteps.begin(), LightmapMaxSizeSteps.end(), maxSize) != LightmapMaxSizeSteps.end();
  return std::isfinite(texelsPerUnit) && texelsPerUnit >= 0.1f && texelsPerUnit <= 400.0f && padding <= 16 && sizeStep &&
         directSamples >= 1 && directSamples <= 4096 && indirectSamples >= 8 && indirectSamples <= 8192 &&
         environmentSamples >= 8 && environmentSamples <= 8192 && bounces <= 4 && std::isfinite(indirectIntensity) &&
         indirectIntensity >= 0 && indirectIntensity <= 5 && std::isfinite(albedoBoost) && albedoBoost >= 1 &&
         albedoBoost <= 10 && directionalMode == 0;
}

bool LightmapData::valid() const noexcept {
  if (!settings.valid() || pages.size() > MaximumLightmapPages) return false;
  for (const auto &page : pages) {
    if (!page.width || !page.height || page.width > 4096 || page.height > 4096) return false;
    if (page.format != LightmapPage::FormatHalf && page.format != LightmapPage::FormatSqrtRgba8) return false;
    if (!std::isfinite(page.range) || page.range <= 0) return false;
    if (page.texels.size() != page.expectedBytes()) return false;
  }
  // Uma página por array na GPU: todas do mesmo tamanho.
  for (const auto &page : pages)
    if (page.width != pages.front().width || page.height != pages.front().height || page.format != pages.front().format) return false;
  for (const auto &entry : entries) {
    if (entry.page >= pages.size()) return false;
    for (float v : entry.scaleOffset) if (!std::isfinite(v) || v < 0 || v > 1) return false;
    if (entry.scaleOffset[0] <= 0 || entry.scaleOffset[1] <= 0) return false;
  }
  return std::isfinite(bakeSeconds) && bakeSeconds >= 0;
}

u64 LightmapData::gpuBytes() const noexcept {
  u64 total = 0;
  for (const auto &page : pages) total += page.expectedBytes();
  return total;
}

std::vector<u8> LightmapData::serialize() const {
  std::vector<u8> out;
  u64 reserve = 256 + entries.size() * 32;
  for (const auto &page : pages) reserve += 16 + page.texels.size();
  out.reserve(static_cast<usize>(reserve));
  const auto put32 = [&](u32 v) { for (u32 i = 0; i < 4; ++i) out.push_back(static_cast<u8>(v >> (i * 8))); };
  const auto put64 = [&](u64 v) { put32(static_cast<u32>(v)); put32(static_cast<u32>(v >> 32)); };
  const auto putFloat = [&](float v) { u32 bits; std::memcpy(&bits, &v, 4); put32(bits); };
  out.insert(out.end(), {'A', 'E', 'L', 'M'});
  put32(Version);
  put64(guidHigh); put64(guidLow); put64(sourceHash);
  putFloat(bakeSeconds);
  // Configurações com que foi assado (o Inspector compara com as atuais).
  putFloat(settings.texelsPerUnit); put32(settings.padding); put32(settings.maxSize);
  put32(settings.directSamples); put32(settings.indirectSamples); put32(settings.environmentSamples);
  put32(settings.bounces); put32(settings.denoise ? 1 : 0); put32(settings.compress ? 1 : 0);
  putFloat(settings.indirectIntensity); putFloat(settings.albedoBoost); put32(settings.directionalMode);
  put32(stats.receivers); put32(stats.contributors); put32(stats.triangles); put32(stats.lights);
  put64(stats.texels); put64(stats.rays); put32(stats.rejectedReceivers); putFloat(stats.occupancy);
  put32(static_cast<u32>(pages.size()));
  for (const auto &page : pages) {
    put32(page.width); put32(page.height); put32(page.format); putFloat(page.range);
    out.insert(out.end(), page.texels.begin(), page.texels.end());
  }
  put32(static_cast<u32>(entries.size()));
  for (const auto &entry : entries) {
    put64(entry.objectId); put32(entry.slot); put32(entry.page);
    for (float v : entry.scaleOffset) putFloat(v);
  }
  return out;
}

bool LightmapData::deserialize(std::span<const u8> bytes, LightmapData &out, std::string *error) {
  const auto fail = [&](const char *message) { if (error) *error = message; return false; };
  if (bytes.size() < 8 || bytes.size() > MaximumBytes || std::memcmp(bytes.data(), "AELM", 4) != 0)
    return fail("arquivo de lightmap inválido");
  usize at = 4;
  bool truncated = false;
  const auto need = [&](usize n) { if (at + n > bytes.size()) { truncated = true; return false; } return true; };
  const auto get32 = [&]() -> u32 {
    if (!need(4)) return 0;
    u32 v = 0; for (u32 i = 0; i < 4; ++i) v |= static_cast<u32>(bytes[at + i]) << (i * 8);
    at += 4; return v;
  };
  const auto get64 = [&]() { const u64 lo = get32(); return lo | (static_cast<u64>(get32()) << 32); };
  const auto getFloat = [&]() { const u32 bits = get32(); float v; std::memcpy(&v, &bits, 4); return v; };
  if (get32() != Version) return fail("versão de lightmap desconhecida");
  LightmapData value;
  value.guidHigh = get64(); value.guidLow = get64(); value.sourceHash = get64();
  value.bakeSeconds = getFloat();
  auto &s = value.settings;
  s.texelsPerUnit = getFloat(); s.padding = get32(); s.maxSize = get32();
  s.directSamples = get32(); s.indirectSamples = get32(); s.environmentSamples = get32();
  s.bounces = get32(); s.denoise = get32() != 0; s.compress = get32() != 0;
  s.indirectIntensity = getFloat(); s.albedoBoost = getFloat(); s.directionalMode = get32();
  auto &st = value.stats;
  st.receivers = get32(); st.contributors = get32(); st.triangles = get32(); st.lights = get32();
  st.texels = get64(); st.rays = get64(); st.rejectedReceivers = get32(); st.occupancy = getFloat();
  const u32 pageCount = get32();
  if (truncated || pageCount > MaximumLightmapPages) return fail("lightmap truncado");
  value.pages.resize(pageCount);
  for (auto &page : value.pages) {
    page.width = get32(); page.height = get32(); page.format = get32(); page.range = getFloat();
    if (truncated || page.width > 4096 || page.height > 4096 || page.format > 1) return fail("página de lightmap inválida");
    const u64 size = page.expectedBytes();
    if (!need(static_cast<usize>(size))) return fail("página de lightmap truncada");
    page.texels.assign(bytes.begin() + at, bytes.begin() + at + size);
    at += size;
  }
  const u32 entryCount = get32();
  if (truncated || entryCount > (1u << 22) || bytes.size() - at != usize(entryCount) * 32) return fail("entradas de lightmap truncadas");
  value.entries.resize(entryCount);
  for (auto &entry : value.entries) {
    entry.objectId = get64(); entry.slot = get32(); entry.page = get32();
    for (float &v : entry.scaleOffset) v = getFloat();
  }
  if (!value.valid()) return fail("valores inválidos no lightmap");
  out = std::move(value);
  return true;
}

namespace {
u64 entryKey(u64 objectId, u32 slot) noexcept { return objectId * 1000003ull ^ (u64(slot) << 48) ^ slot; }
} // namespace

LightmapLookup::LightmapLookup(std::shared_ptr<const LightmapData> data) : data_(std::move(data)) {
  if (!data_) return;
  index_.reserve(data_->entries.size());
  for (u32 i = 0; i < data_->entries.size(); ++i) index_[entryKey(data_->entries[i].objectId, data_->entries[i].slot)] = i;
}

const LightmapRendererEntry *LightmapLookup::find(u64 objectId, u32 slot) const noexcept {
  if (!data_) return nullptr;
  const auto found = index_.find(entryKey(objectId, slot));
  if (found == index_.end()) return nullptr;
  const auto &entry = data_->entries[found->second];
  return entry.objectId == objectId && entry.slot == slot ? &entry : nullptr;
}

void decodeLightmapTexel(const LightmapPage &page, u32 x, u32 y, float rgb[3], float &visibility) noexcept {
  rgb[0] = rgb[1] = rgb[2] = 0;
  visibility = 0;
  if (x >= page.width || y >= page.height || page.texels.size() != page.expectedBytes()) return;
  const usize texel = usize(y) * page.width + x;
  if (page.format == LightmapPage::FormatHalf) {
    u16 h[4];
    std::memcpy(h, page.texels.data() + texel * 8, 8);
    for (u32 k = 0; k < 3; ++k) rgb[k] = halfToFloat(h[k]);
    visibility = halfToFloat(h[3]);
  } else {
    const u8 *t = page.texels.data() + texel * 4;
    for (u32 k = 0; k < 3; ++k) { const float s = t[k] / 255.0f; rgb[k] = s * s * page.range; }
    const float a = t[3] / 255.0f;
    visibility = a * a;
  }
}

} // namespace ae::renderer
