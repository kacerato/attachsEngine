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
  settings_.overloadFrames = std::max(1u, settings_.overloadFrames);
  settings_.recoveryFrames = std::max(1u, settings_.recoveryFrames);
  targetGpuMilliseconds_ = std::isfinite(targetGpuMilliseconds) && targetGpuMilliseconds > 0.0f
                               ? targetGpuMilliseconds : 0.0f;
  scale_ = settings_.maximumScale;
  overloadStreak_ = 0;
  recoveryStreak_ = 0;
}

void DynamicResolutionController::reconfigure(const DynamicResolutionSettings &settings,
                                              float targetGpuMilliseconds) {
  const float previousScale = scale_;
  reset(settings, targetGpuMilliseconds);
  scale_ = std::clamp(previousScale, settings_.minimumScale, settings_.maximumScale);
}

DynamicResolutionUpdate DynamicResolutionController::observe(float gpuMilliseconds) {
  if (!settings_.enabled || targetGpuMilliseconds_ <= 0.0f ||
      !std::isfinite(gpuMilliseconds) || gpuMilliseconds <= 0.0f) {
    return {scale_, false};
  }

  if (gpuMilliseconds > targetGpuMilliseconds_) {
    recoveryStreak_ = 0;
    if (overloadStreak_ < settings_.overloadFrames) ++overloadStreak_;
    if (overloadStreak_ >= settings_.overloadFrames &&
        scale_ > settings_.minimumScale + 1.0e-4f) {
      const float previous = scale_;
      scale_ = std::max(settings_.minimumScale, scale_ - settings_.decreaseStep);
      overloadStreak_ = 0;
      return {scale_, scale_ != previous};
    }
    return {scale_, false};
  }

  overloadStreak_ = 0;
  if (gpuMilliseconds < targetGpuMilliseconds_ * settings_.recoveryHeadroomRatio) {
    if (recoveryStreak_ < settings_.recoveryFrames) ++recoveryStreak_;
    if (recoveryStreak_ >= settings_.recoveryFrames &&
        scale_ < settings_.maximumScale - 1.0e-4f) {
      const float previous = scale_;
      scale_ = std::min(settings_.maximumScale, scale_ + settings_.increaseStep);
      recoveryStreak_ = 0;
      return {scale_, scale_ != previous};
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
