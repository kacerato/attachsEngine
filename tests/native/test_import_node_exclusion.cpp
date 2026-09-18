// Excluir um nó da importação sem perder a identidade dele (G2, perfil por nó).
//
// O que se protege aqui é a diferença entre "a fonte perdeu o nó" e "o autor
// não quer o nó na cena". As duas tiram o objeto da cena pela mesma regra; só
// a segunda guarda a identidade, para que reincluir depois traga de volta o
// MESMO nó — e não um nó novo com outra identidade, que deixaria órfão tudo o
// que o autor tinha montado em cima dele.
#include "harness.h"

#include "editor/editor_import_reconcile.h"
#include "resources/import_node_map.h"
#include "resources/import_profile.h"

#include <algorithm>

using namespace ae;
using namespace ae::editor;
using namespace ae::resources;

namespace {
AssetGuid guid(u64 value) { return {0, value}; }

// Um modelo mínimo montado à mão, sem passar pelo leitor de GLB: raiz "Carro"
// com dois filhos, "Porta" e "Roda", e "Maçaneta" dentro da porta. Cada nó tem
// um desenho, para que a assinatura geométrica distinga os nós.
GltfImport car() {
  GltfImport model;
  const char *names[]{"Carro", "Porta", "Roda", "Maçaneta"};
  const i32 parents[]{-1, 0, 0, 1};
  for (u32 n = 0; n < 4; ++n) {
    GltfImportNode node;
    node.name = names[n];
    node.parent = parents[n];
    model.nodes.push_back(node);
    renderer::MapDrawRecord draw{};
    draw.indexCount = 3 * (n + 1);
    draw.boundsRadius = static_cast<float>(n + 1);
    model.draws.push_back(draw);
    model.drawNodes.push_back(n);
    model.keys.push_back(std::string(names[n]) + "#0");
    model.names.push_back(names[n]);
  }
  return model;
}

const ImportNodeRecord *byName(const ImportNodeMap &map, std::string_view name) {
  for (const auto &node : map.nodes) if (node.name == name) return &node;
  return nullptr;
}
} // namespace

AE_TEST(excluding_a_node_marks_its_whole_subtree_and_keeps_identity) {
  const auto model = car();
  ImportNodeMap first, second;
  ImportMatchReport report;
  std::string diagnostic;
  AE_EXPECT_TRUE(buildImportNodeMap(model, guid(9), "h", nullptr, ImportAmbiguityPolicy::Refuse, first, report, diagnostic),
                 diagnostic.c_str());
  const auto door = byName(first, "Porta")->id;

  const AssetGuid excluded[]{door};
  AE_EXPECT_TRUE(buildImportNodeMap(model, guid(9), "h", &first, ImportAmbiguityPolicy::Refuse, second, report, diagnostic,
                                    excluded),
                 diagnostic.c_str());
  AE_EXPECT_TRUE(byName(second, "Porta")->excluded, "o nó pedido está excluído");
  AE_EXPECT_TRUE(byName(second, "Maçaneta")->excluded, "e o filho dele também: não se cria filho sem o pai");
  AE_EXPECT_TRUE(!byName(second, "Roda")->excluded, "o irmão não é tocado");
  AE_EXPECT_TRUE(byName(second, "Porta")->id == door, "o nó excluído continua no mapa com a mesma identidade");
  AE_EXPECT_EQ(report.excluded, 2u, "a exclusão nova é contada, com a subárvore");
  AE_EXPECT_EQ(report.removed, 0u, "excluir não é a fonte ter perdido o nó");
}

AE_TEST(changing_only_the_exclusion_advances_the_revision_and_reintroduces) {
  const auto model = car();
  ImportNodeMap first, excludedMap, back;
  ImportMatchReport report;
  std::string diagnostic;
  AE_EXPECT_TRUE(buildImportNodeMap(model, guid(9), "h", nullptr, ImportAmbiguityPolicy::Refuse, first, report, diagnostic),
                 diagnostic.c_str());
  const auto door = byName(first, "Porta")->id;
  const AssetGuid excluded[]{door};

  // Mesmos bytes ("h"), exclusão diferente: a revisão avança mesmo assim.
  AE_EXPECT_TRUE(buildImportNodeMap(model, guid(9), "h", &first, ImportAmbiguityPolicy::Refuse, excludedMap, report,
                                    diagnostic, excluded),
                 diagnostic.c_str());
  AE_EXPECT_TRUE(report.sameContent, "o arquivo é o mesmo");
  AE_EXPECT_EQ(excludedMap.revision, first.revision + 1, "e a revisão avança, porque o que a cena recebe mudou");

  // Reincluir: o nó volta com a MESMA identidade e reintroduzido na revisão
  // nova. Sem isso, a instância reconciliada antes trataria o objeto ausente
  // como "apagado pelo autor" e nunca o traria de volta.
  AE_EXPECT_TRUE(buildImportNodeMap(model, guid(9), "h", &excludedMap, ImportAmbiguityPolicy::Refuse, back, report,
                                    diagnostic),
                 diagnostic.c_str());
  AE_EXPECT_EQ(report.included, 2u, "porta e maçaneta voltam");
  AE_EXPECT_TRUE(byName(back, "Porta")->id == door, "com a mesma identidade de antes");
  AE_EXPECT_EQ(byName(back, "Porta")->introduced, back.revision, "reintroduzido na revisão nova");
  AE_EXPECT_TRUE(byName(back, "Roda")->introduced < back.revision, "o que nunca saiu mantém a revisão de origem");
}

AE_TEST(the_map_file_keeps_the_exclusion) {
  const auto model = car();
  ImportNodeMap map, read;
  ImportMatchReport report;
  std::string diagnostic;
  const AssetGuid none[]{guid(1)}; // identidade que não existe: não exclui nada, não quebra
  AE_EXPECT_TRUE(buildImportNodeMap(model, guid(9), "h", nullptr, ImportAmbiguityPolicy::Refuse, map, report, diagnostic, none),
                 diagnostic.c_str());
  markExcludedNodes(map, std::vector<AssetGuid>{byName(map, "Roda")->id});
  AE_EXPECT_TRUE(ImportNodeMap::deserialize(map.serialize(), read), "o mapa com exclusão relê");
  AE_EXPECT_TRUE(byName(read, "Roda")->excluded, "a exclusão sobrevive ao arquivo");
  AE_EXPECT_TRUE(!byName(read, "Porta")->excluded, "e só ela");
}

AE_TEST(the_import_profile_stores_excluded_nodes_by_identity) {
  ImportProfile profile;
  profile.excludedNodes = {guid(0x2a), guid(0x2b)};
  ImportProfile parsed;
  AE_EXPECT_TRUE(parseImportProfile(serializeImportProfile(profile), parsed), "ida e volta");
  AE_EXPECT_TRUE(sameImportProfile(parsed, profile), "a lista voltou igual");

  // A ordem da exclusão não é escolha do autor.
  ImportProfile reordered = profile;
  std::reverse(reordered.excludedNodes.begin(), reordered.excludedNodes.end());
  AE_EXPECT_TRUE(sameImportProfile(reordered, profile), "mesmos nós em outra ordem é o mesmo perfil");

  // Exclusão não pede nova preparação: a saída do importador é a mesma.
  ImportProfile none;
  AE_EXPECT_TRUE(sameImportPreparation(profile, none), "excluir nó não relê o arquivo");
  AE_EXPECT_TRUE(!sameImportProfile(profile, none), "mas é outro perfil para salvar");

  // Identidade ilegível recusa o arquivo inteiro, em vez de trazer o nó de volta.
  AE_EXPECT_TRUE(!parseImportProfile(
                     R"({"schema":4,"scale":1,"maximumTextureDimension":512,"normals":0,"normalWeighting":0,"tangents":0,"importCameras":false,"excludedNodes":["nao-e-guid"]})",
                     parsed),
                 "exclusão ilegível não vira exclusão nenhuma");
}

namespace {
// Um objeto vinculado a um nó do mapa, com a base igual ao estado atual.
EditorEntityId linkedObject(EditorDocument &document, const AssetGuid &source, const ImportNodeRecord &node,
                            u32 revision, bool edited = false) {
  const auto id = document.createEntity(document.root(), EditorEntityKind::Mesh, node.name);
  auto values = *document.find(id);
  auto *link = scene::editImportLink(values.components);
  link->source = source;
  link->node = node.id;
  link->instance = guid(500);
  link->revision = revision;
  link->baseName = node.name;
  if (edited) values.transform.position[1] += 3; // edição local: o autor moveu o objeto
  document.applyEntityValues(id, values);
  return id;
}
} // namespace

AE_TEST(scene_impact_counts_objects_of_excluded_nodes) {
  const auto model = car();
  ImportNodeMap map;
  ImportMatchReport report;
  std::string diagnostic;
  AE_EXPECT_TRUE(buildImportNodeMap(model, guid(9), "h", nullptr, ImportAmbiguityPolicy::Refuse, map, report, diagnostic),
                 diagnostic.c_str());
  EditorDocument document;
  linkedObject(document, guid(9), *byName(map, "Porta"), map.revision);
  linkedObject(document, guid(9), *byName(map, "Roda"), map.revision, true);

  markExcludedNodes(map, std::vector<AssetGuid>{byName(map, "Porta")->id, byName(map, "Roda")->id});
  const auto impact = importSceneImpact(document, guid(9), ImportMatchReport{}, &map);
  AE_EXPECT_EQ(impact.excludedObjects, 2u, "os dois objetos estão presos a nós excluídos");
  AE_EXPECT_EQ(impact.removedObjects, 1u, "o que não foi tocado sai da cena");
  AE_EXPECT_EQ(impact.orphanObjects, 1u, "o que o autor moveu fica órfão, com a edição dentro");
}

namespace {
// Uma instância completa do modelo, como a instanciação deixaria: um objeto por
// nó, na hierarquia do arquivo, com vínculo e malha iguais à base — isto é, sem
// nenhuma edição local.
std::vector<EditorEntityId> instantiate(EditorDocument &document, const AssetGuid &source, const ImportNodeMap &map) {
  std::vector<EditorEntityId> ids;
  for (const auto &node : map.nodes) {
    EditorEntityId parent = document.root();
    if (node.parent.valid())
      for (usize i = 0; i < map.nodes.size(); ++i)
        if (map.nodes[i].id == node.parent) parent = ids[i];
    const auto id = document.createEntity(parent, EditorEntityKind::Mesh, importEntityName(node.name));
    auto values = *document.find(id);
    EditorTransform transform;
    importNodeTransform(node, transform);
    values.transform = transform;
    auto *link = scene::editImportLink(values.components);
    link->source = source;
    link->instance = guid(700);
    setImportLinkBase(*link, node, transform, -1, map.revision);
    if (auto *mesh = editMeshRenderer(values)) mesh->asset = node.draws.front();
    document.applyEntityValues(id, values);
    ids.push_back(id);
  }
  return ids;
}
bool present(const EditorDocument &document, std::string_view name) {
  std::vector<EditorEntityId> ids;
  document.collectSubtree(document.root(), ids);
  for (const auto id : ids) if (document.find(id)->name == name) return true;
  return false;
}
const scene::ImportLink *linkNamed(const EditorDocument &document, std::string_view name) {
  std::vector<EditorEntityId> ids;
  document.collectSubtree(document.root(), ids);
  for (const auto id : ids)
    if (document.find(id)->name == name) return scene::importLink(document.find(id)->components);
  return nullptr;
}
} // namespace

AE_TEST(reconcile_removes_excluded_nodes_and_brings_the_same_node_back) {
  const auto model = car();
  const auto source = guid(9);
  ImportNodeMap first, excludedMap, back;
  ImportMatchReport match;
  std::string diagnostic;
  AE_EXPECT_TRUE(buildImportNodeMap(model, source, "h", nullptr, ImportAmbiguityPolicy::Refuse, first, match, diagnostic),
                 diagnostic.c_str());
  EditorDocument document;
  instantiate(document, source, first);
  AE_EXPECT_TRUE(present(document, "Porta") && present(document, "Maçaneta"), "a instância começa completa");
  const auto door = byName(first, "Porta")->id;

  // Excluir a porta: ela e a maçaneta saem, porque não carregam edição local.
  const AssetGuid excluded[]{door};
  AE_EXPECT_TRUE(buildImportNodeMap(model, source, "h", &first, ImportAmbiguityPolicy::Refuse, excludedMap, match,
                                    diagnostic, excluded),
                 diagnostic.c_str());
  ImportReconcileReport report;
  AE_EXPECT_TRUE(reconcileImportInstances(document, nullptr, source, excludedMap, report), "reconciliação com exclusão");
  AE_EXPECT_TRUE(!present(document, "Porta") && !present(document, "Maçaneta"), "o nó excluído e a subárvore saem da cena");
  AE_EXPECT_TRUE(present(document, "Carro") && present(document, "Roda"), "o resto fica");
  AE_EXPECT_EQ(report.orphaned, 0u, "nada tinha edição local, então nada ficou órfão");

  // Reincluir: a MESMA porta volta, com a mesma identidade de nó. É isto que a
  // exclusão guarda e a remoção pela fonte não guardaria.
  AE_EXPECT_TRUE(buildImportNodeMap(model, source, "h", &excludedMap, ImportAmbiguityPolicy::Refuse, back, match, diagnostic),
                 diagnostic.c_str());
  report = {};
  AE_EXPECT_TRUE(reconcileImportInstances(document, nullptr, source, back, report), "reconciliação com reinclusão");
  AE_EXPECT_TRUE(present(document, "Porta") && present(document, "Maçaneta"), "porta e maçaneta voltaram");
  const auto *link = linkNamed(document, "Porta");
  AE_EXPECT_TRUE(link && link->node == door, "e a porta que voltou é o MESMO nó de antes");
  AE_EXPECT_EQ(report.keptDeleted, 0u, "a reinclusão não é confundida com objeto apagado pelo autor");
}

AE_TEST(excluding_a_node_the_author_edited_keeps_the_object_as_orphan) {
  const auto model = car();
  const auto source = guid(9);
  ImportNodeMap first, excludedMap;
  ImportMatchReport match;
  std::string diagnostic;
  AE_EXPECT_TRUE(buildImportNodeMap(model, source, "h", nullptr, ImportAmbiguityPolicy::Refuse, first, match, diagnostic),
                 diagnostic.c_str());
  EditorDocument document;
  const auto ids = instantiate(document, source, first);
  // O autor moveu a roda depois de importar.
  auto values = *document.find(ids[2]);
  values.transform.position[0] += 4;
  document.applyEntityValues(ids[2], values);

  const AssetGuid excluded[]{byName(first, "Roda")->id};
  AE_EXPECT_TRUE(buildImportNodeMap(model, source, "h", &first, ImportAmbiguityPolicy::Refuse, excludedMap, match,
                                    diagnostic, excluded),
                 diagnostic.c_str());
  ImportReconcileReport report;
  AE_EXPECT_TRUE(reconcileImportInstances(document, nullptr, source, excludedMap, report), "reconciliação");
  AE_EXPECT_TRUE(present(document, "Roda"), "a roda editada não some com a exclusão");
  const auto *link = linkNamed(document, "Roda");
  AE_EXPECT_TRUE(link && link->orphan, "ela fica órfã, com a edição dentro, esperando decisão do autor");
  AE_EXPECT_EQ(report.orphaned, 1u, "e o relatório diz isso");
}
