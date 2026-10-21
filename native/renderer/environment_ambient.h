#pragma once

// PK-1 (A-02): a luz ambiente como a "Environment Lighting" da janela Lighting
// da Unity 6. As três fontes viram a MESMA sonda de ambiente -- harmônicos
// esféricos reais L2 já convoluídos com o cosseno, o formato que os HDRIs
// cozidos usam (renderer/environment_map.h) e que environment_lighting.glsl
// avalia por normal:
//  - Skybox: projeta o próprio céu procedural (O'Neil, procedural_sky.h) no
//    ângulo atual do sol. Pôr do sol tinge o lado do sol, como na Unity.
//  - Gradient: céu, equador e chão (ambientSkyColor/EquatorColor/GroundColor).
//  - Color: uma cor só (só L0).
// Referência: https://docs.unity3d.com/6000.0/Documentation/Manual/lighting-window.html

#include "renderer/environment_map.h"
#include "renderer/procedural_sky.h"

namespace ae::renderer {

// Irradiância (E, não E/pi) de cada fonte. `sourceRotationRadians` é a mesma
// rotação que o shader aplica à normal antes de avaliar a sonda
// (environmentSourceDirection): o resultado já sai no espaço dela.
void ambientProbeFromProceduralSky(const ProceduralSkyParameters &sky, const float toSun[3],
                                   float sourceRotationRadians, DiffuseIrradianceSh9 &out) noexcept;
void ambientProbeFromGradient(const float skyLinear[3], const float equatorLinear[3],
                              const float groundLinear[3], DiffuseIrradianceSh9 &out) noexcept;
void ambientProbeFromColor(const float colorLinear[3], DiffuseIrradianceSh9 &out) noexcept;

// A sonda do céu só muda quando o sol ou os parâmetros mudam; arrastar a hora
// do dia refaz 256 avaliações, o resto dos quadros reaproveita.
class ProceduralAmbientProbeCache final {
public:
  const DiffuseIrradianceSh9 &resolve(const ProceduralSkyParameters &sky, const float toSun[3],
                                      float sourceRotationRadians) noexcept;
private:
  bool valid_ = false;
  float key_[13]{};
  DiffuseIrradianceSh9 probe_{};
};

} // namespace ae::renderer
