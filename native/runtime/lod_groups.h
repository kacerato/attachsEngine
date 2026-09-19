#pragma once
// Seleção dos LOD Groups para uma vista: quais objetos ficam escondidos.
//
// Roda na CPU, uma vez por vista, como o LODGroup da Unity: é barata (um
// componente por grupo, não um por desenho) e o resultado só muda quando a
// câmera cruza uma transição. Quem desenha usa a lista para marcar os desenhos
// desses objetos como invisíveis — a topologia não muda, então a publicação é
// a de poses, não a da cena inteira.
#include "runtime/scene_components.h"
#include "runtime/transform_math.h"
#include "scene/lod_group.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <vector>

namespace ae::runtime {

struct LodView {
  float position[3]{};
  float verticalFov = 1.0471976f;
  // Positivo: projeção ortográfica com esta meia altura.
  float orthographicHalfHeight = 0;
};

// Altura relativa do grupo `id` nesta vista e o nível que ela escolhe
// (`levelCount` = Culled). Falso quando o objeto não tem um LOD Group válido.
// É a mesma conta da seleção e da linha "Na vista" do Inspector.
inline bool lodGroupViewLevel(const SceneGraph &graph, ObjectId id, const LodView &view, float &relative, u32 &level) {
  const auto *object = graph.find(id);
  const auto *group = object ? static_cast<const scene::LodGroup *>(object->components.find(scene::LodGroup::descriptor)) : nullptr;
  float world[16];
  if (!group || !group->valid() || !worldMatrix(graph, id, world)) return false;
  float scale = 0;
  for (u32 column = 0; column < 3; ++column)
    scale = std::max(scale, std::sqrt(world[column * 4] * world[column * 4] + world[column * 4 + 1] * world[column * 4 + 1] +
                                      world[column * 4 + 2] * world[column * 4 + 2]));
  const float dx = world[12] - view.position[0], dy = world[13] - view.position[1], dz = world[14] - view.position[2];
  relative = scene::lodRelativeHeight(group->size * scale, std::sqrt(dx * dx + dy * dy + dz * dz), view.verticalFov,
                                      view.orthographicHalfHeight);
  level = scene::selectLodGroupLevel(*group, relative);
  return true;
}

// Objetos escondidos, ordenados e sem repetição. Um nível sem objeto, com
// referência inválida (fora da hierarquia do grupo) ou um grupo inativo não
// escondem nada: o conteúdo continua visível em vez de sumir por engano.
inline std::vector<ObjectId> lodHiddenObjects(const SceneGraph &graph, const LodView &view) {
  std::vector<ObjectId> ids;
  graph.collectSubtree(graph.root(), ids);
  std::unordered_set<ObjectId> hidden;
  for (const auto id : ids) {
    const auto *object = graph.find(id);
    if (!object || !graph.activeInHierarchy(id)) continue;
    const auto *group = static_cast<const scene::LodGroup *>(object->components.find(scene::LodGroup::descriptor));
    float relative = 0;
    u32 chosen = 0;
    if (!group || !lodGroupViewLevel(graph, id, view, relative, chosen)) continue;
    std::unordered_set<ObjectId> keep;
    std::vector<ObjectId> drop;
    for (u32 level = 0; level < group->levelCount; ++level) {
      const auto root = group->levels[level];
      if (!root || !referenceAccepts(graph, id, scene::lodGroupReferences[level], root, true)) continue;
      std::vector<ObjectId> subtree;
      graph.collectSubtree(static_cast<ObjectId>(root), subtree);
      if (level == chosen) keep.insert(subtree.begin(), subtree.end());
      else drop.insert(drop.end(), subtree.begin(), subtree.end());
    }
    for (const auto member : drop) if (!keep.count(member)) hidden.insert(member);
  }
  std::vector<ObjectId> result(hidden.begin(), hidden.end());
  std::sort(result.begin(), result.end());
  return result;
}

} // namespace ae::runtime
