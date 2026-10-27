#pragma once

// V-2: luz do céu à noite, no modelo dos ciclos de dia do HDRP (Physically
// Based Sky + Directional "Moon"). Matemática pura, testável no host.
//
// De dia o céu ilumina a cena pelo ambiente e pelo reflexo. Depois do
// crepúsculo náutico o céu sem sol quase não ilumina: o que sobra é o céu
// espalhando a luz da lua (uma fração da direcional Lua, em tom frio, como o
// azul que os jogos usam para a noite) e um piso mínimo de luz das estrelas.
// O reflexo do ambiente cai na mesma proporção -- o mapa de reflexo é do dia,
// e a sonda do HDRP é rerenderizada com o céu escuro.

#include "core/base.h"

#include <algorithm>
#include <cmath>

namespace ae::renderer {

// Fração da irradiância direta da lua que o céu devolve como luz difusa num
// plano horizontal. O céu diurno devolve ~15-25% do sol; o luar espalha igual.
inline constexpr float NightMoonSkyFraction = 0.2f;
// Piso de luz das estrelas e do brilho do ar, em fração do ambiente que o céu
// tinha no fim do crepúsculo: noite de lua nova fica quase preta, não zero.
inline constexpr float NightStarlightFraction = 0.003f;
// Piso absoluto do brilho do ar e das estrelas (lux no plano horizontal): ~1/20
// do céu sob a Lua padrão de 0,5 lux. O ambiente da meia-noite já chega quase
// zerado, então só a fração acima deixava a lua nova 11 EV abaixo da cheia: a
// exposição ia ao piso, as nuvens sumiam no preto e as luminárias estouravam.
// Com ele as nuvens ficam escuras mas recortadas contra o céu (G4, POCO).
inline constexpr float NightStarlightFloor = 1.5e-3f;
// Brilho do céu visível ao luar sobre a luz difusa que ele manda ao chão.
// Fisicamente seria ~1,5; os céus noturnos dos vídeos da Unity (sRGB 15-44 no
// céu) são pintados mais claros que isso. No POCO (Vila, 22 h, exposição -6)
// 12 deu um céu de sRGB ~1 e 300 um céu de anoitecer (sRGB ~190), porque o
// tonemap esmaga o fim escuro; 60 fica no azul-marinho deles. Sem lua o luar é
// zero e o céu volta a ser só estrelas, como a lua nova da Unity.
inline constexpr float NightSkyGlow = 60.0f;
// Tom do céu noturno (luminância 1): frio, como a noite dos jogos da Unity.
inline constexpr float NightSkyTint[3]{0.62f, 0.83f, 1.38f};

// 0 com o sol acima de ~1°, 1 a partir do fim do crepúsculo náutico (~-10°).
inline float nightBlend(float sunElevationSine) noexcept {
  const float t = std::clamp((0.02f - sunElevationSine) / (0.02f + 0.17f), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

inline float nightLuminance(const float c[3]) noexcept {
  return 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2];
}

struct NightSkyLighting {
  float sky[3]{};          // novo ambientColorStrength.rgb
  float ground[3]{};       // novo groundColorSaturation.rgb
  float specularScale = 1; // multiplicador do reflexo do ambiente
  float nightSky[3]{};     // o céu da noite cheia, sem a mistura com o dia
  float nightGround[3]{};
  float night = 0;
};

// `ambient`/`ground`: ambiente do céu agora (já com o daylight do modo);
// `skyDaylight`: fração do meio-dia que o céu ilumina; `moonRadiance`: cor ×
// intensidade da direcional Lua (zero sem lua); `night`: nightBlend.
inline NightSkyLighting evaluateNightSkyLighting(const float ambient[3], const float ground[3],
                                                 float skyDaylight, const float moonRadiance[3],
                                                 float night) noexcept {
  NightSkyLighting out;
  const float pi = 3.14159265358979f;
  const float ambientLuminance = std::max(nightLuminance(ambient), 1e-8f);
  const float groundRatio = std::clamp(nightLuminance(ground) / ambientLuminance, 0.0f, 1.0f);
  const float moonLuminance = std::max(nightLuminance(moonRadiance), 0.0f);
  float nightSky[3];
  for (u32 c = 0; c < 3; ++c) {
    const float moonSky = moonLuminance * NightMoonSkyFraction / pi;
    const float starlight = std::max(ambientLuminance * NightStarlightFraction, NightStarlightFloor);
    nightSky[c] = (moonSky + starlight) * NightSkyTint[c];
    out.nightSky[c] = nightSky[c];
    out.nightGround[c] = nightSky[c] * groundRatio;
    out.sky[c] = ambient[c] + (nightSky[c] - ambient[c]) * night;
    out.ground[c] = ground[c] + (nightSky[c] * groundRatio - ground[c]) * night;
  }
  // O reflexo segue o céu: de dia a fração do meio-dia, à noite a razão entre
  // o céu noturno e o ambiente do crepúsculo.
  const float nightRatio = std::clamp(nightLuminance(nightSky) / ambientLuminance, 0.0f, 1.0f);
  const float daylight = std::clamp(skyDaylight, 0.0f, 1.0f);
  out.specularScale = daylight * (1.0f + (nightRatio - 1.0f) * night);
  out.night = night;
  return out;
}

} // namespace ae::renderer
