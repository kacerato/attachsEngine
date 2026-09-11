#include "harness.h"
#include "editor/editor_archive.h"
#include "editor/editor_session.h"
#include "renderer/authoring_geometry.h"

#include <cstring>
#include <string>
#include <vector>

using namespace ae;
using namespace ae::editor;


namespace {
void appendU32(std::vector<u8> &out, u32 value) {
  for (u32 i = 0; i < 4; ++i) out.push_back(static_cast<u8>(value >> (i * 8)));
}
void appendFloat(std::vector<u8> &out, float value) {
  u32 bits = 0;
  std::memcpy(&bits, &value, 4);
  appendU32(out, bits);
}

// Um GLB com dois nós nomeados apontando para a mesma malha: instâncias, que é
// o caso que separa "a importação funciona" de "a importação duplica tudo".
std::vector<u8> twoNodeGlb() {
  std::vector<u8> binary;
  const float positions[9]{0, 0, 0, 1, 0, 0, 0, 1, 0};
  for (float value : positions) appendFloat(binary, value);
  const u16 indices[3]{0, 1, 2};
  for (u16 value : indices) { binary.push_back(static_cast<u8>(value)); binary.push_back(static_cast<u8>(value >> 8)); }
  while (binary.size() % 4) binary.push_back(0);

  const std::string json =
      std::string(R"({"asset":{"version":"2.0"},)") +
      R"("buffers":[{"byteLength":)" + std::to_string(binary.size()) + R"(}],)" +
      R"("bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6}],)" +
      R"("accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},)"
      R"({"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}],)" +
      R"("meshes":[{"name":"Placa","primitives":[{"attributes":{"POSITION":0},"indices":1,"material":0}]}],)" +
      R"("materials":[{"pbrMetallicRoughness":{"baseColorFactor":[1,0,0,1]}}],)" +
      R"("nodes":[{"name":"Esquerda","mesh":0,"translation":[-2,0,0]},)"
      R"({"name":"Direita","mesh":0,"translation":[2,0,0]}],)" +
      R"("scenes":[{"nodes":[0,1]}],"scene":0})";

  std::string paddedJson = json;
  while (paddedJson.size() % 4) paddedJson.push_back(' ');
  std::vector<u8> glb;
  appendU32(glb, 0x46546C67);
  appendU32(glb, 2);
  appendU32(glb, static_cast<u32>(12 + 8 + paddedJson.size() + 8 + binary.size()));
  appendU32(glb, static_cast<u32>(paddedJson.size()));
  appendU32(glb, 0x4E4F534A);
  glb.insert(glb.end(), paddedJson.begin(), paddedJson.end());
  appendU32(glb, static_cast<u32>(binary.size()));
  appendU32(glb, 0x004E4942);
  glb.insert(glb.end(), binary.begin(), binary.end());
  return glb;
}

// Duplo do consumidor gráfico: monta o pacote como o renderer monta — primitivas
// internas primeiro, biblioteca importada depois — sem nenhum Vulkan.
struct FakeRenderer {
  std::vector<u8> vertices;
  std::vector<u32> indices;
  std::vector<renderer::MapDrawRecord> draws;
  std::vector<renderer::MapMaterialRecord> materials;
  u32 rebuilds = 0;
  bool refuse = false;

  bool publish(std::span<const u8> extraVertices, std::span<const u32> extraIndices,
               std::span<const renderer::MapDrawRecord> extraDraws,
               std::span<const renderer::MapMaterialRecord> extraMaterials,
               EditorSession::PublishedGeometry &out) {
    if (refuse) return false;
    ++rebuilds;
    vertices.clear(); indices.clear(); draws.clear(); materials.clear();
    if (!renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride, vertices, indices, draws, materials))
      return false;
    const auto vertexBase = static_cast<u32>(vertices.size() / renderer::MapVertexStride);
    const auto indexBase = static_cast<u32>(indices.size());
    const auto materialBase = static_cast<u32>(materials.size());
    vertices.insert(vertices.end(), extraVertices.begin(), extraVertices.end());
    indices.insert(indices.end(), extraIndices.begin(), extraIndices.end());
    materials.insert(materials.end(), extraMaterials.begin(), extraMaterials.end());
    for (const auto &source : extraDraws) {
      auto draw = source;
      draw.firstIndex += indexBase;
      draw.vertexOffset += vertexBase;
      draw.materialIndex += materialBase;
      draw.lodGroupId = static_cast<u32>(draws.size());
      draws.push_back(draw);
    }
    out = {draws, materials, vertices, indices};
    return true;
  }
};

// Uma sessão pronta com a biblioteca de primitivas, como o aparelho monta um
// projeto vazio.
void startSession(EditorSession &session, FakeRenderer &renderer) {
  std::vector<u8> vertices;
  std::vector<u32> indices;
  std::vector<renderer::MapDrawRecord> draws;
  std::vector<renderer::MapMaterialRecord> materials;
  AE_EXPECT_TRUE(renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride, vertices, indices, draws, materials),
                 "primitivas internas");
  AE_EXPECT_TRUE(session.importMap(draws, materials, false, vertices, indices, 0), "biblioteca inicial");
  session.setGeometryPublisher([&renderer](std::span<const u8> v, std::span<const u32> i,
                                           std::span<const renderer::MapDrawRecord> d,
                                           std::span<const renderer::MapMaterialRecord> m,
                                           EditorSession::PublishedGeometry &out) {
    return renderer.publish(v, i, d, m, out);
  });
}
} // namespace

AE_TEST(importing_a_glb_creates_objects_that_point_at_the_new_geometry) {
  EditorSession session;
  FakeRenderer renderer;
  startSession(session, renderer);
  // Quantos desenhos as primitivas internas ocupam: é o deslocamento a partir
  // do qual a geometria importada entra no pacote publicado.
  std::vector<u8> baseVertices;
  std::vector<u32> baseIndices;
  std::vector<renderer::MapDrawRecord> baseDraws;
  std::vector<renderer::MapMaterialRecord> baseMaterials;
  AE_EXPECT_TRUE(renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride, baseVertices, baseIndices,
                                                      baseDraws, baseMaterials),
                 "primitivas internas");
  const auto primitives = baseDraws.size();

  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(session.importModel(twoNodeGlb(), "Fontes/placa.glb", {}, report), report.diagnostic.c_str());
  AE_EXPECT_EQ(report.objects, 2u, "dois nós viraram dois objetos");
  AE_EXPECT_EQ(renderer.rebuilds, 1u, "a biblioteca gráfica foi reconstruída uma vez");
  const auto ids = session.document().childrenOf(session.document().root());
  AE_EXPECT_EQ(ids.size(), 2u, "dois objetos na cena");
  const auto *esquerda = meshRenderer(*session.document().find(ids[0]));
  const auto *direita = meshRenderer(*session.document().find(ids[1]));
  AE_EXPECT_TRUE(esquerda && direita, "os dois têm malha");
  AE_EXPECT_TRUE(session.document().find(ids[0])->name == std::string("Esquerda"),
                 "o nome do nó do arquivo vira o nome do objeto");
  AE_EXPECT_TRUE(esquerda->asset.valid() && direita->asset.valid(), "os dois têm identidade");
  AE_EXPECT_TRUE(!(esquerda->asset == direita->asset), "identidades distintas para nós distintos");
  AE_EXPECT_TRUE(esquerda->mesh > 0 && direita->mesh > 0, "slots resolvidos");
  AE_EXPECT_TRUE(esquerda->mesh != direita->mesh, "slots distintos");

  // Instâncias: dois desenhos, mas a geometria entrou UMA vez.
  AE_EXPECT_EQ(renderer.draws.size(), primitives + 2, "primitivas mais dois desenhos importados");
  AE_EXPECT_EQ(renderer.draws[esquerda->mesh - 1].vertexOffset,
               renderer.draws[direita->mesh - 1].vertexOffset, "as duas instâncias usam o mesmo bloco");

  // A extração precisa aceitar a cena resultante: é ela que vira quadro.
  std::vector<renderer::MapDrawState> states;
  AE_EXPECT_TRUE(session.extractMap(states), "a cena importada extrai");

  // O registro ficou com a fonte, seu hash e a identidade estável do arquivo.
  const auto *record = session.assets().findByPath("Fontes/placa.glb");
  AE_EXPECT_TRUE(record != nullptr, "a fonte está no registro");
  AE_EXPECT_TRUE(record->guid == report.source, "a identidade da fonte é a informada");
  AE_EXPECT_EQ(record->contentHash.size(), 64u, "hash de conteúdo gravado");
  AE_EXPECT_EQ(record->importerVersion, 1u, "versão do importador gravada");
}

AE_TEST(importing_twice_keeps_one_source_and_stable_identities) {
  EditorSession session;
  FakeRenderer renderer;
  startSession(session, renderer);
  const auto glb = twoNodeGlb();
  EditorSession::ModelImportReport primeira;
  AE_EXPECT_TRUE(session.importModel(glb, "Fontes/placa.glb", {}, primeira), primeira.diagnostic.c_str());
  const auto identidade = meshRenderer(*session.document().find(
      session.document().childrenOf(session.document().root())[0]))->asset;
  EditorSession::ModelImportReport segunda;
  AE_EXPECT_TRUE(session.importModel(glb, "Fontes/placa.glb", {}, segunda), segunda.diagnostic.c_str());
  AE_EXPECT_TRUE(segunda.reimported, "a segunda vez é reimportação, não outra fonte");
  AE_EXPECT_TRUE(segunda.source == primeira.source, "a fonte mantém a identidade");
  AE_EXPECT_EQ(session.assets().size(), 1u, "um recurso de fonte no registro");
  // Reimportar publica a geometria de novo e NÃO duplica os objetos: quem quer
  // outra cópia instancia de novo, e isso é um ato diferente.
  AE_EXPECT_EQ(segunda.objects, 0u, "nenhum objeto criado na reimportação");
  // Cópia, não a `span`: criar objetos mais adiante realoca o armazenamento do
  // documento e uma vista sobre ele ficaria pendurada.
  const auto filhos = session.document().childrenOf(session.document().root());
  const std::vector<EditorEntityId> ids(filhos.begin(), filhos.end());
  AE_EXPECT_EQ(ids.size(), 2u, "os dois objetos originais continuam sendo dois");
  AE_EXPECT_TRUE(meshRenderer(*session.document().find(ids[0]))->asset == identidade,
                 "a identidade da malha é a mesma depois da reimportação");
  AE_EXPECT_TRUE(meshRenderer(*session.document().find(ids[0]))->mesh > 0,
                 "e o slot continua resolvido");
  AE_EXPECT_EQ(renderer.rebuilds, 2u, "a biblioteca gráfica foi republicada");

  // Uma SEGUNDA fonte, diferente: entra ao lado, sem mexer nos slots da
  // primeira — é o que a biblioteca por bloco garante.
  EditorSession::ModelImportReport outra;
  AE_EXPECT_TRUE(session.importModel(glb, "Fontes/outra.glb", {}, outra), outra.diagnostic.c_str());
  AE_EXPECT_TRUE(!outra.reimported, "caminho novo é fonte nova");
  AE_EXPECT_TRUE(!(outra.source == primeira.source), "identidade de fonte diferente");
  AE_EXPECT_EQ(session.assets().size(), 2u, "duas fontes no registro");
  AE_EXPECT_TRUE(meshRenderer(*session.document().find(ids[0]))->asset == identidade,
                 "a primeira fonte não foi tocada");
}

AE_TEST(a_refused_import_leaves_the_scene_exactly_as_it_was) {
  EditorSession session;
  FakeRenderer renderer;
  startSession(session, renderer);
  const auto revision = session.document().revision();

  EditorSession::ModelImportReport report;
  // Arquivo que não é GLB.
  const std::string texto = "nao sou um glb";
  AE_EXPECT_TRUE(!session.importModel(std::span<const u8>(reinterpret_cast<const u8 *>(texto.data()), texto.size()),
                                      "Fontes/x.glb", {}, report),
                 "arquivo inválido recusado");
  AE_EXPECT_TRUE(!report.diagnostic.empty(), "com motivo concreto");
  AE_EXPECT_EQ(session.document().revision(), revision, "o documento não mudou");
  AE_EXPECT_EQ(renderer.rebuilds, 0u, "a biblioteca gráfica não foi tocada");
  AE_EXPECT_EQ(session.assets().size(), 0u, "nada entrou no registro");

  // E um consumidor gráfico que recusa a geometria também não pode deixar a
  // cena com objetos apontando para nada.
  renderer.refuse = true;
  AE_EXPECT_TRUE(!session.importModel(twoNodeGlb(), "Fontes/placa.glb", {}, report), "publicação recusada");
  AE_EXPECT_EQ(session.document().revision(), revision, "o documento continua intacto");
  AE_EXPECT_EQ(session.assets().size(), 0u, "e o registro também");
}

AE_TEST(imported_identities_survive_saving_and_reopening_the_project) {
  EditorSession session;
  FakeRenderer renderer;
  startSession(session, renderer);
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(session.importModel(twoNodeGlb(), "Fontes/placa.glb", {}, report), report.diagnostic.c_str());
  const auto ids = session.document().childrenOf(session.document().root());
  const auto identidade = meshRenderer(*session.document().find(ids[1]))->asset;
  const auto texto = serializeEditorDocument(session.document(), 0);
  const auto registro = session.serializeAssets();

  // Reabrir: sessão nova, biblioteca de primitivas, registro relido e a mesma
  // fonte reimportada — é o que o projeto faz ao ser aberto de novo.
  EditorSession reaberta;
  FakeRenderer outroRenderer;
  startSession(reaberta, outroRenderer);
  AE_EXPECT_TRUE(reaberta.loadAssets(registro), "registro relido");
  AE_EXPECT_EQ(reaberta.assets().size(), 1u, "a fonte voltou");
  EditorSession::ModelImportReport revisita;
  AE_EXPECT_TRUE(reaberta.importModel(twoNodeGlb(), "Fontes/placa.glb", {}, revisita), revisita.diagnostic.c_str());
  AE_EXPECT_TRUE(revisita.reimported, "reabrir o projeto reimporta a fonte, não duplica");
  AE_EXPECT_EQ(revisita.objects, 0u, "e não cria objeto nenhum");
  AE_EXPECT_TRUE(revisita.source == report.source, "a fonte reabriu com a mesma identidade");

  EditorDocument carregado;
  AE_EXPECT_TRUE(deserializeEditorDocument(texto, 0, carregado), "cena relida");
  const auto *render = meshRenderer(*carregado.find(ids[1]));
  AE_EXPECT_TRUE(render != nullptr && render->asset == identidade,
                 "a identidade da malha sobreviveu ao arquivo");
}
