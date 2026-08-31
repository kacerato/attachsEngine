#pragma once

#include "core/base.h"
#include "core/gpu_pass_class.h"

#include <array>
#include <vulkan/vulkan.h>

namespace ae::rhi {

// As classes de passe vivem em core/gpu_pass_class.h porque o relatório de
// perfil e os marcadores de captura precisam da mesma lista; aqui só se mede.
struct GpuFrameTimings {
  double frameMs = 0.0;
  std::array<double, GpuPassClassCount> passesMs{};
};

// Timestamp GPU reutilizável por qualquer renderer. Mede execução real na
// fila, não tempo de CPU entre vkQueueSubmit/acquire. O pool acompanha um
// frame em voo hoje e pode ser replicado por frame slot quando 2–3 frames em
// voo forem habilitados.
class VulkanGpuFrameTimer final {
public:
  VulkanGpuFrameTimer() = default;
  ~VulkanGpuFrameTimer();

  VulkanGpuFrameTimer(const VulkanGpuFrameTimer &) = delete;
  VulkanGpuFrameTimer &operator=(const VulkanGpuFrameTimer &) = delete;

  bool initialize(VkDevice device, VkPhysicalDevice physicalDevice, u32 queueFamily);
  void shutdown();
  bool available() const { return queryPool_ != VK_NULL_HANDLE; }

  // collectPrevious deve ser chamado somente depois da fence do frame
  // anterior. begin/end são gravados no command buffer do frame atual.
  bool collectPrevious(GpuFrameTimings &timings);
  void begin(VkCommandBuffer commandBuffer);
  void markPassEnd(VkCommandBuffer commandBuffer, GpuPassClass pass);
  void end(VkCommandBuffer commandBuffer);

private:
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueryPool queryPool_ = VK_NULL_HANDLE;
  double timestampPeriodNanoseconds_ = 0.0;
  u64 timestampMask_ = ~u64{0};
  u32 recordedQueryCount_ = 0;
  bool pending_ = false;
};

} // namespace ae::rhi
