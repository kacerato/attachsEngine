#include "editor/editor_component_references.h"
#include "scene/script_behavior.h"
#include "scene/import_link.h"
#include "editor/editor_history.h"
#include "editor/editor_map_scene.h"

#include <algorithm>

namespace ae::editor {
namespace {

void assignLabel(char (&destination)[EditorHistory::kLabelCapacity], std::string_view label) {
  const usize copied = std::min<usize>(label.size(), EditorHistory::kLabelCapacity - 1);
  for (usize index = 0; index < copied; ++index) destination[index] = label[index];
  for (usize index = copied; index < EditorHistory::kLabelCapacity; ++index)
    destination[index] = '\0';
}

// Um comando de valores é fundível com o anterior quando é a continuação do
// mesmo gesto sobre o mesmo objeto. O `before` do primeiro é preservado: é ele
// que representa o estado anterior ao gesto inteiro.
bool canMerge(const EditorCommand &previous, const EditorCommand &next) noexcept {
  return next.mergeToken != kNoMerge && previous.mergeToken == next.mergeToken &&
         previous.kind == EditorCommandKind::ApplyValues &&
         next.kind == EditorCommandKind::ApplyValues && previous.id == next.id;
}

} // namespace

bool EditorHistory::begin(std::string_view label) {
  if (open_) return false;
  pending_ = Transaction{};
  assignLabel(pending_.label, label);
  open_ = true;
  return true;
}

void EditorHistory::end() {
  if (!open_) return;
  commitOpenTransaction();
}
bool EditorHistory::recordResource(std::string_view label,std::function<bool(bool)> replay) {
  if(open_||replaying_||!replay) return false;
  Transaction transaction;assignLabel(transaction.label,label);transaction.resourceReplay=std::move(replay);
  undoStack_.push_back(std::move(transaction));redoStack_.clear();
  if(undoStack_.size()>kMaximumTransactions) undoStack_.erase(undoStack_.begin());
  return true;
}

bool EditorHistory::cancel(EditorDocument &document) {
  if(!open_) return false;
  auto candidate=document;
  for(usize i=pending_.commands.size();i>0;--i)
    if(!applyBackward(candidate,pending_.commands[i-1])) return false;
  document=std::move(candidate);
  pending_=Transaction{};
  open_=false;
  return true;
}

void EditorHistory::commitOpenTransaction() {
  open_ = false;
  if (pending_.commands.empty()) {
    pending_ = Transaction{};
    return;
  }
  undoStack_.push_back(std::move(pending_));
  pending_ = Transaction{};
  // Qualquer edição nova invalida o futuro que havia sido desfeito: refazer
  // sobre um documento diferente aplicaria comandos a objetos que já não são
  // os mesmos.
  redoStack_.clear();
  if (undoStack_.size() > kMaximumTransactions)
    undoStack_.erase(undoStack_.begin(), undoStack_.begin() + 1);
}

bool EditorHistory::record(const EditorCommand &command) {
  if (replaying_) return true;
  const bool implicit = !open_;
  if (implicit) {
    // O caso simples — um interruptor do Inspector — não precisa de cerimônia:
    // ele vira uma transação de um comando só.
    pending_ = Transaction{};
    assignLabel(pending_.label, "Edit");
    open_ = true;
  }
  if (!pending_.commands.empty() && canMerge(pending_.commands.back(), command)) {
    pending_.commands.back().after = command.after;
  } else {
    pending_.commands.push_back(command);
  }
  if (implicit) commitOpenTransaction();
  return true;
}

EditorEntityId EditorHistory::createEntity(EditorDocument &document, EditorEntityId parent,
                                           EditorEntityKind kind, std::string_view name) {
  const EditorEntityId id = document.createEntity(parent, kind, name);
  if (id == kInvalidEntity) return kInvalidEntity;
  const EditorEntity *created = document.find(id);
  if (created == nullptr) return kInvalidEntity;
  u32 childIndex = 0;
  document.childIndexOf(id, childIndex);
  EditorCommand command{};
  command.kind = EditorCommandKind::Create;
  command.id = id;
  command.after = *created;
  command.afterParent = created->parent;
  command.afterIndex = childIndex;
  record(command);
  return id;
}

EditorEntityId EditorHistory::duplicateEntity(EditorDocument &document, EditorEntityId id) {
  const auto *source=document.find(id);
  if(!source || id==document.root() || open_) return kInvalidEntity;
  std::vector<EditorEntityId> ids;document.collectSubtree(id,ids);
  if(document.entityCount()+ids.size()>EditorDocument::kMaximumEntities) return kInvalidEntity;
  // Snapshot before creating: document growth invalidates pointers into its records.
  std::vector<EditorEntity> values;
  for(auto member:ids) values.push_back(*document.find(member));
  std::vector<EditorEntityId> created;created.reserve(ids.size());
  begin("Duplicate");
  for(usize i=0;i<values.size();++i) {
    auto value=values[i];auto parent=value.parent;
    if(i>0) for(usize j=0;j<i;++j) if(ids[j]==parent) {parent=created[j];break;}
    const auto copy=createEntity(document,parent,value.kind,value.name);
    if(!copy) {end();undo(document);return kInvalidEntity;}
    created.push_back(copy);
  }
  // Resolve all clone identities before remapping references, including forward
  // references to siblings. External object references intentionally survive.
  for(usize i=0;i<values.size();++i) {
    auto value=values[i];
    for(usize c=0;c<value.components.size();++c) if(const auto *script=scene::scriptBehavior(value.components.at(c))) {
      auto replacement=*script;
      for(auto &p:replacement.properties) {
        u64 target=0;if(!scene::scriptPropertyObject(p,target)) continue;
        for(usize j=0;j<ids.size();++j) if(target==ids[j]) {scene::retargetScriptPropertyObject(p,created[j]);break;}
      }
      if(!value.components.replaceInstance(script->instanceId(),replacement)) {end();undo(document);return kInvalidEntity;}
    }
    for(usize c=0;c<value.components.size();++c) {
      const auto *component=value.components.at(c);if(component->type().references.empty()) continue;
      auto replacement=component->clone();
      for(const auto &property:replacement->type().references) {
        const auto target=property.read(*replacement);
        for(usize j=0;j<ids.size();++j) if(target==ids[j]) {property.write(*replacement,created[j]);break;}
      }
      if(!value.components.replaceInstance(component->instanceId(),*replacement)) {end();undo(document);return kInvalidEntity;}
    }
    // Vínculo com a fonte importada (M08.2). Duplicar a instância inteira cria
    // OUTRA instância: mesma fonte, identidade de instância nova. Duplicar uma
    // parte solta cria um objeto independente — duas peças dizendo ser o mesmo
    // nó da mesma instância travariam a reconciliação.
    if(const auto *link=scene::importLink(value.components)) {
      const auto *rootLink=scene::importLink(values.front().components);
      if(rootLink && rootLink->root && rootLink->instance==link->instance && rootLink->source==link->source) {
        auto copy=*link;
        // Semente estável DENTRO desta duplicação (a revisão do documento muda a
        // cada filho aplicado); ids de entidade nunca são reciclados.
        copy.instance=resources::assetGuidFromSeed("copia:"+link->instance.text()+":"+std::to_string(ids.front())+":"+
                                                   std::to_string(created.front()));
        copy.orphan=link->orphan;
        if(!value.components.replace(copy)) {end();undo(document);return kInvalidEntity;}
      } else if(!link->unlinked) {
        auto copy=*link;copy.unlinked=true;
        if(!value.components.replace(copy)) {end();undo(document);return kInvalidEntity;}
      }
    }
    if(!applyValues(document,created[i],value)) {end();undo(document);return kInvalidEntity;}
  }
  end();return created.front();
}

bool EditorHistory::destroyEntity(EditorDocument &document, EditorEntityId id) {
  if (!document.exists(id) || id == document.root()) return false;
  std::vector<EditorEntityId> subtree;
  document.collectSubtree(id, subtree);

  // Registrados em pré-ordem invertida. Assim, desfazer (que percorre a
  // transação ao contrário) restaura os pais antes dos filhos, e refazer
  // remove as folhas antes dos pais. Nenhuma das duas direções precisa de um
  // caso especial para a raiz da subárvore.
  std::vector<EditorCommand> commands;
  commands.reserve(subtree.size());
  for (usize index = subtree.size(); index > 0; --index) {
    const EditorEntityId member = subtree[index - 1];
    const EditorEntity *entity = document.find(member);
    if (entity == nullptr) return false;
    u32 childIndex = 0;
    document.childIndexOf(member, childIndex);
    EditorCommand command{};
    command.kind = EditorCommandKind::Destroy;
    command.id = member;
    command.before = *entity;
    command.beforeParent = entity->parent;
    command.beforeIndex = childIndex;
    commands.push_back(command);
  }

  if (!document.destroyEntity(id)) return false;
  const bool implicit = !open_;
  if (implicit) begin("Delete");
  for (const EditorCommand &command : commands) record(command);
  if (implicit) end();
  return true;
}

bool EditorHistory::applyValues(EditorDocument &document, EditorEntityId id,
                                const EditorEntity &values, EditorMergeToken mergeToken) {
  const EditorEntity *current = document.find(id);
  if (current == nullptr) return false;
  const EditorEntity before = *current;
  if (!document.applyEntityValues(id, values)) return false;
  const EditorEntity *updated = document.find(id);
  if (updated == nullptr) return false;
  EditorCommand command{};
  command.kind = EditorCommandKind::ApplyValues;
  command.id = id;
  command.before = before;
  command.after = *updated;
  command.mergeToken = mergeToken;
  record(command);
  return true;
}

bool EditorHistory::setTransform(EditorDocument &document, EditorEntityId id,
                                 const EditorTransform &transform,
                                 EditorMergeToken mergeToken) {
  const EditorEntity *current = document.find(id);
  if (current == nullptr) return false;
  EditorEntity values = *current;
  values.transform = transform;
  return applyValues(document, id, values, mergeToken);
}

bool EditorHistory::reparentKeepingWorld(EditorDocument &document,EditorEntityId id,EditorEntityId parent) {
  if(open_ || id==document.root() || document.isDescendantOf(parent,id)) return false;
  float world[16],parentWorld[16];EditorTransform local;
  if(!editorWorldMatrix(document,id,world) || !editorWorldMatrix(document,parent,parentWorld) ||
     !editorLocalTransformForWorld(world,parentWorld,local)) return false;
  begin("Reparent");
  if(!reparent(document,id,parent,static_cast<u32>(document.childrenOf(parent).size()))) {end();return false;}
  if(!setTransform(document,id,local)) {end();undo(document);return false;}
  end();return true;
}

bool EditorHistory::reparent(EditorDocument &document, EditorEntityId id,
                             EditorEntityId newParent, u32 childIndex) {
  const EditorEntity *current = document.find(id);
  if (current == nullptr) return false;
  const EditorEntityId oldParent = current->parent;
  u32 oldIndex = 0;
  if (!document.childIndexOf(id, oldIndex)) return false;
  // Preserve valid ancestor ownership when changing hierarchy. Existing drafts
  // with unresolved links stay editable and receive diagnostics at execution.
  EditorDocument candidate=document;
  if(!candidate.reparent(id,newParent,childIndex)) return false;
  std::vector<EditorEntityId> moved;document.collectSubtree(id,moved);
  for(auto member:moved) {
    const auto &entity=*document.find(member);
    for(usize i=0;i<entity.components.size();++i) {
      const auto *value=entity.components.at(i);
      for(const auto &property:value->type().references) {
        const auto target=property.read(*value);
        if(property.scope==scene::ObjectReferenceScope::SelfOrAncestor &&
           editorReferenceAccepts(document,member,property,target,true)&&!editorReferenceAccepts(candidate,member,property,target,true)) return false;
      }
    }
  }
  if (!document.reparent(id, newParent, childIndex)) return false;
  u32 newIndex = 0;
  document.childIndexOf(id, newIndex);
  EditorCommand command{};
  command.kind = EditorCommandKind::Reparent;
  command.id = id;
  command.beforeParent = oldParent;
  command.beforeIndex = oldIndex;
  command.afterParent = newParent;
  command.afterIndex = newIndex;
  record(command);
  return true;
}

bool EditorHistory::applyForward(EditorDocument &document, const EditorCommand &command) const {
  switch (command.kind) {
    case EditorCommandKind::ApplyValues:
      return document.applyEntityValues(command.id, command.after);
    case EditorCommandKind::Create:
      return document.restoreEntity(command.after, command.afterIndex);
    case EditorCommandKind::Destroy:
      return document.destroyEntity(command.id);
    case EditorCommandKind::Reparent:
      return document.reparent(command.id, command.afterParent, command.afterIndex);
    case EditorCommandKind::Views:
      document.setViews(command.afterViews);
      return true;
    case EditorCommandKind::Layers:
      if(!command.afterLayers) return false;
      document.setLayers(*command.afterLayers);
      return true;
    case EditorCommandKind::InputActions:
      return command.afterInput && document.setInputActions(*command.afterInput);
  }
  return false;
}

bool EditorHistory::applyBackward(EditorDocument &document, const EditorCommand &command) const {
  switch (command.kind) {
    case EditorCommandKind::ApplyValues:
      return document.applyEntityValues(command.id, command.before);
    case EditorCommandKind::Create:
      return document.destroyEntity(command.id);
    case EditorCommandKind::Destroy:
      return document.restoreEntity(command.before, command.beforeIndex);
    case EditorCommandKind::Reparent:
      return document.reparent(command.id, command.beforeParent, command.beforeIndex);
    case EditorCommandKind::Views:
      document.setViews(command.beforeViews);
      return true;
    case EditorCommandKind::Layers:
      if(!command.beforeLayers) return false;
      document.setLayers(*command.beforeLayers);
      return true;
    case EditorCommandKind::InputActions:
      return command.beforeInput && document.setInputActions(*command.beforeInput);
  }
  return false;
}

bool EditorHistory::setViews(EditorDocument &document, const runtime::SceneViews &views) {
  if (document.views() == views) return true;
  EditorCommand command{};
  command.kind = EditorCommandKind::Views;
  command.beforeViews = document.views();
  command.afterViews = views;
  document.setViews(views);
  // Rótulo próprio: no menu de Desfazer, "Vistas" diz o que volta — o passo
  // genérico "Edit" faria o autor achar que perderia a edição do objeto.
  const bool standalone = !isOpen();
  if (standalone) begin("Vistas");
  const bool recorded = record(command);
  if (standalone) end();
  return recorded;
}
bool EditorHistory::setLayers(EditorDocument &document, const runtime::GameplayLayers &layers) {
  if(document.layers()==layers) return true;
  EditorCommand command{};command.kind=EditorCommandKind::Layers;
  command.beforeLayers=std::make_shared<runtime::GameplayLayers>(document.layers());
  command.afterLayers=std::make_shared<runtime::GameplayLayers>(layers);
  const bool standalone=!isOpen();
  if(standalone && !begin("Camadas físicas")) return false;
  document.setLayers(layers);
  const bool recorded=record(command);
  if(standalone) end();
  return recorded;
}
bool EditorHistory::setInputActions(EditorDocument &document,const runtime::InputActionMap &map) {
  if(document.inputActions()==map) return true;
  if(!map.valid()) return false;
  EditorCommand command{};command.kind=EditorCommandKind::InputActions;
  command.beforeInput=std::make_shared<runtime::InputActionMap>(document.inputActions());
  command.afterInput=std::make_shared<runtime::InputActionMap>(map);
  const bool standalone=!isOpen();
  if(standalone && !begin("Mapa de entrada")) return false;
  if(!document.setInputActions(map)) {if(standalone) cancel(document);return false;}
  const bool recorded=record(command);
  if(standalone) end();
  return recorded;
}

bool EditorHistory::undo(EditorDocument &document) {
  // Um undo no meio de um arraste desfaria metade do gesto e deixaria a outra
  // metade viva no documento. Fechar primeiro torna o passo inteiro.
  if (open_) commitOpenTransaction();
  if (undoStack_.empty()) return false;
  if(undoStack_.back().resourceReplay) {
    replaying_=true;const bool ok=undoStack_.back().resourceReplay(false);replaying_=false;
    if(!ok) return false;
    redoStack_.push_back(std::move(undoStack_.back()));undoStack_.pop_back();return true;
  }
  Transaction transaction = std::move(undoStack_.back());
  undoStack_.pop_back();
  replaying_ = true;
  bool ok = true;
  for (usize index = transaction.commands.size(); index > 0; --index)
    ok = applyBackward(document, transaction.commands[index - 1]) && ok;
  replaying_ = false;
  redoStack_.push_back(std::move(transaction));
  return ok;
}

bool EditorHistory::redo(EditorDocument &document) {
  if (open_) commitOpenTransaction();
  if (redoStack_.empty()) return false;
  if(redoStack_.back().resourceReplay) {
    replaying_=true;const bool ok=redoStack_.back().resourceReplay(true);replaying_=false;
    if(!ok) return false;
    undoStack_.push_back(std::move(redoStack_.back()));redoStack_.pop_back();return true;
  }
  Transaction transaction = std::move(redoStack_.back());
  redoStack_.pop_back();
  replaying_ = true;
  bool ok = true;
  for (const EditorCommand &command : transaction.commands)
    ok = applyForward(document, command) && ok;
  replaying_ = false;
  undoStack_.push_back(std::move(transaction));
  return ok;
}

std::string_view EditorHistory::undoLabel() const noexcept {
  if (undoStack_.empty()) return {};
  return undoStack_.back().label;
}

std::string_view EditorHistory::redoLabel() const noexcept {
  if (redoStack_.empty()) return {};
  return redoStack_.back().label;
}

void EditorHistory::clear() noexcept {
  undoStack_.clear();
  redoStack_.clear();
  pending_ = Transaction{};
  open_ = false;
  replaying_ = false;
}

} // namespace ae::editor
