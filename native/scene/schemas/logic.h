// Família Lógica: tempo e código de gameplay. Incluído somente por
// scene/component_schema.h, que define ComponentSchema antes das famílias.
#pragma once
#include "scene/script_behavior.h"
#include "scene/timer.h"

namespace ae::scene {
inline constexpr std::array<ComponentSchema, 2> logicSchemas{{
  {.type=&Timer::descriptor, .name="Timer", .description="Dispara eventos temporizados para comportamentos",
   .family=ComponentFamily::Logic, .structuralInPlay=PlayMutability::SafePoint,
   .consumer="runtime/scene_timers.h → ScriptBridge → Behavior.TimerElapsed",
   .subfamily="Tempo", .icon="component/timer", .searchTerms="Timer Countdown Interval Cronometro",
   .reference="https://docs.godotengine.org/en/4.5/classes/class_timer.html",
   .apiName="GameTimer"},
  {.type=&ScriptBehavior::descriptor, .name="Comportamento", .description="Código C# do projeto",
   .family=ComponentFamily::Logic, .propertiesInPlay=PlayMutability::Never,
   .consumer="runtime/script_bridge.cpp → runtime .NET", .invalidates=Invalidate::Script,
   .subfamily="Código", .icon="scripting/code", .searchTerms="MonoBehaviour Script Behaviour",
   .reference="https://docs.unity3d.com/6000.0/Documentation/ScriptReference/MonoBehaviour.html",
   .listedInAdd=false}
}};
} // namespace ae::scene
