#pragma once

// Céu procedural como o Skybox/Procedural da Unity: espalhamento atmosférico de
// O'Neil (GPU Gems 2, cap. 16, "Accurate Atmospheric Scattering"), com os mesmos
// parâmetros que a Unity expõe -- tom do céu, espessura da atmosfera, chão e
// exposição. A cor sai do ângulo do sol: meio-dia azul, pôr do sol alaranjado e
// noite escura são o mesmo modelo, sem gradiente pintado.
//
// Esta versão em C++ é a mesma conta do shader (rhi/shaders/sky_procedural.glsl).
// O renderer a usa para tirar a luz ambiente do próprio céu (a "Environment
// Lighting Source: Skybox" da Unity) e os testes de host a usam para provar o
// comportamento por hora.

#include "core/base.h"

namespace ae::renderer {

struct ProceduralSkyParameters {
  float skyTint[3]{0.5f, 0.5f, 0.5f};  // 0,5 é neutro; azul deixa o céu mais azul
  float ground[3]{0.369f, 0.349f, 0.341f};
  float atmosphereThickness = 1.0f;     // 0..5
  // A Unity usa 1,3 e mostra o céu sem tonemap; aqui o céu passa pela exposição
  // da cena e pelo tonemap, então 2,4 reproduz o brilho do céu dela (medido no
  // POCO: zênite (46,52,62) com 1,3). Mesmo valor no shader do céu.
  float exposure = 2.4f;
};

// Radiância linear (antes do tonemap) na direção `view` (unitária), com o sol
// na direção `toSun` (unitária, do chão para o sol). Sem o disco solar.
void evaluateProceduralSky(const ProceduralSkyParameters &parameters, const float toSun[3],
                           const float view[3], float out[3]) noexcept;

// Luz ambiente do céu: média do hemisfério de cima (zênite e quatro rumos no
// horizonte) e do chão, ponderada como uma irradiância difusa simples.
void proceduralSkyAmbient(const ProceduralSkyParameters &parameters, const float toSun[3],
                          float sky[3], float equator[3], float ground[3]) noexcept;

} // namespace ae::renderer
