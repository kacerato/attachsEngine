#include "runtime/game_world.h"

#include <algorithm>
#include <atomic>
#include <cstring>

namespace ae::runtime {
namespace {
void identityMatrix(float out[16]) {
  for (u32 i = 0; i < 16; ++i) out[i] = 0;
  out[0] = out[5] = out[10] = out[15] = 1;
}
} // namespace

const char *worldStatusMessage(WorldStatus status) noexcept {
  switch (status) {
    case WorldStatus::Ok: return "";
    case WorldStatus::NotRunning: return "Não há mundo de execução ativo";
    case WorldStatus::ForeignWorld: return "A referência pertence a outra execução";
    case WorldStatus::StaleHandle: return "A referência aponta para um objeto já removido";
    case WorldStatus::UnknownObject: return "Objeto inexistente";
    case WorldStatus::UnknownComponent: return "Tipo de componente desconhecido";
    case WorldStatus::ComponentMissing: return "O objeto não possui esse componente";
    case WorldStatus::ComponentUnavailable: return "O componente não pode ser usado neste objeto";
    case WorldStatus::ComponentInUse: return "Outro componente ainda depende deste";
    case WorldStatus::NotMutableInPlay: return "Esta alteração não é permitida durante a execução";
    case WorldStatus::TransformOwnedByPhysics: return "A pose deste objeto é publicada pela física";
    case WorldStatus::InvalidArgument: return "Argumento inválido";
    case WorldStatus::LimitReached: return "Limite de capacidade atingido";
    case WorldStatus::Rejected: return "Operação recusada";
  }
  return "Operação recusada";
}

u32 GameWorld::nextWorldId() noexcept {
  // Contador do processo: um handle guardado por um script de uma sessão
  // anterior nunca é reinterpretado dentro da sessão nova.
  static std::atomic<u32> counter{0};
  u32 value = counter.fetch_add(1, std::memory_order_relaxed) + 1;
  if (value == 0) value = counter.fetch_add(1, std::memory_order_relaxed) + 1;
  return value;
}

void GameWorld::clear() {
  graph_.reset();
  slots_.clear();
  authorities_.clear();
  commands_.clear();
  scratch_.clear();
  worldId_ = 0;
  structuralRevision_ = 0;
  elapsed_ = 0;
}

bool GameWorld::load(const SceneGraph &source) {
  clear();
  std::vector<ObjectId> ids;
  source.collectSubtree(source.root(), ids);
  for (const ObjectId id : ids) {
    const auto *object = source.find(id);
    if (!object || object->components.hasUnresolved()) return false;
  }
  graph_ = source;
  ObjectId highest = graph_.root();
  for (const ObjectId id : ids) highest = std::max(highest, id);
  slots_.assign(static_cast<usize>(highest) + 1, Slot{});
  authorities_.assign(static_cast<usize>(highest) + 1, TransformAuthority::Free);
  for (const ObjectId id : ids) slots_[id].generation = 1;
  worldId_ = nextWorldId();
  structuralRevision_ = 1;
  elapsed_ = 0;
  return true;
}

ObjectHandle GameWorld::handle(ObjectId id) const noexcept {
  if (!running() || id >= slots_.size() || slots_[id].generation == 0 || !graph_.exists(id)) return {};
  return {worldId_, id, slots_[id].generation};
}

WorldStatus GameWorld::validate(const ObjectHandle &handle) const noexcept {
  if (!running()) return WorldStatus::NotRunning;
  if (!handle.valid()) return WorldStatus::InvalidArgument;
  if (handle.world != worldId_) return WorldStatus::ForeignWorld;
  if (handle.id >= slots_.size()) return WorldStatus::UnknownObject;
  if (slots_[handle.id].generation == 0 || slots_[handle.id].generation != handle.generation)
    return WorldStatus::StaleHandle;
  return graph_.exists(handle.id) ? WorldStatus::Ok : WorldStatus::StaleHandle;
}

WorldStatus GameWorld::resolve(const ObjectHandle &handle, const SceneObject *&out) const noexcept {
  const auto status = validate(handle);
  if (status != WorldStatus::Ok) return status;
  out = graph_.find(handle.id);
  return out ? WorldStatus::Ok : WorldStatus::StaleHandle;
}

const SceneObject *GameWorld::find(const ObjectHandle &handle) const noexcept {
  const SceneObject *object = nullptr;
  return resolve(handle, object) == WorldStatus::Ok ? object : nullptr;
}

scene::Components *GameWorld::editComponents(ObjectId id) { return graph_.editComponents(id); }

// --- hierarquia -----------------------------------------------------------

ObjectHandle GameWorld::parentOf(const ObjectHandle &h) const noexcept {
  const auto *object = find(h);
  return object ? handle(object->parent) : ObjectHandle{};
}

u32 GameWorld::childCount(const ObjectHandle &h) const noexcept {
  if (validate(h) != WorldStatus::Ok) return 0;
  return static_cast<u32>(graph_.childrenOf(h.id).size());
}

ObjectHandle GameWorld::childAt(const ObjectHandle &h, u32 index) const noexcept {
  if (validate(h) != WorldStatus::Ok) return {};
  const auto children = graph_.childrenOf(h.id);
  return index < children.size() ? handle(children[index]) : ObjectHandle{};
}

ObjectHandle GameWorld::findChildByName(const ObjectHandle &parent, std::string_view name, bool recursive) const noexcept {
  if (validate(parent) != WorldStatus::Ok || name.empty() || name.size() >= kNameCapacity) return {};
  if (!recursive) {
    for (const ObjectId child : graph_.childrenOf(parent.id)) {
      const auto *object = graph_.find(child);
      if (object && name == object->name) return handle(child);
    }
    return {};
  }
  std::vector<ObjectId> subtree;
  graph_.collectSubtree(parent.id, subtree);
  for (usize i = 1; i < subtree.size(); ++i) {
    const auto *object = graph_.find(subtree[i]);
    if (object && name == object->name) return handle(subtree[i]);
  }
  return {};
}

std::string_view GameWorld::nameOf(const ObjectHandle &h) const noexcept {
  const auto *object = find(h);
  return object ? std::string_view(object->name) : std::string_view{};
}

WorldStatus GameWorld::setName(const ObjectHandle &h, std::string_view name) {
  const auto status = validate(h);
  if (status != WorldStatus::Ok) return status;
  if (name.empty() || name.size() >= kNameCapacity) return WorldStatus::InvalidArgument;
  return graph_.setName(h.id, name) ? WorldStatus::Ok : WorldStatus::Rejected;
}

bool GameWorld::activeInHierarchy(const ObjectHandle &h) const noexcept {
  return validate(h) == WorldStatus::Ok && graph_.activeInHierarchy(h.id);
}

WorldStatus GameWorld::setActive(const ObjectHandle &h, bool active) {
  const auto status = validate(h);
  if (status != WorldStatus::Ok) return status;
  if (!graph_.setActive(h.id, active)) return WorldStatus::Rejected;
  ++structuralRevision_;
  return WorldStatus::Ok;
}

// --- ciclo de vida --------------------------------------------------------

ObjectHandle GameWorld::createObject(const ObjectHandle &parent, std::string_view name, WorldStatus &status) {
  status = validate(parent);
  if (status != WorldStatus::Ok) return {};
  if (name.empty() || name.size() >= kNameCapacity) { status = WorldStatus::InvalidArgument; return {}; }
  const ObjectId id = graph_.createEntity(parent.id, ObjectKind::Folder, name);
  if (id == kInvalidObject) { status = WorldStatus::LimitReached; return {}; }
  if (id >= slots_.size()) {
    slots_.resize(static_cast<usize>(id) + 1);
    authorities_.resize(static_cast<usize>(id) + 1, TransformAuthority::Free);
  }
  // Ids nunca são reciclados dentro de um mundo, então a geração nova sempre
  // começa em 1; o campo existe para que instanciar cena reaproveitando ids no
  // futuro não passe a acertar handles antigos em silêncio.
  slots_[id].generation = 1;
  authorities_[id] = TransformAuthority::Free;
  ++structuralRevision_;
  status = WorldStatus::Ok;
  return {worldId_, id, slots_[id].generation};
}

void GameWorld::markSubtreeStale(ObjectId id) {
  graph_.collectSubtree(id, scratch_);
  for (const ObjectId member : scratch_)
    if (member < slots_.size()) slots_[member].generation = 0;
}

bool GameWorld::queue(const PendingCommand &command) {
  if (commands_.size() >= kMaximumPendingCommands) return false;
  commands_.push_back(command);
  return true;
}

WorldStatus GameWorld::destroyObject(const ObjectHandle &h) {
  const auto status = validate(h);
  if (status != WorldStatus::Ok) return status;
  if (h.id == graph_.root()) return WorldStatus::Rejected;
  // A referência vence AGORA: quem guardou o handle passa a ser recusado ainda
  // dentro deste callback, mesmo que o armazenamento só saia no ponto seguro.
  markSubtreeStale(h.id);
  if (!queue({PendingCommand::Kind::Destroy, h.id, kInvalidObject, 0, 0})) return WorldStatus::LimitReached;
  ++structuralRevision_;
  return WorldStatus::Ok;
}

WorldStatus GameWorld::setParent(const ObjectHandle &h, const ObjectHandle &parent, u32 childIndex) {
  const auto status = validate(h);
  if (status != WorldStatus::Ok) return status;
  const auto parentStatus = validate(parent);
  if (parentStatus != WorldStatus::Ok) return parentStatus;
  if (h.id == graph_.root()) return WorldStatus::Rejected;
  if (graph_.isDescendantOf(parent.id, h.id)) return WorldStatus::Rejected;
  if (!queue({PendingCommand::Kind::Reparent, h.id, parent.id, childIndex, 0})) return WorldStatus::LimitReached;
  return WorldStatus::Ok;
}

u32 GameWorld::flush(std::vector<ObjectId> *destroyed) {
  u32 applied = 0;
  // Índice, não iterador: um comando aplicado pode enfileirar outro, e a fila
  // pode realocar. A ordem é a de chegada — conflitos entre dois comandos sobre
  // o mesmo objeto resolvem pelo último, e comandos para objeto já removido são
  // descartados porque o estado final pedido já vale.
  for (usize i = 0; i < commands_.size(); ++i) {
    const auto command = commands_[i];
    switch (command.kind) {
      case PendingCommand::Kind::Destroy:
        if (destroyed) {
          graph_.collectSubtree(command.object, scratch_);
          destroyed->insert(destroyed->end(), scratch_.begin(), scratch_.end());
        }
        if (graph_.destroyEntity(command.object)) ++applied;
        break;
      case PendingCommand::Kind::Reparent:
        if (graph_.exists(command.object) && graph_.exists(command.parent) &&
            graph_.reparent(command.object, command.parent, command.childIndex)) ++applied;
        break;
      case PendingCommand::Kind::RemoveComponent:
        if (auto *components = editComponents(command.object)) {
          if (components->removeInstance(command.instance)) ++applied;
        }
        break;
    }
  }
  commands_.clear();
  if (applied) ++structuralRevision_;
  return applied;
}

// --- componentes ----------------------------------------------------------

u32 GameWorld::componentCount(const ObjectHandle &h) const noexcept {
  const auto *object = find(h);
  return object ? static_cast<u32>(object->components.size()) : 0;
}

ComponentHandle GameWorld::componentAt(const ObjectHandle &h, u32 index) const noexcept {
  const auto *object = find(h);
  if (!object || index >= object->components.size()) return {};
  return {h, object->components.at(index)->instanceId()};
}

ComponentHandle GameWorld::findComponent(const ObjectHandle &h, std::string_view typeId, u32 ordinal) const noexcept {
  const auto *object = find(h);
  if (!object) return {};
  u32 seen = 0;
  for (usize i = 0; i < object->components.size(); ++i) {
    const auto *value = object->components.at(i);
    if (value->type().id != typeId) continue;
    if (seen++ != ordinal) continue;
    return {h, value->instanceId()};
  }
  return {};
}

std::string_view GameWorld::componentTypeId(const ComponentHandle &component) const noexcept {
  const auto *value = readComponent(component);
  return value ? value->type().id : std::string_view{};
}

const scene::ComponentValue *GameWorld::readComponent(const ComponentHandle &component) const noexcept {
  const auto *object = find(component.object);
  if (!object || !component.instance) return nullptr;
  return object->components.findInstance(component.instance);
}

ComponentHandle GameWorld::addComponent(const ObjectHandle &h, std::string_view typeId, WorldStatus &status) {
  const auto *object = find(h);
  status = validate(h);
  if (status != WorldStatus::Ok || !object) return {};
  const auto *schema = scene::findComponentSchema(typeId);
  if (!schema) { status = WorldStatus::UnknownComponent; return {}; }
  if (schema->structuralInPlay == scene::PlayMutability::Never) { status = WorldStatus::NotMutableInPlay; return {}; }
  if (scene::componentUnavailableReason(*schema, object->components)) {
    status = WorldStatus::ComponentUnavailable;
    return {};
  }
  auto *components = editComponents(h.id);
  if (!components) { status = WorldStatus::StaleHandle; return {}; }
  auto *created = components->add(*schema->type);
  if (!created) { status = WorldStatus::LimitReached; return {}; }
  status = WorldStatus::Ok;
  return {h, created->instanceId()};
}

WorldStatus GameWorld::removeComponent(const ComponentHandle &component) {
  const auto *object = find(component.object);
  const auto status = validate(component.object);
  if (status != WorldStatus::Ok || !object) return status;
  const auto *value = object->components.findInstance(component.instance);
  if (!value) return WorldStatus::ComponentMissing;
  const auto *schema = scene::findComponentSchema(value->type().id);
  if (!schema) return WorldStatus::UnknownComponent;
  if (schema->structuralInPlay == scene::PlayMutability::Never) return WorldStatus::NotMutableInPlay;
  // Só bloqueia quando esta é a ÚLTIMA instância do tipo: remover um dos dois
  // colisores não quebra quem exige "um colisor".
  u32 remaining = 0;
  for (usize i = 0; i < object->components.size(); ++i)
    if (object->components.at(i)->type().id == value->type().id) ++remaining;
  if (remaining <= 1 && scene::componentRemovalBlockedBy(value->type().id, object->components))
    return WorldStatus::ComponentInUse;
  if (!queue({PendingCommand::Kind::RemoveComponent, component.object.id, kInvalidObject, 0, component.instance}))
    return WorldStatus::LimitReached;
  return WorldStatus::Ok;
}

WorldStatus GameWorld::getProperty(const ComponentHandle &component, std::string_view propertyId,
                                   scene::ComponentPropertyValue &out) const {
  const auto status = validate(component.object);
  if (status != WorldStatus::Ok) return status;
  const auto *value = readComponent(component);
  if (!value) return WorldStatus::ComponentMissing;
  if (propertyId.empty()) return WorldStatus::InvalidArgument;
  u32 matches = 0;
  scene::ComponentPropertyValue found;
  for (const auto &p : value->type().numbers) if (p.id == propertyId) { found = p.read(*value); ++matches; }
  for (const auto &p : value->type().booleans) if (p.id == propertyId) { found = p.read(*value); ++matches; }
  for (const auto &p : value->type().enums) if (p.id == propertyId) { found = p.read(*value); ++matches; }
  for (const auto &p : value->type().references) if (p.id == propertyId && p.read) {
    found = scene::ObjectReference{p.read(*value)}; ++matches;
  }
  if (matches != 1) return WorldStatus::InvalidArgument;
  out = found;
  return WorldStatus::Ok;
}

WorldStatus GameWorld::setProperty(const ComponentHandle &component, std::string_view propertyId,
                                   const scene::ComponentPropertyValue &value) {
  const auto *object = find(component.object);
  const auto status = validate(component.object);
  if (status != WorldStatus::Ok || !object) return status;
  const auto *current = object->components.findInstance(component.instance);
  if (!current) return WorldStatus::ComponentMissing;
  const auto typeId = current->type().id;
  const auto *schema = scene::findComponentSchema(typeId);
  if (!schema) return WorldStatus::UnknownComponent;
  if (schema->propertiesInPlay == scene::PlayMutability::Never) return WorldStatus::NotMutableInPlay;
  // Uma referência a objeto só é aceita quando aponta para algo vivo NESTE
  // mundo. Aceitar um id solto deixaria o dado autoral apontando para o vazio.
  if (const auto *reference = std::get_if<scene::ObjectReference>(&value))
    if (reference->id && !graph_.exists(static_cast<ObjectId>(reference->id))) return WorldStatus::UnknownObject;
  auto *components = editComponents(component.object.id);
  if (!components) return WorldStatus::StaleHandle;
  switch (scene::setComponentProperty(*components, typeId, propertyId, value, component.instance)) {
    case scene::ComponentPropertyStatus::Applied: return WorldStatus::Ok;
    case scene::ComponentPropertyStatus::MissingComponent: return WorldStatus::ComponentMissing;
    case scene::ComponentPropertyStatus::UnknownProperty:
    case scene::ComponentPropertyStatus::AmbiguousProperty:
    case scene::ComponentPropertyStatus::TypeMismatch: return WorldStatus::InvalidArgument;
    case scene::ComponentPropertyStatus::InvalidValue: return WorldStatus::Rejected;
  }
  return WorldStatus::Rejected;
}

// --- transform ------------------------------------------------------------

TransformAuthority GameWorld::authorityOf(const ObjectHandle &h) const noexcept {
  if (validate(h) != WorldStatus::Ok || h.id >= authorities_.size()) return TransformAuthority::Free;
  return authorities_[h.id];
}

void GameWorld::setAuthority(ObjectId id, TransformAuthority authority) {
  if (id < authorities_.size()) authorities_[id] = authority;
}

void GameWorld::clearAuthorities() {
  std::fill(authorities_.begin(), authorities_.end(), TransformAuthority::Free);
}

WorldStatus GameWorld::localTransform(const ObjectHandle &h, Transform &out) const {
  const SceneObject *object = nullptr;
  const auto status = resolve(h, object);
  if (status != WorldStatus::Ok) return status;
  out = object->transform;
  return WorldStatus::Ok;
}

WorldStatus GameWorld::setLocalTransform(const ObjectHandle &h, const Transform &value) {
  const auto status = validate(h);
  if (status != WorldStatus::Ok) return status;
  if (!isTransformValid(value)) return WorldStatus::InvalidArgument;
  // A autoridade da pose é de quem simula. Escrever aqui sobre um corpo ou um
  // personagem faria o objeto voltar no passo seguinte, sem nenhum aviso.
  if (h.id < authorities_.size() && authorities_[h.id] != TransformAuthority::Free)
    return WorldStatus::TransformOwnedByPhysics;
  // Mover um ANCESTRAL de uma pose simulada dessincronizaria o corpo filho do
  // mesmo jeito; a recusa cobre a subárvore inteira, não só o próprio objeto.
  graph_.collectSubtree(h.id, scratch_);
  for (const ObjectId member : scratch_)
    if (member < authorities_.size() && authorities_[member] != TransformAuthority::Free)
      return WorldStatus::TransformOwnedByPhysics;
  return graph_.setTransform(h.id, value) ? WorldStatus::Ok : WorldStatus::Rejected;
}

WorldStatus GameWorld::worldTransform(const ObjectHandle &h, Transform &out) const {
  const auto status = validate(h);
  if (status != WorldStatus::Ok) return status;
  float world[16], identity[16];
  identityMatrix(identity);
  if (!worldMatrix(graph_, h.id, world)) return WorldStatus::Rejected;
  return localTransformForWorld(world, identity, out) ? WorldStatus::Ok : WorldStatus::Rejected;
}

WorldStatus GameWorld::setWorldTransform(const ObjectHandle &h, const Transform &value) {
  const auto status = validate(h);
  if (status != WorldStatus::Ok) return status;
  if (!isTransformValid(value)) return WorldStatus::InvalidArgument;
  float desired[16], parent[16];
  transformMatrix(value, desired);
  if (!parentWorldMatrix(graph_, h.id, parent)) return WorldStatus::Rejected;
  Transform local;
  if (!localTransformForWorld(desired, parent, local)) return WorldStatus::Rejected;
  return setLocalTransform(h, local);
}

} // namespace ae::runtime
