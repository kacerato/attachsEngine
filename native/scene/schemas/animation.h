// Família Animação. Incluído somente por scene/component_schema.h.
#pragma once
#include "scene/animation.h"
#include "scene/animator.h"

namespace ae::scene {
// Animação legada e Animator escreveriam a mesma pose: um ou outro por objeto.
inline constexpr std::array<ComponentRule,1> animationConflicts{{
  {"astra.animation.animator","Animator e Animação escreveriam a mesma pose; use um dos dois neste objeto"}
}};
inline constexpr std::array<ComponentRule,1> animatorConflicts{{
  {"astra.animation","Animator e Animação escreveriam a mesma pose; use um dos dois neste objeto"}
}};
#include "scene/generated/animation_schemas.inc"
} // namespace ae::scene
