#pragma once

#include "rhi/device.h"
#include "rhi/resource.h"
#include "rhi/upload_context.h"

#include <android/asset_manager.h>
#include <atomic>
#include <vector>

namespace ae::platform::android {

struct AndroidTextureResidency final {
  u32 sourceWidth=0,sourceHeight=0,sourceLevels=0;
  u32 residentWidth=0,residentHeight=0,residentLevels=0,baseMip=0;
  u64 sourceBytes=0,residentBytes=0;
};

bool readAndroidAsset(AAssetManager *assets, const char *name, std::vector<u8> &bytes,
                      const std::atomic<bool> *cancel = nullptr);

bool loadAndroidTexture(rhi::VulkanDevice &device, rhi::VulkanUploadContext &upload,
                        AAssetManager *assets, const char *name, u32 maxDimension,
                        u64 budgetBytes, const rhi::SamplerDesc &samplerDescription,
                        bool sampleMipChain,
                        rhi::VulkanImage &image, rhi::VulkanSampler &sampler,
                        const std::atomic<bool> *cancel = nullptr,
                        const char *diagnosticCategory = "Texture",
                        u32 residencyMipBias = 0,
                        AndroidTextureResidency *residency = nullptr);

} // namespace ae::platform::android
