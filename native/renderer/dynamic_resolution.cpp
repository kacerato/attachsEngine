#include "renderer/dynamic_resolution.h"

#include <algorithm>
#include <cmath>

namespace ae::renderer {

void DynamicResolutionController::reset(const DynamicResolutionSettings &settings,
                                        float targetGpuMilliseconds) {
  settings_ = settings;
  settings_.minimumScale = std::clamp(settings_.minimumScale, DynamicResolutionFloor, 1.0f);
  settings_.maximumScale = std::clamp(settings_.maximumScale, settings_.minimumScale, 1.0f);
  settings_.decreaseStep = std::clamp(settings_.decreaseStep, 0.01f, 0.25f);
  settings_.increaseStep = std::clamp(settings_.increaseStep, 0.005f, 0.25f);
  settings_.recoveryHeadroomRatio =
      std::clamp(settings_.recoveryHeadroomRatio, 0.5f, 0.95f);
  settings_.overloadFrames = std::clamp(settings_.overloadFrames, 1u, WindowCapacity);
  settings_.recoveryFrames = std::max(1u, settings_.recoveryFrames);
  settings_.settleFrames = std::min(settings_.settleFrames, 8u);
  settings_.maximumDecreaseStep =
      std::clamp(settings_.maximumDecreaseStep, settings_.decreaseStep, 0.25f);
  targetGpuMilliseconds_ = std::isfinite(targetGpuMilliseconds) && targetGpuMilliseconds > 0.0f
                               ? targetGpuMilliseconds : 0.0f;
  scale_ = settings_.maximumScale;
  overloadStreak_ = 0;
  recoveryStreak_ = 0;
  settleRemaining_ = 0;
  decisionGpuMilliseconds_ = 0.0f;
  restartWindow();
}

void DynamicResolutionController::reconfigure(const DynamicResolutionSettings &settings,
                                              float targetGpuMilliseconds) {
  const float previousScale = scale_;
  reset(settings, targetGpuMilliseconds);
  scale_ = std::clamp(previousScale, settings_.minimumScale, settings_.maximumScale);
}

void DynamicResolutionController::restartWindow() {
  windowCount_ = 0;
  windowHead_ = 0;
}

float DynamicResolutionController::filteredGpuMilliseconds() const {
  if (windowCount_ == 0) return 0.0f;
  const u32 count = std::min(windowCount_, settings_.overloadFrames);
  std::array<float, WindowCapacity> recent{};
  for (u32 index = 0; index < count; ++index)
    recent[index] = window_[(windowHead_ + WindowCapacity - 1 - index) % WindowCapacity];
  const u32 middle = count / 2;
  std::nth_element(recent.begin(), recent.begin() + middle, recent.begin() + count);
  if (count % 2 != 0) return recent[middle];
  const float upper = recent[middle];
  const float lower = *std::max_element(recent.begin(), recent.begin() + middle);
  return 0.5f * (lower + upper);
}

DynamicResolutionUpdate DynamicResolutionController::observe(float gpuMilliseconds) {
  if (!settings_.enabled || targetGpuMilliseconds_ <= 0.0f ||
      !std::isfinite(gpuMilliseconds) || gpuMilliseconds <= 0.0f) {
    return {scale_, false};
  }
  // Amostras ainda da escala anterior à última troca: descartadas.
  if (settleRemaining_ > 0) {
    --settleRemaining_;
    return {scale_, false};
  }
  window_[windowHead_] = gpuMilliseconds;
  windowHead_ = (windowHead_ + 1) % WindowCapacity;
  windowCount_ = std::min(windowCount_ + 1, WindowCapacity);
  const float filtered = filteredGpuMilliseconds();
  const auto changeTo = [&](float next) -> DynamicResolutionUpdate {
    const float previous = scale_;
    scale_ = next;
    decisionGpuMilliseconds_ = filtered;
    overloadStreak_ = 0;
    recoveryStreak_ = 0;
    settleRemaining_ = settings_.settleFrames;
    restartWindow();
    return {scale_, scale_ != previous};
  };

  if (filtered > targetGpuMilliseconds_) {
    recoveryStreak_ = 0;
    overloadStreak_ = std::min(windowCount_, settings_.overloadFrames);
    if (overloadStreak_ >= settings_.overloadFrames &&
        scale_ > settings_.minimumScale + 1.0e-4f) {
      // Escala que caberia em 95% do budget se o custo seguir a área; o degrau
      // fica entre o mínimo configurado e o teto, para um erro de previsão
      // (custo fixo de pós, CPU-bound) não derrubar a imagem de uma vez.
      const float fitting = scale_ * std::sqrt(0.95f * targetGpuMilliseconds_ / filtered);
      const float step = std::clamp(scale_ - fitting, settings_.decreaseStep,
                                    settings_.maximumDecreaseStep);
      return changeTo(std::max(settings_.minimumScale, scale_ - step));
    }
    return {scale_, false};
  }

  overloadStreak_ = 0;
  if (filtered < targetGpuMilliseconds_ * settings_.recoveryHeadroomRatio) {
    if (recoveryStreak_ < settings_.recoveryFrames) ++recoveryStreak_;
    if (recoveryStreak_ >= settings_.recoveryFrames &&
        scale_ < settings_.maximumScale - 1.0e-4f) {
      return changeTo(std::min(settings_.maximumScale, scale_ + settings_.increaseStep));
    }
  } else {
    recoveryStreak_ = 0;
  }
  return {scale_, false};
}

u32 scaledRenderExtent(u32 fullExtent, float scale) {
  if (fullExtent == 0) return 1;
  if (!std::isfinite(scale) || scale <= 0.0f) return fullExtent;
  if (scale >= 0.999f) return fullExtent;
  const u32 raw = std::max(1u, static_cast<u32>(static_cast<float>(fullExtent) *
                                                std::clamp(scale, DynamicResolutionFloor, 1.0f)));
  if (raw < 8u) return raw;
  return std::max(8u, raw & ~7u);
}

} // namespace ae::renderer
