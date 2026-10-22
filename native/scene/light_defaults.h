#pragma once
#include "scene/light_units.h"

namespace ae::scene {
// A tabela ÚNICA de luzes padrão (L-05 da auditoria full Unity).
//
// Antes cada caminho de criação tinha o próprio número: "Luz direcional" nascia
// com 10 000 lux, "Sol com dia e noite" com 3 na unidade relativa e o modelo de
// referência com 100 000 lux. O mesmo comando de menu produzia cenas com 2,5 EV
// de diferença. Agora o menu, a receita do sol, o modelo e o valor padrão da
// classe leem daqui.
//
// Como a Unity separa os dois mundos:
//  - URP: intensidade sem unidade, sol = 1 e o resto da cena calibrado com
//    ele. É a unidade "Relativa" da Astra (LightUnit::Engine). O sol nasce com
//    3, o brilho que o céu procedural e a exposição 0 EV já esperam (a
//    referência do Ambiente é 2): é o sol que "Sol com dia e noite" já usava.
//  - HDRP: unidades físicas (lux, lúmen) com exposição calibrada. Luzes locais
//    nascem em lúmen, porque é o que vem impresso na caixa da lâmpada; o sol
//    físico de 100 000 lux só aparece numa cena que também traz o volume de
//    exposição correspondente (o modelo de referência, -6,8 EV).
//
// Ângulos do spot: 30° e 21,8°, os padrões do Light da Unity.
struct LightDefault {
  LightKind kind;
  LightUnit unit;
  float intensity;
  float range;
  float spotAngle;
  float innerSpotAngle;
  // Tipo de sombra: 0 nenhuma, 1 dura, 2 suave. Como no menu GameObject ›
  // Light da Unity, só o sol nasce projetando sombra.
  u32 shadowMode;
};

inline constexpr LightDefault kDirectionalLightDefault{LightKind::Directional, LightUnit::Engine, 3.0f, 10.0f, 30.0f, 21.8f, 2};
inline constexpr LightDefault kPointLightDefault{LightKind::Point, LightUnit::LuxLumen, 1000.0f, 10.0f, 30.0f, 21.8f, 0};
inline constexpr LightDefault kSpotLightDefault{LightKind::Spot, LightUnit::LuxLumen, 1000.0f, 10.0f, 30.0f, 21.8f, 0};
// Sol físico do meio-dia, só para cenas com exposição calibrada para ele.
inline constexpr float kPhysicalSunLux = 100000.0f;
// V-2: a Lua do "Sol com dia e noite", como a Moon do tutorial HDRP Night
// (Directional, 0,5 lux, Filter and Temperature frio, sombra ligada). Os
// quadros do tutorial mostram 8570 K e 20000 K; 12000 K deu, no POCO, o azul
// da noite dos vídeos (azul/vermelho ~1,6 com os postes acesos).
inline constexpr LightDefault kMoonLightDefault{LightKind::Directional, LightUnit::LuxLumen, 0.5f, 10.0f, 30.0f, 21.8f, 2};
inline constexpr float kMoonColorTemperature = 12000.0f;

inline constexpr const LightDefault &lightDefault(LightKind kind) noexcept {
  return kind == LightKind::Directional ? kDirectionalLightDefault
       : kind == LightKind::Spot ? kSpotLightDefault : kPointLightDefault;
}
} // namespace ae::scene
