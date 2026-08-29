#pragma once

#include "platform/free_camera_controller.h"
#include "renderer/map_package.h"
#include "rhi/device.h"
#include "rhi/resource.h"
#include "rhi/upload_context.h"

#include <android/asset_manager.h>
#include <atomic>
#include <vector>

namespace ae::platform::android {

// AEEN v2. skyIrradianceSH holds 9 SH (bands l=0,1,2) irradiance
// coefficients, RGB each padded to a 4th float for std140-safe layout,
// matching `vec4 skyIrradianceSH[9]` in environment_lighting.glsl exactly.
// Basis order and cosine-lobe convolution are documented in
// tools/cook-procedural-sky.py; both sides must stay in lockstep since there
// is no shared codegen for this cross-language layout.
struct EnvironmentLighting final {
  float sunDirectionIntensity[4]{};
  float sunColorAngularRadius[4]{};
  float ambientColorStrength[4]{};
  float parameters[4]{}; // exposure, rotation, maximum environment LOD, reserved
  float skyIrradianceSH[36]{};
};
static_assert(sizeof(EnvironmentLighting) == 208);

// Runtime representation of the cooked Dirt Road test scene. Source glTF,
// image decoders and import metadata remain outside the APK render path.
class DirtRoadResources final {
public:
  bool initialize(rhi::VulkanDevice &device, rhi::VulkanUploadContext &upload,
                  AAssetManager *assets, bool forceTextureFallback,
                  const std::atomic<bool> *cancel = nullptr);
  void shutdown();

  VkBuffer vertexBuffer() const { return vertices_.handle(); }
  VkBuffer indexBuffer() const { return indices_.handle(); }
  VkImageView view(u32 index) const { return images_[index].view(); }
  VkSampler sampler(u32 index) const { return samplers_[index].handle(); }
  u32 textureCount() const { return static_cast<u32>(images_.size()); }
  VkImageView environmentView() const { return environmentImage_.view(); }
  VkSampler environmentSampler() const { return environmentSampler_.handle(); }
  const EnvironmentLighting &environmentLighting() const { return environmentLighting_; }
  const std::vector<renderer::MapMaterialRecord> &materials() const { return materials_; }
  const std::vector<renderer::MapDrawRecord> &draws() const { return draws_; }
  const renderer::MapPackageHeader &header() const { return header_; }
  platform::FreeCameraState defaultCamera() const;

private:
  renderer::MapPackageHeader header_{};
  std::vector<renderer::MapTextureRecord> textureRecords_;
  std::vector<renderer::MapMaterialRecord> materials_;
  std::vector<renderer::MapDrawRecord> draws_;
  std::vector<rhi::VulkanImage> images_;
  std::vector<rhi::VulkanSampler> samplers_;
  rhi::VulkanImage environmentImage_;
  rhi::VulkanSampler environmentSampler_;
  EnvironmentLighting environmentLighting_{};
  rhi::VulkanBuffer vertices_;
  rhi::VulkanBuffer indices_;
};

} // namespace ae::platform::android
