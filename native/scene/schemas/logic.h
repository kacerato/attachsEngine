// Família Lógica: tempo e código de gameplay. Incluído somente por
// scene/component_schema.h, que define ComponentSchema antes das famílias.
#pragma once
#include "scene/script_behavior.h"
#include "scene/timer.h"
#include "scene/event_connection.h"
#include "scene/transform_constraints.h"
#include "scene/transform_tween.h"
#include "scene/tween_sequence.h"
#include "scene/spring_constraint.h"

namespace ae::scene {
inline constexpr std::array<ComponentRule,2> springPositionConflicts{{{"astra.constraint.position","Escolha restrição direta ou mola para posição"},{"astra.constraint.parent","Parent Constraint já escreve posição"}}};
inline constexpr std::array<ComponentRule,4> springRotationConflicts{{{"astra.constraint.rotation","Escolha restrição direta ou mola para rotação"},{"astra.constraint.aim","Aim já escreve rotação"},{"astra.constraint.parent","Parent Constraint já escreve rotação"},{"astra.constraint.look_at","Look At já escreve rotação"}}};
inline constexpr std::array<ComponentRule,1> springScaleConflicts{{{"astra.constraint.scale","Escolha restrição direta ou mola para escala"}}};
#include "scene/generated/logic_schemas.inc"
} // namespace ae::scene
