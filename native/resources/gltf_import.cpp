#include "resources/gltf_import.h"
#include "resources/json_reader.h"
#include "resources/gltf_codecs.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <array>

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

struct Accessor {
  u32 component = 0, components = 0, count = 0;
  bool normalized = false;
  const u8 *data = nullptr;
  u32 stride = 0;
  std::vector<u8> storage;
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

  // --- Codecs (Entrega 4) ---------------------------------------------------
  u64 expandedBytes = 0;                                // saída de codecs de geometria nesta importação
  std::unordered_map<u32, std::vector<u8>> decodedViews{}; // bufferViews meshopt já decodificadas

  u64 remainingExpansion() const {
    return limits->maximumExpandedBytes > expandedBytes ? limits->maximumExpandedBytes - expandedBytes : 0;
  }

  // Bytes de uma bufferView. Com EXT_meshopt_compression (ou a versão KHR) a
  // view é decodificada uma vez e guardada: acessores, dados esparsos e imagens
  // leem o resultado sem saber que houve compressão.
  bool viewBytes(const Node &root, i64 viewIndex, std::span<const u8> &bytes, u32 &byteStride, std::string &reason) {
    bytes = {};
    byteStride = 0;
    const auto *views = array(root, "bufferViews");
    if (!views || viewIndex < 0 || viewIndex >= views->childCount) { reason = "bufferView inexistente."; return false; }
    const auto &view = *json->child(*views, static_cast<u32>(viewIndex));
    if (view.kind != Kind::Object) { reason = "bufferView inválida."; return false; }
    byteStride = static_cast<u32>(json->number(view, "byteStride", 0));
    const auto *extensions = json->member(view, "extensions");
    const Node *meshopt = nullptr;
    if (extensions && extensions->kind == Kind::Object) {
      meshopt = json->member(*extensions, "EXT_meshopt_compression");
      if (!meshopt) meshopt = json->member(*extensions, "KHR_meshopt_compression");
    }
    if (meshopt) {
      if (const auto found = decodedViews.find(static_cast<u32>(viewIndex)); found != decodedViews.end()) {
        bytes = found->second;
        return true;
      }
      if (meshopt->kind != Kind::Object || json->index(*meshopt, "buffer") != 0) {
        reason = "Dados meshopt fora do bloco binário do GLB.";
        return false;
      }
      const auto offset = static_cast<u64>(json->number(*meshopt, "byteOffset", 0));
      const auto length = static_cast<u64>(json->number(*meshopt, "byteLength", 0));
      const double count = json->number(*meshopt, "count", 0), stride = json->number(*meshopt, "byteStride", 0);
      if (!length || offset > binary.size() || length > binary.size() - offset) {
        reason = "Dados meshopt com intervalo de bytes inválido.";
        return false;
      }
      if (!(count > 0) || count > 4294967295.0 || count != std::floor(count) || !(stride > 0) || stride > 256 ||
          stride != std::floor(stride)) {
        reason = "Dados meshopt com contagem ou passo inválido.";
        return false;
      }
      std::vector<u8> decoded;
      if (!decodeMeshoptView(binary.subspan(static_cast<usize>(offset), static_cast<usize>(length)), static_cast<u32>(count),
                             static_cast<u32>(stride), json->string(*meshopt, "mode"), json->string(*meshopt, "filter"),
                             remainingExpansion(), decoded, reason))
        return false;
      if (static_cast<u64>(json->number(view, "byteLength", 0)) != decoded.size()) {
        reason = "bufferView meshopt com byteLength diferente do conteúdo decodificado.";
        return false;
      }
      expandedBytes += decoded.size();
      ++out->meshoptViews;
      bytes = decodedViews.emplace(static_cast<u32>(viewIndex), std::move(decoded)).first->second;
      return true;
    }
    if (json->index(view, "buffer") != 0) {
      reason = "Este GLB aponta para um buffer externo; só o bloco binário embutido é lido.";
      return false;
    }
    const auto offset = static_cast<u64>(json->number(view, "byteOffset", 0));
    const auto length = static_cast<u64>(json->number(view, "byteLength", 0));
    if (offset > binary.size() || length > binary.size() - offset) { reason = "bufferView fora do bloco binário."; return false; }
    bytes = binary.subspan(static_cast<usize>(offset), static_cast<usize>(length));
    return true;
  }

  // Imagem de uma textura: `source` quando existe (PNG/JPEG), senão a imagem de
  // KHR_texture_basisu (KTX2).
  i64 textureSource(const Node &texture) const {
    if (texture.kind != Kind::Object) return -1;
    if (const auto source = json->index(texture, "source"); source >= 0) return source;
    const auto *extensions = json->member(texture, "extensions");
    const auto *basisu = extensions && extensions->kind == Kind::Object ? json->member(*extensions, "KHR_texture_basisu") : nullptr;
    return basisu && basisu->kind == Kind::Object ? json->index(*basisu, "source") : -1;
  }

  // Formato de um acessor sem ler dados: com Draco o acessor glTF não tem
  // bufferView e só descreve tipo, componentes e contagem.
  bool accessorFormat(const Node &root, i64 index, Accessor &target, const char *what) {
    const auto *accessors = array(root, "accessors");
    if (!accessors || index < 0 || index >= accessors->childCount) return fail(what);
    const auto &accessor = *json->child(*accessors, static_cast<u32>(index));
    if (accessor.kind != Kind::Object) return fail(what);
    target = {};
    target.component = static_cast<u32>(json->number(accessor, "componentType", 0));
    target.components = componentCount(json->string(accessor, "type"));
    target.count = static_cast<u32>(json->number(accessor, "count", 0));
    target.normalized = json->boolean(accessor, "normalized", false);
    if (!componentSize(target.component) || !target.components || target.components > 4 || !target.count) return fail(what);
    return true;
  }

  bool decodeDraco(const Node &root, const Node &attributes, const Node &compressed, i64 indicesAccessor,
                   std::vector<std::pair<std::string_view, Accessor>> &decoded, Accessor &indices) {
    if (compressed.kind != Kind::Object) return fail("Extensão Draco inválida.");
    const auto *map = json->member(compressed, "attributes");
    if (!map || map->kind != Kind::Object) return fail("Extensão Draco sem mapa de atributos.");
    std::span<const u8> bytes;
    u32 stride = 0;
    std::string reason;
    if (!viewBytes(root, json->index(compressed, "bufferView"), bytes, stride, reason)) {
      out->diagnostic = "Bloco Draco sem bufferView utilizável: " + reason;
      return false;
    }
    static constexpr std::string_view semantics[]{"POSITION", "NORMAL", "TANGENT", "TEXCOORD_0", "TEXCOORD_1", "COLOR_0"};
    std::vector<DracoAttributeRequest> requests;
    decoded.clear();
    for (const auto semantic : semantics) {
      const auto id = json->index(*map, semantic);
      if (id < 0) continue;
      Accessor format;
      if (!accessorFormat(root, json->index(attributes, semantic), format, "Atributo Draco sem acessor válido na primitiva."))
        return false;
      requests.push_back({static_cast<u32>(id), format.component, format.components});
      decoded.emplace_back(semantic, std::move(format));
    }
    if (requests.empty()) return fail("Extensão Draco sem atributos conhecidos.");
    DracoPrimitive primitive;
    std::string diagnostic;
    if (!decodeDracoPrimitive(bytes, requests, remainingExpansion(), primitive, diagnostic)) {
      out->diagnostic = diagnostic;
      return false;
    }
    for (usize i = 0; i < decoded.size(); ++i) {
      auto &target = decoded[i].second;
      if (target.count != primitive.vertexCount) {
        out->diagnostic = "Contagem de vértices do bloco Draco (" + std::to_string(primitive.vertexCount) +
                          ") diferente do acessor " + std::string(decoded[i].first) + " (" + std::to_string(target.count) + ").";
        return false;
      }
      target.storage = std::move(primitive.attributes[i]);
      target.stride = componentSize(target.component) * target.components;
      target.data = target.storage.data();
      expandedBytes += target.storage.size();
    }
    indices = {};
    indices.component = kComponentUnsignedInt;
    indices.components = 1;
    indices.count = static_cast<u32>(primitive.indices.size());
    indices.stride = 4;
    indices.storage.resize(primitive.indices.size() * 4);
    std::memcpy(indices.storage.data(), primitive.indices.data(), indices.storage.size());
    indices.data = indices.storage.data();
    expandedBytes += indices.storage.size();
    if (indicesAccessor >= 0) {
      Accessor declared;
      if (!accessorFormat(root, indicesAccessor, declared, "Índices da primitiva Draco inválidos.")) return false;
      if (declared.count != indices.count) {
        out->diagnostic = "Contagem de índices do bloco Draco (" + std::to_string(indices.count) + ") diferente do acessor (" +
                          std::to_string(declared.count) + ").";
        return false;
      }
    }
    ++out->dracoPrimitives;
    return true;
  }

  bool resolveAccessor(const Node &root, i64 index, Accessor &out, const char *what) {
    const auto *accessors = array(root, "accessors");
    if (!accessors || index < 0 || index >= accessors->childCount) return fail(what);
    const auto &accessor = *json->child(*accessors, static_cast<u32>(index));
    if (accessor.kind != Kind::Object) return fail(what);
    out.component = static_cast<u32>(json->number(accessor, "componentType", 0));
    out.components = componentCount(json->string(accessor, "type"));
    out.count = static_cast<u32>(json->number(accessor, "count", 0));
    out.normalized = json->boolean(accessor, "normalized", false);
    const auto size = componentSize(out.component);
    if (!size || !out.components || !out.count) return fail(what);
    const u64 packedBytes=static_cast<u64>(out.count)*size*out.components;
    if(packedBytes>limits->maximumBytes) return fail("Acessor excede o orçamento de memória da importação.");

    const auto viewIndex = json->index(accessor, "bufferView");
    if(viewIndex<0) {
      // glTF permits an implicit zero-filled base, with or without sparse data.
      out.stride=size*out.components;
      out.storage.resize(static_cast<usize>(packedBytes),0);out.data=out.storage.data();
      return resolveSparse(root,accessor,out,what);
    }
    std::span<const u8> viewData;
    u32 declaredStride = 0;
    std::string reason;
    if (!viewBytes(root, viewIndex, viewData, declaredStride, reason)) {
      this->out->diagnostic = std::string(what) + " " + reason;
      return false;
    }
    const auto accessorOffset = static_cast<u64>(json->number(accessor, "byteOffset", 0));
    out.stride = declaredStride ? declaredStride : size * out.components;
    if (out.stride < size * out.components) return fail(what);
    const u64 span = static_cast<u64>(out.stride) * (out.count - 1) + size * out.components;
    if (accessorOffset > viewData.size() || span > viewData.size() - accessorOffset) return fail(what);
    out.data = viewData.data() + accessorOffset;
    return resolveSparse(root,accessor,out,what);
  }

  bool resolveSparse(const Node &root,const Node &accessor,Accessor &target,const char *what) {
    const auto *sparse=json->member(accessor,"sparse");
    if(!sparse) return true;
    const auto count=json->index(*sparse,"count");
    const auto *indices=json->member(*sparse,"indices"),*values=json->member(*sparse,"values");
    if(count<=0 || count>target.count || !indices || !values) return fail(what);
    const auto component=static_cast<u32>(json->number(*indices,"componentType",0));
    if(component!=kComponentUnsignedByte && component!=kComponentUnsignedShort && component!=kComponentUnsignedInt)
      return fail("Índices esparsos precisam ser inteiros sem sinal.");
    const auto elementSize=componentSize(target.component)*target.components;
    const auto bytes=[&](const Node &source,u64 length,const u8 *&data) {
      std::span<const u8> view;u32 stride=0;std::string reason;
      if(!viewBytes(root,json->index(source,"bufferView"),view,stride,reason) || stride) return false;
      const auto local=json->number(source,"byteOffset",0);
      if(local<0 || local!=std::floor(local) || local>static_cast<double>(view.size()) ||
         length>view.size()-static_cast<u64>(local)) return false;
      data=view.data()+static_cast<usize>(local);return true;
    };
    const u8 *indexData=nullptr,*valueData=nullptr;
    if(!bytes(*indices,static_cast<u64>(count)*componentSize(component),indexData) ||
       !bytes(*values,static_cast<u64>(count)*elementSize,valueData)) return fail("Dados esparsos fora do buffer.");
    if(target.storage.empty()) {
      target.storage.resize(static_cast<usize>(target.count)*elementSize);
      for(u32 i=0;i<target.count;++i)
        std::memcpy(target.storage.data()+static_cast<usize>(i)*elementSize,target.data+static_cast<usize>(i)*target.stride,elementSize);
    }
    u32 previous=0;
    for(u32 i=0;i<static_cast<u32>(count);++i) {
      if((i&0xffff)==0 && cancelled()) return false;
      u32 index=0;std::memcpy(&index,indexData+static_cast<usize>(i)*componentSize(component),componentSize(component));
      if(index>=target.count || (i && index<=previous)) return fail("Índices esparsos fora de ordem ou da faixa.");
      previous=index;
      std::memcpy(target.storage.data()+static_cast<usize>(index)*elementSize,valueData+static_cast<usize>(i)*elementSize,elementSize);
    }
    target.stride=elementSize;target.data=target.storage.data();return true;
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

  // --- Texturas (M09.1) -----------------------------------------------------
  std::unordered_map<u64, u32> textureCache{};        // (textura glTF, sRGB) -> índice de saída
  std::unordered_map<u32, DecodedImage> imageCache{}; // imagem glTF decodificada uma vez
  u32 textureCap = 0;                                 // maior lado residente deste arquivo
  std::unordered_map<u32, u32> imageUses{};           // usos restantes de cada imagem decodificada

  static u64 residentAstcBytes(u32 width, u32 height, u32 cap) {
    while (std::max(width, height) > cap) {
      width = width > 1 ? width / 2 : 1;
      height = height > 1 ? height / 2 : 1;
    }
    u64 total = 0;
    for (;; width = width > 1 ? width / 2 : 1, height = height > 1 ? height / 2 : 1) {
      total += static_cast<u64>((width + 3) / 4) * ((height + 3) / 4) * 16;
      if (width == 1 && height == 1) break;
    }
    return total;
  }

  // KTX2 que vai subir como ASTC: o aparelho amostra ASTC 4x4 e o arquivo traz
  // a cadeia completa de mips (sem mips não há como gerá-los sem codificador).
  bool astcEligible(std::span<const u8> bytes, u32 width, u32 height) const {
    u32 levels = 0;
    return limits->astc4x4 && detectImageContainer(bytes) == ImageContainer::Ktx2 && ktx2LevelCount(bytes, levels) &&
           levels == mipLevelCount(width, height);
  }

  static u64 residentChainBytes(u32 width, u32 height, u32 cap) {
    while (std::max(width, height) > cap) {
      width = width > 1 ? width / 2 : 1;
      height = height > 1 ? height / 2 : 1;
    }
    u64 total = 0;
    for (;; width = width > 1 ? width / 2 : 1, height = height > 1 ? height / 2 : 1) {
      total += static_cast<u64>(width) * height * 4;
      if (width == 1 && height == 1) break;
    }
    return total;
  }

  // Escolhe o limite de resolução do arquivo lendo só cabeçalhos: o mesmo
  // limite para todas as texturas, reduzido até o conjunto caber no orçamento.
  void planTextureResolution(const Node &root) {
    textureCap = std::max<u32>(1, limits->maximumTextureDimension);
    const auto *materials = array(root, "materials");
    const auto *textures = array(root, "textures");
    const auto *images = array(root, "images");
    if (!materials || !textures || !images) return;
    struct Reference { u32 width, height; bool astc; };
    std::unordered_map<u64, Reference> references;
    std::unordered_set<u64> seen;
    const auto consider = [&](const Node *owner, std::string_view name, bool srgb) {
      const auto *info = owner && owner->kind == Kind::Object ? json->member(*owner, name) : nullptr;
      if (!info || info->kind != Kind::Object || json->index(*info, "texCoord") > 1) return;
      const auto index = json->index(*info, "index");
      if (index < 0 || index >= textures->childCount) return;
      const u64 key = static_cast<u64>(index) * 2 + (srgb ? 1 : 0);
      if (!seen.insert(key).second) return;
      const auto &texture = *json->child(*textures, static_cast<u32>(index));
      const auto source = textureSource(texture);
      if (source < 0 || source >= images->childCount) return;
      // Cada (textura, espaço de cor) resolvida é um uso da imagem; no último
      // uso a imagem cheia decodificada é liberada (ver resolveTexture).
      ++imageUses[static_cast<u32>(source)];
      const auto &image = *json->child(*images, static_cast<u32>(source));
      const auto viewIndex = image.kind == Kind::Object ? json->index(image, "bufferView") : -1;
      std::span<const u8> bytes;
      u32 stride = 0;
      std::string reason;
      if (viewIndex < 0 || !viewBytes(root, viewIndex, bytes, stride, reason)) return;
      u32 width = 0, height = 0;
      if (readImageDimensions(bytes, limits->image, width, height))
        references.emplace(key, Reference{width, height, astcEligible(bytes, width, height)});
    };
    for (u32 i = 0; i < materials->childCount; ++i) {
      const auto &material = *json->child(*materials, i);
      if (material.kind != Kind::Object) continue;
      const auto *pbr = json->member(material, "pbrMetallicRoughness");
      consider(pbr, "baseColorTexture", true);
      consider(pbr, "metallicRoughnessTexture", false);
      consider(&material, "normalTexture", false);
      consider(&material, "emissiveTexture", true);
    }
    if (references.empty()) return;
    const u32 floor = std::min(textureCap, std::max<u32>(1, limits->minimumTextureDimension));
    for (;;) {
      u64 total = 0;
      for (const auto &[key, reference] : references)
        total += reference.astc ? residentAstcBytes(reference.width, reference.height, textureCap)
                                : residentChainBytes(reference.width, reference.height, textureCap);
      if (total <= limits->maximumTextureBytes || textureCap / 2 < floor) break;
      textureCap /= 2;
    }
    out->residentTextureDimension = textureCap;
    if (textureCap < limits->maximumTextureDimension)
      noteTexture("Resolução das texturas limitada a " + std::to_string(textureCap) +
                  " px para o arquivo caber no orçamento de memória da importação.");
  }

  void noteTexture(std::string text) {
    if (out->textureNotes.size() >= 16) return;
    for (const auto &existing : out->textureNotes) if (existing == text) return;
    out->textureNotes.push_back(std::move(text));
  }

  // Bytes de uma imagem embutida no bloco binário. URI externa ou data URI não
  // é lida neste caminho: o seletor entrega um arquivo, não a pasta.
  bool imageBytes(const Node &root, i64 imageIndex, std::span<const u8> &bytes) {
    const auto *images = array(root, "images");
    if (!images || imageIndex < 0 || imageIndex >= images->childCount) { noteTexture("Textura aponta para imagem inexistente."); return false; }
    const auto &image = *json->child(*images, static_cast<u32>(imageIndex));
    if (image.kind != Kind::Object) { noteTexture("Imagem inválida."); return false; }
    const auto viewIndex = json->index(image, "bufferView");
    if (viewIndex < 0) { noteTexture("Imagem externa ou em data URI não é lida neste perfil."); return false; }
    u32 stride = 0;
    std::string reason;
    if (!viewBytes(root, viewIndex, bytes, stride, reason) || bytes.empty()) {
      noteTexture(reason.empty() ? std::string("Imagem com intervalo de bytes inválido.") : "Imagem: " + reason);
      return false;
    }
    return true;
  }

  u32 samplerFlags(const Node &root, i64 samplerIndex) {
    u32 flags = renderer::AuthoringTextureLinearFilter | renderer::AuthoringTextureLinearMip |
                renderer::AuthoringTextureRepeatU | renderer::AuthoringTextureRepeatV;
    const auto *samplers = array(root, "samplers");
    if (!samplers || samplerIndex < 0 || samplerIndex >= samplers->childCount) return flags;
    const auto &sampler = *json->child(*samplers, static_cast<u32>(samplerIndex));
    if (sampler.kind != Kind::Object) return flags;
    const auto mag = static_cast<i64>(json->number(sampler, "magFilter", 9729));
    const auto min = static_cast<i64>(json->number(sampler, "minFilter", 9987));
    if (mag == 9728) flags &= ~renderer::AuthoringTextureLinearFilter;
    // NEAREST_MIPMAP_NEAREST (9984) e LINEAR_MIPMAP_NEAREST (9985) escolhem o mip
    // mais próximo; sem mipmap declarado (9728/9729) também.
    if (min == 9984 || min == 9985 || min == 9728 || min == 9729) flags &= ~renderer::AuthoringTextureLinearMip;
    const auto wrap = [&](const char *field, u32 repeat, u32 mirror) {
      const auto mode = static_cast<i64>(json->number(sampler, field, 10497));
      if (mode == 33071) flags &= ~repeat;                       // CLAMP_TO_EDGE
      else if (mode == 33648) flags = (flags & ~repeat) | mirror; // MIRRORED_REPEAT
    };
    wrap("wrapS", renderer::AuthoringTextureRepeatU, renderer::AuthoringTextureMirrorU);
    wrap("wrapT", renderer::AuthoringTextureRepeatV, renderer::AuthoringTextureMirrorV);
    return flags;
  }

  // KTX2 com cadeia completa de mips num aparelho que amostra ASTC 4x4:
  // transcodificado direto para blocos, sem RGBA intermediário nem mips em CPU.
  // Devolve verdadeiro quando decidiu a textura (aplicada, ou recusada pelo
  // orçamento); falso manda para o caminho RGBA8.
  std::unordered_set<i64> astcImages{}; // imagens KTX2 já contadas no caminho ASTC

  bool resolveAstc(const Node &root, const Node &texture, i64 source, std::span<const u8> bytes, bool srgb, u32 &result) {
    if (!limits->astc4x4 || detectImageContainer(bytes) != ImageContainer::Ktx2) return false;
    u32 width = 0, height = 0;
    if (!readImageDimensions(bytes, limits->image, width, height)) return false;
    if (!astcEligible(bytes, width, height)) {
      noteTexture("KTX2 sem cadeia completa de mips: transcodificado para RGBA8, com mips gerados na importação.");
      return false;
    }
    const u32 levels = mipLevelCount(width, height), cap = std::max<u32>(1, textureCap);
    u32 dropped = 0;
    for (u32 w = width, h = height; std::max(w, h) > cap && dropped + 1 < levels;
         w = w > 1 ? w / 2 : 1, h = h > 1 ? h / 2 : 1)
      ++dropped;
    u64 needed = 0;
    for (u32 level = 0, w = width, h = height; level < levels; ++level, w = w > 1 ? w / 2 : 1, h = h > 1 ? h / 2 : 1)
      if (level >= dropped) needed += static_cast<u64>((w + 3) / 4) * ((h + 3) / 4) * 16;
    if (out->textureBytes + needed > limits->maximumTextureBytes) {
      noteTexture("Orçamento de memória de texturas da importação esgotado; texturas restantes ficaram de fora.");
      return true;
    }
    renderer::AuthoringTexture made;
    made.format = renderer::AuthoringTextureAstc4x4;
    made.srgb = srgb;
    made.samplerFlags = samplerFlags(root, texture.kind == Kind::Object ? json->index(texture, "sampler") : -1);
    std::string diagnostic;
    if (!transcodeKtx2Astc4x4(bytes, dropped, made.mipChain, made.width, made.height, made.levels, diagnostic) ||
        !made.valid()) {
      noteTexture((diagnostic.empty() ? std::string("Falha ao transcodificar KTX2 para ASTC.") : diagnostic) +
                  " Usado RGBA8.");
      return false;
    }
    if (dropped) ++out->reducedTextures;
    ++out->astcTextures;
    // Por IMAGEM, como no caminho RGBA8: a mesma imagem em cor e em dados é uma só.
    if (astcImages.insert(source).second) ++out->ktx2Images;
    out->textureBytes += made.mipChain.size();
    result = static_cast<u32>(out->textures.size());
    out->textures.push_back(std::make_shared<renderer::AuthoringTexture>(std::move(made)));
    return true;
  }

  // Resolve uma textura glTF para o índice de saída, decodificando e gerando
  // mips na primeira vez. Inválido quando não foi possível aplicar — e o motivo
  // vai para `textureNotes`.
  u32 resolveTexture(const Node &root, i64 textureIndex, bool srgb) {
    const auto *textures = array(root, "textures");
    if (!textures || textureIndex < 0 || textureIndex >= textures->childCount) {
      noteTexture("Material aponta para textura inexistente.");
      return renderer::InvalidMapTexture;
    }
    const u64 key = static_cast<u64>(textureIndex) * 2 + (srgb ? 1 : 0);
    if (const auto found = textureCache.find(key); found != textureCache.end()) return found->second;
    u32 result = renderer::InvalidMapTexture;
    const auto &texture = *json->child(*textures, static_cast<u32>(textureIndex));
    const auto source = textureSource(texture);
    std::span<const u8> bytes;
    if (source < 0) {
      noteTexture("Textura só em extensão sem decodificador neste perfil (WebP ou AVIF).");
    } else if (imageBytes(root, source, bytes) && !resolveAstc(root, texture, source, bytes, srgb, result)) {
      auto decoded = imageCache.find(static_cast<u32>(source));
      if (decoded == imageCache.end()) {
        DecodedImage image;
        std::string diagnostic;
        if (!decodeImageRgba8(bytes, limits->image, image, diagnostic)) {
          noteTexture(diagnostic);
          image = {};
        } else if (detectImageContainer(bytes) == ImageContainer::Ktx2) {
          ++out->ktx2Images;
        }
        decoded = imageCache.emplace(static_cast<u32>(source), std::move(image)).first;
      }
      if (!decoded->second.rgba.empty()) {
        renderer::AuthoringTexture made;
        made.srgb = srgb;
        made.samplerFlags = samplerFlags(root, texture.kind == Kind::Object ? json->index(texture, "sampler") : -1);
        // Nível residente: descarta os mips de cima até o maior lado caber no
        // limite. Calculado antes de qualquer alocação da cadeia.
        const u32 cap = std::max<u32>(1, textureCap);
        u32 dropped = 0, width = decoded->second.width, height = decoded->second.height;
        while (std::max(width, height) > cap) {
          width = width > 1 ? width / 2 : 1;
          height = height > 1 ? height / 2 : 1;
          ++dropped;
        }
        made.width = width;
        made.height = height;
        const u64 needed = [&] {
          u64 total = 0;
          for (u32 w = width, h = height;; w = w > 1 ? w / 2 : 1, h = h > 1 ? h / 2 : 1) {
            total += static_cast<u64>(w) * h * 4;
            if (w == 1 && h == 1) break;
          }
          return total;
        }();
        // Orçamento conferido ANTES de gerar os mips, já com a resolução residente.
        std::vector<u8> chain;
        u32 levels = 0;
        if (out->textureBytes + needed > limits->maximumTextureBytes) {
          noteTexture("Orçamento de memória de texturas da importação esgotado; texturas restantes ficaram de fora.");
        } else if (buildMipChain(decoded->second, srgb, chain, levels) && levels > dropped) {
          usize skip = 0;
          for (u32 level = 0, w = decoded->second.width, h = decoded->second.height; level < dropped;
               ++level, w = w > 1 ? w / 2 : 1, h = h > 1 ? h / 2 : 1)
            skip += static_cast<usize>(w) * h * 4;
          made.levels = levels - dropped;
          made.mipChain.assign(chain.begin() + static_cast<std::ptrdiff_t>(skip), chain.end());
          if (made.valid()) {
            if (dropped) ++out->reducedTextures;
            out->textureBytes += made.mipChain.size();
            result = static_cast<u32>(out->textures.size());
            out->textures.push_back(std::make_shared<renderer::AuthoringTexture>(std::move(made)));
          } else {
            noteTexture("Não foi possível gerar os mipmaps de uma textura.");
          }
        } else {
          noteTexture("Não foi possível gerar os mipmaps de uma textura.");
        }
      }
      // Pico de memória: sem isto todas as imagens cheias do arquivo ficariam
      // vivas até o fim da importação (o Porsche real fez o sistema encerrar
      // outros apps). A cadeia de mips reduzida já guarda o que interessa.
      if (const auto use = imageUses.find(static_cast<u32>(source)); use != imageUses.end() && use->second &&
                                                                      --use->second == 0)
        imageCache.erase(static_cast<u32>(source));
    }
    textureCache.emplace(key, result);
    return result;
  }

  // KHR_texture_transform assado nas UVs (Entrega 4). Por material e por conjunto
  // de UV guarda-se a matriz 2x3 comum a todas as texturas APLICADAS naquele
  // conjunto; se duas discordam, nenhuma é assada e as transformadas são contadas
  // como não aplicadas. Linhas: u' = m0·u + m1·v + m2, v' = m3·u + m4·v + m5.
  struct UvTransform {
    float m[6]{1, 0, 0, 0, 1, 0};
    bool used = false, conflict = false;
    u32 transformed = 0;
  };
  std::vector<std::array<UvTransform, 2>> uvTransforms{};

  // Liga uma `textureInfo` do material a um slot. Devolve se a textura foi
  // aplicada; toda referência não aplicada é contada em `skippedTextures`.
  bool assignTexture(const Node &root, const Node &owner, std::string_view name, bool srgb, u32 slot,
                     renderer::MapMaterialRecord &target, u32 feature, u32 material) {
    const auto *info = json->member(owner, name);
    if (!info || info->kind != Kind::Object) return false;
    const Node *transform = nullptr;
    if (const auto *extensions = json->member(*info, "extensions"); extensions && extensions->kind == Kind::Object)
      if (const auto *value = json->member(*extensions, "KHR_texture_transform"); value && value->kind == Kind::Object)
        transform = value;
    auto coordinate = json->index(*info, "texCoord");
    // A extensão pode trocar o conjunto de UV da textura.
    if (transform && json->index(*transform, "texCoord") >= 0) coordinate = json->index(*transform, "texCoord");
    if (coordinate > 1) {
      noteTexture("Textura usa TEXCOORD_" + std::to_string(coordinate) + "; só TEXCOORD_0 e TEXCOORD_1 são lidos.");
      ++out->skippedTextures;
      return false;
    }
    const auto index = resolveTexture(root, json->index(*info, "index"), srgb);
    if (index == renderer::InvalidMapTexture) {
      ++out->skippedTextures;
      return false;
    }
    target.textureIndices[slot] = index;
    target.flags |= feature;
    if (coordinate == 1) target.textureCoordinates |= 1u << (slot * 2);
    // Matriz do glTF (GLSL, colunas): translação · rotação anti-horária · escala.
    float m[6]{1, 0, 0, 0, 1, 0};
    if (transform) {
      float offset[2]{0, 0}, scale[2]{1, 1};
      const auto pair = [&](std::string_view field, float *values) {
        const auto *list = json->member(*transform, field);
        if (!list) return true;
        if (list->kind != Kind::Array || list->childCount != 2) return false;
        for (u32 k = 0; k < 2; ++k) {
          const auto *value = json->child(*list, k);
          if (!value || value->kind != Kind::Number) return false;
          values[k] = static_cast<float>(value->number);
        }
        return true;
      };
      const float rotation = static_cast<float>(json->number(*transform, "rotation", 0));
      bool valid = pair("offset", offset) && pair("scale", scale);
      const float c = std::cos(rotation), s = std::sin(rotation);
      const float candidate[6]{c * scale[0], -s * scale[1], offset[0], s * scale[0], c * scale[1], offset[1]};
      for (const float value : candidate) valid &= std::isfinite(value);
      if (valid) std::copy(candidate, candidate + 6, m);
      else ++out->unappliedTextureTransforms; // malformada: a textura entra sem ela
    }
    if (material < uvTransforms.size()) {
      auto &entry = uvTransforms[material][coordinate == 1 ? 1 : 0];
      if (!entry.used) {
        std::copy(m, m + 6, entry.m);
        entry.used = true;
      } else {
        for (u32 k = 0; k < 6; ++k) entry.conflict |= std::fabs(entry.m[k] - m[k]) > 1e-6f;
      }
      const float neutral[6]{1, 0, 0, 0, 1, 0};
      bool identity = true;
      for (u32 k = 0; k < 6; ++k) identity &= std::fabs(m[k] - neutral[k]) <= 1e-6f;
      if (!identity) ++entry.transformed;
    }
    return true;
  }

  bool readMaterials(const Node &root) {
    const auto *materials = array(root, "materials");
    const u32 count = materials ? materials->childCount : 0;
    planTextureResolution(root);
    // Um material neutro fecha a lista: primitiva sem material é legal em glTF
    // e precisa de alguma coisa para apontar.
    out->materials.resize(count + 1);
    out->materialNames.assign(count + 1, std::string());
    uvTransforms.assign(count + 1, {});
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
      // Decodificar texturas é a parte lenta desta etapa: o cancelamento precisa
      // valer entre um material e outro.
      if (cancelled()) return fail("Importação cancelada.");
      const auto &source = *json->child(*materials, i);
      if (source.kind != Kind::Object) return fail("Material inválido.");
      auto &target = out->materials[i];
      if (const auto name = json->string(source, "name"); name.size() <= 128) out->materialNames[i] = std::string(name);
      if (const auto *pbr = json->member(source, "pbrMetallicRoughness"); pbr && pbr->kind == Kind::Object) {
        if (!readVector(*pbr, "baseColorFactor", target.baseColorFactor, 4)) return false;
        target.roughness = static_cast<float>(json->number(*pbr, "roughnessFactor", 1));
        target.metallic = static_cast<float>(json->number(*pbr, "metallicFactor", 1));
        // Cor base em sRGB; metálico/rugosidade é dado linear (canais G e B).
        assignTexture(root, *pbr, "baseColorTexture", true, 0, target, 0, i);
        assignTexture(root, *pbr, "metallicRoughnessTexture", false, 2, target, renderer::MapMaterialMetallicRoughnessMap, i);
      }
      float emissive[3]{0, 0, 0};
      if (!readVector(source, "emissiveFactor", emissive, 3)) return false;
      std::copy(emissive, emissive + 3, target.emissiveFactorAndStrength);
      target.emissiveFactorAndStrength[3] = 1;
      if (assignTexture(root, source, "normalTexture", false, 1, target, renderer::MapMaterialNormalMap, i))
        if (const auto *normal = json->member(source, "normalTexture"); normal && normal->kind == Kind::Object)
          target.normalScale = static_cast<float>(json->number(*normal, "scale", 1));
      assignTexture(root, source, "emissiveTexture", true, 3, target, renderer::MapMaterialEmissiveMap, i);
      // Oclusão (R4): o quadro de material não tem um quinto slot de textura.
      // Entra só quando é a MESMA textura e o MESMO UV do metálico/rugosidade
      // (ORM, canal R), sem transformação e com força 1 -- o caso em que ler o
      // canal R é exatamente o que o arquivo pede. Qualquer outro caso é
      // declarado, nunca aproximado em silêncio.
      if (const auto *occlusion = json->member(source, "occlusionTexture")) {
        const auto *pbr = json->member(source, "pbrMetallicRoughness");
        const auto *packed = pbr && pbr->kind == Kind::Object ? json->member(*pbr, "metallicRoughnessTexture") : nullptr;
        const auto transformed = [&](const Node *info) {
          const auto *extensions = info && info->kind == Kind::Object ? json->member(*info, "extensions") : nullptr;
          return extensions && extensions->kind == Kind::Object && json->member(*extensions, "KHR_texture_transform");
        };
        const bool applied = occlusion->kind == Kind::Object && packed && packed->kind == Kind::Object &&
                             (target.flags & renderer::MapMaterialMetallicRoughnessMap) != 0 &&
                             json->index(*occlusion, "index") >= 0 &&
                             json->index(*occlusion, "index") == json->index(*packed, "index") &&
                             std::max<i64>(0, json->index(*occlusion, "texCoord")) == std::max<i64>(0, json->index(*packed, "texCoord")) &&
                             !transformed(occlusion) && !transformed(packed) &&
                             std::fabs(json->number(*occlusion, "strength", 1) - 1) < 1e-6;
        if (applied) {
          target.flags |= renderer::MapMaterialOcclusionInMetallicRoughness;
          ++out->appliedOcclusion;
        } else {
          ++out->unappliedOcclusion;
        }
      }
      const auto alphaMode = json->string(source, "alphaMode");
      if (alphaMode == "BLEND") target.flags |= renderer::MapMaterialBlend;
      else if (alphaMode == "MASK") {
        target.flags |= renderer::MapMaterialAlphaMask;
        target.alphaCutoff = static_cast<float>(json->number(source, "alphaCutoff", .5));
      }
      if (json->boolean(source, "doubleSided", false)) target.flags |= renderer::MapMaterialDoubleSided;
    }
    for (const auto &sets : uvTransforms)
      for (const auto &entry : sets) {
        if (entry.conflict) out->unappliedTextureTransforms += entry.transformed;
        else out->bakedTextureTransforms += entry.transformed;
      }
    return true;
  }

  // Uma primitiva vira um bloco de vértices e um bloco de índices no mesmo par
  // de buffers que o pacote de mapa usa. Instâncias reaproveitam o bloco: o
  // mesmo mesh em dois nós custa dois desenhos, não duas cópias da geometria.
  struct PrimitiveRange { u32 firstIndex = 0, indexCount = 0, vertexOffset = 0, material = 0;
                          float center[3]{}; float radius = 0; std::string key; };

  // Cópia refletida por S = diag(-1,1,1) de uma primitiva já lida, para um nó
  // espelhado. Posição, normal e tangente têm X negado; o sinal da bitangente
  // (w) inverte, porque (S·N)×(S·T) = -S·(N×T); e cada triângulo troca dois
  // vértices, o que o glTF manda fazer quando o determinante é negativo.
  bool mirrorRange(const PrimitiveRange &source, PrimitiveRange &mirrored) {
    u32 vertexCount = 0;
    for (u32 i = source.firstIndex; i < source.firstIndex + source.indexCount; ++i)
      vertexCount = std::max(vertexCount, out->indices[i] + 1);
    const u64 vertexBase = out->vertices.size() / renderer::MapVertexStride;
    if (vertexBase + vertexCount > limits->maximumVertices) return fail("O arquivo passa do limite de vértices desta importação.");
    if (out->indices.size() + source.indexCount > limits->maximumIndices)
      return fail("O arquivo passa do limite de índices desta importação.");
    mirrored = source;
    mirrored.vertexOffset = static_cast<u32>(vertexBase);
    mirrored.firstIndex = static_cast<u32>(out->indices.size());
    mirrored.center[0] = -source.center[0];
    mirrored.key = source.key + "!espelho";
    out->vertices.resize(out->vertices.size() + static_cast<usize>(vertexCount) * renderer::MapVertexStride);
    for (u32 v = 0; v < vertexCount; ++v) {
      u8 *target = out->vertices.data() + (static_cast<usize>(vertexBase) + v) * renderer::MapVertexStride;
      std::memcpy(target, out->vertices.data() + (static_cast<usize>(source.vertexOffset) + v) * renderer::MapVertexStride,
                  renderer::MapVertexStride);
      float x = 0;
      std::memcpy(&x, target, 4);
      x = -x;
      std::memcpy(target, &x, 4);
      for (const usize offset : {usize{12}, usize{20}, usize{26}}) { // normal.x, tangente.x, tangente.w
        i16 value = 0;
        std::memcpy(&value, target + offset, 2);
        value = static_cast<i16>(value == -32768 ? 32767 : -value);
        std::memcpy(target + offset, &value, 2);
      }
    }
    for (u32 i = source.firstIndex; i + 2 < source.firstIndex + source.indexCount; i += 3) {
      const u32 a = out->indices[i], b = out->indices[i + 1], c = out->indices[i + 2];
      out->indices.insert(out->indices.end(), {a, c, b});
    }
    return true;
  }

  bool readPrimitive(const Node &root, const Node &primitive, PrimitiveRange &range) {
    const auto *primitiveExtensions = json->member(primitive, "extensions");
    const auto *dracoExtension = primitiveExtensions && primitiveExtensions->kind == Kind::Object
                                     ? json->member(*primitiveExtensions, "KHR_draco_mesh_compression") : nullptr;
    const bool draco = dracoExtension != nullptr;
    // O bloco Draco devolve lista de triângulos, qualquer que seja o modo declarado.
    const auto mode = draco ? 4u : static_cast<u32>(json->number(primitive, "mode", 4));
    if(mode!=4 && mode!=5 && mode!=6) { ++out->skippedPrimitives; return true; }
    const auto *attributes = json->member(primitive, "attributes");
    if (!attributes || attributes->kind != Kind::Object) return fail("Primitiva sem atributos.");
    // KHR_draco_mesh_compression: atributos e índices saem do bloco comprimido;
    // o acessor glTF continua mandando no tipo, nos componentes e na contagem.
    std::vector<std::pair<std::string_view, Accessor>> dracoAttributes;
    Accessor dracoIndices{};
    if (draco && !decodeDraco(root, *attributes, *dracoExtension, json->index(primitive, "indices"), dracoAttributes, dracoIndices))
      return false;
    const auto resolveAttribute = [&](std::string_view semantic, Accessor &target, const char *what) {
      for (auto &entry : dracoAttributes)
        if (entry.first == semantic) {
          target = std::move(entry.second);
          target.data = target.storage.data();
          return true;
        }
      return resolveAccessor(root, json->index(*attributes, semantic), target, what);
    };
    const auto declared = [&](std::string_view semantic) {
      if (json->index(*attributes, semantic) >= 0) return true;
      for (const auto &entry : dracoAttributes)
        if (entry.first == semantic) return true;
      return false;
    };
    Accessor position{};
    if (!resolveAttribute("POSITION", position, "Primitiva sem POSITION utilizável.")) return false;
    if (position.components != 3) return fail("POSITION precisa ser VEC3.");

    Accessor normal{}, tangent{}, uv0{}, uv1{}, color{};
    const bool hasNormal = declared("NORMAL") && resolveAttribute("NORMAL", normal, "NORMAL inválido.");
    if (declared("NORMAL") && !hasNormal) return false;
    const bool hasTangent = declared("TANGENT") && resolveAttribute("TANGENT", tangent, "TANGENT inválido.");
    if (declared("TANGENT") && !hasTangent) return false;
    const bool hasUv0 = declared("TEXCOORD_0") && resolveAttribute("TEXCOORD_0", uv0, "TEXCOORD_0 inválido.");
    if (declared("TEXCOORD_0") && !hasUv0) return false;
    const bool hasUv1 = declared("TEXCOORD_1") && resolveAttribute("TEXCOORD_1", uv1, "TEXCOORD_1 inválido.");
    if (declared("TEXCOORD_1") && !hasUv1) return false;
    const bool hasColor = declared("COLOR_0") && resolveAttribute("COLOR_0", color, "COLOR_0 inválido.");
    if (declared("COLOR_0") && !hasColor) return false;
    for(const auto *attribute:{&normal,&tangent,&uv0,&uv1,&color})
      if(attribute->data && attribute->count!=position.count) return fail("Atributos da malha têm contagens de vértices diferentes.");
    if((hasNormal && normal.components!=3) || (hasTangent && tangent.components!=4) ||
       (hasUv0 && uv0.components!=2) || (hasUv1 && uv1.components!=2) ||
       (hasColor && color.components!=3 && color.components!=4)) return fail("Formato de atributo de vértice inválido.");

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
    std::vector<u32> topology;
    if (indicesIndex >= 0 || draco) {
      Accessor indices{};
      if (draco) {
        indices = std::move(dracoIndices);
        indices.data = indices.storage.data();
      } else if (!resolveAccessor(root, indicesIndex, indices, "Índices inválidos.")) {
        return false;
      }
      if (indices.components != 1) return fail("Índices precisam ser SCALAR.");
      if(indices.normalized || (indices.component!=kComponentUnsignedByte && indices.component!=kComponentUnsignedShort &&
         indices.component!=kComponentUnsignedInt)) return fail("Índices precisam ser inteiros sem sinal, não normalizados.");
      if (mode==4 && indices.count % 3) return fail("A primitiva não tem um número de índices múltiplo de três.");
      if (out->indices.size() + indices.count > limits->maximumIndices)
        return fail("O arquivo passa do limite de índices desta importação.");
      topology.reserve(indices.count);
      for (u32 i = 0; i < indices.count; ++i) {
        if ((i & 0xFFFF) == 0 && cancelled()) return false;
        u32 value=0;std::memcpy(&value,indices.data+static_cast<usize>(i)*indices.stride,componentSize(indices.component));
        if (value >= position.count) return fail("Índice fora da faixa de vértices da primitiva.");
        topology.push_back(value);
      }
      range.indexCount = indices.count;
    } else {
      if (mode==4 && position.count % 3) return fail("Primitiva sem índices precisa de vértices múltiplos de três.");
      if (out->indices.size() + position.count > limits->maximumIndices)
        return fail("O arquivo passa do limite de índices desta importação.");
      topology.reserve(position.count);
      for (u32 i = 0; i < position.count; ++i) topology.push_back(i);
      range.indexCount = position.count;
    }
    if(mode==4) out->indices.insert(out->indices.end(),topology.begin(),topology.end());
    else {
      const u64 count=topology.size()<3?0:(topology.size()-2)*3ull;
      if(out->indices.size()+count>limits->maximumIndices) return fail("Triangulação excede o limite de índices.");
      for(usize i=2;i<topology.size();++i) {
        if((i&0xffff)==0 && cancelled()) return false;
        const u32 a=mode==6?topology[0]:topology[i-2+(i%2)];
        const u32 b=mode==6?topology[i-1]:topology[i-1-(i%2)],c=topology[i];
        if(a==b || b==c || a==c) continue;
        out->indices.insert(out->indices.end(),{a,b,c});
      }
    }
    range.indexCount=static_cast<u32>(out->indices.size())-range.firstIndex;
    if(!hasNormal) {
      std::vector<std::array<float,3>> normals(position.count);
      for(u32 i=range.firstIndex;i<range.firstIndex+range.indexCount;i+=3) {
        if(((i-range.firstIndex)&0xffff)==0 && cancelled()) return false;
        const u32 a=out->indices[i],b=out->indices[i+1],c=out->indices[i+2];
        float p[3],q[3],r[3];
        std::memcpy(p,write+static_cast<usize>(a)*renderer::MapVertexStride,12);
        std::memcpy(q,write+static_cast<usize>(b)*renderer::MapVertexStride,12);
        std::memcpy(r,write+static_cast<usize>(c)*renderer::MapVertexStride,12);
        for(u32 axis=0;axis<3;++axis) {q[axis]-=p[axis];r[axis]-=p[axis];}
        const float face[3]{q[1]*r[2]-q[2]*r[1],q[2]*r[0]-q[0]*r[2],q[0]*r[1]-q[1]*r[0]};
        for(const auto vertex:{a,b,c}) for(u32 axis=0;axis<3;++axis) normals[vertex][axis]+=face[axis];
      }
      for(u32 i=0;i<position.count;++i) {
        if((i&0xffff)==0 && cancelled()) return false;
        const auto &n=normals[i];const float length=std::hypot(n[0],n[1],n[2]);
        i16 packed[4]{0,0,32767,0};
        if(length>0 && std::isfinite(length)) for(u32 axis=0;axis<3;++axis) packed[axis]=packSnorm(n[axis]/length);
        std::memcpy(write+static_cast<usize>(i)*renderer::MapVertexStride+12,packed,8);
      }
    }
    const auto material = json->index(primitive, "material");
    range.material = material >= 0 && static_cast<usize>(material) + 1 < out->materials.size()
                         ? static_cast<u32>(material)
                         : static_cast<u32>(out->materials.size() - 1);
    // KHR_texture_transform: quando todas as texturas do material num conjunto de
    // UV concordam, a transformação é assada nas UVs desta primitiva — antes das
    // tangentes geradas, que precisam seguir as UVs que o shader amostra.
    if (range.material < uvTransforms.size())
      for (u32 set = 0; set < 2; ++set) {
        const auto &entry = uvTransforms[range.material][set];
        if (!entry.transformed || entry.conflict) continue;
        for (u32 v = 0; v < position.count; ++v) {
          u8 *vertex = write + static_cast<usize>(v) * renderer::MapVertexStride;
          float uv[2];
          std::memcpy(uv, vertex + 28 + set * 8, 8);
          const float u = entry.m[0] * uv[0] + entry.m[1] * uv[1] + entry.m[2];
          const float w = entry.m[3] * uv[0] + entry.m[4] * uv[1] + entry.m[5];
          uv[0] = u;
          uv[1] = w;
          std::memcpy(vertex + 28 + set * 8, uv, 8);
        }
      }
    // glTF: sem TANGENT, quem importa gera as tangentes quando o material tem
    // mapa normal. Tangente zero vira NaN na base TBN e a superfície fica preta
    // (visto no Porsche real: 46 primitivas com mapa normal e sem TANGENT).
    // Acúmulo por triângulo a partir das derivadas de UV, ortogonalizado contra
    // a normal; não é MikkTSpace bit a bit.
    const auto &owner = out->materials[range.material];
    if (!hasTangent && (owner.flags & renderer::MapMaterialNormalMap)) {
      const u32 set = (owner.textureCoordinates >> 2) & 1u; // conjunto de UV do slot 1 (normal)
      std::vector<std::array<float, 6>> accumulated(position.count, std::array<float, 6>{});
      for (u32 i = range.firstIndex; i < range.firstIndex + range.indexCount; i += 3) {
        if (((i - range.firstIndex) & 0xffff) == 0 && cancelled()) return false;
        const u32 corner[3]{out->indices[i], out->indices[i + 1], out->indices[i + 2]};
        float p[3][3], uv[3][2];
        for (u32 k = 0; k < 3; ++k) {
          const u8 *vertex = write + static_cast<usize>(corner[k]) * renderer::MapVertexStride;
          std::memcpy(p[k], vertex, 12);
          std::memcpy(uv[k], vertex + 28 + set * 8, 8);
        }
        const float e1[3]{p[1][0] - p[0][0], p[1][1] - p[0][1], p[1][2] - p[0][2]};
        const float e2[3]{p[2][0] - p[0][0], p[2][1] - p[0][1], p[2][2] - p[0][2]};
        const float du1 = uv[1][0] - uv[0][0], dv1 = uv[1][1] - uv[0][1];
        const float du2 = uv[2][0] - uv[0][0], dv2 = uv[2][1] - uv[0][1];
        const float determinant = du1 * dv2 - du2 * dv1;
        if (!std::isfinite(determinant) || std::fabs(determinant) < 1e-20f) continue;
        const float r = 1.0f / determinant;
        for (u32 k = 0; k < 3; ++k)
          for (u32 axis = 0; axis < 3; ++axis) {
            accumulated[corner[k]][axis] += (e1[axis] * dv2 - e2[axis] * dv1) * r;
            accumulated[corner[k]][3 + axis] += (e2[axis] * du1 - e1[axis] * du2) * r;
          }
      }
      for (u32 v = 0; v < position.count; ++v) {
        if ((v & 0xffff) == 0 && cancelled()) return false;
        u8 *vertex = write + static_cast<usize>(v) * renderer::MapVertexStride;
        i16 packedNormal[4]{};
        std::memcpy(packedNormal, vertex + 12, 8);
        float n[3]{packedNormal[0] / 32767.0f, packedNormal[1] / 32767.0f, packedNormal[2] / 32767.0f};
        const float normalLength = std::hypot(n[0], n[1], n[2]);
        if (normalLength > 0) for (auto &axis : n) axis /= normalLength;
        const auto &a = accumulated[v];
        const float along = n[0] * a[0] + n[1] * a[1] + n[2] * a[2];
        float t[3]{a[0] - n[0] * along, a[1] - n[1] * along, a[2] - n[2] * along};
        float length = std::hypot(t[0], t[1], t[2]);
        if (!std::isfinite(length) || length <= 1e-8f) {
          // UV degenerado: qualquer perpendicular à normal mantém a base válida.
          const float axis[3]{std::fabs(n[0]) < .9f ? 1.0f : 0.0f, std::fabs(n[0]) < .9f ? 0.0f : 1.0f, 0};
          t[0] = n[1] * axis[2] - n[2] * axis[1];
          t[1] = n[2] * axis[0] - n[0] * axis[2];
          t[2] = n[0] * axis[1] - n[1] * axis[0];
          length = std::hypot(t[0], t[1], t[2]);
        }
        i16 packedTangent[4]{0, 0, 0, 32767};
        if (length > 0 && std::isfinite(length)) {
          for (u32 axis = 0; axis < 3; ++axis) packedTangent[axis] = packSnorm(t[axis] / length);
          const float cross[3]{n[1] * t[2] - n[2] * t[1], n[2] * t[0] - n[0] * t[2], n[0] * t[1] - n[1] * t[0]};
          if (cross[0] * a[3] + cross[1] * a[4] + cross[2] * a[5] < 0) packedTangent[3] = -32767;
        }
        std::memcpy(vertex + 20, packedTangent, 8);
      }
      ++out->generatedTangentPrimitives;
    }
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

  // Quantized attributes use the same typed accessor decoder. Known appearance
  // extensions can enter the declared static geometry profile, with explicit
  // losses in the review dialog. Never ignore unknown geometry/codecs.
  const auto appearance=[](std::string_view name) {
    return name=="KHR_texture_transform" || name=="EXT_texture_webp" ||
      name=="EXT_texture_avif" || name=="KHR_materials_clearcoat" ||
      name=="KHR_materials_transmission" || name=="KHR_materials_volume" || name=="KHR_materials_ior" ||
      name=="KHR_materials_specular" || name=="KHR_materials_sheen" || name=="KHR_materials_iridescence" ||
      name=="KHR_materials_anisotropy" || name=="KHR_materials_unlit" || name=="KHR_materials_pbrSpecularGlossiness" ||
      name=="KHR_materials_variants" || name=="KHR_materials_emissive_strength" || name=="KHR_lights_punctual";
  };
  if(const auto *required=importer.array(*root,"extensionsRequired")) for(u32 i=0;i<required->childCount;++i) {
    const auto &extension=*document.child(*required,i);
    if(extension.kind!=Kind::String) return giveUp("extensionsRequired contém uma entrada inválida.");
    const auto name=document.textOf(extension);
    // Codecs com decodificador conectado (Entrega 4). Geometria e textura são
    // trilhas separadas: cada nome só entra aqui porque o consumidor dele existe.
    const bool codec=name=="KHR_draco_mesh_compression" || name=="EXT_meshopt_compression" ||
                     name=="KHR_meshopt_compression" || name=="KHR_texture_basisu";
    if(name!="KHR_mesh_quantization" && !codec && !appearance(name)) {
      const std::string reason="Extensão obrigatória sem decodificador: "+std::string(name)+". Fonte preservada; geometria não foi importada.";
      return giveUp(reason.c_str());
    }
  }
  for(const auto field:{"extensionsUsed","extensionsRequired"})
    if(const auto *used=importer.array(*root,field)) for(u32 i=0;i<used->childCount;++i) {
      const auto &extension=*document.child(*used,i);
      if(extension.kind!=Kind::String) continue;
      const auto name=document.textOf(extension);
      if(appearance(name) && std::find(result.appearanceExtensions.begin(),result.appearanceExtensions.end(),name)==result.appearanceExtensions.end())
        result.appearanceExtensions.emplace_back(name);
    }

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
  // A pilha carrega o índice EMITIDO do pai, não uma matriz acumulada: a
  // árvore é a saída, e compor mundo aqui só voltaria a achatar a hierarquia.
  // `mirrored`: o pai saiu com a reflexão compensada (S à direita), então este
  // nó recebe S à esquerda.
  struct Pending { u32 node; i32 parent; bool mirrored; };
  std::unordered_map<u64, Importer::PrimitiveRange> mirroredRanges;
  u32 shearedNodes = 0;
  std::string shearedNames;
  std::vector<Pending> stack;
  std::vector<bool> visited(nodes->childCount, false);
  const float identity[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  const auto *scenes = importer.array(*root, "scenes");
  const auto sceneIndex = document.index(*root, "scene");
  std::vector<u32> roots;
  std::unordered_map<std::string,u32> keyCounts;
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
    stack.push_back({roots[index - 1], -1, false});
  }
  (void)identity;
  if (!std::isfinite(limits.rootScale) || limits.rootScale < 1e-4f || limits.rootScale > 1e4f)
    return giveUp("Escala de importação inválida.");

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
    for (u32 i = 0; i < 16; ++i) if (!std::isfinite(local[i])) return giveUp("Transformação de nó não finita.");

    const auto name = document.string(node, "name");
    const std::string nodeKey = name.empty() ? "n" + std::to_string(entry.node) : std::string(name);

    // Reflexão (escala negativa), resolvida aqui e de forma exata. Editor e
    // física só aceitam TRS com escala positiva, e relaxar isso globalmente
    // mudaria esses consumidores. Com S = diag(-1,1,1), a pose local vira
    // S_pai · L · S_nó e a geometria do nó espelhado é refletida por S: o mundo
    // de cada vértice, as normais e a face da frente ficam como no arquivo.
    const float determinant = local[0] * (local[5] * local[10] - local[9] * local[6]) -
                              local[4] * (local[1] * local[10] - local[9] * local[2]) +
                              local[8] * (local[1] * local[6] - local[5] * local[2]);
    const bool mirrored = (determinant < 0) != entry.mirrored;
    if (entry.mirrored) for (const u32 i : {0u, 4u, 8u, 12u}) local[i] = -local[i];
    if (mirrored) {
      for (u32 i = 0; i < 4; ++i) local[i] = -local[i];
      ++result.mirroredNodes;
    }
    // Cisalhamento na pose local não cabe em posição/rotação/escala. Recusado
    // com os nomes, nunca aproximado (a mesma tolerância do editor ao decompor).
    {
      bool sheared = false;
      for (u32 a = 0; a < 3; ++a)
        for (u32 b = a + 1; b < 3; ++b) {
          const float lengthA = std::hypot(local[a * 4], local[a * 4 + 1], local[a * 4 + 2]);
          const float lengthB = std::hypot(local[b * 4], local[b * 4 + 1], local[b * 4 + 2]);
          const float dot = local[a * 4] * local[b * 4] + local[a * 4 + 1] * local[b * 4 + 1] + local[a * 4 + 2] * local[b * 4 + 2];
          if (lengthA > 0 && lengthB > 0 && std::fabs(dot) > 1e-4f * lengthA * lengthB) sheared = true;
        }
      if (sheared && shearedNodes++ < 4) shearedNames += (shearedNames.empty() ? "" : ", ") + nodeKey;
    }
    // Todo nó entra na árvore, TENHA OU NÃO malha. Um grupo, um pivô de
    // dobradiça ou um alvo de animação é exatamente o que segura a articulação;
    // descartá-lo por não desenhar nada é o que achata a hierarquia.
    if (result.nodes.size() >= limits.maximumNodes)
      return giveUp("O arquivo passa do limite de nós desta importação.");
    const auto emitted = static_cast<u32>(result.nodes.size());
    GltfImportNode imported;
    imported.name = name.empty() ? nodeKey : std::string(name);
    imported.parent = entry.parent;
    // Perfil (R3): escala uniforme só nas raízes, S·L. Uniforme e positiva, não
    // interfere na reflexão nem no teste de cisalhamento acima.
    if (entry.parent < 0)
      for (const u32 i : {0u, 1u, 2u, 4u, 5u, 6u, 8u, 9u, 10u, 12u, 13u, 14u}) local[i] *= limits.rootScale;
    std::copy(local, local + 16, imported.localMatrix);
    // Identificador autoral: o primeiro campo textual reconhecido do `extras`.
    // Não é o nome — nome é rótulo, e rótulo se repete.
    if (const auto *extras = document.member(node, "extras"); extras && extras->kind == Kind::Object)
      for (const auto *field : {"astra_id", "uuid", "guid", "id"}) {
        const auto value = document.string(*extras, field);
        if (!value.empty() && value.size() <= 256) { imported.authoredId = std::string(value); break; }
      }
    result.nodes.push_back(std::move(imported));

    if (document.member(node, "camera")) ++result.skippedCameras;
    const auto meshIndex = document.index(node, "mesh");
    if (meshIndex >= 0 && meshIndex < meshes->childCount) {
      if (document.index(node, "skin") >= 0) ++result.skippedSkins;
      const auto &sourceRanges = byMesh[static_cast<u32>(meshIndex)];
      for (usize r = 0; r < sourceRanges.size(); ++r) {
        if (result.draws.size() >= limits.maximumDraws)
          return giveUp("O arquivo passa do limite de desenhos desta importação.");
        // Nó espelhado desenha a cópia refletida, criada uma vez por primitiva.
        const u64 mirrorKey = (static_cast<u64>(meshIndex) << 32) | r;
        auto mirroredRange = mirroredRanges.find(mirrorKey);
        if (mirrored && mirroredRange == mirroredRanges.end()) {
          Importer::PrimitiveRange copy;
          if (!importer.mirrorRange(sourceRanges[r], copy)) return giveUp(result.diagnostic.c_str());
          mirroredRange = mirroredRanges.emplace(mirrorKey, std::move(copy)).first;
        }
        const auto &range = mirrored ? mirroredRange->second : sourceRanges[r];
        renderer::MapDrawRecord draw{};
        draw.firstIndex = range.firstIndex;
        draw.indexCount = range.indexCount;
        draw.vertexOffset = range.vertexOffset;
        draw.materialIndex = range.material;
        // Modelo IDENTIDADE e limites LOCAIS: a geometria vive no espaço do nó,
        // e a pose vem da árvore. Assar a matriz de mundo aqui apagaria a
        // hierarquia -- e foi o que uma versão anterior desta importação fez.
        draw.model[0] = draw.model[5] = draw.model[10] = draw.model[15] = 1;
        std::copy(range.center, range.center + 3, draw.boundsCenter);
        draw.boundsRadius = range.radius;
        draw.lodGroupId = static_cast<u32>(result.draws.size());
        result.draws.push_back(draw);
        result.drawNodes.push_back(emitted);
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
        stack.push_back({index, static_cast<i32>(emitted), mirrored});
      }
  }

  if (shearedNodes) {
    const std::string reason = "Cisalhamento (shear) na transformação local de " + std::to_string(shearedNodes) +
                               " nó(s): " + shearedNames + (shearedNodes > 4 ? ", …" : "") +
                               ". A pose não cabe em posição, rotação e escala e não é aproximada; aplique a "
                               "transformação no editor 3D e exporte de novo.";
    return giveUp(reason.c_str());
  }

  if (result.draws.empty())
    return giveUp("O arquivo não tem nenhuma malha de triângulos que esta importação saiba ler.");
  // Names in glTF are labels, not unique identifiers. Retain legacy keys when
  // unique; namespace every collision, including the first occurrence.
  for(const auto &key:result.keys) ++keyCounts[key];
  for(usize i=0;i<result.keys.size();++i) if(keyCounts[result.keys[i]]>1)
    result.keys[i]+="@node:"+std::to_string(result.drawNodes[i]);
  importer.report(1.f, "Concluído");
  out = std::move(result);
  return true;
}

} // namespace ae::resources
