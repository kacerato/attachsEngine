#include "rhi/upload_context.h"

#include <cstring>
#include <array>
#include <algorithm>

namespace ae::rhi {

VulkanUploadContext::~VulkanUploadContext() {
  shutdown();
}

bool VulkanUploadContext::initialize(VkDevice device, u32 queueFamilyIndex) {
  if (device == VK_NULL_HANDLE || device_ != VK_NULL_HANDLE) return false;
  device_ = device;
  vkGetDeviceQueue(device_, queueFamilyIndex, 0, &queue_);

  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags =
      VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = queueFamilyIndex;
  if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_) != VK_SUCCESS) {
    shutdown();
    return false;
  }

  VkCommandBufferAllocateInfo commandInfo{};
  commandInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  commandInfo.commandPool = commandPool_;
  commandInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  commandInfo.commandBufferCount = 1;
  if (vkAllocateCommandBuffers(device_, &commandInfo, &commandBuffer_) != VK_SUCCESS) {
    shutdown();
    return false;
  }

  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  if (vkCreateFence(device_, &fenceInfo, nullptr, &fence_) != VK_SUCCESS) {
    shutdown();
    return false;
  }
  return true;
}

void VulkanUploadContext::shutdown() {
  if (device_ != VK_NULL_HANDLE) {
    if (fence_ != VK_NULL_HANDLE) vkDestroyFence(device_, fence_, nullptr);
    if (commandPool_ != VK_NULL_HANDLE) vkDestroyCommandPool(device_, commandPool_, nullptr);
  }
  fence_ = VK_NULL_HANDLE;
  commandBuffer_ = VK_NULL_HANDLE;
  commandPool_ = VK_NULL_HANDLE;
  queue_ = VK_NULL_HANDLE;
  device_ = VK_NULL_HANDLE;
}

bool VulkanUploadContext::uploadRgba8ToSampledImage(VulkanMemoryAllocator &allocator,
                                                     const void *sourceBytes,
                                                     u64 sourceSizeBytes,
                                                     VulkanImage &destination) {
  if (!isRgba8UploadValid(destination.description(), sourceSizeBytes)) return false;
  return uploadSampledMipChain(allocator, sourceBytes, sourceSizeBytes, destination);
}

bool VulkanUploadContext::uploadSampledMipChain(VulkanMemoryAllocator &allocator,
    const void *sourceBytes, u64 sourceSizeBytes, VulkanImage &destination) {
  const ImageDesc desc = destination.description();
  constexpr auto requiredUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  if (device_ == VK_NULL_HANDLE || sourceBytes == nullptr || !allocator.isReady() ||
      allocator.device() != device_ || destination.owner_ != &allocator ||
      !destination.isReady() || destination.uploadSubmitted_ ||
      desc.aspectMask != VK_IMAGE_ASPECT_COLOR_BIT || (desc.usage & requiredUsage) != requiredUsage ||
      sourceSizeBytes == 0 || sourceSizeBytes != sampledChainByteSize(desc)) {
    return false;
  }

  BufferDesc stagingDesc{};
  stagingDesc.sizeBytes = sourceSizeBytes;
  stagingDesc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  stagingDesc.memoryClass = MemoryClass::Staging;
  stagingDesc.cpuAccess = CpuAccess::SequentialWrite;
  stagingDesc.preferDeviceMemory = false;
  VulkanBuffer staging;
  if (!allocator.createBuffer(stagingDesc, &staging) || staging.mappedData() == nullptr) return false;
  std::memcpy(staging.mappedData(), sourceBytes, static_cast<usize>(sourceSizeBytes));
  if (!allocator.flushBuffer(staging)) return false;

  if (vkResetCommandPool(device_, commandPool_, 0) != VK_SUCCESS) return false;
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  if (vkBeginCommandBuffer(commandBuffer_, &beginInfo) != VK_SUCCESS) return false;

  VkImageMemoryBarrier toTransfer{};
  toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  toTransfer.image = destination.handle();
  toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  toTransfer.subresourceRange.levelCount = desc.mipLevels;
  toTransfer.subresourceRange.layerCount = 1;
  toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  vkCmdPipelineBarrier(commandBuffer_, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                       &toTransfer);

  std::array<VkBufferImageCopy, 32> copies{};
  u64 offset = 0;
  for (u32 mip = 0; mip < desc.mipLevels; ++mip) {
    auto &copy = copies[mip];
    copy.bufferOffset = offset;
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.mipLevel = mip;
    copy.imageSubresource.layerCount = 1;
    copy.imageExtent = {std::max(1u,desc.width>>mip), std::max(1u,desc.height>>mip), 1};
    offset += sampledMipByteSize(desc.format, copy.imageExtent.width, copy.imageExtent.height);
  }
  vkCmdCopyBufferToImage(commandBuffer_, staging.handle(), destination.handle(),
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, desc.mipLevels, copies.data());

  VkImageMemoryBarrier toShaderRead{};
  toShaderRead.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  toShaderRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  toShaderRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  toShaderRead.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  toShaderRead.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  toShaderRead.image = destination.handle();
  toShaderRead.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  toShaderRead.subresourceRange.levelCount = desc.mipLevels;
  toShaderRead.subresourceRange.layerCount = 1;
  toShaderRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  toShaderRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(commandBuffer_, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                       &toShaderRead);

  if (vkEndCommandBuffer(commandBuffer_) != VK_SUCCESS) return false;
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer_;
  if (vkQueueSubmit(queue_, 1, &submitInfo, fence_) != VK_SUCCESS) return false;
  destination.uploadSubmitted_ = true;
  const VkResult waitResult = vkWaitForFences(device_, 1, &fence_, VK_TRUE, UINT64_MAX);
  if (waitResult != VK_SUCCESS) {
    // Não libere o staging ainda em uso se a espera falhou por falta de memória.
    // Em device-lost, o caller encerra/recria todo o contexto.
    vkQueueWaitIdle(queue_);
    return false;
  }
  return vkResetFences(device_, 1, &fence_) == VK_SUCCESS;
}

} // namespace ae::rhi
