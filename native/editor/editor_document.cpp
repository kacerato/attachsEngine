#include "editor/editor_document.h"
#include "editor/editor_properties.h"

#include <algorithm>
#include <cmath>

namespace ae::editor {
namespace {

bool isFiniteTriple(const float values[3]) noexcept {
  return std::isfinite(values[0]) && std::isfinite(values[1]) && std::isfinite(values[2]);
}

} // namespace

bool isTransformValid(const EditorTransform &transform) noexcept {
  if (!isFiniteTriple(transform.position) || !isFiniteTriple(transform.rotationDegrees) ||
      !isFiniteTriple(transform.scale))
    return false;
  // Escala zero é aceita — é como se achata um objeto de propósito — mas
  // negativa inverte a orientação das faces e quebraria a normal e o culling
  // sem nenhum aviso na tela. Espelhar é uma operação própria, não um sinal.
  for (u32 axis = 0; axis < 3; ++axis)
    if (transform.scale[axis] < 0.0f) return false;
  return true;
}

void assignEntityName(EditorEntity &entity, std::string_view name) noexcept {
  const usize copied = std::min<usize>(name.size(), kEditorNameCapacity - 1);
  for (usize index = 0; index < copied; ++index) entity.name[index] = name[index];
  for (usize index = copied; index < kEditorNameCapacity; ++index) entity.name[index] = '\0';
}

EditorDocument::EditorDocument() { reset(); }

void EditorDocument::reset() {
  records_.clear();
  records_.resize(1);  // posição 0 é o id inválido e nunca vive
  nextId_ = 1;
  aliveCount_ = 0;
  revision_ = 0;
  rootId_ = nextId_++;
  records_.resize(rootId_ + 1);
  Record &root = records_[rootId_];
  root.alive = true;
  root.entity = EditorEntity{};
  root.entity.id = rootId_;
  root.entity.parent = kInvalidEntity;
  root.entity.kind = EditorEntityKind::Folder;
  assignEntityName(root.entity, "Cena");
  aliveCount_ = 1;
}

EditorDocument::Record *EditorDocument::record(EditorEntityId id) noexcept {
  if (id == kInvalidEntity || id >= records_.size()) return nullptr;
  Record &candidate = records_[id];
  return candidate.alive ? &candidate : nullptr;
}

const EditorDocument::Record *EditorDocument::record(EditorEntityId id) const noexcept {
  if (id == kInvalidEntity || id >= records_.size()) return nullptr;
  const Record &candidate = records_[id];
  return candidate.alive ? &candidate : nullptr;
}

bool EditorDocument::exists(EditorEntityId id) const noexcept { return record(id) != nullptr; }

const EditorEntity *EditorDocument::find(EditorEntityId id) const noexcept {
  const Record *found = record(id);
  return found != nullptr ? &found->entity : nullptr;
}

std::span<const EditorEntityId> EditorDocument::childrenOf(EditorEntityId id) const noexcept {
  const Record *found = record(id);
  if (found == nullptr) return {};
  return {found->children.data(), found->children.size()};
}

bool EditorDocument::childIndexOf(EditorEntityId id, u32 &outIndex) const noexcept {
  const Record *found = record(id);
  if (found == nullptr) return false;
  const Record *parent = record(found->entity.parent);
  if (parent == nullptr) return false;
  for (usize index = 0; index < parent->children.size(); ++index) {
    if (parent->children[index] != id) continue;
    outIndex = static_cast<u32>(index);
    return true;
  }
  return false;
}

bool EditorDocument::isDescendantOf(EditorEntityId candidate,
                                    EditorEntityId ancestor) const noexcept {
  if (candidate == kInvalidEntity || ancestor == kInvalidEntity) return false;
  EditorEntityId walk = candidate;
  // O teto de passos é o número de entidades: uma hierarquia com ciclo (que as
  // guardas de reparent impedem) terminaria aqui em vez de travar a interface.
  for (u32 step = 0; step <= records_.size(); ++step) {
    if (walk == ancestor) return true;
    const Record *found = record(walk);
    if (found == nullptr) return false;
    walk = found->entity.parent;
    if (walk == kInvalidEntity) return false;
  }
  return false;
}

void EditorDocument::collectSubtree(EditorEntityId id, std::vector<EditorEntityId> &out) const {
  out.clear();
  if (record(id) == nullptr) return;
  out.push_back(id);
  // Pré-ordem iterativa: `out` é a própria fila. A subárvore de uma cena grande
  // pode ser funda, e uma recursão aqui rodaria na thread de UI.
  for (usize cursor = 0; cursor < out.size(); ++cursor) {
    const Record *found = record(out[cursor]);
    if (found == nullptr) continue;
    for (const EditorEntityId child : found->children) out.push_back(child);
  }
}

bool EditorDocument::attachToParent(EditorEntityId id, EditorEntityId parent, u32 childIndex) {
  Record *parentRecord = record(parent);
  if (parentRecord == nullptr) return false;
  const u32 clamped = std::min<u32>(childIndex, static_cast<u32>(parentRecord->children.size()));
  parentRecord->children.insert(parentRecord->children.begin() + clamped, id);
  records_[id].entity.parent = parent;
  return true;
}

bool EditorDocument::detachFromParent(EditorEntityId id) {
  Record *found = record(id);
  if (found == nullptr) return false;
  Record *parentRecord = record(found->entity.parent);
  if (parentRecord == nullptr) return false;
  auto &children = parentRecord->children;
  const auto position = std::find(children.begin(), children.end(), id);
  if (position == children.end()) return false;
  children.erase(position);
  return true;
}

EditorEntityId EditorDocument::createEntity(EditorEntityId parent, EditorEntityKind kind,
                                            std::string_view name) {
  if (record(parent) == nullptr) return kInvalidEntity;
  if (entityCount() >= kMaximumEntities) return kInvalidEntity;
  const EditorEntityId id = nextId_++;
  if (id >= records_.size()) records_.resize(id + 1);
  Record &created = records_[id];
  created = Record{};
  created.alive = true;
  created.entity = EditorEntity{};
  created.entity.id = id;
  created.entity.kind = kind;
  assignEntityName(created.entity, name);
  if (!attachToParent(id, parent, static_cast<u32>(records_[parent].children.size()))) {
    created.alive = false;
    return kInvalidEntity;
  }
  ++aliveCount_;
  ++revision_;
  return id;
}

bool EditorDocument::restoreEntity(const EditorEntity &entity, u32 childIndex) {
  if (entity.id == kInvalidEntity || entity.id > kMaximumEntities || entityCount() >= kMaximumEntities) return false;
  if (exists(entity.id)) return false;
  if (record(entity.parent) == nullptr) return false;
  if (!isTransformValid(entity.transform) || !validEditorAppearance(entity)) return false;
  if (entity.id >= records_.size()) records_.resize(entity.id + 1);
  Record &restored = records_[entity.id];
  restored = Record{};
  restored.alive = true;
  restored.entity = entity;
  // O terminador pode ter se perdido se a struct veio de fora; garanti-lo aqui
  // evita que uma leitura de `name` saia do buffer mais tarde.
  restored.entity.name[kEditorNameCapacity - 1] = '\0';
  if (!attachToParent(entity.id, entity.parent, childIndex)) {
    restored.alive = false;
    return false;
  }
  ++aliveCount_;
  // Um id restaurado nunca pode ser reemitido: um `redo` que criasse outra
  // entidade com o mesmo id apagaria esta.
  if (entity.id >= nextId_) nextId_ = entity.id + 1;
  ++revision_;
  return true;
}

bool EditorDocument::destroyEntity(EditorEntityId id) {
  if (id == rootId_ || record(id) == nullptr) return false;
  std::vector<EditorEntityId> subtree;
  collectSubtree(id, subtree);
  if (!detachFromParent(id)) return false;
  for (const EditorEntityId member : subtree) {
    Record &found = records_[member];
    found.alive = false;
    found.children.clear();
    if (aliveCount_ > 0) --aliveCount_;
  }
  ++revision_;
  return true;
}

bool EditorDocument::setName(EditorEntityId id, std::string_view name) {
  Record *found = record(id);
  if (found == nullptr) return false;
  // Nome vazio deixaria uma linha em branco na hierarquia, indistinguível de um
  // erro de desenho. Renomear para nada é recusado; apagar é destruir.
  if (name.empty()) return false;
  assignEntityName(found->entity, name);
  ++revision_;
  return true;
}

bool EditorDocument::setTransform(EditorEntityId id, const EditorTransform &transform) {
  Record *found = record(id);
  if (found == nullptr) return false;
  if (!isTransformValid(transform)) return false;
  found->entity.transform = transform;
  ++revision_;
  return true;
}

bool EditorDocument::applyEntityValues(EditorEntityId id, const EditorEntity &values) {
  Record *found = record(id);
  if (found == nullptr) return false;
  if (!isTransformValid(values.transform) || !validEditorAppearance(values)) return false;
  EditorEntity &target = found->entity;
  // Identidade e posição na árvore não vêm daqui: trocá-las exigiria mexer nas
  // listas de filhos, e um "aplicar valores" que reparenta em silêncio é uma
  // armadilha para quem escrever o próximo comando.
  target.transform = values.transform;
  target.active = values.active;
  target.visible = values.visible;
  target.castShadow = values.castShadow;
  target.receiveShadow = values.receiveShadow;
  target.isStatic = values.isStatic;
  target.layer = values.layer;
  target.components=values.components;
  target.rigidBodyEnabled=values.rigidBodyEnabled;
  std::copy(std::begin(values.rigidBody),std::end(values.rigidBody),target.rigidBody);
  std::copy(values.environment,values.environment+4,target.environment);
  for (u32 index = 0; index < kEditorNameCapacity; ++index) target.name[index] = values.name[index];
  target.name[kEditorNameCapacity - 1] = '\0';
  ++revision_;
  return true;
}

bool EditorDocument::reparent(EditorEntityId id, EditorEntityId newParent, u32 childIndex) {
  if (id == rootId_ || record(id) == nullptr || record(newParent) == nullptr) return false;
  // Reparentar para dentro da própria subárvore criaria um ciclo: a entidade
  // sumiria da hierarquia e toda travessia entraria em laço.
  if (isDescendantOf(newParent, id)) return false;
  if (!detachFromParent(id)) return false;
  if (!attachToParent(id, newParent, childIndex)) return false;
  ++revision_;
  return true;
}

} // namespace ae::editor
