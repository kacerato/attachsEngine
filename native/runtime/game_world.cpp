#include "runtime/game_world.h"
#include "runtime/prefab.h"
#include "scene/camera_look.h"
#include "scene/camera.h"
#include "scene/environment.h"
#include "runtime/scene_components.h"

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
    case WorldStatus::UnknownResource: return "Recurso inexistente";
    case WorldStatus::ResourceTypeMismatch: return "Tipo de recurso incompatível";
    case WorldStatus::ClipNotInComponent: return "Clipe fora da lista do componente Animação";
    case WorldStatus::UnknownElement: return "Elemento da coleção inexistente";
    case WorldStatus::OperationExpired: return "Resultado da operação não está mais disponível";
    case WorldStatus::PropertyNotTweenable: return "A propriedade não possui escrita numérica elegível para tween";
    case WorldStatus::PropertyAlreadyTweening: return "Outro tween já controla esta propriedade";
    case WorldStatus::PropertyWrittenExternally: return "Outro escritor alterou a propriedade durante o tween";
    case WorldStatus::UnknownOperation: return "O componente não declara esse método ou evento";
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
  hierarchyChanges_.clear();
  hierarchyDropped_ = 0;
  slots_.clear();
  authorities_.clear();
  commands_.clear();
  operationResults_.clear();
  nextOperationId_ = 1;
  scratch_.clear();
  worldId_ = 0;
  structuralRevision_ = 0;
  invalidated_ = 0;
  elapsed_ = 0;
  clock_.reset();
  delayedDestroy_.clear();
  unpublishedClones_.clear();
}

u32 GameWorld::subtreeInvalidation(ObjectId id) {
  u32 mask = 0;
  graph_.collectSubtree(id, scratch_);
  for (const ObjectId member : scratch_)
    if (const auto *object = graph_.find(member))
      for (usize i = 0; i < object->components.size(); ++i)
        if (const auto *schema = scene::findComponentSchema(object->components.at(i)->type().id))
          mask |= schema->invalidates;
  return mask;
}

void GameWorld::invalidateType(std::string_view typeId) {
  if (const auto *schema = scene::findComponentSchema(typeId)) invalidated_ |= schema->invalidates;
}

bool GameWorld::load(const SceneGraph &source) {
  clear();
  std::vector<ObjectId> ids;
  source.collectSubtree(source.root(), ids);
  for (const ObjectId id : ids) {
    const auto *object = source.find(id);
    if (!object || object->components.hasUnresolved() || !source.tags().contains(object->tag)) return false;
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

bool GameWorld::activeSelf(const ObjectHandle &h) const noexcept {
  const auto *object = find(h);
  return object && object->active;
}

bool GameWorld::activeInHierarchy(const ObjectHandle &h) const noexcept {
  return validate(h) == WorldStatus::Ok && graph_.activeInHierarchy(h.id);
}

WorldStatus GameWorld::setActive(const ObjectHandle &h, bool active) {
  const auto status = validate(h);
  if (status != WorldStatus::Ok) return status;
  const bool changed = graph_.find(h.id) && graph_.find(h.id)->active != active;
  if (!changed) return WorldStatus::Ok;
  if (!graph_.setActive(h.id, active)) return WorldStatus::Rejected;
  ++structuralRevision_;
  if (changed) invalidated_ |= subtreeInvalidation(h.id);
  return WorldStatus::Ok;
}

std::string_view GameWorld::tagOf(const ObjectHandle &h) const noexcept {
  const auto *object=find(h);return object?std::string_view(object->tag):std::string_view{};
}
WorldStatus GameWorld::setTag(const ObjectHandle &h,std::string_view tag) {
  const auto status=validate(h);if(status!=WorldStatus::Ok) return status;
  if(!graph_.tags().contains(tag)) return WorldStatus::InvalidArgument;
  auto value=*find(h);if(value.tag==tag) return WorldStatus::Ok;
  value.tag=tag;return graph_.applyEntityValues(h.id,value)?WorldStatus::Ok:WorldStatus::Rejected;
}
WorldStatus GameWorld::compareTag(const ObjectHandle &h,std::string_view tag,bool &matches) const {
  matches=false;const auto status=validate(h);if(status!=WorldStatus::Ok) return status;
  if(!graph_.tags().contains(tag)) return WorldStatus::InvalidArgument;
  matches=tagOf(h)==tag;return WorldStatus::Ok;
}
WorldStatus GameWorld::findTagged(std::string_view tag,std::span<u64> output,u32 &count,bool firstOnly) const {
  count=0;if(!running()) return WorldStatus::NotRunning;
  if(!graph_.tags().contains(tag)) return WorldStatus::InvalidArgument;
  // Só visita os filhos de objetos vivos e ativos: não percorre a cadeia
  // de ancestrais para cada candidato, nem inclui destruições pendentes.
  std::vector<ObjectId> ids{graph_.root()};
  for(usize cursor=0;cursor<ids.size();++cursor) {
    const auto id=ids[cursor];
    const auto h=handle(id);
    if(!alive(h) || !activeSelf(h)) continue;
    if(id!=graph_.root() && tagOf(h)==tag) {
      if(count<output.size()) output[count]=id;
      ++count;if(firstOnly) break;
    }
    for(const auto child:graph_.childrenOf(id)) ids.push_back(child);
  }
  return WorldStatus::Ok;
}

WorldStatus GameWorld::setGroups(const ObjectHandle &h,const ObjectGroups &groups) {
  const auto status=validate(h);if(status!=WorldStatus::Ok) return status;
  auto value=*find(h);if(value.groups==groups) return WorldStatus::Ok;
  value.groups=groups;return graph_.applyEntityValues(h.id,value)?WorldStatus::Ok:WorldStatus::Rejected;
}
WorldStatus GameWorld::setGroupMembership(const ObjectHandle &h,std::string_view name,bool member) {
  const auto status=validate(h);if(status!=WorldStatus::Ok) return status;
  if(!ObjectGroups::validName(name)) return WorldStatus::InvalidArgument;
  auto groups=find(h)->groups;
  if(!(member?groups.add(name):groups.remove(name))) return WorldStatus::Rejected;
  return setGroups(h,groups);
}
WorldStatus GameWorld::isInGroup(const ObjectHandle &h,std::string_view name,bool &member) const {
  member=false;const auto status=validate(h);if(status!=WorldStatus::Ok) return status;
  if(!ObjectGroups::validName(name)) return WorldStatus::InvalidArgument;
  member=find(h)->groups.contains(name);return WorldStatus::Ok;
}
WorldStatus GameWorld::findGroup(std::string_view name,std::span<u64> output,u32 &count,bool includeInactive) const {
  count=0;if(!running()) return WorldStatus::NotRunning;
  if(!ObjectGroups::validName(name)) return WorldStatus::InvalidArgument;
  std::vector<ObjectId> ids{graph_.root()};
  while(!ids.empty()) {
    const auto id=ids.back();ids.pop_back();const auto h=handle(id);
    if(!alive(h) || (!includeInactive && !activeSelf(h))) continue;
    const auto *object=find(h);
    if(object->groups.contains(name)) {if(count<output.size()) output[count]=id;++count;}
    const auto children=graph_.childrenOf(id);
    for(auto at=children.rbegin();at!=children.rend();++at) ids.push_back(*at);
  }
  return WorldStatus::Ok;
}

WorldStatus GameWorld::setLayer(const ObjectHandle &h, u32 layer) {
  const SceneObject *object = nullptr;
  const auto status = resolve(h, object);
  if (status != WorldStatus::Ok) return status;
  if (layer >= GameplayLayers::kCount) return WorldStatus::InvalidArgument;
  if (object->layer == layer) return WorldStatus::Ok;
  auto values = *object;
  values.layer = layer;
  if (!graph_.applyEntityValues(h.id, values)) return WorldStatus::Rejected;
  invalidated_ |= subtreeInvalidation(h.id);
  return WorldStatus::Ok;
}

WorldStatus GameWorld::setRenderFlags(const ObjectHandle &h, bool visible, bool castShadow, bool receiveShadow) {
  const SceneObject *object = nullptr;
  const auto status = resolve(h, object);
  if (status != WorldStatus::Ok) return status;
  if (object->visible == visible && object->castShadow == castShadow && object->receiveShadow == receiveShadow)
    return WorldStatus::Ok;
  auto values = *object;
  values.visible = visible;
  values.castShadow = castShadow;
  values.receiveShadow = receiveShadow;
  if (!graph_.applyEntityValues(h.id, values)) return WorldStatus::Rejected;
  invalidated_ |= scene::Invalidate::Draw | scene::Invalidate::ShadowMap;
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
  recordHierarchyChange(parent.id, HierarchyChangeKind::ChildrenChanged);
  status = WorldStatus::Ok;
  return {worldId_, id, slots_[id].generation};
}

ObjectHandle GameWorld::createPrimitive(const ObjectHandle &parent,scene::PrimitiveType type,const PrimitiveResource &resource,WorldStatus &status) {
  status=validate(parent);if(status!=WorldStatus::Ok) return {};
  if(!scene::validPrimitive(type)) {status=WorldStatus::InvalidArgument;return {};}
  if(!resource.mesh || !resource.asset.valid()) {status=WorldStatus::UnknownResource;return {};}
  SceneObject values;
  if(!configurePrimitive(values,type,resource)) {status=WorldStatus::Rejected;return {};}
  assignObjectName(values,scene::primitiveNames[static_cast<u32>(type)]);
  const auto id=graph_.createEntity(parent.id,ObjectKind::Mesh,values.name);
  if(!id) {status=WorldStatus::LimitReached;return {};}
  if(!graph_.applyEntityValues(id,values)) {graph_.destroyEntity(id);status=WorldStatus::Rejected;return {};}
  if(id>=slots_.size()) {slots_.resize(static_cast<usize>(id)+1);authorities_.resize(static_cast<usize>(id)+1,TransformAuthority::Free);}
  slots_[id].generation=1;authorities_[id]=TransformAuthority::Free;
  ++structuralRevision_;invalidated_|=subtreeInvalidation(id);
  recordHierarchyChange(parent.id,HierarchyChangeKind::ChildrenChanged);
  status=WorldStatus::Ok;return handle(id);
}

void GameWorld::markSubtreeStale(ObjectId id) {
  graph_.collectSubtree(id, scratch_);
  for (const ObjectId member : scratch_)
    if (member < slots_.size()) slots_[member].generation = 0;
}

ObjectHandle GameWorld::instantiate(const ObjectHandle &source,const ObjectHandle &parent,ObjectCloneMap &mapping,WorldStatus &status) {
  mapping.clear();status=validate(source);if(status!=WorldStatus::Ok) return {};
  status=validate(parent);if(status!=WorldStatus::Ok) return {};
  if(source.id==graph_.root()) {status=WorldStatus::InvalidArgument;return {};}
  if(unpublishedClones_.size()>=32) {status=WorldStatus::LimitReached;return {};}
  const auto root=graph_.cloneSubtree(source.id,parent.id,mapping,[&](ObjectId id) {return handle(id).valid();});
  if(!root) {status=WorldStatus::Rejected;return {};}
  status=WorldStatus::Ok;return registerInstantiation(root,mapping);
}

ObjectHandle GameWorld::instantiate(const Prefab &prefab,const ObjectHandle &parent,ObjectCloneMap &mapping,WorldStatus &status,std::string &diagnostic) {
  mapping.clear();diagnostic.clear();status=validate(parent);if(status!=WorldStatus::Ok) return {};
  if(unpublishedClones_.size()>=32) {status=WorldStatus::LimitReached;return {};}
  const auto root=prefab.instantiate(graph_,parent.id,mapping,diagnostic);
  if(!root) {status=WorldStatus::Rejected;return {};}
  status=WorldStatus::Ok;return registerInstantiation(root,mapping);
}

ObjectHandle GameWorld::instantiateScene(const SceneGraph &source,const ObjectHandle &parent,std::string_view name,ObjectCloneMap &mapping,WorldStatus &status) {
  mapping.clear();status=validate(parent);if(status!=WorldStatus::Ok) return {};
  if(name.empty() || name.size()>=kNameCapacity) {status=WorldStatus::InvalidArgument;return {};}
  if(unpublishedClones_.size()>=32) {status=WorldStatus::LimitReached;return {};}
  // Uma cópia com contêiner: clonar os filhos da raiz um a um perderia as
  // referências cruzadas entre eles, que só são remapeadas dentro de um clone.
  SceneGraph staged=source;
  std::vector<ObjectId> topLevel;
  {const auto children=staged.childrenOf(staged.root());topLevel.assign(children.begin(),children.end());}
  const auto container=staged.createEntity(staged.root(),ObjectKind::Folder,name);
  if(!container) {status=WorldStatus::LimitReached;return {};}
  for(u32 index=0;index<topLevel.size();++index)
    if(!staged.reparent(topLevel[index],container,index)) {status=WorldStatus::Rejected;return {};}
  const auto root=graph_.cloneSubtree(staged,container,parent.id,mapping);
  if(!root) {mapping.clear();status=WorldStatus::Rejected;return {};}
  status=WorldStatus::Ok;return registerInstantiation(root,mapping);
}

ObjectHandle GameWorld::registerInstantiation(ObjectId root,const ObjectCloneMap &mapping) {
  for(const auto &[original,copy]:mapping) {
    (void)original;
    if(copy>=slots_.size()) {slots_.resize(static_cast<usize>(copy)+1);authorities_.resize(static_cast<usize>(copy)+1,TransformAuthority::Free);}
    slots_[copy].generation=1;authorities_[copy]=TransformAuthority::Free;
  }
  ++structuralRevision_;invalidated_|=subtreeInvalidation(root);
  if(const auto *object=graph_.find(root)) recordHierarchyChange(object->parent,HierarchyChangeKind::ChildrenChanged);
  unpublishedClones_.push_back(root);
  return handle(root);
}

WorldStatus GameWorld::finishInstantiation(const ObjectHandle &root,bool commit) {
  const auto status=validate(root);if(status!=WorldStatus::Ok) return status;
  const auto found=std::find(unpublishedClones_.begin(),unpublishedClones_.end(),root.id);
  if(found==unpublishedClones_.end()) return WorldStatus::InvalidArgument;
  unpublishedClones_.erase(found);
  if(!commit) {markSubtreeStale(root.id);graph_.destroyEntity(root.id);++structuralRevision_;}
  return WorldStatus::Ok;
}

bool GameWorld::queue(PendingCommand command, u64 *operationId) {
  if (commands_.size() >= kMaximumPendingCommands) return false;
  if (operationId) {
    if (operationResults_.size() >= kMaximumPendingCommands) operationResults_.pop_front();
    command.operationId = nextOperationId_++;
    operationResults_.push_back({command.operationId, WorldOperationState::Pending, WorldStatus::Ok});
    *operationId = command.operationId;
  }
  commands_.push_back(command);
  return true;
}

void GameWorld::finishOperation(u64 operationId, WorldStatus result) {
  if (!operationId) return;
  for (auto &record : operationResults_) if (record.id == operationId) {
    record.state = result == WorldStatus::Ok ? WorldOperationState::Applied : WorldOperationState::Failed;
    record.result = result;
    return;
  }
}

WorldStatus GameWorld::operationResult(u32 world, u64 operationId, WorldOperationState &state,
                                       WorldStatus &result) const noexcept {
  if (!running()) return WorldStatus::NotRunning;
  if (world != worldId_) return WorldStatus::ForeignWorld;
  if (!operationId || operationId >= nextOperationId_) return WorldStatus::InvalidArgument;
  for (const auto &record : operationResults_) if (record.id == operationId) {
    state = record.state;
    result = record.result;
    return WorldStatus::Ok;
  }
  return WorldStatus::OperationExpired;
}

WorldStatus GameWorld::destroyObject(const ObjectHandle &h, u64 *operationId) {
  const auto status = validate(h);
  if (status != WorldStatus::Ok) return status;
  if (h.id == graph_.root()) return WorldStatus::Rejected;
  // A referência vence AGORA: quem guardou o handle passa a ser recusado ainda
  // dentro deste callback, mesmo que o armazenamento só saia no ponto seguro.
  if (!queue({PendingCommand::Kind::Destroy, h.id, kInvalidObject, 0, 0}, operationId)) return WorldStatus::LimitReached;
  markSubtreeStale(h.id);
  ++structuralRevision_;
  return WorldStatus::Ok;
}

WorldStatus GameWorld::setParent(const ObjectHandle &h, const ObjectHandle &parent, u32 childIndex,
                                 ReparentPosePolicy posePolicy, u64 *operationId) {
  const auto status = validate(h);
  if (status != WorldStatus::Ok) return status;
  const auto parentStatus = validate(parent);
  if (parentStatus != WorldStatus::Ok) return parentStatus;
  if (h.id == graph_.root()) return WorldStatus::Rejected;
  if (graph_.isDescendantOf(parent.id, h.id)) return WorldStatus::Rejected;
  if (posePolicy != ReparentPosePolicy::KeepLocal && posePolicy != ReparentPosePolicy::KeepWorld)
    return WorldStatus::InvalidArgument;
  graph_.collectSubtree(h.id, scratch_);
  for (const ObjectId member : scratch_)
    if (member < authorities_.size() && authorities_[member] != TransformAuthority::Free)
      return WorldStatus::TransformOwnedByPhysics;
  if (posePolicy == ReparentPosePolicy::KeepWorld) {
    float current[16], targetParent[16];
    Transform local;
    if (!worldMatrix(graph_, h.id, current) || !worldMatrix(graph_, parent.id, targetParent) ||
        !localTransformForWorld(current, targetParent, local)) return WorldStatus::Rejected;
  }
  if (!queue({PendingCommand::Kind::Reparent, h.id, parent.id, childIndex, 0, posePolicy}, operationId))
    return WorldStatus::LimitReached;
  return WorldStatus::Ok;
}

u32 GameWorld::flush(std::vector<ObjectId> *destroyed) {
  u32 applied = 0;
  // Índice, não iterador: um comando aplicado pode enfileirar outro, e a fila
  // pode realocar. A ordem é a de chegada; cada comando revalida as condições
  // contra as mudanças que os comandos anteriores acabaram de aplicar.
  for (usize i = 0; i < commands_.size(); ++i) {
    const auto command = commands_[i];
    WorldStatus outcome = WorldStatus::Rejected;
    switch (command.kind) {
      case PendingCommand::Kind::Destroy: {
        if (destroyed) {
          graph_.collectSubtree(command.object, scratch_);
          destroyed->insert(destroyed->end(), scratch_.begin(), scratch_.end());
        }
        const auto *object = graph_.find(command.object);
        const ObjectId parent = object ? object->parent : kInvalidObject;
        if (graph_.destroyEntity(command.object)) {
          ++applied; outcome = WorldStatus::Ok;
          recordHierarchyChange(parent, HierarchyChangeKind::ChildrenChanged);
        }
        break;
      }
      case PendingCommand::Kind::Reparent: {
        if (!graph_.exists(command.object) || !graph_.exists(command.parent)) {
          outcome = WorldStatus::StaleHandle; break;
        }
        if (graph_.isDescendantOf(command.parent, command.object)) break;
        const ObjectId previousParent = graph_.find(command.object)->parent;
        // Ownership can change after queueing (e.g. physics rebuild). Revalidate
        // the complete subtree before publishing either reparent policy.
        graph_.collectSubtree(command.object, scratch_);
        if (std::any_of(scratch_.begin(), scratch_.end(), [this](ObjectId member) {
              return member < authorities_.size() && authorities_[member] != TransformAuthority::Free;
            })) {
          outcome = WorldStatus::TransformOwnedByPhysics; break;
        }
        if (command.posePolicy == ReparentPosePolicy::KeepWorld) {
          float current[16], targetParent[16];
          Transform local;
          if (!worldMatrix(graph_, command.object, current) ||
              !worldMatrix(graph_, command.parent, targetParent) ||
              !localTransformForWorld(current, targetParent, local)) break;
          if (graph_.reparent(command.object, command.parent, command.childIndex) &&
              graph_.setTransform(command.object, local)) { ++applied; outcome = WorldStatus::Ok; }
        } else if (graph_.reparent(command.object, command.parent, command.childIndex)) {
          ++applied; outcome = WorldStatus::Ok;
        }
        if (outcome == WorldStatus::Ok) {
          invalidated_ |= subtreeInvalidation(command.object);
          // Reordenar dentro do mesmo pai muda a lista de filhos, não o pai.
          if (previousParent != command.parent) {
            recordParentChangedSubtree(command.object);
            recordHierarchyChange(previousParent, HierarchyChangeKind::ChildrenChanged);
          }
          recordHierarchyChange(command.parent, HierarchyChangeKind::ChildrenChanged);
        }
        break;
      }
      case PendingCommand::Kind::RemoveComponent:
        // A callback may have added a dependent after removal was queued.
        // Re-resolve at the safe point before publishing the structural edit.
        if(const auto *object=graph_.find(command.object)) {
          if(scene::componentInstanceRemovalBlockedBy(command.instance,object->components) ||
             componentRemovalReferenceUse(graph_,command.object,command.instance).object) {
            outcome = WorldStatus::ComponentInUse; break;
          }
        } else {
          outcome = WorldStatus::StaleHandle; break;
        }
        if (auto *components = editComponents(command.object)) {
          const auto *removed = components->findInstance(command.instance);
          const std::string typeId = removed ? std::string(removed->type().id) : std::string();
          if (components->removeInstance(command.instance)) {
            ++applied; outcome = WorldStatus::Ok; invalidateType(typeId);
          }
          else outcome = WorldStatus::ComponentMissing;
        }
        break;
    }
    finishOperation(command.operationId, outcome);
  }
  commands_.clear();
  if (applied) ++structuralRevision_;
  return applied;
}

void GameWorld::recordHierarchyChange(ObjectId id, HierarchyChangeKind kind) {
  // A raiz sintética não tem comportamentos; objeto morto não recebe callback.
  if (id == kInvalidObject || id == graph_.root() || !graph_.exists(id)) return;
  const auto target = handle(id);
  if (!target.valid()) return;
  for (const auto &change : hierarchyChanges_)
    if (change.object == target && change.kind == kind) return;  // um aviso por quadro basta
  if (hierarchyChanges_.size() >= kHierarchyChangeCapacity) {
    hierarchyChanges_.erase(hierarchyChanges_.begin());
    ++hierarchyDropped_;
  }
  hierarchyChanges_.push_back({target, kind});
}

void GameWorld::recordParentChangedSubtree(ObjectId id) {
  std::vector<ObjectId> members;
  graph_.collectSubtree(id, members);
  for (const ObjectId member : members) recordHierarchyChange(member, HierarchyChangeKind::ParentChanged);
}

std::vector<GameWorld::HierarchyChange> GameWorld::takeHierarchyChanges() {
  std::vector<HierarchyChange> out;
  out.swap(hierarchyChanges_);
  return out;
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
  auto plan=scene::planComponentAddition(object->components,typeId,true);
  if (!plan.ready) {
    status = WorldStatus::ComponentUnavailable;
    return {};
  }
  auto *components = editComponents(h.id);
  if (!components) { status = WorldStatus::StaleHandle; return {}; }
  const auto createdInstance=plan.requestedInstance;
  *components=std::move(plan.candidate);
  ++structuralRevision_;
  invalidated_ |= schema->invalidates;
  status = WorldStatus::Ok;
  return {h, createdInstance};
}

ComponentHandle GameWorld::addBehavior(const ObjectHandle &h, std::string_view typeId,
    std::string_view source, WorldStatus &status) {
  status=validate(h);if(status!=WorldStatus::Ok) return {};
  scene::ScriptBehavior value;value.scriptType=typeId;value.source=source;
  if(!value.valid() || typeId.find('\0')!=std::string_view::npos || source.find('\0')!=std::string_view::npos) {
    status=WorldStatus::InvalidArgument;return {};
  }
  const auto *object=find(h);
  if(object->components.size()>=scene::Components::MaximumCount) {status=WorldStatus::LimitReached;return {};}
  auto *components=editComponents(h.id);
  auto *created=static_cast<scene::ScriptBehavior *>(components->add(scene::ScriptBehavior::descriptor));
  if(!created) {status=WorldStatus::LimitReached;return {};}
  created->scriptType=std::move(value.scriptType);created->source=std::move(value.source);
  ++structuralRevision_;status=WorldStatus::Ok;return {h,created->instanceId()};
}

WorldStatus GameWorld::destroyAfter(const ObjectHandle &h,double seconds) {
  const auto status=validate(h);if(status!=WorldStatus::Ok) return status;
  if(!std::isfinite(seconds) || seconds<0 || !std::isfinite(elapsed_+seconds)) return WorldStatus::InvalidArgument;
  if(seconds==0) return destroyObject(h);
  if(h.id==graph_.root()) return WorldStatus::InvalidArgument;
  for(auto &pending:delayedDestroy_) if(pending.object.id==h.id) {
    pending.due=std::min(pending.due,elapsed_+seconds);return WorldStatus::Ok;
  }
  if(delayedDestroy_.size()>=4096) return WorldStatus::LimitReached;
  delayedDestroy_.push_back({h,elapsed_+seconds});return WorldStatus::Ok;
}
void GameWorld::advanceClock(double delta) {
  if(!std::isfinite(delta) || delta<0 || !std::isfinite(elapsed_+delta)) return;
  elapsed_+=delta;
  std::erase_if(delayedDestroy_,[&](const DelayedDestroy &pending) {
    if(!alive(pending.object)) return true;
    // A fila cheia é transitória: conserve o pedido para o próximo ponto seguro.
    return pending.due<=elapsed_ && destroyObject(pending.object)!=WorldStatus::LimitReached;
  });
}

WorldStatus GameWorld::removeComponent(const ComponentHandle &component, u64 *operationId) {
  const auto *object = find(component.object);
  const auto status = validate(component.object);
  if (status != WorldStatus::Ok || !object) return status;
  const auto *value = object->components.findInstance(component.instance);
  if (!value) return WorldStatus::ComponentMissing;
  const auto *schema = scene::findComponentSchema(value->type().id);
  if (!schema) return WorldStatus::UnknownComponent;
  if (schema->structuralInPlay == scene::PlayMutability::Never && !scene::scriptBehavior(value)) return WorldStatus::NotMutableInPlay;
  // Só bloqueia quando esta é a ÚLTIMA instância do tipo: remover um dos dois
  // colisores não quebra quem exige "um colisor".
  if (scene::componentInstanceRemovalBlockedBy(component.instance,object->components) ||
      componentRemovalReferenceUse(graph_,component.object.id,component.instance).object)
    return WorldStatus::ComponentInUse;
  // Scripts não são consumidos por buffers nativos em execução. A remoção
  // imediata impede reentrada de mensagem/callback na instância removida.
  if(scene::scriptBehavior(value) && !operationId) {
    editComponents(component.object.id)->removeInstance(component.instance);
    ++structuralRevision_;return WorldStatus::Ok;
  }
  if (!queue({PendingCommand::Kind::RemoveComponent, component.object.id, kInvalidObject, 0, component.instance}, operationId))
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
  // Enabled é estado compartilhado pelo componente e pela instância C#.
  // Os campos autorados de script continuam no caminho validado de editBehavior.
  const bool behaviorEnabled = typeId == "astra.script.behavior" && propertyId == "enabled" &&
                               std::holds_alternative<bool>(value);
  if (schema->propertiesInPlay == scene::PlayMutability::Never && !behaviorEnabled)
    return WorldStatus::NotMutableInPlay;
  if (propertyId == "enabled") if (const auto *enabled = std::get_if<bool>(&value))
    for (const auto &property : current->type().booleans)
      if (property.id == propertyId && property.read(*current) == *enabled) return WorldStatus::Ok;
  // A ABI transporta u64, mas o grafo usa u32. Truncar aqui poderia fazer um
  // ID inválido acertar outro objeto vivo com os mesmos 32 bits baixos.
  if (const auto *reference = std::get_if<scene::ObjectReference>(&value)) {
    if (reference->id > std::numeric_limits<ObjectId>::max()) return WorldStatus::InvalidArgument;
    if (reference->id) {
      const auto target = static_cast<ObjectId>(reference->id);
      if (!graph_.exists(target)) return WorldStatus::UnknownObject;
      if (!handle(target).valid()) return WorldStatus::StaleHandle;
    }
    for (const auto &property : current->type().references)
      if (property.id == propertyId && !referenceAccepts(graph_, component.object.id, property, reference->id))
        return WorldStatus::Rejected;
  }
  auto *components = editComponents(component.object.id);
  if (!components) return WorldStatus::StaleHandle;
  switch (scene::setComponentProperty(*components, typeId, propertyId, value, component.instance)) {
    case scene::ComponentPropertyStatus::Applied: {
      u32 invalidates=schema->invalidates;
      if(typeId==scene::Character::descriptor.id&&scene::characterMotionProperty(propertyId))invalidates&=~(scene::Invalidate::PhysicsBody|scene::Invalidate::PhysicsShape);
      invalidated_|=invalidates;return WorldStatus::Ok;
    }
    case scene::ComponentPropertyStatus::MissingComponent: return WorldStatus::ComponentMissing;
    case scene::ComponentPropertyStatus::UnknownProperty:
    case scene::ComponentPropertyStatus::AmbiguousProperty:
    case scene::ComponentPropertyStatus::TypeMismatch: return WorldStatus::InvalidArgument;
    case scene::ComponentPropertyStatus::InvalidValue: return WorldStatus::Rejected;
  }
  return WorldStatus::Rejected;
}

WorldStatus GameWorld::validateTweenNumber(const ComponentHandle &component,std::string_view propertyId,float destination,float &initial) const {
  const auto valid=validate(component.object);if(valid!=WorldStatus::Ok)return valid;
  const auto *value=readComponent(component);if(!value)return WorldStatus::ComponentMissing;
  const auto *schema=scene::findComponentSchema(value->type().id);if(!schema)return WorldStatus::UnknownComponent;
  if(schema->propertiesInPlay==scene::PlayMutability::Never)return WorldStatus::NotMutableInPlay;
  const scene::ComponentNumber *number=nullptr;
  for(const auto &p:value->type().numbers)if(p.id==propertyId){if(number)return WorldStatus::InvalidArgument;number=&p;}
  if(!number)return WorldStatus::InvalidArgument;
  constexpr u32 heavy=scene::Invalidate::PhysicsBody|scene::Invalidate::PhysicsShape|scene::Invalidate::MeshDerived|scene::Invalidate::TextureResidency|scene::Invalidate::Policy|scene::Invalidate::Script;
  const auto capability=number->presentation.capability.empty()?schema->capability:number->presentation.capability;
  if(!number->tweenable||!number->read||!number->write||!number->presentation.isEditable(*value)||!number->presentation.isVisible(*value)||!core::engineCapabilityAuthorable(capability)||((schema->invalidates|number->presentation.invalidates)&heavy))return WorldStatus::PropertyNotTweenable;
  if(!std::isfinite(destination)||destination<number->minimum||destination>number->maximum)return WorldStatus::Rejected;
  initial=number->read(*value);return std::isfinite(initial)?WorldStatus::Ok:WorldStatus::Rejected;
}
WorldStatus GameWorld::setTweenNumber(const ComponentHandle &component,std::string_view propertyId,float value) {
  float previous=0;const auto status=validateTweenNumber(component,propertyId,value,previous);if(status!=WorldStatus::Ok)return status;
  auto *components=editComponents(component.object.id);if(!components)return WorldStatus::StaleHandle;
  auto *current=components->editInstance(component.instance);if(!current)return WorldStatus::ComponentMissing;
  for(const auto &p:current->type().numbers)if(p.id==propertyId) {
    auto *field=p.write(*current);if(!field)return WorldStatus::Rejected;
    *field=value;if(!current->valid()){*field=previous;return WorldStatus::Rejected;}
    const auto *schema=scene::findComponentSchema(current->type().id);
    invalidated_|=schema->invalidates|p.presentation.invalidates;return WorldStatus::Ok;
  }
  return WorldStatus::InvalidArgument;
}

WorldStatus GameWorld::setTriple(const ComponentHandle &component,std::string_view propertyId,
                                const float (&values)[3]) {
  const auto status=validate(component.object);
  if(status!=WorldStatus::Ok) return status;
  const auto *current=readComponent(component);
  if(!current) return WorldStatus::ComponentMissing;
  const auto *schema=scene::findComponentSchema(current->type().id);
  if(!schema) return WorldStatus::UnknownComponent;
  if(schema->propertiesInPlay==scene::PlayMutability::Never) return WorldStatus::NotMutableInPlay;
  auto *components=editComponents(component.object.id);
  if(!components) return WorldStatus::StaleHandle;
  switch(scene::setComponentTriple(*components,current->type().id,propertyId,values,component.instance)) {
    case scene::ComponentPropertyStatus::Applied: invalidated_|=schema->invalidates;return WorldStatus::Ok;
    case scene::ComponentPropertyStatus::MissingComponent: return WorldStatus::ComponentMissing;
    case scene::ComponentPropertyStatus::InvalidValue: return WorldStatus::Rejected;
    default: return WorldStatus::InvalidArgument;
  }
}

WorldStatus GameWorld::setTimeScale(float value) {
  if(!running()) return WorldStatus::NotRunning;
  return clock_.setScale(value)?WorldStatus::Ok:WorldStatus::InvalidArgument;
}
bool GameWorld::beginFrame(double elapsed,bool editorStep) {
  if(!running() || !clock_.beginFrame(elapsed,editorStep)) return false;
  advanceClock(clock_.delta());
  return true;
}

WorldStatus GameWorld::getSlotProperty(const ComponentHandle &component,std::string_view propertyId,u32 slot,
                                       scene::ComponentPropertyValue &out) const {
  const auto status=validate(component.object);if(status!=WorldStatus::Ok)return status;
  const auto *value=readComponent(component);if(!value)return WorldStatus::ComponentMissing;
  const scene::ComponentSlotNumber *number=nullptr;const scene::ComponentSlotEnum *enumeration=nullptr;u32 matches=0;
  for(const auto &p:value->type().slotNumbers)if(p.id==propertyId){number=&p;++matches;}
  for(const auto &p:value->type().slotEnums)if(p.id==propertyId){enumeration=&p;++matches;}
  if(matches!=1)return WorldStatus::InvalidArgument;
  if(number) {
    if(!number->read||slot>=number->slotCount(*value))return WorldStatus::InvalidArgument;
    out=number->read(*value,slot);return WorldStatus::Ok;
  }
  if(!enumeration->read||slot>=enumeration->slotCount(*value))return WorldStatus::InvalidArgument;
  out=enumeration->read(*value,slot);return WorldStatus::Ok;
}

WorldStatus GameWorld::setSlotProperty(const ComponentHandle &component,std::string_view propertyId,u32 slot,
                                       const scene::ComponentPropertyValue &input,
                                       const ComponentResourceResolver &resolveResource) {
  const auto status=validate(component.object);if(status!=WorldStatus::Ok)return status;
  const auto *source=readComponent(component);if(!source)return WorldStatus::ComponentMissing;
  const auto *schema=scene::findComponentSchema(source->type().id);
  if(!schema)return WorldStatus::UnknownComponent;
  if(schema->propertiesInPlay==scene::PlayMutability::Never)return WorldStatus::NotMutableInPlay;
  const scene::ComponentSlotNumber *number=nullptr;const scene::ComponentSlotEnum *enumeration=nullptr;u32 matches=0;
  for(const auto &p:source->type().slotNumbers)if(p.id==propertyId){number=&p;++matches;}
  for(const auto &p:source->type().slotEnums)if(p.id==propertyId){enumeration=&p;++matches;}
  if(matches!=1)return WorldStatus::InvalidArgument;
  auto candidate=source->clone();if(!candidate||&candidate->type()!=&source->type())return WorldStatus::Rejected;
  bool written=false;
  if(number) {
    const auto *v=std::get_if<float>(&input);
    if(!v||!std::isfinite(*v)||*v<number->minimum||*v>number->maximum||
       !number->presentation.isEditable(*source)||slot>=number->slotCount(*source)||!number->write)
      return WorldStatus::InvalidArgument;
    written=number->write(*candidate,slot,*v);
  } else {
    const auto *v=std::get_if<u32>(&input);bool option=false;
    if(v)for(const auto &entry:enumeration->options)if(entry.value==*v){option=true;break;}
    if(!v||!option||!enumeration->presentation.isEditable(*source)||
       slot>=enumeration->slotCount(*source)||!enumeration->write)return WorldStatus::InvalidArgument;
    written=enumeration->write(*candidate,slot,*v);
  }
  if(!written||!candidate->valid())return WorldStatus::Rejected;
  // Wrap e filtro fazem parte da chave da textura publicada. A troca só entra
  // no mundo quando o consumidor confirmou as variantes do slot candidato.
  const bool changesSampler=propertyId=="sampling.wrap"||propertyId=="sampling.filter"||
      (propertyId.starts_with("sampling.")&&
       (propertyId.ends_with(".wrap")||propertyId.ends_with(".filter")));
  if(changesSampler&&
     (!resolveResource||!resolveResource({},resources::AssetType::Texture,propertyId,slot,*candidate)))
    return WorldStatus::ComponentUnavailable;
  auto *components=editComponents(component.object.id);
  if(!components||!components->replaceInstance(component.instance,*candidate)) return WorldStatus::StaleHandle;
  invalidated_|=schema->invalidates;
  return WorldStatus::Ok;
}

WorldStatus GameWorld::getResource(const ComponentHandle &component, std::string_view propertyId, u32 slot,
                                   resources::AssetGuid &out) const {
  const auto status = validate(component.object);
  if (status != WorldStatus::Ok) return status;
  const auto *value = readComponent(component);
  if (!value) return WorldStatus::ComponentMissing;
  const scene::ComponentResourceBinding *match = nullptr;
  for (const auto &binding : value->type().resourceBindings)
    if (binding.id == propertyId) { if (match) return WorldStatus::InvalidArgument; match = &binding; }
  if (!match || slot >= match->slotCount(*value) || !match->read) return WorldStatus::InvalidArgument;
  out = match->at(*value, slot);
  return WorldStatus::Ok;
}

WorldStatus GameWorld::resourceElementId(const ComponentHandle &component,std::string_view propertyId,u32 slot,u64 &out) const {
  const auto status=validate(component.object);
  if(status!=WorldStatus::Ok) return status;
  const auto *value=readComponent(component);
  if(!value) return WorldStatus::ComponentMissing;
  const scene::ComponentResourceBinding *match=nullptr;
  for(const auto &binding:value->type().resourceBindings) if(binding.id==propertyId) {
    if(match) return WorldStatus::InvalidArgument;
    match=&binding;
  }
  if(!match || !match->elementId || slot>=match->slotCount(*value)) return WorldStatus::InvalidArgument;
  out=match->elementAt(*value,slot);
  return out?WorldStatus::Ok:WorldStatus::UnknownElement;
}

WorldStatus GameWorld::getResourceByElementId(const ComponentHandle &component,std::string_view propertyId,u64 elementId,
                                               resources::AssetGuid &out) const {
  const auto status=validate(component.object);
  if(status!=WorldStatus::Ok) return status;
  const auto *value=readComponent(component);
  if(!value) return WorldStatus::ComponentMissing;
  if(!elementId) return WorldStatus::InvalidArgument;
  const scene::ComponentResourceBinding *match=nullptr;
  for(const auto &binding:value->type().resourceBindings) if(binding.id==propertyId) {
    if(match) return WorldStatus::InvalidArgument;
    match=&binding;
  }
  if(!match || !match->elementId) return WorldStatus::InvalidArgument;
  for(u32 slot=0;slot<match->slotCount(*value);++slot) if(match->elementAt(*value,slot)==elementId)
    return getResource(component,propertyId,slot,out);
  return WorldStatus::UnknownElement;
}

WorldStatus GameWorld::setResource(const ComponentHandle &component, std::string_view propertyId, u32 slot,
                                   resources::AssetGuid resource, const resources::AssetRegistry &assets,
                                   std::span<const resources::EnvironmentProfile> environmentProfiles,
                                   const ComponentResourceResolver &resolveResource) {
  const auto status = validate(component.object);
  if (status != WorldStatus::Ok) return status;
  const auto *current = readComponent(component);
  if (!current) return WorldStatus::ComponentMissing;
  const auto *schema = scene::findComponentSchema(current->type().id);
  if (!schema) return WorldStatus::UnknownComponent;
  if (schema->propertiesInPlay == scene::PlayMutability::Never) return WorldStatus::NotMutableInPlay;
  const scene::ComponentResourceBinding *match = nullptr;
  for (const auto &binding : current->type().resourceBindings)
    if (binding.id == propertyId) { if (match) return WorldStatus::InvalidArgument; match = &binding; }
  if (!match || slot >= match->slotCount(*current) || !match->write) return WorldStatus::InvalidArgument;
  // Clipe de animação é sub-recurso da fonte importada (identidade derivada,
  // sem registro próprio): quem conhece as fontes carregadas decide.
  const bool clip = match->kind == resources::AssetType::AnimationClip;
  if (resource.valid() && !match->declaresNone(resource) && !clip) {
    const auto *record = assets.find(resource);
    if (!record) return WorldStatus::UnknownResource;
    if (record->type != match->kind) return WorldStatus::ResourceTypeMismatch;
  }
  const resources::EnvironmentProfile *environmentProfile=nullptr;
  if(resource.valid()&&match->kind==resources::AssetType::EnvironmentProfile) {
    for(const auto &profile:environmentProfiles) if(profile.guid==resource) {environmentProfile=&profile;break;}
    // O registro comprova identidade/tipo, mas só a biblioteca contém os
    // valores que tornam a troca de perfil observável no mesmo frame.
    if(!environmentProfile) return WorldStatus::UnknownResource;
  }
  // Valida a troca sobre uma cópia. Além de manter a mutação atômica, isto dá
  // ao publicador o componente efetivo (incluindo sampler e binding recém
  // escritos) antes de aceitar uma textura/material como utilizável.
  auto candidate=current->clone();
  if(!candidate||&candidate->type()!=&current->type()||!match->write(*candidate,slot,resource))
    return WorldStatus::Rejected;
  if(environmentProfile&&&candidate->type()==&scene::Environment::descriptor) {
    auto &environment=static_cast<scene::Environment&>(*candidate);
    environment.values=resources::applyEnvironmentProfile(environment.values,*environmentProfile);
  }
  if(!candidate->valid()) return WorldStatus::Rejected;
  // Mesmo limpar um override pode revelar um material/textura herdado. O
  // consumidor resolve o candidato completo antes do commit, em vez de
  // presumirmos que GUID vazio significa ausência visual.
  const bool needsConsumerResolution=match->kind==resources::AssetType::Mesh||
      match->kind==resources::AssetType::Texture||match->kind==resources::AssetType::Material||(clip&&resource.valid());
  if(needsConsumerResolution&&(!resolveResource||!resolveResource(resource,match->kind,propertyId,slot,*candidate)))
    return WorldStatus::ComponentUnavailable;
  if(!candidate->valid()) return WorldStatus::Rejected;
  auto *components=editComponents(component.object.id);
  if(!components||!components->replaceInstance(component.instance,*candidate)) return WorldStatus::StaleHandle;
  // A malha de um MeshRenderer é também a forma de um colisor Malha sem
  // malha própria (Invalidate::MeshDerived inclui a colisão).
  invalidated_|=schema->invalidates|(match->kind==resources::AssetType::Mesh?scene::Invalidate::MeshDerived:0u);
  return WorldStatus::Ok;
}

WorldStatus GameWorld::setResourceByElementId(const ComponentHandle &component,std::string_view propertyId,u64 elementId,
                                               resources::AssetGuid value,const resources::AssetRegistry &assets,
                                               std::span<const resources::EnvironmentProfile> environmentProfiles,
                                               const ComponentResourceResolver &resolveResource) {
  const auto status=validate(component.object);
  if(status!=WorldStatus::Ok) return status;
  const auto *current=readComponent(component);
  if(!current) return WorldStatus::ComponentMissing;
  if(!elementId) return WorldStatus::InvalidArgument;
  const scene::ComponentResourceBinding *match=nullptr;
  for(const auto &binding:current->type().resourceBindings) if(binding.id==propertyId) {
    if(match) return WorldStatus::InvalidArgument;
    match=&binding;
  }
  if(!match || !match->elementId) return WorldStatus::InvalidArgument;
  for(u32 slot=0;slot<match->slotCount(*current);++slot) if(match->elementAt(*current,slot)==elementId)
    return setResource(component,propertyId,slot,value,assets,environmentProfiles,resolveResource);
  return WorldStatus::UnknownElement;
}

WorldStatus GameWorld::appendAnimationClip(const ComponentHandle &component,resources::AssetGuid clip,u64 &elementId,
                                            const ComponentResourceResolver &resolveResource) {
  elementId=0;
  const auto status=validate(component.object);
  if(status!=WorldStatus::Ok) return status;
  const auto *current=readComponent(component);
  if(!current) return WorldStatus::ComponentMissing;
  if(&current->type()!=&scene::Animation::descriptor) return WorldStatus::InvalidArgument;
  const auto *schema=scene::findComponentSchema(current->type().id);
  if(!schema) return WorldStatus::UnknownComponent;
  if(schema->structuralInPlay==scene::PlayMutability::Never) return WorldStatus::NotMutableInPlay;
  if(!clip.valid()) return WorldStatus::InvalidArgument;
  auto candidate=current->clone();
  if(!candidate||&candidate->type()!=&scene::Animation::descriptor) return WorldStatus::Rejected;
  auto &animation=static_cast<scene::Animation &>(*candidate);
  const u64 added=animation.appendClip(clip);
  if(!added) return WorldStatus::LimitReached;
  if(!animation.valid()) return WorldStatus::Rejected;
  const u32 slot=static_cast<u32>(animation.clips.size()-1);
  if(!resolveResource||!resolveResource(clip,resources::AssetType::AnimationClip,"clips",slot,*candidate))
    return WorldStatus::ComponentUnavailable;
  if(!candidate->valid()) return WorldStatus::Rejected;
  auto *components=editComponents(component.object.id);
  if(!components||!components->replaceInstance(component.instance,*candidate)) return WorldStatus::StaleHandle;
  elementId=added;
  return WorldStatus::Ok;
}

WorldStatus GameWorld::removeAnimationClip(const ComponentHandle &component,u64 elementId) {
  const auto status=validate(component.object);
  if(status!=WorldStatus::Ok) return status;
  const auto *current=readComponent(component);
  if(!current) return WorldStatus::ComponentMissing;
  if(&current->type()!=&scene::Animation::descriptor||!elementId) return WorldStatus::InvalidArgument;
  const auto *schema=scene::findComponentSchema(current->type().id);
  if(!schema) return WorldStatus::UnknownComponent;
  if(schema->structuralInPlay==scene::PlayMutability::Never) return WorldStatus::NotMutableInPlay;
  auto candidate=current->clone();
  if(!candidate||&candidate->type()!=&scene::Animation::descriptor) return WorldStatus::Rejected;
  if(!static_cast<scene::Animation &>(*candidate).removeClip(elementId)) return WorldStatus::UnknownElement;
  if(!candidate->valid()) return WorldStatus::Rejected;
  auto *components=editComponents(component.object.id);
  return components&&components->replaceInstance(component.instance,*candidate)?WorldStatus::Ok:WorldStatus::StaleHandle;
}

WorldStatus GameWorld::moveAnimationClip(const ComponentHandle &component,u64 elementId,u32 targetIndex) {
  const auto status=validate(component.object);
  if(status!=WorldStatus::Ok) return status;
  const auto *current=readComponent(component);
  if(!current) return WorldStatus::ComponentMissing;
  if(&current->type()!=&scene::Animation::descriptor||!elementId) return WorldStatus::InvalidArgument;
  const auto *schema=scene::findComponentSchema(current->type().id);
  if(!schema) return WorldStatus::UnknownComponent;
  if(schema->structuralInPlay==scene::PlayMutability::Never) return WorldStatus::NotMutableInPlay;
  const auto &animation=static_cast<const scene::Animation &>(*current);
  if(targetIndex>=animation.clips.size()) return WorldStatus::InvalidArgument;
  auto candidate=current->clone();
  if(!candidate||&candidate->type()!=&scene::Animation::descriptor) return WorldStatus::Rejected;
  if(!static_cast<scene::Animation &>(*candidate).moveClip(elementId,targetIndex)) return WorldStatus::UnknownElement;
  if(!candidate->valid()) return WorldStatus::Rejected;
  auto *components=editComponents(component.object.id);
  return components&&components->replaceInstance(component.instance,*candidate)?WorldStatus::Ok:WorldStatus::StaleHandle;
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

WorldStatus GameWorld::placeLocalTransform(const ObjectHandle &h, const Transform &value) {
  const auto status = validate(h);
  if (status != WorldStatus::Ok) return status;
  if (!isTransformValid(value)) return WorldStatus::InvalidArgument;
  bool simulated = false;
  graph_.collectSubtree(h.id, scratch_);
  for (const ObjectId member : scratch_)
    simulated = simulated || (member < authorities_.size() && authorities_[member] != TransformAuthority::Free);
  if (!graph_.setTransform(h.id, value)) return WorldStatus::Rejected;
  // A forma de um composto depende da pose das partes; recriar vale também
  // para um colisor filho que não é dono de corpo.
  if (simulated) invalidated_ |= scene::Invalidate::PhysicsBody | scene::Invalidate::PhysicsShape;
  return WorldStatus::Ok;
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

WorldStatus GameWorld::applyCameraLook(const ObjectHandle &h,float x,float y) {
  const auto status=validate(h);
  if(status!=WorldStatus::Ok) return status;
  if(!std::isfinite(x)||!std::isfinite(y)) return WorldStatus::InvalidArgument;
  const auto *object=graph_.find(h.id);
  if(!object->components.find(scene::Camera::descriptor)) return WorldStatus::ComponentMissing;
  const auto *look=static_cast<const scene::CameraLook*>(object->components.find(scene::CameraLook::descriptor));
  if(!look) return WorldStatus::ComponentMissing;
  if(!look->enabled || !activeInHierarchy(h)) return WorldStatus::Ok;
  Transform transform=object->transform;
  if(!scene::applyCameraLookRotation(transform.rotationDegrees,*look,x,y)) return WorldStatus::InvalidArgument;
  if(x==0&&y==0) return WorldStatus::Ok;
  return setLocalTransform(h,transform);
}

} // namespace ae::runtime
