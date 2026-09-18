#include "editor/editor_import_reconcile.h"
#include "runtime/transform_math.h"
#include "scene/script_behavior.h"

#include <algorithm>
#include <limits>
#include <map>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace ae::editor {
namespace {
using resources::AssetGuid;
using resources::ImportNodeMap;
using resources::ImportNodeRecord;
using scene::ImportLink;

struct GuidHash {
  usize operator()(const AssetGuid &guid) const noexcept { return static_cast<usize>(guid.high ^ (guid.low * 31)); }
};

// Histórico quando há, documento direto quando não há.
struct Mutator {
  EditorDocument &document;
  EditorHistory *history;
  bool apply(EditorEntityId id, const EditorEntity &values) const {
    return history ? history->applyValues(document, id, values) : document.applyEntityValues(id, values);
  }
  EditorEntityId create(EditorEntityId parent, std::string_view name) const {
    return history ? history->createEntity(document, parent, EditorEntityKind::Mesh, name)
                   : document.createEntity(parent, EditorEntityKind::Mesh, name);
  }
  bool destroy(EditorEntityId id) const { return history ? history->destroyEntity(document, id) : document.destroyEntity(id); }
  bool reparent(EditorEntityId id, EditorEntityId parent) const {
    const auto index = static_cast<u32>(document.childrenOf(parent).size());
    return history ? history->reparent(document, id, parent, index) : document.reparent(id, parent, index);
  }
};

bool sameVector(const float a[3], const float b[3]) { return a[0] == b[0] && a[1] == b[1] && a[2] == b[2]; }
void copyVector(float to[3], const float from[3]) { std::copy(from, from + 3, to); }

const ImportLink *linkOf(const EditorEntity &entity) { return scene::importLink(entity.components); }

// Identidade de cada slot do objeto, na ordem. Vazio sem malha.
std::vector<AssetGuid> slotAssets(const EditorEntity &entity) {
  std::vector<AssetGuid> slots;
  if (const auto *render = meshRenderer(entity))
    for (u32 slot = 0; slot < render->slotCount(); ++slot) slots.push_back(render->slotAsset(slot));
  return slots;
}

bool slotCarriesMaterial(const scene::MeshRenderer &render, u32 slot) {
  if (render.slotMaterial(slot).enabled || render.slotMaterialAsset(slot).valid()) return true;
  // R4: textura trocada nesta instância também é dado local do slot.
  for (const auto &texture : render.slotTextures(slot)) if (texture.valid()) return true;
  if (render.slotSurface(slot).overrides()) return true;
  for (const auto &sampling : render.slotSampling(slot)) if (sampling.overrides()) return true;
  if (render.slotChannels(slot).overrides() || render.slotOcclusionTexture(slot).valid()) return true;
  return false;
}

// Onde o objeto está pendurado, dito em termos da fonte: 0 no nível das
// raízes, 1 sob o nó `out`, 2 sob algo que não é desta instância.
int localParentNode(const EditorDocument &document, const EditorEntity &entity, const ImportLink &link, AssetGuid &out) {
  out = {};
  if (link.root && link.node.valid()) return 0;
  const auto *parent = document.find(entity.parent);
  const auto *parentLink = parent ? linkOf(*parent) : nullptr;
  if (!parentLink || parentLink->instance != link.instance || parentLink->source != link.source || parentLink->primitive >= 0 ||
      parentLink->orphan || parentLink->unlinked)
    return !parentLink && !link.baseParent.valid() ? 0 : 2;
  if (!parentLink->node.valid()) return 0;
  out = parentLink->node;
  return 1;
}

bool referenced(const EditorDocument &document, EditorEntityId target) {
  std::vector<EditorEntityId> ids;
  document.collectSubtree(document.root(), ids);
  for (const auto id : ids) {
    const auto &entity = *document.find(id);
    for (usize c = 0; c < entity.components.size(); ++c) {
      const auto *component = entity.components.at(c);
      for (const auto &property : component->type().references)
        if (property.read && property.read(*component) == target) return true;
      if (const auto *script = scene::scriptBehavior(component))
        for (const auto &property : script->properties) {
          if (property.valueType != "object") continue;
          std::istringstream in(property.value);
          u64 value = 0;
          if (in >> value && value == target) return true;
        }
    }
  }
  return false;
}

// O objeto carrega algo que o usuário pôs nele? Então não pode sumir sozinho.
// `allowMaterial`: material local conta como dado PRESERVÁVEL (ele cabe num
// slot), usado na migração de partes para slots.
bool carriesLocalData(const EditorDocument &document, EditorEntityId id, const ImportLink &link, bool allowMaterial = false) {
  const auto *entity = document.find(id);
  if (!entity) return false;
  if (!document.childrenOf(id).empty()) return true;
  for (usize c = 0; c < entity->components.size(); ++c) {
    const auto &type = entity->components.at(c)->type();
    if (&type != &scene::MeshRenderer::descriptor && &type != &ImportLink::descriptor) return true;
  }
  if (slotAssets(*entity) != link.baseSlots()) return true;
  if (const auto *render = meshRenderer(*entity)) {
    if (!render->enabled) return true;
    if (!allowMaterial)
      for (u32 slot = 0; slot < render->slotCount(); ++slot)
        if (slotCarriesMaterial(*render, slot)) return true;
  }
  if (link.baseName != entity->name || !sameVector(entity->transform.position, link.basePosition) ||
      !sameVector(entity->transform.rotationDegrees, link.baseRotation) || !sameVector(entity->transform.scale, link.baseScale))
    return true;
  const EditorEntity defaults;
  if (entity->active != defaults.active || entity->visible != defaults.visible || entity->castShadow != defaults.castShadow ||
      entity->receiveShadow != defaults.receiveShadow || entity->isStatic != defaults.isStatic || entity->layer != defaults.layer ||
      entity->rigidBodyEnabled != defaults.rigidBodyEnabled)
    return true;
  return referenced(document, id);
}

// Troca as identidades dos slots, preservando o material dos que continuam.
void setSlots(EditorEntity &values, const std::vector<AssetGuid> &assets, const ImportSlotResolver &slotOf) {
  if (assets.empty()) {
    const auto *render = meshRenderer(values);
    if (!render) return;
    bool material = false;
    for (u32 slot = 0; slot < render->slotCount(); ++slot) material |= slotCarriesMaterial(*render, slot);
    if (!material) values.components.remove(scene::MeshRenderer::descriptor);
    else if (auto *edit = editMeshRenderer(values)) { edit->asset = {}; edit->mesh = 0; edit->submeshes.clear(); }
    return;
  }
  auto *render = editMeshRenderer(values);
  if (!render) return;
  render->submeshes.resize(assets.size() - 1);
  for (u32 slot = 0; slot < assets.size(); ++slot) {
    *render->editSlotAsset(slot) = assets[slot];
    // Sem resolvedor, a reconciliação de slots do pacote resolve depois.
    *render->editSlotMesh(slot) = slotOf && assets[slot].valid() ? slotOf(assets[slot]) : 0;
  }
}

// Câmera do arquivo no objeto recém-criado.
//
// Só no CRIAR: numa reimportação, uma câmera que o autor removeu ou reajustou
// fica como ele deixou — é a mesma regra que a reconciliação já aplica ao resto
// da edição local. A pose não entra aqui porque já é a do nó: a meia volta que
// converte o -Z do glTF no +Z desta engine é aplicada pelo importador, na pose
// do nó, e só em nó que carrega a câmera e mais nada.
void setImportedCamera(EditorEntity &values, const ImportNodeRecord &node) {
  if (!node.camera) return;
  auto *camera = editCamera(values);
  if (!camera) return;
  camera->projection = node.cameraOrthographic ? scene::CameraProjection::Orthographic
                                               : scene::CameraProjection::Perspective;
  if (!node.cameraOrthographic) camera->verticalFov = node.cameraVerticalFov;
  else camera->orthographicHalfHeight = node.cameraHalfHeight;
  camera->nearPlane = node.cameraNear;
  // `zfar` é opcional no glTF (câmera infinita). Zero aqui significa "o arquivo
  // não declarou": o padrão do componente vale, em vez de um infinito que ele
  // recusaria.
  if (node.cameraFar > node.cameraNear) camera->farPlane = node.cameraFar;
  // A câmera importada não rouba o Play de quem já existe na cena: entra como
  // enquadramento disponível, e a escolha continua sendo do autor.
  camera->enabled = false;
  if (!camera->valid()) values.components.remove(scene::Camera::descriptor);
}

void note(ImportReconcileReport &report, std::string text) {
  if (report.notes.size() < 32) report.notes.push_back(std::move(text));
}
} // namespace

std::string importEntityName(std::string_view name) {
  EditorEntity entity;
  assignEntityName(entity, name.empty() ? std::string_view("Objeto") : name);
  return entity.name;
}

bool importNodeTransform(const ImportNodeRecord &node, EditorTransform &out) {
  const float identity[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  return runtime::localTransformForWorld(node.localMatrix, identity, out);
}

void setImportLinkBase(ImportLink &link, const ImportNodeRecord &node, const EditorTransform &transform, i32 primitive,
                       u32 revision, bool slots) {
  link.node = node.id;
  link.primitive = primitive;
  link.revision = revision;
  link.orphan = false;
  link.baseParent = primitive >= 0 ? AssetGuid{} : node.parent;
  link.basePrimitives = static_cast<u32>(node.draws.size());
  link.baseSubmeshes.clear();
  if (primitive >= 0) {
    // A parte artificial (representação legada) não tem pose própria na
    // fonte: identidade.
    const EditorTransform identity;
    copyVector(link.basePosition, identity.position);
    copyVector(link.baseRotation, identity.rotationDegrees);
    copyVector(link.baseScale, identity.scale);
    link.baseAsset = static_cast<usize>(primitive) < node.draws.size() ? node.draws[static_cast<usize>(primitive)] : AssetGuid{};
    return;
  }
  link.baseName = importEntityName(node.name);
  copyVector(link.basePosition, transform.position);
  copyVector(link.baseRotation, transform.rotationDegrees);
  copyVector(link.baseScale, transform.scale);
  if (slots) {
    link.baseAsset = node.draws.empty() ? AssetGuid{} : node.draws.front();
    if (node.draws.size() > 1) link.baseSubmeshes.assign(node.draws.begin() + 1, node.draws.end());
  } else {
    link.baseAsset = node.draws.size() == 1 ? node.draws.front() : AssetGuid{};
  }
}

u32 adoptLegacyImportInstances(EditorDocument &document, EditorHistory *history, const AssetGuid &source,
                               const ImportNodeMap &map, ImportReconcileReport &report) {
  struct Owner { u32 node; i32 primitive; };
  std::unordered_map<AssetGuid, Owner, GuidHash> owners;
  // Nó excluído pelo perfil não adota objeto legado: ligar o objeto e em
  // seguida removê-lo por exclusão apagaria algo que o autor já tinha, sem que
  // ele tenha pedido nada sobre ESTE objeto.
  for (usize n = 0; n < map.nodes.size(); ++n)
    if (!map.nodes[n].excluded)
      for (usize p = 0; p < map.nodes[n].draws.size(); ++p)
        owners[map.nodes[n].draws[p]] = {static_cast<u32>(n), map.nodes[n].draws.size() == 1 ? -1 : static_cast<i32>(p)};
  u32 roots = 0;
  for (const auto &node : map.nodes) if (!node.parent.valid()) ++roots;

  struct Claim { EditorEntityId id; u32 node; i32 primitive; };
  std::map<EditorEntityId, std::vector<Claim>> byInstance;
  std::map<EditorEntityId, EditorEntityId> wrappers;
  std::vector<EditorEntityId> ids;
  document.collectSubtree(document.root(), ids);
  for (const auto id : ids) {
    const auto &entity = *document.find(id);
    const auto *render = meshRenderer(entity);
    // Só a representação anterior à Entrega 2 é adotada: um objeto por
    // primitiva. Um objeto sem vínculo com vários slots não tem prova de
    // qual instância é e fica como está.
    if (linkOf(entity) || !render || !render->asset.valid() || render->slotCount() > 1) continue;
    const auto owner = owners.find(render->asset);
    if (owner == owners.end()) continue;
    std::vector<Claim> chain;
    EditorEntityId current = id;
    if (owner->second.primitive >= 0) {
      chain.push_back({id, owner->second.node, owner->second.primitive});
      const auto *parent = document.find(entity.parent);
      if (!parent || parent->id == document.root() || linkOf(*parent) ||
          importEntityName(map.nodes[owner->second.node].name) != parent->name) { ++report.unproven; continue; }
      current = parent->id;
    }
    chain.push_back({current, owner->second.node, -1});
    u32 node = owner->second.node;
    bool proven = true;
    while (map.nodes[node].parent.valid()) {
      const auto parentIndex = map.indexOf(map.nodes[node].parent);
      const auto *parent = document.find(document.find(current)->parent);
      if (parentIndex < 0 || !parent || parent->id == document.root() || linkOf(*parent) ||
          importEntityName(map.nodes[static_cast<usize>(parentIndex)].name) != parent->name) { proven = false; break; }
      node = static_cast<u32>(parentIndex);
      current = parent->id;
      chain.push_back({current, node, -1});
    }
    if (!proven) { ++report.unproven; continue; }
    EditorEntityId key = current;
    if (roots > 1) {
      const auto *wrapper = document.find(document.find(current)->parent);
      if (wrapper && wrapper->id != document.root() && wrapper->kind == EditorEntityKind::Folder && !meshRenderer(*wrapper) &&
          !linkOf(*wrapper)) {
        key = wrapper->id;
        wrappers[key] = key;
      }
    }
    auto &claims = byInstance[key];
    claims.insert(claims.end(), chain.begin(), chain.end());
  }

  u32 adopted = 0;
  const auto revision = map.revision;
  bool opened = false;
  for (auto &[key, claims] : byInstance) {
    // Cada objeto um nó, cada nó um objeto. Qualquer coisa diferente disso é
    // exatamente a adivinhação que esta função não faz.
    std::sort(claims.begin(), claims.end(), [](const Claim &a, const Claim &b) { return a.id < b.id; });
    std::vector<Claim> unique;
    bool consistent = true;
    std::map<std::pair<u32, i32>, EditorEntityId> slots;
    for (const auto &claim : claims) {
      if (!unique.empty() && unique.back().id == claim.id) {
        if (unique.back().node != claim.node || unique.back().primitive != claim.primitive) consistent = false;
        continue;
      }
      const auto [it, inserted] = slots.try_emplace({claim.node, claim.primitive}, claim.id);
      if (!inserted && it->second != claim.id) consistent = false;
      unique.push_back(claim);
    }
    if (!consistent) {
      ++report.skippedInstances;
      note(report, "Objetos legados sem correspondência única com a fonte; vínculo não criado.");
      continue;
    }
    const auto instance = resources::assetGuidFromSeed("adotado:" + source.text() + ":" + std::to_string(key) + ":" +
                                                       std::to_string(document.revision()));
    if (history && !history->isOpen()) opened = history->begin("Vincular à fonte");
    const Mutator edit{document, history};
    const auto wrapper = wrappers.find(key);
    if (wrapper != wrappers.end()) {
      auto values = *document.find(key);
      auto *link = scene::editImportLink(values.components);
      if (!link) continue;
      link->source = source;
      link->instance = instance;
      link->root = true;
      link->node = {};
      link->revision = revision;
      link->baseName = values.name;
      edit.apply(key, values);
    }
    for (const auto &claim : unique) {
      auto values = *document.find(claim.id);
      auto *link = scene::editImportLink(values.components);
      if (!link) continue;
      const auto &node = map.nodes[claim.node];
      EditorTransform transform;
      if (!importNodeTransform(node, transform)) transform = values.transform;
      link->source = source;
      link->instance = instance;
      // Nó de várias primitivas na cena legada: o objeto do nó não tem malha e
      // as partes carregam as primitivas. A migração para slots vem depois.
      setImportLinkBase(*link, node, transform, claim.primitive, revision, node.draws.size() <= 1);
      if (claim.primitive >= 0) link->baseName = values.name;
      link->root = claim.id == key;
      if (edit.apply(claim.id, values)) ++adopted;
    }
  }
  if (opened) history->end();
  report.adopted += adopted;
  return adopted;
}

bool reconcileImportInstances(EditorDocument &document, EditorHistory *history, const AssetGuid &source,
                              const ImportNodeMap &map, ImportReconcileReport &report, const ImportSlotResolver &slotOf) {
  struct Instance {
    std::vector<EditorEntityId> members;
    EditorEntityId wrapper = 0, rootParent = 0;
    u32 revision = ~0u, newest = 0;
    bool conflict = false;
    std::unordered_map<AssetGuid, EditorEntityId, GuidHash> nodes;
    std::map<std::pair<AssetGuid, i32>, EditorEntityId> parts;
  };
  std::vector<EditorEntityId> order;
  document.collectSubtree(document.root(), order);
  std::unordered_map<EditorEntityId, usize> position;
  for (usize i = 0; i < order.size(); ++i) position[order[i]] = i;
  std::map<AssetGuid, Instance> instances;
  for (const auto id : order) {
    const auto &entity = *document.find(id);
    const auto *link = linkOf(entity);
    if (!link || link->source != source || link->orphan || link->unlinked) continue;
    auto &instance = instances[link->instance];
    instance.members.push_back(id);
    instance.revision = std::min(instance.revision, link->revision);
    instance.newest = std::max(instance.newest, link->revision);
    if (!link->node.valid()) {
      if (instance.wrapper) instance.conflict = true;
      instance.wrapper = id;
    } else if (link->primitive < 0) {
      if (!instance.nodes.try_emplace(link->node, id).second) instance.conflict = true;
      if (!link->baseParent.valid() && !instance.rootParent) instance.rootParent = entity.parent;
    } else if (!instance.parts.try_emplace({link->node, link->primitive}, id).second) {
      instance.conflict = true;
    }
  }

  bool opened = false;
  const Mutator edit{document, history};
  for (auto &[guid, instance] : instances) {
    // Instância em dia e sem partes legadas: nada a fazer.
    if (instance.revision == map.revision && instance.newest == map.revision && instance.parts.empty()) continue;
    ++report.instances;
    if (instance.conflict || instance.newest > map.revision) {
      // Dois objetos dizendo ser o mesmo nó da mesma instância, ou uma cena de
      // uma revisão que este mapa não conhece: nenhuma mutação é segura.
      ++report.skippedInstances;
      note(report, "Instância com vínculos inconsistentes; mantida sem reconciliar.");
      continue;
    }
    if (history && !history->isOpen()) opened = history->begin("Reconciliar importação");
    if (instance.wrapper) instance.rootParent = instance.wrapper;
    std::unordered_set<EditorEntityId> created;

    // 0. Migração das partes artificiais (um filho por primitiva) para slots
    // do objeto do nó. Só quando é PROVADO que nada se perde: todas as partes
    // presentes, pristinas (material local é preservado no slot) e o objeto do
    // nó sem malha própria. Qualquer dúvida mantém o legado, com diagnóstico.
    std::map<AssetGuid, std::vector<std::pair<i32, EditorEntityId>>> partsByNode;
    for (const auto &[key, id] : instance.parts) partsByNode[key.first].push_back({key.second, id});
    for (auto &[nodeId, parts] : partsByNode) {
      std::sort(parts.begin(), parts.end());
      const auto owner = instance.nodes.find(nodeId);
      const char *reason = nullptr;
      if (owner == instance.nodes.end() || !document.exists(owner->second)) reason = "o objeto do nó não existe";
      const auto *nodeEntity = reason ? nullptr : document.find(owner->second);
      if (!reason && meshRenderer(*nodeEntity)) reason = "o objeto do nó já tem malha própria";
      if (!reason && parts.size() != linkOf(*nodeEntity)->basePrimitives) reason = "faltam partes";
      for (usize i = 0; !reason && i < parts.size(); ++i) {
        const auto *part = document.find(parts[i].second);
        if (!part || parts[i].first != static_cast<i32>(i)) reason = "faltam partes";
        else if (part->parent != owner->second) reason = "uma parte saiu do nó";
        else if (carriesLocalData(document, parts[i].second, *linkOf(*part), true)) reason = "uma parte tem dados locais";
      }
      if (reason) {
        ++report.legacyParts;
        note(report, std::string(nodeEntity ? nodeEntity->name : "Nó") + ": partes por primitiva mantidas (" + reason + ").");
        continue;
      }
      auto values = *nodeEntity;
      auto *render = editMeshRenderer(values);
      auto *link = scene::editImportLink(values.components);
      if (!render || !link) continue;
      render->submeshes.resize(parts.size() - 1);
      std::vector<AssetGuid> base;
      for (usize i = 0; i < parts.size(); ++i) {
        const auto &part = *document.find(parts[i].second);
        const auto *partRender = meshRenderer(part);
        const auto slot = static_cast<u32>(i);
        if (partRender) {
          *render->editSlotAsset(slot) = partRender->asset;
          *render->editSlotMesh(slot) = partRender->mesh;
          *render->editSlotMaterial(slot) = partRender->material;
          *render->editSlotMaterialAsset(slot) = partRender->materialAsset;
          *render->editSlotTextures(slot) = partRender->textures;
          *render->editSlotSurface(slot) = partRender->surface;
          *render->editSlotSampling(slot) = partRender->sampling;
          *render->editSlotChannels(slot) = partRender->channels;
          *render->editSlotOcclusionTexture(slot) = partRender->occlusionTexture;
        }
        base.push_back(linkOf(part)->baseAsset);
      }
      link->baseAsset = base.front();
      link->baseSubmeshes.assign(base.begin() + 1, base.end());
      if (!edit.apply(owner->second, values)) continue;
      for (const auto &[primitive, id] : parts) {
        edit.destroy(id);
        instance.parts.erase({nodeId, primitive});
      }
      ++report.consolidated;
    }
    const auto partsMode = [&](const AssetGuid &node) {
      const auto first = instance.parts.lower_bound({node, std::numeric_limits<i32>::min()});
      return first != instance.parts.end() && first->first.first == node;
    };

    // A. Nós que a instância ainda não conhecia, já com os slots.
    for (const auto &node : map.nodes) {
      // Excluído pelo perfil: não entra na cena. Não é "apagado pelo autor",
      // então não conta em keptDeleted — é uma escolha de importação.
      if (node.excluded) continue;
      if (instance.nodes.count(node.id)) continue;
      if (node.introduced <= instance.revision) { ++report.keptDeleted; continue; }
      const EditorEntityId parent = node.parent.valid()
          ? (instance.nodes.count(node.parent) ? instance.nodes[node.parent] : 0)
          : instance.rootParent;
      if (!parent || !document.exists(parent)) {
        note(report, "Nó novo \"" + node.name + "\" sem pai na instância; não criado.");
        continue;
      }
      EditorTransform transform;
      if (!importNodeTransform(node, transform)) {
        ++report.conflicts;
        note(report, "Nó novo \"" + node.name + "\" com matriz fora de TRS; não criado.");
        continue;
      }
      const auto id = edit.create(parent, importEntityName(node.name));
      if (!id) { note(report, "Limite de objetos atingido ao criar \"" + node.name + "\"."); continue; }
      auto values = *document.find(id);
      values.transform = transform;
      setSlots(values, node.draws, slotOf);
      setImportedCamera(values, node);
      auto *link = scene::editImportLink(values.components);
      if (!link) continue;
      link->source = source;
      link->instance = guid;
      setImportLinkBase(*link, node, transform, -1, map.revision);
      edit.apply(id, values);
      instance.nodes[node.id] = id;
      created.insert(id);
      ++report.created;
    }

    // B. Campos, slots e pai dos nós que continuam na fonte.
    const auto conflict = [&](const EditorEntity &entity, const char *field) {
      ++report.conflicts;
      note(report, std::string(entity.name) + ": " + field + " mudou na fonte e no objeto; mantido o valor local.");
    };
    std::vector<std::pair<EditorEntityId, AssetGuid>> reparents;
    for (const auto &node : map.nodes) {
      // O excluído vai sair na etapa C; atualizar antes só produziria conflito
      // falso num objeto que está de saída.
      if (node.excluded) continue;
      const auto found = instance.nodes.find(node.id);
      if (found == instance.nodes.end() || created.count(found->second) || !document.exists(found->second)) continue;
      const auto id = found->second;
      auto values = *document.find(id);
      const auto previous = *linkOf(values);
      EditorTransform fresh;
      const bool trs = importNodeTransform(node, fresh);
      bool touched = false;
      const auto newName = importEntityName(node.name);
      if (newName != previous.baseName) {
        if (previous.baseName == values.name) { assignEntityName(values, newName); touched = true; }
        else if (newName != values.name) conflict(values, "o nome");
      }
      if (trs) {
        const auto three = [&](float local[3], const float base[3], const float incoming[3], const char *field) {
          if (sameVector(incoming, base)) return;
          if (sameVector(local, base)) { copyVector(local, incoming); touched = true; }
          else if (!sameVector(local, incoming)) conflict(values, field);
        };
        three(values.transform.position, previous.basePosition, fresh.position, "a posição");
        three(values.transform.rotationDegrees, previous.baseRotation, fresh.rotationDegrees, "a rotação");
        three(values.transform.scale, previous.baseScale, fresh.scale, "a escala");
      } else {
        conflict(values, "a matriz (fora de TRS)");
        copyVector(fresh.position, previous.basePosition);
        copyVector(fresh.rotationDegrees, previous.baseRotation);
        copyVector(fresh.scale, previous.baseScale);
      }
      // Slots: o vetor inteiro de identidades é o campo. Mudança de contagem
      // só é aplicada quando não descarta material que o usuário pôs num slot.
      const bool legacy = partsMode(node.id);
      std::vector<AssetGuid> incoming;
      if (!legacy) incoming = node.draws;
      else if (node.draws.size() == 1) incoming.push_back(node.draws.front());
      const auto base = previous.baseSlots();
      const auto local = slotAssets(values);
      if (incoming != base) {
        if (local == base) {
          bool dropsMaterial = false;
          if (const auto *render = meshRenderer(values))
            for (u32 slot = static_cast<u32>(incoming.size()); slot < render->slotCount(); ++slot)
              dropsMaterial |= slotCarriesMaterial(*render, slot);
          if (dropsMaterial) conflict(values, "os slots de material");
          else { setSlots(values, incoming, slotOf); touched = true; }
        } else if (local != incoming) {
          conflict(values, "a malha");
        }
      }
      auto *link = scene::editImportLink(values.components);
      if (!link) continue;
      const bool keepRoot = link->root;
      setImportLinkBase(*link, node, fresh, -1, map.revision, !legacy);
      link->root = keepRoot;
      if (!trs) link->baseName = newName;
      edit.apply(id, values);
      if (touched) ++report.updated;

      // Partes legadas que não puderam virar slots: só atualização da malha.
      if (legacy && node.draws.size() > 1)
        for (usize p = 0; p < node.draws.size(); ++p) {
          const auto part = instance.parts.find({node.id, static_cast<i32>(p)});
          if (part == instance.parts.end()) { ++report.keptDeleted; continue; }
          if (!document.exists(part->second)) continue;
          auto partValues = *document.find(part->second);
          const auto partPrevious = *linkOf(partValues);
          const auto *partRender = meshRenderer(partValues);
          const AssetGuid partLocal = partRender ? partRender->asset : AssetGuid{};
          if (node.draws[p] != partPrevious.baseAsset) {
            if (partLocal == partPrevious.baseAsset) { setSlots(partValues, {node.draws[p]}, slotOf); ++report.updated; }
            else if (partLocal != node.draws[p]) conflict(partValues, "a malha");
          }
          auto *partLink = scene::editImportLink(partValues.components);
          if (!partLink) continue;
          const auto name = partPrevious.baseName;
          setImportLinkBase(*partLink, node, fresh, static_cast<i32>(p), map.revision);
          partLink->baseName = name;
          edit.apply(part->second, partValues);
        }

      AssetGuid localParent;
      const auto where = localParentNode(document, *document.find(id), previous, localParent);
      const bool localIsBase = (where == 0 && !previous.baseParent.valid()) || (where == 1 && localParent == previous.baseParent);
      if (node.parent != previous.baseParent) {
        if (localIsBase) reparents.push_back({id, node.parent});
        else if (!(where == 1 && localParent == node.parent) && !(where == 0 && !node.parent.valid()))
          conflict(values, "o pai");
      }
    }
    for (const auto &[id, parentNode] : reparents) {
      const EditorEntityId target = parentNode.valid()
          ? (instance.nodes.count(parentNode) ? instance.nodes[parentNode] : 0)
          : instance.rootParent;
      if (!target || !document.exists(target) || target == id || document.isDescendantOf(target, id) || !edit.reparent(id, target)) {
        const auto *entity = document.find(id);
        ++report.conflicts;
        note(report, std::string(entity ? entity->name : "Objeto") + ": o pai novo não existe nesta instância; mantido.");
        continue;
      }
      ++report.updated;
    }

    // C. O que a fonte não tem mais, das folhas para a raiz.
    std::vector<EditorEntityId> gone;
    for (const auto id : instance.members) {
      if (!document.exists(id) || id == instance.wrapper) continue;
      const auto &link = *linkOf(*document.find(id));
      const auto *node = map.find(link.node);
      // Excluído pelo perfil sai pela MESMA regra de quem sumiu da fonte: sem
      // edição local, sai; com edição local, fica órfão. O nó continua no mapa,
      // e reincluir depois o traz de volta com a mesma identidade.
      const bool missing = !node || node->excluded || (link.primitive >= 0 &&
                                     (node->draws.size() <= 1 || static_cast<usize>(link.primitive) >= node->draws.size()));
      if (missing) gone.push_back(id);
    }
    std::sort(gone.begin(), gone.end(), [&](EditorEntityId a, EditorEntityId b) { return position[a] > position[b]; });
    for (const auto id : gone) {
      if (!document.exists(id)) continue;
      const auto link = *linkOf(*document.find(id));
      if (carriesLocalData(document, id, link)) {
        auto values = *document.find(id);
        if (auto *edited = scene::editImportLink(values.components)) edited->orphan = true;
        if (edit.apply(id, values)) {
          ++report.orphaned;
          note(report, std::string(values.name) + ": removido da fonte; mantido na cena como órfão com os dados locais.");
        }
      } else if (edit.destroy(id)) {
        ++report.removed;
      }
    }
    if (instance.wrapper && document.exists(instance.wrapper)) {
      auto values = *document.find(instance.wrapper);
      if (auto *link = scene::editImportLink(values.components)) link->revision = map.revision;
      edit.apply(instance.wrapper, values);
    }
  }
  if (opened) history->end();
  return true;
}

ImportSceneImpact importSceneImpact(const EditorDocument &document, const resources::AssetGuid &source,
                                    const resources::ImportMatchReport &match,
                                    const resources::ImportNodeMap *candidate) {
  ImportSceneImpact impact;
  std::unordered_set<AssetGuid, GuidHash> removed(match.removedNodes.begin(), match.removedNodes.end());
  std::unordered_set<AssetGuid, GuidHash> instances;
  std::vector<EditorEntityId> ids;
  document.collectSubtree(document.root(), ids);
  for (const auto id : ids) {
    const auto *entity = document.find(id);
    const auto *link = entity ? linkOf(*entity) : nullptr;
    if (!link || link->source != source) continue;
    if (link->instance.valid()) instances.insert(link->instance);
    if (link->unlinked) { ++impact.unlinked; continue; }
    if (link->orphan) { ++impact.alreadyOrphan; continue; }
    ++impact.linked;
    const u32 overrides = importOverrides(document, id);
    if (overrides) ++impact.editedObjects;
    const auto *record = candidate ? candidate->find(link->node) : nullptr;
    const bool excluded = record && record->excluded;
    if (!link->node.valid() || (!removed.count(link->node) && !excluded)) continue;
    if (excluded) ++impact.excludedObjects;
    // A mesma regra da reconciliação: o que não carrega nada do autor sai com a
    // fonte; o que carrega fica na cena, desligado, esperando decisão.
    if (overrides) {
      ++impact.orphanObjects;
      if (impact.orphanNames.size() < 6) impact.orphanNames.push_back(entity->name);
    } else {
      ++impact.removedObjects;
    }
  }
  impact.instances = static_cast<u32>(instances.size());
  impact.newNodes = match.added;
  return impact;
}

u32 importOverrides(const EditorDocument &document, EditorEntityId id) {
  const auto *entity = document.find(id);
  const auto *link = entity ? linkOf(*entity) : nullptr;
  if (!link || link->orphan || link->unlinked || !link->node.valid()) return 0;
  u32 mask = 0;
  if (link->baseName != entity->name && link->primitive < 0) mask |= ImportOverrideName;
  if (!sameVector(entity->transform.position, link->basePosition)) mask |= ImportOverridePosition;
  if (!sameVector(entity->transform.rotationDegrees, link->baseRotation)) mask |= ImportOverrideRotation;
  if (!sameVector(entity->transform.scale, link->baseScale)) mask |= ImportOverrideScale;
  if (link->primitive < 0) {
    AssetGuid parent;
    const auto where = localParentNode(document, *entity, *link, parent);
    if (where == 2 || (where == 0 && link->baseParent.valid()) || (where == 1 && parent != link->baseParent))
      mask |= ImportOverrideParent;
  }
  if (slotAssets(*entity) != link->baseSlots()) mask |= ImportOverrideMesh;
  // A fonte decide o material enquanto nenhum slot troca valores, material do
  // projeto ou textura; qualquer um desses é alteração local visível no vínculo.
  if (const auto *render = meshRenderer(*entity))
    for (u32 slot = 0; slot < render->slotCount(); ++slot)
      if (slotCarriesMaterial(*render, slot)) { mask |= ImportOverrideMaterial; break; }
  return mask;
}

bool revertImportOverrides(EditorDocument &document, EditorHistory &history, EditorEntityId id, u32 mask,
                           const ImportSlotResolver &slotOf) {
  const auto *entity = document.find(id);
  const auto *found = entity ? linkOf(*entity) : nullptr;
  if (!found || found->orphan || found->unlinked || history.isOpen()) return false;
  const auto link = *found;
  mask &= importOverrides(document, id);
  if (!mask) return false;
  auto values = *entity;
  if (mask & ImportOverrideName) assignEntityName(values, link.baseName);
  if (mask & ImportOverridePosition) copyVector(values.transform.position, link.basePosition);
  if (mask & ImportOverrideRotation) copyVector(values.transform.rotationDegrees, link.baseRotation);
  if (mask & ImportOverrideScale) copyVector(values.transform.scale, link.baseScale);
  if (mask & ImportOverrideMesh) setSlots(values, link.baseSlots(), slotOf);
  if (mask & ImportOverrideMaterial)
    if (auto *render = editMeshRenderer(values))
      for (u32 slot = 0; slot < render->slotCount(); ++slot) {
        render->editSlotMaterial(slot)->enabled = false;
        *render->editSlotMaterialAsset(slot) = {};
        *render->editSlotTextures(slot) = {};
        *render->editSlotSurface(slot) = {};
        *render->editSlotSampling(slot) = {};
        *render->editSlotChannels(slot) = {};
        *render->editSlotOcclusionTexture(slot) = {};
      }
  history.begin("Reverter à fonte");
  bool ok = history.applyValues(document, id, values);
  if (ok && (mask & ImportOverrideParent)) {
    // O pai da base, procurado na MESMA instância.
    EditorEntityId target = 0;
    std::vector<EditorEntityId> ids;
    document.collectSubtree(document.root(), ids);
    for (const auto candidate : ids) {
      const auto *other = linkOf(*document.find(candidate));
      if (!other || other->instance != link.instance || other->source != link.source || other->primitive >= 0 || other->orphan ||
          other->unlinked)
        continue;
      if (link.baseParent.valid() ? other->node == link.baseParent : (!other->node.valid() && other->root)) { target = candidate; break; }
    }
    if (target && target != id && !document.isDescendantOf(target, id))
      ok = history.reparent(document, id, target, static_cast<u32>(document.childrenOf(target).size()));
  }
  history.end();
  return ok;
}

u32 unlinkImportInstance(EditorDocument &document, EditorHistory &history, EditorEntityId member) {
  const auto *entity = document.find(member);
  const auto *found = entity ? linkOf(*entity) : nullptr;
  if (!found || found->unlinked || history.isOpen()) return 0;
  const auto instance = found->instance;
  const auto source = found->source;
  std::vector<EditorEntityId> ids;
  document.collectSubtree(document.root(), ids);
  u32 count = 0;
  history.begin("Desvincular instância");
  for (const auto id : ids) {
    const auto *link = linkOf(*document.find(id));
    if (!link || link->unlinked || link->instance != instance || link->source != source) continue;
    auto values = *document.find(id);
    if (auto *edited = scene::editImportLink(values.components)) edited->unlinked = true;
    if (history.applyValues(document, id, values)) ++count;
  }
  history.end();
  return count;
}

bool unlinkImportObject(EditorDocument &document, EditorHistory &history, EditorEntityId id) {
  const auto *entity = document.find(id);
  if (!entity || !linkOf(*entity) || linkOf(*entity)->unlinked || history.isOpen()) return false;
  auto values = *entity;
  if (auto *edited = scene::editImportLink(values.components)) edited->unlinked = true;
  return history.applyValues(document, id, values);
}
} // namespace ae::editor
