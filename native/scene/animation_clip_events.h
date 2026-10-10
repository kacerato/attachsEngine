#pragma once
#include "scene/components.h"
namespace ae::scene {
inline constexpr std::array<ComponentParameter,3> clipCuePayload{{
  {"tag","Código",ComponentValueKind::Integer},
  {"value","Valor",ComponentValueKind::Number},
  {"cue","ID no clipe",ComponentValueKind::Integer}}};
inline constexpr std::array<ComponentParameter,1> clipCueLossPayload{{
  {"count","Eventos suprimidos",ComponentValueKind::Integer}}};
inline constexpr ComponentEvent clipCueEvent{"clip_event","Evento do clipe","Código, valor e identidade local do evento; somente travessia em Play",clipCuePayload};
inline constexpr ComponentEvent clipCueLossEvent{"clip_events_lost","Limite de eventos","Travessia excedeu o orçamento; não são repetidos no próximo quadro",clipCueLossPayload};
inline constexpr std::array<ComponentEvent,2> animationClipEvents{{clipCueEvent,clipCueLossEvent}};
}
