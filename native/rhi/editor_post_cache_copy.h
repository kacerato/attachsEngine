#pragma once
#include <vulkan/vulkan.h>
namespace ae::rhi {
// Both images have exactly the same VkFormat/extent. Copy preserves encoded
// UNORM/SRGB bits; no shader sample, blit, filtering or gamma roundtrip.
// Caller already acquired swapImage and owns the completed previous frame fence.
inline void recordEditorPostCacheCopy(VkCommandBuffer command,VkImage swapImage,VkImage cacheImage,
    uint32_t width,uint32_t height,bool capture,bool cacheInitialized,VkImageLayout presentationLayout,
    PFN_vkCmdPipelineBarrier barrier,PFN_vkCmdCopyImage copyImage){
  VkImageMemoryBarrier before[2]{};
  for(auto &b:before){b.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};}
  // Restore overwrites every pixel. The acquired image may be a different,
  // never-presented swapchain image, so discard rather than assume its layout.
  before[0].image=swapImage;before[0].oldLayout=capture?presentationLayout:VK_IMAGE_LAYOUT_UNDEFINED;
  before[0].newLayout=capture?VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  before[0].srcAccessMask=capture?VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT:0;
  before[0].dstAccessMask=capture?VK_ACCESS_TRANSFER_READ_BIT:VK_ACCESS_TRANSFER_WRITE_BIT;
  before[1].image=cacheImage;before[1].oldLayout=cacheInitialized?VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED;
  before[1].newLayout=capture?VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  before[1].srcAccessMask=cacheInitialized?VK_ACCESS_TRANSFER_READ_BIT:0;
  before[1].dstAccessMask=capture?VK_ACCESS_TRANSFER_WRITE_BIT:VK_ACCESS_TRANSFER_READ_BIT;
  barrier(command,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT|VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,2,before);
  VkImageCopy copy{};copy.srcSubresource=copy.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.extent={width,height,1};
  copyImage(command,capture?swapImage:cacheImage,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      capture?cacheImage:swapImage,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
  VkImageMemoryBarrier after[2]{before[0],before[1]};
  after[0].oldLayout=before[0].newLayout;after[0].newLayout=presentationLayout;
  after[0].srcAccessMask=before[0].dstAccessMask;after[0].dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  after[1].oldLayout=before[1].newLayout;after[1].newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  after[1].srcAccessMask=before[1].dstAccessMask;after[1].dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
  barrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT|VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      0,0,nullptr,0,nullptr,2,after);
}
}
