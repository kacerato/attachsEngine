#pragma once
#include "scene/physics2d_components.h"
#include "scene/physics2d_event_connection.h"
#include "scene/physics_field2d.h"
namespace ae::scene {
inline constexpr std::array<ComponentRule,3> physics2dConflicts{{
 {"astra.physics.body","Corpos 2D e 3D não compartilham o mesmo objeto"},
 {"astra.physics.character","Personagem 3D possui a pose deste objeto"},
 {"astra.physics.collider","Colisores 2D e 3D usam mundos separados"}
}};
inline constexpr std::array<ComponentRule,1> body2dRequirements{{{"astra.physics2d.collider","Adicione uma forma 2D ao corpo"}}};
inline constexpr std::array<ComponentRule,1> body2dRequired{{{"astra.physics2d.body","Adicione Body2D ao objeto"}}};
#include "scene/generated/physics2d_schemas.inc"
}
