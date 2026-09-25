#include "resources/import_cache.h"
#include "core/sha256.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

namespace ae::resources {
namespace {
constexpr char kMagic[8]{'A', 'S', 'T', 'R', 'A', 'I', 'C', '1'};
// S3: seção opcional de LOD/ordem de índices antes do terminador. Só existe
// quando a importação gerou algo; derivados anteriores continuam válidos.
constexpr char kLodMagic[8]{'A', 'S', 'T', 'R', 'A', 'L', 'O', 'D'};
// Bloco F: seção opcional com a imagem de origem de cada textura.
constexpr char kImageMagic[8]{'A', 'S', 'T', 'R', 'A', 'I', 'M', 'G'};
constexpr u32 kCounterCount = 22;

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
                     "|compression=" + number(limits.textureCompression) + "|astcenc=5.7.0-fast" +
                     "|rootScale=" + [&] {
                       u32 bits = 0;
                       std::memcpy(&bits, &limits.rootScale, sizeof bits);
                       return number(bits);
                     }() +
                     "|normals=" + number(limits.normals) + "|normalWeighting=" + number(limits.normalWeighting) +
                     "|smoothingAngle=" + [&] {
                       u32 bits = 0;
                       std::memcpy(&bits, &limits.smoothingAngle, sizeof bits);
                       return number(bits);
                     }() +
                     "|tangents=" + number(limits.tangents) +
                     "|cameras=" + number(limits.importCameras ? 1 : 0) +
                     "|lights=" + number(limits.importLights ? 1 : 0) +
                     "|imageDimension=" + number(limits.image.maximumDimension) +
                     "|imagePixels=" + number(limits.image.maximumPixels) +
                     "|imageEncoded=" + number(limits.image.maximumEncodedBytes) +
                     // Só entra na chave ligado: perfis sem LOD mantêm os derivados já gravados.
                     (limits.generateLods || limits.optimizePolygonOrder
                          ? "|lods=" + number(limits.generateLods ? 1 : 0) + "," + number(limits.optimizePolygonOrder ? 1 : 0) +
                                "," + number(limits.maximumLodLevels) + "|meshlod=1"
                          : std::string());
  return Sha256::hex(std::span<const u8>(reinterpret_cast<const u8 *>(text.data()), text.size()));
}

std::string importCacheRelativePath(std::string_view key) {
  return ".astra/cache/imports/" + std::string(key) + ".aic";
}

bool writeImportCache(const GltfImport &model, std::string_view key, std::vector<u8> &out,
                      std::vector<u64> *textureOffsets) {
  if (textureOffsets) textureOffsets->clear();
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
    writer.pods(node.morphWeights);
  }
  writer.pods(model.drawNodes);
  writer.pods(model.cameras);
  writer.pods(model.lights);
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
    // Textura parcial (bloco C): o derivado sempre guarda a cadeia inteira.
    std::vector<u8> scratch;
    std::span<const u8> chain;
    if (!renderer::readAuthoringTextureLevels(*texture, 0, scratch, chain)) {
      out.clear();
      return false;
    }
    writer.u64v(chain.size());
    if (textureOffsets) textureOffsets->push_back(out.size());
    writer.raw(chain.data(), chain.size());
  }
  writer.u64v(model.textureBytes);
  const u32 counters[kCounterCount]{model.skippedTextures, model.skippedAnimations, model.skippedSkins,
                                    model.skippedPrimitives, model.skippedCameras, model.skippedLights,
                                    model.unappliedTextureTransforms, model.unappliedOcclusion,
                                    model.bakedTextureTransforms, model.reducedTextures, model.dracoPrimitives,
                                    model.meshoptViews, model.ktx2Images, model.astcTextures, model.mirroredNodes,
                                    model.generatedTangentPrimitives, model.residentTextureDimension,
                                    model.appliedOcclusion, model.generatedNormalPrimitives,
                                    model.texturedPrimitivesWithoutUv, model.stretchedUvPrimitives,
                                    // A pior razão de densidade é o único diagnóstico não inteiro:
                                    // vai como os bits do float, para não perder a casa decimal nem
                                    // abrir um segundo bloco de formato só para ele.
                                    [&] { u32 bits = 0; std::memcpy(&bits, &model.worstTexelDensityRatio, 4); return bits; }()};
  writer.u32v(kCounterCount);
  writer.raw(counters, sizeof counters);
  writer.texts(model.textureNotes);
  writer.texts(model.notes);
  // Schema 6: skins, influências por vértice e clipes.
  writer.u32v(static_cast<u32>(model.skins.size()));
  for (const auto &skin : model.skins) {
    writer.text(skin.name);
    writer.pods(skin.joints);
    writer.pods(skin.inverseBind);
    writer.pods(skin.jointSpheres);
    writer.i32v(skin.skeleton);
  }
  writer.pods(model.drawSkins);
  writer.pods(model.skinInfluences);
  writer.u32v(static_cast<u32>(model.animations.size()));
  for (const auto &clip : model.animations) {
    writer.text(clip.name);
    writer.raw(&clip.duration, sizeof clip.duration);
    writer.u32v(static_cast<u32>(clip.channels.size()));
    for (const auto &channel : clip.channels) {
      writer.u32v(channel.node);
      writer.u32v(static_cast<u32>(channel.path));
      writer.u32v(static_cast<u32>(channel.interpolation));
      writer.u32v(channel.weightCount);
      writer.pods(channel.times);
      writer.pods(channel.values);
    }
  }
  writer.u32v(model.unsupportedAnimationChannels);
  // Schema 7: blend shapes por primitiva e por desenho.
  writer.u32v(static_cast<u32>(model.morphs.size()));
  for (const auto &set : model.morphs) {
    writer.u32v(set.targetCount);
    writer.u32v(set.vertexCount);
    writer.pods(set.deltas);
    writer.pods(set.defaultWeights);
    writer.pods(set.maximumDisplacement);
    writer.texts(set.names);
  }
  writer.pods(model.drawMorphs);
  if (!model.meshLods.empty() || model.lodSourceTriangles) {
    writer.raw(kLodMagic, sizeof kLodMagic);
    writer.pods(model.meshLods);
    writer.u32v(model.lodDraws);writer.u32v(model.lodLevels);
    writer.u32v(model.lodSkippedSmall);writer.u32v(model.lodSkippedDeformed);
    writer.u64v(model.lodSourceTriangles);writer.u64v(model.lodTriangles);
    writer.u32v(model.lodBudgetReached ? 1 : 0);
    writer.raw(&model.acmrBefore, sizeof model.acmrBefore);writer.raw(&model.acmrAfter, sizeof model.acmrAfter);
  }
  // Bloco F: com texturas, a seção de imagens sempre vai (vazia por textura
  // quando quem montou o modelo não sabe a origem).
  if (!model.textures.empty()) {
    auto images = model.textureImages;
    images.resize(model.textures.size());
    writer.raw(kImageMagic, sizeof kImageMagic);
    writer.texts(images);
  }
  // Terminador: um arquivo cortado no meio da escrita nunca passa por inteiro.
  writer.raw(kMagic, sizeof kMagic);
  return true;
}

bool makeTexturesPartial(GltfImport &model, std::span<const u64> textureOffsets, const std::string &path,
                         u32 keepDimension) {
  if (textureOffsets.size() != model.textures.size()) return false;
  std::vector<renderer::SharedAuthoringTexture> next;
  next.reserve(model.textures.size());
  for (usize i = 0; i < model.textures.size(); ++i) {
    const auto &texture = model.textures[i];
    if (!texture || !texture->valid()) return false;
    u32 first = 0;
    for (u32 w = texture->width, h = texture->height; first + 1 < texture->levels && std::max(w, h) > keepDimension;
         ++first, w = w > 1 ? w / 2 : 1, h = h > 1 ? h / 2 : 1) {}
    if (!first || texture->partial()) {next.push_back(texture); continue;}
    auto partial = std::make_shared<renderer::AuthoringTexture>();
    partial->width = texture->width;partial->height = texture->height;partial->levels = texture->levels;
    partial->srgb = texture->srgb;partial->samplerFlags = texture->samplerFlags;partial->format = texture->format;
    partial->firstLevel = first;
    partial->file = std::make_shared<renderer::AuthoringTextureFile>(renderer::AuthoringTextureFile{path, textureOffsets[i]});
    const u64 skip = texture->expectedBytes() - texture->chainBytesFrom(first);
    partial->mipChain.assign(texture->mipChain.begin() + static_cast<std::ptrdiff_t>(skip), texture->mipChain.end());
    if (!partial->valid()) return false;
    next.push_back(std::move(partial));
  }
  model.textures = std::move(next);
  return true;
}

bool readImportCache(std::span<const u8> bytes, std::string_view key, GltfImport &out,
                     const ImportCachePartialTextures *partial) {
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
    node.morphWeights = reader.pods<float>();
    if (node.parent < -1 || node.parent >= static_cast<i32>(i)) return refuse();
  }
  model.drawNodes = reader.pods<u32>();
  model.cameras = reader.pods<GltfImportCamera>();
  model.lights = reader.pods<GltfImportLight>();
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
    if (!partial) texture.mipChain = reader.pods<u8>();
    else {
      // Mesmo layout de `pods`: contagem de 8 bytes e a cadeia logo depois.
      const u64 count = reader.u64v();
      const u64 at = reader.at;
      if (!reader.ok || count != texture.expectedBytes() || count > bytes.size() - at) return refuse();
      u32 first = 0;
      for (u32 w = texture.width, h = texture.height; first + 1 < texture.levels && std::max(w, h) > partial->keepDimension;
           ++first, w = w > 1 ? w / 2 : 1, h = h > 1 ? h / 2 : 1) {}
      const u64 skip = count - texture.chainBytesFrom(first);
      texture.mipChain.assign(bytes.begin() + static_cast<std::ptrdiff_t>(at + skip),
                              bytes.begin() + static_cast<std::ptrdiff_t>(at + count));
      if (first) {
        texture.firstLevel = first;
        texture.file = std::make_shared<renderer::AuthoringTextureFile>(renderer::AuthoringTextureFile{partial->path, at});
      }
      reader.at += static_cast<usize>(count);
    }
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
  model.appliedOcclusion = counters[17];
  model.generatedNormalPrimitives = counters[18];
  model.texturedPrimitivesWithoutUv = counters[19];
  model.stretchedUvPrimitives = counters[20];
  std::memcpy(&model.worstTexelDensityRatio, &counters[21], 4);
  if (!std::isfinite(model.worstTexelDensityRatio) || model.worstTexelDensityRatio < 0) return refuse();
  model.textureNotes = reader.texts();
  model.notes = reader.texts();
  const u32 skinCount = reader.u32v();
  if (!reader.ok || skinCount > bytes.size()) return refuse();
  model.skins.resize(skinCount);
  for (auto &skin : model.skins) {
    skin.name = reader.text();
    skin.joints = reader.pods<u32>();
    skin.inverseBind = reader.pods<float>();
    skin.jointSpheres = reader.pods<float>();
    skin.skeleton = reader.i32v();
    if (!reader.ok) return refuse();
  }
  model.drawSkins = reader.pods<i32>();
  model.skinInfluences = reader.pods<u8>();
  const u32 clipCount = reader.u32v();
  if (!reader.ok || clipCount > bytes.size()) return refuse();
  model.animations.resize(clipCount);
  for (auto &clip : model.animations) {
    clip.name = reader.text();
    reader.raw(&clip.duration, sizeof clip.duration);
    const u32 channels = reader.u32v();
    if (!reader.ok || channels > bytes.size()) return refuse();
    clip.channels.resize(channels);
    for (auto &channel : clip.channels) {
      channel.node = reader.u32v();
      const u32 path = reader.u32v(), interpolation = reader.u32v();
      if (path > 3 || interpolation > 2) return refuse();
      channel.path = static_cast<AnimationPath>(path);
      channel.interpolation = static_cast<AnimationInterpolation>(interpolation);
      channel.weightCount = reader.u32v();
      channel.times = reader.pods<float>();
      channel.values = reader.pods<float>();
      if (!reader.ok) return refuse();
    }
  }
  model.unsupportedAnimationChannels = reader.u32v();
  const u32 morphCount = reader.u32v();
  if (!reader.ok || morphCount > bytes.size()) return refuse();
  model.morphs.resize(morphCount);
  for (auto &set : model.morphs) {
    set.targetCount = reader.u32v();
    set.vertexCount = reader.u32v();
    set.deltas = reader.pods<float>();
    set.defaultWeights = reader.pods<float>();
    set.maximumDisplacement = reader.pods<float>();
    set.names = reader.texts();
    if (!reader.ok || !validMorphTargetSet(set)) return refuse();
  }
  model.drawMorphs = reader.pods<i32>();
  if (reader.ok && bytes.size() - reader.at >= sizeof kLodMagic &&
      std::memcmp(bytes.data() + reader.at, kLodMagic, sizeof kLodMagic) == 0) {
    reader.at += sizeof kLodMagic;
    model.meshLods = reader.pods<renderer::MeshLodLevel>();
    model.lodDraws = reader.u32v();model.lodLevels = reader.u32v();
    model.lodSkippedSmall = reader.u32v();model.lodSkippedDeformed = reader.u32v();
    model.lodSourceTriangles = reader.u64v();model.lodTriangles = reader.u64v();
    model.lodBudgetReached = reader.u32v() != 0;
    reader.raw(&model.acmrBefore, sizeof model.acmrBefore);reader.raw(&model.acmrAfter, sizeof model.acmrAfter);
    if (!reader.ok) return refuse();
  }
  if (reader.ok && bytes.size() - reader.at >= sizeof kImageMagic &&
      std::memcmp(bytes.data() + reader.at, kImageMagic, sizeof kImageMagic) == 0) {
    reader.at += sizeof kImageMagic;
    model.textureImages = reader.texts();
    if (!reader.ok || model.textureImages.size() != model.textures.size()) return refuse();
  } else if (reader.ok && !model.textures.empty()) {
    // Derivado de antes da seção de imagens: sem ela o editor não liga textura
    // e arquivo. Recusado para o importador refazer e regravar no mesmo lugar.
    return refuse();
  }
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
  // Cada nível aponta para um desenho e uma faixa existentes, em ordem e com
  // erro não decrescente: é o que a seleção pressupõe sem conferir por quadro.
  for (usize i = 0; i < model.meshLods.size(); ++i) {
    const auto &lod = model.meshLods[i];
    if (lod.draw >= model.draws.size() || lod.level < 1 || lod.level >= renderer::MeshLodMaximumLevels ||
        lod.indexCount < 3 || u64(lod.firstIndex) + lod.indexCount > model.indices.size() ||
        !std::isfinite(lod.geometricError) || lod.geometricError < 0)
      return refuse();
    if (i && model.meshLods[i - 1].draw == lod.draw &&
        (model.meshLods[i - 1].level + 1 != lod.level || model.meshLods[i - 1].geometricError > lod.geometricError))
      return refuse();
    if (i && model.meshLods[i - 1].draw > lod.draw) return refuse();
  }
  // Skin e animação do cache endereçam a árvore e os vértices: qualquer índice
  // fora dela é arquivo corrompido, e a reimportação é o caminho seguro.
  if (model.drawSkins.size() != model.draws.size() || model.drawMorphs.size() != model.draws.size()) return refuse();
  for (usize d = 0; d < model.drawMorphs.size(); ++d) {
    const i32 morph = model.drawMorphs[d];
    if (morph < -1 || morph >= static_cast<i32>(model.morphs.size())) return refuse();
    // Um vértice do desenho sem linha de deltas leria fora do conjunto.
    if (morph >= 0) {
      const auto &draw = model.draws[d];
      u32 highest = 0;
      for (u32 i = 0; i < draw.indexCount; ++i) {
        if (usize(draw.firstIndex) + i >= model.indices.size()) return refuse();
        highest = std::max(highest, model.indices[draw.firstIndex + i]);
      }
      if (highest >= model.morphs[static_cast<usize>(morph)].vertexCount) return refuse();
    }
  }
  if (!model.skinInfluences.empty() &&
      model.skinInfluences.size() != model.vertices.size() / renderer::MapVertexStride * SkinInfluenceStride)
    return refuse();
  for (const auto &skin : model.skins) {
    if (skin.joints.size() > MaximumSkinJoints) return refuse();
    if (skin.joints.empty()) continue;
    if (skin.inverseBind.size() != skin.joints.size() * 16 || skin.jointSpheres.size() != skin.joints.size() * 4 ||
        skin.skeleton < -1 || skin.skeleton >= static_cast<i32>(model.nodes.size()))
      return refuse();
    for (const auto joint : skin.joints) if (joint >= model.nodes.size()) return refuse();
    for (const auto value : skin.inverseBind) if (!std::isfinite(value)) return refuse();
  }
  for (const auto skin : model.drawSkins)
    if (skin < -1 || skin >= static_cast<i32>(model.skins.size()) ||
        (skin >= 0 && (model.skins[static_cast<usize>(skin)].joints.empty() || model.skinInfluences.empty())))
      return refuse();
  for (const auto &clip : model.animations) {
    if (!std::isfinite(clip.duration) || clip.duration < 0 || clip.channels.empty()) return refuse();
    for (const auto &channel : clip.channels)
      if (channel.node >= model.nodes.size() || !validAnimationChannel(channel)) return refuse();
  }
  // Uma câmera do cache aponta para um nó: índice fora da árvore é arquivo
  // corrompido, e ler lente inválida de volta seria pior que reimportar.
  for (const auto &camera : model.cameras)
    if (camera.node >= model.nodes.size() || !std::isfinite(camera.nearPlane) || camera.nearPlane <= 0 ||
        !std::isfinite(camera.farPlane) || camera.farPlane < 0 ||
        !std::isfinite(camera.verticalFovDegrees) || !std::isfinite(camera.orthographicHalfHeight))
      return refuse();
  for (const auto &light : model.lights) {
    if (light.node >= model.nodes.size() || light.kind > 2 || !std::isfinite(light.intensity) ||
        light.intensity < 0 || light.intensity > 1000000 || !std::isfinite(light.range) ||
        light.range <= 0 || light.range > 1000 || !std::isfinite(light.innerAngle) ||
        !std::isfinite(light.outerAngle) || light.innerAngle < 0 || light.outerAngle > 89 ||
        light.innerAngle > light.outerAngle)
      return refuse();
    for (const auto channel : light.color)
      if (!std::isfinite(channel) || channel < 0 || channel > 1) return refuse();
  }
  for (const auto &material : model.materials)
    for (const auto texture : material.textureIndices)
      if (texture != renderer::InvalidMapTexture && texture >= model.textures.size()) return refuse();
  out = std::move(model);
  return true;
}
} // namespace ae::resources
