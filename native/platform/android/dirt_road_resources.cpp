#include "platform/android/dirt_road_resources.h"

#include "platform/android/android_texture_loader.h"

#include <android/log.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <bit>
#include <cmath>

namespace ae::platform::android {
namespace {
constexpr const char *LogTag = "Aether.Android";
constexpr u64 Megabyte = 1024ull * 1024;

VkSamplerAddressMode addressMode(bool clamp) {
  return clamp ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

u32 readWord(const std::vector<u8> &bytes, usize offset) {
  return static_cast<u32>(bytes[offset]) | static_cast<u32>(bytes[offset + 1]) << 8 |
         static_cast<u32>(bytes[offset + 2]) << 16 | static_cast<u32>(bytes[offset + 3]) << 24;
}

bool decodeEnvironment(const std::vector<u8> &bytes, EnvironmentLighting &lighting) {
  if (bytes.size() != 80 || readWord(bytes, 0) != 0x4E454541 || readWord(bytes, 4) != 1 ||
      readWord(bytes, 8) != bytes.size()) return false;
  float values[16]{};
  for (usize index = 0; index < 16; ++index)
    values[index] = std::bit_cast<float>(readWord(bytes, 16 + index * 4));
  if (!std::all_of(values, values + 16, [](float value) { return std::isfinite(value); })) return false;
  std::memcpy(&lighting, values, sizeof(values));
  return
         lighting.sunDirectionIntensity[3] >= 0 && lighting.ambientColorStrength[3] >= 0 &&
         lighting.parameters[0] > 0 && lighting.parameters[2] >= 0;
}
}

bool DirtRoadResources::initialize(rhi::VulkanDevice &device, rhi::VulkanUploadContext &upload,
                                   AAssetManager *assets, bool forceTextureFallback,
                                   const std::atomic<bool> *cancel) {
  if (assets == nullptr || !images_.empty()) return false;
  const auto started = std::chrono::steady_clock::now();
  std::vector<u8> packageBytes;
  if (!readAndroidAsset(assets, "dirt_road/scene.aemap", packageBytes, cancel)) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "[DirtRoad] scene.aemap ausente ou truncado.");
    return false;
  }
  renderer::MapPackageView package;
  if (!renderer::decodeMapPackage(packageBytes, package)) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "[DirtRoad] pacote AEMAP inválido.");
    return false;
  }
  header_ = package.header;
  textureRecords_.assign(package.textures.begin(), package.textures.end());
  materials_.assign(package.materials.begin(), package.materials.end());
  draws_.assign(package.draws.begin(), package.draws.end());

  auto &allocator = device.memoryAllocator();
  rhi::BufferDesc buffer{};
  buffer.preferDeviceMemory = true;
  buffer.cpuAccess = rhi::CpuAccess::None;
  buffer.sizeBytes = package.vertices.size();
  buffer.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  if (!allocator.createBuffer(buffer, &vertices_) ||
      !upload.uploadBuffer(allocator, package.vertices.data(), package.vertices.size(), vertices_,
                           VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) return false;
  buffer.sizeBytes = package.indices.size_bytes();
  buffer.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  if (!allocator.createBuffer(buffer, &indices_) ||
      !upload.uploadBuffer(allocator, package.indices.data(), package.indices.size_bytes(), indices_,
                           VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_INDEX_READ_BIT)) return false;

  VkPhysicalDeviceFeatures features{};
  vkGetPhysicalDeviceFeatures(device.physicalDevice(), &features);
  const bool astc = features.textureCompressionASTC_LDR && !forceTextureFallback;
  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(device.physicalDevice(), &properties);
  const auto budget = allocator.budgetSnapshot().entries[static_cast<usize>(rhi::MemoryClass::Texture)];
  const u64 available = budget.limitBytes > budget.usedBytes ? budget.limitBytes - budget.usedBytes : 0;
  const u64 safeTotal = std::min<u64>(128 * Megabyte, available * 7 / 10);
  // Full ASTC set is ~53 MiB. If memory cannot hold it, each texture chooses a
  // lower resident base mip using the same deterministic per-resource policy.
  const u64 perTextureBudget = safeTotal >= 64 * Megabyte
                                   ? 32 * Megabyte
                                   : std::max<u64>(64 * 1024, safeTotal / header_.textureCount);
  const u32 maxDimension = std::min(4096u, properties.limits.maxImageDimension2D);
  images_.resize(header_.textureCount);
  samplers_.resize(header_.textureCount);
  for (u32 index = 0; index < header_.textureCount; ++index) {
    if (cancel != nullptr && cancel->load()) return false;
    char name[96];
    std::snprintf(name, sizeof(name), astc ? "dirt_road/texture_%03u.aetex"
                                           : "dirt_road/texture_%03u-fallback.aetex", index);
    const u32 flags = textureRecords_[index].flags;
    rhi::SamplerDesc sampling{};
    sampling.minFilter = (flags & 1u) != 0 ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    sampling.magFilter = sampling.minFilter;
    sampling.mipmapMode = (flags & 2u) != 0 ? VK_SAMPLER_MIPMAP_MODE_LINEAR
                                             : VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampling.addressU = addressMode((flags & 4u) != 0);
    sampling.addressV = addressMode((flags & 8u) != 0);
    if (!loadAndroidTexture(device, upload, assets, name, maxDimension, perTextureBudget,
                            sampling, images_[index], samplers_[index], cancel, "DirtRoad")) return false;
  }
  std::vector<u8> environmentBytes;
  if (!readAndroidAsset(assets, "dirt_road/environment.aeenv", environmentBytes, cancel) ||
      !decodeEnvironment(environmentBytes, environmentLighting_)) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "[Environment] Metadados AEEN inválidos.");
    return false;
  }
  rhi::SamplerDesc environmentSampling{};
  environmentSampling.minFilter = VK_FILTER_LINEAR;
  environmentSampling.magFilter = VK_FILTER_LINEAR;
  environmentSampling.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  environmentSampling.addressU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  environmentSampling.addressV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  if (!loadAndroidTexture(device, upload, assets, "dirt_road/environment.aetex", maxDimension,
                          128 * Megabyte, environmentSampling, environmentImage_,
                          environmentSampler_, cancel, "Environment")) return false;
  __android_log_print(ANDROID_LOG_INFO, LogTag,
      "[DirtRoad] ready draws=%u materials=%u textures=%u triangles=%u encoding=%s load_ms=%.3f",
      header_.drawCount, header_.materialCount, header_.textureCount, header_.triangleCount,
      astc ? "ASTC6x6" : "RGBA8-fallback",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count());
  return true;
}

platform::FreeCameraState DirtRoadResources::defaultCamera() const {
  platform::FreeCameraState state{};
  std::memcpy(state.position, header_.defaultCameraPosition, sizeof(state.position));
  state.yaw = header_.defaultCameraYaw;
  state.pitch = header_.defaultCameraPitch;
  return state;
}

void DirtRoadResources::shutdown() {
  environmentSampler_.shutdown();
  environmentImage_.reset();
  environmentLighting_ = {};
  for (auto &sampler : samplers_) sampler.shutdown();
  for (auto &image : images_) image.reset();
  samplers_.clear();
  images_.clear();
  indices_.reset();
  vertices_.reset();
  draws_.clear();
  materials_.clear();
  textureRecords_.clear();
  header_ = {};
}

} // namespace ae::platform::android
