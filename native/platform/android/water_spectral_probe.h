#pragma once
#include "rhi/device.h"
namespace ae::platform::android {
// Opt-in diagnostic only; caller supplies an exclusively owned device/queue.
bool runWaterSpectralProbe(rhi::VulkanDevice &device);
}
