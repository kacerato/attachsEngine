#pragma once
#include "editor/editor_map_scene.h"
#include "scene/lod_group.h"

#include "editor/editor_history.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string_view>

namespace ae::editor {
// "Recalculate Bounds" do LODGroup da Unity: o tamanho do grupo é a maior
// extensão da caixa, em mundo, de todos os triângulos do nível mais detalhado
// (LOD 0, ou o próprio objeto do grupo quando o nível ainda não tem objeto),
// dividida pela maior escala global do grupo — a seleção multiplica de volta.
// Falso quando não há geometria: um tamanho inventado escolheria níveis errados.
inline bool fitLodGroupSize(const EditorMapScene &resources, const EditorDocument &document, EditorEntityId id,
                            scene::LodGroup &group) {
  const auto root = group.levels[0] ? static_cast<EditorEntityId>(group.levels[0]) : id;
  if (!document.find(root)) return false;
  std::vector<EditorEntityId> members;
  document.collectSubtree(root, members);
  float low[3]{1e30f, 1e30f, 1e30f}, high[3]{-1e30f, -1e30f, -1e30f};
  bool any = false;
  for (const auto member : members) {
    const auto *entity = document.find(member);
    const auto *render = entity ? meshRenderer(*entity) : nullptr;
    float world[16];
    if (!render || !editorWorldMatrix(document, member, world)) continue;
    for (u32 slot = 0; slot < render->slotCount(); ++slot) {
      std::span<const EditorPickMesh::Triangle> triangles;
      float relative[16];
      if (!resources.localGeometry(render->slotMesh(slot), triangles, relative)) continue;
      for (const auto &triangle : triangles)
        for (u32 v = 0; v < 3; ++v) {
          float local[3], point[3];
          for (u32 k = 0; k < 3; ++k)
            local[k] = relative[12 + k] + relative[k] * triangle[v * 3] + relative[4 + k] * triangle[v * 3 + 1] +
                       relative[8 + k] * triangle[v * 3 + 2];
          for (u32 k = 0; k < 3; ++k)
            point[k] = world[12 + k] + world[k] * local[0] + world[4 + k] * local[1] + world[8 + k] * local[2];
          for (u32 k = 0; k < 3; ++k) {
            low[k] = std::min(low[k], point[k]);
            high[k] = std::max(high[k], point[k]);
          }
          any = true;
        }
    }
  }
  float groupWorld[16];
  if (!any || !editorWorldMatrix(document, id, groupWorld)) return false;
  float scale = 0;
  for (u32 column = 0; column < 3; ++column)
    scale = std::max(scale, std::sqrt(groupWorld[column * 4] * groupWorld[column * 4] +
                                      groupWorld[column * 4 + 1] * groupWorld[column * 4 + 1] +
                                      groupWorld[column * 4 + 2] * groupWorld[column * 4 + 2]));
  const float extent = std::max({high[0] - low[0], high[1] - low[1], high[2] - low[2]});
  if (!(scale > 0) || !std::isfinite(extent) || extent <= 0) return false;
  group.size = std::clamp(extent / scale, .001f, 1000000.0f);
  return true;
}
// Índice do sufixo `_LOD<n>` no fim do nome (sem diferenciar maiúsculas), ou -1.
// É a convenção do Model Importer da Unity: "Porta_LOD0", "Porta_LOD1"...
inline int importLodSuffix(std::string_view name) {
  usize digits = 0;
  while (digits < name.size() && std::isdigit(static_cast<unsigned char>(name[name.size() - 1 - digits]))) ++digits;
  if (!digits || digits > 2 || name.size() < digits + 4) return -1;
  const auto tag = name.substr(name.size() - digits - 4, 4);
  if (tag[0] != '_' || std::toupper(static_cast<unsigned char>(tag[1])) != 'L' ||
      std::toupper(static_cast<unsigned char>(tag[2])) != 'O' || std::toupper(static_cast<unsigned char>(tag[3])) != 'D')
    return -1;
  int value = 0;
  for (usize i = name.size() - digits; i < name.size(); ++i) value = value * 10 + (name[i] - '0');
  return value;
}

// Como o Model Importer da Unity: filhos diretos com `_LOD0`, `_LOD1`...
// ganham um LOD Group no pai, com cada filho no seu nível e o tamanho medido
// pelo LOD 0. Exige o LOD 0 e níveis contíguos a partir dele; um nível além do
// quarto, ou um buraco na sequência, deixa o grupo com os níveis até ali e é
// dito em `notes`. Pai que já tem LOD Group não é tocado. Roda dentro da
// transação de quem chama. Devolve quantos grupos criou.
inline u32 addImportedLodGroups(EditorDocument &document, EditorHistory &history, const EditorMapScene &resources,
                                std::span<const EditorEntityId> created, std::vector<std::string> &notes) {
  u32 groups = 0;
  for (const auto parentId : created) {
    const auto *parent = document.find(parentId);
    if (!parent || parent->components.find(scene::LodGroup::descriptor)) continue;
    std::array<EditorEntityId, scene::LodGroupMaximumLevels> levels{};
    bool beyond = false;
    for (const auto childId : created) {
      const auto *child = document.find(childId);
      if (!child || child->parent != parentId) continue;
      const int level = importLodSuffix(child->name);
      if (level < 0) continue;
      if (level >= static_cast<int>(scene::LodGroupMaximumLevels)) { beyond = true; continue; }
      if (!levels[static_cast<usize>(level)]) levels[static_cast<usize>(level)] = childId;
    }
    u32 count = 0;
    while (count < scene::LodGroupMaximumLevels && levels[count]) ++count;
    if (count < 2) continue;
    if (beyond || (count < scene::LodGroupMaximumLevels && std::any_of(levels.begin() + count, levels.end(), [](auto id) { return id != 0; })))
      notes.push_back(std::string(parent->name) + ": LOD Group com os níveis 0 a " + std::to_string(count - 1) +
                      "; níveis fora da sequência ou além do quarto ficaram de fora.");
    auto value = *parent;
    auto *group = static_cast<scene::LodGroup *>(value.components.add(scene::LodGroup::descriptor));
    if (!group) continue;
    group->levelCount = count;
    for (u32 i = 0; i < count; ++i) group->levels[i] = levels[i];
    if (!fitLodGroupSize(resources, document, parentId, *group))
      notes.push_back(std::string(parent->name) + ": LOD 0 sem malha; use Recalcular tamanho no LOD Group.");
    if (group->valid() && history.applyValues(document, parentId, value)) ++groups;
  }
  return groups;
}
} // namespace ae::editor
