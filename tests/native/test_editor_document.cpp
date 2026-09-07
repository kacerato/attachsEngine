#include "editor/editor_document.h"
#include "harness.h"

#include <cmath>

#include <cstring>
#include <string>

using namespace ae;
using namespace ae::editor;

namespace {

// A hierarquia do mockup: pastas por assunto, com os objetos dentro.
struct WaterLab final {
  EditorDocument document;
  EditorEntityId environment = kInvalidEntity;
  EditorEntityId architecture = kInvalidEntity;
  EditorEntityId concreteBlock = kInvalidEntity;
  EditorEntityId glassWall = kInvalidEntity;
};

WaterLab buildWaterLab() {
  WaterLab lab;
  lab.environment =
      lab.document.createEntity(lab.document.root(), EditorEntityKind::Folder, "Environment");
  lab.document.createEntity(lab.environment, EditorEntityKind::Light, "Sky");
  lab.architecture =
      lab.document.createEntity(lab.document.root(), EditorEntityKind::Folder, "Architecture");
  lab.glassWall =
      lab.document.createEntity(lab.architecture, EditorEntityKind::Mesh, "Glass Wall");
  lab.concreteBlock =
      lab.document.createEntity(lab.architecture, EditorEntityKind::Mesh, "Concrete Block");
  return lab;
}

} // namespace

AE_TEST(document_starts_with_a_root_that_cannot_be_removed) {
  EditorDocument document;
  AE_EXPECT_TRUE(document.root() != kInvalidEntity, "");
  AE_EXPECT_TRUE(document.exists(document.root()), "");
  AE_EXPECT_TRUE(!document.destroyEntity(document.root()),
                 "a raiz e o alvo de 'adicionar a cena': destrui-la deixaria o editor sem chao");
  AE_EXPECT_TRUE(document.childrenOf(document.root()).empty(), "");
}

AE_TEST(document_children_keep_the_order_they_were_added) {
  // A hierarquia do mockup é uma lista ordenada, não um conjunto: reordenar
  // sozinha faria a árvore embaralhar a cada recarga.
  const WaterLab lab = buildWaterLab();
  const auto children = lab.document.childrenOf(lab.architecture);
  AE_EXPECT_EQ(children.size(), usize{2}, "");
  AE_EXPECT_EQ(children[0], lab.glassWall, "");
  AE_EXPECT_EQ(children[1], lab.concreteBlock, "");
}

AE_TEST(document_destroy_takes_the_whole_subtree) {
  WaterLab lab = buildWaterLab();
  AE_EXPECT_TRUE(lab.document.destroyEntity(lab.architecture), "");
  AE_EXPECT_TRUE(!lab.document.exists(lab.architecture), "");
  AE_EXPECT_TRUE(!lab.document.exists(lab.concreteBlock),
                 "um filho orfao continuaria vivo e invisivel na hierarquia");
  AE_EXPECT_TRUE(!lab.document.exists(lab.glassWall), "");
  AE_EXPECT_TRUE(lab.document.exists(lab.environment), "o irmao nao e afetado");
  AE_EXPECT_EQ(lab.document.childrenOf(lab.document.root()).size(), usize{1}, "");
}

AE_TEST(document_ids_are_never_recycled) {
  // Um id reciclado faria um comando antigo do historico acertar outro objeto.
  WaterLab lab = buildWaterLab();
  const EditorEntityId destroyed = lab.concreteBlock;
  AE_EXPECT_TRUE(lab.document.destroyEntity(destroyed), "");
  const EditorEntityId created =
      lab.document.createEntity(lab.architecture, EditorEntityKind::Mesh, "Novo");
  AE_EXPECT_TRUE(created != destroyed, "");
}

AE_TEST(document_restore_puts_the_entity_back_with_the_same_id_and_line) {
  WaterLab lab = buildWaterLab();
  const EditorEntity copy = *lab.document.find(lab.glassWall);
  u32 index = 0;
  AE_EXPECT_TRUE(lab.document.childIndexOf(lab.glassWall, index), "");
  AE_EXPECT_EQ(index, 0u, "era o primeiro filho");
  AE_EXPECT_TRUE(lab.document.destroyEntity(lab.glassWall), "");
  AE_EXPECT_TRUE(lab.document.restoreEntity(copy, index), "");
  AE_EXPECT_TRUE(lab.document.exists(lab.glassWall), "");
  const auto children = lab.document.childrenOf(lab.architecture);
  AE_EXPECT_EQ(children[0], lab.glassWall, "voltou para a mesma linha da hierarquia");
}

AE_TEST(document_restore_refuses_to_duplicate_a_live_id) {
  WaterLab lab = buildWaterLab();
  const EditorEntity copy = *lab.document.find(lab.glassWall);
  AE_EXPECT_TRUE(!lab.document.restoreEntity(copy, 0),
                 "dois registros com o mesmo id deixariam um deles inalcancavel");
}

AE_TEST(document_reparent_moves_the_entity_and_its_subtree) {
  WaterLab lab = buildWaterLab();
  AE_EXPECT_TRUE(lab.document.reparent(lab.concreteBlock, lab.environment, 0), "");
  AE_EXPECT_EQ(lab.document.find(lab.concreteBlock)->parent, lab.environment, "");
  AE_EXPECT_EQ(lab.document.childrenOf(lab.environment)[0], lab.concreteBlock,
               "entrou no indice pedido");
  AE_EXPECT_EQ(lab.document.childrenOf(lab.architecture).size(), usize{1},
               "e saiu da lista antiga");
}

AE_TEST(document_reparent_into_its_own_subtree_is_refused) {
  // Um ciclo faria a entidade sumir da arvore e toda travessia entrar em laco.
  WaterLab lab = buildWaterLab();
  AE_EXPECT_TRUE(!lab.document.reparent(lab.architecture, lab.concreteBlock, 0), "");
  AE_EXPECT_TRUE(!lab.document.reparent(lab.architecture, lab.architecture, 0),
                 "nem para dentro de si mesma");
  AE_EXPECT_EQ(lab.document.find(lab.architecture)->parent, lab.document.root(),
               "e nada mudou");
}

AE_TEST(document_reparent_clamps_an_out_of_range_index) {
  WaterLab lab = buildWaterLab();
  AE_EXPECT_TRUE(lab.document.reparent(lab.concreteBlock, lab.environment, 999), "");
  const auto children = lab.document.childrenOf(lab.environment);
  AE_EXPECT_EQ(children[children.size() - 1], lab.concreteBlock, "entrou no fim");
}

AE_TEST(document_root_cannot_be_reparented) {
  EditorDocument document;
  const EditorEntityId folder =
      document.createEntity(document.root(), EditorEntityKind::Folder, "F");
  AE_EXPECT_TRUE(!document.reparent(document.root(), folder, 0), "");
}

AE_TEST(document_names_are_truncated_and_always_terminated) {
  EditorDocument document;
  const std::string longName(200, 'A');
  const EditorEntityId id =
      document.createEntity(document.root(), EditorEntityKind::Mesh, longName);
  const EditorEntity *entity = document.find(id);
  AE_EXPECT_TRUE(entity != nullptr, "");
  AE_EXPECT_EQ(std::strlen(entity->name), usize{kEditorNameCapacity - 1},
               "o nome ocupa a capacidade menos o terminador");
  AE_EXPECT_EQ(entity->name[kEditorNameCapacity - 1], '\0',
               "sem terminador, qualquer leitura de nome sairia do buffer");
}

AE_TEST(document_refuses_an_empty_name) {
  EditorDocument document;
  const EditorEntityId id =
      document.createEntity(document.root(), EditorEntityKind::Mesh, "Cube");
  AE_EXPECT_TRUE(!document.setName(id, ""),
                 "uma linha em branco na hierarquia e indistinguivel de erro de desenho");
  AE_EXPECT_TRUE(std::strcmp(document.find(id)->name, "Cube") == 0, "");
}

AE_TEST(document_refuses_a_transform_that_would_break_the_renderer) {
  EditorDocument document;
  const EditorEntityId id =
      document.createEntity(document.root(), EditorEntityKind::Mesh, "Cube");
  EditorTransform notFinite{};
  notFinite.position[1] = std::nanf("");
  AE_EXPECT_TRUE(!document.setTransform(id, notFinite), "NaN entraria em toda matriz derivada");

  EditorTransform mirrored{};
  mirrored.scale[0] = -1.0f;
  AE_EXPECT_TRUE(!document.setTransform(id, mirrored),
                 "escala negativa inverte as faces sem nenhum aviso na tela");

  EditorTransform flattened{};
  flattened.scale[2] = 0.0f;
  AE_EXPECT_TRUE(document.setTransform(id, flattened), "achatar de proposito e legal");
}

AE_TEST(document_apply_values_never_reparents_or_changes_identity) {
  WaterLab lab = buildWaterLab();
  EditorEntity values = *lab.document.find(lab.concreteBlock);
  values.id = 999;
  values.parent = lab.environment;
  values.transform.position[0] = 1.25f;
  values.visible = false;
  AE_EXPECT_TRUE(lab.document.applyEntityValues(lab.concreteBlock, values), "");

  const EditorEntity *updated = lab.document.find(lab.concreteBlock);
  AE_EXPECT_EQ(updated->id, lab.concreteBlock, "identidade nao vem dos valores");
  AE_EXPECT_EQ(updated->parent, lab.architecture,
               "mudar de pai e reparent, nao um efeito colateral de editar campos");
  AE_EXPECT_TRUE(updated->transform.position[0] == 1.25f, "");
  AE_EXPECT_TRUE(!updated->visible, "");
}

AE_TEST(document_revision_moves_only_on_accepted_mutations) {
  // A revisão é o que diz ao renderer que a extração anterior não vale mais.
  // Subir numa mutação recusada forçaria um reextração inteira à toa.
  EditorDocument document;
  const u64 initial = document.revision();
  const EditorEntityId id =
      document.createEntity(document.root(), EditorEntityKind::Mesh, "Cube");
  AE_EXPECT_TRUE(document.revision() > initial, "");
  const u64 afterCreate = document.revision();
  EditorTransform broken{};
  broken.scale[0] = -2.0f;
  AE_EXPECT_TRUE(!document.setTransform(id, broken), "");
  AE_EXPECT_EQ(document.revision(), afterCreate, "recusa nao conta como mudanca");
}

AE_TEST(document_subtree_is_collected_in_preorder) {
  WaterLab lab = buildWaterLab();
  std::vector<EditorEntityId> subtree;
  lab.document.collectSubtree(lab.architecture, subtree);
  AE_EXPECT_EQ(subtree.size(), usize{3}, "");
  AE_EXPECT_EQ(subtree[0], lab.architecture, "a propria entidade vem primeiro");
  AE_EXPECT_EQ(subtree[1], lab.glassWall, "");
  AE_EXPECT_EQ(subtree[2], lab.concreteBlock, "");
}

AE_TEST(document_descendant_test_is_reflexive_and_bounded) {
  WaterLab lab = buildWaterLab();
  AE_EXPECT_TRUE(lab.document.isDescendantOf(lab.concreteBlock, lab.architecture), "");
  AE_EXPECT_TRUE(lab.document.isDescendantOf(lab.concreteBlock, lab.document.root()), "");
  AE_EXPECT_TRUE(lab.document.isDescendantOf(lab.architecture, lab.architecture),
                 "reflexiva de proposito: e o que faz reparent recusar o proprio pai");
  AE_EXPECT_TRUE(!lab.document.isDescendantOf(lab.architecture, lab.concreteBlock), "");
  AE_EXPECT_TRUE(!lab.document.isDescendantOf(kInvalidEntity, lab.document.root()), "");
}

AE_TEST(document_operations_on_unknown_ids_fail_without_touching_anything) {
  WaterLab lab = buildWaterLab();
  const u64 revision = lab.document.revision();
  AE_EXPECT_TRUE(!lab.document.setName(4242, "X"), "");
  AE_EXPECT_TRUE(!lab.document.setTransform(4242, EditorTransform{}), "");
  AE_EXPECT_TRUE(!lab.document.destroyEntity(4242), "");
  AE_EXPECT_TRUE(!lab.document.reparent(4242, lab.document.root(), 0), "");
  AE_EXPECT_TRUE(lab.document.createEntity(4242, EditorEntityKind::Mesh, "X") == kInvalidEntity,
                 "");
  AE_EXPECT_EQ(lab.document.revision(), revision, "");
}

AE_TEST(document_reset_returns_to_a_single_root) {
  WaterLab lab = buildWaterLab();
  lab.document.reset();
  AE_EXPECT_TRUE(lab.document.childrenOf(lab.document.root()).empty(), "");
  AE_EXPECT_TRUE(!lab.document.exists(lab.concreteBlock), "");
  AE_EXPECT_EQ(lab.document.revision(), u64{0}, "documento novo comeca do zero");
}
