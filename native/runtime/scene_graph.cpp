#include "runtime/scene_graph.h"

#include <algorithm>
#include <cmath>

namespace ae::runtime {
namespace {

bool isFiniteTriple(const float values[3]) noexcept {
  return std::isfinite(values[0]) && std::isfinite(values[1]) && std::isfinite(values[2]);
}

} // namespace

bool isTransformValid(const Transform &transform) noexcept {
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

void assignObjectName(SceneObject &object, std::string_view name) noexcept {
  const usize copied = std::min<usize>(name.size(), kNameCapacity - 1);
  for (usize index = 0; index < copied; ++index) object.name[index] = name[index];
  for (usize index = copied; index < kNameCapacity; ++index) object.name[index] = '\0';
}

bool SceneGraph::acceptObject(const SceneObject &object) const { return object.components.valid(); }

SceneGraph::SceneGraph() { reset(); }

void SceneGraph::reset() {
  layers_.reset();
  input_ = InputActionMap{};
  records_.clear();
  records_.resize(1);  // posição 0 é o id inválido e nunca vive
  nextId_ = 1;
  aliveCount_ = 0;
  revision_ = 0;
  rootId_ = nextId_++;
  records_.resize(rootId_ + 1);
  Record &root = records_[rootId_];
  root.alive = true;
  root.object = SceneObject{};
  root.object.id = rootId_;
  root.object.parent = kInvalidObject;
  root.object.kind = ObjectKind::Folder;
  assignObjectName(root.object, "Cena");
  aliveCount_ = 1;
}

SceneGraph::Record *SceneGraph::record(ObjectId id) noexcept {
  if (id == kInvalidObject || id >= records_.size()) return nullptr;
  Record &candidate = records_[id];
  return candidate.alive ? &candidate : nullptr;
}

const SceneGraph::Record *SceneGraph::record(ObjectId id) const noexcept {
  if (id == kInvalidObject || id >= records_.size()) return nullptr;
  const Record &candidate = records_[id];
  return candidate.alive ? &candidate : nullptr;
}

bool SceneGraph::exists(ObjectId id) const noexcept { return record(id) != nullptr; }

const SceneObject *SceneGraph::find(ObjectId id) const noexcept {
  const Record *found = record(id);
  return found != nullptr ? &found->object : nullptr;
}

std::span<const ObjectId> SceneGraph::childrenOf(ObjectId id) const noexcept {
  const Record *found = record(id);
  if (found == nullptr) return {};
  return {found->children.data(), found->children.size()};
}

bool SceneGraph::childIndexOf(ObjectId id, u32 &outIndex) const noexcept {
  const Record *found = record(id);
  if (found == nullptr) return false;
  const Record *parent = record(found->object.parent);
  if (parent == nullptr) return false;
  for (usize index = 0; index < parent->children.size(); ++index) {
    if (parent->children[index] != id) continue;
    outIndex = static_cast<u32>(index);
    return true;
  }
  return false;
}

bool SceneGraph::isDescendantOf(ObjectId candidate, ObjectId ancestor) const noexcept {
  if (candidate == kInvalidObject || ancestor == kInvalidObject) return false;
  ObjectId walk = candidate;
  // O teto de passos é o número de objetos: uma hierarquia com ciclo (que as
  // guardas de reparent impedem) terminaria aqui em vez de travar a interface.
  for (u32 step = 0; step <= records_.size(); ++step) {
    if (walk == ancestor) return true;
    const Record *found = record(walk);
    if (found == nullptr) return false;
    walk = found->object.parent;
    if (walk == kInvalidObject) return false;
  }
  return false;
}

bool SceneGraph::activeInHierarchy(ObjectId id) const noexcept {
  if (record(id) == nullptr) return false;
  for (ObjectId walk = id; walk != kInvalidObject;) {
    const Record *found = record(walk);
    if (found == nullptr || !found->object.active) return false;
    walk = found->object.parent;
  }
  return true;
}

void SceneGraph::collectSubtree(ObjectId id, std::vector<ObjectId> &out) const {
  out.clear();
  if (record(id) == nullptr) return;
  out.push_back(id);
  // Pré-ordem iterativa: `out` é a própria fila. A subárvore de uma cena grande
  // pode ser funda, e uma recursão aqui rodaria na thread de UI.
  for (usize cursor = 0; cursor < out.size(); ++cursor) {
    const Record *found = record(out[cursor]);
    if (found == nullptr) continue;
    for (const ObjectId child : found->children) out.push_back(child);
  }
}

bool SceneGraph::attachToParent(ObjectId id, ObjectId parent, u32 childIndex) {
  Record *parentRecord = record(parent);
  if (parentRecord == nullptr) return false;
  const u32 clamped = std::min<u32>(childIndex, static_cast<u32>(parentRecord->children.size()));
  parentRecord->children.insert(parentRecord->children.begin() + clamped, id);
  records_[id].object.parent = parent;
  return true;
}

bool SceneGraph::detachFromParent(ObjectId id) {
  Record *found = record(id);
  if (found == nullptr) return false;
  Record *parentRecord = record(found->object.parent);
  if (parentRecord == nullptr) return false;
  auto &children = parentRecord->children;
  const auto position = std::find(children.begin(), children.end(), id);
  if (position == children.end()) return false;
  children.erase(position);
  return true;
}

ObjectId SceneGraph::createEntity(ObjectId parent, ObjectKind kind, std::string_view name) {
  if (record(parent) == nullptr) return kInvalidObject;
  if (entityCount() >= kMaximumObjects) return kInvalidObject;
  const ObjectId id = nextId_++;
  if (id >= records_.size()) records_.resize(id + 1);
  Record &created = records_[id];
  created = Record{};
  created.alive = true;
  created.object = SceneObject{};
  created.object.id = id;
  created.object.kind = kind;
  assignObjectName(created.object, name);
  if (!attachToParent(id, parent, static_cast<u32>(records_[parent].children.size()))) {
    created.alive = false;
    return kInvalidObject;
  }
  ++aliveCount_;
  ++revision_;
  return id;
}

bool SceneGraph::restoreEntity(const SceneObject &object, u32 childIndex) {
  if (object.id == kInvalidObject || object.id > kMaximumObjects || entityCount() >= kMaximumObjects) return false;
  if (exists(object.id)) return false;
  if (record(object.parent) == nullptr) return false;
  if (!isTransformValid(object.transform) || !acceptObject(object)) return false;
  if (object.id >= records_.size()) records_.resize(object.id + 1);
  Record &restored = records_[object.id];
  restored = Record{};
  restored.alive = true;
  restored.object = object;
  // O terminador pode ter se perdido se a struct veio de fora; garanti-lo aqui
  // evita que uma leitura de `name` saia do buffer mais tarde.
  restored.object.name[kNameCapacity - 1] = '\0';
  if (!attachToParent(object.id, object.parent, childIndex)) {
    restored.alive = false;
    return false;
  }
  ++aliveCount_;
  // Um id restaurado nunca pode ser reemitido: um `redo` que criasse outro
  // objeto com o mesmo id apagaria este.
  if (object.id >= nextId_) nextId_ = object.id + 1;
  ++revision_;
  return true;
}

bool SceneGraph::destroyEntity(ObjectId id) {
  if (id == rootId_ || record(id) == nullptr) return false;
  std::vector<ObjectId> subtree;
  collectSubtree(id, subtree);
  if (!detachFromParent(id)) return false;
  for (const ObjectId member : subtree) {
    Record &found = records_[member];
    found.alive = false;
    found.children.clear();
    if (aliveCount_ > 0) --aliveCount_;
  }
  ++revision_;
  return true;
}

bool SceneGraph::setName(ObjectId id, std::string_view name) {
  Record *found = record(id);
  if (found == nullptr) return false;
  // Nome vazio deixaria uma linha em branco na hierarquia, indistinguível de um
  // erro de desenho. Renomear para nada é recusado; apagar é destruir.
  if (name.empty()) return false;
  assignObjectName(found->object, name);
  ++revision_;
  return true;
}

bool SceneGraph::setTransform(ObjectId id, const Transform &transform) {
  Record *found = record(id);
  if (found == nullptr) return false;
  if (!isTransformValid(transform)) return false;
  found->object.transform = transform;
  ++revision_;
  return true;
}

bool SceneGraph::applyEntityValues(ObjectId id, const SceneObject &values) {
  Record *found = record(id);
  if (found == nullptr) return false;
  if (!isTransformValid(values.transform) || !acceptObject(values)) return false;
  SceneObject &target = found->object;
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
  target.components = values.components;
  target.rigidBodyEnabled = values.rigidBodyEnabled;
  std::copy(std::begin(values.rigidBody), std::end(values.rigidBody), target.rigidBody);
  std::copy(values.environment, values.environment + 4, target.environment);
  for (u32 index = 0; index < kNameCapacity; ++index) target.name[index] = values.name[index];
  target.name[kNameCapacity - 1] = '\0';
  ++revision_;
  return true;
}

bool SceneGraph::setActive(ObjectId id, bool active) {
  Record *found = record(id);
  if (found == nullptr) return false;
  if (found->object.active == active) return true;
  found->object.active = active;
  ++revision_;
  return true;
}

scene::Components *SceneGraph::editComponents(ObjectId id) {
  Record *found = record(id);
  if (found == nullptr) return nullptr;
  ++revision_;
  return &found->object.components;
}

bool SceneGraph::reparent(ObjectId id, ObjectId newParent, u32 childIndex) {
  if (id == rootId_ || record(id) == nullptr || record(newParent) == nullptr) return false;
  // Reparentar para dentro da própria subárvore criaria um ciclo: o objeto
  // sumiria da hierarquia e toda travessia entraria em laço.
  if (isDescendantOf(newParent, id)) return false;
  if (!detachFromParent(id)) return false;
  if (!attachToParent(id, newParent, childIndex)) return false;
  ++revision_;
  return true;
}

} // namespace ae::runtime
