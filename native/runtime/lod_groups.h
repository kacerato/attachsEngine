#pragma once
// Seleção dos LOD Groups para uma vista: o que some e o que cruza.
//
// Roda na CPU, uma vez por vista, como o LODGroup da Unity: é barata (um
// componente por grupo, não um por desenho) e o resultado só muda quando a
// câmera cruza uma faixa ou uma animação de troca está correndo. Quem desenha
// usa o resultado para marcar desenhos invisíveis e para escrever a cobertura
// do cross-fade — a topologia não muda, então a publicação é a de poses.
#include "runtime/scene_components.h"
#include "runtime/transform_math.h"
#include "scene/lod_group.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
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

// Estado de EXECUÇÃO do Animate Cross-fading: por grupo, o nível que estava na
// tela, o que está entrando e quando a troca começou. Nunca é gravado.
struct LodCrossFadeClock {
  struct Transition { u32 from = 0, to = 0; double start = 0; bool known = false; };
  std::unordered_map<ObjectId, Transition> groups;
};

// O que a vista faz com um objeto de nível. `hidden` some; senão `dither` é a
// cobertura com sinal do contrato do renderer (renderer/lod_selection.h e o
// shader lod_dither.glsl): positivo no nível que sai, negativo no que entra,
// complementares; zero é o objeto inteiro.
struct LodObjectState {
  ObjectId object = kInvalidObject;
  bool hidden = false;
  float dither = 0;
  bool operator==(const LodObjectState &o) const {
    return object == o.object && hidden == o.hidden && dither == o.dither;
  }
};

// Estados ordenados por objeto; só aparecem os objetos que algum grupo afeta.
// Um nível sem objeto, com referência inválida (fora da hierarquia do grupo)
// ou um grupo inativo não escondem nada. Um objeto comum a dois níveis fica
// com o estado mais visível entre eles. `clock` é opcional: sem ele, a troca
// animada é direta.
inline std::vector<LodObjectState> lodObjectStates(const SceneGraph &graph, const LodView &view,
                                                   LodCrossFadeClock *clock = nullptr, double now = 0) {
  std::vector<ObjectId> ids;
  graph.collectSubtree(graph.root(), ids);
  std::unordered_map<ObjectId, LodObjectState> states;
  std::unordered_set<ObjectId> alive;
  // Mais visível vence: inteiro > com cobertura > escondido.
  const auto merge = [&](ObjectId member, bool hidden, float dither) {
    auto [it, inserted] = states.try_emplace(member, LodObjectState{member, hidden, hidden ? 0 : dither});
    if (inserted) return;
    auto &current = it->second;
    if (hidden) return;
    if (current.hidden || dither == 0 || (current.dither != 0 && std::abs(dither) < std::abs(current.dither))) {
      current.hidden = false;
      current.dither = dither;
    }
  };
  for (const auto id : ids) {
    const auto *object = graph.find(id);
    if (!object || !graph.activeInHierarchy(id)) continue;
    const auto *group = static_cast<const scene::LodGroup *>(object->components.find(scene::LodGroup::descriptor));
    float relative = 0;
    u32 chosen = 0;
    if (!group || !lodGroupViewLevel(graph, id, view, relative, chosen)) continue;
    alive.insert(id);
    // Par que está na tela: `shown` inteiro ou saindo, `next` entrando.
    u32 shown = chosen, next = group->levelCount;
    float factor = 0;
    if (group->fadeMode == scene::LodFadeMode::CrossFade && group->animateCrossFading && !group->forcedLevel && clock) {
      auto &transition = clock->groups[id];
      // Primeira vista do grupo: já parado no nível escolhido, sem animar a entrada.
      if (!transition.known) transition = {chosen, chosen, now - scene::LodCrossFadeAnimationSeconds, true};
      if (transition.to != chosen) {
        // Troca no meio de outra: parte do que está mais visível agora.
        const float progress = static_cast<float>((now - transition.start) / scene::LodCrossFadeAnimationSeconds);
        transition = {progress < .5f ? transition.from : transition.to, chosen, now, true};
      }
      factor = std::clamp(static_cast<float>((now - transition.start) / scene::LodCrossFadeAnimationSeconds), 0.0f, 1.0f);
      if (factor < 1 && transition.from != transition.to) {
        shown = transition.from;
        next = transition.to;
      } else {
        factor = 0;
      }
    } else {
      if (clock) clock->groups.erase(id);
      factor = scene::lodGroupFadeFactor(*group, chosen, relative);
      if (factor > 0) next = chosen + 1; // `levelCount` quando o último some aos poucos
    }
    for (u32 level = 0; level < group->levelCount; ++level) {
      const auto root = group->levels[level];
      if (!root || !referenceAccepts(graph, id, scene::lodGroupReferences[level], root, true)) continue;
      std::vector<ObjectId> subtree;
      graph.collectSubtree(static_cast<ObjectId>(root), subtree);
      const bool isShown = level == shown && shown < group->levelCount;
      const bool isNext = factor > 0 && level == next;
      // O que sai ainda aparece em (1 - factor); o que entra em factor.
      const float dither = isShown ? (factor > 0 ? factor : 0) : isNext ? -factor : 0;
      for (const auto member : subtree) merge(member, !(isShown || isNext), dither);
    }
  }
  if (clock)
    for (auto it = clock->groups.begin(); it != clock->groups.end();)
      it = alive.count(it->first) ? std::next(it) : clock->groups.erase(it);
  std::vector<LodObjectState> result;
  result.reserve(states.size());
  for (const auto &[object, state] : states) result.push_back(state);
  std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) { return a.object < b.object; });
  return result;
}

// Verdadeiro enquanto alguma troca animada ainda está correndo: a tela precisa
// ser republicada a cada quadro até ela terminar.
inline bool lodCrossFadeRunning(const LodCrossFadeClock &clock, double now) {
  for (const auto &[id, transition] : clock.groups)
    if (transition.from != transition.to && now - transition.start < scene::LodCrossFadeAnimationSeconds) return true;
  return false;
}

// Objetos escondidos, ordenados — a seleção sem cross-fade.
inline std::vector<ObjectId> lodHiddenObjects(const SceneGraph &graph, const LodView &view) {
  std::vector<ObjectId> hidden;
  for (const auto &state : lodObjectStates(graph, view))
    if (state.hidden) hidden.push_back(state.object);
  return hidden;
}

} // namespace ae::runtime
