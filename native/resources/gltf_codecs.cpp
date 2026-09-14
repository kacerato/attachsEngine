#include "resources/gltf_codecs.h"

#include <cstring>
#include <memory>
#include <mutex>

// Cabeçalhos de terceiros: os avisos deles não são avisos da Astra, que compila
// com -Werror. Silenciados só em volta das inclusões.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#include "meshoptimizer.h"
#include "draco/compression/decode.h"
#include "draco/mesh/mesh.h"
#include "basisu_transcoder.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace ae::resources {
namespace {
bool refuse(std::string &diagnostic, std::string text) {
  diagnostic = std::move(text);
  return false;
}

u32 glTFComponentSize(u32 component) {
  switch (component) {
    case 5120: case 5121: return 1;
    case 5122: case 5123: return 2;
    case 5125: case 5126: return 4;
    default: return 0;
  }
}

template <typename T>
bool copyDracoAttribute(const draco::PointAttribute &attribute, u32 points, u32 components, std::vector<u8> &bytes) {
  bytes.resize(static_cast<usize>(points) * components * sizeof(T));
  T value[4]{};
  for (u32 p = 0; p < points; ++p) {
    const auto index = attribute.mapped_index(draco::PointIndex(p));
    if (!attribute.ConvertValue<T>(index, static_cast<int8_t>(components), value)) return false;
    std::memcpy(bytes.data() + static_cast<usize>(p) * components * sizeof(T), value, components * sizeof(T));
  }
  return true;
}
} // namespace

bool decodeMeshoptView(std::span<const u8> compressed, u32 count, u32 byteStride, std::string_view mode,
                       std::string_view filter, u64 maximumBytes, std::vector<u8> &out, std::string &diagnostic) {
  out.clear();
  diagnostic.clear();
  // meshoptimizer confere estas condições com assert: um arquivo hostil
  // derrubaria o processo em build de depuração. Validadas antes, com motivo.
  if (!count || !byteStride || byteStride > 256) return refuse(diagnostic, "Visão meshopt com contagem ou passo inválido.");
  const bool attributes = mode == "ATTRIBUTES", triangles = mode == "TRIANGLES", indices = mode == "INDICES";
  if (!attributes && !triangles && !indices)
    return refuse(diagnostic, "Visão meshopt com modo desconhecido: " + std::string(mode) + ".");
  if (attributes && byteStride % 4) return refuse(diagnostic, "Visão meshopt de atributos com passo que não é múltiplo de 4.");
  if (!attributes && byteStride != 2 && byteStride != 4) return refuse(diagnostic, "Visão meshopt de índices precisa de passo 2 ou 4.");
  if (triangles && count % 3) return refuse(diagnostic, "Visão meshopt de triângulos com contagem que não é múltipla de 3.");
  const bool noFilter = filter.empty() || filter == "NONE";
  if (!noFilter && !attributes) return refuse(diagnostic, "Filtro meshopt só vale para atributos.");
  // Filtro e passo conferidos ANTES de decodificar (asserts de vertexfilter.cpp v1.2).
  if (!noFilter) {
    const bool known = filter == "OCTAHEDRAL" || filter == "QUATERNION" || filter == "COLOR" || filter == "EXPONENTIAL";
    if (!known) return refuse(diagnostic, "Filtro meshopt desconhecido: " + std::string(filter) + ".");
    if (filter == "QUATERNION" && byteStride != 8) return refuse(diagnostic, "Filtro meshopt QUATERNION exige passo 8.");
    if ((filter == "OCTAHEDRAL" || filter == "COLOR") && byteStride != 4 && byteStride != 8)
      return refuse(diagnostic, "Filtro meshopt " + std::string(filter) + " exige passo 4 ou 8.");
  }
  const u64 bytes = static_cast<u64>(count) * byteStride;
  if (bytes > maximumBytes) return refuse(diagnostic, "Visão meshopt expande além do orçamento da importação.");
  out.resize(static_cast<usize>(bytes));
  const int result = attributes ? meshopt_decodeVertexBuffer(out.data(), count, byteStride, compressed.data(), compressed.size())
                     : triangles ? meshopt_decodeIndexBuffer(out.data(), count, byteStride, compressed.data(), compressed.size())
                                 : meshopt_decodeIndexSequence(out.data(), count, byteStride, compressed.data(), compressed.size());
  if (result != 0) {
    out.clear();
    return refuse(diagnostic, "Dados meshopt corrompidos (código " + std::to_string(result) + ").");
  }
  if (noFilter) return true;
  // Passos exigidos pelos asserts de vertexfilter.cpp (v1.2): quaternião só em 8.
  if (filter == "QUATERNION") {
    if (byteStride != 8) {
      out.clear();
      return refuse(diagnostic, "Filtro meshopt QUATERNION exige passo 8.");
    }
    meshopt_decodeFilterQuat(out.data(), count, byteStride);
    return true;
  }
  if (filter == "OCTAHEDRAL" || filter == "COLOR") {
    if (byteStride != 4 && byteStride != 8) {
      out.clear();
      return refuse(diagnostic, "Filtro meshopt " + std::string(filter) + " exige passo 4 ou 8.");
    }
    if (filter == "OCTAHEDRAL") meshopt_decodeFilterOct(out.data(), count, byteStride);
    else meshopt_decodeFilterColor(out.data(), count, byteStride);
    return true;
  }
  if (filter == "EXPONENTIAL") {
    meshopt_decodeFilterExp(out.data(), count, byteStride);
    return true;
  }
  out.clear();
  return refuse(diagnostic, "Filtro meshopt desconhecido: " + std::string(filter) + ".");
}

bool decodeDracoPrimitive(std::span<const u8> compressed, std::span<const DracoAttributeRequest> requests,
                          u64 maximumBytes, DracoPrimitive &out, std::string &diagnostic) {
  out = {};
  diagnostic.clear();
  if (compressed.empty() || compressed.size() > 0x7fffffffu) return refuse(diagnostic, "Bloco Draco vazio ou grande demais.");
  draco::DecoderBuffer buffer;
  buffer.Init(reinterpret_cast<const char *>(compressed.data()), compressed.size());
  const auto type = draco::Decoder::GetEncodedGeometryType(&buffer);
  if (!type.ok()) return refuse(diagnostic, "Bloco Draco ilegível: " + type.status().error_msg_string());
  if (type.value() != draco::TRIANGULAR_MESH) return refuse(diagnostic, "Bloco Draco não é uma malha de triângulos.");
  draco::Decoder decoder;
  auto decoded = decoder.DecodeMeshFromBuffer(&buffer);
  if (!decoded.ok()) return refuse(diagnostic, "Draco recusou o bloco: " + decoded.status().error_msg_string());
  const std::unique_ptr<draco::Mesh> mesh = std::move(decoded).value();
  if (!mesh) return refuse(diagnostic, "Draco não devolveu malha.");
  const u64 points = mesh->num_points(), faces = mesh->num_faces();
  u64 needed = faces * 3 * sizeof(u32);
  for (const auto &request : requests) needed += points * request.components * glTFComponentSize(request.componentType);
  if (needed > maximumBytes) return refuse(diagnostic, "Bloco Draco expande além do orçamento da importação.");
  if (points > 0xffffffffu) return refuse(diagnostic, "Bloco Draco com vértices demais.");
  out.vertexCount = static_cast<u32>(points);
  out.indices.resize(static_cast<usize>(faces * 3));
  for (u64 f = 0; f < faces; ++f) {
    const auto &face = mesh->face(draco::FaceIndex(static_cast<u32>(f)));
    for (u32 corner = 0; corner < 3; ++corner) {
      const u32 value = face[corner].value();
      if (value >= points) {
        out = {};
        return refuse(diagnostic, "Bloco Draco com índice fora dos vértices.");
      }
      out.indices[static_cast<usize>(f * 3 + corner)] = value;
    }
  }
  out.attributes.resize(requests.size());
  for (usize r = 0; r < requests.size(); ++r) {
    const auto &request = requests[r];
    const auto *attribute = mesh->GetAttributeByUniqueId(request.uniqueId);
    if (!attribute) {
      out = {};
      return refuse(diagnostic, "Bloco Draco sem o atributo " + std::to_string(request.uniqueId) + " que o arquivo declara.");
    }
    if (attribute->num_components() != request.components || request.components > 4) {
      out = {};
      return refuse(diagnostic, "Atributo Draco com número de componentes diferente do acessor.");
    }
    bool copied = false;
    switch (request.componentType) {
      case 5120: copied = copyDracoAttribute<int8_t>(*attribute, out.vertexCount, request.components, out.attributes[r]); break;
      case 5121: copied = copyDracoAttribute<uint8_t>(*attribute, out.vertexCount, request.components, out.attributes[r]); break;
      case 5122: copied = copyDracoAttribute<int16_t>(*attribute, out.vertexCount, request.components, out.attributes[r]); break;
      case 5123: copied = copyDracoAttribute<uint16_t>(*attribute, out.vertexCount, request.components, out.attributes[r]); break;
      case 5125: copied = copyDracoAttribute<uint32_t>(*attribute, out.vertexCount, request.components, out.attributes[r]); break;
      case 5126: copied = copyDracoAttribute<float>(*attribute, out.vertexCount, request.components, out.attributes[r]); break;
      default: break;
    }
    if (!copied) {
      out = {};
      return refuse(diagnostic, "Atributo Draco não pôde ser convertido para o tipo do acessor.");
    }
  }
  return true;
}

bool isKtx2(std::span<const u8> bytes) {
  static constexpr u8 identifier[12]{0xAB, 'K', 'T', 'X', ' ', '2', '0', 0xBB, '\r', '\n', 0x1A, '\n'};
  return bytes.size() >= 12 && std::memcmp(bytes.data(), identifier, 12) == 0;
}

bool readKtx2Dimensions(std::span<const u8> bytes, u32 &width, u32 &height) {
  width = height = 0;
  if (!isKtx2(bytes) || bytes.size() < 48) return false;
  std::memcpy(&width, bytes.data() + 20, 4);
  std::memcpy(&height, bytes.data() + 24, 4);
  if (!width || !height) {
    width = height = 0;
    return false;
  }
  return true;
}

bool transcodeKtx2Rgba8(std::span<const u8> bytes, u32 &width, u32 &height, std::vector<u8> &rgba,
                        std::string &diagnostic) {
  width = height = 0;
  rgba.clear();
  diagnostic.clear();
  if (!isKtx2(bytes) || bytes.size() > 0xffffffffu) return refuse(diagnostic, "Imagem KTX2 inválida.");
  static std::once_flag initialized;
  std::call_once(initialized, [] { basist::basisu_transcoder_init(); });
  basist::ktx2_transcoder transcoder;
  if (!transcoder.init(bytes.data(), static_cast<u32>(bytes.size())))
    return refuse(diagnostic, "KTX2 sem supercompressão Basis ou com cabeçalho inválido; só KTX2/BasisU é lido.");
  if (transcoder.get_faces() != 1 || transcoder.get_layers() > 1)
    return refuse(diagnostic, "KTX2 de cubemap ou array não é uma textura 2D.");
  if (transcoder.is_hdr()) return refuse(diagnostic, "KTX2 HDR não é lido neste perfil.");
  if (!transcoder.start_transcoding()) return refuse(diagnostic, "Não foi possível iniciar a transcodificação do KTX2.");
  const u32 w = transcoder.get_width(), h = transcoder.get_height();
  if (!w || !h) return refuse(diagnostic, "KTX2 sem dimensões.");
  rgba.resize(static_cast<usize>(w) * h * 4);
  if (!transcoder.transcode_image_level(0, 0, 0, rgba.data(), w * h, basist::transcoder_texture_format::cTFRGBA32)) {
    rgba.clear();
    return refuse(diagnostic, "Falha ao transcodificar o KTX2.");
  }
  width = w;
  height = h;
  return true;
}
} // namespace ae::resources
