#pragma once
#include "scene/components.h"
#include <array>
namespace ae::scene {
// Input ownership is distinct from physical pose authority.
enum class MotorControlSource:u32 {None=0,Ui=1,Keyboard=2,Gamepad=3,Script=4,Ai=5};
inline constexpr std::array<ComponentEnumOption,6> motorControlOptions{{
  {0,"Automático"},{1,"UI"},{2,"Teclado / mouse"},{3,"Gamepad"},{4,"Script"},{5,"IA"}}};
inline const char *motorControlName(MotorControlSource s) {
  const auto n=u32(s);return n<motorControlOptions.size()?motorControlOptions[n].name:"Inválido";
}
struct MotorControlPolicy {
  MotorControlSource source=MotorControlSource::None;
  float uiPriority=10,keyboardPriority=10,gamepadPriority=10,scriptPriority=20,aiPriority=5;
  float priority(MotorControlSource s) const {
    switch(s){case MotorControlSource::Ui:return uiPriority;case MotorControlSource::Keyboard:return keyboardPriority;
    case MotorControlSource::Gamepad:return gamepadPriority;case MotorControlSource::Script:return scriptPriority;
    case MotorControlSource::Ai:return aiPriority;default:return -1;}
  }
};
inline bool motorControlProperty(std::string_view id){return id=="control_source"||id.starts_with("control_priority_");}
template<class T> bool motorControlAutomatic(const ComponentValue &v){return static_cast<const T&>(v).control.source==MotorControlSource::None;}
}
