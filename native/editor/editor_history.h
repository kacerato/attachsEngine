// Undo/redo do editor.
//
// O histórico é dono das mutações: quem quer mudar a cena chama um método daqui,
// que altera o documento **e** registra como desfazer. Uma API que deixasse a UI
// mudar o documento e "avisar" o histórico depois produziria, inevitavelmente,
// campos que o Ctrl+Z não desfaz — e nada na tela diria quais.
//
// Duas decisões carregam o resto:
//
// 1. **Transação, não comando.** A unidade de desfazer é um grupo rotulado. Uma
//    remoção apaga uma subárvore inteira e um arraste de gizmo produz centenas
//    de eventos; os dois têm de voltar em um passo. Sem transação, apagar uma
//    pasta com dez filhos exigiria onze Ctrl+Z.
//
// 2. **Estado, não delta.** Cada comando guarda a entidade inteira antes e
//    depois. É mais memória por passo (algumas centenas de bytes) do que um
//    delta por campo, e em troca desfazer é uma atribuição que não pode
//    divergir do original — não existe "campo que o delta esqueceu".
//
// Sem Vulkan, sem Android, sem I/O: testável integralmente no host.
#pragma once

#include "core/base.h"
#include "editor/editor_document.h"

#include <string_view>
#include <vector>
#include <functional>

namespace ae::editor {

enum class EditorCommandKind : u8 { ApplyValues, Create, Destroy, Reparent, Views };

// Token de fusão. Comandos consecutivos sobre a mesma entidade com o mesmo
// token não nulo viram um só: é o que faz um arraste de gizmo inteiro ocupar um
// passo de undo em vez de um por frame. Zero desativa a fusão.
using EditorMergeToken = u32;
inline constexpr EditorMergeToken kNoMerge = 0;

struct EditorCommand final {
  EditorCommandKind kind = EditorCommandKind::ApplyValues;
  EditorEntityId id = kInvalidEntity;
  EditorEntity before{};
  EditorEntity after{};
  EditorEntityId beforeParent = kInvalidEntity;
  EditorEntityId afterParent = kInvalidEntity;
  u32 beforeIndex = 0;
  u32 afterIndex = 0;
  EditorMergeToken mergeToken = kNoMerge;
  // Estado de CENA, não de objeto (Views): salvar, renomear e excluir um
  // enquadramento é edição autoral e tem de caber no mesmo Desfazer do resto.
  runtime::SceneViews beforeViews{}, afterViews{};
};

class EditorHistory final {
public:
  // Teto de passos guardados. O mais antigo cai quando estoura: um editor móvel
  // não pode trocar memória por um histórico infinito que ninguém percorre.
  static constexpr u32 kMaximumTransactions = 256;
  static constexpr u32 kLabelCapacity = 48;

  // Abre uma transação. Chamadas aninhadas são recusadas — aninhamento exigiria
  // decidir o que "desfazer" significa no meio de um grupo, e nenhuma resposta
  // é boa. As operações abaixo abrem e fecham uma transação própria quando
  // nenhuma está aberta, de modo que o caso simples não precisa de cerimônia.
  bool begin(std::string_view label);
  void end();
  // Roll back an open gesture without consuming undo or discarding redo.
  bool cancel(EditorDocument &document);
  bool isOpen() const noexcept { return open_; }
  // Standalone resource transaction, already committed. Replay must be atomic:
  // false leaves the history cursor in place. Never mixes with scene commands.
  bool recordResource(std::string_view label,std::function<bool(bool)> replay);

  EditorEntityId createEntity(EditorDocument &document, EditorEntityId parent,
                              EditorEntityKind kind, std::string_view name);
  bool destroyEntity(EditorDocument &document, EditorEntityId id);
  EditorEntityId duplicateEntity(EditorDocument &document, EditorEntityId id);
  bool reparentKeepingWorld(EditorDocument &document, EditorEntityId id, EditorEntityId parent);
  bool applyValues(EditorDocument &document, EditorEntityId id, const EditorEntity &values,
                   EditorMergeToken mergeToken = kNoMerge);
  bool setTransform(EditorDocument &document, EditorEntityId id, const EditorTransform &transform,
                    EditorMergeToken mergeToken = kNoMerge);
  bool reparent(EditorDocument &document, EditorEntityId id, EditorEntityId newParent,
                u32 childIndex);
  // Vistas salvas da cena, com o passo de Desfazer do resto da edição.
  bool setViews(EditorDocument &document, const runtime::SceneViews &views);

  // Desfazer/refazer fecham qualquer transação aberta antes de agir: um undo no
  // meio de um arraste desfaria metade dele e deixaria a outra metade viva.
  bool undo(EditorDocument &document);
  bool redo(EditorDocument &document);

  bool canUndo() const noexcept { return !undoStack_.empty(); }
  bool canRedo() const noexcept { return !redoStack_.empty(); }
  // Rótulo do próximo passo, para o menu e para o aviso na tela. Vazio quando
  // não há passo.
  std::string_view undoLabel() const noexcept;
  std::string_view redoLabel() const noexcept;
  u32 undoDepth() const noexcept { return static_cast<u32>(undoStack_.size()); }
  u32 redoDepth() const noexcept { return static_cast<u32>(redoStack_.size()); }

  void clear() noexcept;

private:
  struct Transaction final {
    char label[kLabelCapacity]{};
    std::vector<EditorCommand> commands;
    std::function<bool(bool)> resourceReplay;
  };

  bool record(const EditorCommand &command);
  bool applyForward(EditorDocument &document, const EditorCommand &command) const;
  bool applyBackward(EditorDocument &document, const EditorCommand &command) const;
  // Fecha a transação aberta e a empilha, descartando-a se ficou vazia: um
  // grupo sem comandos viraria um Ctrl+Z que não faz nada visível.
  void commitOpenTransaction();

  std::vector<Transaction> undoStack_;
  std::vector<Transaction> redoStack_;
  Transaction pending_{};
  bool open_ = false;
  // Verdadeiro enquanto um undo/redo está aplicando comandos, para que os
  // helpers de mutação não registrem o que eles próprios estão desfazendo.
  bool replaying_ = false;
};

} // namespace ae::editor
