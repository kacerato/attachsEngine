#pragma once
#include "core/base.h"

namespace ae::renderer {

// O que desenhar da grade editorial neste quadro.
//
// A **política** é do editor (qual espaçamento, quanto some no horizonte) e a
// **coverage** é do fragmento: um plano analítico desenha linhas de qualquer
// densidade sem lista de segmentos, e escreve profundidade, que é o que faz uma
// caixa opaca esconder as linhas atrás dela.
//
// A versão anterior desenhava 258 segmentos como linhas de INTERFACE, depois de
// toda a cena e sem profundidade nenhuma. Por construção ela aparecia por cima
// de qualquer objeto, e trocar de década de escala trocava todas as linhas de
// uma vez — o "piscar" ao aproximar e afastar.
struct GridPlan {
  bool enabled = false;
  float planeHeight = 0;
  // Célula fina e a de cada dez. As duas coexistem com mistura contínua: é ela
  // que substitui a troca abrupta de década.
  float minorSpacing = 0;
  float majorSpacing = 0;
  // Peso da célula fina, 0..1. Em 1 as duas aparecem; em 0 só a grossa restou,
  // e a fina já terminou de sumir antes de a próxima década entrar.
  float minorOpacity = 0;
  // Distância em que a grade termina de sumir. Sem isto, o horizonte vira uma
  // faixa branca de linhas acumuladas.
  float fadeDistance = 0;
  bool valid() const noexcept {
    return enabled && minorSpacing > 0 && majorSpacing >= minorSpacing && fadeDistance > 0;
  }
};

} // namespace ae::renderer
