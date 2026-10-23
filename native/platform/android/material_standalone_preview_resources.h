#pragma once
#include "rhi/memory_allocator.h"
#include "renderer/material_preview_geometry.h"
#include "renderer/material_override.h"
#include <array>
#include <span>

namespace ae::platform::android {
// Owned exclusively by the independent preview. The caller must finish the
// previous preview submission before prepare/reset, as with its render targets.
// Publication is atomic: a failed allocation/flush leaves the old set intact.
class MaterialStandalonePreviewResources {
public:
  bool prepare(rhi::VulkanMemoryAllocator &allocator,
      std::shared_ptr<const renderer::MaterialPreviewGeometry> geometry,
      const renderer::MaterialOverride &effective,u32 textureBase,u32 textureCount,
      std::span<const u32> bindlessSlots,bool bindless,std::string &diagnostic) {
    diagnostic.clear();
    if(!geometry||!effective.enabled) {diagnostic="Dados da prévia de material indisponíveis.";return false;}
    const auto material=renderer::applyMaterialOverride(geometry->base,effective,textureBase);
    for(u32 binding=0;binding<scene::MaterialTextureCount;++binding) {
      const auto index=material.textureIndices[binding];
      if(index!=renderer::InvalidMapTexture && (index>=textureCount || (bindless&&index>=bindlessSlots.size()))) {
        diagnostic="Binding de textura ainda não foi publicado no renderer.";return false;
      }
    }
    u32 occlusion=renderer::MaterialExtensionNoTexture;
    if(effective.occlusionTexture!=scene::MaterialTextureKeep&&effective.occlusionTexture!=renderer::InvalidMapTexture) {
      const u64 index=u64{effective.occlusionTexture}+textureBase;
      if(!bindless) {diagnostic="Prévia de oclusão separada requer descritores bindless neste backend.";return false;}
      if(index>=textureCount||index>=bindlessSlots.size()) {diagnostic="Textura de oclusão ainda não foi publicada no renderer.";return false;}
      occlusion=bindlessSlots[index];
    }
    // PK-9: altura, detalhe e clear coat. Sem bindless o shader não os lê.
    u32 extraSlots[scene::MaterialExtraTextureCount];
    for(u32 extra=0;extra<scene::MaterialExtraTextureCount;++extra) {
      extraSlots[extra]=renderer::MaterialExtensionNoTexture;
      const u64 index=u64{effective.extraTextures[extra]}+textureBase;
      if(bindless&&renderer::materialExtraTexturePresent(effective.extraTextures[extra])&&index<textureCount&&index<bindlessSlots.size())
        extraSlots[extra]=bindlessSlots[index];
    }
    if(geometry_==geometry&&effective_==effective&&textureBase_==textureBase&&occlusion_==occlusion) return true;
    // Encoded entry1 addresses storage entry0; encoded zero means no extension.
    std::array<float,renderer::MaterialExtensionFloats> extension{};
    renderer::materialExtensionEntry(effective,material.flags,occlusion,extension.data(),renderer::MaterialExtensionNoTexture,extraSlots);
    rhi::VulkanBuffer vertices,indices,instance,materialExtension;
    const auto create=[&](std::span<const u8> bytes,VkBufferUsageFlags usage,rhi::VulkanBuffer &out) {
      rhi::BufferDesc desc{};desc.sizeBytes=bytes.size();desc.usage=usage;
      desc.cpuAccess=rhi::CpuAccess::SequentialWrite;desc.preferDeviceMemory=false;
      if(bytes.empty()||!allocator.createBuffer(desc,&out)||!out.mappedData()) return false;
      std::memcpy(out.mappedData(),bytes.data(),bytes.size());return allocator.flushBuffer(out);
    };
    const auto bytes=[](const auto &value) {
      return std::span<const u8>{reinterpret_cast<const u8*>(std::data(value)),std::size(value)*sizeof(*std::data(value))};
    };
    const bool newGeometry=geometry_!=geometry;
    if((newGeometry&&(!create(bytes(geometry->vertices),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,vertices)||
       !create(bytes(geometry->indices),VK_BUFFER_USAGE_INDEX_BUFFER_BIT,indices)||
       !create({reinterpret_cast<const u8*>(&geometry->instance),sizeof(geometry->instance)},VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,instance)))||
       !create(bytes(extension),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,materialExtension)) {
      diagnostic="Memória ou upload da prévia de material indisponível.";return false;
    }
    if(newGeometry) {vertices_=std::move(vertices);indices_=std::move(indices);instance_=std::move(instance);}
    extension_=std::move(materialExtension);geometry_=std::move(geometry);material_=material;
    effective_=effective;textureBase_=textureBase;occlusion_=occlusion;return true;
  }
  void reset() {vertices_.reset();indices_.reset();instance_.reset();extension_.reset();geometry_.reset();}
  VkBuffer vertices() const {return vertices_.handle();}
  VkBuffer indices() const {return indices_.handle();}
  VkBuffer instance() const {return instance_.handle();}
  VkBuffer extension() const {return extension_.handle();}
  u32 indexCount() const {return geometry_?geometry_->draw.indexCount:0;}
  const renderer::MapMaterialRecord &material() const {return material_;}
  const scene::MaterialShading &shading() const {return effective_.shading;}
private:
  rhi::VulkanBuffer vertices_,indices_,instance_,extension_;
  std::shared_ptr<const renderer::MaterialPreviewGeometry> geometry_;
  renderer::MapMaterialRecord material_{};
  renderer::MaterialOverride effective_{};
  u32 textureBase_=0,occlusion_=renderer::MaterialExtensionNoTexture;
};
} // namespace ae::platform::android
