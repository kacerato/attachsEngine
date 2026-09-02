#pragma once

#include "core/base.h"

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
};

struct DynamicResolutionUpdate final {
  float scale = 1.0f;
  bool changed = false;
};

// Estado pequeno e backend-independent. Mudanças são discretas e assimétricas:
// cede rápido quando a GPU perde o budget e recupera devagar, evitando shimmer e
// a oscilação que uma decisão por frame produziria.
class DynamicResolutionController final {
public:
  void reset(const DynamicResolutionSettings &settings, float targetGpuMilliseconds);
  DynamicResolutionUpdate observe(float gpuMilliseconds);

  float scale() const { return scale_; }
  float targetGpuMilliseconds() const { return targetGpuMilliseconds_; }
  const DynamicResolutionSettings &settings() const { return settings_; }

private:
  DynamicResolutionSettings settings_{};
  float targetGpuMilliseconds_ = 0.0f;
  float scale_ = 1.0f;
  u32 overloadStreak_ = 0;
  u32 recoveryStreak_ = 0;
};

// Extents são alinhados a 8 pixels para manter compute/tile paths previsíveis.
// Entrada inválida falha para a extensão cheia, nunca para zero.
u32 scaledRenderExtent(u32 fullExtent, float scale);

} // namespace ae::renderer
