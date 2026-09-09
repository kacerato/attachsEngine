#pragma once

#include "platform/free_camera_controller.h"
#include "renderer/map_package.h"
#include "renderer/environment_map.h"
#include "renderer/environment_lighting.h"
#include "renderer/static_collision_mesh.h"
#include "renderer/water_grid.h"
#include "rhi/device.h"
#include "rhi/resource.h"
#include "rhi/upload_context.h"

#include <android/asset_manager.h>
#include <atomic>
#include <vector>

namespace ae::platform::android {

using EnvironmentLighting = renderer::EnvironmentLighting;

// Runtime representation of the cooked Dirt Road test scene. Source glTF,
// image decoders and import metadata remain outside the APK render path.
class DirtRoadResources final {
public:
  // `waterGridSegments` é a densidade que a política de renderização escolheu
  // para a malha de água. Zero mantém a malha assada intacta, que é o
  // comportamento de sempre; qualquer valor menor que o assado faz a grade ser
  // reindexada com salto. Ver renderer/water_grid.h.
  bool initialize(rhi::VulkanDevice &device, rhi::VulkanUploadContext &upload,
                  AAssetManager *assets, bool forceTextureFallback,
                  float waterDisplacementAllowance,
                  const std::atomic<bool> *cancel = nullptr,
                  const char *assetRoot = "dirt_road",
                  u32 waterGridSegments = 0, bool waterAuthoring = false);
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
  // Renderer-owned pose commit; immutable geometry/material identity is checked.
  bool updateDrawPose(u32 index, const renderer::MapDrawRecord &draw) {
    if (index >= draws_.size()) return false;
    const auto &old = draws_[index];
    if (old.firstIndex != draw.firstIndex || old.indexCount != draw.indexCount ||
        old.vertexOffset != draw.vertexOffset || old.materialIndex != draw.materialIndex ||
        old.lodGroupId != draw.lodGroupId || old.lodLevel != draw.lodLevel) return false;
    draws_[index] = draw;
    return true;
  }
  // The renderer validates geometry references before publishing an authored list.
  void setAuthoredDraws(std::vector<renderer::MapDrawRecord> draws) { draws_=std::move(draws); }
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
