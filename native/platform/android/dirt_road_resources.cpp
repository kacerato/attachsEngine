#include "platform/android/dirt_road_resources.h"

#include "platform/android/android_texture_loader.h"
#include "renderer/spatial_render_chunks.h"
#include "renderer/environment_map.h"
#include "renderer/water_authoring_geometry.h"

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

bool decodeEnvironment(const std::vector<u8> &bytes, EnvironmentLighting &lighting,
                       renderer::EnvironmentMapDescription &mapDescription) {
  if (bytes.size() < 16 || readWord(bytes, 0) != 0x4E454541 ||
      readWord(bytes, 8) != bytes.size()) return false;
  const u32 version = readWord(bytes, 4);
  const usize valueCount = version == 1 ? 16 : (version == 2 || version == 3) ? 32 : 0;
  if (valueCount == 0 || !renderer::decodeEnvironmentMapDescription(bytes, mapDescription)) return false;

  // AEEN v2 owns all global visual parameters. Defaults only keep v1 projects
  // readable; they are migration data, not per-scene renderer constants.
  EnvironmentLighting decoded{};
  const float skyZenithCloudCoverage[4] = {0.08f, 0.30f, 0.72f, 0.46f};
  const float skyHorizonCloudDensity[4] = {0.62f, 0.79f, 1.08f, 0.88f};
  const float groundColorSaturation[4] = {0.75f, 0.82f, 0.60f, 1.0f};
  const float cloudLightWindSpeed[4] = {1.35f, 1.42f, 1.52f, 0.0035f};
  std::memcpy(decoded.skyZenithCloudCoverage, skyZenithCloudCoverage,
              sizeof(skyZenithCloudCoverage));
  std::memcpy(decoded.skyHorizonCloudDensity, skyHorizonCloudDensity,
              sizeof(skyHorizonCloudDensity));
  std::memcpy(decoded.groundColorSaturation, groundColorSaturation,
              sizeof(groundColorSaturation));
  std::memcpy(decoded.cloudLightWindSpeed, cloudLightWindSpeed,
              sizeof(cloudLightWindSpeed));

  float values[32]{};
  for (usize index = 0; index < valueCount; ++index)
    values[index] = std::bit_cast<float>(readWord(bytes, 16 + index * 4));
  if (!std::all_of(values, values + valueCount,
                   [](float value) { return std::isfinite(value); })) return false;
  std::memcpy(&decoded, values, valueCount * sizeof(float));
  if (decoded.sunDirectionIntensity[3] < 0 || decoded.ambientColorStrength[3] < 0 ||
      decoded.parameters[0] <= 0 || decoded.parameters[2] < 0 ||
      decoded.groundColorSaturation[3] < 0) return false;
  const float sunLength = std::sqrt(decoded.sunDirectionIntensity[0] *
                                        decoded.sunDirectionIntensity[0] +
                                    decoded.sunDirectionIntensity[1] *
                                        decoded.sunDirectionIntensity[1] +
                                    decoded.sunDirectionIntensity[2] *
                                        decoded.sunDirectionIntensity[2]);
  if (sunLength < 1.0e-5f) return false;
  for (u32 axis = 0; axis < 3; ++axis)
    decoded.sunDirectionIntensity[axis] /= sunLength;
  // parameters.w was reserved in AEEN v1/v2. It is runtime-derived instead of
  // duplicated in the serialized float payload so legacy lighting stays bit-compatible.
  decoded.parameters[3] = mapDescription.hasPrefilteredSpecular() ? 1.0f : 0.0f;
  if (mapDescription.hasPrefilteredSpecular())
    decoded.parameters[2] = static_cast<float>(mapDescription.specularMipLevels - 1);
  lighting = decoded;
  return true;
}
}

namespace {

// Reindexa a grade de água com salto, sobre o buffer já montado.
//
// A malha assada tem 256 segmentos fixos (tools/build-ocean-demo.py) e a
// política escolhe a densidade por perfil e estado de mar. Trocar a malha
// inteira significaria reconstruir o buffer de vértices compartilhado e
// reindexar barco, caixas e fundo junto — muito risco para o ganho. Aqui os
// vértices ficam onde estão e só os índices da água mudam: os não referenciados
// continuam ocupando memória, e o que se compra é rasterização, que foi o que a
// medição apontou como o custo.
//
// Silencioso quando não há o que fazer: sem grade de água, com densidade zero,
// ou quando o salto resultante é 1, o buffer sai como entrou.
void decimateWaterGrid(renderer::SpatialRenderChunks &chunks,
                       const std::vector<renderer::MapMaterialRecord> &materials,
                       std::vector<renderer::MapDrawRecord> &draws,
                       u32 targetSegments) {
  if (targetSegments == 0) return;
  constexpr u32 kCameraGrid =
      renderer::MapMaterialWater | renderer::MapMaterialWaterCameraGrid;
  for (auto &draw : draws) {
    if (draw.materialIndex >= materials.size()) continue;
    if ((materials[draw.materialIndex].flags & kCameraGrid) != kCameraGrid) continue;
    if (draw.indexCount % 6u != 0u) continue;

    // Segmentos da grade assada, deduzidos da contagem: seis índices por célula
    // e a grade é quadrada. Se a conta não fechar num quadrado perfeito, o draw
    // não é a grade que este código sabe reindexar, e ele fica como está.
    const u32 cells = draw.indexCount / 6u;
    u32 baked = 1u;
    while (baked * baked < cells) ++baked;
    if (baked * baked != cells) continue;

    const u32 step = renderer::waterGridDecimation(baked, targetSegments);
    if (step <= 1u) continue;

    // O primeiro índice do draw aponta para o canto da grade; decimar preserva
    // esse canto, então a reescrita cabe no espaço que o draw já ocupa.
    const u32 base = chunks.indices[draw.firstIndex];
    usize written = 0;
    if (!renderer::decimateWaterGridIndices(baked, step, base,
                                            chunks.indices.data() + draw.firstIndex,
                                            draw.indexCount, written)) continue;
    __android_log_print(ANDROID_LOG_INFO, LogTag,
        "[Water] grade %u seg -> %u seg (salto %u): %u indices, %.0f%% do original.",
        baked, baked / step, step, static_cast<u32>(written),
        100.0 * static_cast<double>(written) / static_cast<double>(draw.indexCount));
    draw.indexCount = static_cast<u32>(written);
  }
}

} // namespace

bool DirtRoadResources::initialize(rhi::VulkanDevice &device, rhi::VulkanUploadContext &upload,
                                   AAssetManager *assets, bool forceTextureFallback,
                                   float waterDisplacementAllowance,
                                   const std::atomic<bool> *cancel,
                                   const char *assetRoot,
                                   u32 waterGridSegments, bool waterAuthoring) {
  if (assets == nullptr || assetRoot == nullptr || assetRoot[0] == '\0' || !images_.empty()) return false;
  const auto started = std::chrono::steady_clock::now();
  std::vector<u8> packageBytes;
  char assetPath[160]{};
  std::snprintf(assetPath, sizeof(assetPath), "%s/scene.aemap", assetRoot);
  if (!readAndroidAsset(assets, assetPath, packageBytes, cancel)) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "[DirtRoad] scene.aemap ausente ou truncado.");
    return false;
  }
  renderer::MapPackageView package;
  if (!renderer::decodeMapPackage(packageBytes, package)) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "[DirtRoad] pacote AEMAP inválido.");
    return false;
  }
  header_ = package.header;
  packageFingerprint_ = package.contentFingerprint;
  textureRecords_.assign(package.textures.begin(), package.textures.end());
  materials_.assign(package.materials.begin(), package.materials.end());
  if (!renderer::buildStaticCollisionMesh(package, collisionMesh_)) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag,
                        "[DirtRoad] falha ao construir colisão estática mundial.");
    return false;
  }
  renderer::SpatialRenderChunks renderChunks;
  renderer::SpatialRenderChunkSettings chunkSettings{};
  chunkSettings.waterDisplacementAllowance = waterDisplacementAllowance;
  if (!renderer::buildSpatialRenderChunks(package, chunkSettings, renderChunks)) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag,
                        "[DirtRoad] falha ao construir render chunks espaciais.");
    return false;
  }
  draws_ = std::move(renderChunks.draws);
  decimateWaterGrid(renderChunks, materials_, draws_, waterGridSegments);

  std::vector<u8> authoringVertices;
  if(waterAuthoring) {
    authoringVertices.assign(package.vertices.begin(),package.vertices.end());
    if(!renderer::appendWaterAuthoringGeometry(header_.vertexStride,
        waterGridSegments?waterGridSegments:128,authoringVertices,renderChunks.indices,draws_,materials_)) return false;
  }
  const std::span<const u8> vertexData=waterAuthoring?std::span<const u8>(authoringVertices):package.vertices;

  auto &allocator = device.memoryAllocator();
  rhi::BufferDesc buffer{};
  buffer.preferDeviceMemory = true;
  buffer.cpuAccess = rhi::CpuAccess::None;
  buffer.sizeBytes = vertexData.size();
  buffer.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  if (!allocator.createBuffer(buffer, &vertices_) ||
      !upload.uploadBuffer(allocator, vertexData.data(), vertexData.size(), vertices_,
                           VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) return false;
  buffer.sizeBytes = renderChunks.indices.size() * sizeof(u32);
  buffer.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  if (!allocator.createBuffer(buffer, &indices_) ||
      !upload.uploadBuffer(allocator, renderChunks.indices.data(),
                           renderChunks.indices.size() * sizeof(u32), indices_,
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
    std::snprintf(name, sizeof(name), astc ? "%s/texture_%03u.aetex"
                                           : "%s/texture_%03u-fallback.aetex", assetRoot, index);
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
  std::snprintf(assetPath, sizeof(assetPath), "%s/environment.aeenv", assetRoot);
  if (!readAndroidAsset(assets, assetPath, environmentBytes, cancel) ||
      !decodeEnvironment(environmentBytes, environmentLighting_, environmentMapDescription_)) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "[Environment] Metadados AEEN inválidos.");
    return false;
  }
  rhi::SamplerDesc environmentSampling{};
  environmentSampling.minFilter = VK_FILTER_LINEAR;
  environmentSampling.magFilter = VK_FILTER_LINEAR;
  environmentSampling.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  environmentSampling.addressU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  environmentSampling.addressV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  std::snprintf(assetPath, sizeof(assetPath), "%s/environment.aetex", assetRoot);
  if (!loadAndroidTexture(device, upload, assets, assetPath, maxDimension,
                          128 * Megabyte, environmentSampling, environmentImage_,
                          environmentSampler_, cancel, "Environment")) return false;
  if (environmentMapDescription_.hasPrefilteredSpecular()) {
    rhi::SamplerDesc specularSampling{};
    specularSampling.minFilter = VK_FILTER_LINEAR;
    specularSampling.magFilter = VK_FILTER_LINEAR;
    specularSampling.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    specularSampling.addressU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    specularSampling.addressV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    std::snprintf(assetPath, sizeof(assetPath), "%s/environment-specular.aetex", assetRoot);
    if (!loadAndroidTexture(device, upload, assets, assetPath,
                            std::min(maxDimension, environmentMapDescription_.specularWidth),
                            8 * Megabyte, specularSampling, environmentSpecularImage_,
                            environmentSpecularSampler_, cancel, "Environment/Specular")) return false;
  }
  if (environmentMapDescription_.hasSplitSumBrdf()) {
    rhi::SamplerDesc brdfSampling{};
    brdfSampling.minFilter = VK_FILTER_LINEAR;
    brdfSampling.magFilter = VK_FILTER_LINEAR;
    brdfSampling.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    brdfSampling.addressU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    brdfSampling.addressV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    std::snprintf(assetPath, sizeof(assetPath), "%s/environment-brdf.aetex", assetRoot);
    if (!loadAndroidTexture(device, upload, assets, assetPath,
                            std::min(maxDimension, environmentMapDescription_.brdfWidth),
                            2 * Megabyte, brdfSampling, environmentBrdfImage_,
                            environmentBrdfSampler_, cancel, "Environment/BRDF")) return false;
  }
  __android_log_print(ANDROID_LOG_INFO, LogTag,
      "[Environment] projection=%s specular=%ux%u/%u brdf=%ux%u split_sum=%s",
      environmentMapDescription_.specularProjection == renderer::EnvironmentProjection::Octahedral
          ? "octahedral" : "equirectangular",
      environmentMapDescription_.specularWidth, environmentMapDescription_.specularHeight,
      environmentMapDescription_.specularMipLevels, environmentMapDescription_.brdfWidth,
      environmentMapDescription_.brdfHeight,
      environmentMapDescription_.hasSplitSumBrdf() ? "true" : "false");
  __android_log_print(ANDROID_LOG_INFO, LogTag,
      "[DirtRoad] ready assets=%s source_draws=%u render_chunks=%zu materials=%u textures=%u triangles=%u encoding=%s load_ms=%.3f",
      assetRoot, header_.drawCount, draws_.size(), header_.materialCount, header_.textureCount,
      header_.triangleCount,
      astc ? "ASTC6x6" : "RGBA8-fallback",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count());
  return true;
}

// Shared Vulkan resource upload backend; this entry never reads a demo package.
bool DirtRoadResources::initializePrimitives(rhi::VulkanDevice &device,rhi::VulkanUploadContext &upload) {
  if(vertices_.handle()!=VK_NULL_HANDLE || !draws_.empty()) return false;
  std::vector<u8> vertices;std::vector<u32> indices;
  if(!renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride,vertices,indices,draws_,materials_)) return false;
  auto &allocator=device.memoryAllocator();
  rhi::BufferDesc buffer{};buffer.preferDeviceMemory=true;buffer.cpuAccess=rhi::CpuAccess::None;
  buffer.sizeBytes=vertices.size();buffer.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  if(!allocator.createBuffer(buffer,&vertices_) || !upload.uploadBuffer(allocator,vertices.data(),vertices.size(),vertices_,VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) return false;
  buffer.sizeBytes=indices.size()*sizeof(u32);buffer.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  if(!allocator.createBuffer(buffer,&indices_) || !upload.uploadBuffer(allocator,indices.data(),buffer.sizeBytes,indices_,VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,VK_ACCESS_INDEX_READ_BIT)) return false;
  pickingVertices_=std::move(vertices);pickingIndices_=std::move(indices);
  // Neutral editor preview lighting, not a hidden authored light or demo HDRI.
  environmentLighting_={};
  environmentLighting_.sunDirectionIntensity[0]=.4082483f;
  environmentLighting_.sunDirectionIntensity[1]=.8164966f;
  environmentLighting_.sunDirectionIntensity[2]=.4082483f;
  environmentLighting_.sunDirectionIntensity[3]=2;
  for(u32 i=0;i<3;++i) {
    environmentLighting_.sunColorAngularRadius[i]=1;
    environmentLighting_.ambientColorStrength[i]=.5f;
    environmentLighting_.skyZenithCloudCoverage[i]=.15f;
    environmentLighting_.skyHorizonCloudDensity[i]=.15f;
    environmentLighting_.groundColorSaturation[i]=.15f;
  }
  environmentLighting_.ambientColorStrength[3]=1;
  environmentLighting_.groundColorSaturation[3]=1;
  environmentLighting_.parameters[0]=1;
  const u8 pixel[]{128,128,128,255};
  rhi::ImageDesc image{};image.width=image.height=1;image.format=VK_FORMAT_R8G8B8A8_UNORM;
  image.usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
  image.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;image.memoryClass=rhi::MemoryClass::Texture;
  if(!allocator.createImage(image,&environmentImage_) || !upload.uploadRgba8ToSampledImage(allocator,pixel,sizeof(pixel),environmentImage_)) return false;
  rhi::SamplerDesc sampler{};
  if(!environmentSampler_.initialize(device.handle(),sampler)) return false;
  header_={};header_.vertexStride=renderer::MapVertexStride;packageFingerprint_=0;
  __android_log_print(ANDROID_LOG_INFO,LogTag,"[Authoring] primitive_library=1 packages=0 water=0 authored_objects=0");
  return true;
}

platform::FreeCameraState DirtRoadResources::defaultCamera() const {
  platform::FreeCameraState state{};
  std::memcpy(state.position, header_.defaultCameraPosition, sizeof(state.position));
  state.yaw = header_.defaultCameraYaw;
  state.pitch = header_.defaultCameraPitch;
  return state;
}

platform::FreeCameraState DirtRoadResources::defaultGameplayCamera() const {
  platform::FreeCameraState state{};
  state.position[0] = (header_.boundsMinimum[0] + header_.boundsMaximum[0]) * 0.5f;
  // The importer already chooses a useful eye-height from the complete map
  // bounds. Reuse that authored/global height while moving X/Z into the map;
  // using boundsMinimum put the player below sloped terrain in this asset.
  state.position[1] = header_.defaultCameraPosition[1];
  state.position[2] = (header_.boundsMinimum[2] + header_.boundsMaximum[2]) * 0.5f;
  state.yaw = header_.defaultCameraYaw;
  state.pitch = 0.0f;
  return state;
}

void DirtRoadResources::shutdown() {
  environmentBrdfSampler_.shutdown();
  environmentBrdfImage_.reset();
  environmentSpecularSampler_.shutdown();
  environmentSpecularImage_.reset();
  environmentSampler_.shutdown();
  environmentImage_.reset();
  environmentLighting_ = {};
  environmentMapDescription_ = {};
  collisionMesh_.clear();
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
  packageFingerprint_ = 0;
  pickingVertices_.clear();pickingIndices_.clear();
}

} // namespace ae::platform::android
