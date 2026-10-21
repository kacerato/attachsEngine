#include "harness.h"
#include "rhi/swapchain_frame_gate.h"
#include <type_traits>
using namespace ae;using namespace ae::rhi;
namespace {
template<class T>T object(u64 n){if constexpr(std::is_pointer_v<T>)return reinterpret_cast<T>(n);else return T(n);}
struct Calls {VkResult query=VK_SUCCESS,acquire=VK_SUCCESS,reset=VK_SUCCESS;u32 queries=0,acquires=0,resets=0;bool zeroTimeout=true,signaled=true;} calls;
VKAPI_ATTR VkResult VKAPI_CALL query(VkDevice,VkFence){++calls.queries;return calls.query==VK_SUCCESS&&!calls.signaled?VK_NOT_READY:calls.query;}
VKAPI_ATTR VkResult VKAPI_CALL acquire(VkDevice,VkSwapchainKHR,u64 timeout,VkSemaphore,VkFence fence,u32 *image){++calls.acquires;calls.zeroTimeout&=timeout==0&&fence==VK_NULL_HANDLE;*image=42;return calls.acquire;}
VKAPI_ATTR VkResult VKAPI_CALL reset(VkDevice,u32 count,const VkFence*){++calls.resets;if(calls.reset==VK_SUCCESS&&count==1)calls.signaled=false;return calls.reset;}
SwapchainStatus attempt(u32 *image){return acquireVulkanFrameNonblocking(object<VkDevice>(1),object<VkSwapchainKHR>(2),object<VkFence>(3),object<VkSemaphore>(4),image,query,acquire,reset);}
class FakeSwapchain final:public ISwapchain {
public:
 bool recreate(u32,u32)override{calls.signaled=true;return true;}
 SwapchainStatus pollFrameReady()override{return pollVulkanFrameReady(object<VkDevice>(1),object<VkFence>(3),query);}
 SwapchainStatus acquireNextImage(u32 *index)override{return attempt(index);}
 SwapchainStatus present(u32)override{return SwapchainStatus::Ok;}
 u32 width()const override{return 64;}u32 height()const override{return 64;}
};
}
AE_TEST(swapchain_frame_gate_busy_fence_returns_before_acquire_reset_or_camera_snapshot){
 calls={};calls.signaled=false;FakeSwapchain swapchain;u32 image=77,pose=0,snapshot=0;
 for(u32 n=1;n<=100;++n){pose=n;AE_EXPECT_TRUE(swapchain.pollFrameReady()==SwapchainStatus::NotReady,"normal input remains serviceable between busy retries");AE_EXPECT_TRUE(swapchain.acquireNextImage(&image)==SwapchainStatus::NotReady,"acquire rechecks readiness");}
 AE_EXPECT_EQ(calls.acquires,0u,"never asks compositor while previous GPU work is busy");AE_EXPECT_EQ(calls.resets,0u,"never reset an in-flight fence");AE_EXPECT_EQ(image,77u,"busy path preserves caller index");
 calls.signaled=true;AE_EXPECT_TRUE(swapchain.pollFrameReady()==SwapchainStatus::Ok,"previous submission completed");snapshot=pose;
 AE_EXPECT_TRUE(swapchain.acquireNextImage(&image)==SwapchainStatus::Ok&&snapshot==100,"camera snapshot follows readiness and latest accepted input");AE_EXPECT_TRUE(calls.zeroTimeout&&calls.acquires==1&&calls.resets==1,"single timeout0 acquisition resets only for actual submission");
}
AE_TEST(swapchain_frame_gate_compositor_retry_never_resets_fence_or_leaks_image_index){
 calls={};u32 image=77;calls.acquire=VK_NOT_READY;AE_EXPECT_TRUE(attempt(&image)==SwapchainStatus::NotReady,"no image available");calls.acquire=VK_TIMEOUT;AE_EXPECT_TRUE(attempt(&image)==SwapchainStatus::NotReady,"driver timeout maps to retry");
 AE_EXPECT_TRUE(calls.signaled&&calls.resets==0&&image==77&&calls.zeroTimeout,"retry leaves fence/semaphore contract and index intact");calls.acquire=VK_SUCCESS;AE_EXPECT_TRUE(attempt(&image)==SwapchainStatus::Ok&&image==42&&calls.resets==1,"later successful acquire arms exactly one submission");
}
AE_TEST(swapchain_frame_gate_lifecycle_and_errors_do_not_arm_unsignaled_submission){
 for(auto result:{VK_ERROR_OUT_OF_DATE_KHR,VK_ERROR_SURFACE_LOST_KHR,VK_ERROR_DEVICE_LOST}){calls={};calls.acquire=result;u32 image=77;const auto status=attempt(&image);
  AE_EXPECT_TRUE(status==(result==VK_ERROR_OUT_OF_DATE_KHR?SwapchainStatus::OutOfDateMustRecreate:result==VK_ERROR_SURFACE_LOST_KHR?SwapchainStatus::SurfaceLost:SwapchainStatus::FatalError),"preserve lifecycle recovery status");AE_EXPECT_TRUE(image==77&&calls.resets==0&&calls.signaled,"lifecycle failure never resets fence or commits image");}
 calls={};calls.query=VK_ERROR_DEVICE_LOST;u32 image=77;AE_EXPECT_TRUE(attempt(&image)==SwapchainStatus::FatalError&&calls.acquires==0,"device loss during readiness stops before acquire");
}
AE_TEST(swapchain_frame_gate_suboptimal_is_still_a_real_submission_and_reset_failure_is_fatal){
 calls={};calls.acquire=VK_SUBOPTIMAL_KHR;u32 image=77;AE_EXPECT_TRUE(attempt(&image)==SwapchainStatus::SuboptimalNeedsRecreate&&image==42&&calls.resets==1,"render then recreate suboptimal image");
 calls={};calls.reset=VK_ERROR_DEVICE_LOST;image=77;AE_EXPECT_TRUE(attempt(&image)==SwapchainStatus::FatalError&&image==77,"failed reset cannot publish acquired index as usable");
}
AE_TEST(swapchain_frame_gate_single_frame_depth_and_recreate_resume){
 calls={};FakeSwapchain swapchain;u32 image=0;AE_EXPECT_TRUE(swapchain.acquireNextImage(&image)==SwapchainStatus::Ok,"first signaled frame");AE_EXPECT_TRUE(swapchain.pollFrameReady()==SwapchainStatus::NotReady,"no second frame until prior fence signals");AE_EXPECT_EQ(calls.acquires,1u,"one frame maximum without backlog");
 AE_EXPECT_TRUE(swapchain.recreate(64,64)&&swapchain.pollFrameReady()==SwapchainStatus::Ok,"new swapchain/resume signaled fence is immediately eligible");AE_EXPECT_TRUE(swapchain.acquireNextImage(&image)==SwapchainStatus::Ok&&calls.acquires==2,"recreate retries normally");
}
AE_TEST(swapchain_frame_gate_readiness_hint_is_rechecked_and_invalid_arguments_do_no_work){
 calls={};FakeSwapchain swapchain;AE_EXPECT_TRUE(swapchain.pollFrameReady()==SwapchainStatus::Ok,"initial readiness is only a hint");calls.signaled=false;u32 image=77;
 AE_EXPECT_TRUE(swapchain.acquireNextImage(&image)==SwapchainStatus::NotReady&&calls.acquires==0&&calls.resets==0&&image==77,"hint cannot bypass acquire fence check or claim an image");
 calls={};AE_EXPECT_TRUE(attempt(nullptr)==SwapchainStatus::FatalError&&calls.queries==0,"null caller index rejected before Vulkan calls");
 AE_EXPECT_TRUE(pollVulkanFrameReady(VK_NULL_HANDLE,object<VkFence>(3),query)==SwapchainStatus::FatalError&&calls.queries==0,"missing device never queried");
}
