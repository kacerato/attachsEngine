#include "harness.h"
#include "resources/gltf_import.h"
#include "resources/json_reader.h"

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

// Monta um GLB de verdade — cabeçalho, blocos alinhados, JSON e binário — em
// vez de um arquivo de exemplo guardado no repositório. O teste falha se o
// leitor deixar de aceitar um arquivo que qualquer exportador produziria.
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

// Um triângulo com posições e normais, indexado, num único nó transladado.
struct Triangle {
  std::string json;
  std::vector<u8> binary;
};
Triangle triangleAsset(const char *nodeExtra = "", const char *materialsAndMeshExtra = "") {
  std::vector<u8> binary;
  const float positions[9]{0, 0, 0, 1, 0, 0, 0, 1, 0};
  for (float value : positions) appendFloat(binary, value);
  const float normals[9]{0, 0, 1, 0, 0, 1, 0, 0, 1};
  for (float value : normals) appendFloat(binary, value);
  const u16 indices[3]{0, 1, 2};
  for (u16 value : indices) { binary.push_back(static_cast<u8>(value)); binary.push_back(static_cast<u8>(value >> 8)); }
  while (binary.size() % 4) binary.push_back(0);

  std::string json = std::string(R"({"asset":{"version":"2.0"},)") +
    R"("buffers":[{"byteLength":)" + std::to_string(binary.size()) + R"(}],)" +
    R"("bufferViews":[)"
      R"({"buffer":0,"byteOffset":0,"byteLength":36},)"
      R"({"buffer":0,"byteOffset":36,"byteLength":36},)"
      R"({"buffer":0,"byteOffset":72,"byteLength":6}],)" +
    R"("accessors":[)"
      R"({"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},)"
      R"({"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},)"
      R"({"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"}],)" +
    R"("meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1},"indices":2,"material":0}]}],)" +
    R"("materials":[{"pbrMetallicRoughness":{"baseColorFactor":[0.25,0.5,0.75,1],"roughnessFactor":0.3,"metallicFactor":0.1},"doubleSided":true}],)" +
    R"("nodes":[{"name":"Triangulo","mesh":0,"translation":[10,0,0])" + std::string(nodeExtra) + R"(}],)" +
    R"("scenes":[{"nodes":[0]}],"scene":0)" + std::string(materialsAndMeshExtra) + "}";
  return {json, binary};
}
} // namespace

AE_TEST(json_reader_accepts_real_documents_and_fails_closed) {
  JsonDocument document;
  AE_EXPECT_TRUE(JsonDocument::parse(R"({"a":1,"b":[true,false,null,"x\u00e9"],"c":{"d":-1.5e2}})", document),
                 "documento com todos os tipos");
  const auto *root = document.root();
  AE_EXPECT_TRUE(root && root->kind == JsonDocument::Kind::Object, "raiz é objeto");
  AE_EXPECT_EQ(document.number(*root, "a", 0), 1.0, "número");
  const auto *array = document.member(*root, "b");
  AE_EXPECT_TRUE(array && array->childCount == 4, "array com quatro itens");
  AE_EXPECT_TRUE(document.child(*array, 0)->boolean, "true");
  AE_EXPECT_TRUE(document.child(*array, 2)->kind == JsonDocument::Kind::Null, "null");
  AE_EXPECT_TRUE(document.textOf(*document.child(*array, 3)) == "x\xc3\xa9", "escape \\u vira UTF-8");
  const auto *nested = document.member(*root, "c");
  AE_EXPECT_TRUE(nested != nullptr, "objeto aninhado");
  AE_EXPECT_EQ(document.number(*nested, "d", 0), -150.0, "expoente");

  // Falha fechada em cada forma de arquivo quebrado. Um leitor tolerante aqui
  // vira geometria errada lá na frente.
  for (const char *broken : {"", "{", "{\"a\":}", "{\"a\":1,}", "[1,2", "{'a':1}", "{\"a\":01}",
                             "{\"a\":1} lixo", "\"sem fechamento", "{\"a\":NaN}", "{\"a\":+1}",
                             "{\"a\":\"\\ud800\"}"})
    AE_EXPECT_TRUE(!JsonDocument::parse(broken, document), broken);
  // Profundidade: um arquivo com aninhamento absurdo não pode estourar a pilha.
  std::string deep(JsonDocument::MaximumDepth + 8, '[');
  AE_EXPECT_TRUE(!JsonDocument::parse(deep, document), "profundidade limitada");
}

AE_TEST(glb_import_reads_geometry_hierarchy_and_material_factors) {
  const auto asset = triangleAsset();
  const auto glb = buildGlb(asset.json, asset.binary);
  GltfImport import;
  AE_EXPECT_TRUE(importGlb(glb, {}, {}, import), import.diagnostic.c_str());
  AE_EXPECT_EQ(import.draws.size(), 1u, "um desenho");
  AE_EXPECT_EQ(import.indices.size(), 3u, "três índices");
  AE_EXPECT_EQ(import.vertices.size(), 3u * renderer::MapVertexStride, "três vértices no passo do pacote");
  AE_EXPECT_TRUE(import.names[0] == "Triangulo", "o nome do nó vira o nome do objeto");

  // A geometria vive no espaço do NÓ: modelo identidade e limites locais. A
  // pose está na árvore, não assada no desenho — assar apagaria a hierarquia.
  AE_EXPECT_EQ(import.draws[0].model[12], 0.f, "desenho sem translação assada");
  AE_EXPECT_EQ(import.draws[0].model[0], 1.f, "modelo identidade");
  AE_EXPECT_TRUE(import.draws[0].boundsCenter[0] < 1.f, "centro em espaço local");
  AE_EXPECT_TRUE(import.draws[0].boundsRadius > 0.f, "raio do limite");

  // A árvore do arquivo chega inteira, com a transformação local do nó.
  AE_EXPECT_EQ(import.nodes.size(), 1u, "um nó");
  AE_EXPECT_TRUE(import.nodes[0].name == "Triangulo", "nome do nó");
  AE_EXPECT_EQ(import.nodes[0].parent, -1, "raiz");
  AE_EXPECT_EQ(import.nodes[0].localMatrix[12], 10.f, "translação local do nó");
  AE_EXPECT_EQ(import.drawNodes.size(), 1u, "um desenho ligado a um nó");
  AE_EXPECT_EQ(import.drawNodes[0], 0u, "o desenho pertence ao nó");

  // Vértice: posição em float, normal empacotada em snorm16, como o pacote.
  float position[3];
  std::memcpy(position, import.vertices.data() + renderer::MapVertexStride, 12);
  AE_EXPECT_EQ(position[0], 1.f, "segunda posição");
  i16 normal[4];
  std::memcpy(normal, import.vertices.data() + 12, 8);
  AE_EXPECT_EQ(normal[2], static_cast<i16>(32767), "normal +Z empacotada");

  // Material: fatores do glTF, sem nenhuma textura. O material neutro extra
  // fecha a lista para primitivas sem material.
  AE_EXPECT_EQ(import.materials.size(), 2u, "um material do arquivo mais o neutro");
  AE_EXPECT_EQ(import.materials[0].baseColorFactor[1], .5f, "cor base");
  AE_EXPECT_EQ(import.materials[0].roughness, .3f, "rugosidade");
  AE_EXPECT_TRUE((import.materials[0].flags & renderer::MapMaterialDoubleSided) != 0, "dupla face");
  AE_EXPECT_EQ(import.materials[0].textureIndices[0], renderer::InvalidMapTexture, "sem textura");
}

AE_TEST(glb_import_reuses_geometry_for_instances_and_reports_what_it_left_behind) {
  // O mesmo mesh em dois nós: dois desenhos, UMA cópia da geometria. Se a
  // importação duplicasse os vértices, um cenário com cem árvores iguais
  // custaria cem vezes a memória.
  auto asset = triangleAsset();
  const std::string um = R"("nodes":[{"name":"Triangulo","mesh":0,"translation":[10,0,0]}])";
  const std::string dois =
      R"("nodes":[{"name":"A","mesh":0,"translation":[10,0,0]},{"name":"B","mesh":0,"translation":[0,5,0]}])";
  const auto posicao = asset.json.find(um);
  AE_EXPECT_TRUE(posicao != std::string::npos, "nós encontrados");
  asset.json.replace(posicao, um.size(), dois);
  asset.json.replace(asset.json.find(R"("scenes":[{"nodes":[0]}])"), std::strlen(R"("scenes":[{"nodes":[0]}])"),
                     R"("scenes":[{"nodes":[0,1]}])");
  // E um arquivo que também traz animação, pele e textura: coisas que esta
  // importação não traz e precisa CONTAR, não esconder.
  asset.json.insert(asset.json.size() - 1,
                    R"(,"animations":[{"channels":[],"samplers":[]}],"skins":[{"joints":[0]}])");
  asset.json.replace(asset.json.find(R"("baseColorFactor":[0.25,0.5,0.75,1])"),
                     std::strlen(R"("baseColorFactor":[0.25,0.5,0.75,1])"),
                     R"("baseColorFactor":[0.25,0.5,0.75,1],"baseColorTexture":{"index":0})");

  const auto glb = buildGlb(asset.json, asset.binary);
  GltfImport import;
  AE_EXPECT_TRUE(importGlb(glb, {}, {}, import), import.diagnostic.c_str());
  AE_EXPECT_EQ(import.draws.size(), 2u, "duas instâncias");
  AE_EXPECT_EQ(import.vertices.size(), 3u * renderer::MapVertexStride, "uma cópia da geometria");
  AE_EXPECT_EQ(import.draws[0].vertexOffset, import.draws[1].vertexOffset, "as duas usam o mesmo bloco");
  AE_EXPECT_EQ(import.nodes.size(), 2u, "dois nós");
  AE_EXPECT_TRUE(import.nodes[0].localMatrix[12] != import.nodes[1].localMatrix[12],
                 "poses diferentes, na árvore e não no desenho");
  AE_EXPECT_TRUE(import.drawNodes[0] != import.drawNodes[1], "cada desenho pertence a seu nó");
  AE_EXPECT_EQ(import.skippedAnimations, 1u, "animação contada");
  AE_EXPECT_EQ(import.skippedSkins, 1u, "pele contada");
  AE_EXPECT_TRUE(import.skippedTextures > 0, "textura contada");
  AE_EXPECT_TRUE(import.anythingSkipped(), "o relatório diz que algo ficou para trás");
}

AE_TEST(glb_import_refuses_broken_files_with_a_concrete_reason) {
  GltfImport import;
  const std::vector<u8> vazio;
  AE_EXPECT_TRUE(!importGlb(vazio, {}, {}, import), "arquivo vazio recusado");
  AE_EXPECT_TRUE(!import.diagnostic.empty(), "com motivo");

  // Um `.gltf` de texto: o seletor do Android entrega um arquivo só, e o `.bin`
  // ao lado não vem junto. O diagnóstico precisa dizer isso, não "erro".
  const std::string texto = R"({"asset":{"version":"2.0"}})";
  AE_EXPECT_TRUE(!importGlb(std::span<const u8>(reinterpret_cast<const u8 *>(texto.data()), texto.size()), {}, {}, import),
                 "gltf de texto recusado");
  AE_EXPECT_TRUE(import.diagnostic.find(".glb") != std::string::npos, "o motivo aponta o caminho a seguir");

  // Extensão exigida muda o significado dos dados: importar ignorando daria
  // geometria errada com cara de certa.
  auto asset = triangleAsset();
  asset.json.insert(asset.json.size() - 1, R"(,"extensionsRequired":["KHR_draco_mesh_compression"])");
  AE_EXPECT_TRUE(!importGlb(buildGlb(asset.json, asset.binary), {}, {}, import), "extensão exigida recusada");
  AE_EXPECT_TRUE(import.diagnostic.find("extens") != std::string::npos, "o motivo fala de extensão");

  // Índice apontando para fora da primitiva: aceitar leria memória de outro
  // vértice e desenharia lixo.
  auto quebrado = triangleAsset();
  auto binario = quebrado.binary;
  binario[72] = 9; // primeiro índice vira 9, com só três vértices
  AE_EXPECT_TRUE(!importGlb(buildGlb(quebrado.json, binario), {}, {}, import), "índice fora da faixa recusado");
  AE_EXPECT_TRUE(import.diagnostic.find("Índice") != std::string::npos, "o motivo fala do índice");
  AE_EXPECT_TRUE(import.draws.empty() && import.vertices.empty(), "nada parcial sobrou");

  // Ciclo na hierarquia: um nó que é filho de si mesmo rodaria para sempre.
  auto ciclo = triangleAsset();
  ciclo.json.replace(ciclo.json.find(R"("translation":[10,0,0])"), std::strlen(R"("translation":[10,0,0])"),
                     R"("translation":[10,0,0],"children":[0])");
  AE_EXPECT_TRUE(!importGlb(buildGlb(ciclo.json, ciclo.binary), {}, {}, import), "ciclo recusado");
  AE_EXPECT_TRUE(import.diagnostic.find("ciclo") != std::string::npos, "o motivo fala do ciclo");
}

AE_TEST(glb_import_respects_limits_and_cancellation) {
  const auto asset = triangleAsset();
  const auto glb = buildGlb(asset.json, asset.binary);

  GltfImportLimits apertado;
  apertado.maximumBytes = 16;
  GltfImport import;
  AE_EXPECT_TRUE(!importGlb(glb, apertado, {}, import), "limite de bytes respeitado");
  AE_EXPECT_TRUE(import.diagnostic.find("limite") != std::string::npos, "o motivo fala do limite");

  GltfImportLimits poucosVertices;
  poucosVertices.maximumVertices = 2;
  AE_EXPECT_TRUE(!importGlb(glb, poucosVertices, {}, import), "limite de vértices respeitado");
  AE_EXPECT_TRUE(import.vertices.empty(), "nada parcial sobrou");

  // Cancelamento: quem desistiu precisa saber que desistiu, e não confundir
  // com arquivo inválido.
  GltfImportProgress cancelador;
  cancelador.cancelled = [](void *) { return true; };
  AE_EXPECT_TRUE(!importGlb(glb, {}, cancelador, import), "importação cancelada");
  AE_EXPECT_TRUE(import.cancelled, "marcada como cancelamento, não como erro");

  // Progresso: chega ao fim e é monotônico. Uma barra que anda para trás é
  // pior que nenhuma barra.
  struct Trilha { float last = -1; bool monotonic = true; bool finished = false; };
  Trilha trilha;
  GltfImportProgress observador;
  observador.context = &trilha;
  observador.report = [](void *context, float fraction, const char *) {
    auto &t = *static_cast<Trilha *>(context);
    if (fraction < t.last) t.monotonic = false;
    t.last = fraction;
    if (fraction >= 1.f) t.finished = true;
  };
  AE_EXPECT_TRUE(importGlb(glb, {}, observador, import), import.diagnostic.c_str());
  AE_EXPECT_TRUE(trilha.monotonic, "progresso não anda para trás");
  AE_EXPECT_TRUE(trilha.finished, "progresso chega ao fim");
}

AE_TEST(glb_import_keys_are_stable_across_a_reimport_that_adds_an_object) {
  // O caso que quebra referências em toda engine que usa índice: o usuário
  // acrescenta um objeto no editor 3D, reexporta, e tudo o que vinha depois
  // muda de índice. A chave precisa continuar a mesma para o que não mudou.
  auto original = triangleAsset();
  const auto glb = buildGlb(original.json, original.binary);
  GltfImport antes;
  AE_EXPECT_TRUE(importGlb(glb, {}, {}, antes), antes.diagnostic.c_str());
  AE_EXPECT_EQ(antes.keys.size(), 1u, "uma chave");

  // Reexportação com um nó novo ANTES do existente: o índice do "Triangulo"
  // passa de 0 para 1.
  auto reexportado = triangleAsset();
  const std::string um = R"("nodes":[{"name":"Triangulo","mesh":0,"translation":[10,0,0]}])";
  const std::string dois =
      R"("nodes":[{"name":"Novo","mesh":0,"translation":[0,0,0]},{"name":"Triangulo","mesh":0,"translation":[10,0,0]}])";
  reexportado.json.replace(reexportado.json.find(um), um.size(), dois);
  reexportado.json.replace(reexportado.json.find(R"("scenes":[{"nodes":[0]}])"),
                           std::strlen(R"("scenes":[{"nodes":[0]}])"), R"("scenes":[{"nodes":[0,1]}])");
  GltfImport depois;
  AE_EXPECT_TRUE(importGlb(buildGlb(reexportado.json, reexportado.binary), {}, {}, depois),
                 depois.diagnostic.c_str());
  AE_EXPECT_EQ(depois.keys.size(), 2u, "duas chaves");
  AE_EXPECT_TRUE(depois.keys[1] == antes.keys[0], "a chave do objeto existente não mudou de valor");
  AE_EXPECT_TRUE(depois.keys[0] != depois.keys[1], "objetos diferentes têm chaves diferentes");
}
