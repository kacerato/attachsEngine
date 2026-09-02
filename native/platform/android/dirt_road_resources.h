#pragma once

#include "platform/free_camera_controller.h"
#include "renderer/map_package.h"
#include "renderer/environment_map.h"
#include "renderer/static_collision_mesh.h"
#include "rhi/device.h"
#include "rhi/resource.h"
#include "rhi/upload_context.h"

#include <android/asset_manager.h>
#include <atomic>
#include <vector>

namespace ae::platform::android {

struct EnvironmentLighting final {
  float sunDirectionIntensity[4]{};
  float sunColorAngularRadius[4]{};
  float ambientColorStrength[4]{};
  float parameters[4]{}; // exposure, rotation, maximum environment LOD, reserved
  // AEEN v2 serializa estes parâmetros no Environment Resource global. O
  // decoder mantém migração explícita para projetos AEEN v1.
  float skyZenithCloudCoverage[4]{}; // rgb linear, cobertura 0..1
  float skyHorizonCloudDensity[4]{}; // rgb linear, densidade 0..1
  float groundColorSaturation[4]{};  // rgb linear, saturação global
  float cloudLightWindSpeed[4]{};    // rgb linear, velocidade angular
};
static_assert(sizeof(EnvironmentLighting) == 128);

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
  VkImageView environmentSpecularView() const {
    return environmentSpecularImage_.isReady() ? environmentSpecularImage_.view() : environmentImage_.view();
  }
  VkSampler environmentSpecularSampler() const {
    return environmentSpecularSampler_.isReady() ? environmentSpecularSampler_.handle()
                                                 : environmentSampler_.handle();
  }
  VkImageView environmentBrdfView() const {
    return environmentBrdfImage_.isReady() ? environmentBrdfImage_.view() : environmentImage_.view();
  }
  VkSampler environmentBrdfSampler() const {
    return environmentBrdfSampler_.isReady() ? environmentBrdfSampler_.handle()
                                             : environmentSampler_.handle();
  }
  const renderer::EnvironmentMapDescription &environmentMapDescription() const {
    return environmentMapDescription_;
  }
  const EnvironmentLighting &environmentLighting() const { return environmentLighting_; }
  const std::vector<renderer::MapMaterialRecord> &materials() const { return materials_; }
  const std::vector<renderer::MapDrawRecord> &draws() const { return draws_; }
  const renderer::MapPackageHeader &header() const { return header_; }
  u64 packageFingerprint() const { return packageFingerprint_; }
  platform::FreeCameraState defaultCamera() const;
  platform::FreeCameraState defaultGameplayCamera() const;
  const renderer::StaticCollisionMesh &staticCollisionMesh() const { return collisionMesh_; }
  void releaseStaticCollisionCpuData() { collisionMesh_.clear(); }

private:
  renderer::MapPackageHeader header_{};
  u64 packageFingerprint_ = 0;
  std::vector<renderer::MapTextureRecord> textureRecords_;
  std::vector<renderer::MapMaterialRecord> materials_;
  std::vector<renderer::MapDrawRecord> draws_;
  std::vector<rhi::VulkanImage> images_;
  std::vector<rhi::VulkanSampler> samplers_;
  rhi::VulkanImage environmentImage_;
  rhi::VulkanSampler environmentSampler_;
  rhi::VulkanImage environmentSpecularImage_;
  rhi::VulkanSampler environmentSpecularSampler_;
  rhi::VulkanImage environmentBrdfImage_;
  rhi::VulkanSampler environmentBrdfSampler_;
  EnvironmentLighting environmentLighting_{};
  renderer::EnvironmentMapDescription environmentMapDescription_{};
  renderer::StaticCollisionMesh collisionMesh_{};
  rhi::VulkanBuffer vertices_;
  rhi::VulkanBuffer indices_;
};

} // namespace ae::platform::android
