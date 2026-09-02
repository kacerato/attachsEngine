#pragma once

#include "core/base.h"
#include "rhi/memory_allocator.h"

#include <vulkan/vulkan.h>

namespace ae::rhi {

struct ComputeLimits final {
  bool supported = false;
  bool dedicatedQueue = false;
  u32 maximumWorkGroupCount[3]{};
  u32 maximumWorkGroupSize[3]{};
  u32 maximumWorkGroupInvocations = 0;
  u32 maximumPushConstantBytes = 0;
};

struct ComputeBindingDesc final {
  u32 binding = 0;
  VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
  u32 descriptorCount = 1;
  bool operator==(const ComputeBindingDesc &) const = default;
};

struct ComputeKernelDesc final {
  const u32 *spirv = nullptr;
  usize spirvBytes = 0;
  const ComputeBindingDesc *bindings = nullptr;
  u32 bindingCount = 0;
  u32 pushConstantBytes = 0;
  const char *entryPoint = "main";
  const char *debugName = "ComputeKernel";
};

struct ComputeDispatch final { u32 x = 1; u32 y = 1; u32 z = 1; };

struct ComputeShaderReflection final {
  ComputeBindingDesc bindings[16]{};
  u32 bindingCount = 0;
  u32 localSize[3]{1, 1, 1};
  bool hasPushConstants = false;
};

bool isComputeKernelDescValid(const ComputeKernelDesc &desc, const ComputeLimits &limits);
bool isComputeDispatchValid(const ComputeDispatch &dispatch, const ComputeLimits &limits);
bool isComputeLocalSizeValid(const ComputeShaderReflection &reflection,
                             const ComputeLimits &limits);
bool isComputeIndirectOffsetValid(u64 offsetBytes, u64 bufferSizeBytes);
bool reflectComputeShader(const u32 *spirv, usize spirvBytes, ComputeShaderReflection &outReflection);

class VulkanComputeKernel final {
public:
  VulkanComputeKernel() = default;
  ~VulkanComputeKernel();
  VulkanComputeKernel(const VulkanComputeKernel &) = delete;
  VulkanComputeKernel &operator=(const VulkanComputeKernel &) = delete;

  bool initialize(VkDevice device, const ComputeLimits &limits, const ComputeKernelDesc &desc,
                  VkPipelineCache pipelineCache = VK_NULL_HANDLE);
  void shutdown();
  bool writeBuffer(u32 binding, VkDescriptorType type, const VulkanBuffer &buffer,
                   u64 offsetBytes = 0, u64 rangeBytes = 0);
  bool writeBuffer(u32 binding, VkDescriptorType type, VkBuffer buffer, u64 bufferSizeBytes,
                   u64 offsetBytes = 0, u64 rangeBytes = 0);
  bool writeImage(u32 binding, VkDescriptorType type, VkImageView imageView,
                  VkImageLayout layout, VkSampler sampler = VK_NULL_HANDLE);
  bool writeSampler(u32 binding, VkSampler sampler);
  bool recordDispatch(VkCommandBuffer commandBuffer, const ComputeDispatch &dispatch,
                      const void *pushConstants = nullptr, u32 pushConstantBytes = 0) const;
  bool recordDispatchIndirect(VkCommandBuffer commandBuffer, const VulkanBuffer &arguments,
                              u64 offsetBytes, const void *pushConstants = nullptr,
                              u32 pushConstantBytes = 0) const;
  bool isReady() const { return pipeline_ != VK_NULL_HANDLE; }
  VkPipeline handle() const { return pipeline_; }
  VkPipelineLayout layout() const { return pipelineLayout_; }
  VkDescriptorSet descriptorSet() const { return descriptorSet_; }

private:
  const ComputeBindingDesc *findBinding(u32 binding) const;
  bool bindCommon(VkCommandBuffer commandBuffer, const void *pushConstants,
                  u32 pushConstantBytes) const;
  VkDevice device_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
  VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
  VkDescriptorSet descriptorSet_ = VK_NULL_HANDLE;
  VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
  VkPipeline pipeline_ = VK_NULL_HANDLE;
  ComputeLimits limits_{};
  ComputeBindingDesc bindings_[16]{};
  u32 bindingCount_ = 0;
  u16 requiredBindingsMask_ = 0;
  u16 writtenBindingsMask_ = 0;
  u32 pushConstantBytes_ = 0;
};

// Contexto de submissao compute reutilizavel. Mantem pool, command buffer e
// fence na fila escolhida pelo device; permite gravar, submeter de forma
// assincrona e aguardar explicitamente sem introduzir vkQueueWaitIdle.
class VulkanComputeContext final {
public:
  VulkanComputeContext() = default;
  ~VulkanComputeContext();
  VulkanComputeContext(const VulkanComputeContext &) = delete;
  VulkanComputeContext &operator=(const VulkanComputeContext &) = delete;

  bool initialize(VkDevice device, VkQueue queue, u32 queueFamily);
  void shutdown();
  VkCommandBuffer begin();
  struct SubmitSync final {
    const VkSemaphore *waitSemaphores = nullptr;
    const VkPipelineStageFlags *waitStages = nullptr;
    u32 waitCount = 0;
    const VkSemaphore *signalSemaphores = nullptr;
    u32 signalCount = 0;
  };
  bool submit();
  bool submit(const SubmitSync &sync);
  bool wait(u64 timeoutNanoseconds = UINT64_MAX);
  bool isReady() const { return commandPool_ != VK_NULL_HANDLE; }
  bool isPending() const { return pending_; }
  VkCommandBuffer commandBuffer() const { return commandBuffer_; }

private:
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue queue_ = VK_NULL_HANDLE;
  VkCommandPool commandPool_ = VK_NULL_HANDLE;
  VkCommandBuffer commandBuffer_ = VK_NULL_HANDLE;
  VkFence fence_ = VK_NULL_HANDLE;
  bool recording_ = false;
  bool pending_ = false;
};

void cmdComputeMemoryBarrier(VkCommandBuffer commandBuffer,
                             VkPipelineStageFlags destinationStage,
                             VkAccessFlags destinationAccess);
void cmdComputeBufferBarrier(VkCommandBuffer commandBuffer, VkBuffer buffer,
                             u64 offsetBytes, u64 sizeBytes,
                             VkPipelineStageFlags sourceStage, VkAccessFlags sourceAccess,
                             VkPipelineStageFlags destinationStage, VkAccessFlags destinationAccess,
                             u32 sourceQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                             u32 destinationQueueFamily = VK_QUEUE_FAMILY_IGNORED);
void cmdComputeImageBarrier(VkCommandBuffer commandBuffer, VkImage image,
                            const VkImageSubresourceRange &range,
                            VkImageLayout oldLayout, VkImageLayout newLayout,
                            VkPipelineStageFlags sourceStage, VkAccessFlags sourceAccess,
                            VkPipelineStageFlags destinationStage, VkAccessFlags destinationAccess,
                            u32 sourceQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                            u32 destinationQueueFamily = VK_QUEUE_FAMILY_IGNORED);

} // namespace ae::rhi
