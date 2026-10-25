// Família Efeitos: partículas (G5) e comportamentos de efeito (G6a). Incluído
// somente por scene/component_schema.h.
#pragma once
#include "scene/particle_system.h"
#include "scene/vfx_behaviors.h"

namespace ae::scene {
// O laser desenha pela Linha do próprio objeto; o Instanciar ao colidir ouve
// os toques do Sistema de partículas do próprio objeto.
inline constexpr std::array<ComponentRule, 1> laserRequirements{{
  {"astra.render.line", "Adicione uma Linha a este objeto: o laser desenha por ela"}
}};
inline constexpr std::array<ComponentRule, 1> collisionSpawnRequirements{{
  {"astra.effects.particle_system", "Adicione um Sistema de partículas com Colisão a este objeto"}
}};
#include "scene/generated/effects_schemas.inc"
} // namespace ae::scene
