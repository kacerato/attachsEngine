#pragma once
#include "rhi/device.h"
namespace ae::rhi {
// Readiness is a hint only; acquire repeats it. Never reset or wait a fence here.
// PFNs make the exact production Vulkan call sequence testable without a driver.
inline SwapchainStatus pollVulkanFrameReady(VkDevice device,VkFence fence,PFN_vkGetFenceStatus query){
 if(!device||!fence||!query)return SwapchainStatus::FatalError;
 const auto result=query(device,fence);
 return result==VK_SUCCESS?SwapchainStatus::Ok:result==VK_NOT_READY?SwapchainStatus::NotReady:SwapchainStatus::FatalError;
}
inline SwapchainStatus acquireVulkanFrameNonblocking(VkDevice device,VkSwapchainKHR swapchain,
 VkFence fence,VkSemaphore available,u32 *outImage,PFN_vkGetFenceStatus query,
 PFN_vkAcquireNextImageKHR acquire,PFN_vkResetFences reset){
 if(!outImage||!swapchain||!available||!acquire||!reset)return SwapchainStatus::FatalError;
 const auto ready=pollVulkanFrameReady(device,fence,query);if(ready!=SwapchainStatus::Ok)return ready;
 u32 image=0;
 const auto result=acquire(device,swapchain,0,available,VK_NULL_HANDLE,&image);
 if(result==VK_NOT_READY||result==VK_TIMEOUT)return SwapchainStatus::NotReady;
 if(result==VK_ERROR_OUT_OF_DATE_KHR)return SwapchainStatus::OutOfDateMustRecreate;
 if(result==VK_ERROR_SURFACE_LOST_KHR)return SwapchainStatus::SurfaceLost;
 if(result!=VK_SUCCESS&&result!=VK_SUBOPTIMAL_KHR)return SwapchainStatus::FatalError;
 // Acquire succeeded: a submission is now required. Retrying or lifecycle
 // statuses above never reset the fence or consume the acquire semaphore.
 if(reset(device,1,&fence)!=VK_SUCCESS)return SwapchainStatus::FatalError;
 *outImage=image;
 return result==VK_SUBOPTIMAL_KHR?SwapchainStatus::SuboptimalNeedsRecreate:SwapchainStatus::Ok;
}
}
