// Família Câmera: projeção e comportamento de enquadramento. Incluído somente
// por scene/component_schema.h.
#pragma once
#include "scene/camera.h"
#include "scene/camera_follow.h"
#include "scene/camera_look.h"

namespace ae::scene {
inline constexpr std::array<ComponentRule, 1> lookRequirements{{
  {"astra.camera", "Adicione Câmera a este objeto"}
}};
inline constexpr std::array<ComponentRule,1> followRequirements{{
  {"astra.camera","Adicione Câmera a este objeto"}
}};
inline constexpr std::array<ComponentRule,2> followConflicts{{
  {"astra.physics.body","A câmera seguidora não pode receber pose do corpo físico"},
  {"astra.physics.character","A câmera seguidora não pode receber pose do personagem"}
}};
#include "scene/generated/camera_schemas.inc"
} // namespace ae::scene
