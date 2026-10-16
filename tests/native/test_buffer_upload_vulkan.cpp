#include "harness.h"
#include "rhi/upload_context.h"
#include <cstring>

using namespace ae;
namespace {
struct VulkanUploadFixture {
  VkInstance instance{};VkDevice device{};
  rhi::VulkanMemoryAllocator allocator;
  rhi::VulkanUploadContext upload;
  bool ready=false;
  VulkanUploadFixture() {
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.apiVersion=VK_API_VERSION_1_1;
    VkInstanceCreateInfo create{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};create.pApplicationInfo=&app;
    if(vkCreateInstance(&create,nullptr,&instance)!=VK_SUCCESS)return;
    u32 count=0;vkEnumeratePhysicalDevices(instance,&count,nullptr);
    std::vector<VkPhysicalDevice> devices(count);
    if(!count || vkEnumeratePhysicalDevices(instance,&count,devices.data())!=VK_SUCCESS)return;
    for(auto physical:devices) {
      u32 families=0;vkGetPhysicalDeviceQueueFamilyProperties(physical,&families,nullptr);
      std::vector<VkQueueFamilyProperties> queues(families);vkGetPhysicalDeviceQueueFamilyProperties(physical,&families,queues.data());
      for(u32 family=0;family<families;++family)if(queues[family].queueFlags&VK_QUEUE_GRAPHICS_BIT) {
        float priority=1;VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queue.queueFamilyIndex=family;queue.queueCount=1;queue.pQueuePriorities=&priority;
        VkDeviceCreateInfo info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};info.queueCreateInfoCount=1;info.pQueueCreateInfos=&queue;
        if(vkCreateDevice(physical,&info,nullptr,&device)!=VK_SUCCESS)continue;
        VkPhysicalDeviceProperties properties;vkGetPhysicalDeviceProperties(physical,&properties);
        std::printf("Vulkan device: %s\n",properties.deviceName);
        ready=allocator.initialize(instance,physical,device,rhi::MemoryBudgetConfig::fromTotalBytes(128u<<20)) && upload.initialize(device,family);
        return;
      }
    }
  }
  ~VulkanUploadFixture(){if(device)vkDeviceWaitIdle(device);upload.shutdown();allocator.shutdown();if(device)vkDestroyDevice(device,nullptr);if(instance)vkDestroyInstance(instance,nullptr);}
  bool buffer(usize size,rhi::VulkanBuffer &out) {
    rhi::BufferDesc desc{};desc.sizeBytes=size;desc.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    desc.cpuAccess=rhi::CpuAccess::Random;desc.preferDeviceMemory=false;return allocator.createBuffer(desc,&out);
  }
  bool equals(rhi::VulkanBuffer &buffer,std::span<const u8> bytes) {
    return allocator.invalidateBuffer(buffer) && buffer.mappedData() && std::memcmp(buffer.mappedData(),bytes.data(),bytes.size())==0;
  }
};
}
AE_TEST(vulkan_buffer_ranges_publish_batch_and_reuse_fence) {
  VulkanUploadFixture f;AE_EXPECT_TRUE(f.ready,"real host Vulkan device and allocator required");
  std::vector<u8> old(1u<<20,13),next=old,oldIndex(4096,0),nextIndex=oldIndex;
  next[3001]=91;next[720000]=99;nextIndex[4095]=12;
  rhi::VulkanBuffer vertices,indices;
  AE_EXPECT_TRUE(f.buffer(old.size(),vertices) && f.buffer(oldIndex.size(),indices),"create test buffers");
  constexpr auto stage=VK_PIPELINE_STAGE_HOST_BIT;constexpr auto access=VK_ACCESS_HOST_READ_BIT;
  AE_EXPECT_TRUE(f.upload.uploadBuffer(f.allocator,old.data(),old.size(),vertices,stage,access),"initial vertex upload");
  AE_EXPECT_TRUE(f.upload.uploadBuffer(f.allocator,oldIndex.data(),oldIndex.size(),indices,stage,access),"initial index upload");
  const auto v=planBufferUpdate(old,next),i=planBufferUpdate(oldIndex,nextIndex);
  const rhi::BufferRangeUpload batch[]{{next,v.ranges,&vertices,stage,access},{nextIndex,i.ranges,&indices,stage,access}};
  AE_EXPECT_TRUE(f.upload.uploadBufferRanges(f.allocator,batch),"one partial batch uploads both buffers");
  AE_EXPECT_TRUE(f.equals(vertices,next) && f.equals(indices,nextIndex),"real Vulkan readback matches every byte, including unchanged regions");
  // A bad second update must refuse the batch before modifying the first.
  const BufferUpdateRange invalid{4096,4};
  const rhi::BufferRangeUpload rejected[]{{old,v.ranges,&vertices,stage,access},{oldIndex,{&invalid,1},&indices,stage,access}};
  AE_EXPECT_TRUE(!f.upload.uploadBufferRanges(f.allocator,rejected),"out-of-bounds second buffer rejects the entire batch");
  AE_EXPECT_TRUE(f.equals(vertices,next) && f.equals(indices,nextIndex),"rejected batch leaves both revisions intact");
  AE_EXPECT_TRUE(f.upload.uploadBuffer(f.allocator,old.data(),old.size(),vertices,stage,access),"full upload after partial batch resets fence correctly");
  AE_EXPECT_TRUE(f.equals(vertices,old),"full upload after partial batch readback");
  const rhi::BufferRangeUpload empty{old,{},&vertices,stage,access};
  AE_EXPECT_TRUE(f.upload.uploadBufferRanges(f.allocator,{&empty,1}),"unchanged buffer is a successful no-op");
  std::printf("Partial geometry: %llu bytes staged of %zu bytes\n",static_cast<unsigned long long>(v.uploadedBytes+i.uploadedBytes),old.size()+oldIndex.size());
}
