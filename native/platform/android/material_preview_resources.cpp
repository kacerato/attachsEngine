#include "platform/android/material_preview_resources.h"
#include "renderer/sphere_mesh.h"
#include "renderer/texture_payload.h"
#include <android/log.h>
#include <array>
#include <memory>
#include <vector>
#include <cstring>
#include <chrono>

namespace ae::platform::android {
namespace {
using Asset=std::unique_ptr<AAsset,decltype(&AAsset_close)>;
bool readExact(AAsset *asset,void *destination,usize size,const std::atomic<bool> *cancel=nullptr) {
  auto *bytes=static_cast<u8*>(destination);
  while(size) {
    if(cancel && cancel->load())return false;
    const int n=AAsset_read(asset,bytes,std::min<usize>(size,1024*1024));
    if(n<=0)return false;
    bytes+=n;size-=static_cast<usize>(n);
  }
  return true;
}
bool loadTexture(rhi::VulkanDevice &device,rhi::VulkanUploadContext &upload,AAssetManager *assets,
    const char *name,u32 maxDimension,u64 budget,rhi::VulkanImage &image,rhi::VulkanSampler &sampler,
    const std::atomic<bool> *cancel) {
  Asset asset(AAssetManager_open(assets,name,AASSET_MODE_RANDOM),AAsset_close);
  if(!asset) { __android_log_print(ANDROID_LOG_ERROR,"Aether.Android","[MaterialPreview] Asset ausente: %s",name);return false; }
  std::array<u8,32> header{};
  renderer::TexturePayload payload;
  if(!readExact(asset.get(),header.data(),header.size()) ||
      !renderer::decodeTextureHeader(header,AAsset_getLength64(asset.get()),payload)) return false;
  auto desc=payload.description;
  VkFormatProperties props{};
  vkGetPhysicalDeviceFormatProperties(device.physicalDevice(),desc.format,&props);
  constexpr auto required=VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
  if((props.optimalTilingFeatures&required)!=required) return false;
  const u32 baseMip=renderer::chooseResidentMip(desc,maxDimension,budget);
  if(baseMip==desc.mipLevels)return false;
  u64 offset=32;
  for(u32 mip=0;mip<baseMip;++mip) {
    offset+=rhi::sampledMipByteSize(desc.format,desc.width,desc.height);
    desc.width=std::max(1u,desc.width/2);desc.height=std::max(1u,desc.height/2);--desc.mipLevels;
  }
  const u64 size=rhi::sampledChainByteSize(desc);
  if(AAsset_seek64(asset.get(),static_cast<off64_t>(offset),SEEK_SET)!=static_cast<off64_t>(offset))return false;
  std::vector<u8> bytes(static_cast<usize>(size));
  if(!readExact(asset.get(),bytes.data(),bytes.size(),cancel) || (cancel && cancel->load()) ||
      !device.memoryAllocator().createImage(desc,&image) ||
      !upload.uploadSampledMipChain(device.memoryAllocator(),bytes.data(),size,image))return false;
  rhi::SamplerDesc sampling{};
  sampling.maxLod=static_cast<float>(desc.mipLevels-1);
  // Lat-long environment repeats U, clamps V; BRDF LUT clamps both axes.
  if(desc.format==VK_FORMAT_R16G16B16A16_SFLOAT) {
    sampling.addressV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if(desc.mipLevels==1)sampling.addressU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  }
  if(!sampler.initialize(device.handle(),sampling))return false;
  __android_log_print(ANDROID_LOG_INFO,"Aether.Android",
      "[MaterialPreview] texture=%s resident=%ux%u mips=%u bytes=%llu",name,desc.width,desc.height,desc.mipLevels,
      static_cast<unsigned long long>(size));
  return true;
}
}
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
    if(!loadTexture(device,upload,assets,!astc&&i<3?fallback[i]:names[i],maxDimension,
        i<3?perMapBudget:4ull*1024*1024,images_[i],samplers_[i],cancel)) {
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
