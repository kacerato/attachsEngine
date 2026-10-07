// Família Áudio: fontes, escuta e mixer. Incluído somente por scene/component_schema.h.
#pragma once
#include "scene/audio.h"
#include "scene/audio_mixer.h"
namespace ae::scene {
// Efeitos e envios formam a cadeia do bus: só existem no objeto de um Bus de áudio.
inline constexpr std::array<ComponentRule,1> audioEffectRequirements{{
  {"astra.audio.bus","Adicione Bus de áudio a este objeto: efeitos e envios processam o sinal do bus"}
}};
#include "scene/generated/audio_schemas.inc"
}
