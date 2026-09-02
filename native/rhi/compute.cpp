#include "rhi/compute.h"

#include <algorithm>

namespace ae::rhi {
VulkanComputeKernel::~VulkanComputeKernel() { shutdown(); }

bool VulkanComputeKernel::initialize(VkDevice device, const ComputeLimits &limits,
                                     const ComputeKernelDesc &desc,
                                     VkPipelineCache pipelineCache) {
  shutdown();
  if (device == VK_NULL_HANDLE || !isComputeKernelDescValid(desc, limits)) return false;
  ComputeShaderReflection reflection{};
  if (!reflectComputeShader(desc.spirv, desc.spirvBytes, reflection) ||
      reflection.bindingCount != desc.bindingCount ||
      reflection.hasPushConstants != (desc.pushConstantBytes > 0)) return false;
  if (!isComputeLocalSizeValid(reflection, limits)) return false;
  for (u32 reflectedIndex = 0; reflectedIndex < reflection.bindingCount; ++reflectedIndex) {
    bool matched = false;
    for (u32 declaredIndex = 0; declaredIndex < desc.bindingCount; ++declaredIndex) {
      if (reflection.bindings[reflectedIndex] == desc.bindings[declaredIndex]) {
        matched = true; break;
      }
    }
    if (!matched) return false;
  }
  device_ = device; limits_ = limits; bindingCount_ = desc.bindingCount;
  pushConstantBytes_ = desc.pushConstantBytes;
  if (bindingCount_ > 0) std::copy(desc.bindings, desc.bindings + bindingCount_, bindings_);

  VkDescriptorSetLayoutBinding vkBindings[16]{};
  VkDescriptorPoolSize poolSizes[16]{};
  u32 poolSizeCount = 0;
  for (u32 i = 0; i < bindingCount_; ++i) {
    vkBindings[i] = {bindings_[i].binding, bindings_[i].type, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    u32 poolIndex = poolSizeCount;
    for (u32 existing = 0; existing < poolSizeCount; ++existing) {
      if (poolSizes[existing].type == bindings_[i].type) { poolIndex = existing; break; }
    }
    if (poolIndex == poolSizeCount) poolSizes[poolSizeCount++] = {bindings_[i].type, 0};
    poolSizes[poolIndex].descriptorCount++;
    requiredBindingsMask_ |= static_cast<u16>(1u << i);
  }
  VkDescriptorSetLayoutCreateInfo setInfo{};
  setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  setInfo.bindingCount = bindingCount_; setInfo.pBindings = vkBindings;
  if (vkCreateDescriptorSetLayout(device_, &setInfo, nullptr, &descriptorSetLayout_) != VK_SUCCESS) {
    shutdown(); return false;
  }
  if (bindingCount_ > 0) {
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1; poolInfo.poolSizeCount = poolSizeCount; poolInfo.pPoolSizes = poolSizes;
    if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descriptorPool_) != VK_SUCCESS) {
      shutdown(); return false;
    }
    VkDescriptorSetAllocateInfo allocation{};
    allocation.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocation.descriptorPool = descriptorPool_; allocation.descriptorSetCount = 1;
    allocation.pSetLayouts = &descriptorSetLayout_;
    if (vkAllocateDescriptorSets(device_, &allocation, &descriptorSet_) != VK_SUCCESS) {
      shutdown(); return false;
    }
  }
  VkPushConstantRange pushRange{};
  pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT; pushRange.size = pushConstantBytes_;
  VkPipelineLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layoutInfo.setLayoutCount = bindingCount_ > 0 ? 1u : 0u;
  layoutInfo.pSetLayouts = bindingCount_ > 0 ? &descriptorSetLayout_ : nullptr;
  layoutInfo.pushConstantRangeCount = pushConstantBytes_ > 0 ? 1u : 0u;
  layoutInfo.pPushConstantRanges = pushConstantBytes_ > 0 ? &pushRange : nullptr;
  if (vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
    shutdown(); return false;
  }
  VkShaderModuleCreateInfo shaderInfo{};
  shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shaderInfo.codeSize = desc.spirvBytes; shaderInfo.pCode = desc.spirv;
  VkShaderModule module = VK_NULL_HANDLE;
  if (vkCreateShaderModule(device_, &shaderInfo, nullptr, &module) != VK_SUCCESS) {
    shutdown(); return false;
  }
  VkComputePipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT; pipelineInfo.stage.module = module;
  pipelineInfo.stage.pName = desc.entryPoint; pipelineInfo.layout = pipelineLayout_;
  const VkResult result = vkCreateComputePipelines(device_, pipelineCache, 1, &pipelineInfo,
                                                   nullptr, &pipeline_);
  vkDestroyShaderModule(device_, module, nullptr);
  if (result != VK_SUCCESS) { shutdown(); return false; }
  return true;
}

void VulkanComputeKernel::shutdown() {
  if (device_ != VK_NULL_HANDLE) {
    if (pipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, pipeline_, nullptr);
    if (pipelineLayout_ != VK_NULL_HANDLE) vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
    if (descriptorPool_ != VK_NULL_HANDLE) vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
    if (descriptorSetLayout_ != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device_, descriptorSetLayout_, nullptr);
  }
  device_ = VK_NULL_HANDLE; descriptorSetLayout_ = VK_NULL_HANDLE; descriptorPool_ = VK_NULL_HANDLE;
  descriptorSet_ = VK_NULL_HANDLE; pipelineLayout_ = VK_NULL_HANDLE; pipeline_ = VK_NULL_HANDLE;
  limits_ = {}; bindingCount_ = 0; requiredBindingsMask_ = 0; writtenBindingsMask_ = 0;
  pushConstantBytes_ = 0;
}

const ComputeBindingDesc *VulkanComputeKernel::findBinding(u32 binding) const {
  for (u32 i = 0; i < bindingCount_; ++i) if (bindings_[i].binding == binding) return &bindings_[i];
  return nullptr;
}

bool VulkanComputeKernel::writeBuffer(u32 binding, VkDescriptorType type,
                                      const VulkanBuffer &buffer, u64 offsetBytes, u64 rangeBytes) {
  if (!buffer.isReady()) return false;
  return writeBuffer(binding, type, buffer.handle(), buffer.sizeBytes(), offsetBytes, rangeBytes);
}

bool VulkanComputeKernel::writeBuffer(u32 binding, VkDescriptorType type, VkBuffer buffer,
                                      u64 bufferSizeBytes, u64 offsetBytes, u64 rangeBytes) {
  const auto *declared = findBinding(binding);
  if (!isReady() || declared == nullptr || declared->type != type || buffer == VK_NULL_HANDLE ||
      (type != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER && type != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) ||
      offsetBytes >= bufferSizeBytes) return false;
  if (rangeBytes == 0) rangeBytes = bufferSizeBytes - offsetBytes;
  if (rangeBytes > bufferSizeBytes - offsetBytes) return false;
  VkDescriptorBufferInfo info{buffer, offsetBytes, rangeBytes};
  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = descriptorSet_; write.dstBinding = binding; write.descriptorCount = 1;
  write.descriptorType = type; write.pBufferInfo = &info;
  vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
  writtenBindingsMask_ |= static_cast<u16>(1u << (declared - bindings_));
  return true;
}

bool VulkanComputeKernel::writeImage(u32 binding, VkDescriptorType type, VkImageView imageView,
                                     VkImageLayout layout, VkSampler sampler) {
  const auto *declared = findBinding(binding);
  if (!isReady() || declared == nullptr || declared->type != type || imageView == VK_NULL_HANDLE ||
      (type != VK_DESCRIPTOR_TYPE_STORAGE_IMAGE && type != VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE &&
       type != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) ||
      (type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER && sampler == VK_NULL_HANDLE)) return false;
  VkDescriptorImageInfo info{sampler, imageView, layout};
  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = descriptorSet_; write.dstBinding = binding; write.descriptorCount = 1;
  write.descriptorType = type; write.pImageInfo = &info;
  vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
  writtenBindingsMask_ |= static_cast<u16>(1u << (declared - bindings_));
  return true;
}

bool VulkanComputeKernel::writeSampler(u32 binding, VkSampler sampler) {
  const auto *declared = findBinding(binding);
  if (!isReady() || declared == nullptr || declared->type != VK_DESCRIPTOR_TYPE_SAMPLER ||
      sampler == VK_NULL_HANDLE) return false;
  VkDescriptorImageInfo info{sampler, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED};
  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = descriptorSet_; write.dstBinding = binding; write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER; write.pImageInfo = &info;
  vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
  writtenBindingsMask_ |= static_cast<u16>(1u << (declared - bindings_));
  return true;
}

bool VulkanComputeKernel::bindCommon(VkCommandBuffer commandBuffer, const void *pushConstants,
                                     u32 pushConstantBytes) const {
  if (!isReady() || commandBuffer == VK_NULL_HANDLE ||
      writtenBindingsMask_ != requiredBindingsMask_ || pushConstantBytes != pushConstantBytes_ ||
      (pushConstantBytes > 0 && pushConstants == nullptr)) return false;
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
  if (bindingCount_ > 0) vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
      pipelineLayout_, 0, 1, &descriptorSet_, 0, nullptr);
  if (pushConstantBytes > 0) vkCmdPushConstants(commandBuffer, pipelineLayout_,
      VK_SHADER_STAGE_COMPUTE_BIT, 0, pushConstantBytes, pushConstants);
  return true;
}

bool VulkanComputeKernel::recordDispatch(VkCommandBuffer commandBuffer,
                                         const ComputeDispatch &dispatch,
                                         const void *pushConstants, u32 pushConstantBytes) const {
  if (!isComputeDispatchValid(dispatch, limits_) ||
      !bindCommon(commandBuffer, pushConstants, pushConstantBytes)) return false;
  vkCmdDispatch(commandBuffer, dispatch.x, dispatch.y, dispatch.z); return true;
}

bool VulkanComputeKernel::recordDispatchIndirect(VkCommandBuffer commandBuffer,
                                                 const VulkanBuffer &arguments, u64 offsetBytes,
                                                 const void *pushConstants,
                                                 u32 pushConstantBytes) const {
  if (!arguments.isReady() ||
      (arguments.usage() & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT) == 0 ||
      !isComputeIndirectOffsetValid(offsetBytes, arguments.sizeBytes()) ||
      !bindCommon(commandBuffer, pushConstants, pushConstantBytes)) return false;
  vkCmdDispatchIndirect(commandBuffer, arguments.handle(), offsetBytes); return true;
}

VulkanComputeContext::~VulkanComputeContext() { shutdown(); }

bool VulkanComputeContext::initialize(VkDevice device, VkQueue queue, u32 queueFamily) {
  shutdown();
  if (device == VK_NULL_HANDLE || queue == VK_NULL_HANDLE || queueFamily == UINT32_MAX) return false;
  device_ = device; queue_ = queue;
  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.queueFamilyIndex = queueFamily;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_) != VK_SUCCESS) {
    shutdown(); return false;
  }
  VkCommandBufferAllocateInfo allocation{};
  allocation.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocation.commandPool = commandPool_; allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocation.commandBufferCount = 1;
  if (vkAllocateCommandBuffers(device_, &allocation, &commandBuffer_) != VK_SUCCESS) {
    shutdown(); return false;
  }
  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  if (vkCreateFence(device_, &fenceInfo, nullptr, &fence_) != VK_SUCCESS) {
    shutdown(); return false;
  }
  return true;
}

void VulkanComputeContext::shutdown() {
  if (device_ != VK_NULL_HANDLE) {
    if (pending_) vkDeviceWaitIdle(device_);
    if (fence_ != VK_NULL_HANDLE) vkDestroyFence(device_, fence_, nullptr);
    if (commandPool_ != VK_NULL_HANDLE) vkDestroyCommandPool(device_, commandPool_, nullptr);
  }
  device_ = VK_NULL_HANDLE; queue_ = VK_NULL_HANDLE; commandPool_ = VK_NULL_HANDLE;
  commandBuffer_ = VK_NULL_HANDLE; fence_ = VK_NULL_HANDLE; recording_ = false; pending_ = false;
}

VkCommandBuffer VulkanComputeContext::begin() {
  if (!isReady() || recording_ || pending_) return VK_NULL_HANDLE;
  if (vkResetFences(device_, 1, &fence_) != VK_SUCCESS ||
      vkResetCommandBuffer(commandBuffer_, 0) != VK_SUCCESS) return VK_NULL_HANDLE;
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  if (vkBeginCommandBuffer(commandBuffer_, &beginInfo) != VK_SUCCESS) return VK_NULL_HANDLE;
  recording_ = true;
  return commandBuffer_;
}

bool VulkanComputeContext::submit() { return submit(SubmitSync{}); }

bool VulkanComputeContext::submit(const SubmitSync &sync) {
  if (!recording_ || vkEndCommandBuffer(commandBuffer_) != VK_SUCCESS) return false;
  recording_ = false;
  if ((sync.waitCount > 0 && (sync.waitSemaphores == nullptr || sync.waitStages == nullptr)) ||
      (sync.signalCount > 0 && sync.signalSemaphores == nullptr)) return false;
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.waitSemaphoreCount = sync.waitCount;
  submitInfo.pWaitSemaphores = sync.waitSemaphores;
  submitInfo.pWaitDstStageMask = sync.waitStages;
  submitInfo.commandBufferCount = 1; submitInfo.pCommandBuffers = &commandBuffer_;
  submitInfo.signalSemaphoreCount = sync.signalCount;
  submitInfo.pSignalSemaphores = sync.signalSemaphores;
  if (vkQueueSubmit(queue_, 1, &submitInfo, fence_) != VK_SUCCESS) return false;
  pending_ = true;
  return true;
}

void cmdComputeBufferBarrier(VkCommandBuffer commandBuffer, VkBuffer buffer,
                             u64 offsetBytes, u64 sizeBytes,
                             VkPipelineStageFlags sourceStage, VkAccessFlags sourceAccess,
                             VkPipelineStageFlags destinationStage, VkAccessFlags destinationAccess,
                             u32 sourceQueueFamily, u32 destinationQueueFamily) {
  if (commandBuffer == VK_NULL_HANDLE || buffer == VK_NULL_HANDLE || sizeBytes == 0) return;
  VkBufferMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
  barrier.srcAccessMask = sourceAccess; barrier.dstAccessMask = destinationAccess;
  barrier.srcQueueFamilyIndex = sourceQueueFamily; barrier.dstQueueFamilyIndex = destinationQueueFamily;
  barrier.buffer = buffer; barrier.offset = offsetBytes; barrier.size = sizeBytes;
  vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0,
                       0, nullptr, 1, &barrier, 0, nullptr);
}

void cmdComputeImageBarrier(VkCommandBuffer commandBuffer, VkImage image,
                            const VkImageSubresourceRange &range,
                            VkImageLayout oldLayout, VkImageLayout newLayout,
                            VkPipelineStageFlags sourceStage, VkAccessFlags sourceAccess,
                            VkPipelineStageFlags destinationStage, VkAccessFlags destinationAccess,
                            u32 sourceQueueFamily, u32 destinationQueueFamily) {
  if (commandBuffer == VK_NULL_HANDLE || image == VK_NULL_HANDLE ||
      range.levelCount == 0 || range.layerCount == 0) return;
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.srcAccessMask = sourceAccess; barrier.dstAccessMask = destinationAccess;
  barrier.oldLayout = oldLayout; barrier.newLayout = newLayout;
  barrier.srcQueueFamilyIndex = sourceQueueFamily; barrier.dstQueueFamilyIndex = destinationQueueFamily;
  barrier.image = image; barrier.subresourceRange = range;
  vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0,
                       0, nullptr, 0, nullptr, 1, &barrier);
}

bool VulkanComputeContext::wait(u64 timeoutNanoseconds) {
  if (!pending_) return isReady() && !recording_;
  const VkResult result = vkWaitForFences(device_, 1, &fence_, VK_TRUE, timeoutNanoseconds);
  if (result != VK_SUCCESS) return false;
  pending_ = false;
  return true;
}

void cmdComputeMemoryBarrier(VkCommandBuffer commandBuffer,
                             VkPipelineStageFlags destinationStage,
                             VkAccessFlags destinationAccess) {
  if (commandBuffer == VK_NULL_HANDLE) return;
  VkMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
  barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT; barrier.dstAccessMask = destinationAccess;
  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, destinationStage,
                       0, 1, &barrier, 0, nullptr, 0, nullptr);
}

} // namespace ae::rhi
