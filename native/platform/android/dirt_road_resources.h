#pragma once

#include "platform/free_camera_controller.h"
#include "renderer/authoring_texture.h"
#include "renderer/authoring_library_plan.h"
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
#include <span>
#include <string>
#include <vector>

namespace ae::platform::android {

using EnvironmentLighting = renderer::EnvironmentLighting;

struct MaterialTextureResidencyReport final {
  u32 requestedMipBias=0,textures=0,reducedTextures=0,generatedMipTextures=0;
  u64 sourceBytes=0,residentBytes=0;
  bool fullyApplied=true;
};

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
  bool initializePrimitives(rhi::VulkanDevice &device,rhi::VulkanUploadContext &upload);
  // Biblioteca de autoria = primitivas internas + geometria importada. Reconstrói
  // do zero, sempre com a lista COMPLETA de extras: o incremental exigiria
  // manter offsets antigos válidos entre importações, e um offset errado lê a
  // geometria do vizinho sem nenhum erro. Falha fechada -- os buffers antigos só
  // são soltos depois que os novos existem e subiram.
  bool rebuildAuthoringLibrary(rhi::VulkanDevice &device, rhi::VulkanUploadContext &upload,
                               std::span<const u8> extraVertices, std::span<const u32> extraIndices,
                               std::span<const renderer::MapDrawRecord> extraDraws,
                               std::span<const renderer::MapMaterialRecord> extraMaterials,
                               std::span<const renderer::SharedAuthoringTexture> extraTextures = {});
  void shutdown();

  std::span<const u8> pickingVertices() const { return pickingVertices_; }
  std::span<const u32> pickingIndices() const { return pickingIndices_; }
  VkBuffer vertexBuffer() const { return vertices_.handle(); }
  VkBuffer indexBuffer() const { return indices_.handle(); }
  // Texturas do pacote primeiro, depois as das fontes importadas: os índices do
  // pacote não mudam quando uma importação acrescenta ou troca imagens.
  VkImageView view(u32 index) const {
    return index < images_.size() ? images_[index].view() : authoringImages_[index - images_.size()].view();
  }
  VkSampler sampler(u32 index) const {
    return index < samplers_.size() ? samplers_[index].handle() : authoringSamplers_[index - samplers_.size()].handle();
  }
  u32 textureCount() const { return static_cast<u32>(images_.size() + authoringImages_.size()); }
  u32 packageTextureCount() const { return static_cast<u32>(images_.size()); }
  // R4: anisotropia da política de qualidade aplicada aos samplers de material
  // (pacote e autoria) na próxima criação. Um valor de 1 ou menos desliga.
  void setSamplerAnisotropy(float value) { samplerAnisotropy_ = value > 1.0f ? value : 1.0f; }
  // G6-B: viés negativo de mip com ampliação temporal, para a textura manter o
  // detalhe da resolução FINAL (log2(interna/final) - 1, orientação do
  // Arm ASR/FSR 2). Zero sem ampliador; já limitado ao máximo do aparelho.
  void setSamplerMipLodBias(float value) { samplerMipLodBias_ = value; }
  float samplerMipLodBias() const { return samplerMipLodBias_; }
  float samplerAnisotropy() const { return samplerAnisotropy_; }
  // Limite global de materiais: 1 deixa de enviar o mip de maior resolução.
  // Ambiente, LUTs e recursos de UI não passam por este caminho.
  void setTextureResidencyMipBias(u32 value) { textureResidencyMipBias_=value; }
  u32 textureResidencyMipBias() const {return textureResidencyMipBias_;}
  const MaterialTextureResidencyReport &packageTextureResidency() const {return packageTextureResidency_;}
  const MaterialTextureResidencyReport &authoringTextureResidency() const {return authoringTextureResidency_;}
  const std::string &textureResidencyDiagnostic() const {return textureResidencyDiagnostic_;}
  // R2/R4: resultado da última reconstrução da biblioteca (publicação incremental).
  u32 lastReusedTextures() const { return lastReusedTextures_; }
  u32 lastUploadedTextures() const { return lastUploadedTextures_; }
  bool lastGeometryReused() const { return lastGeometryReused_; }
  // Caller waits for in-flight draws before replacing these descriptor resources.
  bool setEnvironmentMap(rhi::VulkanDevice &, rhi::VulkanUploadContext &,
                         renderer::SharedEnvironmentMap);
  VkImageView environmentView() const { return customEnvironment_ ? customEnvironmentImages_[0].view() : environmentImage_.view(); }
  VkSampler environmentSampler() const { return customEnvironment_ ? customEnvironmentSamplers_[0].handle() : environmentSampler_.handle(); }
  VkImageView environmentSpecularView() const {
    if(customEnvironment_) return customEnvironmentImages_[1].view();
    return environmentSpecularImage_.isReady() ? environmentSpecularImage_.view() : environmentImage_.view();
  }
  VkSampler environmentSpecularSampler() const {
    if(customEnvironment_) return customEnvironmentSamplers_[1].handle();
    return environmentSpecularSampler_.isReady() ? environmentSpecularSampler_.handle()
                                                 : environmentSampler_.handle();
  }
  VkImageView environmentBrdfView() const {
    if(customEnvironment_) return customEnvironmentImages_[2].view();
    return environmentBrdfImage_.isReady() ? environmentBrdfImage_.view() : environmentImage_.view();
  }
  VkSampler environmentBrdfSampler() const {
    if(customEnvironment_) return customEnvironmentSamplers_[2].handle();
    return environmentBrdfSampler_.isReady() ? environmentBrdfSampler_.handle()
                                             : environmentSampler_.handle();
  }
  const renderer::EnvironmentMapDescription &environmentMapDescription() const {
    return customEnvironment_ ? customEnvironment_->description : environmentMapDescription_;
  }
  // Independent views may select the packaged environment while the primary
  // view has an authored HDRI resident. These accessors keep that fallback
  // explicit instead of aliasing whichever custom map happens to be active.
  VkImageView packagedEnvironmentView() const { return environmentImage_.view(); }
  VkSampler packagedEnvironmentSampler() const { return environmentSampler_.handle(); }
  VkImageView packagedEnvironmentSpecularView() const {
    return environmentSpecularImage_.isReady() ? environmentSpecularImage_.view() : environmentImage_.view();
  }
  VkSampler packagedEnvironmentSpecularSampler() const {
    return environmentSpecularSampler_.isReady() ? environmentSpecularSampler_.handle()
                                                  : environmentSampler_.handle();
  }
  VkImageView packagedEnvironmentBrdfView() const {
    return environmentBrdfImage_.isReady() ? environmentBrdfImage_.view() : environmentImage_.view();
  }
  VkSampler packagedEnvironmentBrdfSampler() const {
    return environmentBrdfSampler_.isReady() ? environmentBrdfSampler_.handle()
                                              : environmentSampler_.handle();
  }
  const renderer::EnvironmentMapDescription &packagedEnvironmentMapDescription() const {
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
  std::vector<u8> pickingVertices_;
  std::vector<u32> pickingIndices_;
  renderer::MapPackageHeader header_{};
  u64 packageFingerprint_ = 0;
  std::vector<renderer::MapTextureRecord> textureRecords_;
  std::vector<renderer::MapMaterialRecord> materials_;
  std::vector<renderer::MapDrawRecord> draws_;
  std::vector<rhi::VulkanImage> images_;
  std::vector<rhi::VulkanSampler> samplers_;
  // Texturas das fontes importadas (M09.1), trocadas inteiras a cada publicação.
  std::vector<rhi::VulkanImage> authoringImages_;
  std::vector<rhi::VulkanSampler> authoringSamplers_;
  float samplerAnisotropy_ = 1.0f;
  float samplerMipLodBias_ = 0.0f;
  float authoringMipLodBias_ = 0.0f;
  u32 textureResidencyMipBias_=0;
  u32 authoringResidencyMipBias_=~u32{0};
  MaterialTextureResidencyReport packageTextureResidency_{};
  MaterialTextureResidencyReport authoringTextureResidency_{};
  std::string textureResidencyDiagnostic_;
  // Objetos de onde saiu cada imagem de autoria publicada, na mesma ordem: é por
  // eles que a próxima publicação sabe o que já está na GPU.
  std::vector<renderer::SharedAuthoringTexture> authoringTextureSources_;
  float authoringAnisotropy_ = 1.0f;
  u32 lastReusedTextures_ = 0, lastUploadedTextures_ = 0;
  bool lastGeometryReused_ = false;
  rhi::VulkanImage environmentImage_;
  rhi::VulkanSampler environmentSampler_;
  rhi::VulkanImage environmentSpecularImage_;
  rhi::VulkanSampler environmentSpecularSampler_;
  rhi::VulkanImage environmentBrdfImage_;
  rhi::VulkanSampler environmentBrdfSampler_;
  EnvironmentLighting environmentLighting_{};
  renderer::EnvironmentMapDescription environmentMapDescription_{};
  renderer::SharedEnvironmentMap customEnvironment_;
  std::array<rhi::VulkanImage,3> customEnvironmentImages_;
  std::array<rhi::VulkanSampler,3> customEnvironmentSamplers_;
  renderer::StaticCollisionMesh collisionMesh_{};
  rhi::VulkanBuffer vertices_;
  rhi::VulkanBuffer indices_;
};

} // namespace ae::platform::android
