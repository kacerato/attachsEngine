#pragma once
#include "rhi/device.h"
#include "rhi/resource.h"
#include "rhi/upload_context.h"
#include <android/asset_manager.h>
#include <array>
#include <atomic>

namespace ae::platform::android {
// Owned by the renderer, rebuilt with the device. No decoded source image at runtime.
class MaterialPreviewResources {
public:
  static constexpr u32 TextureCount=5;
  bool initialize(rhi::VulkanDevice &device,rhi::VulkanUploadContext &upload,
                  AAssetManager *assets,bool forceTextureFallback,const std::atomic<bool> *cancel=nullptr);
  void shutdown();
  VkImageView view(u32 index) const { return images_[index].view(); }
  VkSampler sampler(u32 index) const { return samplers_[index].handle(); }
  VkBuffer vertexBuffer() const { return vertices_.handle(); }
  VkBuffer indexBuffer() const { return indices_.handle(); }
  u32 indexCount() const { return indexCount_; }
private:
  std::array<rhi::VulkanImage,TextureCount> images_;
  std::array<rhi::VulkanSampler,TextureCount> samplers_;
  rhi::VulkanBuffer vertices_,indices_;
  u32 indexCount_=0;
};
}
