#include "resources/gltf_import.h"
#include "resources/json_reader.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ae::resources {
namespace {
using Node = JsonDocument::Node;
using Kind = JsonDocument::Kind;

constexpr u32 kGlbMagic = 0x46546C67;  // "glTF"
constexpr u32 kChunkJson = 0x4E4F534A; // "JSON"
constexpr u32 kChunkBin = 0x004E4942;  // "BIN\0"

constexpr u32 kComponentByte = 5120, kComponentUnsignedByte = 5121;
constexpr u32 kComponentShort = 5122, kComponentUnsignedShort = 5123;
constexpr u32 kComponentUnsignedInt = 5125, kComponentFloat = 5126;

u32 componentSize(u32 component) {
  switch (component) {
    case kComponentByte: case kComponentUnsignedByte: return 1;
    case kComponentShort: case kComponentUnsignedShort: return 2;
    case kComponentUnsignedInt: case kComponentFloat: return 4;
    default: return 0;
  }
}
u32 componentCount(std::string_view type) {
  if (type == "SCALAR") return 1;
  if (type == "VEC2") return 2;
  if (type == "VEC3") return 3;
  if (type == "VEC4") return 4;
  if (type == "MAT4") return 16;
  return 0;
}

u32 readU32(std::span<const u8> bytes, usize offset) {
  u32 value = 0;
  std::memcpy(&value, bytes.data() + offset, 4);
  return value;
}

i16 packSnorm(float value) {
  const float clamped = std::fmin(std::fmax(value, -1.f), 1.f);
  return static_cast<i16>(std::lround(clamped * 32767.f));
}

void multiply(const float a[16], const float b[16], float out[16]) {
  float value[16]{};
  for (u32 c = 0; c < 4; ++c)
    for (u32 r = 0; r < 4; ++r)
      for (u32 k = 0; k < 4; ++k) value[c * 4 + r] += a[k * 4 + r] * b[c * 4 + k];
  std::copy(value, value + 16, out);
}

struct Accessor {
  u32 component = 0, components = 0, count = 0;
  bool normalized = false;
  const u8 *data = nullptr;
  u32 stride = 0;
};

// Um leitor por acessor, já resolvido contra a view e o buffer: quem consome
// trabalha em floats e não repete a aritmética de offset em cada vértice.
float readComponent(const Accessor &accessor, u32 element, u32 component) {
  const u8 *base = accessor.data + static_cast<usize>(element) * accessor.stride +
                   static_cast<usize>(component) * componentSize(accessor.component);
  switch (accessor.component) {
    case kComponentFloat: { float value; std::memcpy(&value, base, 4); return value; }
    case kComponentUnsignedByte: { u8 value; std::memcpy(&value, base, 1);
      return accessor.normalized ? static_cast<float>(value) / 255.f : static_cast<float>(value); }
    case kComponentByte: { i8 value; std::memcpy(&value, base, 1);
      return accessor.normalized ? std::fmax(static_cast<float>(value) / 127.f, -1.f) : static_cast<float>(value); }
    case kComponentUnsignedShort: { u16 value; std::memcpy(&value, base, 2);
      return accessor.normalized ? static_cast<float>(value) / 65535.f : static_cast<float>(value); }
    case kComponentShort: { i16 value; std::memcpy(&value, base, 2);
      return accessor.normalized ? std::fmax(static_cast<float>(value) / 32767.f, -1.f) : static_cast<float>(value); }
    case kComponentUnsignedInt: { u32 value; std::memcpy(&value, base, 4); return static_cast<float>(value); }
    default: return 0;
  }
}

struct Importer {
  const JsonDocument *json = nullptr;
  std::span<const u8> binary;
  const GltfImportLimits *limits = nullptr;
  const GltfImportProgress *progress = nullptr;
  GltfImport *out = nullptr;

  bool fail(const char *message) {
    out->diagnostic = message;
    return false;
  }
  bool cancelled() {
    if (!progress->cancelled || !progress->cancelled(progress->context)) return false;
    out->cancelled = true;
    out->diagnostic = "Importação cancelada.";
    return true;
  }
  void report(float fraction, const char *stage) {
    if (progress->report) progress->report(progress->context, fraction, stage);
  }

  const Node *array(const Node &root, std::string_view name) const {
    const auto *value = json->member(root, name);
    return value && value->kind == Kind::Array ? value : nullptr;
  }

  bool resolveAccessor(const Node &root, i64 index, Accessor &out, const char *what) {
    const auto *accessors = array(root, "accessors");
    if (!accessors || index < 0 || index >= accessors->childCount) return fail(what);
    const auto &accessor = *json->child(*accessors, static_cast<u32>(index));
    if (accessor.kind != Kind::Object) return fail(what);
    // Acessor esparso reconstrói valores a partir de uma lista de exceções.
    // Ignorá-lo daria geometria silenciosamente errada; recusar é honesto.
    if (json->member(accessor, "sparse")) return fail("Acessor esparso não é suportado nesta importação.");
    out.component = static_cast<u32>(json->number(accessor, "componentType", 0));
    out.components = componentCount(json->string(accessor, "type"));
    out.count = static_cast<u32>(json->number(accessor, "count", 0));
    out.normalized = json->boolean(accessor, "normalized", false);
    const auto size = componentSize(out.component);
    if (!size || !out.components || !out.count) return fail(what);

    const auto viewIndex = json->index(accessor, "bufferView");
    const auto *views = array(root, "bufferViews");
    if (!views || viewIndex < 0 || viewIndex >= views->childCount) return fail(what);
    const auto &view = *json->child(*views, static_cast<u32>(viewIndex));
    if (view.kind != Kind::Object) return fail(what);
    if (json->index(view, "buffer") != 0)
      return fail("Este GLB aponta para um buffer externo; só o bloco binário embutido é lido.");
    const auto viewOffset = static_cast<u64>(json->number(view, "byteOffset", 0));
    const auto viewLength = static_cast<u64>(json->number(view, "byteLength", 0));
    const auto declaredStride = static_cast<u32>(json->number(view, "byteStride", 0));
    const auto accessorOffset = static_cast<u64>(json->number(accessor, "byteOffset", 0));
    out.stride = declaredStride ? declaredStride : size * out.components;
    if (out.stride < size * out.components) return fail(what);
    if (viewOffset > binary.size() || viewLength > binary.size() - viewOffset) return fail(what);
    const u64 span = static_cast<u64>(out.stride) * (out.count - 1) + size * out.components;
    if (accessorOffset > viewLength || span > viewLength - accessorOffset) return fail(what);
    out.data = binary.data() + viewOffset + accessorOffset;
    return true;
  }

  // Transformação local do nó. `matrix` e TRS são exclusivos pelo formato;
  // aceitar os dois juntos escolheria um em silêncio.
  bool localMatrix(const Node &node, float out[16]) {
    const auto *matrix = json->member(node, "matrix");
    const bool hasTrs = json->member(node, "translation") || json->member(node, "rotation") ||
                        json->member(node, "scale");
    if (matrix) {
      if (hasTrs) return fail("Nó com `matrix` e TRS ao mesmo tempo: o arquivo é ambíguo.");
      if (matrix->kind != Kind::Array || matrix->childCount != 16) return fail("Matriz de nó inválida.");
      for (u32 i = 0; i < 16; ++i) {
        const auto *value = json->child(*matrix, i);
        if (!value || value->kind != Kind::Number) return fail("Matriz de nó inválida.");
        out[i] = static_cast<float>(value->number);
      }
      return true;
    }
    float translation[3]{0, 0, 0}, rotation[4]{0, 0, 0, 1}, scale[3]{1, 1, 1};
    if (!readVector(node, "translation", translation, 3)) return false;
    if (!readVector(node, "rotation", rotation, 4)) return false;
    if (!readVector(node, "scale", scale, 3)) return false;
    const float length = std::sqrt(rotation[0] * rotation[0] + rotation[1] * rotation[1] +
                                   rotation[2] * rotation[2] + rotation[3] * rotation[3]);
    if (!(length > 1e-8f)) return fail("Quaternião de nó inválido.");
    const float x = rotation[0] / length, y = rotation[1] / length;
    const float z = rotation[2] / length, w = rotation[3] / length;
    const float basis[9]{1 - 2 * (y * y + z * z), 2 * (x * y + z * w),     2 * (x * z - y * w),
                         2 * (x * y - z * w),     1 - 2 * (x * x + z * z), 2 * (y * z + x * w),
                         2 * (x * z + y * w),     2 * (y * z - x * w),     1 - 2 * (x * x + y * y)};
    std::fill(out, out + 16, 0.f);
    for (u32 column = 0; column < 3; ++column)
      for (u32 row = 0; row < 3; ++row) out[column * 4 + row] = basis[column * 3 + row] * scale[column];
    out[12] = translation[0];
    out[13] = translation[1];
    out[14] = translation[2];
    out[15] = 1;
    return true;
  }

  bool readVector(const Node &node, std::string_view name, float *out, u32 count) {
    const auto *value = json->member(node, name);
    if (!value) return true;
    if (value->kind != Kind::Array || value->childCount != count) return fail("Vetor de nó inválido.");
    for (u32 i = 0; i < count; ++i) {
      const auto *component = json->child(*value, i);
      if (!component || component->kind != Kind::Number) return fail("Vetor de nó inválido.");
      out[i] = static_cast<float>(component->number);
    }
    return true;
  }

  bool readMaterials(const Node &root) {
    const auto *materials = array(root, "materials");
    const u32 count = materials ? materials->childCount : 0;
    // Um material neutro fecha a lista: primitiva sem material é legal em glTF
    // e precisa de alguma coisa para apontar.
    out->materials.resize(count + 1);
    for (auto &material : out->materials) {
      material.baseColorFactor[0] = material.baseColorFactor[1] = material.baseColorFactor[2] = 1;
      material.baseColorFactor[3] = 1;
      material.roughness = 1;
      material.metallic = 1;
      material.normalScale = 1;
      material.specular = 1;
      material.alphaCutoff = .5f;
      for (auto &texture : material.textureIndices) texture = renderer::InvalidMapTexture;
    }
    for (u32 i = 0; i < count; ++i) {
      const auto &source = *json->child(*materials, i);
      if (source.kind != Kind::Object) return fail("Material inválido.");
      auto &target = out->materials[i];
      if (const auto *pbr = json->member(source, "pbrMetallicRoughness"); pbr && pbr->kind == Kind::Object) {
        if (!readVector(*pbr, "baseColorFactor", target.baseColorFactor, 4)) return false;
        target.roughness = static_cast<float>(json->number(*pbr, "roughnessFactor", 1));
        target.metallic = static_cast<float>(json->number(*pbr, "metallicFactor", 1));
        if (json->member(*pbr, "baseColorTexture")) ++out->skippedTextures;
        if (json->member(*pbr, "metallicRoughnessTexture")) ++out->skippedTextures;
      }
      float emissive[3]{0, 0, 0};
      if (!readVector(source, "emissiveFactor", emissive, 3)) return false;
      std::copy(emissive, emissive + 3, target.emissiveFactorAndStrength);
      target.emissiveFactorAndStrength[3] = 1;
      if (json->member(source, "normalTexture")) ++out->skippedTextures;
      if (json->member(source, "emissiveTexture")) ++out->skippedTextures;
      if (json->member(source, "occlusionTexture")) ++out->skippedTextures;
      const auto alphaMode = json->string(source, "alphaMode");
      if (alphaMode == "BLEND") target.flags |= renderer::MapMaterialBlend;
      else if (alphaMode == "MASK") {
        target.flags |= renderer::MapMaterialAlphaMask;
        target.alphaCutoff = static_cast<float>(json->number(source, "alphaCutoff", .5));
      }
      if (json->boolean(source, "doubleSided", false)) target.flags |= renderer::MapMaterialDoubleSided;
    }
    return true;
  }

  // Uma primitiva vira um bloco de vértices e um bloco de índices no mesmo par
  // de buffers que o pacote de mapa usa. Instâncias reaproveitam o bloco: o
  // mesmo mesh em dois nós custa dois desenhos, não duas cópias da geometria.
  struct PrimitiveRange { u32 firstIndex = 0, indexCount = 0, vertexOffset = 0, material = 0;
                          float center[3]{}; float radius = 0; std::string key; };

  bool readPrimitive(const Node &root, const Node &primitive, PrimitiveRange &range) {
    if (static_cast<u32>(json->number(primitive, "mode", 4)) != 4) { ++out->skippedPrimitives; return true; }
    const auto *attributes = json->member(primitive, "attributes");
    if (!attributes || attributes->kind != Kind::Object) return fail("Primitiva sem atributos.");
    Accessor position{};
    if (!resolveAccessor(root, json->index(*attributes, "POSITION"), position,
                         "Primitiva sem POSITION utilizável."))
      return false;
    if (position.components != 3) return fail("POSITION precisa ser VEC3.");

    Accessor normal{}, tangent{}, uv0{}, uv1{}, color{};
    const bool hasNormal = json->index(*attributes, "NORMAL") >= 0 &&
                           resolveAccessor(root, json->index(*attributes, "NORMAL"), normal, "NORMAL inválido.");
    if (json->index(*attributes, "NORMAL") >= 0 && !hasNormal) return false;
    const bool hasTangent = json->index(*attributes, "TANGENT") >= 0 &&
                            resolveAccessor(root, json->index(*attributes, "TANGENT"), tangent, "TANGENT inválido.");
    if (json->index(*attributes, "TANGENT") >= 0 && !hasTangent) return false;
    const bool hasUv0 = json->index(*attributes, "TEXCOORD_0") >= 0 &&
                        resolveAccessor(root, json->index(*attributes, "TEXCOORD_0"), uv0, "TEXCOORD_0 inválido.");
    if (json->index(*attributes, "TEXCOORD_0") >= 0 && !hasUv0) return false;
    const bool hasUv1 = json->index(*attributes, "TEXCOORD_1") >= 0 &&
                        resolveAccessor(root, json->index(*attributes, "TEXCOORD_1"), uv1, "TEXCOORD_1 inválido.");
    if (json->index(*attributes, "TEXCOORD_1") >= 0 && !hasUv1) return false;
    const bool hasColor = json->index(*attributes, "COLOR_0") >= 0 &&
                          resolveAccessor(root, json->index(*attributes, "COLOR_0"), color, "COLOR_0 inválido.");
    if (json->index(*attributes, "COLOR_0") >= 0 && !hasColor) return false;

    const u64 vertexBase = out->vertices.size() / renderer::MapVertexStride;
    if (vertexBase + position.count > limits->maximumVertices)
      return fail("O arquivo passa do limite de vértices desta importação.");
    range.vertexOffset = static_cast<u32>(vertexBase);

    float minimum[3]{0, 0, 0}, maximum[3]{0, 0, 0};
    out->vertices.resize(out->vertices.size() + static_cast<usize>(position.count) * renderer::MapVertexStride);
    u8 *write = out->vertices.data() + vertexBase * renderer::MapVertexStride;
    for (u32 v = 0; v < position.count; ++v) {
      if ((v & 0xFFFF) == 0 && cancelled()) return false;
      float p[3];
      for (u32 axis = 0; axis < 3; ++axis) p[axis] = readComponent(position, v, axis);
      if (!std::isfinite(p[0]) || !std::isfinite(p[1]) || !std::isfinite(p[2]))
        return fail("Posição não finita na geometria.");
      for (u32 axis = 0; axis < 3; ++axis) {
        if (!v || p[axis] < minimum[axis]) minimum[axis] = p[axis];
        if (!v || p[axis] > maximum[axis]) maximum[axis] = p[axis];
      }
      u8 *vertex = write + static_cast<usize>(v) * renderer::MapVertexStride;
      std::memcpy(vertex, p, 12);
      i16 packed[4]{0, 0, 0, 0};
      if (hasNormal && normal.components >= 3)
        for (u32 axis = 0; axis < 3; ++axis) packed[axis] = packSnorm(readComponent(normal, v, axis));
      std::memcpy(vertex + 12, packed, 8);
      i16 packedTangent[4]{0, 0, 0, 0};
      if (hasTangent)
        for (u32 axis = 0; axis < std::min(tangent.components, 4u); ++axis)
          packedTangent[axis] = packSnorm(readComponent(tangent, v, axis));
      std::memcpy(vertex + 20, packedTangent, 8);
      float coordinates[4]{0, 0, 0, 0};
      if (hasUv0 && uv0.components >= 2) { coordinates[0] = readComponent(uv0, v, 0); coordinates[1] = readComponent(uv0, v, 1); }
      if (hasUv1 && uv1.components >= 2) { coordinates[2] = readComponent(uv1, v, 0); coordinates[3] = readComponent(uv1, v, 1); }
      for (auto &value : coordinates) if (!std::isfinite(value)) value = 0;
      std::memcpy(vertex + 28, coordinates, 16);
      u8 rgba[4]{255, 255, 255, 255};
      if (hasColor)
        for (u32 channel = 0; channel < std::min(color.components, 4u); ++channel)
          rgba[channel] = static_cast<u8>(std::lround(std::fmin(std::fmax(readComponent(color, v, channel), 0.f), 1.f) * 255.f));
      std::memcpy(vertex + 44, rgba, 4);
    }
    for (u32 axis = 0; axis < 3; ++axis) range.center[axis] = (minimum[axis] + maximum[axis]) * .5f;
    float radius = 0;
    for (u32 axis = 0; axis < 3; ++axis) {
      const float half = (maximum[axis] - minimum[axis]) * .5f;
      radius += half * half;
    }
    range.radius = std::sqrt(radius);

    range.firstIndex = static_cast<u32>(out->indices.size());
    const auto indicesIndex = json->index(primitive, "indices");
    if (indicesIndex >= 0) {
      Accessor indices{};
      if (!resolveAccessor(root, indicesIndex, indices, "Índices inválidos.")) return false;
      if (indices.components != 1) return fail("Índices precisam ser SCALAR.");
      if (indices.count % 3) return fail("A primitiva não tem um número de índices múltiplo de três.");
      if (out->indices.size() + indices.count > limits->maximumIndices)
        return fail("O arquivo passa do limite de índices desta importação.");
      out->indices.reserve(out->indices.size() + indices.count);
      for (u32 i = 0; i < indices.count; ++i) {
        if ((i & 0xFFFF) == 0 && cancelled()) return false;
        const auto value = static_cast<u32>(readComponent(indices, i, 0));
        if (value >= position.count) return fail("Índice fora da faixa de vértices da primitiva.");
        out->indices.push_back(value);
      }
      range.indexCount = indices.count;
    } else {
      if (position.count % 3) return fail("Primitiva sem índices precisa de vértices múltiplos de três.");
      if (out->indices.size() + position.count > limits->maximumIndices)
        return fail("O arquivo passa do limite de índices desta importação.");
      for (u32 i = 0; i < position.count; ++i) out->indices.push_back(i);
      range.indexCount = position.count;
    }
    const auto material = json->index(primitive, "material");
    range.material = material >= 0 && static_cast<usize>(material) + 1 < out->materials.size()
                         ? static_cast<u32>(material)
                         : static_cast<u32>(out->materials.size() - 1);
    return true;
  }
};
} // namespace

bool importGlb(std::span<const u8> bytes, const GltfImportLimits &limits,
               const GltfImportProgress &progress, GltfImport &out) {
  GltfImport result;
  const auto giveUp = [&](const char *message) {
    result.diagnostic = message;
    const bool cancelled = result.cancelled;
    out = GltfImport{};
    out.diagnostic = message;
    out.cancelled = cancelled;
    return false;
  };
  if (bytes.size() < 12) return giveUp("Arquivo pequeno demais para ser um GLB.");
  if (bytes.size() > limits.maximumBytes) return giveUp("Arquivo maior que o limite desta importação.");
  if (readU32(bytes, 0) != kGlbMagic)
    return giveUp("Este arquivo não é um GLB. Um `.gltf` depende de arquivos ao lado dele, "
                  "que o seletor do Android não entrega; exporte como `.glb`.");
  if (readU32(bytes, 4) != 2) return giveUp("Só GLB versão 2 é lido.");
  const u64 declared = readU32(bytes, 8);
  if (declared > bytes.size()) return giveUp("O GLB declara mais bytes do que o arquivo tem.");

  std::string_view json;
  std::span<const u8> binary;
  usize cursor = 12;
  while (cursor + 8 <= declared) {
    const u64 length = readU32(bytes, cursor);
    const u32 type = readU32(bytes, cursor + 4);
    cursor += 8;
    if (length > declared - cursor) return giveUp("Bloco do GLB passa do fim do arquivo.");
    if (type == kChunkJson && json.empty())
      json = std::string_view(reinterpret_cast<const char *>(bytes.data() + cursor), length);
    else if (type == kChunkBin && binary.empty())
      binary = bytes.subspan(cursor, length);
    // Blocos desconhecidos são ignorados pelo próprio formato.
    cursor += length + ((4 - (length % 4)) % 4);
  }
  if (json.empty()) return giveUp("GLB sem bloco JSON.");

  JsonDocument document;
  if (!JsonDocument::parse(json, document)) return giveUp("O JSON do GLB está mal formado.");
  const auto *root = document.root();
  if (!root || root->kind != Kind::Object) return giveUp("O JSON do GLB não é um objeto.");

  Importer importer{&document, binary, &limits, &progress, &result};
  importer.report(0.f, "Lendo o arquivo");
  if (importer.cancelled()) return giveUp("Importação cancelada.");

  // Extensões exigidas mudam o significado dos dados. Importar ignorando uma
  // delas produziria geometria errada com cara de certa.
  if (const auto *required = importer.array(*root, "extensionsRequired"); required && required->childCount)
    return giveUp("Este GLB exige extensões do formato que esta importação não lê.");

  if (const auto *animations = importer.array(*root, "animations")) result.skippedAnimations = animations->childCount;
  if (const auto *skins = importer.array(*root, "skins")) result.skippedSkins = skins->childCount;
  if (const auto *cameras = importer.array(*root, "cameras")) result.skippedCameras = cameras->childCount;

  if (!importer.readMaterials(*root)) return giveUp(result.diagnostic.c_str());
  importer.report(.1f, "Materiais");

  const auto *meshes = importer.array(*root, "meshes");
  const auto *nodes = importer.array(*root, "nodes");
  if (!meshes || !nodes) return giveUp("GLB sem malhas ou sem nós.");
  if (nodes->childCount > limits.maximumNodes) return giveUp("O arquivo passa do limite de nós desta importação.");

  // Geometria primeiro, uma vez por mesh: instâncias reaproveitam os blocos.
  std::vector<std::vector<Importer::PrimitiveRange>> byMesh(meshes->childCount);
  for (u32 m = 0; m < meshes->childCount; ++m) {
    if (importer.cancelled()) return giveUp("Importação cancelada.");
    const auto &mesh = *document.child(*meshes, m);
    if (mesh.kind != Kind::Object) return giveUp("Malha inválida.");
    const auto meshName = document.string(mesh, "name");
    const std::string meshKey = meshName.empty() ? "m" + std::to_string(m) : std::string(meshName);
    const auto *primitives = importer.array(mesh, "primitives");
    if (!primitives) continue;
    for (u32 p = 0; p < primitives->childCount; ++p) {
      const auto &primitive = *document.child(*primitives, p);
      if (primitive.kind != Kind::Object) return giveUp("Primitiva inválida.");
      Importer::PrimitiveRange range;
      range.key = meshKey + "#" + std::to_string(p);
      if (!importer.readPrimitive(*root, primitive, range)) return giveUp(result.diagnostic.c_str());
      if (range.indexCount) byMesh[m].push_back(range);
    }
    importer.report(.1f + .6f * static_cast<float>(m + 1) / static_cast<float>(meshes->childCount), "Geometria");
  }

  // Hierarquia depois: cada nó com malha vira um desenho com a matriz de mundo
  // acumulada. Iterativo e com visitados, para que um arquivo com ciclo falhe
  // em vez de rodar para sempre.
  struct Pending { u32 node; float world[16]; };
  std::vector<Pending> stack;
  std::vector<bool> visited(nodes->childCount, false);
  const float identity[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  const auto *scenes = importer.array(*root, "scenes");
  const auto sceneIndex = document.index(*root, "scene");
  std::vector<u32> roots;
  if (scenes && scenes->childCount) {
    const auto chosen = sceneIndex >= 0 && sceneIndex < scenes->childCount ? static_cast<u32>(sceneIndex) : 0u;
    const auto &scene = *document.child(*scenes, chosen);
    if (const auto *list = importer.array(scene, "nodes"))
      for (u32 i = 0; i < list->childCount; ++i) {
        const auto *value = document.child(*list, i);
        if (!value || value->kind != Kind::Number) return giveUp("Lista de nós da cena inválida.");
        roots.push_back(static_cast<u32>(value->number));
      }
  } else {
    // Sem cena declarada, todo nó sem pai é raiz.
    std::vector<bool> child(nodes->childCount, false);
    for (u32 n = 0; n < nodes->childCount; ++n) {
      const auto *children = importer.array(*document.child(*nodes, n), "children");
      if (!children) continue;
      for (u32 c = 0; c < children->childCount; ++c) {
        const auto *value = document.child(*children, c);
        if (!value || value->kind != Kind::Number) return giveUp("Lista de filhos inválida.");
        const auto index = static_cast<u32>(value->number);
        if (index >= nodes->childCount) return giveUp("Nó filho inexistente.");
        child[index] = true;
      }
    }
    for (u32 n = 0; n < nodes->childCount; ++n) if (!child[n]) roots.push_back(n);
  }
  // Empilhados ao contrário: a pilha desempilha do fim, e a ordem dos desenhos
  // precisa ser a ordem do ARQUIVO. Ela vira a ordem da hierarquia no editor e,
  // por causa das chaves estáveis, também a ordem em que o usuário reconhece os
  // objetos numa reimportação.
  for (auto index = roots.size(); index > 0; --index) {
    if (roots[index - 1] >= nodes->childCount) return giveUp("Nó raiz inexistente.");
    Pending entry{roots[index - 1], {}};
    std::copy(identity, identity + 16, entry.world);
    stack.push_back(entry);
  }

  while (!stack.empty()) {
    if (importer.cancelled()) return giveUp("Importação cancelada.");
    const auto entry = stack.back();
    stack.pop_back();
    if (visited[entry.node]) return giveUp("A hierarquia do arquivo tem um ciclo ou um nó com dois pais.");
    visited[entry.node] = true;
    const auto &node = *document.child(*nodes, entry.node);
    if (node.kind != Kind::Object) return giveUp("Nó inválido.");
    float local[16];
    if (!importer.localMatrix(node, local)) return giveUp(result.diagnostic.c_str());
    float world[16];
    multiply(entry.world, local, world);
    for (u32 i = 0; i < 16; ++i) if (!std::isfinite(world[i])) return giveUp("Transformação de nó não finita.");

    if (document.member(node, "camera")) ++result.skippedCameras;
    const auto meshIndex = document.index(node, "mesh");
    if (meshIndex >= 0 && meshIndex < meshes->childCount) {
      if (document.index(node, "skin") >= 0) ++result.skippedSkins;
      const auto name = document.string(node, "name");
      const std::string nodeKey = name.empty() ? "n" + std::to_string(entry.node) : std::string(name);
      for (const auto &range : byMesh[static_cast<u32>(meshIndex)]) {
        if (result.draws.size() >= limits.maximumDraws)
          return giveUp("O arquivo passa do limite de desenhos desta importação.");
        renderer::MapDrawRecord draw{};
        draw.firstIndex = range.firstIndex;
        draw.indexCount = range.indexCount;
        draw.vertexOffset = range.vertexOffset;
        draw.materialIndex = range.material;
        std::copy(world, world + 16, draw.model);
        // O centro do desenho é o centro LOCAL levado ao mundo: o extrator do
        // editor e o culling usam este par, e um centro em espaço local faria o
        // objeto sumir quando o nó estivesse longe da origem.
        for (u32 axis = 0; axis < 3; ++axis)
          draw.boundsCenter[axis] = world[axis] * range.center[0] + world[4 + axis] * range.center[1] +
                                    world[8 + axis] * range.center[2] + world[12 + axis];
        float scale = 0;
        for (u32 column = 0; column < 3; ++column) {
          float length = 0;
          for (u32 row = 0; row < 3; ++row) length += world[column * 4 + row] * world[column * 4 + row];
          scale = std::fmax(scale, std::sqrt(length));
        }
        draw.boundsRadius = range.radius * scale;
        draw.lodGroupId = static_cast<u32>(result.draws.size());
        result.draws.push_back(draw);
        result.names.push_back(name.empty() ? std::string("Malha ") + std::to_string(result.draws.size())
                                            : std::string(name));
        result.keys.push_back(nodeKey + "/" + range.key);
      }
    }
    if (const auto *children = importer.array(node, "children"))
      for (u32 c = children->childCount; c > 0; --c) {
        const auto *value = document.child(*children, c - 1);
        if (!value || value->kind != Kind::Number) return giveUp("Lista de filhos inválida.");
        const auto index = static_cast<u32>(value->number);
        if (index >= nodes->childCount) return giveUp("Nó filho inexistente.");
        Pending next{index, {}};
        std::copy(world, world + 16, next.world);
        stack.push_back(next);
      }
  }

  if (result.draws.empty())
    return giveUp("O arquivo não tem nenhuma malha de triângulos que esta importação saiba ler.");
  importer.report(1.f, "Concluído");
  out = std::move(result);
  return true;
}

} // namespace ae::resources
