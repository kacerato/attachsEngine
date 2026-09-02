#include "rhi/render_graph_vulkan.h"

namespace ae::rhi {

bool cmdRenderGraphBarrier(VkCommandBuffer commandBuffer,
                           const rendergraph::Barrier &barrier,
                           const rendergraph::ResourceDesc &description,
                           const VulkanGraphResource &resource,
                           u32 sourceQueueFamily, u32 destinationQueueFamily) {
  if (commandBuffer == VK_NULL_HANDLE) return false;
  VkPipelineStageFlags sourceStage = 0, destinationStage = 0;
  VkAccessFlags sourceAccess = 0, destinationAccess = 0;
  if (!graphStageToVulkan(barrier.srcStage, sourceStage) ||
      !graphStageToVulkan(barrier.dstStage, destinationStage) ||
      !graphAccessToVulkan(barrier.srcAccess, sourceAccess) ||
      !graphAccessToVulkan(barrier.dstAccess, destinationAccess)) return false;

  if (description.kind == rendergraph::ResourceDesc::Kind::Buffer) {
    if (resource.buffer == VK_NULL_HANDLE || resource.sizeBytes == 0) return false;
    VkBufferMemoryBarrier vkBarrier{};
    vkBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    vkBarrier.srcAccessMask = sourceAccess; vkBarrier.dstAccessMask = destinationAccess;
    vkBarrier.srcQueueFamilyIndex = sourceQueueFamily;
    vkBarrier.dstQueueFamilyIndex = destinationQueueFamily;
    vkBarrier.buffer = resource.buffer; vkBarrier.offset = 0; vkBarrier.size = resource.sizeBytes;
    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0,
                         0, nullptr, 1, &vkBarrier, 0, nullptr);
    return true;
  }

  if (resource.image == VK_NULL_HANDLE || resource.aspectMask == 0 ||
      resource.mipLevels == 0 || resource.arrayLayers == 0) return false;
  VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  if (!graphLayoutToVulkan(barrier.oldLayout, oldLayout) ||
      !graphLayoutToVulkan(barrier.newLayout, newLayout)) return false;
  VkImageMemoryBarrier vkBarrier{};
  vkBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  vkBarrier.srcAccessMask = sourceAccess; vkBarrier.dstAccessMask = destinationAccess;
  vkBarrier.oldLayout = oldLayout; vkBarrier.newLayout = newLayout;
  vkBarrier.srcQueueFamilyIndex = sourceQueueFamily;
  vkBarrier.dstQueueFamilyIndex = destinationQueueFamily;
  vkBarrier.image = resource.image;
  vkBarrier.subresourceRange = {resource.aspectMask, 0, resource.mipLevels, 0, resource.arrayLayers};
  vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0,
                       0, nullptr, 0, nullptr, 1, &vkBarrier);
  return true;
}

} // namespace ae::rhi
