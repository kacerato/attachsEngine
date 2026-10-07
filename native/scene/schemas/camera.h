// Família Câmera: projeção e comportamento de enquadramento. Incluído somente
// por scene/component_schema.h.
#pragma once
#include "scene/camera.h"
#include "scene/camera_follow.h"
#include "scene/camera_look.h"
#include "scene/virtual_camera.h"

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
// O Cérebro escreve pose e lente da própria câmera: nenhum outro controlador
// de pose pode morar no mesmo objeto.
inline constexpr std::array<ComponentRule,1> brainRequirements{{
  {"astra.camera","Adicione Câmera a este objeto"}
}};
inline constexpr std::array<ComponentRule,5> brainConflicts{{
  {"astra.camera.follow","O Cérebro e Acompanhar alvo escreveriam a mesma pose"},
  {"astra.camera.look","O Cérebro e Olhar escreveriam a mesma rotação"},
  {"astra.camera.virtual","Câmera virtual e Cérebro ficam em objetos separados"},
  {"astra.physics.body","A câmera do Cérebro não pode receber pose do corpo físico"},
  {"astra.physics.character","A câmera do Cérebro não pode receber pose do personagem"}
}};
inline constexpr std::array<ComponentRule,2> virtualCameraConflicts{{
  {"astra.physics.body","A câmera virtual não pode receber pose do corpo físico"},
  {"astra.physics.character","A câmera virtual não pode receber pose do personagem"}
}};
#include "scene/generated/camera_schemas.inc"
} // namespace ae::scene
