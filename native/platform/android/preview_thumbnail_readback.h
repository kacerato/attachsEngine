#pragma once
#include "renderer/render_view.h"
#include "rhi/memory_allocator.h"
#include <string>

namespace ae::platform::android {

struct PreviewThumbnailPixels {
  renderer::RenderViewSnapshot lease;
  // Display encoded sRGB RGBA8, tightly packed, in preview image row order.
  // This is the rendered target, not an icon/manual image or CPU substitute.
  std::vector<u8> rgba;
};
enum class ThumbnailReadbackStatus : u8 {Idle,Pending,Ready,Cancelled,Failed};

// One bounded copy at a time. Integrate after the independent preview's post
// render pass, whose final layout is SHADER_READ_ONLY_OPTIMAL. The renderer
// retains that image until this same submission's fence has completed.
class PreviewThumbnailReadback final {
public:
  static constexpr u32 MaximumSide=512;
  static constexpr u64 MaximumPixels=512ull*512;
  bool record(rhi::VulkanMemoryAllocator &,VkCommandBuffer,const rhi::VulkanImage &,
              const renderer::RenderViewSnapshot &,std::string &error);
  // Call only after vkQueueSubmit succeeds. A recorded but unsubmitted command
  // can be abandoned. take() polls before fence reset; the explicit waited path
  // below supports swapchains that wait and reset within acquireNextImage().
  bool submitted(VkFence);
  bool abandonUnsubmitted();
  void cancel(){deliver_=false;}
  ThumbnailReadbackStatus take(VkDevice,rhi::VulkanMemoryAllocator &,PreviewThumbnailPixels &,std::string &error);
  // Owner has SUCCESSFULLY WAITED for exactly the submitted fence and has NOT
  // submitted new work/reused this buffer. Fence may already have been reset by
  // acquire; polling its new state would confuse two different submissions.
  // Does not wait or query Vulkan, and refuses a different/null fence.
  ThumbnailReadbackStatus takeAfterSubmissionWait(VkFence,rhi::VulkanMemoryAllocator &,
      PreviewThumbnailPixels &,std::string &error);
  bool busy() const {return recorded_;}
  // Owner must have finished the queue/device before shutdown or recreation.
  void resetAfterDeviceIdle();
private:
  ThumbnailReadbackStatus consume(rhi::VulkanMemoryAllocator &,PreviewThumbnailPixels &,std::string &);
  rhi::VulkanBuffer buffer_;
  renderer::RenderViewSnapshot lease_;
  VkFence fence_=VK_NULL_HANDLE;
  VkFormat format_=VK_FORMAT_UNDEFINED;
  bool recorded_=false,deliver_=false;
};
} // namespace ae::platform::android
