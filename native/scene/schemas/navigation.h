// Família Navegação: malha assada, agentes, obstáculos, links e modificadores.
// Incluído somente por scene/component_schema.h.
#pragma once
#include "scene/navigation.h"

namespace ae::scene {
inline constexpr std::array<ComponentRule,2> navAgentConflicts{{
  {"astra.path.follow","Agente e Seguir caminho escreveriam a mesma pose"},
  {"astra.navigation.obstacle","Um objeto é agente ou obstáculo, não os dois"}
}};
inline constexpr std::array<ComponentRule,1> navObstacleConflicts{{
  {"astra.navigation.agent","Um objeto é agente ou obstáculo, não os dois"}
}};
#include "scene/generated/navigation_schemas.inc"
} // namespace ae::scene
