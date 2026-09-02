#pragma once

#include "rendergraph/render_graph.h"

#include <vulkan/vulkan.h>

namespace ae::rhi {

struct VulkanGraphResource final {
  VkBuffer buffer = VK_NULL_HANDLE;
  VkImage image = VK_NULL_HANDLE;
  VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  u32 mipLevels = 1;
  u32 arrayLayers = 1;
  u64 sizeBytes = 0;
};

bool graphStageToVulkan(const char *name, VkPipelineStageFlags &outFlags);
bool graphAccessToVulkan(const char *name, VkAccessFlags &outFlags);
bool graphLayoutToVulkan(rendergraph::ResourceLayout layout, VkImageLayout &outLayout);

// Traduz uma barreira compilada pelo Render Graph em uma barreira Vulkan
// real. Queue families diferentes executam tambem a transferencia de
// ownership; IGNORED mantem sincronizacao dentro da mesma fila.
bool cmdRenderGraphBarrier(VkCommandBuffer commandBuffer,
                           const rendergraph::Barrier &barrier,
                           const rendergraph::ResourceDesc &description,
                           const VulkanGraphResource &resource,
                           u32 sourceQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                           u32 destinationQueueFamily = VK_QUEUE_FAMILY_IGNORED);

} // namespace ae::rhi
