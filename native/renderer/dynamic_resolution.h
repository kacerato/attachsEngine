#pragma once

#include "core/base.h"

#include <array>

namespace ae::renderer {

// Piso duro da escala interna, e o único lugar onde esse número existe: o
// controlador, o cálculo de extensão e a política de qualidade precisam
// concordar, e antes disso o 0,5 estava escrito três vezes. Abaixo disto um
// pixel interno cobre mais de quatro pixels de tela e o upscale deixa de ser
// uma troca de qualidade por cadência -- vira borrão.
inline constexpr float DynamicResolutionFloor = 0.5f;

// Política resolvida por projeto/dispositivo. O controlador trabalha somente
// com tempo de GPU: acquire, compositor e pacing não podem formar um laço de
// realimentação com a resolução interna.
struct DynamicResolutionSettings final {
  bool enabled = false;
  float minimumScale = 1.0f;
  float maximumScale = 1.0f;
  float decreaseStep = 0.05f;
  float increaseStep = 0.025f;
  float recoveryHeadroomRatio = 0.72f;
  u32 overloadFrames = 6;
  u32 recoveryFrames = 120;
  // O tempo de GPU lido num quadro é o de 2-3 quadros atrás (cerca do quadro
  // anterior, mais o que está em voo). Logo depois de uma troca, essas amostras
  // ainda são da escala velha: decidir com elas cortaria duas vezes pelo mesmo
  // excesso. É o "frames of latency" do FrameTimingManager da Unity.
  u32 settleFrames = 3;
  // Teto do degrau proporcional: a GPU num excesso de 2x desce mais que o
  // degrau mínimo de uma vez (custo ~ área ~ escala^2), mas nunca mais que isto.
  float maximumDecreaseStep = 0.15f;
};

struct DynamicResolutionUpdate final {
  float scale = 1.0f;
  bool changed = false;
};

// Estado pequeno e backend-independent. Mudanças são discretas e assimétricas:
// cede rápido quando a GPU perde o budget e recupera devagar, evitando shimmer e
// a oscilação que uma decisão por frame produziria.
//
// A decisão usa a mediana das últimas `overloadFrames` amostras, não a amostra
// do quadro: um pico isolado (shader compilando, upload) não derruba a escala,
// e uma GPU oscilando em volta do orçamento (8,6 / 7,9 ms num budget de 8)
// desce, coisa que a sequência "N quadros seguidos acima" nunca fazia porque
// cada quadro abaixo zerava a contagem. Histerese: desce acima do budget, sobe
// só abaixo de budget x recoveryHeadroomRatio por recoveryFrames quadros.
class DynamicResolutionController final {
public:
  void reset(const DynamicResolutionSettings &settings, float targetGpuMilliseconds);
  // Runtime policy changes (for example thermal pressure) must not jump back to
  // maximum resolution. Preserves the current scale inside the new bounds and
  // clears only the decision streaks.
  void reconfigure(const DynamicResolutionSettings &settings, float targetGpuMilliseconds);
  DynamicResolutionUpdate observe(float gpuMilliseconds);

  float scale() const { return scale_; }
  // Mediana da janela atual (0 sem amostras): o que a última decisão viu.
  float filteredGpuMilliseconds() const;
  // Mediana que levou à última troca de escala (a janela é zerada na troca).
  float decisionGpuMilliseconds() const { return decisionGpuMilliseconds_; }
  float targetGpuMilliseconds() const { return targetGpuMilliseconds_; }
  const DynamicResolutionSettings &settings() const { return settings_; }

private:
  static constexpr u32 WindowCapacity = 16;
  void restartWindow();

  DynamicResolutionSettings settings_{};
  std::array<float, WindowCapacity> window_{};
  u32 windowCount_ = 0;
  u32 windowHead_ = 0;
  u32 settleRemaining_ = 0;
  float decisionGpuMilliseconds_ = 0.0f;
  float targetGpuMilliseconds_ = 0.0f;
  float scale_ = 1.0f;
  u32 overloadStreak_ = 0;
  u32 recoveryStreak_ = 0;
};

// Extents são alinhados a 8 pixels para manter compute/tile paths previsíveis.
// Entrada inválida falha para a extensão cheia, nunca para zero.
u32 scaledRenderExtent(u32 fullExtent, float scale);

} // namespace ae::renderer
