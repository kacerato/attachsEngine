// Família Luz. Incluído somente por scene/component_schema.h.
#pragma once
#include "scene/light.h"
#include "scene/time_of_day.h"
#include "scene/lighting_settings.h"

namespace ae::scene {
// A hora do dia move o sol: só existe no objeto de uma Luz (a direcional).
inline constexpr std::array<ComponentRule,1> timeOfDayRequirements{{
  {"astra.render.light","Adicione uma Luz direcional a este objeto: a hora do dia move o sol"}
}};
#include "scene/generated/lighting_schemas.inc"
} // namespace ae::scene
