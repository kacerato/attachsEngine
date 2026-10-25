// Família Áudio: fontes, escuta e mixer. Incluído somente por scene/component_schema.h.
#pragma once
#include "scene/audio.h"
#include "scene/audio_mixer.h"
#include "scene/vfx_behaviors.h"
namespace ae::scene {
// O som repetido toca o clipe da Fonte de áudio do próprio objeto.
inline constexpr std::array<ComponentRule,1> effectSoundRequirements{{
  {"astra.audio.source","Adicione uma Fonte de áudio a este objeto: o som repetido toca o clipe dela"}
}};
// Efeitos e envios formam a cadeia do bus: só existem no objeto de um Bus de áudio.
inline constexpr std::array<ComponentRule,1> audioEffectRequirements{{
  {"astra.audio.bus","Adicione Bus de áudio a este objeto: efeitos e envios processam o sinal do bus"}
}};
#include "scene/generated/audio_schemas.inc"
}
