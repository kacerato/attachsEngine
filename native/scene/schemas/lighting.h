// Família Luz. Incluído somente por scene/component_schema.h.
#pragma once
#include "scene/light.h"

namespace ae::scene {
inline constexpr std::array<ComponentSchema, 1> lightingSchemas{{
  {.type=&Light::descriptor, .name="Luz", .description="Direcional, pontual ou spot",
   .family=ComponentFamily::Lighting, .structuralInPlay=PlayMutability::SafePoint,
   .consumer="runtime/scene_lights.cpp → renderer/punctual_lights.h", .invalidates=Invalidate::LightCluster,
   .subfamily="Luzes", .icon="lighting/sun",
   .searchTerms="Light DirectionalLight3D OmniLight3D SpotLight3D Point Spot Sol",
   .reference="https://docs.unity3d.com/6000.0/Documentation/Manual/class-Light.html",
   .apiName="Light"}
}};
} // namespace ae::scene
