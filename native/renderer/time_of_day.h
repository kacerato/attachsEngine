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
  float moonPhase = 0.5f;      // 0 nova, 0.5 cheia
};

TimeOfDayState evaluateTimeOfDay(const TimeOfDayInput &input) noexcept;

// Hora atual: a autoral mais o tempo de Play convertido pela duração do dia.
// dayLengthMinutes <= 0 congela o relógio. O resultado volta a 0..24.
float advanceTimeOfDayHour(float authoredHour, float dayLengthMinutes, double elapsedSeconds) noexcept;


} // namespace ae::renderer
