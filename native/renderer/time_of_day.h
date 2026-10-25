#pragma once

// G4: hora do dia. Matemática pura (sem cena nem GPU), testável no host.
//
// Posição solar pela aproximação de declinação de Cooper (1969) e ângulo horário
// de 15° por hora; é o modelo do NOAA simplificado, com erro de ~1° -- suficiente
// para iluminação de jogo. Cor do sol por temperatura de corpo negro em função da
// elevação (mais quente perto do horizonte, onde a massa de ar é maior), como no
// estudo de gradientes por hora do UniStorm e no Sky da Godot. A fase da lua sai
// da data por um mês sinódico de 29,53 dias.
//
// Convenções do mundo: Y para cima, norte em -Z quando northDegrees é 0, leste em
// +X. `toSun` aponta do chão para o sol; a luz direcional emite em -toSun.

#include "core/base.h"

namespace ae::renderer {

struct TimeOfDayInput {
  float hour = 12.0f;          // 0..24, hora solar local
  float dayOfYear = 172.0f;    // 1..366 (172 = solstício de junho)
  float latitude = -15.0f;     // graus; negativo no hemisfério sul
  float northDegrees = 0.0f;   // giro do norte em torno de Y
};

struct TimeOfDayState {
  float toSun[3]{0, 1, 0};
  float elevation = 1.0f;      // seno da elevação: 1 zênite, 0 horizonte, <0 abaixo
  float sunColor[3]{1, 1, 1};  // multiplicador linear sobre a cor autoral da luz
  float sunIntensity = 1.0f;   // multiplicador sobre a intensidade autoral (0 à noite)
  // A parte de sunColor/sunIntensity que faz as vezes do ar entre o sol e o chão
  // (cor relativa a 5800 K e o 0,35..1 da massa de ar). O céu físico integra a
  // própria transmitância e precisa do sol fora da atmosfera: divide por estes.
  float atmosphereTint[3]{1, 1, 1};
  float atmosphereDimming = 1.0f;
  float moonPhase = 0.5f;      // 0 nova, 0.5 cheia
};

TimeOfDayState evaluateTimeOfDay(const TimeOfDayInput &input) noexcept;

// V-2: a lua da mesma data e hora, para a direcional com Corpo celeste = Lua
// (a "Moon" dos ciclos de dia do HDRP). A direção é a do céu
// (sky_celestial.glsl, celestialMoonDirection): o sol girado pela fase em torno
// do eixo horizontal perpendicular a ele, então a lua cheia nasce quando o sol
// se põe. O brilho segue a fração iluminada ao cubo (a meia-lua reflete ~1/8
// da cheia, como a curva de fase lunar medida) e some com a lua abaixo do
// horizonte e com o sol acima dele, como o script do ciclo desliga a Moon de dia.
struct MoonState {
  float toMoon[3]{0, 1, 0};
  float elevation = 1.0f;     // seno da elevação da lua
  float illuminated = 1.0f;   // fração acesa vista da Terra: 0 nova, 1 cheia
  float intensity = 1.0f;     // multiplicador sobre a intensidade autoral da Lua
};
MoonState evaluateMoon(const TimeOfDayState &sun) noexcept;

// V-2: a esfera celeste da hora, para as estrelas girarem em torno do polo como
// o "Space Rotation" do Physically Based Sky e os ciclos com latitude da Unity.
// `rows` leva uma direção do mundo para a esfera fixa das estrelas (linhas de
// uma rotação): giram 15° por hora em torno do polo, que fica a |latitude| do
// horizonte, e um dia a mais adianta o céu ~1° (tempo sideral). Um ponto fixo da
// esfera, como o sol de um mesmo dia, cai sempre no mesmo lugar dela.
void celestialSphereRows(const TimeOfDayInput &input, float rows[3][3]) noexcept;

// Hora atual: a autoral mais o tempo de Play convertido pela duração do dia.
// dayLengthMinutes <= 0 congela o relógio. O resultado volta a 0..24.
float advanceTimeOfDayHour(float authoredHour, float dayLengthMinutes, double elapsedSeconds) noexcept;


} // namespace ae::renderer
