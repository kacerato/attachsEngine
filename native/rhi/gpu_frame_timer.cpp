#include "rhi/gpu_frame_timer.h"

#include <array>

namespace ae::rhi {

VulkanGpuFrameTimer::~VulkanGpuFrameTimer() { shutdown(); }

bool VulkanGpuFrameTimer::initialize(VkDevice device, VkPhysicalDevice physicalDevice,
                                     u32 queueFamily) {
  shutdown();
  if (device == VK_NULL_HANDLE || physicalDevice == VK_NULL_HANDLE) return false;

  u32 familyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, nullptr);
  if (queueFamily >= familyCount || familyCount == 0) return false;
  std::array<VkQueueFamilyProperties, 32> families{};
  const u32 queriedCount = familyCount > families.size() ? static_cast<u32>(families.size()) : familyCount;
  u32 boundedCount = queriedCount;
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &boundedCount, families.data());
  if (queueFamily >= boundedCount || families[queueFamily].timestampValidBits == 0) return false;

  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(physicalDevice, &properties);
  if (properties.limits.timestampPeriod <= 0.0f) return false;

  VkQueryPoolCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
  info.queryType = VK_QUERY_TYPE_TIMESTAMP;
  info.queryCount = GpuFramePassCount + 1;
  if (vkCreateQueryPool(device, &info, nullptr, &queryPool_) != VK_SUCCESS) return false;

  device_ = device;
  timestampPeriodNanoseconds_ = properties.limits.timestampPeriod;
  const u32 validBits = families[queueFamily].timestampValidBits;
  timestampMask_ = validBits >= 64 ? ~u64{0} : (u64{1} << validBits) - 1;
  return true;
}

void VulkanGpuFrameTimer::shutdown() {
  if (device_ != VK_NULL_HANDLE && queryPool_ != VK_NULL_HANDLE)
    vkDestroyQueryPool(device_, queryPool_, nullptr);
  device_ = VK_NULL_HANDLE;
  queryPool_ = VK_NULL_HANDLE;
  timestampPeriodNanoseconds_ = 0.0;
  timestampMask_ = ~u64{0};
  recordedQueryCount_ = 0;
  pending_ = false;
}

bool VulkanGpuFrameTimer::collectPrevious(GpuFrameTimings &timings) {
  timings = {};
  if (!available() || !pending_) return false;
  std::array<u64, GpuFramePassCount + 1> timestamps{};
  const VkResult result = vkGetQueryPoolResults(
      device_, queryPool_, 0, timestamps.size(), sizeof(timestamps), timestamps.data(), sizeof(u64),
      VK_QUERY_RESULT_64_BIT);
  if (result != VK_SUCCESS) return false;
  const auto toMilliseconds = [&](u64 begin, u64 end) {
    return static_cast<double>((end - begin) & timestampMask_) *
           timestampPeriodNanoseconds_ / 1'000'000.0;
  };
  timings.frameMs = toMilliseconds(timestamps.front(), timestamps.back());
  for (u32 pass = 0; pass < GpuFramePassCount; ++pass)
    timings.passesMs[pass] = toMilliseconds(timestamps[pass], timestamps[pass + 1]);
  pending_ = false;
  return true;
}

void VulkanGpuFrameTimer::begin(VkCommandBuffer commandBuffer) {
  if (!available() || commandBuffer == VK_NULL_HANDLE) return;
  pending_ = false;
  recordedQueryCount_ = 1;
  vkCmdResetQueryPool(commandBuffer, queryPool_, 0, GpuFramePassCount + 1);
  vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPool_, 0);
}

void VulkanGpuFrameTimer::markPassEnd(VkCommandBuffer commandBuffer, GpuFramePass pass) {
  if (!available() || commandBuffer == VK_NULL_HANDLE) return;
  const u32 targetQuery = static_cast<u32>(pass) + 1;
  if (targetQuery >= GpuFramePassCount + 1 || targetQuery < recordedQueryCount_) return;
  // Preenche checkpoints omitidos no mesmo ponto. Assim renderers simples
  // podem marcar apenas Geometry, enquanto o formato permanece global.
  while (recordedQueryCount_ <= targetQuery) {
    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        queryPool_, recordedQueryCount_++);
  }
}

void VulkanGpuFrameTimer::end(VkCommandBuffer commandBuffer) {
  if (!available() || commandBuffer == VK_NULL_HANDLE || recordedQueryCount_ == 0) return;
  while (recordedQueryCount_ < GpuFramePassCount + 1) {
    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        queryPool_, recordedQueryCount_++);
  }
  pending_ = true;
}

} // namespace ae::rhi
