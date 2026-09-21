// Normais e tangentes derivadas na importação, como escolha autoral.
//
// O caso que motiva isto é concreto: o glTF permite uma primitiva sem NORMAL —
// o cliente deve sombrear pela face — e este renderer lê a normal do vértice.
// Sem gerar nada, a malha chega com normal (0,0,0) e some no sombreamento. O
// perfil passa a decidir entre usar o que veio no arquivo e recalcular.
#include "harness.h"

#include "resources/gltf_import.h"
#include "resources/import_profile.h"
#include "resources/mesh_derived.h"
#include "resources/import_node_map.h"
#include "editor/editor_import_reconcile.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
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

namespace {
// Um no com camera perspectiva e nada mais, do jeito que um exportador produz:
// a camera fica sozinha no no, e a malha vive em outro.
Asset cameraScene(const char *cameraJson, bool cameraOnMeshNode = false) {
  auto asset = triangle(true);
  const auto meshNode = std::string(R"("nodes":[{"name":"Triangulo","mesh":0}])");
  const auto replacement = cameraOnMeshNode
      ? std::string(R"("nodes":[{"name":"Triangulo","mesh":0,"camera":0}])")
      : std::string(R"("nodes":[{"name":"Triangulo","mesh":0},{"name":"Camera","camera":0,"translation":[0,2,-5]}])");
  asset.json.replace(asset.json.find(meshNode), meshNode.size(), replacement);
  const auto scene = std::string(R"("scenes":[{"nodes":[0]}])");
  asset.json.replace(asset.json.find(scene), scene.size(),
                     cameraOnMeshNode ? scene : std::string(R"("scenes":[{"nodes":[0,1]}])"));
  asset.json.insert(asset.json.find(R"("meshes":)"), std::string(R"("cameras":[)") + cameraJson + "],");
  return asset;
}

Asset lightScene() {
  auto asset = triangle(true);
  const auto meshNode = std::string(R"("nodes":[{"name":"Triangulo","mesh":0}])");
  const auto replacement = std::string(
      R"("nodes":[{"name":"Triangulo","mesh":0},{"name":"Luz pontual","translation":[1,2,3],"extensions":{"KHR_lights_punctual":{"light":0}}},{"name":"Spot","extensions":{"KHR_lights_punctual":{"light":1}}}])");
  asset.json.replace(asset.json.find(meshNode), meshNode.size(), replacement);
  const auto scene = std::string(R"("scenes":[{"nodes":[0]}])");
  asset.json.replace(asset.json.find(scene), scene.size(), std::string(R"("scenes":[{"nodes":[0,1,2]}])"));
  asset.json.insert(asset.json.find(R"("meshes":)"),
      R"("extensionsUsed":["KHR_lights_punctual"],"extensions":{"KHR_lights_punctual":{"lights":[{"type":"point","color":[0.25,0.5,1],"intensity":800,"range":12},{"type":"spot","intensity":1500,"range":30,"spot":{"innerConeAngle":0.2,"outerConeAngle":0.6}}]}},)");
  return asset;
}
} // namespace

AE_TEST(import_leaves_cameras_out_until_the_profile_asks) {
  const auto asset = cameraScene(R"({"type":"perspective","perspective":{"yfov":0.7,"znear":0.2,"zfar":300}})");
  GltfImport model;
  AE_EXPECT_TRUE(importGlb(buildGlb(asset.json, asset.binary), {}, {}, model), model.diagnostic.c_str());
  AE_EXPECT_TRUE(model.cameras.empty(), "sem o perfil, a câmera não entra");
  AE_EXPECT_EQ(model.skippedCameras, 1u, "e continua contada como perdida, para o autor saber que existe");
}

AE_TEST(import_brings_the_camera_lens_and_turns_it_to_engine_forward) {
  const auto asset = cameraScene(R"({"type":"perspective","perspective":{"yfov":0.7,"znear":0.2,"zfar":300}})");
  GltfImportLimits limits;
  limits.importCameras = true;
  GltfImport model;
  AE_EXPECT_TRUE(importGlb(buildGlb(asset.json, asset.binary), limits, {}, model), model.diagnostic.c_str());
  AE_EXPECT_EQ(model.cameras.size(), 1u, "a câmera do arquivo entrou");
  AE_EXPECT_EQ(model.skippedCameras, 0u, "e não é mais perda");
  const auto &camera = model.cameras.front();
  AE_EXPECT_TRUE(!camera.orthographic, "perspectiva");
  AE_EXPECT_TRUE(std::fabs(camera.verticalFovDegrees - 40.1070f) < 0.01f, "yfov em radianos vira graus");
  AE_EXPECT_TRUE(camera.nearPlane == 0.2f && camera.farPlane == 300.f, "os planos vêm do arquivo");

  // O glTF olha para -Z e esta engine para +Z: a pose do nó leva meia volta,
  // senão a câmera importada enquadra o lado oposto ao que o autor escolheu.
  const auto &matrix = model.nodes[camera.node].localMatrix;
  AE_EXPECT_TRUE(matrix[10] == -1.f, "a coluna Z foi invertida");
  AE_EXPECT_TRUE(matrix[0] == -1.f, "e a X junto, para a base seguir destra");
  AE_EXPECT_TRUE(matrix[5] == 1.f, "o eixo Y e a posição não mudam");
  AE_EXPECT_TRUE(matrix[13] == 2.f && matrix[14] == -5.f, "a posição do nó é preservada");
}

AE_TEST(import_reads_an_orthographic_camera_by_its_own_field) {
  const auto asset = cameraScene(R"({"type":"orthographic","orthographic":{"xmag":4,"ymag":3,"znear":0.1,"zfar":100}})");
  GltfImportLimits limits;
  limits.importCameras = true;
  GltfImport model;
  AE_EXPECT_TRUE(importGlb(buildGlb(asset.json, asset.binary), limits, {}, model), model.diagnostic.c_str());
  AE_EXPECT_EQ(model.cameras.size(), 1u, "a câmera ortográfica entrou");
  AE_EXPECT_TRUE(model.cameras.front().orthographic, "o tipo do arquivo é respeitado");
  AE_EXPECT_TRUE(model.cameras.front().orthographicHalfHeight == 3.f, "ymag é a meia altura visível");
}

AE_TEST(import_refuses_a_camera_that_would_rotate_geometry_with_it) {
  // Câmera no MESMO nó da malha: girar o nó para acertar o enquadramento giraria
  // a geometria do autor junto. A câmera fica de fora, com motivo.
  const auto asset = cameraScene(R"({"type":"perspective","perspective":{"yfov":0.7,"znear":0.2}})", true);
  GltfImportLimits limits;
  limits.importCameras = true;
  GltfImport model;
  AE_EXPECT_TRUE(importGlb(buildGlb(asset.json, asset.binary), limits, {}, model), model.diagnostic.c_str());
  AE_EXPECT_TRUE(model.cameras.empty(), "a câmera no nó com geometria não entra");
  AE_EXPECT_EQ(model.skippedCameras, 1u, "e é contada como não importada");
  AE_EXPECT_TRUE(!model.notes.empty(), "com o motivo escrito no relatório");
  AE_EXPECT_TRUE(model.nodes[0].localMatrix[10] == 1.f, "e a geometria não foi girada");
}

AE_TEST(import_refuses_a_camera_with_an_impossible_lens) {
  const auto asset = cameraScene(R"({"type":"perspective","perspective":{"yfov":0.7,"znear":10,"zfar":1}})");
  GltfImportLimits limits;
  limits.importCameras = true;
  GltfImport model;
  AE_EXPECT_TRUE(!importGlb(buildGlb(asset.json, asset.binary), limits, {}, model),
                 "plano distante atrás do próximo é arquivo inválido, não valor a corrigir");
}

AE_TEST(import_lights_are_explicit_profile_data_with_photometric_values_and_orientation) {
  const auto asset = lightScene();
  GltfImport model;
  AE_EXPECT_TRUE(importGlb(buildGlb(asset.json, asset.binary), {}, {}, model), model.diagnostic.c_str());
  AE_EXPECT_TRUE(model.lights.empty(), "perfil antigo não cria componentes novos");
  AE_EXPECT_EQ(model.skippedLights, 2u, "as luzes omitidas continuam visíveis no relatório");

  GltfImportLimits limits;limits.importLights=true;
  AE_EXPECT_TRUE(importGlb(buildGlb(asset.json, asset.binary), limits, {}, model), model.diagnostic.c_str());
  AE_EXPECT_EQ(model.lights.size(), 2u, "pontual e spot entram como dados autorais");
  AE_EXPECT_EQ(model.skippedLights, 0u, "nenhuma luz válida foi perdida");
  const auto &point=model.lights[0];
  AE_EXPECT_TRUE(point.kind==1 && point.intensity==800 && point.range==12,"unidade, intensidade e alcance preservados");
  AE_EXPECT_TRUE(point.color[0]==.25f && point.color[1]==.5f && point.color[2]==1,"cor linear preservada");
  AE_EXPECT_TRUE(model.nodes[point.node].localMatrix[12]==1 && model.nodes[point.node].localMatrix[13]==2 &&
                 model.nodes[point.node].localMatrix[14]==3,"a pose da luz pontual é a do nó");
  const auto &spot=model.lights[1];
  AE_EXPECT_TRUE(spot.kind==2 && std::fabs(spot.innerAngle-11.4592f)<.01f &&
                 std::fabs(spot.outerAngle-34.3775f)<.01f,"cones em radianos viram meio-ângulos em graus");
  AE_EXPECT_TRUE(model.nodes[spot.node].localMatrix[0]==-1 && model.nodes[spot.node].localMatrix[10]==-1,
                 "o -Z do glTF vira o +Z da luz Astra");

  ImportNodeMap map;ImportMatchReport report;std::string diagnostic;
  const auto source=assetGuidFromSeed("teste:luzes-pontuais");
  AE_EXPECT_TRUE(buildImportNodeMap(model,source,"conteudo",nullptr,ImportAmbiguityPolicy::Refuse,map,report,diagnostic),
                 diagnostic.c_str());
  AE_EXPECT_TRUE(map.nodes[point.node].light && map.nodes[spot.node].light,"o mapa persistente transporta a autoria");
  editor::EditorEntity entity;
  editor::applyImportedNodeComponents(entity,map.nodes[point.node]);
  const auto *component=static_cast<const scene::Light*>(entity.components.find(scene::Light::descriptor));
  AE_EXPECT_TRUE(component && component->unit==scene::LightUnit::LuxCandela && component->kind==scene::LightKind::Point,
                 "o nó cria o componente que o runtime consome");
  AE_EXPECT_TRUE(component->intensity==800 && component->range==12,"o componente recebe os valores fotométricos");
}

AE_TEST(import_reads_the_official_khronos_point_light_glb) {
  std::ifstream input("tests/native/fixtures/gltf/PointLightIntensityTest.glb",std::ios::binary);
  AE_EXPECT_TRUE(static_cast<bool>(input),"o corpus oficial está disponível");
  const std::vector<u8> bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
  GltfImportLimits limits;limits.importLights=true;
  GltfImport model;
  AE_EXPECT_TRUE(importGlb(bytes,limits,{},model),model.diagnostic.c_str());
  AE_EXPECT_TRUE(!model.lights.empty(),"o GLB oficial materializa as luzes pontuais");
  AE_EXPECT_EQ(model.skippedLights,0u,"nenhuma luz oficial válida foi descartada");
  AE_EXPECT_TRUE(std::all_of(model.lights.begin(),model.lights.end(),[](const auto &light) {
    return light.kind==1 && light.intensity==1.0f;
  }),"o teste oficial preserva tipo point e intensidade fotométrica 1");
  AE_EXPECT_TRUE(std::find(model.appearanceExtensions.begin(),model.appearanceExtensions.end(),
                           "KHR_lights_punctual")==model.appearanceExtensions.end(),
                 "a extensão implementada não é relatada como aparência perdida");
}

namespace {
// Cubo unitário com 8 vértices compartilhados e sem NORMAL — a forma em que
// um exportador entrega "caixa de quinas vivas" e o importador precisa decidir
// se a quina é aresta dura ou superfície curva.
Asset sharedCube() {
  std::vector<u8> binary;
  const float corners[24]{-.5f, -.5f, -.5f, .5f, -.5f, -.5f, .5f, .5f, -.5f, -.5f, .5f, -.5f,
                          -.5f, -.5f, .5f,  .5f, -.5f, .5f,  .5f, .5f, .5f,  -.5f, .5f, .5f};
  for (float value : corners) appendFloat(binary, value);
  const u16 faces[36]{0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4,
                      3, 6, 2, 3, 7, 6, 0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5};
  for (u16 value : faces) { binary.push_back(static_cast<u8>(value)); binary.push_back(static_cast<u8>(value >> 8)); }
  std::string json = std::string(R"({"asset":{"version":"2.0"},)") +
    R"("buffers":[{"byteLength":)" + std::to_string(binary.size()) + R"(}],)" +
    R"("bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":96},{"buffer":0,"byteOffset":96,"byteLength":72}],)" +
    R"("accessors":[{"bufferView":0,"componentType":5126,"count":8,"type":"VEC3"},)"
    R"({"bufferView":1,"componentType":5123,"count":36,"type":"SCALAR"}],)" +
    R"("meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"material":0}]}],)" +
    R"("materials":[{"pbrMetallicRoughness":{"baseColorFactor":[1,1,1,1]}}],)" +
    R"("nodes":[{"name":"Caixa","mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})";
  return {json, binary};
}
} // namespace

AE_TEST(smoothing_angle_splits_the_hard_edges_of_a_shared_vertex_cube) {
  // 60° (o padrão da Unity): as quinas de 90° ficam duras. Cada canto passa a
  // ter a normal só da própria face — 24 vértices, normais nos eixos.
  const auto asset = sharedCube();
  GltfImportLimits limits;
  limits.smoothingAngle = GltfSmoothingAngleDefault;
  GltfImport model;
  AE_EXPECT_TRUE(importGlb(buildGlb(asset.json, asset.binary), limits, {}, model), model.diagnostic.c_str());
  const auto vertices = model.vertices.size() / renderer::MapVertexStride;
  AE_EXPECT_EQ(vertices, usize{24}, "8 quinas × 3 faces: cada lado da aresta dura com o próprio vértice");
  for (usize v = 0; v < vertices; ++v) {
    float normal[3];
    readNormal(model, static_cast<u32>(v), normal);
    const float biggest = std::max({std::fabs(normal[0]), std::fabs(normal[1]), std::fabs(normal[2])});
    AE_EXPECT_TRUE(biggest > .99f, "normal no eixo da face, não na diagonal da quina");
  }
  // O desenho continua o mesmo: 12 triângulos, agora sobre os vértices divididos.
  AE_EXPECT_EQ(model.draws.front().indexCount, 36u, "a topologia de triângulos não muda");

  // 180°: tudo suave, como antes do campo — 8 vértices e normais diagonais.
  limits.smoothingAngle = GltfSmoothingAngleNone;
  GltfImport smooth;
  AE_EXPECT_TRUE(importGlb(buildGlb(asset.json, asset.binary), limits, {}, smooth), smooth.diagnostic.c_str());
  AE_EXPECT_EQ(smooth.vertices.size() / renderer::MapVertexStride, usize{8}, "sem divisão");
  float corner[3];
  readNormal(smooth, 0, corner);
  AE_EXPECT_TRUE(std::fabs(std::fabs(corner[0]) - .577f) < .02f, "a normal da quina é a média das três faces");
}

AE_TEST(smoothing_angle_keeps_a_flat_surface_in_one_piece) {
  // Dois triângulos coplanares não têm aresta dura em ângulo nenhum, nem 0°.
  const auto asset = stretchedQuad();
  GltfImportLimits limits;
  limits.smoothingAngle = 0;
  GltfImport model;
  AE_EXPECT_TRUE(importGlb(buildGlb(asset.json, asset.binary), limits, {}, model), model.diagnostic.c_str());
  AE_EXPECT_EQ(model.vertices.size() / renderer::MapVertexStride, usize{4}, "o plano não se parte");
}

AE_TEST(collision_mesh_recipe_simplifies_without_changing_the_visual_mesh) {
  std::vector<u8> vertices(11 * 11 * renderer::MapVertexStride);
  for(u32 y=0;y<=10;++y) for(u32 x=0;x<=10;++x) {
    const float position[3]{static_cast<float>(x),0,static_cast<float>(y)};
    std::memcpy(vertices.data()+(y*11+x)*renderer::MapVertexStride,position,sizeof position);
  }
  std::vector<u32> indices;
  for(u32 y=0;y<10;++y) for(u32 x=0;x<10;++x) {
    const u32 a=y*11+x,b=a+1,c=a+11,d=c+1;
    indices.insert(indices.end(),{a,c,b,b,c,d});
  }
  renderer::MapDrawRecord draw{};draw.indexCount=static_cast<u32>(indices.size());
  const auto originalIndices = indices;
  CollisionMeshRecipe recipe{assetGuidFromSeed("cubo-fonte"), 25, .02f};
  CollisionMeshBuild derived;
  std::string diagnostic;
  AE_EXPECT_TRUE(buildCollisionMesh(vertices, indices, draw, recipe, derived, diagnostic),
                 diagnostic.c_str());
  AE_EXPECT_TRUE(derived.indices.size() < draw.indexCount,
                 "o recurso físico tem menos triângulos que o visual");
  AE_EXPECT_TRUE(derived.indices.size() >= 3 && derived.indices.size() % 3 == 0,
                 "a saída continua sendo uma lista de triângulos");
  AE_EXPECT_TRUE(indices == originalIndices, "a malha visual fonte não foi alterada");
  AE_EXPECT_TRUE(collisionMeshGuid(recipe) == collisionMeshGuid(recipe), "a identidade derivada é determinística");
}

AE_TEST(import_profile_round_trips_collision_mesh_recipes) {
  ImportProfile profile;
  profile.collisionMeshes.push_back({assetGuidFromSeed("parede"), 25, .02f});
  ImportProfile parsed;
  AE_EXPECT_TRUE(parseImportProfile(serializeImportProfile(profile), parsed), "perfil atual é legível");
  AE_EXPECT_TRUE(sameImportProfile(profile, parsed), "fonte e parâmetros do derivado sobrevivem ao arquivo");
  AE_EXPECT_TRUE(sameImportPreparation(profile, ImportProfile{}),
                 "uma receita pós importação não força reler o GLB");
}

AE_TEST(schema_six_collision_recipe_keeps_its_legacy_identity_when_upgraded) {
  const auto source=assetGuidFromSeed("parede-legada");
  const auto text="{\"schema\":6,\"scale\":1,\"maximumTextureDimension\":2048,\"normals\":0,"
      "\"normalWeighting\":0,\"smoothingAngle\":180,\"tangents\":0,\"importCameras\":false,"
      "\"excludedNodes\":[],\"collisionMeshes\":[{\"source\":\""+source.text()+
      "\",\"trianglePercent\":25,\"maximumError\":0.02}]}";
  ImportProfile parsed;
  AE_EXPECT_TRUE(parseImportProfile(text,parsed),"perfil schema 6 continua legível");
  const auto legacy=parsed.collisionMeshes[0].identity;
  parsed.collisionMeshes[0].trianglePercent=50;
  parsed.collisionMeshes[0].maximumError=.05f;
  AE_EXPECT_TRUE(collisionMeshGuid(parsed.collisionMeshes[0])==legacy,
                 "mudar parâmetros não quebra a referência criada pelo schema 6");
  ImportProfile upgraded;
  AE_EXPECT_TRUE(parseImportProfile(serializeImportProfile(parsed),upgraded),"perfil atualizado grava schema 7");
  AE_EXPECT_TRUE(collisionMeshGuid(upgraded.collisionMeshes[0])==legacy,"round-trip conserva o GUID legado");
}
