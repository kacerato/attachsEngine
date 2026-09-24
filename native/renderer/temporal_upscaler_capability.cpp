#include "renderer/temporal_upscaler_capability.h"

namespace ae::renderer {

TemporalUpscalerAvailability probeArmAsr(const TemporalUpscalerProbe &p) {
  using A = TemporalUpscalerAvailability;
  if (!p.armAsrBuilt) return A::NotBuilt;
  if (!p.hdrSceneColor) return A::MissingHdrSceneColor;
  if (!p.shaderFloat16) return A::MissingFloat16;
  if (!p.shaderInt16) return A::MissingInt16;
  if (!p.computeSubgroupQuad) return A::MissingQuadSubgroup;
  if (!p.storageImageExtendedFormats) return A::MissingStorageImageFormats;
  if (!p.sampledDepth32) return A::MissingSampledDepth32;
  return A::Available;
}

TemporalUpscalerAvailability probeFsr2(const TemporalUpscalerProbe &p) {
  using A = TemporalUpscalerAvailability;
  if (!p.fsr2Built) return A::NotBuilt;
  if (!p.hdrSceneColor) return A::MissingHdrSceneColor;
  if (!p.storageImageExtendedFormats) return A::MissingStorageImageFormats;
  if (!p.storageImageWriteWithoutFormat) return A::MissingStorageWriteWithoutFormat;
  if (!p.computeSubgroupBasic || !p.computeSubgroupQuad) return A::MissingQuadSubgroup;
  if (p.shaderFloat16 && !p.shaderInt16) return A::MissingInt16;
  return A::Available;
}

} // namespace ae::renderer
