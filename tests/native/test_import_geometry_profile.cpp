// Normais e tangentes derivadas na importação, como escolha autoral.
//
// O caso que motiva isto é concreto: o glTF permite uma primitiva sem NORMAL —
// o cliente deve sombrear pela face — e este renderer lê a normal do vértice.
// Sem gerar nada, a malha chega com normal (0,0,0) e some no sombreamento. O
// perfil passa a decidir entre usar o que veio no arquivo e recalcular.
#include "harness.h"

#include "resources/gltf_import.h"

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

using namespace ae;
using namespace ae::resources;

namespace {
void appendU32(std::vector<u8> &out, u32 value) {
  for (u32 i = 0; i < 4; ++i) out.push_back(static_cast<u8>(value >> (i * 8)));
}
void appendFloat(std::vector<u8> &out, float value) {
  u32 bits = 0;
  std::memcpy(&bits, &value, 4);
  appendU32(out, bits);
}
std::vector<u8> buildGlb(const std::string &json, const std::vector<u8> &binary) {
  std::string paddedJson = json;
  while (paddedJson.size() % 4) paddedJson.push_back(' ');
  std::vector<u8> paddedBinary = binary;
  while (paddedBinary.size() % 4) paddedBinary.push_back(0);
  std::vector<u8> glb;
  appendU32(glb, 0x46546C67);
  appendU32(glb, 2);
  appendU32(glb, static_cast<u32>(12 + 8 + paddedJson.size() + (paddedBinary.empty() ? 0 : 8 + paddedBinary.size())));
  appendU32(glb, static_cast<u32>(paddedJson.size()));
  appendU32(glb, 0x4E4F534A);
  glb.insert(glb.end(), paddedJson.begin(), paddedJson.end());
  if (!paddedBinary.empty()) {
    appendU32(glb, static_cast<u32>(paddedBinary.size()));
    appendU32(glb, 0x004E4942);
    glb.insert(glb.end(), paddedBinary.begin(), paddedBinary.end());
  }
  return glb;
}

// Um triângulo no plano XY com enrolamento anti-horário: a normal geométrica é
// +Z. `fileNormals` decide se o arquivo declara NORMAL e com que valor.
struct Asset {
  std::string json;
  std::vector<u8> binary;
};
Asset triangle(bool withNormals, float normalZ = 1.0f, bool withUv = false) {
  std::vector<u8> binary;
  const float positions[9]{0, 0, 0, 1, 0, 0, 0, 1, 0};
  for (float value : positions) appendFloat(binary, value);
  for (u32 v = 0; v < 3; ++v) { appendFloat(binary, 0); appendFloat(binary, 0); appendFloat(binary, normalZ); }
  for (u32 v = 0; v < 3; ++v) { appendFloat(binary, 0); appendFloat(binary, 0); }
  const u16 indices[3]{0, 1, 2};
  for (u16 value : indices) { binary.push_back(static_cast<u8>(value)); binary.push_back(static_cast<u8>(value >> 8)); }
  while (binary.size() % 4) binary.push_back(0);

  std::string attributes = R"("POSITION":0)";
  if (withNormals) attributes += R"(,"NORMAL":1)";
  if (withUv) attributes += R"(,"TEXCOORD_0":2)";
  std::string json = std::string(R"({"asset":{"version":"2.0"},)") +
    R"("buffers":[{"byteLength":)" + std::to_string(binary.size()) + R"(}],)" +
    R"("bufferViews":[)"
      R"({"buffer":0,"byteOffset":0,"byteLength":36},)"
      R"({"buffer":0,"byteOffset":36,"byteLength":36},)"
      R"({"buffer":0,"byteOffset":72,"byteLength":24},)"
      R"({"buffer":0,"byteOffset":96,"byteLength":6}],)" +
    R"("accessors":[)"
      R"({"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},)"
      R"({"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},)"
      R"({"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"},)"
      R"({"bufferView":3,"componentType":5123,"count":3,"type":"SCALAR"}],)" +
    R"("meshes":[{"primitives":[{"attributes":{)" + attributes + R"(},"indices":3,"material":0}]}],)" +
    R"("materials":[{"pbrMetallicRoughness":{"baseColorFactor":[1,1,1,1]}}],)" +
    R"("nodes":[{"name":"Triangulo","mesh":0}],)" +
    R"("scenes":[{"nodes":[0]}],"scene":0})";
  return {json, binary};
}

// Um quadrado de dois triângulos com a MESMA área no mundo e áreas de UV muito
// diferentes: é a forma mínima de "textura esticada" que sobrevive a qualquer
// resolução de imagem. O material declara uma textura para o diagnóstico valer.
Asset stretchedQuad() {
  std::vector<u8> binary;
  const float positions[12]{0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0};
  for (float value : positions) appendFloat(binary, value);
  const float uv[8]{0, 0, 0.01f, 0, 0.01f, 0.01f, 0, 1};
  for (float value : uv) appendFloat(binary, value);
  const u16 indices[6]{0, 1, 2, 0, 2, 3};
  for (u16 value : indices) { binary.push_back(static_cast<u8>(value)); binary.push_back(static_cast<u8>(value >> 8)); }
  while (binary.size() % 4) binary.push_back(0);

  std::string json = std::string(R"({"asset":{"version":"2.0"},)") +
    R"("buffers":[{"byteLength":)" + std::to_string(binary.size()) + R"(}],)" +
    R"("bufferViews":[)"
      R"({"buffer":0,"byteOffset":0,"byteLength":48},)"
      R"({"buffer":0,"byteOffset":48,"byteLength":32},)"
      R"({"buffer":0,"byteOffset":80,"byteLength":12}],)" +
    R"("accessors":[)"
      R"({"bufferView":0,"componentType":5126,"count":4,"type":"VEC3"},)"
      R"({"bufferView":1,"componentType":5126,"count":4,"type":"VEC2"},)"
      R"({"bufferView":2,"componentType":5123,"count":6,"type":"SCALAR"}],)" +
    R"("images":[{"mimeType":"image/png","bufferView":0}],"textures":[{"source":0}],)" +
    R"("meshes":[{"primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":1},"indices":2,"material":0}]}],)" +
    R"("materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],)" +
    R"("nodes":[{"name":"Quadrado","mesh":0}],)" +
    R"("scenes":[{"nodes":[0]}],"scene":0})";
  return {json, binary};
}

// Lê a normal do vértice `v` do primeiro desenho, desempacotada de snorm16.
void readNormal(const GltfImport &model, u32 v, float out[3]) {
  const u8 *vertex = model.vertices.data() + static_cast<usize>(v) * renderer::MapVertexStride + 12;
  i16 packed[4]{};
  std::memcpy(packed, vertex, 8);
  for (u32 axis = 0; axis < 3; ++axis) out[axis] = packed[axis] / 32767.0f;
}
} // namespace

AE_TEST(import_generates_normals_when_the_file_has_none) {
  const auto asset = triangle(false);
  GltfImport model;
  AE_EXPECT_TRUE(importGlb(buildGlb(asset.json, asset.binary), {}, {}, model), model.diagnostic.c_str());
  AE_EXPECT_EQ(model.generatedNormalPrimitives, 1u, "a primitiva sem NORMAL teve normais calculadas");
  float normal[3];
  readNormal(model, 0, normal);
  AE_EXPECT_TRUE(std::fabs(normal[2] - 1.0f) < 0.01f, "a normal segue o enrolamento do triângulo (+Z)");
  AE_EXPECT_TRUE(std::fabs(normal[0]) < 0.01f && std::fabs(normal[1]) < 0.01f, "sem componente lateral");
}

AE_TEST(import_keeps_file_normals_by_default) {
  // Normais apontando para -Z, ao contrário do enrolamento: é uma escolha do
  // autor da fonte e o padrão do perfil precisa respeitá-la.
  const auto asset = triangle(true, -1.0f);
  GltfImport model;
  AE_EXPECT_TRUE(importGlb(buildGlb(asset.json, asset.binary), {}, {}, model), model.diagnostic.c_str());
  AE_EXPECT_EQ(model.generatedNormalPrimitives, 0u, "nada foi recalculado");
  float normal[3];
  readNormal(model, 0, normal);
  AE_EXPECT_TRUE(normal[2] < -0.9f, "a normal do arquivo sobreviveu");
}

AE_TEST(import_recalculates_normals_when_the_profile_asks) {
  const auto asset = triangle(true, -1.0f);
  GltfImportLimits limits;
  limits.normals = GltfNormalsCalculate;
  GltfImport model;
  AE_EXPECT_TRUE(importGlb(buildGlb(asset.json, asset.binary), limits, {}, model), model.diagnostic.c_str());
  AE_EXPECT_EQ(model.generatedNormalPrimitives, 1u, "o perfil pediu recálculo");
  float normal[3];
  readNormal(model, 0, normal);
  AE_EXPECT_TRUE(normal[2] > 0.9f, "a normal calculada segue a geometria, não o arquivo");
}

AE_TEST(import_angle_weighting_still_produces_unit_normals) {
  const auto asset = triangle(false);
  GltfImportLimits limits;
  limits.normalWeighting = GltfNormalWeightAngle;
  GltfImport model;
  AE_EXPECT_TRUE(importGlb(buildGlb(asset.json, asset.binary), limits, {}, model), model.diagnostic.c_str());
  for (u32 v = 0; v < 3; ++v) {
    float normal[3];
    readNormal(model, v, normal);
    const float length = std::hypot(normal[0], normal[1], normal[2]);
    AE_EXPECT_TRUE(std::fabs(length - 1.0f) < 0.01f, "a ponderação por ângulo devolve normal unitária");
  }
}

AE_TEST(import_reports_textured_primitive_without_uv) {
  // Material com mapa de cor base e primitiva sem TEXCOORD: não há mapeamento
  // possível, e o relatório precisa nomear isso em vez de deixar a textura
  // aparecer esticada sem explicação.
  auto asset = triangle(true, 1.0f, false);
  const auto needle = std::string(R"("baseColorFactor":[1,1,1,1])");
  asset.json.replace(asset.json.find(needle), needle.size(),
                     R"("baseColorFactor":[1,1,1,1],"baseColorTexture":{"index":0})");
  asset.json.insert(asset.json.find(R"("materials":)"),
                    R"("images":[{"mimeType":"image/png","bufferView":0}],"textures":[{"source":0}],)");
  GltfImport model;
  // A imagem é inválida de propósito: o que se mede aqui é o diagnóstico de UV,
  // e ele não pode depender de a textura ter decodificado.
  const bool imported = importGlb(buildGlb(asset.json, asset.binary), {}, {}, model);
  AE_EXPECT_TRUE(imported, model.diagnostic.c_str());
  AE_EXPECT_EQ(model.texturedPrimitivesWithoutUv, 1u, "a primitiva texturizada sem UV foi contada");
}

AE_TEST(import_reports_uneven_texel_density_as_a_source_problem) {
  const auto asset = stretchedQuad();
  GltfImport model;
  AE_EXPECT_TRUE(importGlb(buildGlb(asset.json, asset.binary), {}, {}, model), model.diagnostic.c_str());
  AE_EXPECT_EQ(model.stretchedUvPrimitives, 1u, "a primitiva com densidade desigual foi contada");
  AE_EXPECT_TRUE(model.worstTexelDensityRatio > 8.0f, "a pior razão observada é publicada para o relatório");
}

AE_TEST(import_does_not_flag_even_texel_density) {
  auto asset = stretchedQuad();
  // Mesma geometria, UV proporcional: a densidade é igual nos dois triângulos.
  const auto needle = std::string(R"({"buffer":0,"byteOffset":48,"byteLength":32})");
  AE_EXPECT_TRUE(asset.json.find(needle) != std::string::npos, "fixtura encontrada");
  const float uv[8]{0, 0, 1, 0, 1, 1, 0, 1};
  std::memcpy(asset.binary.data() + 48, uv, sizeof uv);
  GltfImport model;
  AE_EXPECT_TRUE(importGlb(buildGlb(asset.json, asset.binary), {}, {}, model), model.diagnostic.c_str());
  AE_EXPECT_EQ(model.stretchedUvPrimitives, 0u, "UV proporcional não é apontada como defeito");
}
