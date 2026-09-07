#include "editor/editor_history.h"
#include "harness.h"

#include <cmath>

#include <cstring>

using namespace ae;
using namespace ae::editor;

namespace {

EditorTransform positionAt(float x, float y, float z) {
  EditorTransform transform{};
  transform.position[0] = x;
  transform.position[1] = y;
  transform.position[2] = z;
  return transform;
}

} // namespace

AE_TEST(history_undo_restores_the_previous_transform) {
  EditorDocument document;
  EditorHistory history;
  const EditorEntityId cube =
      history.createEntity(document, document.root(), EditorEntityKind::Mesh, "Concrete Block");
  AE_EXPECT_TRUE(cube != kInvalidEntity, "");
  AE_EXPECT_TRUE(history.setTransform(document, cube, positionAt(1.25f, 0.5f, -2.0f)), "");

  AE_EXPECT_TRUE(history.undo(document), "");
  AE_EXPECT_TRUE(document.find(cube)->transform.position[0] == 0.0f, "voltou ao transform inicial");
  AE_EXPECT_TRUE(history.redo(document), "");
  AE_EXPECT_TRUE(document.find(cube)->transform.position[0] == 1.25f, "e o refazer devolve");
}

AE_TEST(history_undo_of_a_creation_removes_the_entity) {
  EditorDocument document;
  EditorHistory history;
  const EditorEntityId cube =
      history.createEntity(document, document.root(), EditorEntityKind::Mesh, "Cube");
  AE_EXPECT_TRUE(history.undo(document), "");
  AE_EXPECT_TRUE(!document.exists(cube), "");
  AE_EXPECT_TRUE(history.redo(document), "");
  AE_EXPECT_TRUE(document.exists(cube), "");
  AE_EXPECT_EQ(document.find(cube)->id, cube, "o refazer devolve o MESMO id");
}

AE_TEST(history_undo_of_a_deletion_restores_the_whole_subtree_in_order) {
  // Apagar uma pasta com filhos tem de voltar em UM passo, com pais antes dos
  // filhos e cada um na mesma linha da hierarquia.
  EditorDocument document;
  EditorHistory history;
  const EditorEntityId folder =
      history.createEntity(document, document.root(), EditorEntityKind::Folder, "Architecture");
  const EditorEntityId wall =
      history.createEntity(document, folder, EditorEntityKind::Mesh, "Glass Wall");
  const EditorEntityId block =
      history.createEntity(document, folder, EditorEntityKind::Mesh, "Concrete Block");
  const EditorEntityId inner =
      history.createEntity(document, block, EditorEntityKind::Mesh, "Bolt");

  AE_EXPECT_TRUE(history.destroyEntity(document, folder), "");
  AE_EXPECT_TRUE(!document.exists(inner), "");

  AE_EXPECT_TRUE(history.undo(document), "");
  AE_EXPECT_TRUE(document.exists(folder) && document.exists(wall) && document.exists(block) &&
                     document.exists(inner),
                 "a subarvore inteira voltou num passo so");
  const auto children = document.childrenOf(folder);
  AE_EXPECT_EQ(children.size(), usize{2}, "");
  AE_EXPECT_EQ(children[0], wall, "a ordem dos irmaos foi preservada");
  AE_EXPECT_EQ(children[1], block, "");
  AE_EXPECT_EQ(document.childrenOf(block)[0], inner, "e o neto voltou para o pai certo");
}

AE_TEST(history_redo_of_a_deletion_removes_it_again) {
  EditorDocument document;
  EditorHistory history;
  const EditorEntityId folder =
      history.createEntity(document, document.root(), EditorEntityKind::Folder, "F");
  const EditorEntityId child =
      history.createEntity(document, folder, EditorEntityKind::Mesh, "C");
  history.destroyEntity(document, folder);
  history.undo(document);
  AE_EXPECT_TRUE(history.redo(document), "");
  AE_EXPECT_TRUE(!document.exists(folder) && !document.exists(child), "");
}

AE_TEST(history_a_gizmo_drag_is_a_single_undo_step) {
  // O caso que motiva a fusão: centenas de eventos de movimento durante um
  // arraste não podem virar centenas de Ctrl+Z.
  EditorDocument document;
  EditorHistory history;
  const EditorEntityId cube =
      history.createEntity(document, document.root(), EditorEntityKind::Mesh, "Cube");
  const u32 depthBefore = history.undoDepth();

  AE_EXPECT_TRUE(history.begin("Move"), "");
  const EditorMergeToken drag = 7;
  for (u32 step = 1; step <= 120; ++step)
    AE_EXPECT_TRUE(history.setTransform(document, cube, positionAt(static_cast<float>(step), 0, 0),
                                        drag),
                   "");
  history.end();

  AE_EXPECT_EQ(history.undoDepth(), depthBefore + 1, "o arraste inteiro e um passo");
  AE_EXPECT_TRUE(document.find(cube)->transform.position[0] == 120.0f, "");
  AE_EXPECT_TRUE(history.undo(document), "");
  AE_EXPECT_TRUE(document.find(cube)->transform.position[0] == 0.0f,
                 "e voltar desfaz o gesto inteiro, nao o ultimo frame dele");
}

AE_TEST(history_different_merge_tokens_do_not_fuse) {
  EditorDocument document;
  EditorHistory history;
  const EditorEntityId cube =
      history.createEntity(document, document.root(), EditorEntityKind::Mesh, "Cube");
  history.begin("Two drags");
  history.setTransform(document, cube, positionAt(1, 0, 0), 1);
  history.setTransform(document, cube, positionAt(2, 0, 0), 2);
  history.end();
  history.undo(document);
  AE_EXPECT_TRUE(document.find(cube)->transform.position[0] == 0.0f,
                 "a transacao volta inteira, mas os dois comandos continuam distintos");
}

AE_TEST(history_zero_token_never_fuses) {
  // Interruptores do Inspector são passos independentes: apertar Visible e
  // depois Cast Shadow tem de dar dois undos.
  EditorDocument document;
  EditorHistory history;
  const EditorEntityId cube =
      history.createEntity(document, document.root(), EditorEntityKind::Mesh, "Cube");
  const u32 before = history.undoDepth();
  EditorEntity values = *document.find(cube);
  values.visible = false;
  history.applyValues(document, cube, values);
  values.castShadow = false;
  history.applyValues(document, cube, values);
  AE_EXPECT_EQ(history.undoDepth(), before + 2, "dois interruptores, dois passos");
}

AE_TEST(history_a_new_edit_discards_the_redo_future) {
  EditorDocument document;
  EditorHistory history;
  const EditorEntityId cube =
      history.createEntity(document, document.root(), EditorEntityKind::Mesh, "Cube");
  history.setTransform(document, cube, positionAt(5, 0, 0));
  history.undo(document);
  AE_EXPECT_TRUE(history.canRedo(), "");
  history.setTransform(document, cube, positionAt(0, 9, 0));
  AE_EXPECT_TRUE(!history.canRedo(),
                 "refazer sobre um documento diferente acertaria outros objetos");
}

AE_TEST(history_undo_closes_an_open_transaction_first) {
  // Um undo no meio de um arraste desfaria metade do gesto.
  EditorDocument document;
  EditorHistory history;
  const EditorEntityId cube =
      history.createEntity(document, document.root(), EditorEntityKind::Mesh, "Cube");
  history.begin("Move");
  history.setTransform(document, cube, positionAt(3, 0, 0), 5);
  AE_EXPECT_TRUE(history.isOpen(), "");
  AE_EXPECT_TRUE(history.undo(document), "");
  AE_EXPECT_TRUE(!history.isOpen(), "");
  AE_EXPECT_TRUE(document.find(cube)->transform.position[0] == 0.0f, "o gesto inteiro voltou");
}

AE_TEST(history_an_empty_transaction_is_not_a_step) {
  EditorDocument document;
  EditorHistory history;
  const u32 before = history.undoDepth();
  history.begin("Nothing");
  history.end();
  AE_EXPECT_EQ(history.undoDepth(), before,
               "um Ctrl+Z que nao faz nada visivel e pior que nenhum");
}

AE_TEST(history_nested_begin_is_refused) {
  EditorHistory history;
  AE_EXPECT_TRUE(history.begin("Outer"), "");
  AE_EXPECT_TRUE(!history.begin("Inner"),
                 "aninhar exigiria decidir o que desfazer significa no meio de um grupo");
  history.end();
}

AE_TEST(history_labels_describe_the_next_step) {
  EditorDocument document;
  EditorHistory history;
  history.begin("Add Cube");
  history.createEntity(document, document.root(), EditorEntityKind::Mesh, "Cube");
  history.end();
  AE_EXPECT_TRUE(history.undoLabel() == "Add Cube", "");
  history.undo(document);
  AE_EXPECT_TRUE(history.redoLabel() == "Add Cube", "");
  AE_EXPECT_TRUE(history.undoLabel().empty(), "sem passo, sem rotulo");
}

AE_TEST(history_reparent_round_trips) {
  EditorDocument document;
  EditorHistory history;
  const EditorEntityId first =
      history.createEntity(document, document.root(), EditorEntityKind::Folder, "A");
  const EditorEntityId second =
      history.createEntity(document, document.root(), EditorEntityKind::Folder, "B");
  const EditorEntityId cube =
      history.createEntity(document, first, EditorEntityKind::Mesh, "Cube");

  AE_EXPECT_TRUE(history.reparent(document, cube, second, 0), "");
  AE_EXPECT_EQ(document.find(cube)->parent, second, "");
  AE_EXPECT_TRUE(history.undo(document), "");
  AE_EXPECT_EQ(document.find(cube)->parent, first, "");
  AE_EXPECT_EQ(document.childrenOf(first)[0], cube, "e na mesma posicao");
}

AE_TEST(history_does_not_grow_without_bound) {
  EditorDocument document;
  EditorHistory history;
  const EditorEntityId cube =
      history.createEntity(document, document.root(), EditorEntityKind::Mesh, "Cube");
  for (u32 step = 0; step < EditorHistory::kMaximumTransactions + 50; ++step)
    history.setTransform(document, cube, positionAt(static_cast<float>(step), 0, 0));
  AE_EXPECT_TRUE(history.undoDepth() <= EditorHistory::kMaximumTransactions,
                 "um editor movel nao troca memoria por um historico que ninguem percorre");
}

AE_TEST(history_undo_and_redo_are_no_ops_when_there_is_nothing_to_do) {
  EditorDocument document;
  EditorHistory history;
  AE_EXPECT_TRUE(!history.undo(document), "");
  AE_EXPECT_TRUE(!history.redo(document), "");
  AE_EXPECT_EQ(document.revision(), u64{0}, "e nada no documento foi tocado");
}

AE_TEST(history_clear_forgets_both_directions) {
  EditorDocument document;
  EditorHistory history;
  history.createEntity(document, document.root(), EditorEntityKind::Mesh, "Cube");
  history.undo(document);
  history.clear();
  AE_EXPECT_TRUE(!history.canUndo() && !history.canRedo(), "");
}
