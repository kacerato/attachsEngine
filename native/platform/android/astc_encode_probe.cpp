#include "platform/android/astc_encode_probe.h"

#include "rhi/shaders/astc_encode_spirv.h"

#include <android/log.h>

#include <cmath>
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
  VkQueue queue = VK_NULL_HANDLE;
  u32 queueFamily = 0;

  VkCommandPool commandPool = VK_NULL_HANDLE;
  VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
  VkFence fence = VK_NULL_HANDLE;
  VkQueryPool queryPool = VK_NULL_HANDLE;

  VkImage sourceImage = VK_NULL_HANDLE;
  VkDeviceMemory sourceImageMemory = VK_NULL_HANDLE;
  VkImageView sourceImageView = VK_NULL_HANDLE;

  VkBuffer outputBuffer = VK_NULL_HANDLE;
  VkDeviceMemory outputBufferMemory = VK_NULL_HANDLE;
  void *outputBufferMapped = nullptr;

  VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
  VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
  VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
  VkPipeline pipeline = VK_NULL_HANDLE;
  VkShaderModule shaderModule = VK_NULL_HANDLE;

  // Recursos ASTC para validação por hardware decode (blit ASTC->RGBA8 pelo próprio driver).
  VkImage astcImage = VK_NULL_HANDLE;
  VkDeviceMemory astcImageMemory = VK_NULL_HANDLE;
  VkImage readbackImage = VK_NULL_HANDLE;
  VkDeviceMemory readbackImageMemory = VK_NULL_HANDLE;
  VkBuffer readbackBuffer = VK_NULL_HANDLE;
  VkDeviceMemory readbackBufferMemory = VK_NULL_HANDLE;
  void *readbackBufferMapped = nullptr;

  void destroy() {
    if (device == VK_NULL_HANDLE) return;
    if (pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, pipeline, nullptr);
    if (pipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    if (shaderModule != VK_NULL_HANDLE) vkDestroyShaderModule(device, shaderModule, nullptr);
    if (descriptorPool != VK_NULL_HANDLE) vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    if (descriptorSetLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    if (queryPool != VK_NULL_HANDLE) vkDestroyQueryPool(device, queryPool, nullptr);
    if (fence != VK_NULL_HANDLE) vkDestroyFence(device, fence, nullptr);
    if (commandPool != VK_NULL_HANDLE) vkDestroyCommandPool(device, commandPool, nullptr);
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

// Preenche `pixels` (RGBA8, linhas compactas) com um padrão de teste que a heurística de eixo
// único do encoder consegue representar razoavelmente bem (variação suave por bloco 4x4) —
// suficiente para validar bit-packing/decodificação de hardware, não para medir qualidade visual
// de compressão (ver limitação documentada em astc_encode.comp).
void fillTestPattern(std::vector<u8> &pixels, u32 width, u32 height) {
  pixels.resize(static_cast<usize>(width) * height * 4);
  for (u32 y = 0; y < height; ++y) {
    for (u32 x = 0; x < width; ++x) {
      // Blocos 4x4 de cor sólida, variando lentamente ao longo da imagem — cada bloco individual
      // é praticamente uniforme (o caso que a heurística de bounding box representa bem), mas a
      // textura inteira varia, evitando medir só o caso degenerado "tudo uma cor".
      u32 blockX = x / 4;
      u32 blockY = y / 4;
      u8 r = static_cast<u8>((blockX * 7) & 0xFF);
      u8 g = static_cast<u8>((blockY * 11) & 0xFF);
      u8 b = static_cast<u8>(((blockX + blockY) * 5) & 0xFF);
      usize offset = (static_cast<usize>(y) * width + x) * 4;
      pixels[offset + 0] = r;
      pixels[offset + 1] = g;
      pixels[offset + 2] = b;
      pixels[offset + 3] = 255;
    }
  }
}

} // namespace

AstcEncodeProbeResult runAstcEncodeProbe(rhi::VulkanDevice &device, u32 textureWidth, u32 textureHeight) {
  AstcEncodeProbeResult result;

  ProbeResources res;
  res.device = device.handle();
  res.physicalDevice = device.physicalDevice();
  res.queueFamily = device.graphicsQueueFamily();
  vkGetDeviceQueue(res.device, res.queueFamily, 0, &res.queue);

  const u32 blocksX = (textureWidth + 3) / 4;
  const u32 blocksY = (textureHeight + 3) / 4;
  const u32 blockCount = blocksX * blocksY;
  const VkDeviceSize outputBufferSize = static_cast<VkDeviceSize>(blockCount) * 16; // 16 bytes/bloco ASTC

  // ---------------- Comandos e sincronização ----------------
  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.queueFamilyIndex = res.queueFamily;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  if (vkCreateCommandPool(res.device, &poolInfo, nullptr, &res.commandPool) != VK_SUCCESS) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar command pool.");
    res.destroy();
    return result;
  }

  VkCommandBufferAllocateInfo cmdAllocInfo{};
  cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cmdAllocInfo.commandPool = res.commandPool;
  cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cmdAllocInfo.commandBufferCount = 1;
  if (vkAllocateCommandBuffers(res.device, &cmdAllocInfo, &res.commandBuffer) != VK_SUCCESS) {
    res.destroy();
    return result;
  }

  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  if (vkCreateFence(res.device, &fenceInfo, nullptr, &res.fence) != VK_SUCCESS) {
    res.destroy();
    return result;
  }

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
  std::vector<u8> testPixels;
  fillTestPattern(testPixels, textureWidth, textureHeight);
  VkDeviceSize stagingSize = static_cast<VkDeviceSize>(testPixels.size());

  VkBuffer stagingBuffer = VK_NULL_HANDLE;
  VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
  void *stagingMapped = nullptr;
  if (!createHostVisibleBuffer(res, stagingSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &stagingBuffer, &stagingMemory, &stagingMapped)) {
    res.destroy();
    return result;
  }
  std::memcpy(stagingMapped, testPixels.data(), testPixels.size());

  // ---------------- Buffer de saída dos blocos ASTC ----------------
  if (!createHostVisibleBuffer(res, outputBufferSize,
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                               &res.outputBuffer, &res.outputBufferMemory, &res.outputBufferMapped)) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar buffer de saída.");
    vkDestroyBuffer(res.device, stagingBuffer, nullptr);
    vkFreeMemory(res.device, stagingMemory, nullptr);
    res.destroy();
    return result;
  }

  // ---------------- Pipeline de compute ----------------
  VkShaderModuleCreateInfo shaderInfo{};
  shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shaderInfo.codeSize = rhi::shaders::kAstc_EncodeCompSpirvSize;
  shaderInfo.pCode = rhi::shaders::kAstc_EncodeCompSpirv;
  if (vkCreateShaderModule(res.device, &shaderInfo, nullptr, &res.shaderModule) != VK_SUCCESS) {
    vkDestroyBuffer(res.device, stagingBuffer, nullptr);
    vkFreeMemory(res.device, stagingMemory, nullptr);
    res.destroy();
    return result;
  }

  VkDescriptorSetLayoutBinding bindings[2]{};
  bindings[0].binding = 0;
  bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  bindings[0].descriptorCount = 1;
  bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  bindings[1].binding = 1;
  bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  bindings[1].descriptorCount = 1;
  bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  VkDescriptorSetLayoutCreateInfo setLayoutInfo{};
  setLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  setLayoutInfo.bindingCount = 2;
  setLayoutInfo.pBindings = bindings;
  if (vkCreateDescriptorSetLayout(res.device, &setLayoutInfo, nullptr, &res.descriptorSetLayout) != VK_SUCCESS) {
    vkDestroyBuffer(res.device, stagingBuffer, nullptr);
    vkFreeMemory(res.device, stagingMemory, nullptr);
    res.destroy();
    return result;
  }

  VkPushConstantRange pushConstantRange{};
  pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  pushConstantRange.offset = 0;
  pushConstantRange.size = sizeof(u32); // blocksPerRow

  VkPipelineLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layoutInfo.setLayoutCount = 1;
  layoutInfo.pSetLayouts = &res.descriptorSetLayout;
  layoutInfo.pushConstantRangeCount = 1;
  layoutInfo.pPushConstantRanges = &pushConstantRange;
  if (vkCreatePipelineLayout(res.device, &layoutInfo, nullptr, &res.pipelineLayout) != VK_SUCCESS) {
    vkDestroyBuffer(res.device, stagingBuffer, nullptr);
    vkFreeMemory(res.device, stagingMemory, nullptr);
    res.destroy();
    return result;
  }

  VkComputePipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  pipelineInfo.stage.module = res.shaderModule;
  pipelineInfo.stage.pName = "main";
  pipelineInfo.layout = res.pipelineLayout;
  if (vkCreateComputePipelines(res.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &res.pipeline) != VK_SUCCESS) {
    vkDestroyBuffer(res.device, stagingBuffer, nullptr);
    vkFreeMemory(res.device, stagingMemory, nullptr);
    res.destroy();
    return result;
  }

  VkDescriptorPoolSize poolSizes[2]{};
  poolSizes[0] = {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1};
  poolSizes[1] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};
  VkDescriptorPoolCreateInfo descPoolInfo{};
  descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descPoolInfo.maxSets = 1;
  descPoolInfo.poolSizeCount = 2;
  descPoolInfo.pPoolSizes = poolSizes;
  if (vkCreateDescriptorPool(res.device, &descPoolInfo, nullptr, &res.descriptorPool) != VK_SUCCESS) {
    vkDestroyBuffer(res.device, stagingBuffer, nullptr);
    vkFreeMemory(res.device, stagingMemory, nullptr);
    res.destroy();
    return result;
  }

  VkDescriptorSetAllocateInfo descAllocInfo{};
  descAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  descAllocInfo.descriptorPool = res.descriptorPool;
  descAllocInfo.descriptorSetCount = 1;
  descAllocInfo.pSetLayouts = &res.descriptorSetLayout;
  if (vkAllocateDescriptorSets(res.device, &descAllocInfo, &res.descriptorSet) != VK_SUCCESS) {
    vkDestroyBuffer(res.device, stagingBuffer, nullptr);
    vkFreeMemory(res.device, stagingMemory, nullptr);
    res.destroy();
    return result;
  }

  VkDescriptorImageInfo imageDescInfo{};
  imageDescInfo.imageView = res.sourceImageView;
  imageDescInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

  VkDescriptorBufferInfo bufferDescInfo{};
  bufferDescInfo.buffer = res.outputBuffer;
  bufferDescInfo.offset = 0;
  bufferDescInfo.range = outputBufferSize;

  VkWriteDescriptorSet writes[2]{};
  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].dstSet = res.descriptorSet;
  writes[0].dstBinding = 0;
  writes[0].descriptorCount = 1;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  writes[0].pImageInfo = &imageDescInfo;
  writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[1].dstSet = res.descriptorSet;
  writes[1].dstBinding = 1;
  writes[1].descriptorCount = 1;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[1].pBufferInfo = &bufferDescInfo;
  vkUpdateDescriptorSets(res.device, 2, writes, 0, nullptr);

  // ---------------- Gravação e submissão ----------------
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(res.commandBuffer, &beginInfo);

  vkCmdResetQueryPool(res.commandBuffer, res.queryPool, 0, 2);

  transitionImageLayout(res.commandBuffer, res.sourceImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

  VkBufferImageCopy copyRegion{};
  copyRegion.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  copyRegion.imageExtent = {textureWidth, textureHeight, 1};
  vkCmdCopyBufferToImage(res.commandBuffer, stagingBuffer, res.sourceImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

  transitionImageLayout(res.commandBuffer, res.sourceImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

  vkCmdWriteTimestamp(res.commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, res.queryPool, 0);

  vkCmdBindPipeline(res.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, res.pipeline);
  vkCmdBindDescriptorSets(res.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, res.pipelineLayout, 0, 1, &res.descriptorSet, 0, nullptr);
  vkCmdPushConstants(res.commandBuffer, res.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(u32), &blocksX);
  // local_size 8x8 no shader — arredonda para cima o número de grupos de trabalho.
  vkCmdDispatch(res.commandBuffer, (blocksX + 7) / 8, (blocksY + 7) / 8, 1);

  VkMemoryBarrier computeToHostBarrier{};
  computeToHostBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
  computeToHostBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  computeToHostBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
  vkCmdPipelineBarrier(res.commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                       0, 1, &computeToHostBarrier, 0, nullptr, 0, nullptr);

  vkCmdWriteTimestamp(res.commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, res.queryPool, 1);

  vkEndCommandBuffer(res.commandBuffer);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &res.commandBuffer;
  vkQueueSubmit(res.queue, 1, &submitInfo, res.fence);
  vkWaitForFences(res.device, 1, &res.fence, VK_TRUE, UINT64_MAX);

  // ---------------- Medição de tempo via GPU timestamp ----------------
  VkPhysicalDeviceProperties deviceProps{};
  vkGetPhysicalDeviceProperties(res.physicalDevice, &deviceProps);
  float timestampPeriodNs = deviceProps.limits.timestampPeriod;

  u64 timestamps[2] = {0, 0};
  VkResult queryResult = vkGetQueryPoolResults(res.device, res.queryPool, 0, 2, sizeof(timestamps), timestamps,
                                               sizeof(u64), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
  if (queryResult == VK_SUCCESS && timestamps[1] > timestamps[0]) {
    double elapsedNs = static_cast<double>(timestamps[1] - timestamps[0]) * timestampPeriodNs;
    result.encodeMilliseconds = elapsedNs / 1.0e6;
  }
  result.blockCount = blockCount;
  result.succeeded = true;

  vkDestroyBuffer(res.device, stagingBuffer, nullptr);
  vkFreeMemory(res.device, stagingMemory, nullptr);

  // ---------------- Validação via hardware decode ----------------
  // Cria uma imagem ASTC_4x4 real, sobe os blocos gerados pelo compute shader, e usa vkCmdBlitImage
  // (que decodifica ASTC->RGBA no PRÓPRIO hardware do driver) para ler de volta como RGBA8 e
  // comparar contra a textura original — o driver Adreno é o "decoder de referência", não uma
  // reimplementação própria.
  VkFormatProperties astcFormatProps{};
  vkGetPhysicalDeviceFormatProperties(res.physicalDevice, VK_FORMAT_ASTC_4x4_UNORM_BLOCK, &astcFormatProps);
  bool astcBlitSupported = (astcFormatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) != 0 &&
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

  VkCommandBufferBeginInfo beginInfo2{};
  beginInfo2.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo2.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkResetCommandBuffer(res.commandBuffer, 0);
  vkBeginCommandBuffer(res.commandBuffer, &beginInfo2);

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

  vkEndCommandBuffer(res.commandBuffer);

  vkResetFences(res.device, 1, &res.fence);
  VkSubmitInfo submitInfo2{};
  submitInfo2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo2.commandBufferCount = 1;
  submitInfo2.pCommandBuffers = &res.commandBuffer;
  vkQueueSubmit(res.queue, 1, &submitInfo2, res.fence);
  vkWaitForFences(res.device, 1, &res.fence, VK_TRUE, UINT64_MAX);

  // Compara uma amostra de texels (não a imagem inteira, por custo de CPU) entre a fonte original
  // e o resultado decodificado pelo hardware — um texel por bloco 4x4, no canto (0,0) do bloco.
  const u8 *decoded = static_cast<const u8 *>(res.readbackBufferMapped);
  float maxDiff = 0.0f;
  u32 sampledBlocks = 0;
  for (u32 by = 0; by < blocksY; by += 4) { // amostra 1 a cada 4 blocos para manter o teste rápido em 4K
    for (u32 bx = 0; bx < blocksX; bx += 4) {
      u32 x = bx * 4;
      u32 y = by * 4;
      if (x >= textureWidth || y >= textureHeight) continue;
      usize offset = (static_cast<usize>(y) * textureWidth + x) * 4;
      for (int c = 0; c < 3; ++c) {
        float diff = std::fabs(static_cast<float>(decoded[offset + c]) - static_cast<float>(testPixels[offset + c]));
        maxDiff = std::max(maxDiff, diff);
      }
      ++sampledBlocks;
    }
  }
  result.maxChannelDifference = maxDiff;
  // QUANT_32 (5 bits/canal, 32 níveis) já perde precisão de cor por design frente aos 256 níveis
  // originais — espaçamento de quantização de ~8 por nível. Uma diferença de até 24 (~3 níveis)
  // confirma que o hardware decodificou algo estruturalmente coerente com o que foi codificado.
  result.hardwareDecodeMatchesSource = maxDiff <= 24.0f;

  __android_log_print(ANDROID_LOG_INFO, LogTag,
                      "Validação de hardware decode: %u blocos amostrados, diferença máxima de canal=%.1f, coerente=%s.",
                      sampledBlocks, static_cast<double>(maxDiff), result.hardwareDecodeMatchesSource ? "sim" : "não");

  res.destroy();
  return result;
}

} // namespace ae::platform::android
