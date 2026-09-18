// O que uma reimportação faz com a CENA ABERTA, contado antes de publicar.
//
// O relatório de correspondência fala da fonte — quantos nós casaram, nasceram
// ou sumiram. Isso não responde a pergunta que o autor tem na mão ao aceitar:
// "o que acontece com o que eu já montei?". Um nó removido da fonte só vira
// perda quando existe objeto preso a ele, e só vira ÓRFÃO quando esse objeto
// carrega edição local. Estes testes fixam exatamente essa distinção.
#include "harness.h"

#include "editor/editor_import_reconcile.h"

using namespace ae;
using namespace ae::editor;

namespace {
resources::AssetGuid guid(u64 value) { return {0, value}; }

// Um objeto vinculado ao nó `node` da fonte, com a base igual ao estado atual —
// isto é, sem edição local. `move` desloca o objeto DEPOIS da base, que é o que
// faz `importOverrides` enxergar alteração do autor.
EditorEntityId linked(EditorDocument &document, const resources::AssetGuid &source, const resources::AssetGuid &node,
                      const char *name, bool move = false, bool unlink = false, bool orphan = false) {
  const auto id = document.createEntity(document.root(), EditorEntityKind::Mesh, name);
  auto values = *document.find(id);
  auto *link = scene::editImportLink(values.components);
  link->source = source;
  link->node = node;
  link->instance = guid(1000);
  link->revision = 1;
  link->unlinked = unlink;
  link->orphan = orphan;
  link->baseName = name;
  for (u32 axis = 0; axis < 3; ++axis) {
    link->basePosition[axis] = values.transform.position[axis];
    link->baseRotation[axis] = values.transform.rotationDegrees[axis];
    link->baseScale[axis] = values.transform.scale[axis];
  }
  if (move) values.transform.position[0] += 5;
  document.applyEntityValues(id, values);
  return id;
}
} // namespace

AE_TEST(scene_impact_separates_what_leaves_from_what_is_orphaned) {
  EditorDocument document;
  const auto source = guid(7);
  linked(document, source, guid(1), "Intacto");
  linked(document, source, guid(2), "Movido", true);
  linked(document, source, guid(3), "Some");
  linked(document, source, guid(4), "Guardado", true);

  resources::ImportMatchReport match;
  match.added = 2;
  // A fonte nova não tem mais os nós 3 e 4.
  match.removedNodes = {guid(3), guid(4)};
  match.removed = 2;

  const auto impact = importSceneImpact(document, source, match);
  AE_EXPECT_EQ(impact.linked, 4u, "os quatro objetos estão vinculados a esta fonte");
  AE_EXPECT_EQ(impact.instances, 1u, "todos da mesma instanciação");
  AE_EXPECT_EQ(impact.editedObjects, 2u, "dois têm edição local");
  AE_EXPECT_EQ(impact.removedObjects, 1u, "o que sumiu da fonte e não foi tocado sai da cena");
  AE_EXPECT_EQ(impact.orphanObjects, 1u, "o que sumiu da fonte e foi tocado fica órfão");
  AE_EXPECT_EQ(impact.orphanNames.size(), 1u, "com o nome que o autor reconhece");
  AE_EXPECT_EQ(impact.orphanNames.front(), std::string("Guardado"), "e é o objeto certo");
  AE_EXPECT_EQ(impact.newNodes, 2u, "os nós novos entram em cada instância");
  AE_EXPECT_TRUE(impact.touchesScene(), "esta reimportação mexe na cena");
}

AE_TEST(scene_impact_ignores_what_the_author_unlinked_or_already_orphaned) {
  EditorDocument document;
  const auto source = guid(7);
  linked(document, source, guid(1), "Vinculado");
  linked(document, source, guid(2), "Solto", false, true);
  linked(document, source, guid(3), "Órfão antigo", false, false, true);

  resources::ImportMatchReport match;
  // Os nós 2 e 3 sumiram, mas os objetos deles já não seguem a fonte.
  match.removedNodes = {guid(2), guid(3)};

  const auto impact = importSceneImpact(document, source, match);
  AE_EXPECT_EQ(impact.linked, 1u, "só um objeto ainda segue a fonte");
  AE_EXPECT_EQ(impact.unlinked, 1u, "o desvinculado é contado à parte");
  AE_EXPECT_EQ(impact.alreadyOrphan, 1u, "e o órfão antigo também");
  AE_EXPECT_EQ(impact.removedObjects, 0u, "nada sai por conta de quem já estava solto");
  AE_EXPECT_EQ(impact.orphanObjects, 0u, "e nada vira órfão duas vezes");
  AE_EXPECT_TRUE(!impact.touchesScene(), "esta reimportação não mexe na cena");
}

AE_TEST(scene_impact_counts_only_the_source_being_reimported) {
  EditorDocument document;
  linked(document, guid(7), guid(1), "Desta fonte");
  linked(document, guid(8), guid(1), "De outra fonte");

  resources::ImportMatchReport match;
  match.removedNodes = {guid(1)};

  const auto impact = importSceneImpact(document, guid(7), match);
  AE_EXPECT_EQ(impact.linked, 1u, "o objeto da outra fonte não entra na conta");
  AE_EXPECT_EQ(impact.removedObjects, 1u, "e o desta sai");
}
