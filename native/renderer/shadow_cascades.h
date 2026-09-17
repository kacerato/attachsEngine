// Sombras direcionais em cascata — a matemática, sem Vulkan.
//
// Hoje o sol da engine ilumina por N·L e nada mais: uma árvore não escurece o chão
// atrás dela, e por isso a luz não tem propósito direcional. O que falta é oclusão,
// e a forma padrão de tê-la para uma luz direcional num mundo aberto é o mapa de
// sombra em cascata.
//
// Este módulo resolve as três decisões que definem se a sombra fica correta ou
// cheia de artefato, e todas as três são aritmética pura, testável sem GPU:
//
// 1. **Onde cortar as cascatas.** Divisão logarítmica pura concentra resolução
//    demais perto da câmera e deixa a cascata distante grossa; divisão uniforme faz
//    o oposto. A prática consolidada é interpolar entre as duas por um fator lambda.
//
// 2. **Que volume cada cascata cobre.** A esfera que circunscreve o frustum daquela
//    fatia — e não a caixa alinhada à luz. A esfera é invariante à rotação da câmera,
//    então o volume da cascata não muda de tamanho quando o jogador apenas olha em
//    volta; com a caixa, ele muda, e a resolução efetiva da sombra pulsa.
//
// 3. **Como impedir a borda de "nadar".** Ancorando o centro da cascata a múltiplos
//    inteiros do texel do mapa. Sem isso, cada movimento sub-texel da câmera reamostra
//    a sombra num ponto diferente e a borda ferve — o artefato mais visível de CSM.
#pragma once

#include "core/base.h"

namespace ae::renderer {

// Uma cascata resolvida. `viewProjection` é column-major, mesma convenção do resto
// do renderer (ver renderer/normal_matrix.h e o layout de GpuMeshInstance).
struct ShadowCascade final {
  // Limites da fatia do frustum, em distância ao longo do eixo de visão.
  float nearDistance = 0.0f;
  float farDistance = 0.0f;
  // Centro e raio da esfera que circunscreve a fatia, em espaço de mundo.
  float centerWorld[3]{};
  float radiusWorld = 0.0f;
  // Volume conservador de casters: cilindro da esfera receptora extrudado em
  // direção ao sol. Permite retirar draws que não podem afetar esta cascata
  // sem usar o frustum da câmera (que removeria casters fora da tela).
  float lightDirection[3]{0.0f, -1.0f, 0.0f};
  float casterExtrusionWorld = 0.0f;
  // Matriz de mundo para clip da luz, pronta para o passe de sombra e para a
  // amostragem no shading.
  float viewProjection[16]{};
  // Tamanho do texel desta cascata em unidades de mundo. O shading usa isto para
  // escalar o normal-offset: o offset correto é medido em texels, não em metros,
  // senão a cascata distante recebe um deslocamento pequeno demais.
  float worldUnitsPerTexel = 0.0f;
};

struct ShadowCascadeInput final {
  // Câmera: origem, direção de visão e vetor "para cima", todos normalizados.
  float cameraPosition[3]{};
  float cameraForward[3]{0.0f, 0.0f, 1.0f};
  float cameraUp[3]{0.0f, 1.0f, 0.0f};
  // Campo de visão vertical em radianos e razão de aspecto largura/altura.
  float verticalFovRadians = 1.0472f;
  float aspectRatio = 1.7778f;
  float nearPlane = 0.1f;
  // Distância máxima em que o sol projeta sombra. Vem de ShadowSettings.
  float shadowDistance = 200.0f;
  // Direção **da luz para a cena** (a direção em que os fótons viajam),
  // normalizada. É a mesma convenção de `environment.sunDirectionIntensity.xyz`.
  float lightDirection[3]{0.0f, -1.0f, 0.0f};
  // Quanto o volume da luz se estende para trás do centro da cascata, para que
  // objetos fora do frustum ainda projetem sombra dentro dele.
  float casterExtrusion = 200.0f;
  u32 cascadeResolution = 1024;
  // Reserva ao redor do receptor para cache estático. A matriz cobre o raio
  // exato multiplicado por este valor; [1,1.25] é o intervalo de política.
  float receiverGuardBandRatio = 1.0f;
  // A cascata seguinte também cobre a faixa anterior usada pelo crossfade.
  // Deve ser o mesmo valor enviado ao shader, independente do guard band do cache.
  float cascadeBlendRatio = 0.0f;
  // Zero selects perspective; positive values describe the parallel receiver
  // volume in world units. Independent of FOV and object distance.
  float orthographicHalfHeight = 0.0f;
};

// Fator de mistura entre divisão uniforme (0) e logarítmica (1). 0,75 é o ponto que
// a literatura de CSM usa por padrão e que mantém a primeira cascata utilizável sem
// desperdiçar as distantes.
constexpr float DefaultCascadeSplitLambda = 0.75f;

constexpr u32 MaximumShadowCascades = 4;

// Calcula os limites das cascatas. Devolve quantos foram escritos; `count` é
// limitado a MaximumShadowCascades e a entrada inválida devolve 0 sem escrever.
u32 computeCascadeSplits(float nearPlane, float shadowDistance, u32 count, float lambda,
                         float *outSplits);

// Resolve as cascatas completas. Devolve quantas foram escritas. Entrada não finita,
// contagem zero, resolução zero ou direção de luz degenerada devolvem 0 — falha
// fechada: sem cascata, o chamador desenha sem sombra em vez de com sombra errada.
u32 computeShadowCascades(const ShadowCascadeInput &input, u32 count, float lambda,
                          ShadowCascade *outCascades);

// Teste conservador de uma bounding sphere contra o volume de caster da
// cascata. Entrada inválida falha aberta (visível), porque perder uma sombra é
// pior do que submeter um draw extra.
bool isShadowCasterVisible(const ShadowCascade &cascade, const float center[3], float radius);

// Verdadeiro quando o receptor desejado permanece totalmente dentro do volume
// cacheado. Com isso a cascata antiga continua correta e não precisa ser refeita.
bool canReuseStaticShadowCascade(const ShadowCascade &cached, const ShadowCascade &desired,
                                 float guardBandRatio);

} // namespace ae::renderer
