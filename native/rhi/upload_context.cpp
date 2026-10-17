#include "rhi/upload_context.h"
#include <limits>

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
  if (vkResetFences(device_, 1, &fence_) != VK_SUCCESS ||
      vkQueueSubmit(queue_, 1, &submitInfo, fence_) != VK_SUCCESS) return false;
  destination.uploadSubmitted_ = true;
  const VkResult waitResult = vkWaitForFences(device_, 1, &fence_, VK_TRUE, UINT64_MAX);
  if (waitResult != VK_SUCCESS) {
    // Não libere o staging ainda em uso se a espera falhou por falta de memória.
    // Em device-lost, o caller encerra/recria todo o contexto.
    vkQueueWaitIdle(queue_);
    return false;
  }
  return true;
}

bool VulkanUploadContext::uploadBuffer(VulkanMemoryAllocator &allocator, const void *sourceBytes,
    u64 sourceSizeBytes, VulkanBuffer &destination, VkPipelineStageFlags destinationStage,
    VkAccessFlags destinationAccess) {
  if (device_ == VK_NULL_HANDLE || sourceBytes == nullptr || sourceSizeBytes == 0 ||
      !allocator.isReady() || allocator.device() != device_ || destination.owner_ != &allocator ||
      !destination.isReady() || destination.sizeBytes() < sourceSizeBytes || sourceSizeBytes%4 ||
      !(destination.usage_&VK_BUFFER_USAGE_TRANSFER_DST_BIT) ||
      destinationStage == 0 || destinationAccess == 0) return false;

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
  VkBufferCopy copy{0, 0, sourceSizeBytes};
  vkCmdCopyBuffer(commandBuffer_, staging.handle(), destination.handle(), 1, &copy);
  VkBufferMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = destinationAccess;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.buffer = destination.handle();
  barrier.size = sourceSizeBytes;
  vkCmdPipelineBarrier(commandBuffer_, VK_PIPELINE_STAGE_TRANSFER_BIT, destinationStage,
                       0, 0, nullptr, 1, &barrier, 0, nullptr);
  if (vkEndCommandBuffer(commandBuffer_) != VK_SUCCESS) return false;

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer_;
  if (vkResetFences(device_, 1, &fence_) != VK_SUCCESS ||
      vkQueueSubmit(queue_, 1, &submitInfo, fence_) != VK_SUCCESS) return false;
  const VkResult waitResult = vkWaitForFences(device_, 1, &fence_, VK_TRUE, UINT64_MAX);
  if (waitResult != VK_SUCCESS) {
    vkQueueWaitIdle(queue_);
    return false;
  }
  return true;
}

bool VulkanUploadContext::uploadBufferRanges(VulkanMemoryAllocator &allocator,
    std::span<const BufferRangeUpload> updates) {
  if(device_==VK_NULL_HANDLE || !allocator.isReady() || allocator.device()!=device_)return false;
  u64 stagedBytes=0;
  for(const auto &update:updates) {
    const auto *destination=update.destination;
    if(!destination || destination->owner_!=&allocator || !destination->isReady() ||
       update.source.empty() || update.source.size()>destination->sizeBytes() || update.source.size()%4 || !update.stage || !update.access ||
       !(destination->usage_&VK_BUFFER_USAGE_TRANSFER_DST_BIT))return false;
    u64 end=0;
    for(const auto &range:update.ranges) {
      if(!range.bytes || range.offset%4 || range.bytes%4 || range.offset<end ||
         range.offset>update.source.size() || range.bytes>update.source.size()-range.offset)return false;
      end=range.offset+range.bytes;
      if(range.bytes>std::numeric_limits<u64>::max()-stagedBytes)return false;
      stagedBytes+=range.bytes;
    }
  }
  if(!stagedBytes)return true;
  BufferDesc desc{};desc.sizeBytes=stagedBytes;desc.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  desc.memoryClass=MemoryClass::Staging;desc.cpuAccess=CpuAccess::SequentialWrite;desc.preferDeviceMemory=false;
  VulkanBuffer staging;
  if(!allocator.createBuffer(desc,&staging) || !staging.mappedData())return false;
  std::vector<std::vector<VkBufferCopy>> copies(updates.size());
  std::vector<VkBufferMemoryBarrier> barriers;
  u64 offset=0;VkPipelineStageFlags stages=0;
  for(usize i=0;i<updates.size();++i) {
    const auto &update=updates[i];if(update.ranges.empty())continue;
    copies[i].reserve(update.ranges.size());
    for(const auto &range:update.ranges) {
      std::memcpy(static_cast<u8 *>(staging.mappedData())+offset,update.source.data()+range.offset,range.bytes);
      copies[i].push_back({offset,range.offset,range.bytes});offset+=range.bytes;
    }
    VkBufferMemoryBarrier barrier{};barrier.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=update.access;
    barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer=update.destination->handle();barrier.size=update.destination->sizeBytes();
    barriers.push_back(barrier);stages|=update.stage;
  }
  if(!allocator.flushBuffer(staging) || vkResetCommandPool(device_,commandPool_,0)!=VK_SUCCESS)return false;
  VkCommandBufferBeginInfo begin{};begin.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  if(vkBeginCommandBuffer(commandBuffer_,&begin)!=VK_SUCCESS)return false;
  for(usize i=0;i<updates.size();++i)if(!copies[i].empty())
    vkCmdCopyBuffer(commandBuffer_,staging.handle(),updates[i].destination->handle(),u32(copies[i].size()),copies[i].data());
  vkCmdPipelineBarrier(commandBuffer_,VK_PIPELINE_STAGE_TRANSFER_BIT,stages,0,0,nullptr,u32(barriers.size()),barriers.data(),0,nullptr);
  if(vkEndCommandBuffer(commandBuffer_)!=VK_SUCCESS)return false;
  VkSubmitInfo submit{};submit.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;submit.commandBufferCount=1;submit.pCommandBuffers=&commandBuffer_;
  if(vkResetFences(device_,1,&fence_)!=VK_SUCCESS || vkQueueSubmit(queue_,1,&submit,fence_)!=VK_SUCCESS)return false;
  if(vkWaitForFences(device_,1,&fence_,VK_TRUE,UINT64_MAX)!=VK_SUCCESS){vkQueueWaitIdle(queue_);return false;}
  // No fallible operation after successful in-place publication. The next
  // upload resets this completed fence before submitting.
  return true;
}
} // namespace ae::rhi
