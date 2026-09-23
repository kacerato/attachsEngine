#include "platform/android/material_preview_resources.h"
#include "platform/android/android_texture_loader.h"
#include "renderer/sphere_mesh.h"
#include "renderer/texture_payload.h"
#include <android/log.h>
#include <array>
#include <vector>
#include <cstring>
#include <chrono>

namespace ae::platform::android {
bool MaterialPreviewResources::initialize(rhi::VulkanDevice &device,rhi::VulkanUploadContext &upload,
    AAssetManager *assets,bool forceTextureFallback,const std::atomic<bool> *cancel) {
  if(assets==nullptr)return false;
  const auto started=std::chrono::steady_clock::now();
  renderer::MeshData mesh;
  if(!renderer::makeUvSphere(192,96,mesh))return false;
  auto &allocator=device.memoryAllocator();
  rhi::BufferDesc desc{};
  desc.cpuAccess=rhi::CpuAccess::SequentialWrite;desc.preferDeviceMemory=false;
  desc.sizeBytes=mesh.vertices.size()*sizeof(renderer::MeshVertex);desc.usage=VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  if(!allocator.createBuffer(desc,&vertices_))return false;
  std::memcpy(vertices_.mappedData(),mesh.vertices.data(),desc.sizeBytes);
  if(!allocator.flushBuffer(vertices_))return false;
  desc.sizeBytes=mesh.indices.size()*sizeof(u32);desc.usage=VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  if(!allocator.createBuffer(desc,&indices_))return false;
  std::memcpy(indices_.mappedData(),mesh.indices.data(),desc.sizeBytes);
  if(!allocator.flushBuffer(indices_))return false;
  indexCount_=static_cast<u32>(mesh.indices.size());
  VkPhysicalDeviceProperties props{};vkGetPhysicalDeviceProperties(device.physicalDevice(),&props);
  VkPhysicalDeviceFeatures features{};vkGetPhysicalDeviceFeatures(device.physicalDevice(),&features);
  const bool astc=features.textureCompressionASTC_LDR && !forceTextureFallback;
  const auto budget=allocator.budgetSnapshot().entries[static_cast<usize>(rhi::MemoryClass::Texture)];
  const u64 available=budget.limitBytes>budget.usedBytes?budget.limitBytes-budget.usedBytes:0;
  // Leave headroom for VMA alignment/other resources; budget is a cap, not a promise.
  const u64 perMapBudget=std::min<u64>(128ull*1024*1024,available*8/10)/3;
  const u32 maxDimension=std::min(8192u,props.limits.maxImageDimension2D);
  const char *names[5]={"material_preview/albedo.aetex","material_preview/normal.aetex",
      "material_preview/arm.aetex","material_preview/studio.aetex","material_preview/brdf.aetex"};
  const char *fallback[3]={"material_preview/albedo-fallback.aetex","material_preview/normal-fallback.aetex",
      "material_preview/arm-fallback.aetex"};
  for(u32 i=0;i<TextureCount;++i) {
    if(cancel && cancel->load())return false;
    rhi::SamplerDesc sampling{};
    if(i>=3) {
      sampling.addressV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      if(i==4)sampling.addressU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    }
    if(!loadAndroidTexture(device,upload,assets,!astc&&i<3?fallback[i]:names[i],maxDimension,
        i<3?perMapBudget:4ull*1024*1024,sampling,true,images_[i],samplers_[i],cancel,"MaterialPreview")) {
      if(!cancel || !cancel->load())
        __android_log_print(ANDROID_LOG_ERROR,"Aether.Android","[MaterialPreview] Falha de upload no mapa %u",i);
      return false;
    }
  }
  __android_log_print(ANDROID_LOG_INFO,"Aether.Android",
      "[MaterialPreview] ready triangles=%u textures=%s load_ms=%.3f",indexCount_/3,astc?"ASTC6x6":"RGBA8-fallback",
      std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count());
  return true;
}
void MaterialPreviewResources::shutdown() {
  for(auto &sampler:samplers_)sampler.shutdown();
  for(auto &image:images_)image.reset();
  indices_.reset();vertices_.reset();indexCount_=0;
}
}
