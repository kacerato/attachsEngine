// Standalone real-device offscreen test. Uses Vulkan directly so it does not
// depend on editor state, Android, renderer mocks, image codecs or a window.
#define VK_NO_PROTOTYPES
#include "renderer/terrain_pipeline_publication_owner.h"
#include "renderer/material_override.h"
#include "renderer/terrain_surface_table.h"
#include "resources/terrain_surface_texture_refs.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <fstream>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#define VK_FUNCTIONS(X)                                                        \
  X(CreateInstance)                                                            \
  X(DestroyInstance)                                                           \
  X(EnumeratePhysicalDevices)                                                  \
  X(GetPhysicalDeviceProperties)                                               \
  X(GetPhysicalDeviceQueueFamilyProperties)                                    \
  X(GetPhysicalDeviceMemoryProperties)                                         \
  X(CreateDevice)                                                              \
  X(DestroyDevice)                                                             \
  X(GetDeviceQueue)                                                            \
  X(CreateQueryPool) X(DestroyQueryPool) X(CmdResetQueryPool) X(CmdWriteTimestamp) X(GetQueryPoolResults) \
  X(CreateBuffer)                                                              \
  X(DestroyBuffer) X(GetBufferMemoryRequirements) X(AllocateMemory) X(         \
      FreeMemory) X(BindBufferMemory) X(MapMemory) X(UnmapMemory)              \
      X(CreateImage) X(DestroyImage) X(GetImageMemoryRequirements) X(          \
          BindImageMemory) X(CreateImageView) X(DestroyImageView)              \
          X(CreateSampler) X(DestroySampler) X(CreateDescriptorSetLayout) X(   \
              DestroyDescriptorSetLayout) X(CreateDescriptorPool)              \
              X(DestroyDescriptorPool) X(AllocateDescriptorSets) X(            \
                  UpdateDescriptorSets) X(CreateRenderPass)                    \
                  X(DestroyRenderPass) X(CreateFramebuffer) X(                 \
                      DestroyFramebuffer) X(CreateShaderModule)                \
                      X(DestroyShaderModule) X(CreatePipelineLayout) X(        \
                          DestroyPipelineLayout) X(CreateGraphicsPipelines)    \
                          X(DestroyPipeline) X(CreateCommandPool) X(           \
                              DestroyCommandPool) X(AllocateCommandBuffers)    \
                              X(BeginCommandBuffer) X(EndCommandBuffer) X(     \
                                  CmdPipelineBarrier) X(CmdCopyBufferToImage)  \
                                  X(CmdCopyImageToBuffer) X(                   \
                                      CmdBeginRenderPass) X(CmdEndRenderPass) X(CmdNextSubpass) X(CmdClearAttachments)  \
                                      X(CmdBindPipeline)                       \
                                          X(CmdBindDescriptorSets)             \
                                              X(CmdPushConstants) X(CmdDraw) X(CmdCopyImage) X(CmdSetViewport) X(CmdSetScissor)   \
                                                  X(QueueSubmit)               \
                                                      X(QueueWaitIdle)         \
                                                          X(DeviceWaitIdle)
#define DECLARE(name) PFN_vk##name vk##name{};
VK_FUNCTIONS(DECLARE)
#undef DECLARE
void require(bool condition, const char *what) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    std::exit(1);
  }
}
void check(VkResult result, const char *what) {
  if (result != VK_SUCCESS) {
    std::fprintf(stderr, "VkResult %d: ", int(result));
    require(false, what);
  }
}
struct Buffer {
  VkBuffer handle{};
  VkDeviceMemory memory{};
  void *mapped{};
};
struct Image {
  VkImage handle{};
  VkDeviceMemory memory{};
  VkImageView view{};
  uint32_t width = 1;
  uint32_t levels = 1;
};
struct Fixture {
  VkInstance instance{};
  VkPhysicalDevice physical{};
  VkDevice device{};
  VkQueue queue{};
  uint32_t family{},timestampBits{};
  float timestampPeriod=0;
  VkCommandPool pool{};
  std::vector<Buffer> buffers;
  std::vector<Image> images;
  uint32_t memoryType(uint32_t bits, VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties p;
    vkGetPhysicalDeviceMemoryProperties(physical, &p);
    for (uint32_t i = 0; i < p.memoryTypeCount; ++i)
      if ((bits & (1u << i)) &&
          (p.memoryTypes[i].propertyFlags & flags) == flags)
        return i;
    require(false, "required memory type unavailable");
    return 0;
  }
  Fixture() {
#ifdef _WIN32
    auto library = LoadLibraryA("vulkan-1.dll");
    require(library != nullptr, "host Vulkan loader unavailable");
    auto get = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        GetProcAddress(library, "vkGetInstanceProcAddr"));
#else
    auto library = dlopen("libvulkan.so.1", RTLD_NOW);
    require(library != nullptr, "host Vulkan loader unavailable");
    auto get = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        dlsym(library, "vkGetInstanceProcAddr"));
#endif
    require(get != nullptr, "vkGetInstanceProcAddr unavailable");
    vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(
        get(nullptr, "vkCreateInstance"));
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo = &app;
    check(vkCreateInstance(&ci, nullptr, &instance), "create instance");
#define LOAD(name)                                                             \
  vk##name = reinterpret_cast<PFN_vk##name>(get(instance, "vk" #name));        \
  require(vk##name != nullptr, "load vk" #name);
    VK_FUNCTIONS(LOAD)
#undef LOAD
    uint32_t count = 0;
    check(vkEnumeratePhysicalDevices(instance, &count, nullptr),
          "enumerate device count");
    std::vector<VkPhysicalDevice> devices(count);
    check(vkEnumeratePhysicalDevices(instance, &count, devices.data()),
          "enumerate devices");
    for (auto candidate : devices) {
      VkPhysicalDeviceProperties candidateProperties;
      vkGetPhysicalDeviceProperties(candidate, &candidateProperties);
      if (candidateProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)
        continue;
      uint32_t n = 0;
      vkGetPhysicalDeviceQueueFamilyProperties(candidate, &n, nullptr);
      std::vector<VkQueueFamilyProperties> families(n);
      vkGetPhysicalDeviceQueueFamilyProperties(candidate, &n, families.data());
      for (uint32_t i = 0; i < n; ++i)
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
          physical = candidate;
          family = i;
          timestampBits=families[i].timestampValidBits;
          break;
        }
      if (physical)
        break;
    }
    require(physical != VK_NULL_HANDLE, "graphics Vulkan device unavailable");
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physical, &properties);
    timestampPeriod=properties.limits.timestampPeriod;
    std::printf("Real Vulkan device: %s\n", properties.deviceName);
    float priority = 1;
    VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qi.queueFamilyIndex = family;
    qi.queueCount = 1;
    qi.pQueuePriorities = &priority;
    VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    auto getFeatures = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures>(
        get(instance, "vkGetPhysicalDeviceFeatures"));
    VkPhysicalDeviceFeatures available{};
    getFeatures(physical, &available);
    require(available.shaderSampledImageArrayDynamicIndexing,
            "device needs dynamically uniform sampler array indexing");
    VkPhysicalDeviceFeatures enabled{};
    enabled.shaderSampledImageArrayDynamicIndexing = VK_TRUE;
    di.pEnabledFeatures = &enabled;
    di.queueCreateInfoCount = 1;
    di.pQueueCreateInfos = &qi;
    check(vkCreateDevice(physical, &di, nullptr, &device), "create device");
    vkGetDeviceQueue(device, family, 0, &queue);
    VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pi.queueFamilyIndex = family;
    check(vkCreateCommandPool(device, &pi, nullptr, &pool),
          "create command pool");
  }
  Buffer buffer(VkDeviceSize bytes, VkBufferUsageFlags usage) {
    Buffer b;
    VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    ci.size = bytes;
    ci.usage = usage;
    check(vkCreateBuffer(device, &ci, nullptr, &b.handle), "create buffer");
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device, b.handle, &req);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = memoryType(req.memoryTypeBits,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    check(vkAllocateMemory(device, &ai, nullptr, &b.memory), "allocate buffer");
    check(vkBindBufferMemory(device, b.handle, b.memory, 0), "bind buffer");
    check(vkMapMemory(device, b.memory, 0, VK_WHOLE_SIZE, 0, &b.mapped),
          "map buffer");
    buffers.push_back(b);
    return b;
  }
  Image image(uint32_t width, uint32_t height, VkImageUsageFlags usage,uint32_t levels=1,VkFormat format=VK_FORMAT_R8G8B8A8_UNORM,VkImageAspectFlags aspect=VK_IMAGE_ASPECT_COLOR_BIT) {
    Image im;
    im.width = width;
    im.levels = levels;
    VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = format;
    ci.extent = {width, height, 1};
    ci.mipLevels = levels;ci.arrayLayers = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    ci.usage = usage;
    check(vkCreateImage(device, &ci, nullptr, &im.handle), "create image");
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(device, im.handle, &req);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex =
        memoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    check(vkAllocateMemory(device, &ai, nullptr, &im.memory), "allocate image");
    check(vkBindImageMemory(device, im.handle, im.memory, 0), "bind image");
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = im.handle;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = ci.format;
    vi.subresourceRange = {aspect, 0, levels, 0, 1};
    check(vkCreateImageView(device, &vi, nullptr, &im.view),
          "create image view");
    images.push_back(im);
    return im;
  }
  VkCommandBuffer command() {
    VkCommandBufferAllocateInfo ai{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = pool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    VkCommandBuffer cmd;
    check(vkAllocateCommandBuffers(device, &ai, &cmd), "allocate commands");
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    check(vkBeginCommandBuffer(cmd, &bi), "begin commands");
    return cmd;
  }
  void submit(VkCommandBuffer cmd) {
    check(vkEndCommandBuffer(cmd), "end commands");
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    check(vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE), "submit");
    check(vkQueueWaitIdle(queue), "wait real GPU");
  }
  void transition(VkCommandBuffer cmd, const Image &im, VkImageLayout old,
                  VkImageLayout next, VkAccessFlags src, VkAccessFlags dst,
                  VkPipelineStageFlags from, VkPipelineStageFlags to) {
    VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.oldLayout = old;
    b.newLayout = next;
    b.srcAccessMask = src;
    b.dstAccessMask = dst;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = im.handle;
    b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, im.levels, 0, 1};
    vkCmdPipelineBarrier(cmd, from, to, 0, 0, nullptr, 0, nullptr, 1, &b);
  }
  ~Fixture() {
    vkDeviceWaitIdle(device);
    for (auto &im : images) {
      vkDestroyImageView(device, im.view, nullptr);
      vkDestroyImage(device, im.handle, nullptr);
      vkFreeMemory(device, im.memory, nullptr);
    }
    for (auto &b : buffers) {
      vkUnmapMemory(device, b.memory);
      vkDestroyBuffer(device, b.handle, nullptr);
      vkFreeMemory(device, b.memory, nullptr);
    }
    vkDestroyCommandPool(device, pool, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
  }
};
VkShaderModule shader(VkDevice device, const std::string &path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  require(bool(file), "compiled production shader unavailable");
  auto size = file.tellg();
  require(size > 0 && size % 4 == 0, "valid SPIR-V size");
  std::vector<uint32_t> code(size_t(size) / 4);
  file.seekg(0);
  file.read(reinterpret_cast<char *>(code.data()), size);
  VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  ci.codeSize = size_t(size);
  ci.pCode = code.data();
  VkShaderModule result;
  check(vkCreateShaderModule(device, &ci, nullptr, &result), "create shader");
  return result;
}
#include "rhi/editor_post_cache_copy.h"
#include "renderer/editor_post_cache_key.h"
int main(int argc,char **argv){
  require(argc==2,"usage: editor_post_cache_gpu <shader directory>");Fixture f;const std::string dir=argv[1];
  auto vert=shader(f.device,dir+"/cache.vert.spv"),postFrag=shader(f.device,dir+"/post.frag.spv"),uiFrag=shader(f.device,dir+"/ui.frag.spv");
  auto source=f.image(32,32,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
  auto staging=f.buffer(32*32*4,VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  for(uint32_t y=0;y<32;++y)for(uint32_t x=0;x<32;++x){auto *p=static_cast<uint8_t*>(staging.mapped)+(y*32+x)*4;p[0]=uint8_t((x*17+y*3)&255);p[1]=uint8_t((x/3+y/2)%2?230:19);p[2]=uint8_t(y*8);p[3]=255;}
  auto upload=f.command();f.transition(upload,source,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
  VkBufferImageCopy sourceCopy{};sourceCopy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};sourceCopy.imageExtent={32,32,1};
  vkCmdCopyBufferToImage(upload,staging.handle,source.handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&sourceCopy);
  f.transition(upload,source,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);f.submit(upload);
  VkSamplerCreateInfo si{};si.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;si.magFilter=si.minFilter=VK_FILTER_LINEAR;si.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;si.addressModeU=si.addressModeV=si.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  VkSampler sampler{};check(vkCreateSampler(f.device,&si,nullptr,&sampler),"sampler");
  auto environment=f.buffer(4096,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),exposure=f.buffer(16,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
  std::memset(environment.mapped,0,4096);std::memset(exposure.mapped,0,16);auto *env=static_cast<float*>(environment.mapped);
  env[12]=1;env[45]=0;env[46]=1;env[56]=env[61]=env[66]=1;env[824]=env[825]=.125f;env[826]=env[827]=.75f;env[870]=env[871]=1;
  std::array<VkDescriptorSetLayoutBinding,8> bindings{};for(uint32_t i=0;i<8;++i){bindings[i].binding=i;bindings[i].descriptorCount=1;bindings[i].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;bindings[i].descriptorType=i==3?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:i==4?VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;}
  VkDescriptorSetLayoutCreateInfo li{};li.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;li.bindingCount=8;li.pBindings=bindings.data();VkDescriptorSetLayout layout{};check(vkCreateDescriptorSetLayout(f.device,&li,nullptr,&layout),"post descriptors");
  VkDescriptorPoolSize sizes[]{{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,6},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1}};
  VkDescriptorPoolCreateInfo dpi{};dpi.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;dpi.maxSets=1;dpi.poolSizeCount=3;dpi.pPoolSizes=sizes;VkDescriptorPool pool{};check(vkCreateDescriptorPool(f.device,&dpi,nullptr,&pool),"pool");
  VkDescriptorSetAllocateInfo ai{};ai.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;ai.descriptorPool=pool;ai.descriptorSetCount=1;ai.pSetLayouts=&layout;VkDescriptorSet set{};check(vkAllocateDescriptorSets(f.device,&ai,&set),"set");
  VkDescriptorImageInfo imageInfo{sampler,source.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};VkDescriptorBufferInfo envInfo{environment.handle,0,4096},exposureInfo{exposure.handle,0,16};
  VkWriteDescriptorSet writes[8]{};for(uint32_t i=0;i<8;++i){auto &w=writes[i];w.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;w.dstSet=set;w.dstBinding=i;w.descriptorCount=1;w.descriptorType=bindings[i].descriptorType;if(i==3)w.pBufferInfo=&envInfo;else if(i==4)w.pBufferInfo=&exposureInfo;else w.pImageInfo=&imageInfo;}vkUpdateDescriptorSets(f.device,8,writes,0,nullptr);
  VkPushConstantRange range{VK_SHADER_STAGE_FRAGMENT_BIT,0,128};VkPipelineLayoutCreateInfo pli{};pli.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;pli.setLayoutCount=1;pli.pSetLayouts=&layout;pli.pushConstantRangeCount=1;pli.pPushConstantRanges=&range;VkPipelineLayout postLayout{};check(vkCreatePipelineLayout(f.device,&pli,nullptr,&postLayout),"post layout");
  range.size=16;pli.setLayoutCount=0;VkPipelineLayout uiLayout{};check(vkCreatePipelineLayout(f.device,&pli,nullptr,&uiLayout),"UI layout");
  const auto pipeline=[&](VkRenderPass pass,VkShaderModule frag,VkPipelineLayout pipelineLayout,bool blend){
    VkPipelineShaderStageCreateInfo stages[2]{};for(auto &s:stages){s.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;s.pName="main";}stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[0].module=vert;stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[1].module=frag;
    VkPipelineVertexInputStateCreateInfo vi{};vi.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;VkPipelineInputAssemblyStateCreateInfo ia{};ia.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo vp{};vp.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;vp.viewportCount=vp.scissorCount=1;VkPipelineRasterizationStateCreateInfo raster{};raster.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;raster.polygonMode=VK_POLYGON_MODE_FILL;raster.lineWidth=1;
    VkPipelineMultisampleStateCreateInfo ms{};ms.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState ba{};ba.blendEnable=blend;ba.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;ba.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;ba.colorBlendOp=VK_BLEND_OP_ADD;ba.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE;ba.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;ba.alphaBlendOp=VK_BLEND_OP_ADD;ba.colorWriteMask=15;
    VkPipelineColorBlendStateCreateInfo cb{};cb.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;cb.attachmentCount=1;cb.pAttachments=&ba;VkDynamicState dynamicStates[]{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};VkPipelineDynamicStateCreateInfo ds{};ds.sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;ds.dynamicStateCount=2;ds.pDynamicStates=dynamicStates;
    VkGraphicsPipelineCreateInfo pi{};pi.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;pi.stageCount=2;pi.pStages=stages;pi.pVertexInputState=&vi;pi.pInputAssemblyState=&ia;pi.pViewportState=&vp;pi.pRasterizationState=&raster;pi.pMultisampleState=&ms;pi.pColorBlendState=&cb;pi.pDynamicState=&ds;pi.layout=pipelineLayout;pi.renderPass=pass;VkPipeline result{};check(vkCreateGraphicsPipelines(f.device,VK_NULL_HANDLE,1,&pi,nullptr,&result),"post/UI pipeline");return result;
  };
  unsigned comparisons=0;
  for(auto format:{VK_FORMAT_R8G8B8A8_UNORM,VK_FORMAT_B8G8R8A8_UNORM,VK_FORMAT_R8G8B8A8_SRGB,VK_FORMAT_B8G8R8A8_SRGB})for(uint32_t width:{32u,48u}){
    const uint32_t height=32;auto target=f.image(width,height,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,1,format);
    auto cached=f.image(width,height,VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,1,format);auto readback=f.buffer(width*height*4,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    VkAttachmentDescription attachment{};attachment.format=format;attachment.samples=VK_SAMPLE_COUNT_1_BIT;attachment.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;attachment.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;attachment.finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference ref{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};VkSubpassDescription subpass{};subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;subpass.colorAttachmentCount=1;subpass.pColorAttachments=&ref;
    VkRenderPassCreateInfo ri{};ri.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;ri.attachmentCount=1;ri.pAttachments=&attachment;ri.subpassCount=1;ri.pSubpasses=&subpass;
    VkRenderPass clear{},load{};check(vkCreateRenderPass(f.device,&ri,nullptr,&clear),"clear post");attachment.loadOp=VK_ATTACHMENT_LOAD_OP_LOAD;attachment.initialLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;check(vkCreateRenderPass(f.device,&ri,nullptr,&load),"load UI");
    VkFramebufferCreateInfo fi{};fi.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;fi.renderPass=clear;fi.attachmentCount=1;fi.pAttachments=&target.view;fi.width=width;fi.height=height;fi.layers=1;VkFramebuffer fb{};check(vkCreateFramebuffer(f.device,&fi,nullptr,&fb),"framebuffer");
    auto postPipeline=pipeline(clear,postFrag,postLayout,false),uiPipeline=pipeline(clear,uiFrag,uiLayout,true);bool cacheInitialized=false,targetReadback=false;
    const auto draw=[&](bool fused,bool restore,bool capture,float alpha,bool effects,bool clipped){
      auto cmd=f.command();if(restore){require(cacheInitialized,"restore requires captured scene");if(targetReadback)f.transition(cmd,target,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);ae::rhi::recordEditorPostCacheCopy(cmd,target.handle,cached.handle,width,height,false,true,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,vkCmdPipelineBarrier,vkCmdCopyImage);}
      VkClearValue background{};background.color={{.028f,.032f,.039f,1}};VkRenderPassBeginInfo begin{};begin.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;begin.renderPass=restore?load:clear;begin.framebuffer=fb;begin.renderArea.extent={width,height};begin.clearValueCount=restore?0u:1u;begin.pClearValues=&background;
      VkViewport viewport{0,0,float(width),float(height),0,1};VkRect2D full{{0,0},{width,height}},scene=clipped?VkRect2D{{4,4},{width-8,height-8}}:full;
      if(!restore){vkCmdBeginRenderPass(cmd,&begin,VK_SUBPASS_CONTENTS_INLINE);vkCmdSetViewport(cmd,0,1,&viewport);vkCmdSetScissor(cmd,0,1,&scene);vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,postPipeline);vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,postLayout,0,1,&set,0,nullptr);
        env[48]=effects?1.f:0.f;env[49]=.85f;env[50]=.85f;env[51]=1.35f;
        std::array<float,32> push{};push[0]=push[1]=1.f/32;push[2]=effects?1.f:0.f;push[3]=1;push[4]=1.15f;push[5]=effects?.08f:0.f;push[6]=.1f;push[8]=effects?1.15f:1.f;push[9]=1.04f;push[10]=(format==VK_FORMAT_R8G8B8A8_SRGB||format==VK_FORMAT_B8G8R8A8_SRGB)?0.f:1.f;push[12]=push[13]=1;push[14]=1.732f;push[15]=float(width)/height;push[23]=.1f;push[31]=1000;
        vkCmdPushConstants(cmd,postLayout,VK_SHADER_STAGE_FRAGMENT_BIT,0,128,push.data());vkCmdDraw(cmd,3,1,0,0);
        if(!fused){vkCmdEndRenderPass(cmd);if(capture){ae::rhi::recordEditorPostCacheCopy(cmd,target.handle,cached.handle,width,height,true,cacheInitialized,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,vkCmdPipelineBarrier,vkCmdCopyImage);cacheInitialized=true;}}
      }
      if(!fused||restore){begin.renderPass=load;begin.clearValueCount=0;vkCmdBeginRenderPass(cmd,&begin,VK_SUBPASS_CONTENTS_INLINE);}
      vkCmdSetViewport(cmd,0,1,&viewport);VkRect2D uiRect{{0,2},{width/2,height-4}};vkCmdSetScissor(cmd,0,1,&uiRect);vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,uiPipeline);
      const float ui[]{.41f,.82f,.17f,alpha};vkCmdPushConstants(cmd,uiLayout,VK_SHADER_STAGE_FRAGMENT_BIT,0,16,ui);vkCmdDraw(cmd,3,1,0,0);vkCmdEndRenderPass(cmd);
      f.transition(cmd,target,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={width,height,1};vkCmdCopyImageToBuffer(cmd,target.handle,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback.handle,1,&copy);f.submit(cmd);targetReadback=true;
      const auto *bytes=static_cast<const uint8_t*>(readback.mapped);return std::vector<uint8_t>(bytes,bytes+width*height*4);
    };
    const auto equal=[&](const auto &a,const auto &b,const char *label){require(a.size()==b.size(),"same pixel extent");for(size_t i=0;i<a.size();++i)require(std::abs(int(a[i])-int(b[i]))<=1,label);++comparisons;};
    for(bool clipped:{false,true})for(bool effects:{false,true})for(float alpha:{0.f,.05f,.5f,1.f}){
      auto fused=draw(true,false,false,alpha,effects,clipped),separate=draw(false,false,true,alpha,effects,clipped);equal(fused,separate,"fused/separate production post FXAA+AO+bloom, exact scissor and UI blend <=1UNORM");
      const float changedAlpha=1.f-alpha;auto expected=draw(true,false,false,changedAlpha,effects,clipped),hit=draw(false,true,false,changedAlpha,effects,clipped);equal(expected,hit,"cached scene plus changed UI <=1UNORM; no stale UI captured");
      if(alpha==0.f)require(fused!=expected,"changing overlay alpha must change rendered pixels");
    }
    // A cached scene can exist before another swapchain image's first acquire.
    // Restore into a genuinely fresh image: no fake prior PRESENT transition.
    const auto firstAcquireExpected=draw(true,false,false,.5f,true,false);
    draw(false,false,true,.5f,true,false);
    vkDestroyFramebuffer(f.device,fb,nullptr);
    target=f.image(width,height,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,1,format);
    fi.pAttachments=&target.view;
    check(vkCreateFramebuffer(f.device,&fi,nullptr,&fb),"first-acquire framebuffer");
    targetReadback=false;
    equal(firstAcquireExpected,draw(false,true,false,.5f,true,false),"first acquire restores full cached scene from UNDEFINED");
    vkDestroyPipeline(f.device,uiPipeline,nullptr);vkDestroyPipeline(f.device,postPipeline,nullptr);vkDestroyFramebuffer(f.device,fb,nullptr);vkDestroyRenderPass(f.device,load,nullptr);vkDestroyRenderPass(f.device,clear,nullptr);
  }
  std::printf("PASS: %u fused/separate/cache pixel comparisons, UNORM/SRGB, UI alpha mutation, production FXAA/AO/bloom, scissor and resize\n",comparisons);
  vkDestroyPipelineLayout(f.device,uiLayout,nullptr);vkDestroyPipelineLayout(f.device,postLayout,nullptr);vkDestroyDescriptorPool(f.device,pool,nullptr);vkDestroyDescriptorSetLayout(f.device,layout,nullptr);vkDestroySampler(f.device,sampler,nullptr);vkDestroyShaderModule(f.device,uiFrag,nullptr);vkDestroyShaderModule(f.device,postFrag,nullptr);vkDestroyShaderModule(f.device,vert,nullptr);
}
