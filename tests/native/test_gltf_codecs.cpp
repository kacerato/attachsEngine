// M08/M09 Entrega 4 — codecs de importação: meshopt, Draco e KTX2/BasisU.
//
// meshopt é codificado aqui mesmo, com o codificador real da v1.2 vendorizada.
// Draco e KTX2 usam blocos gerados pelas ferramentas oficiais das mesmas versões
// (tests/native/fixtures/codecs/README.txt diz como regenerar e os SHA-256).
#include "harness.h"
#include "resources/gltf_codecs.h"
#include "resources/gltf_import.h"
#include "resources/image_decode.h"
#include "resources/import_cache.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include "meshoptimizer.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

using namespace ae;

namespace {
void putLittle32(std::vector<u8> &out, u32 value) {
  for (u32 i = 0; i < 4; ++i) out.push_back(static_cast<u8>(value >> (i * 8)));
}

void pad4(std::vector<u8> &bytes) {
  while (bytes.size() % 4) bytes.push_back(0);
}

std::vector<u8> buildGlb(std::string json, std::vector<u8> binary) {
  while (json.size() % 4) json.push_back(' ');
  pad4(binary);
  std::vector<u8> glb;
  putLittle32(glb, 0x46546C67);
  putLittle32(glb, 2);
  putLittle32(glb, static_cast<u32>(12 + 8 + json.size() + 8 + binary.size()));
  putLittle32(glb, static_cast<u32>(json.size()));
  putLittle32(glb, 0x4E4F534A);
  glb.insert(glb.end(), json.begin(), json.end());
  putLittle32(glb, static_cast<u32>(binary.size()));
  putLittle32(glb, 0x004E4942);
  glb.insert(glb.end(), binary.begin(), binary.end());
  return glb;
}

float vertexFloat(const std::vector<u8> &vertices, usize vertex, usize offset) {
  float value = 0;
  std::memcpy(&value, vertices.data() + vertex * renderer::MapVertexStride + offset, 4);
  return value;
}

i16 vertexSnorm(const std::vector<u8> &vertices, usize vertex, usize offset) {
  i16 value = 0;
  std::memcpy(&value, vertices.data() + vertex * renderer::MapVertexStride + offset, 2);
  return value;
}

// O codificador de índices pode girar um triângulo; girar preserva a orientação.
std::array<u32, 3> canonical(u32 a, u32 b, u32 c) {
  if (b < a && b < c) return {b, c, a};
  if (c < a && c < b) return {c, a, b};
  return {a, b, c};
}

// Erro médio contra o gradiente de grad8.png (R = 32x, G = 32y, B = 128, A = 255).
bool gradientError(const std::vector<u8> &bytes, double &error, bool &opaque, std::string &diagnostic) {
  resources::DecodedImage image;
  if (!resources::decodeImageRgba8(bytes, {}, image, diagnostic)) return false;
  if (image.width != 8 || image.height != 8 || image.rgba.size() != 256) {
    diagnostic = "dimensões inesperadas";
    return false;
  }
  error = 0;
  opaque = true;
  for (u32 y = 0; y < 8; ++y)
    for (u32 x = 0; x < 8; ++x) {
      const u8 *pixel = image.rgba.data() + (y * 8 + x) * 4;
      error += std::fabs(pixel[0] - 32.0 * x) + std::fabs(pixel[1] - 32.0 * y) + std::fabs(pixel[2] - 128.0);
      opaque &= pixel[3] == 255;
    }
  error /= 64.0 * 3.0;
  return true;
}

// draco_encoder 1.5.7 -cl 7 sobre quad.obj: posição (id 0), UV (id 1), normal (id 2).
const std::vector<u8> kQuadDraco{
    0x44,0x52,0x41,0x43,0x4f,0x02,0x02,0x01,0x01,0x00,0x00,0x00,0x04,0x02,0x02,0x02,0x00,0x00,0x01,0x1f,0xff,0x01,0x11,0xff,
    0x01,0x11,0xff,0x01,0x11,0x03,0xff,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x01,0x00,0x09,0x03,0x00,0x00,0x02,0x01,0x03,
    0x09,0x02,0x00,0x01,0x02,0x01,0x01,0x09,0x03,0x00,0x02,0x03,0x01,0x01,0x01,0x00,0x03,0x03,0x01,0x30,0x01,0x10,0x03,0x00,
    0x28,0x82,0x98,0x00,0x00,0x00,0x00,0x00,0xff,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x80,0x3f,0x0b,0x05,0x01,0x01,0x00,0x02,0x03,0x01,0x40,0x01,0x00,0x0c,0x02,0x00,0x00,0x00,0x80,0x02,0x80,0x70,
    0x00,0x00,0x00,0x00,0xff,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x3f,0x0a,0x06,0x03,0x01,
    0x01,0x01,0x01,0x01,0x40,0x01,0x00,0xff,0x00,0x00,0x00,0x7f,0x00,0x00,0x00,0xff,0x02,0x44,0x40,0x08};

// basisu v2_50 -ktx2 grad8.png (ETC1S).
const std::vector<u8> kGradEtc1s{
    0xab,0x4b,0x54,0x58,0x20,0x32,0x30,0xbb,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x08,0x00,0x00,0x00,
    0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x00,0x00,0x00,
    0x68,0x00,0x00,0x00,0x2c,0x00,0x00,0x00,0x94,0x00,0x00,0x00,0x24,0x00,0x00,0x00,0xb8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x8e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x46,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x2c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x28,0x00,0xa3,0x01,0x02,0x00,
    0x03,0x03,0x00,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xff,0xff,0xff,0xff,0x1f,0x00,0x00,0x00,0x4b,0x54,0x58,0x77,0x72,0x69,0x74,0x65,0x72,0x00,0x42,0x61,0x73,0x69,0x73,0x20,
    0x55,0x6e,0x69,0x76,0x65,0x72,0x73,0x61,0x6c,0x20,0x32,0x2e,0x35,0x30,0x00,0x00,0x04,0x00,0x03,0x00,0x2d,0x00,0x00,0x00,
    0x0d,0x00,0x00,0x00,0x2c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,0xc0,0x44,0x00,0x00,0x00,0x00,0x00,0x00,0xe2,0xc0,0x05,0x30,0x25,0x00,0x00,
    0x00,0x00,0x00,0x04,0x71,0xc1,0x62,0x00,0x30,0x01,0x00,0x00,0x00,0x00,0x00,0x80,0x18,0x00,0x26,0x00,0x04,0x00,0x00,0x00,
    0x00,0x90,0xfa,0x44,0x09,0x04,0xa0,0x54,0xff,0x07,0xa0,0x52,0xff,0x07,0xa0,0x4a,0xff,0x07,0x00,0xc1,0x44,0x00,0x00,0x00,
    0x00,0x00,0x00,0xf2,0x5f,0x4d,0x00,0x98,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x20,0x02,0x26,0x02,0x00,0x00,0x00,0x00,0x00,
    0x90,0x38,0x02,0xc0,0x04,0x80,0x00,0x00,0x00,0x00,0x00,0x22,0x10,0x00,0x70};

// basisu v2_50 -ktx2 -uastc grad8.png (UASTC com supercompressão Zstandard).
const std::vector<u8> kGradUastc{
    0xab,0x4b,0x54,0x58,0x20,0x32,0x30,0xbb,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x08,0x00,0x00,0x00,
    0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x02,0x00,0x00,0x00,
    0x68,0x00,0x00,0x00,0x2c,0x00,0x00,0x00,0x94,0x00,0x00,0x00,0x24,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xb8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x34,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x2c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x28,0x00,0xa6,0x01,0x02,0x00,
    0x03,0x03,0x00,0x00,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xff,0xff,0xff,0xff,0x1f,0x00,0x00,0x00,0x4b,0x54,0x58,0x77,0x72,0x69,0x74,0x65,0x72,0x00,0x42,0x61,0x73,0x69,0x73,0x20,
    0x55,0x6e,0x69,0x76,0x65,0x72,0x73,0x61,0x6c,0x20,0x32,0x2e,0x35,0x30,0x00,0x00,0x28,0xb5,0x2f,0xfd,0x20,0x40,0x5d,0x01,
    0x00,0x04,0x02,0xdb,0x92,0x06,0x00,0x0f,0x30,0xc0,0xff,0x43,0xc8,0x51,0xd9,0x62,0xea,0x73,0xfb,0xdb,0x92,0x03,0x01,0xff,
    0x13,0x94,0x0d,0x19,0x0f,0xf0,0x4f,0x03,0x1a,0xff,0xd3,0x03,0x00,0x60,0x13,0x60,0x8a,0x31,0x66,0x13};

std::string dracoQuadJson(u32 positionCount, usize blockLength) {
  const usize padded = (blockLength + 3) & ~usize{3};
  return std::string(R"({"asset":{"version":"2.0"},"extensionsUsed":["KHR_draco_mesh_compression"],)") +
         R"("extensionsRequired":["KHR_draco_mesh_compression"],"buffers":[{"byteLength":)" + std::to_string(padded) +
         R"(}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":)" + std::to_string(blockLength) +
         R"(}],"accessors":[{"componentType":5126,"count":)" + std::to_string(positionCount) +
         R"(,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},{"componentType":5126,"count":4,"type":"VEC2"},)" +
         R"({"componentType":5126,"count":4,"type":"VEC3"},{"componentType":5123,"count":6,"type":"SCALAR"}],)" +
         R"("meshes":[{"name":"Quad","primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":1,"NORMAL":2},"indices":3,)" +
         R"("extensions":{"KHR_draco_mesh_compression":{"bufferView":0,"attributes":{"POSITION":0,"TEXCOORD_0":1,"NORMAL":2}}}}]}],)" +
         R"("nodes":[{"name":"Quad","mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})";
}
} // namespace

AE_TEST(m09e4_meshopt_views_decode_to_the_original_quad) {
  const float positions[12]{0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0};
  const unsigned int indices[6]{0, 1, 2, 0, 2, 3};
  std::vector<u8> vertexData(meshopt_encodeVertexBufferBound(4, 12));
  vertexData.resize(meshopt_encodeVertexBuffer(vertexData.data(), vertexData.size(), positions, 4, 12));
  std::vector<u8> indexData(meshopt_encodeIndexBufferBound(6, 4));
  indexData.resize(meshopt_encodeIndexBuffer(indexData.data(), indexData.size(), indices, 6));
  AE_EXPECT_TRUE(!vertexData.empty() && !indexData.empty(), "o codificador real produziu os dois blocos");
  std::vector<u8> binary = vertexData;
  pad4(binary);
  const usize indexOffset = binary.size();
  binary.insert(binary.end(), indexData.begin(), indexData.end());
  pad4(binary);
  // A bufferView aponta para um buffer de reserva sem dados, como o gltfpack faz;
  // os bytes reais estão na extensão, no bloco binário do GLB.
  const std::string json =
      std::string(R"({"asset":{"version":"2.0"},"extensionsUsed":["EXT_meshopt_compression"],)") +
      R"("extensionsRequired":["EXT_meshopt_compression"],"buffers":[{"byteLength":)" + std::to_string(binary.size()) +
      R"(},{"byteLength":60,"extensions":{"EXT_meshopt_compression":{"fallback":true}}}],"bufferViews":[)" +
      R"({"buffer":1,"byteLength":48,"byteStride":12,"extensions":{"EXT_meshopt_compression":{"buffer":0,"byteOffset":0,"byteLength":)" +
      std::to_string(vertexData.size()) + R"(,"byteStride":12,"count":4,"mode":"ATTRIBUTES"}}},)" +
      R"({"buffer":1,"byteOffset":48,"byteLength":12,"extensions":{"EXT_meshopt_compression":{"buffer":0,"byteOffset":)" +
      std::to_string(indexOffset) + R"(,"byteLength":)" + std::to_string(indexData.size()) +
      R"(,"byteStride":2,"count":6,"mode":"TRIANGLES"}}}],)" +
      R"("accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},)" +
      R"({"bufferView":1,"componentType":5123,"count":6,"type":"SCALAR"}],)" +
      R"("meshes":[{"name":"Quad","primitives":[{"attributes":{"POSITION":0},"indices":1}]}],)" +
      R"("nodes":[{"name":"Quad","mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})";
  resources::GltfImport model;
  AE_EXPECT_TRUE(resources::importGlb(buildGlb(json, binary), {}, {}, model), model.diagnostic.c_str());
  AE_EXPECT_EQ(model.meshoptViews, 2u, "as duas visões comprimidas foram decodificadas");
  AE_EXPECT_EQ(model.vertices.size() / renderer::MapVertexStride, usize{4}, "quatro vértices");
  bool exact = model.vertices.size() / renderer::MapVertexStride == 4;
  for (usize v = 0; exact && v < 4; ++v)
    for (usize axis = 0; axis < 3; ++axis) exact &= vertexFloat(model.vertices, v, axis * 4) == positions[v * 3 + axis];
  AE_EXPECT_TRUE(exact, "posições iguais às originais, sem perda");
  AE_EXPECT_EQ(model.indices.size(), usize{6}, "seis índices");
  std::vector<std::array<u32, 3>> expected{canonical(0, 1, 2), canonical(0, 2, 3)}, actual;
  for (usize i = 0; i + 2 < model.indices.size(); i += 3)
    actual.push_back(canonical(model.indices[i], model.indices[i + 1], model.indices[i + 2]));
  std::sort(expected.begin(), expected.end());
  std::sort(actual.begin(), actual.end());
  AE_EXPECT_TRUE(expected == actual, "os mesmos triângulos, com a mesma orientação");
  AE_EXPECT_TRUE(model.appearanceExtensions.empty(), "compressão não vira omissão de aparência");
}

AE_TEST(m09e4_meshopt_filters_and_hostile_views_fail_closed) {
  const float normals[16]{0, 0, 1, 0, 1, 0, 0, 0, 0, -1, 0, 0, 0.6f, 0.8f, 0, 0};
  std::vector<u8> filtered(16);
  meshopt_encodeFilterOct(filtered.data(), 4, 4, 8, normals);
  std::vector<u8> encoded(meshopt_encodeVertexBufferBound(4, 4));
  encoded.resize(meshopt_encodeVertexBuffer(encoded.data(), encoded.size(), filtered.data(), 4, 4));
  std::vector<u8> decoded;
  std::string diagnostic;
  AE_EXPECT_TRUE(resources::decodeMeshoptView(encoded, 4, 4, "ATTRIBUTES", "OCTAHEDRAL", 1u << 20, decoded, diagnostic),
                 diagnostic.c_str());
  bool close = decoded.size() == 16;
  for (usize n = 0; close && n < 4; ++n)
    for (usize axis = 0; axis < 3; ++axis)
      close &= std::fabs(static_cast<float>(static_cast<i8>(decoded[n * 4 + axis])) / 127.0f - normals[n * 4 + axis]) < 0.05f;
  AE_EXPECT_TRUE(close, "normais reconstruídas pelo filtro octaédrico");

  AE_EXPECT_TRUE(!resources::decodeMeshoptView(encoded, 4, 4, "ATTRIBUTES", "QUATERNION", 1u << 20, decoded, diagnostic) &&
                     diagnostic.find("QUATERNION") != std::string::npos,
                 "passo incompatível com o filtro recusado antes da biblioteca");
  AE_EXPECT_TRUE(!resources::decodeMeshoptView(encoded, 4, 4, "ATTRIBUTES", "SEPIA", 1u << 20, decoded, diagnostic),
                 "filtro desconhecido recusado");
  std::vector<u8> truncated(encoded.begin(), encoded.begin() + static_cast<std::ptrdiff_t>(encoded.size() / 2));
  AE_EXPECT_TRUE(!resources::decodeMeshoptView(truncated, 4, 4, "ATTRIBUTES", "NONE", 1u << 20, decoded, diagnostic) &&
                     decoded.empty(),
                 "bloco truncado recusado com saída vazia");
  AE_EXPECT_TRUE(!resources::decodeMeshoptView(encoded, 4000000, 4, "ATTRIBUTES", "", 1024, decoded, diagnostic) &&
                     diagnostic.find("orçamento") != std::string::npos,
                 "expansão além do orçamento recusada antes de alocar");
  AE_EXPECT_TRUE(!resources::decodeMeshoptView(encoded, 5, 2, "TRIANGLES", "", 1u << 20, decoded, diagnostic),
                 "triângulos com contagem que não é múltipla de 3");
  AE_EXPECT_TRUE(!resources::decodeMeshoptView(encoded, 4, 4, "PONTOS", "", 1u << 20, decoded, diagnostic),
                 "modo desconhecido recusado");
}

AE_TEST(m09e4_draco_primitive_decodes_the_quantized_quad_with_its_orientation) {
  resources::GltfImport model;
  AE_EXPECT_TRUE(resources::importGlb(buildGlb(dracoQuadJson(4, kQuadDraco.size()), kQuadDraco), {}, {}, model),
                 model.diagnostic.c_str());
  AE_EXPECT_EQ(model.dracoPrimitives, 1u, "uma primitiva Draco decodificada");
  const usize vertices = model.vertices.size() / renderer::MapVertexStride;
  AE_EXPECT_EQ(vertices, usize{4}, "quatro vértices");
  AE_EXPECT_EQ(model.indices.size(), usize{6}, "dois triângulos");
  bool corners = vertices == 4, uvs = vertices == 4, normals = vertices == 4;
  for (usize v = 0; v < vertices; ++v) {
    const float x = vertexFloat(model.vertices, v, 0), y = vertexFloat(model.vertices, v, 4), z = vertexFloat(model.vertices, v, 8);
    const auto near = [](float value, float target) { return std::fabs(value - target) < 1e-3f; };
    corners &= (near(x, 0) || near(x, 1)) && (near(y, 0) || near(y, 1)) && near(z, 0);
    uvs &= std::fabs(vertexFloat(model.vertices, v, 28) - x) < 2e-3f && std::fabs(vertexFloat(model.vertices, v, 32) - y) < 2e-3f;
    normals &= vertexSnorm(model.vertices, v, 16) > 32000;
  }
  AE_EXPECT_TRUE(corners, "posições dequantizadas nos cantos do quadrado");
  AE_EXPECT_TRUE(uvs, "UV acompanha a posição, como no OBJ de origem");
  AE_EXPECT_TRUE(normals, "normal +Z");
  bool facing = model.indices.size() == 6;
  for (usize i = 0; facing && i < 6; i += 3) {
    const auto at = [&](usize corner, usize axis) { return vertexFloat(model.vertices, model.indices[i + corner], axis * 4); };
    const float ux = at(1, 0) - at(0, 0), uy = at(1, 1) - at(0, 1), vx = at(2, 0) - at(0, 0), vy = at(2, 1) - at(0, 1);
    facing &= ux * vy - uy * vx > 0;
  }
  AE_EXPECT_TRUE(facing, "orientação dos triângulos preservada");

  resources::GltfImport mismatch;
  AE_EXPECT_TRUE(!resources::importGlb(buildGlb(dracoQuadJson(5, kQuadDraco.size()), kQuadDraco), {}, {}, mismatch) &&
                     mismatch.diagnostic.find("Draco") != std::string::npos,
                 "acessor com contagem diferente do bloco recusado");
  std::vector<u8> truncated(kQuadDraco.begin(), kQuadDraco.begin() + static_cast<std::ptrdiff_t>(kQuadDraco.size() / 2));
  resources::GltfImport broken;
  AE_EXPECT_TRUE(!resources::importGlb(buildGlb(dracoQuadJson(4, truncated.size()), truncated), {}, {}, broken) &&
                     broken.diagnostic.find("Draco") != std::string::npos,
                 "bloco Draco truncado recusado com motivo");
  resources::GltfImportLimits tight;
  tight.maximumExpandedBytes = 32;
  resources::GltfImport starved;
  AE_EXPECT_TRUE(!resources::importGlb(buildGlb(dracoQuadJson(4, kQuadDraco.size()), kQuadDraco), tight, {}, starved) &&
                     starved.diagnostic.find("orçamento") != std::string::npos,
                 "expansão Draco além do orçamento recusada");
}

AE_TEST(m09e4_ktx2_etc1s_and_uastc_transcode_to_rgba8) {
  AE_EXPECT_TRUE(resources::detectImageContainer(kGradEtc1s) == resources::ImageContainer::Ktx2, "KTX2 reconhecido pelo identificador");
  double error = 0;
  bool opaque = false;
  std::string diagnostic;
  AE_EXPECT_TRUE(gradientError(kGradEtc1s, error, opaque, diagnostic), diagnostic.c_str());
  AE_EXPECT_TRUE(error < 24.0, "ETC1S transcodificado perto do gradiente de origem");
  AE_EXPECT_TRUE(opaque, "alfa opaco no ETC1S");
  AE_EXPECT_TRUE(gradientError(kGradUastc, error, opaque, diagnostic), diagnostic.c_str());
  AE_EXPECT_TRUE(error < 8.0, "UASTC (com Zstandard) transcodificado perto do gradiente de origem");
  AE_EXPECT_TRUE(opaque, "alfa opaco no UASTC");

  resources::ImageDecodeLimits small;
  small.maximumDimension = 4;
  u32 width = 0, height = 0;
  AE_EXPECT_TRUE(resources::readImageDimensions(kGradUastc, {}, width, height) && width == 8 && height == 8,
                 "dimensões lidas só do cabeçalho");
  AE_EXPECT_TRUE(!resources::readImageDimensions(kGradUastc, small, width, height), "limite de dimensão vale para KTX2");
  resources::DecodedImage refused;
  AE_EXPECT_TRUE(!resources::decodeImageRgba8(kGradUastc, small, refused, diagnostic) && refused.rgba.empty(),
                 "recusado pelo cabeçalho, antes de transcodificar");
  std::vector<u8> truncated(kGradEtc1s.begin(), kGradEtc1s.begin() + 100);
  AE_EXPECT_TRUE(!resources::decodeImageRgba8(truncated, {}, refused, diagnostic) && !diagnostic.empty() && refused.rgba.empty(),
                 "KTX2 truncado recusado com motivo");
}

namespace {
using Matrix = std::array<float, 16>;
constexpr Matrix kIdentity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

Matrix multiply(const Matrix &a, const Matrix &b) {
  Matrix result{};
  for (u32 column = 0; column < 4; ++column)
    for (u32 row = 0; row < 4; ++row) {
      float sum = 0;
      for (u32 k = 0; k < 4; ++k) sum += a[k * 4 + row] * b[column * 4 + k];
      result[column * 4 + row] = sum;
    }
  return result;
}

std::array<float, 3> transformPoint(const Matrix &m, const std::array<float, 3> &p, bool translate = true) {
  const float w = translate ? 1.0f : 0.0f;
  return {m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12] * w, m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13] * w,
          m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14] * w};
}

Matrix worldOf(const resources::GltfImport &model, u32 node) {
  std::vector<u32> chain;
  for (i32 n = static_cast<i32>(node); n >= 0; n = model.nodes[static_cast<usize>(n)].parent) chain.push_back(static_cast<u32>(n));
  Matrix world = kIdentity;
  for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
    Matrix local{};
    std::copy(model.nodes[*it].localMatrix, model.nodes[*it].localMatrix + 16, local.begin());
    world = multiply(world, local);
  }
  return world;
}

// Triângulo com normal (0.6, 0, 0.8) em três nós: um com escala -1 em X, um filho
// que herda a reflexão e um nó comum.
std::string mirrorJson(const std::string &nodes, const std::string &roots) {
  return std::string(R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":80}],"bufferViews":[)") +
         R"({"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":36},{"buffer":0,"byteOffset":72,"byteLength":6}],)" +
         R"("accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,2,0]},)" +
         R"({"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"}],)" +
         R"("meshes":[{"name":"Tri","primitives":[{"attributes":{"POSITION":0,"NORMAL":1},"indices":2}]}],"nodes":[)" + nodes +
         R"(],"scenes":[{"nodes":[)" + roots + R"(]}],"scene":0})";
}

std::vector<u8> mirrorBinary() {
  const float positions[9]{0, 0, 0, 1, 0, 0, 0, 2, 0};
  const float normals[9]{0.6f, 0, 0.8f, 0.6f, 0, 0.8f, 0.6f, 0, 0.8f};
  const u16 indices[3]{0, 1, 2};
  std::vector<u8> binary(80, 0);
  std::memcpy(binary.data(), positions, 36);
  std::memcpy(binary.data() + 36, normals, 36);
  std::memcpy(binary.data() + 72, indices, 6);
  return binary;
}
} // namespace

AE_TEST(m09e4_negative_scale_becomes_positive_trs_with_mirrored_geometry_and_the_same_world) {
  const auto json = mirrorJson(R"({"name":"Espelho","mesh":0,"translation":[5,0,0],"scale":[-1,1,1],"children":[1]},)"
                               R"({"name":"Filho","mesh":0,"translation":[2,0,0]},{"name":"Direto","mesh":0})",
                               "0,2");
  resources::GltfImport model;
  AE_EXPECT_TRUE(resources::importGlb(buildGlb(json, mirrorBinary()), {}, {}, model), model.diagnostic.c_str());
  AE_EXPECT_EQ(model.mirroredNodes, 2u, "o nó com escala negativa e o filho que herda a reflexão");
  AE_EXPECT_EQ(model.draws.size(), usize{3}, "três desenhos");
  bool positive = model.nodes.size() == 3;
  for (const auto &node : model.nodes) {
    const auto *m = node.localMatrix;
    positive &= m[0] * (m[5] * m[10] - m[9] * m[6]) - m[4] * (m[1] * m[10] - m[9] * m[2]) + m[8] * (m[1] * m[6] - m[5] * m[2]) > 0;
  }
  AE_EXPECT_TRUE(positive, "toda pose local sai com determinante positivo e cabe no TRS do editor");

  // Mundo original pelo glTF: Espelho = T(5)·S, Filho = T(5)·S·T(2) = T(3)·S, Direto = I.
  const Matrix originals[3]{{-1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 5, 0, 0, 1},
                            {-1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 3, 0, 0, 1},
                            kIdentity};
  const std::array<float, 3> triangle[3]{{0, 0, 0}, {1, 0, 0}, {0, 2, 0}};
  bool samePoints = model.draws.size() == 3, sameNormals = samePoints, frontFaces = samePoints;
  for (usize d = 0; samePoints && d < 3; ++d) {
    const auto &draw = model.draws[d];
    const auto node = model.drawNodes[d];
    AE_EXPECT_TRUE(node < 3, "desenho ligado a um nó");
    const auto world = worldOf(model, node);
    std::array<std::array<float, 3>, 3> emitted{};
    for (u32 k = 0; k < 3; ++k) {
      const auto index = draw.vertexOffset + model.indices[draw.firstIndex + k];
      emitted[k] = transformPoint(world, {vertexFloat(model.vertices, index, 0), vertexFloat(model.vertices, index, 4),
                                          vertexFloat(model.vertices, index, 8)});
      const std::array<float, 3> normal{vertexSnorm(model.vertices, index, 12) / 32767.0f, vertexSnorm(model.vertices, index, 14) / 32767.0f,
                                        vertexSnorm(model.vertices, index, 16) / 32767.0f};
      const auto worldNormal = transformPoint(world, normal, false);
      const auto expectedNormal = transformPoint(originals[node], {0.6f, 0, 0.8f}, false);
      for (u32 axis = 0; axis < 3; ++axis) sameNormals &= std::fabs(worldNormal[axis] - expectedNormal[axis]) < 1e-3f;
    }
    for (const auto &corner : triangle) {
      const auto expected = transformPoint(originals[node], corner);
      bool found = false;
      for (const auto &point : emitted)
        found |= std::fabs(point[0] - expected[0]) < 1e-4f && std::fabs(point[1] - expected[1]) < 1e-4f &&
                 std::fabs(point[2] - expected[2]) < 1e-4f;
      samePoints &= found;
    }
    // Face da frente (anti-horária) concorda com a normal no mundo.
    const std::array<float, 3> u{emitted[1][0] - emitted[0][0], emitted[1][1] - emitted[0][1], emitted[1][2] - emitted[0][2]};
    const std::array<float, 3> v{emitted[2][0] - emitted[0][0], emitted[2][1] - emitted[0][1], emitted[2][2] - emitted[0][2]};
    const std::array<float, 3> cross{u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
    const auto expectedNormal = transformPoint(originals[node], {0.6f, 0, 0.8f}, false);
    frontFaces &= cross[0] * expectedNormal[0] + cross[1] * expectedNormal[1] + cross[2] * expectedNormal[2] > 0;
  }
  AE_EXPECT_TRUE(samePoints, "vértices no mundo iguais aos do arquivo");
  AE_EXPECT_TRUE(sameNormals, "normais no mundo iguais às do arquivo");
  AE_EXPECT_TRUE(frontFaces, "face da frente coerente com a normal, como o glTF define para determinante negativo");
  std::vector<std::string> keys = model.keys;
  std::sort(keys.begin(), keys.end());
  AE_EXPECT_TRUE(std::adjacent_find(keys.begin(), keys.end()) == keys.end(), "geometria espelhada tem chave própria");

  resources::GltfImport sheared;
  const auto shearJson = mirrorJson(R"({"name":"Torto","mesh":0,"matrix":[1,0,0,0,0.5,1,0,0,0,0,1,0,0,0,0,1]})", "0");
  AE_EXPECT_TRUE(!resources::importGlb(buildGlb(shearJson, mirrorBinary()), {}, {}, sheared) &&
                     sheared.diagnostic.find("Torto") != std::string::npos &&
                     sheared.diagnostic.find("isalhamento") != std::string::npos,
                 "cisalhamento recusado com o nome do nó, sem aproximação");
}

namespace {
// Quadrado com UV = posição e uma textura KTX2; o material é o do teste.
std::vector<u8> transformGlb(const std::string &material) {
  const float positions[12]{0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0};
  const float uvs[8]{0, 0, 1, 0, 1, 1, 0, 1};
  const u16 indices[6]{0, 1, 2, 0, 2, 3};
  std::vector<u8> binary(92);
  std::memcpy(binary.data(), positions, 48);
  std::memcpy(binary.data() + 48, uvs, 32);
  std::memcpy(binary.data() + 80, indices, 12);
  binary.insert(binary.end(), kGradUastc.begin(), kGradUastc.end());
  pad4(binary);
  const std::string json =
      std::string(R"({"asset":{"version":"2.0"},"extensionsUsed":["KHR_texture_basisu","KHR_texture_transform"],)") +
      R"("buffers":[{"byteLength":)" + std::to_string(binary.size()) + R"(}],"bufferViews":[)" +
      R"({"buffer":0,"byteOffset":0,"byteLength":48},{"buffer":0,"byteOffset":48,"byteLength":32},)" +
      R"({"buffer":0,"byteOffset":80,"byteLength":12},{"buffer":0,"byteOffset":92,"byteLength":)" +
      std::to_string(kGradUastc.size()) + R"(}],)" +
      R"("accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},)" +
      R"({"bufferView":1,"componentType":5126,"count":4,"type":"VEC2"},{"bufferView":2,"componentType":5123,"count":6,"type":"SCALAR"}],)" +
      R"("images":[{"bufferView":3,"mimeType":"image/ktx2"}],"textures":[{"extensions":{"KHR_texture_basisu":{"source":0}}}],)" +
      R"("materials":[)" + material + R"(],)" +
      R"("meshes":[{"name":"Tela","primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":1},"indices":2,"material":0}]}],)" +
      R"("nodes":[{"name":"Tela","mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})";
  return buildGlb(json, binary);
}
} // namespace

AE_TEST(m09e4_texture_transform_is_baked_into_uvs_when_the_material_agrees) {
  // Rotação de 90° anti-horária, escala (2, 1) e deslocamento (0.5, 0), pela matriz do glTF:
  // u' = cos·2·u − sin·1·v + 0.5 e v' = sin·2·u + cos·1·v.
  const auto transformed = transformGlb(
      R"({"pbrMetallicRoughness":{"baseColorTexture":{"index":0,"extensions":{"KHR_texture_transform":)"
      R"({"offset":[0.5,0],"rotation":1.5707963267948966,"scale":[2,1]}}}}})");
  resources::GltfImport model;
  AE_EXPECT_TRUE(resources::importGlb(transformed, {}, {}, model), model.diagnostic.c_str());
  AE_EXPECT_EQ(model.bakedTextureTransforms, 1u, "a transformação foi assada nas UVs");
  AE_EXPECT_EQ(model.unappliedTextureTransforms, 0u, "e não aparece mais como omissão");
  const float expected[8]{0.5f, 0, 0.5f, 2, -0.5f, 2, -0.5f, 0};
  bool uvs = model.vertices.size() / renderer::MapVertexStride == 4;
  for (usize v = 0; uvs && v < 4; ++v)
    uvs &= std::fabs(vertexFloat(model.vertices, v, 28) - expected[v * 2]) < 1e-5f &&
           std::fabs(vertexFloat(model.vertices, v, 32) - expected[v * 2 + 1]) < 1e-5f;
  AE_EXPECT_TRUE(uvs, "UVs iguais às que o shader do glTF amostraria");

  // Normal no mesmo conjunto de UV sem a transformação: o material discorda,
  // nada é assado e a omissão continua declarada.
  const auto conflicting = transformGlb(
      R"({"pbrMetallicRoughness":{"baseColorTexture":{"index":0,"extensions":{"KHR_texture_transform":{"scale":[2,2]}}}},)"
      R"("normalTexture":{"index":0}})");
  resources::GltfImport disagreement;
  AE_EXPECT_TRUE(resources::importGlb(conflicting, {}, {}, disagreement), disagreement.diagnostic.c_str());
  AE_EXPECT_EQ(disagreement.bakedTextureTransforms, 0u, "nada assado quando o material discorda");
  AE_EXPECT_EQ(disagreement.unappliedTextureTransforms, 1u, "a transformação continua contada como não aplicada");
  AE_EXPECT_TRUE(disagreement.vertices.size() >= renderer::MapVertexStride * 2 &&
                     vertexFloat(disagreement.vertices, 1, 28) == 1.0f && vertexFloat(disagreement.vertices, 1, 32) == 0.0f,
                 "UVs intactas");

  // `texCoord` da extensão troca o conjunto: a textura passa a TEXCOORD_1 e é
  // esse conjunto que recebe a transformação (sem TEXCOORD_1, UV zero vira o deslocamento).
  const auto overridden = transformGlb(
      R"({"pbrMetallicRoughness":{"baseColorTexture":{"index":0,"extensions":{"KHR_texture_transform":)"
      R"({"offset":[0.5,0.25],"texCoord":1}}}}})");
  resources::GltfImport swapped;
  AE_EXPECT_TRUE(resources::importGlb(overridden, {}, {}, swapped), swapped.diagnostic.c_str());
  AE_EXPECT_TRUE(!swapped.materials.empty() && (swapped.materials.front().textureCoordinates & 1u), "cor base em TEXCOORD_1");
  AE_EXPECT_TRUE(swapped.vertices.size() >= renderer::MapVertexStride &&
                     vertexFloat(swapped.vertices, 0, 36) == 0.5f && vertexFloat(swapped.vertices, 0, 40) == 0.25f &&
                     vertexFloat(swapped.vertices, 1, 28) == 1.0f,
                 "só o conjunto 1 foi transformado");
}

namespace {
// basisu v2_50 -ktx2 -uastc -mipmap grad8.png (UASTC com Zstandard, 4 níveis: 8, 4, 2, 1).
const std::vector<u8> kGradUastcMips{
    0xab,0x4b,0x54,0x58,0x20,0x32,0x30,0xbb,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x08,0x00,0x00,0x00,
    0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x02,0x00,0x00,0x00,
    0xb0,0x00,0x00,0x00,0x2c,0x00,0x00,0x00,0xdc,0x00,0x00,0x00,0x24,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x4b,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x34,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x32,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x19,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x19,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x19,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x19,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x2c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x28,0x00,0xa6,0x01,0x02,0x00,
    0x03,0x03,0x00,0x00,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xff,0xff,0xff,0xff,0x1f,0x00,0x00,0x00,0x4b,0x54,0x58,0x77,0x72,0x69,0x74,0x65,0x72,0x00,0x42,0x61,0x73,0x69,0x73,0x20,
    0x55,0x6e,0x69,0x76,0x65,0x72,0x73,0x61,0x6c,0x20,0x32,0x2e,0x35,0x30,0x00,0x00,0x28,0xb5,0x2f,0xfd,0x20,0x10,0x81,0x00,
    0x00,0x57,0x51,0x11,0xf0,0x3f,0xc1,0xf8,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x28,0xb5,0x2f,0xfd,0x20,0x10,0x81,
    0x00,0x00,0xdb,0x14,0x8f,0x06,0x8f,0x2f,0xbe,0xff,0xc3,0xcc,0xf3,0xff,0xf3,0xff,0xf3,0xff,0x28,0xb5,0x2f,0xfd,0x20,0x10,
    0x81,0x00,0x00,0xdb,0xa2,0x46,0x21,0x2f,0x9a,0x68,0xff,0x5f,0xd9,0x55,0xd9,0x66,0xea,0x77,0xfb,0x28,0xb5,0x2f,0xfd,0x20,
    0x40,0x5d,0x01,0x00,0x04,0x02,0xdb,0x92,0x06,0x00,0x0f,0x30,0xc0,0xff,0x43,0xc8,0x51,0xd9,0x62,0xea,0x73,0xfb,0xdb,0x92,
    0x03,0x01,0xff,0x13,0x94,0x0d,0x19,0x0f,0xf0,0x4f,0x03,0x1a,0xff,0xd3,0x03,0x00,0x60,0x13,0x60,0x8a,0x31,0x66,0x13};

// Quadrado com uma textura KTX2 como cor base, sem transformação de UV.
std::vector<u8> ktx2Quad(const std::vector<u8> &image) {
  const float positions[12]{0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0};
  const float uvs[8]{0, 0, 1, 0, 1, 1, 0, 1};
  const u16 indices[6]{0, 1, 2, 0, 2, 3};
  std::vector<u8> binary(92);
  std::memcpy(binary.data(), positions, 48);
  std::memcpy(binary.data() + 48, uvs, 32);
  std::memcpy(binary.data() + 80, indices, 12);
  binary.insert(binary.end(), image.begin(), image.end());
  pad4(binary);
  const std::string json =
      std::string(R"({"asset":{"version":"2.0"},"extensionsUsed":["KHR_texture_basisu"],"extensionsRequired":["KHR_texture_basisu"],)") +
      R"("buffers":[{"byteLength":)" + std::to_string(binary.size()) + R"(}],"bufferViews":[)" +
      R"({"buffer":0,"byteOffset":0,"byteLength":48},{"buffer":0,"byteOffset":48,"byteLength":32},)" +
      R"({"buffer":0,"byteOffset":80,"byteLength":12},{"buffer":0,"byteOffset":92,"byteLength":)" + std::to_string(image.size()) + R"(}],)" +
      R"("accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},)" +
      R"({"bufferView":1,"componentType":5126,"count":4,"type":"VEC2"},{"bufferView":2,"componentType":5123,"count":6,"type":"SCALAR"}],)" +
      R"("images":[{"bufferView":3,"mimeType":"image/ktx2"}],"textures":[{"extensions":{"KHR_texture_basisu":{"source":0}}}],)" +
      R"("materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],)" +
      R"("meshes":[{"name":"Tela","primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":1},"indices":2,"material":0}]}],)" +
      R"("nodes":[{"name":"Tela","mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})";
  return buildGlb(json, binary);
}
} // namespace

AE_TEST(m09e4_ktx2_with_full_mips_becomes_astc_blocks_only_when_the_device_samples_astc) {
  std::vector<u8> chain;
  u32 width = 0, height = 0, levels = 0;
  std::string diagnostic;
  AE_EXPECT_TRUE(resources::transcodeKtx2Astc4x4(kGradUastcMips, 0, chain, width, height, levels, diagnostic), diagnostic.c_str());
  AE_EXPECT_TRUE(width == 8 && height == 8 && levels == 4 && chain.size() == 112,
                 "quatro níveis de blocos ASTC 4x4: 4 + 1 + 1 + 1 blocos de 16 bytes");
  AE_EXPECT_TRUE(resources::transcodeKtx2Astc4x4(kGradUastcMips, 1, chain, width, height, levels, diagnostic) && width == 4 &&
                     levels == 3 && chain.size() == 48,
                 "o nível residente descarta os mips de cima");
  AE_EXPECT_TRUE(!resources::transcodeKtx2Astc4x4(kGradUastcMips, 4, chain, width, height, levels, diagnostic) && chain.empty(),
                 "nível inexistente recusado");

  resources::GltfImportLimits astc;
  astc.astc4x4 = true;
  resources::GltfImport compressed;
  AE_EXPECT_TRUE(resources::importGlb(ktx2Quad(kGradUastcMips), astc, {}, compressed), compressed.diagnostic.c_str());
  AE_EXPECT_EQ(compressed.astcTextures, 1u, "a textura subiu como ASTC");
  AE_EXPECT_TRUE(!compressed.textures.empty() && compressed.textures.front()->format == renderer::AuthoringTextureAstc4x4 &&
                     compressed.textures.front()->levels == 4 && compressed.textures.front()->valid() &&
                     compressed.textureBytes == 112,
                 "blocos com os mips do arquivo, contados pelo tamanho comprimido");

  resources::GltfImport plain;
  AE_EXPECT_TRUE(resources::importGlb(ktx2Quad(kGradUastcMips), {}, {}, plain), plain.diagnostic.c_str());
  AE_EXPECT_TRUE(plain.astcTextures == 0 && !plain.textures.empty() &&
                     plain.textures.front()->format == renderer::AuthoringTextureRgba8 && plain.textureBytes == 340,
                 "sem ASTC no aparelho: RGBA8 como antes (64+16+4+1 texels)");

  resources::GltfImport noMips;
  AE_EXPECT_TRUE(resources::importGlb(ktx2Quad(kGradUastc), astc, {}, noMips), noMips.diagnostic.c_str());
  bool noted = false;
  for (const auto &note : noMips.textureNotes) noted |= note.find("cadeia completa") != std::string::npos;
  AE_EXPECT_TRUE(noMips.astcTextures == 0 && !noMips.textures.empty() &&
                     noMips.textures.front()->format == renderer::AuthoringTextureRgba8 && noted,
                 "KTX2 sem mips cai para RGBA8, com o motivo");

  astc.maximumTextureDimension = 4;
  resources::GltfImport capped;
  AE_EXPECT_TRUE(resources::importGlb(ktx2Quad(kGradUastcMips), astc, {}, capped), capped.diagnostic.c_str());
  AE_EXPECT_TRUE(!capped.textures.empty() && capped.textures.front()->width == 4 && capped.textures.front()->levels == 3 &&
                     capped.reducedTextures == 1,
                 "limite residente vale para ASTC descartando níveis");
}

AE_TEST(r2_import_cache_round_trips_the_prepared_model_and_refuses_stale_or_corrupt_files) {
  resources::GltfImportLimits astc;
  astc.astc4x4 = true;
  const std::string sourceHash(64, 'a');
  const auto key = resources::importCacheKey(sourceHash, astc);
  AE_EXPECT_EQ(key.size(), usize{64}, "chave SHA-256 em hexadecimal");
  AE_EXPECT_TRUE(key != resources::importCacheKey(sourceHash, {}), "outro formato alvo, outra chave");
  AE_EXPECT_TRUE(key != resources::importCacheKey(std::string(64, 'b'), astc), "outra fonte, outra chave");

  // Modelo com texturas ASTC e mipmaps.
  resources::GltfImport textured;
  AE_EXPECT_TRUE(resources::importGlb(ktx2Quad(kGradUastcMips), astc, {}, textured), textured.diagnostic.c_str());
  std::vector<u8> bytes;
  AE_EXPECT_TRUE(resources::writeImportCache(textured, key, bytes), "cache gravado");
  resources::GltfImport restored;
  AE_EXPECT_TRUE(resources::readImportCache(bytes, key, restored), "cache lido");
  std::vector<u8> again;
  AE_EXPECT_TRUE(resources::writeImportCache(restored, key, again) && again == bytes,
                 "o modelo lido grava os mesmos bytes: nada se perdeu na volta");
  AE_EXPECT_TRUE(restored.textures.size() == 1 && restored.textures.front()->format == renderer::AuthoringTextureAstc4x4 &&
                     restored.textures.front()->mipChain == textured.textures.front()->mipChain && restored.astcTextures == 1,
                 "textura ASTC com a cadeia inteira e os contadores da prévia");

  // Modelo com hierarquia e nós espelhados.
  const auto mirrored = buildGlb(mirrorJson(R"({"name":"Espelho","mesh":0,"translation":[5,0,0],"scale":[-1,1,1],"children":[1]},)"
                                            R"({"name":"Filho","mesh":0,"translation":[2,0,0]},{"name":"Direto","mesh":0})",
                                            "0,2"),
                                 mirrorBinary());
  resources::GltfImport tree;
  AE_EXPECT_TRUE(resources::importGlb(mirrored, {}, {}, tree), tree.diagnostic.c_str());
  const auto treeKey = resources::importCacheKey(sourceHash, {});
  std::vector<u8> treeBytes, treeAgain;
  resources::GltfImport treeRestored;
  AE_EXPECT_TRUE(resources::writeImportCache(tree, treeKey, treeBytes) && resources::readImportCache(treeBytes, treeKey, treeRestored) &&
                     resources::writeImportCache(treeRestored, treeKey, treeAgain) && treeAgain == treeBytes,
                 "árvore, pais, matrizes e geometria espelhada voltam iguais");
  AE_EXPECT_TRUE(treeRestored.nodes.size() == 3 && treeRestored.nodes[1].parent == 0 && treeRestored.mirroredNodes == 2,
                 "hierarquia e contador de espelhamento");

  resources::GltfImport refused;
  AE_EXPECT_TRUE(!resources::readImportCache(bytes, treeKey, refused) && refused.draws.empty(), "chave de outra configuração recusada");
  std::vector<u8> truncated(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(bytes.size() / 2));
  AE_EXPECT_TRUE(!resources::readImportCache(truncated, key, refused) && refused.draws.empty(), "arquivo cortado recusado");
  std::vector<u8> corrupt = bytes;
  corrupt[3] ^= 0xff;
  AE_EXPECT_TRUE(!resources::readImportCache(corrupt, key, refused), "cabeçalho corrompido recusado");
  std::vector<u8> tail = bytes;
  tail.back() ^= 0xff;
  AE_EXPECT_TRUE(!resources::readImportCache(tail, key, refused), "terminador corrompido recusado");
  AE_EXPECT_TRUE(resources::importCacheRelativePath(key) == ".astra/cache/imports/" + key + ".aic", "caminho do derivado");
}

AE_TEST(m09e4_basisu_only_texture_is_applied_through_the_same_texture_pipeline) {
  std::vector<u8> binary;
  const float positions[12]{0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0};
  const float uvs[8]{0, 0, 1, 0, 1, 1, 0, 1};
  const u16 indices[6]{0, 1, 2, 0, 2, 3};
  binary.resize(92);
  std::memcpy(binary.data(), positions, 48);
  std::memcpy(binary.data() + 48, uvs, 32);
  std::memcpy(binary.data() + 80, indices, 12);
  binary.insert(binary.end(), kGradUastc.begin(), kGradUastc.end());
  pad4(binary);
  const std::string json =
      std::string(R"({"asset":{"version":"2.0"},"extensionsUsed":["KHR_texture_basisu"],"extensionsRequired":["KHR_texture_basisu"],)") +
      R"("buffers":[{"byteLength":)" + std::to_string(binary.size()) + R"(}],"bufferViews":[)" +
      R"({"buffer":0,"byteOffset":0,"byteLength":48},{"buffer":0,"byteOffset":48,"byteLength":32},)" +
      R"({"buffer":0,"byteOffset":80,"byteLength":12},{"buffer":0,"byteOffset":92,"byteLength":)" +
      std::to_string(kGradUastc.size()) + R"(}],)" +
      R"("accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},)" +
      R"({"bufferView":1,"componentType":5126,"count":4,"type":"VEC2"},{"bufferView":2,"componentType":5123,"count":6,"type":"SCALAR"}],)" +
      R"("images":[{"bufferView":3,"mimeType":"image/ktx2"}],"textures":[{"extensions":{"KHR_texture_basisu":{"source":0}}}],)" +
      R"("materials":[{"name":"Grade","pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],)" +
      R"("meshes":[{"name":"Tela","primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":1},"indices":2,"material":0}]}],)" +
      R"("nodes":[{"name":"Tela","mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})";
  resources::GltfImport model;
  AE_EXPECT_TRUE(resources::importGlb(buildGlb(json, binary), {}, {}, model), model.diagnostic.c_str());
  AE_EXPECT_EQ(model.ktx2Images, 1u, "a imagem KTX2 foi transcodificada");
  AE_EXPECT_EQ(model.textures.size(), usize{1}, "uma textura aplicada");
  AE_EXPECT_EQ(model.skippedTextures, 0u, "nada ficou de fora");
  AE_EXPECT_TRUE(!model.materials.empty() && model.materials.front().textureIndices[0] == 0, "cor base aponta para a textura KTX2");
  AE_EXPECT_TRUE(!model.textures.empty() && model.textures.front()->srgb && model.textures.front()->width == 8 &&
                     model.textures.front()->levels == 4 && model.textures.front()->valid(),
                 "sRGB, 8 px e mips gerados pelo mesmo caminho do PNG");
  AE_EXPECT_TRUE(std::find(model.appearanceExtensions.begin(), model.appearanceExtensions.end(), "KHR_texture_basisu") ==
                     model.appearanceExtensions.end(),
                 "KTX2 deixou de ser listado como omissão");
}
