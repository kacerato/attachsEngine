#include "platform/android/astc_encode_probe.h"

#include "rhi/compute.h"
#include "rhi/shaders/astc_encode_spirv.h"

#include <android/log.h>

#include <cmath>
#include "profiler/gpu_timestamp.h"
#include "platform/astc_probe_validation.h"
#include <cstring>
#include <vector>

namespace ae::platform::android {

namespace {

constexpr const char *LogTag = "Aether.AstcProbe";

// Todo o estado Vulkan deste probe é local e descartado ao final — nunca compartilhado com o
// shell gráfico de produção, que só empresta VkDevice/VkPhysicalDevice/fila já inicializados.
struct ProbeResources {
  VkDevice device = VK_NULL_HANDLE;
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  u32 queueFamily = 0;

  VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
  VkQueryPool queryPool = VK_NULL_HANDLE;
  VkSemaphore computeToGraphics = VK_NULL_HANDLE;
  rhi::VulkanComputeContext computeContext;
  rhi::VulkanComputeContext graphicsContext;
  rhi::VulkanComputeKernel computeKernel;

  VkImage sourceImage = VK_NULL_HANDLE;
  VkDeviceMemory sourceImageMemory = VK_NULL_HANDLE;
  VkImageView sourceImageView = VK_NULL_HANDLE;

  VkBuffer outputBuffer = VK_NULL_HANDLE;
  VkDeviceMemory outputBufferMemory = VK_NULL_HANDLE;
  void *outputBufferMapped = nullptr;

  // Recursos ASTC para validação por hardware decode (blit ASTC->RGBA8 pelo próprio driver).
  VkImage astcImage = VK_NULL_HANDLE;
  VkDeviceMemory astcImageMemory = VK_NULL_HANDLE;
  VkImage readbackImage = VK_NULL_HANDLE;
  VkDeviceMemory readbackImageMemory = VK_NULL_HANDLE;
  VkBuffer readbackBuffer = VK_NULL_HANDLE;
  VkDeviceMemory readbackBufferMemory = VK_NULL_HANDLE;
  void *readbackBufferMapped = nullptr;

  VkBuffer stagingBuffer = VK_NULL_HANDLE;
  VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
  void *stagingMapped = nullptr;
  ~ProbeResources() { destroy(); }

  bool check(VkResult result, const char *operation) const {
    if (result == VK_SUCCESS) return true;
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "%s falhou: VkResult=%d.", operation, result);
    return false;
  }

  bool submitAndWait() {
    return computeContext.submit() && computeContext.wait();
  }

  void destroy() {
    if (device == VK_NULL_HANDLE) return;
    graphicsContext.shutdown();
    computeContext.shutdown();
    computeKernel.shutdown();
    if (computeToGraphics != VK_NULL_HANDLE) vkDestroySemaphore(device, computeToGraphics, nullptr);
    if (stagingMapped != nullptr) vkUnmapMemory(device, stagingMemory);
    if (stagingBuffer != VK_NULL_HANDLE) vkDestroyBuffer(device, stagingBuffer, nullptr);
    if (stagingMemory != VK_NULL_HANDLE) vkFreeMemory(device, stagingMemory, nullptr);
    if (queryPool != VK_NULL_HANDLE) vkDestroyQueryPool(device, queryPool, nullptr);
    if (sourceImageView != VK_NULL_HANDLE) vkDestroyImageView(device, sourceImageView, nullptr);
    if (sourceImage != VK_NULL_HANDLE) vkDestroyImage(device, sourceImage, nullptr);
    if (sourceImageMemory != VK_NULL_HANDLE) vkFreeMemory(device, sourceImageMemory, nullptr);
    if (outputBufferMapped != nullptr) vkUnmapMemory(device, outputBufferMemory);
    if (outputBuffer != VK_NULL_HANDLE) vkDestroyBuffer(device, outputBuffer, nullptr);
    if (outputBufferMemory != VK_NULL_HANDLE) vkFreeMemory(device, outputBufferMemory, nullptr);
    if (astcImage != VK_NULL_HANDLE) vkDestroyImage(device, astcImage, nullptr);
    if (astcImageMemory != VK_NULL_HANDLE) vkFreeMemory(device, astcImageMemory, nullptr);
    if (readbackImage != VK_NULL_HANDLE) vkDestroyImage(device, readbackImage, nullptr);
    if (readbackImageMemory != VK_NULL_HANDLE) vkFreeMemory(device, readbackImageMemory, nullptr);
    if (readbackBufferMapped != nullptr) vkUnmapMemory(device, readbackBufferMemory);
    if (readbackBuffer != VK_NULL_HANDLE) vkDestroyBuffer(device, readbackBuffer, nullptr);
    if (readbackBufferMemory != VK_NULL_HANDLE) vkFreeMemory(device, readbackBufferMemory, nullptr);
    device = VK_NULL_HANDLE;
  }
};

u32 findMemoryType(VkPhysicalDevice physicalDevice, u32 typeBits, VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties memProperties{};
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
  for (u32 i = 0; i < memProperties.memoryTypeCount; ++i) {
    if ((typeBits & (1u << i)) &&
        (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
      return i;
    }
  }
  return UINT32_MAX;
}

bool createHostVisibleBuffer(ProbeResources &res, VkDeviceSize sizeBytes, VkBufferUsageFlags usage,
                             VkBuffer *outBuffer, VkDeviceMemory *outMemory, void **outMapped) {
  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = sizeBytes;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  if (vkCreateBuffer(res.device, &bufferInfo, nullptr, outBuffer) != VK_SUCCESS) return false;

  VkMemoryRequirements memReq{};
  vkGetBufferMemoryRequirements(res.device, *outBuffer, &memReq);
  u32 memType = findMemoryType(res.physicalDevice, memReq.memoryTypeBits,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  if (memType == UINT32_MAX) return false;

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memReq.size;
  allocInfo.memoryTypeIndex = memType;
  if (vkAllocateMemory(res.device, &allocInfo, nullptr, outMemory) != VK_SUCCESS) return false;
  if (vkBindBufferMemory(res.device, *outBuffer, *outMemory, 0) != VK_SUCCESS) return false;
  if (outMapped != nullptr) {
    if (vkMapMemory(res.device, *outMemory, 0, sizeBytes, 0, outMapped) != VK_SUCCESS) return false;
  }
  return true;
}

bool createDeviceLocalImage(ProbeResources &res, u32 width, u32 height, VkFormat format,
                            VkImageUsageFlags usage, VkImage *outImage, VkDeviceMemory *outMemory) {
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.format = format;
  imageInfo.extent = {width, height, 1};
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.usage = usage;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  if (vkCreateImage(res.device, &imageInfo, nullptr, outImage) != VK_SUCCESS) return false;

  VkMemoryRequirements memReq{};
  vkGetImageMemoryRequirements(res.device, *outImage, &memReq);
  u32 memType = findMemoryType(res.physicalDevice, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (memType == UINT32_MAX) return false;

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memReq.size;
  allocInfo.memoryTypeIndex = memType;
  if (vkAllocateMemory(res.device, &allocInfo, nullptr, outMemory) != VK_SUCCESS) return false;
  return vkBindImageMemory(res.device, *outImage, *outMemory, 0) == VK_SUCCESS;
}

void transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
                           VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                           VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) {
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  barrier.srcAccessMask = srcAccess;
  barrier.dstAccessMask = dstAccess;
  vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

} // namespace

AstcEncodeProbeResult runAstcEncodeProbe(rhi::VulkanDevice &device, u32 textureWidth, u32 textureHeight) {
  AstcEncodeProbeResult result;
  if (device.handle() == VK_NULL_HANDLE || textureWidth == 0 || textureHeight == 0 ||
      textureWidth > 4096 || textureHeight > 4096) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Probe aceita dimensões entre 1 e 4096 e device válido.");
    return result;
  }

  ProbeResources res;
  res.device = device.handle();
  res.physicalDevice = device.physicalDevice();
  res.queueFamily = device.computeQueueFamily();

  u32 queueCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(res.physicalDevice, &queueCount, nullptr);
  std::vector<VkQueueFamilyProperties> queues(queueCount);
  vkGetPhysicalDeviceQueueFamilyProperties(res.physicalDevice, &queueCount, queues.data());
  if (res.queueFamily >= queueCount || !(queues[res.queueFamily].queueFlags & VK_QUEUE_COMPUTE_BIT) ||
      queues[res.queueFamily].timestampValidBits == 0) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Fila sem compute/timestamps: evidência indisponível.");
    return result;
  }
  VkPhysicalDeviceProperties deviceProps{};
  vkGetPhysicalDeviceProperties(res.physicalDevice, &deviceProps);
  if (textureWidth > deviceProps.limits.maxImageDimension2D ||
      textureHeight > deviceProps.limits.maxImageDimension2D ||
      !deviceProps.limits.timestampComputeAndGraphics) return result;

  const u32 blocksX = (textureWidth + 3) / 4;
  const u32 blocksY = (textureHeight + 3) / 4;
  const u32 blockCount = blocksX * blocksY;
  const VkDeviceSize outputBufferSize = static_cast<VkDeviceSize>(blockCount) * 16; // 16 bytes/bloco ASTC

  // ---------------- Comandos e sincronização ----------------
  if (!res.computeContext.initialize(res.device, device.computeQueue(), res.queueFamily)) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao inicializar contexto compute.");
    res.destroy();
    return result;
  }
  const bool separateGraphicsQueue = res.queueFamily != device.graphicsQueueFamily();
  if (separateGraphicsQueue &&
      !res.graphicsContext.initialize(res.device, device.graphicsQueue(),
                                      device.graphicsQueueFamily())) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao inicializar contexto grafico para readback.");
    res.destroy();
    return result;
  }
  if (separateGraphicsQueue) {
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    if (vkCreateSemaphore(res.device, &semaphoreInfo, nullptr, &res.computeToGraphics) != VK_SUCCESS) {
      __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar semaforo compute->graphics.");
      res.destroy();
      return result;
    }
  }
  res.commandBuffer = res.computeContext.commandBuffer();

  VkQueryPoolCreateInfo queryPoolInfo{};
  queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
  queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
  queryPoolInfo.queryCount = 2;
  if (vkCreateQueryPool(res.device, &queryPoolInfo, nullptr, &res.queryPool) != VK_SUCCESS) {
    res.destroy();
    return result;
  }

  // ---------------- Imagem de origem (RGBA8, storage) ----------------
  if (!createDeviceLocalImage(res, textureWidth, textureHeight, VK_FORMAT_R8G8B8A8_UNORM,
                              VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                              &res.sourceImage, &res.sourceImageMemory)) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar imagem de origem.");
    res.destroy();
    return result;
  }

  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = res.sourceImage;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
  viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  if (vkCreateImageView(res.device, &viewInfo, nullptr, &res.sourceImageView) != VK_SUCCESS) {
    res.destroy();
    return result;
  }

  // Staging: sobe o padrão de teste RGBA8 via buffer host-visible + vkCmdCopyBufferToImage —
  // mesma técnica de VulkanUploadContext, reimplementada aqui localmente porque este probe é
  // standalone e não compartilha estado com o upload context do shell.
  std::vector<u8> testPixels(static_cast<usize>(textureWidth) * textureHeight * 4);
  if (!ae::platform::fillAstcProbePattern(testPixels, textureWidth, textureHeight)) return result;
  VkDeviceSize stagingSize = static_cast<VkDeviceSize>(testPixels.size());

  if (!createHostVisibleBuffer(res, stagingSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &res.stagingBuffer, &res.stagingMemory, &res.stagingMapped)) {
    res.destroy();
    return result;
  }
  std::memcpy(res.stagingMapped, testPixels.data(), testPixels.size());

  // ---------------- Buffer de saída dos blocos ASTC ----------------
  if (!createHostVisibleBuffer(res, outputBufferSize,
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                               &res.outputBuffer, &res.outputBufferMemory, &res.outputBufferMapped)) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar buffer de saída.");
    res.destroy();
    return result;
  }

  // ---------------- Pipeline de compute ----------------
  const rhi::ComputeBindingDesc bindings[] = {
      {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
      {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
  };
  rhi::ComputeKernelDesc kernelDesc;
  kernelDesc.spirv = rhi::shaders::kAstc_EncodeCompSpirv;
  kernelDesc.spirvBytes = rhi::shaders::kAstc_EncodeCompSpirvSize;
  kernelDesc.bindings = bindings;
  kernelDesc.bindingCount = 2;
  kernelDesc.pushConstantBytes = sizeof(u32);
  kernelDesc.debugName = "AstcEncode4x4";
  if (!res.computeKernel.initialize(res.device, device.computeLimits(), kernelDesc,
                                    device.pipelineCache().driverHandle()) ||
      !res.computeKernel.writeImage(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                    res.sourceImageView, VK_IMAGE_LAYOUT_GENERAL) ||
      !res.computeKernel.writeBuffer(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                     res.outputBuffer, outputBufferSize)) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar/bindar kernel compute ASTC.");
    res.destroy();
    return result;
  }
  device.setObjectName(VK_OBJECT_TYPE_PIPELINE,
                       reinterpret_cast<u64>(res.computeKernel.handle()), "Compute.AstcEncode4x4");
  device.setObjectName(VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                       reinterpret_cast<u64>(res.computeKernel.layout()), "Compute.AstcEncode4x4.Layout");

  // ---------------- Gravação e submissão ----------------
  res.commandBuffer = res.computeContext.begin();
  if (res.commandBuffer == VK_NULL_HANDLE) return result;

  vkCmdResetQueryPool(res.commandBuffer, res.queryPool, 0, 2);

  transitionImageLayout(res.commandBuffer, res.sourceImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

  VkBufferImageCopy copyRegion{};
  copyRegion.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  copyRegion.imageExtent = {textureWidth, textureHeight, 1};
  vkCmdCopyBufferToImage(res.commandBuffer, res.stagingBuffer, res.sourceImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

  transitionImageLayout(res.commandBuffer, res.sourceImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

  vkCmdWriteTimestamp(res.commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, res.queryPool, 0);

  // local_size 8x8 no shader — arredonda para cima o número de grupos de trabalho.
  device.cmdBeginDebugLabel(res.commandBuffer, "Compute/AstcEncode4x4", 0.34f, 0.70f, 0.92f);
  const bool dispatchRecorded = res.computeKernel.recordDispatch(
      res.commandBuffer, {(blocksX + 7) / 8, (blocksY + 7) / 8, 1},
      &blocksX, sizeof(blocksX));
  device.cmdEndDebugLabel(res.commandBuffer);
  if (!dispatchRecorded) return result;
  if (separateGraphicsQueue) {
    rhi::cmdComputeBufferBarrier(res.commandBuffer, res.outputBuffer, 0, outputBufferSize,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_ACCESS_SHADER_WRITE_BIT,
                                 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0,
                                 res.queueFamily, device.graphicsQueueFamily());
  } else {
    rhi::cmdComputeMemoryBarrier(res.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_ACCESS_TRANSFER_READ_BIT);
  }

  vkCmdWriteTimestamp(res.commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, res.queryPool, 1);

  if (separateGraphicsQueue) {
    rhi::VulkanComputeContext::SubmitSync sync{};
    sync.signalSemaphores = &res.computeToGraphics; sync.signalCount = 1;
    if (!res.computeContext.submit(sync) || !res.computeContext.wait()) return result;
  } else if (!res.submitAndWait()) return result;

  u64 timestamps[2] = {0, 0};
  VkResult queryResult = vkGetQueryPoolResults(res.device, res.queryPool, 0, 2, sizeof(timestamps), timestamps,
                                               sizeof(u64), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
  result.timingValid = queryResult == VK_SUCCESS && ae::profiler::gpuTimestampMilliseconds(
      timestamps[0], timestamps[1], queues[res.queueFamily].timestampValidBits,
      deviceProps.limits.timestampPeriod, result.encodeMilliseconds);
  if (!result.timingValid) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Timestamp inválido: evidência de desempenho rejeitada.");
    return result;
  }
  result.blockCount = blockCount;

  // ---------------- Validação via hardware decode ----------------
  // Cria uma imagem ASTC_4x4 real, sobe os blocos gerados pelo compute shader, e usa vkCmdBlitImage
  // (que decodifica ASTC->RGBA no PRÓPRIO hardware do driver) para ler de volta como RGBA8 e
  // comparar contra a textura original — o driver Adreno é o "decoder de referência", não uma
  // reimplementação própria.
  VkFormatProperties astcFormatProps{};
  vkGetPhysicalDeviceFormatProperties(res.physicalDevice, VK_FORMAT_ASTC_4x4_UNORM_BLOCK, &astcFormatProps);
  VkFormatProperties rgbaProps{};
  vkGetPhysicalDeviceFormatProperties(res.physicalDevice, VK_FORMAT_R8G8B8A8_UNORM, &rgbaProps);
  bool astcBlitSupported = (rgbaProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) != 0 && (astcFormatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) != 0 &&
                           (astcFormatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0;

  if (!astcBlitSupported) {
    __android_log_print(ANDROID_LOG_WARN, LogTag,
                        "Driver não suporta blit de leitura para VK_FORMAT_ASTC_4x4_UNORM_BLOCK — pulando validação de hardware decode.");
    res.destroy();
    return result;
  }

  if (!createDeviceLocalImage(res, textureWidth, textureHeight, VK_FORMAT_ASTC_4x4_UNORM_BLOCK,
                              VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                              &res.astcImage, &res.astcImageMemory)) {
    __android_log_print(ANDROID_LOG_WARN, LogTag, "Falha ao criar imagem ASTC de validação.");
    res.destroy();
    return result;
  }
  if (!createDeviceLocalImage(res, textureWidth, textureHeight, VK_FORMAT_R8G8B8A8_UNORM,
                              VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                              &res.readbackImage, &res.readbackImageMemory)) {
    __android_log_print(ANDROID_LOG_WARN, LogTag, "Falha ao criar imagem de readback.");
    res.destroy();
    return result;
  }

  VkDeviceSize readbackBufferSize = static_cast<VkDeviceSize>(textureWidth) * textureHeight * 4;
  if (!createHostVisibleBuffer(res, readbackBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               &res.readbackBuffer, &res.readbackBufferMemory, &res.readbackBufferMapped)) {
    __android_log_print(ANDROID_LOG_WARN, LogTag, "Falha ao criar buffer de readback.");
    res.destroy();
    return result;
  }

  rhi::VulkanComputeContext &readbackContext = separateGraphicsQueue
                                                   ? res.graphicsContext
                                                   : res.computeContext;
  res.commandBuffer = readbackContext.begin();
  if (res.commandBuffer == VK_NULL_HANDLE) return result;

  if (separateGraphicsQueue) {
    rhi::cmdComputeBufferBarrier(res.commandBuffer, res.outputBuffer, 0, outputBufferSize,
                                 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_ACCESS_TRANSFER_READ_BIT,
                                 res.queueFamily, device.graphicsQueueFamily());
  }

  transitionImageLayout(res.commandBuffer, res.astcImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
  VkBufferImageCopy astcCopyRegion{};
  astcCopyRegion.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  astcCopyRegion.imageExtent = {textureWidth, textureHeight, 1};
  vkCmdCopyBufferToImage(res.commandBuffer, res.outputBuffer, res.astcImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &astcCopyRegion);

  transitionImageLayout(res.commandBuffer, res.astcImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
  transitionImageLayout(res.commandBuffer, res.readbackImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

  VkImageBlit blitRegion{};
  blitRegion.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  blitRegion.srcOffsets[1] = {static_cast<i32>(textureWidth), static_cast<i32>(textureHeight), 1};
  blitRegion.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  blitRegion.dstOffsets[1] = {static_cast<i32>(textureWidth), static_cast<i32>(textureHeight), 1};
  vkCmdBlitImage(res.commandBuffer, res.astcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                res.readbackImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blitRegion, VK_FILTER_NEAREST);

  transitionImageLayout(res.commandBuffer, res.readbackImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

  VkBufferImageCopy readbackCopyRegion{};
  readbackCopyRegion.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  readbackCopyRegion.imageExtent = {textureWidth, textureHeight, 1};
  vkCmdCopyImageToBuffer(res.commandBuffer, res.readbackImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, res.readbackBuffer, 1, &readbackCopyRegion);

  VkMemoryBarrier readbackBarrier{};
  readbackBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
  readbackBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  readbackBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
  vkCmdPipelineBarrier(res.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                       0, 1, &readbackBarrier, 0, nullptr, 0, nullptr);
  if (separateGraphicsQueue) {
    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    rhi::VulkanComputeContext::SubmitSync sync{};
    sync.waitSemaphores = &res.computeToGraphics; sync.waitStages = &waitStage; sync.waitCount = 1;
    if (!readbackContext.submit(sync) || !readbackContext.wait()) return result;
  } else if (!readbackContext.submit() || !readbackContext.wait()) return result;

  const auto *decoded = static_cast<const u8 *>(res.readbackBufferMapped);
  u32 maxDifference = 0;
  if (!ae::platform::compareAstcProbePixels(testPixels, {decoded, testPixels.size()}, maxDifference)) return result;
  result.testedTexels = static_cast<u64>(textureWidth) * textureHeight;
  result.maxChannelDifference = static_cast<float>(maxDifference);
  // Limite estrutural do corpus sintético, não um critério de qualidade de importação.
  result.hardwareDecodeMatchesSource = maxDifference <= 24;
  result.succeeded = result.timingValid && result.hardwareDecodeMatchesSource;
  __android_log_print(ANDROID_LOG_INFO, LogTag,
      "Hardware decode: %llu texels RGBA, diferença máxima=%u, coerente=%s.",
      static_cast<unsigned long long>(result.testedTexels), maxDifference,
      result.hardwareDecodeMatchesSource ? "sim" : "não");

  res.destroy();
  return result;
}

} // namespace ae::platform::android
