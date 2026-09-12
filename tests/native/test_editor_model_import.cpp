#include "harness.h"
#include "editor/editor_archive.h"
#include "editor/editor_session.h"
#include "renderer/authoring_geometry.h"
#include "editor/editor_filesystem.h"

#include <cstring>
#include <string>
#include <vector>
#include <chrono>
#include <filesystem>
#include <fstream>

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

namespace {
// Um assembly de verdade: uma raiz SEM malha com dois filhos que têm malha, e a
// porta deslocada do próprio centro — é o caso que separa "renderizou igual à
// imagem" de "a hierarquia sobreviveu para editar".
std::vector<u8> assemblyGlb() {
  std::vector<u8> binary;
  // Um quadrado de 1x1 no plano XY, com o canto na origem: o centro do mesh
  // (0.5,0.5,0) NÃO é a origem do nó, que é onde a dobradiça vive.
  const float positions[12]{0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0};
  for (float value : positions) appendFloat(binary, value);
  const u16 indices[6]{0, 1, 2, 0, 2, 3};
  for (u16 value : indices) { binary.push_back(static_cast<u8>(value)); binary.push_back(static_cast<u8>(value >> 8)); }
  while (binary.size() % 4) binary.push_back(0);

  const std::string json =
      std::string(R"({"asset":{"version":"2.0"},)") +
      R"("buffers":[{"byteLength":)" + std::to_string(binary.size()) + R"(}],)" +
      R"("bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":48},{"buffer":0,"byteOffset":48,"byteLength":12}],)" +
      R"("accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3"},)"
      R"({"bufferView":1,"componentType":5123,"count":6,"type":"SCALAR"}],)" +
      R"("meshes":[{"name":"Chapa","primitives":[{"attributes":{"POSITION":0},"indices":1}]}],)" +
      R"("nodes":[)"
      R"({"name":"Veiculo","children":[1,2],"translation":[0,0,0]},)"
      R"({"name":"Carroceria","mesh":0,"translation":[0,0,0]},)"
      R"({"name":"PortaEsquerda","mesh":0,"translation":[3,0,0]}],)" +
      R"("scenes":[{"nodes":[0]}],"scene":0})";

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
} // namespace

AE_TEST(importing_an_assembly_keeps_the_tree_and_the_empty_parent) {
  EditorSession session;
  FakeRenderer renderer;
  startSession(session, renderer);

  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(session.importModel(assemblyGlb(), "Fontes/veiculo.glb", {}, report), report.diagnostic.c_str());
  AE_EXPECT_EQ(report.objects, 3u, "raiz mais dois filhos");
  AE_EXPECT_EQ(report.groups, 1u, "o nó sem malha sobreviveu como grupo");

  const auto raizes = session.document().childrenOf(session.document().root());
  AE_EXPECT_EQ(raizes.size(), 1u, "uma raiz do modelo, não três irmãos achatados");
  const auto veiculo = raizes[0];
  AE_EXPECT_TRUE(session.document().find(veiculo)->name == std::string("Veiculo"), "a raiz é o nó do arquivo");
  AE_EXPECT_TRUE(meshRenderer(*session.document().find(veiculo)) == nullptr,
                 "um nó sem malha não ganha malha inventada");

  const auto filhos = session.document().childrenOf(veiculo);
  AE_EXPECT_EQ(filhos.size(), 2u, "dois filhos");
  AE_EXPECT_TRUE(session.document().find(filhos[0])->name == std::string("Carroceria"), "ordem do arquivo");
  AE_EXPECT_TRUE(session.document().find(filhos[1])->name == std::string("PortaEsquerda"), "ordem do arquivo");
  // A pose local do nó virou a transformação do objeto — é isso que torna a
  // porta editável em vez de fundida na geometria.
  AE_EXPECT_EQ(session.document().find(filhos[1])->transform.position[0], 3.f, "translação local da porta");
  AE_EXPECT_EQ(session.document().find(filhos[0])->transform.position[0], 0.f, "carroceria na origem do pai");
}

AE_TEST(moving_one_part_does_not_move_its_sibling_and_moving_the_root_moves_both) {
  EditorSession session;
  FakeRenderer renderer;
  startSession(session, renderer);
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(session.importModel(assemblyGlb(), "Fontes/veiculo.glb", {}, report), report.diagnostic.c_str());

  const auto veiculo = session.document().childrenOf(session.document().root())[0];
  const auto filhosSpan = session.document().childrenOf(veiculo);
  const std::vector<EditorEntityId> filhos(filhosSpan.begin(), filhosSpan.end());

  const auto desenhoDe = [&](EditorEntityId id) {
    std::vector<renderer::MapDrawState> states;
    if (!session.extractMap(states)) return renderer::MapDrawState{};
    for (const auto &state : states) if (state.objectId == id) return state;
    return renderer::MapDrawState{};
  };

  const auto carroceriaAntes = desenhoDe(filhos[0]);
  // Mover SÓ a porta.
  auto porta = *session.document().find(filhos[1]);
  porta.transform.position[1] += 5;
  AE_EXPECT_TRUE(session.document().applyEntityValues(filhos[1], porta), "porta movida");
  const auto carroceriaDepois = desenhoDe(filhos[0]);
  AE_EXPECT_EQ(carroceriaDepois.pose.draw.model[13], carroceriaAntes.pose.draw.model[13],
               "a carroceria não se move quando a porta se move");
  AE_EXPECT_EQ(desenhoDe(filhos[1]).pose.draw.model[13], carroceriaAntes.pose.draw.model[13] + 5.f,
               "a porta se moveu");

  // Mover a RAIZ leva os dois.
  auto raiz = *session.document().find(veiculo);
  raiz.transform.position[2] += 10;
  AE_EXPECT_TRUE(session.document().applyEntityValues(veiculo, raiz), "raiz movida");
  AE_EXPECT_EQ(desenhoDe(filhos[0]).pose.draw.model[14], carroceriaAntes.pose.draw.model[14] + 10.f,
               "a carroceria seguiu o pai");
  AE_EXPECT_EQ(desenhoDe(filhos[1]).pose.draw.model[14], carroceriaAntes.pose.draw.model[14] + 10.f,
               "a porta também seguiu o pai");
}

AE_TEST(the_pivot_of_an_imported_part_is_the_node_origin_not_the_mesh_centre) {
  // A chapa tem o canto na origem e o centro em (0.5,0.5,0). Se o pivô fosse o
  // centro visual, girar a porta a arrastaria para longe da dobradiça.
  EditorSession session;
  FakeRenderer renderer;
  startSession(session, renderer);
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(session.importModel(assemblyGlb(), "Fontes/veiculo.glb", {}, report), report.diagnostic.c_str());

  const auto veiculo = session.document().childrenOf(session.document().root())[0];
  const auto filhosSpan = session.document().childrenOf(veiculo);
  const std::vector<EditorEntityId> filhos(filhosSpan.begin(), filhosSpan.end());

  std::vector<renderer::MapDrawState> states;
  AE_EXPECT_TRUE(session.extractMap(states), "extração");
  for (const auto &state : states) if (state.objectId == filhos[1]) {
    // O objeto está em x=3 e a geometria ocupa [3,4]: o nó é a origem, e o
    // canto do mesh coincide com ela.
    AE_EXPECT_EQ(state.pose.draw.model[12], 3.f, "a origem do objeto é a origem do nó");
    return;
  }
  AE_EXPECT_TRUE(false, "a porta apareceu na extração");
}

AE_TEST(a_recreated_surface_gets_the_imported_geometry_back) {
  // O defeito M03, reproduzido no aparelho: mandar o editor para segundo plano
  // e voltar deixava a hierarquia inteira e o viewport VAZIO, com
  // "Falha ao publicar documento no renderer" a cada quadro.
  //
  // A causa não é a cena nem a superfície: recriar a superfície reconstrói o
  // renderer do zero, e a biblioteca de autoria dele volta a ter só as
  // primitivas internas. A cena continua apontando para os desenhos do modelo
  // importado, e a publicação passa a falhar contra uma biblioteca que não os
  // tem mais.
  EditorSession session;
  FakeRenderer first;
  startSession(session, first);
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(session.importModel(twoNodeGlb(), "Fontes/placa.glb", {}, report), report.diagnostic.c_str());
  AE_EXPECT_EQ(report.objects, 2u, "dois objetos importados");

  std::vector<EditorMapUpdate> updates;
  AE_EXPECT_TRUE(session.extractMap(updates), "publica antes de suspender");
  const auto before = session.document().entityCount();

  // A superfície é recriada: consumidor gráfico NOVO, sem nada da importação.
  FakeRenderer second;
  session.setGeometryPublisher([&second](std::span<const u8> v, std::span<const u32> i,
                                         std::span<const renderer::MapDrawRecord> d,
                                         std::span<const renderer::MapMaterialRecord> m,
                                         EditorSession::PublishedGeometry &out) {
    return second.publish(v, i, d, m, out);
  });
  std::string diagnostic;
  AE_EXPECT_TRUE(session.republishGeometry(diagnostic), diagnostic.c_str());
  AE_EXPECT_EQ(second.rebuilds, 1u, "a biblioteca do consumidor novo foi montada uma vez");
  AE_EXPECT_TRUE(second.draws.size() > first.draws.size() - 1,
                 "o pacote novo carrega as primitivas e a geometria importada");

  // O que o defeito quebrava: a cena sobrevive E volta a publicar.
  AE_EXPECT_EQ(session.document().entityCount(), before, "nenhum objeto perdido na reidratação");
  updates.clear();
  AE_EXPECT_TRUE(session.extractMap(updates), "publica de novo depois da reidratação");
  u32 drawn = 0;
  for (const auto &update : updates) if (update.objectId && update.visible) ++drawn;
  AE_EXPECT_TRUE(drawn >= 2u, "os objetos importados voltam a ser desenhados");

  // Reidratar sem nada importado é silenciosamente verdadeiro: a biblioteca do
  // consumidor novo já são as mesmas primitivas.
  EditorSession limpa;
  FakeRenderer vazio;
  startSession(limpa, vazio);
  AE_EXPECT_TRUE(limpa.republishGeometry(diagnostic), "sem importação não há o que reidratar");
  AE_EXPECT_EQ(vazio.rebuilds, 0u, "e nada é republicado à toa");
}

namespace {
// Uma pasta de projeto de verdade: renomear e apagar mexem no disco, e testar
// isso com um duplo do sistema de arquivos testaria o duplo.
struct ProjectDirectory {
  std::filesystem::path root;
  ProjectDirectory() {
    root = std::filesystem::temp_directory_path() /
           ("aether-recursos-" + std::to_string(
               std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root / "Fontes");
    std::filesystem::create_directories(root / ".astra");
  }
  ~ProjectDirectory() { std::error_code ignored; std::filesystem::remove_all(root, ignored); }
};
} // namespace

AE_TEST(renaming_a_resource_keeps_every_object_that_uses_it) {
  // O retorno de ter identidade separada do caminho: mover um arquivo NAO pode
  // quebrar um objeto. Antes do GUID, a cena apontava para o indice do desenho
  // e qualquer mexida no pacote trocava um objeto por outro em silencio.
  ProjectDirectory project;
  EditorSession session;
  FakeRenderer renderer;
  startSession(session, renderer);
  AE_EXPECT_TRUE(session.setProjectDirectory(project.root.string().c_str()), "projeto aberto");
  std::ofstream(project.root / "Fontes" / "placa.glb", std::ios::binary) << "glb";

  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(session.importModel(twoNodeGlb(), "Fontes/placa.glb", {}, report), report.diagnostic.c_str());
  AE_EXPECT_EQ(report.objects, 2u, "dois objetos");

  std::vector<EditorMapUpdate> before;
  AE_EXPECT_TRUE(session.extractMap(before), "publica antes");
  u32 drawnBefore = 0;
  for (const auto &update : before) if (update.objectId && update.visible) ++drawnBefore;

  EditorSession::ResourceChangeReport change;
  AE_EXPECT_TRUE(session.moveResource("Fontes/placa.glb", "Fontes/tabuleta.glb", change),
                 change.diagnostic.c_str());
  AE_EXPECT_EQ(change.retargeted, 1u, "o recurso foi reapontado");
  // O registro precisa ir ao disco AGORA. Medido no aparelho: renomear a fonte
  // e reabrir antes de salvar apagava o veiculo da tela com a hierarquia
  // inteira preservada, porque o registro ainda apontava para o nome antigo.
  AE_EXPECT_TRUE(session.assetRegistryDirty(), "o registro pede gravacao imediata");
  session.clearAssetRegistryDirty();
  AE_EXPECT_TRUE(session.serializeAssets().find("tabuleta.glb") != std::string::npos,
                 "e o texto gravado ja carrega o nome novo");
  AE_EXPECT_TRUE(std::filesystem::exists(project.root / "Fontes" / "tabuleta.glb"), "arquivo movido");
  AE_EXPECT_TRUE(!std::filesystem::exists(project.root / "Fontes" / "placa.glb"), "nao ficou copia");


  // O que importa: a cena continua inteira e continua desenhando.
  std::vector<EditorMapUpdate> after;
  AE_EXPECT_TRUE(session.extractMap(after), "publica depois");
  u32 drawnAfter = 0;
  for (const auto &update : after) if (update.objectId && update.visible) ++drawnAfter;
  AE_EXPECT_EQ(drawnAfter, drawnBefore, "renomear nao muda o que e desenhado");
}

AE_TEST(a_move_that_would_collide_changes_nothing) {
  ProjectDirectory project;
  EditorSession session;
  FakeRenderer renderer;
  startSession(session, renderer);
  AE_EXPECT_TRUE(session.setProjectDirectory(project.root.string().c_str()), "projeto aberto");
  std::ofstream(project.root / "Fontes" / "placa.glb", std::ios::binary) << "glb";
  std::ofstream(project.root / "Fontes" / "ocupado.glb", std::ios::binary) << "outro";
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(session.importModel(twoNodeGlb(), "Fontes/placa.glb", {}, report), report.diagnostic.c_str());

  EditorSession::ResourceChangeReport change;
  AE_EXPECT_TRUE(!session.moveResource("Fontes/placa.glb", "Fontes/ocupado.glb", change),
                 "destino ocupado e recusado");
  AE_EXPECT_TRUE(!change.diagnostic.empty(), "e diz por que");
  AE_EXPECT_TRUE(std::filesystem::exists(project.root / "Fontes" / "placa.glb"), "origem intacta");
  // O registro tambem: uma recusa que deixasse o caminho novo gravado faria o
  // projeto reabrir procurando um arquivo que nunca se moveu.
  std::vector<EditorMapUpdate> updates;
  AE_EXPECT_TRUE(session.extractMap(updates), "a cena continua publicando");
}

AE_TEST(deleting_a_resource_in_use_is_refused_and_says_who_uses_it) {
  ProjectDirectory project;
  EditorSession session;
  FakeRenderer renderer;
  startSession(session, renderer);
  AE_EXPECT_TRUE(session.setProjectDirectory(project.root.string().c_str()), "projeto aberto");
  std::ofstream(project.root / "Fontes" / "placa.glb", std::ios::binary) << "glb";
  EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(session.importModel(twoNodeGlb(), "Fontes/placa.glb", {}, report), report.diagnostic.c_str());

  EditorSession::ResourceChangeReport change;
  AE_EXPECT_TRUE(!session.deleteResource("Fontes/placa.glb", false, change), "recusa sem forcar");
  AE_EXPECT_TRUE(change.sceneUsers >= 2u, "conta os objetos que usam");
  AE_EXPECT_TRUE(change.diagnostic.find("objeto") != std::string::npos, "e diz quantos");
  AE_EXPECT_TRUE(std::filesystem::exists(project.root / "Fontes" / "placa.glb"), "arquivo intacto");

  // Forcando: o arquivo sai e os objetos ficam SEM malha, visivelmente, em vez
  // de apontar para a malha que por acaso ocupar o indice antigo.
  EditorSession::ResourceChangeReport forced;
  AE_EXPECT_TRUE(session.deleteResource("Fontes/placa.glb", true, forced), forced.diagnostic.c_str());
  AE_EXPECT_TRUE(!std::filesystem::exists(project.root / "Fontes" / "placa.glb"), "arquivo apagado");
  AE_EXPECT_TRUE(forced.sceneUsers >= 2u, "o relatorio diz quantos ficaram sem malha");
  std::vector<EditorMapUpdate> updates;
  AE_EXPECT_TRUE(session.extractMap(updates), "a cena continua publicando sem o recurso");
}

AE_TEST(the_project_state_folder_is_not_a_resource) {
  // `.astra` guarda historico, registro e cache. Apagar por engano custaria o
  // projeto inteiro, e ele nao e um recurso do usuario.
  ProjectDirectory project;
  EditorSession session;
  FakeRenderer renderer;
  startSession(session, renderer);
  AE_EXPECT_TRUE(session.setProjectDirectory(project.root.string().c_str()), "projeto aberto");
  EditorSession::ResourceChangeReport change;
  AE_EXPECT_TRUE(!session.deleteResource(".astra", true, change), "recusa apagar o estado do projeto");
  AE_EXPECT_TRUE(std::filesystem::exists(project.root / ".astra"), "pasta intacta");
  AE_EXPECT_TRUE(!session.deleteResource("", true, change), "caminho vazio e recusado");
}

AE_TEST(the_visible_tree_follows_a_rename_on_disk) {
  // A arvore que o painel desenha e outra estrutura que a pasta corrente:
  // `refresh` recarregava so a segunda. Sem reler a primeira, um arquivo
  // renomeado continuava aparecendo com um nome que ja nao existia no disco --
  // e tocar nele abriria um caminho morto.
  ProjectDirectory project;
  std::ofstream(project.root / "Fontes" / "placa.glb", std::ios::binary) << "glb";
  EditorFileSystem files;
  AE_EXPECT_TRUE(files.setRoot(project.root.string().c_str()), "raiz");

  unsigned fontes = 0;
  for (unsigned index = 0; index < files.tree().size(); ++index)
    if (files.tree()[index].name == "Fontes") fontes = index;
  AE_EXPECT_TRUE(fontes != 0, "pasta na arvore");
  AE_EXPECT_TRUE(files.toggle(fontes), "expandir");

  bool before = false;
  for (const auto &entry : files.tree()) if (entry.name == "placa.glb") before = true;
  AE_EXPECT_TRUE(before, "o arquivo aparece expandido");

  AE_EXPECT_TRUE(files.movePath("Fontes/placa.glb", "Fontes/tabuleta.glb"), files.error().c_str());
  bool listed = false, stale = false, stillExpanded = false;
  for (const auto &entry : files.tree()) {
    if (entry.name == "tabuleta.glb") listed = true;
    if (entry.name == "placa.glb") stale = true;
    if (entry.name == "Fontes" && entry.expanded) stillExpanded = true;
  }
  AE_EXPECT_TRUE(listed, "o painel mostra o nome novo");
  AE_EXPECT_TRUE(!stale, "e nao mostra o antigo");
  AE_EXPECT_TRUE(stillExpanded, "a pasta continua aberta onde o usuario a deixou");

  AE_EXPECT_TRUE(files.removePath("Fontes/tabuleta.glb"), files.error().c_str());
  for (const auto &entry : files.tree())
    AE_EXPECT_TRUE(entry.name != "tabuleta.glb", "apagar tambem some da arvore");
}

AE_TEST(the_project_state_folder_does_not_show_up_in_the_panel) {
  // `.astra` guarda historico, registro e cache. Mostra-lo na navegacao normal
  // convida a apagar, e apaga-lo custa o projeto inteiro. O caminho continua
  // resolvivel por nome, que e como um diagnostico ainda aponta para dentro
  // dele.
  ProjectDirectory project;
  std::ofstream(project.root / ".astra" / "assets.txt") << "x";
  EditorFileSystem files;
  AE_EXPECT_TRUE(files.setRoot(project.root.string().c_str()), "raiz");
  for (const auto &entry : files.tree())
    AE_EXPECT_TRUE(entry.name != ".astra", "a pasta de estado nao aparece");
  AE_EXPECT_TRUE(!files.resolveFile(".astra/assets.txt").empty(),
                 "e continua alcancavel por quem pede por nome");

  files.showProjectState(true);
  AE_EXPECT_TRUE(files.rebuildTree(), "arvore com o estado do projeto");
  bool visible = false;
  for (const auto &entry : files.tree()) if (entry.name == ".astra") visible = true;
  AE_EXPECT_TRUE(visible, "o diagnostico pode pedir para ver");
}
