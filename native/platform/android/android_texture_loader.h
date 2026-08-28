#pragma once

#include "rhi/device.h"
#include "rhi/resource.h"
#include "rhi/upload_context.h"

#include <android/asset_manager.h>
#include <atomic>
#include <vector>

namespace ae::platform::android {

bool readAndroidAsset(AAssetManager *assets, const char *name, std::vector<u8> &bytes,
                      const std::atomic<bool> *cancel = nullptr);

bool loadAndroidTexture(rhi::VulkanDevice &device, rhi::VulkanUploadContext &upload,
                        AAssetManager *assets, const char *name, u32 maxDimension,
                        u64 budgetBytes, const rhi::SamplerDesc &samplerDescription,
                        rhi::VulkanImage &image, rhi::VulkanSampler &sampler,
                        const std::atomic<bool> *cancel = nullptr,
                        const char *diagnosticCategory = "Texture");

} // namespace ae::platform::android
