#include "platform/android/android_texture_loader.h"

#include "renderer/texture_payload.h"

#include <android/log.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <memory>

namespace ae::platform::android {
namespace {
using Asset = std::unique_ptr<AAsset, decltype(&AAsset_close)>;

bool readExact(AAsset *asset, void *destination, usize size, const std::atomic<bool> *cancel) {
  auto *bytes = static_cast<u8 *>(destination);
  while (size != 0) {
    if (cancel != nullptr && cancel->load()) return false;
    const int read = AAsset_read(asset, bytes, std::min<usize>(size, 1024 * 1024));
    if (read <= 0) return false;
    bytes += read;
    size -= static_cast<usize>(read);
  }
  return true;
}
} // namespace

bool readAndroidAsset(AAssetManager *assets, const char *name, std::vector<u8> &bytes,
                      const std::atomic<bool> *cancel) {
  if (assets == nullptr || name == nullptr) return false;
  Asset asset(AAssetManager_open(assets, name, AASSET_MODE_RANDOM), AAsset_close);
  if (!asset) return false;
  const off64_t length = AAsset_getLength64(asset.get());
  if (length <= 0 || static_cast<u64>(length) > 1024ull * 1024 * 1024) return false;
  std::vector<u8> loaded(static_cast<usize>(length));
  if (!readExact(asset.get(), loaded.data(), loaded.size(), cancel)) return false;
  bytes = std::move(loaded);
  return true;
}

bool loadAndroidTexture(rhi::VulkanDevice &device, rhi::VulkanUploadContext &upload,
                        AAssetManager *assets, const char *name, u32 maxDimension,
                        u64 budgetBytes, const rhi::SamplerDesc &samplerDescription,
                        bool sampleMipChain,
                        rhi::VulkanImage &image, rhi::VulkanSampler &sampler,
                        const std::atomic<bool> *cancel, const char *diagnosticCategory,
                        u32 residencyMipBias, AndroidTextureResidency *residency) {
  Asset asset(AAssetManager_open(assets, name, AASSET_MODE_RANDOM), AAsset_close);
  if (!asset) {
    __android_log_print(ANDROID_LOG_ERROR, "Aether.Android", "[%s] Asset ausente: %s",
                        diagnosticCategory, name);
    return false;
  }
  std::array<u8, 32> header{};
  renderer::TexturePayload payload;
  if (!readExact(asset.get(), header.data(), header.size(), cancel) ||
      !renderer::decodeTextureHeader(header, AAsset_getLength64(asset.get()), payload)) return false;
  const auto sourceDescription = payload.description;
  auto description = sourceDescription;
  VkFormatProperties properties{};
  vkGetPhysicalDeviceFormatProperties(device.physicalDevice(), description.format, &properties);
  constexpr auto required = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
                            VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
  if ((properties.optimalTilingFeatures & required) != required) return false;
  const auto resident=renderer::chooseResidentRange(description,maxDimension,budgetBytes,residencyMipBias);
  if (!resident.valid()) {
    __android_log_print(ANDROID_LOG_ERROR,"Aether.Android",
        "[%s] texture=%s não possui mip residente para bias=%u.",diagnosticCategory,name,residencyMipBias);
    return false;
  }
  description=resident.description;
  const u32 baseMip=resident.baseMip;
  const u64 offset=32+resident.byteOffset,size=resident.byteSize;
  if (AAsset_seek64(asset.get(), static_cast<off64_t>(offset), SEEK_SET) != static_cast<off64_t>(offset)) return false;
  std::vector<u8> payloadBytes(static_cast<usize>(size));
  if (!readExact(asset.get(), payloadBytes.data(), payloadBytes.size(), cancel) ||
      (cancel != nullptr && cancel->load()) ||
      !device.memoryAllocator().createImage(description, &image) ||
      !upload.uploadSampledMipChain(device.memoryAllocator(), payloadBytes.data(), size, image)) return false;
  rhi::SamplerDesc residentSampler = samplerDescription;
  if (sampleMipChain)
    residentSampler.maxLod = static_cast<float>(description.mipLevels - 1);
  if (!sampler.initialize(device.handle(), residentSampler)) return false;
  if(residency) *residency={sourceDescription.width,sourceDescription.height,sourceDescription.mipLevels,
      description.width,description.height,description.mipLevels,baseMip,payload.payloadBytes,size};
  __android_log_print(ANDROID_LOG_INFO, "Aether.Android",
      "[%s] texture=%s resident=%ux%u mips=%u bytes=%llu", diagnosticCategory, name,
      description.width, description.height, description.mipLevels,
      static_cast<unsigned long long>(size));
  return true;
}

} // namespace ae::platform::android
