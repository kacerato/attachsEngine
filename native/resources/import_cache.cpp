#include "resources/import_cache.h"
#include "core/sha256.h"

#include <cstring>
#include <memory>

namespace ae::resources {
namespace {
constexpr char kMagic[8]{'A', 'S', 'T', 'R', 'A', 'I', 'C', '1'};
constexpr u32 kCounterCount = 17;

struct Writer {
  std::vector<u8> &out;
  void raw(const void *data, usize size) {
    const auto *bytes = static_cast<const u8 *>(data);
    out.insert(out.end(), bytes, bytes + size);
  }
  void u32v(u32 value) { raw(&value, sizeof value); }
  void u64v(u64 value) { raw(&value, sizeof value); }
  void i32v(i32 value) { raw(&value, sizeof value); }
  void text(std::string_view value) {
    u32v(static_cast<u32>(value.size()));
    raw(value.data(), value.size());
  }
  void texts(const std::vector<std::string> &values) {
    u32v(static_cast<u32>(values.size()));
    for (const auto &value : values) text(value);
  }
  template <typename T> void pods(const std::vector<T> &values) {
    u64v(values.size());
    if (!values.empty()) raw(values.data(), values.size() * sizeof(T));
  }
};

struct Reader {
  std::span<const u8> in;
  usize at = 0;
  bool ok = true;
  bool raw(void *data, usize size) {
    if (!ok || size > in.size() - at) return ok = false;
    std::memcpy(data, in.data() + at, size);
    at += size;
    return true;
  }
  u32 u32v() {
    u32 value = 0;
    raw(&value, sizeof value);
    return value;
  }
  u64 u64v() {
    u64 value = 0;
    raw(&value, sizeof value);
    return value;
  }
  i32 i32v() {
    i32 value = 0;
    raw(&value, sizeof value);
    return value;
  }
  std::string text() {
    const u32 size = u32v();
    if (!ok || size > in.size() - at) {
      ok = false;
      return {};
    }
    std::string value(reinterpret_cast<const char *>(in.data() + at), size);
    at += size;
    return value;
  }
  std::vector<std::string> texts() {
    const u32 count = u32v();
    std::vector<std::string> values;
    // Cada texto ocupa ao menos os 4 bytes do tamanho.
    if (!ok || count > (in.size() - at) / 4) {
      ok = false;
      return values;
    }
    values.reserve(count);
    for (u32 i = 0; i < count && ok; ++i) values.push_back(text());
    return values;
  }
  template <typename T> std::vector<T> pods() {
    const u64 count = u64v();
    std::vector<T> values;
    if (!ok || count > (in.size() - at) / sizeof(T)) {
      ok = false;
      return values;
    }
    values.resize(static_cast<usize>(count));
    if (count) raw(values.data(), static_cast<usize>(count) * sizeof(T));
    return values;
  }
};
} // namespace

std::string importCacheKey(std::string_view sourceContentHash, const GltfImportLimits &limits) {
  const auto number = [](u64 value) { return std::to_string(value); };
  std::string text = "astra-import-cache|schema=" + number(ImportCacheSchema) +
                     "|importer=" + number(ImportCacheImporterRevision) +
                     "|codecs=draco-1.5.7,meshoptimizer-v1.2,basis_universal-v2_50,stb_image-2.30" +
                     "|source=" + std::string(sourceContentHash) + "|bytes=" + number(limits.maximumBytes) +
                     "|nodes=" + number(limits.maximumNodes) + "|draws=" + number(limits.maximumDraws) +
                     "|vertices=" + number(limits.maximumVertices) + "|indices=" + number(limits.maximumIndices) +
                     "|textureBytes=" + number(limits.maximumTextureBytes) +
                     "|textureDimension=" + number(limits.maximumTextureDimension) +
                     "|minimumTextureDimension=" + number(limits.minimumTextureDimension) +
                     "|expanded=" + number(limits.maximumExpandedBytes) + "|astc4x4=" + number(limits.astc4x4 ? 1 : 0) +
                     "|rootScale=" + [&] {
                       u32 bits = 0;
                       std::memcpy(&bits, &limits.rootScale, sizeof bits);
                       return number(bits);
                     }() +
                     "|imageDimension=" + number(limits.image.maximumDimension) +
                     "|imagePixels=" + number(limits.image.maximumPixels) +
                     "|imageEncoded=" + number(limits.image.maximumEncodedBytes);
  return Sha256::hex(std::span<const u8>(reinterpret_cast<const u8 *>(text.data()), text.size()));
}

std::string importCacheRelativePath(std::string_view key) {
  return ".astra/cache/imports/" + std::string(key) + ".aic";
}

bool writeImportCache(const GltfImport &model, std::string_view key, std::vector<u8> &out) {
  out.clear();
  if (key.empty() || model.draws.empty() || model.nodes.empty()) return false;
  Writer writer{out};
  writer.raw(kMagic, sizeof kMagic);
  writer.u32v(ImportCacheSchema);
  writer.text(key);
  writer.texts(model.appearanceExtensions);
  writer.pods(model.draws);
  writer.pods(model.materials);
  writer.texts(model.materialNames);
  writer.pods(model.vertices);
  writer.pods(model.indices);
  writer.texts(model.names);
  writer.texts(model.keys);
  writer.u32v(static_cast<u32>(model.nodes.size()));
  for (const auto &node : model.nodes) {
    writer.text(node.name);
    writer.i32v(node.parent);
    writer.raw(node.localMatrix, sizeof node.localMatrix);
    writer.text(node.authoredId);
  }
  writer.pods(model.drawNodes);
  writer.u32v(static_cast<u32>(model.textures.size()));
  for (const auto &texture : model.textures) {
    if (!texture || !texture->valid()) {
      out.clear();
      return false;
    }
    writer.u32v(texture->width);
    writer.u32v(texture->height);
    writer.u32v(texture->levels);
    writer.u32v(texture->format);
    writer.u32v(texture->samplerFlags);
    writer.u32v(texture->srgb ? 1 : 0);
    writer.pods(texture->mipChain);
  }
  writer.u64v(model.textureBytes);
  const u32 counters[kCounterCount]{model.skippedTextures, model.skippedAnimations, model.skippedSkins,
                                    model.skippedPrimitives, model.skippedCameras, model.skippedLights,
                                    model.unappliedTextureTransforms, model.unappliedOcclusion,
                                    model.bakedTextureTransforms, model.reducedTextures, model.dracoPrimitives,
                                    model.meshoptViews, model.ktx2Images, model.astcTextures, model.mirroredNodes,
                                    model.generatedTangentPrimitives, model.residentTextureDimension};
  writer.u32v(kCounterCount);
  writer.raw(counters, sizeof counters);
  writer.texts(model.textureNotes);
  // Terminador: um arquivo cortado no meio da escrita nunca passa por inteiro.
  writer.raw(kMagic, sizeof kMagic);
  return true;
}

bool readImportCache(std::span<const u8> bytes, std::string_view key, GltfImport &out) {
  out = {};
  Reader reader{bytes};
  const auto refuse = [&out]() {
    out = {};
    return false;
  };
  char magic[8]{};
  if (!reader.raw(magic, sizeof magic) || std::memcmp(magic, kMagic, sizeof magic) != 0) return refuse();
  if (reader.u32v() != ImportCacheSchema || reader.text() != key || !reader.ok) return refuse();
  GltfImport model;
  model.appearanceExtensions = reader.texts();
  model.draws = reader.pods<renderer::MapDrawRecord>();
  model.materials = reader.pods<renderer::MapMaterialRecord>();
  model.materialNames = reader.texts();
  model.vertices = reader.pods<u8>();
  model.indices = reader.pods<u32>();
  model.names = reader.texts();
  model.keys = reader.texts();
  const u32 nodeCount = reader.u32v();
  if (!reader.ok || nodeCount > bytes.size()) return refuse();
  model.nodes.resize(nodeCount);
  for (u32 i = 0; i < nodeCount && reader.ok; ++i) {
    auto &node = model.nodes[i];
    node.name = reader.text();
    node.parent = reader.i32v();
    reader.raw(node.localMatrix, sizeof node.localMatrix);
    node.authoredId = reader.text();
    if (node.parent < -1 || node.parent >= static_cast<i32>(i)) return refuse();
  }
  model.drawNodes = reader.pods<u32>();
  const u32 textureCount = reader.u32v();
  if (!reader.ok || textureCount > bytes.size()) return refuse();
  model.textures.reserve(textureCount);
  for (u32 i = 0; i < textureCount && reader.ok; ++i) {
    renderer::AuthoringTexture texture;
    texture.width = reader.u32v();
    texture.height = reader.u32v();
    texture.levels = reader.u32v();
    texture.format = reader.u32v();
    texture.samplerFlags = reader.u32v();
    texture.srgb = reader.u32v() != 0;
    texture.mipChain = reader.pods<u8>();
    if (!reader.ok || !texture.valid()) return refuse();
    model.textures.push_back(std::make_shared<renderer::AuthoringTexture>(std::move(texture)));
  }
  model.textureBytes = reader.u64v();
  if (reader.u32v() != kCounterCount) return refuse();
  u32 counters[kCounterCount]{};
  reader.raw(counters, sizeof counters);
  model.skippedTextures = counters[0];
  model.skippedAnimations = counters[1];
  model.skippedSkins = counters[2];
  model.skippedPrimitives = counters[3];
  model.skippedCameras = counters[4];
  model.skippedLights = counters[5];
  model.unappliedTextureTransforms = counters[6];
  model.unappliedOcclusion = counters[7];
  model.bakedTextureTransforms = counters[8];
  model.reducedTextures = counters[9];
  model.dracoPrimitives = counters[10];
  model.meshoptViews = counters[11];
  model.ktx2Images = counters[12];
  model.astcTextures = counters[13];
  model.mirroredNodes = counters[14];
  model.generatedTangentPrimitives = counters[15];
  model.residentTextureDimension = counters[16];
  model.textureNotes = reader.texts();
  char trailer[8]{};
  if (!reader.raw(trailer, sizeof trailer) || std::memcmp(trailer, kMagic, sizeof trailer) != 0 ||
      reader.at != bytes.size())
    return refuse();
  // Coerência mínima que quem publica pressupõe.
  if (model.draws.empty() || model.nodes.empty() || model.drawNodes.size() != model.draws.size() ||
      model.vertices.size() % renderer::MapVertexStride != 0)
    return refuse();
  for (const auto node : model.drawNodes)
    if (node >= model.nodes.size()) return refuse();
  for (const auto &material : model.materials)
    for (const auto texture : material.textureIndices)
      if (texture != renderer::InvalidMapTexture && texture >= model.textures.size()) return refuse();
  out = std::move(model);
  return true;
}
} // namespace ae::resources
