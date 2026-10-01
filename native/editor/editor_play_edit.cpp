#include "editor/editor_play_edit.h"

#include "scene/component_schema.h"
#include "scene/script_behavior.h"

#include <cstring>
#include <sstream>
#include <unordered_map>

namespace ae::editor {
namespace {

using runtime::ObjectId;
using runtime::WorldStatus;

std::string serialized(const scene::ComponentValue &value) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  value.write(out);
  return out.str();
}

const char *componentName(const scene::ComponentValue &value) {
  const auto *schema = scene::findComponentSchema(value.type().id);
  return schema ? schema->name : "Componente";
}

class Applier {
public:
  Applier(const runtime::SceneGraph &before, const runtime::SceneGraph &after, const PlayEditContext &context)
      : before_(before), after_(after), context_(context), world_(context.play.world()) {}

  PlayEditResult run() {
    std::vector<ObjectId> previous, current;
    before_.collectSubtree(before_.root(), previous);
    after_.collectSubtree(after_.root(), current);
    // Remoção primeiro, só do topo de cada subárvore removida: o mundo remove
    // os filhos junto, e pedir de novo cada filho seria uma recusa falsa.
    for (const ObjectId id : previous) {
      if (id == before_.root() || after_.exists(id) || !after_.exists(before_.find(id)->parent)) continue;
      const auto handle = world_.handle(id);
      if (handle.valid()) record("Remover objeto", world_.destroyObject(handle));
    }
    // Pré-ordem: o pai de um objeto criado já existe no mundo quando ele chega.
    for (const ObjectId id : current) object(id);
    return std::move(result_);
  }

private:
  void record(std::string_view what, WorldStatus status) {
    if (status == WorldStatus::Ok) { ++result_.applied; return; }
    if (!result_.refused.empty()) return;
    result_.refused = std::string(what) + ": " + runtime::worldStatusMessage(status);
    result_.refusedField = what;
  }
  ObjectId mapped(ObjectId id) const {
    const auto found = ids_.find(id);
    return found == ids_.end() ? id : found->second;
  }

  void object(ObjectId id) {
    const auto &next = *after_.find(id);
    const auto *previous = before_.find(id);
    runtime::ObjectHandle handle;
    runtime::SceneObject blank;
    if (!previous) {
      WorldStatus status = WorldStatus::Ok;
      handle = world_.createObject(world_.handle(mapped(next.parent)), next.name, status);
      if (!handle.valid()) { record("Criar objeto", status); return; }
      ids_[id] = handle.id;
      result_.created.emplace_back(id, handle.id);
      runtime::Transform transform;
      world_.localTransform(handle, transform);
      blank.transform = transform;
      assignObjectName(blank, next.name);
      // O objeto novo nasce com os componentes do mundo (nenhum), então a
      // base de comparação é o próprio estado inicial dele.
      previous = &blank;
    } else {
      handle = world_.handle(id);
      // O jogo removeu o objeto depois da foto: não há o que editar.
      if (!handle.valid()) return;
    }
    const bool root = id == after_.root();
    if (!root) fields(handle, *previous, next);
    components(handle, *previous, next);
  }

  void fields(const runtime::ObjectHandle &handle, const runtime::SceneObject &previous, const runtime::SceneObject &next) {
    if (previous.id && previous.parent != next.parent) {
      u32 index = 0;
      after_.childIndexOf(next.id, index);
      record("Mudar o pai", world_.setParent(handle, world_.handle(mapped(next.parent)), index,
                                             runtime::ReparentPosePolicy::KeepLocal));
    }
    if (std::strcmp(previous.name, next.name) != 0) record("Nome", world_.setName(handle, next.name));
    if (previous.active != next.active) record("Ativo", world_.setActive(handle, next.active));
    if (previous.layer != next.layer) record("Camada", world_.setLayer(handle, next.layer));
    if (previous.tag != next.tag) record("Tag", world_.setTag(handle, next.tag));
    if (previous.groups != next.groups) record("Grupos", world_.setGroups(handle, next.groups));
    if (previous.visible != next.visible || previous.castShadow != next.castShadow ||
        previous.receiveShadow != next.receiveShadow)
      record("Visibilidade e sombras", world_.setRenderFlags(handle, next.visible, next.castShadow, next.receiveShadow));
    if (previous.isStatic != next.isStatic) record("Estático", WorldStatus::NotMutableInPlay);
    // Canal a canal sobre a pose ATUAL do mundo: a física ou um script podem
    // ter movido o objeto depois da foto, e só o eixo tocado é do usuário.
    runtime::Transform transform;
    if (world_.localTransform(handle, transform) != WorldStatus::Ok) return;
    bool changed = false;
    const auto merge = [&](const float (&from)[3], const float (&to)[3], float (&out)[3]) {
      for (u32 axis = 0; axis < 3; ++axis)
        if (from[axis] != to[axis]) { out[axis] = to[axis]; changed = true; }
    };
    merge(previous.transform.position, next.transform.position, transform.position);
    merge(previous.transform.rotationDegrees, next.transform.rotationDegrees, transform.rotationDegrees);
    merge(previous.transform.scale, next.transform.scale, transform.scale);
    if (changed) record("Transform", world_.placeLocalTransform(handle, transform));
  }

  void components(const runtime::ObjectHandle &handle, const runtime::SceneObject &previous, const runtime::SceneObject &next) {
    for (usize i = 0; i < previous.components.size(); ++i) {
      const auto *value = previous.components.at(i);
      if (!next.components.findInstance(value->instanceId()))
        record(componentName(*value), world_.removeComponent({handle, value->instanceId()}));
    }
    // Ordem relativa dos que continuam: o mundo não reordena em execução, e
    // fingir que reordenou mudaria a ordem dos callbacks só na tela.
    usize cursor = 0;
    for (usize i = 0; i < next.components.size(); ++i) {
      const auto *value = next.components.at(i);
      const auto *old = previous.components.findInstance(value->instanceId());
      if (!old) continue;
      while (cursor < previous.components.size() && !next.components.findInstance(previous.components.at(cursor)->instanceId()))
        ++cursor;
      if (cursor < previous.components.size() && previous.components.at(cursor)->instanceId() != value->instanceId()) {
        record("Reordenar componentes", WorldStatus::NotMutableInPlay);
        break;
      }
      ++cursor;
    }
    for (usize i = 0; i < next.components.size(); ++i) {
      const auto *value = next.components.at(i);
      const auto *old = previous.components.findInstance(value->instanceId());
      if (!old) {
        WorldStatus status = WorldStatus::Ok;
        const auto created = world_.addComponent(handle, value->type().id, status);
        record(componentName(*value), status);
        if (!created.valid()) continue;
        if (const auto *fresh = world_.readComponent(created)) {
          const auto baseline = fresh->clone();
          properties(created, *baseline, *value);
        }
        continue;
      }
      if (serialized(*old) == serialized(*value)) continue;
      if (const auto *script = scene::scriptBehavior(value)) behavior(handle, *scene::scriptBehavior(old), *script);
      else properties({handle, value->instanceId()}, *old, *value);
    }
  }

  void behavior(const runtime::ObjectHandle &handle, const scene::ScriptBehavior &old, const scene::ScriptBehavior &next) {
    if (old.scriptType != next.scriptType || old.source != next.source) {
      record("Trocar o script", WorldStatus::NotMutableInPlay);
      return;
    }
    std::vector<scene::ScriptPropertyValue> changed;
    for (const auto &property : next.properties) {
      const auto match = std::find_if(old.properties.begin(), old.properties.end(),
                                      [&](const auto &p) { return p.id == property.id; });
      if (match == old.properties.end() || match->value != property.value || match->valueType != property.valueType) {
        changed.push_back(property);
        // Objeto criado no espelho durante o Play: o script recebe o id do mundo.
        scene::remapScriptPropertyObjects(changed.back(), [&](u64 target) {
          return target ? static_cast<u64>(mapped(static_cast<ObjectId>(target))) : target;
        });
      }
    }
    auto applied = next;
    for (const auto &property : changed) applied.setProperty(property.id, property.valueType, property.value);
    if (context_.play.editBehavior(handle.id, applied, changed)) { ++result_.applied; return; }
    if (result_.refused.empty()) result_.refusedField = "Comportamento";
    if (result_.refused.empty())
      result_.refused = "Comportamento: " + (context_.play.scriptDiagnostics().empty()
                                                 ? std::string("a instância recusou a edição")
                                                 : context_.play.scriptDiagnostics());
  }

  // Um valor por propriedade que mudou. Recursos antes dos valores por slot:
  // trocar a malha muda quantos slots existem.
  void properties(const runtime::ComponentHandle &component, const scene::ComponentValue &old,
                  const scene::ComponentValue &next) {
    const auto &type = next.type();
    const u32 before = result_.applied;
    bool attempted = false;
    for (const auto &binding : type.resourceBindings) {
      if (!binding.read) continue;
      const u32 slots = std::min(binding.slotCount(old), binding.slotCount(next));
      for (u32 slot = 0; slot < slots; ++slot) {
        const auto value = binding.read(next, slot);
        if (binding.read(old, slot) == value) continue;
        attempted = true;
        record(binding.name ? binding.name : "Recurso",
               world_.setResource(component, binding.id, slot, value, context_.assets, context_.environmentProfiles,
                                  context_.resolveResource));
      }
    }
    for (const auto &p : type.numbers)
      if (p.read && p.read(old) != p.read(next)) {
        attempted = true;
        record(p.name, world_.setProperty(component, p.id, p.read(next)));
      }
    for (const auto &p : type.booleans)
      if (p.read && p.read(old) != p.read(next)) {
        attempted = true;
        record(p.name, world_.setProperty(component, p.id, p.read(next)));
      }
    for (const auto &p : type.enums)
      if (p.read && p.read(old) != p.read(next)) {
        attempted = true;
        record(p.name, world_.setProperty(component, p.id, p.read(next)));
      }
    for (const auto &p : type.references)
      if (p.read && p.write && p.read(old) != p.read(next)) {
        attempted = true;
        const u64 target = p.read(next);
        record(p.name, world_.setProperty(component, p.id,
                                          scene::ObjectReference{target ? static_cast<u64>(mapped(static_cast<ObjectId>(target))) : 0}));
      }
    for (const auto &p : type.slotNumbers) {
      if (!p.read) continue;
      const u32 slots = std::min(p.slotCount(old), p.slotCount(next));
      for (u32 slot = 0; slot < slots; ++slot)
        if (p.read(old, slot) != p.read(next, slot)) {
          attempted = true;
          record(p.name, world_.setSlotProperty(component, p.id, slot, p.read(next, slot), context_.resolveResource));
        }
    }
    for (const auto &p : type.slotEnums) {
      if (!p.read) continue;
      const u32 slots = std::min(p.slotCount(old), p.slotCount(next));
      for (u32 slot = 0; slot < slots; ++slot)
        if (p.read(old, slot) != p.read(next, slot)) {
          attempted = true;
          record(p.name, world_.setSlotProperty(component, p.id, slot, p.read(next, slot), context_.resolveResource));
        }
    }
    // O valor mudou num campo que não tem endereço público (lista, curva):
    // o mundo não sabe aplicar em execução, e isso é dito em vez de ignorado.
    if (!attempted && result_.applied == before && serialized(old) != serialized(next))
      record(componentName(next), WorldStatus::NotMutableInPlay);
  }

  const runtime::SceneGraph &before_;
  const runtime::SceneGraph &after_;
  const PlayEditContext &context_;
  runtime::GameWorld &world_;
  std::unordered_map<ObjectId, ObjectId> ids_;
  PlayEditResult result_;
};

} // namespace

PlayEditResult applyPlayEdits(const runtime::SceneGraph &before, const runtime::SceneGraph &after,
                              const PlayEditContext &context) {
  if (!context.play.active()) return {0, "Play parado", "Play", {}};
  auto result = Applier(before, after, context).run();
  if (!context.play.commitEdits() && result.refused.empty()) {
    result.refusedField = "Física";
    result.refused = context.play.physicsError().empty() ? std::string("O mundo recusou a edição no ponto seguro")
                                                         : context.play.physicsError();
  }
  return result;
}

} // namespace ae::editor
