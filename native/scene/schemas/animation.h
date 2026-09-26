// Família Animação. Incluído somente por scene/component_schema.h.
#pragma once
#include "scene/animation.h"

namespace ae::scene {
inline constexpr std::array<ComponentSchema, 1> animationSchemas{{
  {.type=&Animation::descriptor, .name="Animação", .description="Clipes tocados e misturados no Play",
   .family=ComponentFamily::Animation, .structuralInPlay=PlayMutability::SafePoint,
   .consumer="runtime/scene_animation.cpp → pose local dos nós da instância", .capability="animation.clip",
   .invalidates=Invalidate::Transform,
   .subfamily="Clipes", .icon="assets/animation", .searchTerms="Animation AnimationPlayer Clip",
   .reference="https://docs.unity3d.com/6000.0/Documentation/Manual/class-Animation.html"}
}};
} // namespace ae::scene
