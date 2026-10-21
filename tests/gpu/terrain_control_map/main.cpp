// Standalone real-device offscreen test. Uses Vulkan directly so it does not
// depend on editor state, Android, renderer mocks, image codecs or a window.
#define VK_NO_PROTOTYPES
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
                                      CmdBeginRenderPass) X(CmdEndRenderPass)  \
                                      X(CmdBindPipeline)                       \
                                          X(CmdBindDescriptorSets)             \
                                              X(CmdPushConstants) X(CmdDraw)   \
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
  Image image(uint32_t width, uint32_t height, VkImageUsageFlags usage,uint32_t levels=1) {
    Image im;
    im.width = width;
    im.levels = levels;
    VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = VK_FORMAT_R8G8B8A8_UNORM;
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
    vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, levels, 0, 1};
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
int main(int argc, char **argv) {
  require(argc == 2||argc==3,
          "usage: astra_terrain_control_gpu <compiled shader directory> [original 1x256 Geo RGBA]");
  Fixture f;
  constexpr uint32_t width = 32, height = 32, controlWidth = 65;
  std::vector<uint8_t> originalGeo(1024,255);
  if(argc==3){std::ifstream image(argv[2],std::ios::binary);image.read(reinterpret_cast<char*>(originalGeo.data()),1024);require(image.gcount()==1024&&image.peek()==EOF,"actual original Geo decode is 1x256 RGBA without downscale");}
  std::array<Image, 11> textures;
  std::array<std::array<uint8_t, 4>, 10> colors{{{255, 0, 0, 255},
                                                 {0, 255, 0, 255},
                                                 {0, 0, 255, 255},
                                                 {255, 255, 255, 255},
                                                 {0, 0, 0, 0},
                                                 {128, 128, 255, 255},
                                                 {191, 128, 238, 255},
                                                 {51, 102, 153, 204},
                                                 {0, 0, 0, 0},
                                                 {26, 51, 77, 255}}};
  std::vector<uint8_t> pixels(controlWidth * controlWidth * 4, 0);
  for (uint32_t y = 0; y < controlWidth; ++y)
    for (uint32_t x = 0; x < controlWidth; ++x) {
      auto *p = pixels.data() + (y * controlWidth + x) * 4;
      p[0] = 255;
      if (x >= 31 && x <= 33 && y >= 16 && y <= 48) {
        p[0] = 0;
        p[1] = 255;
      }
      if (x == 0) {
        p[0] = 0;
        p[2] = 255;
      }
      if (x == controlWidth - 1) {
        p[0] = 0;
        p[3] = 255;
      }
    }
  auto upload = f.command();
  std::vector<uint8_t> gradient(controlWidth * controlWidth * 4, 255);
  for (uint32_t y = 0; y < controlWidth; ++y)
    for (uint32_t x = 0; x < controlWidth; ++x) {
      auto p = (y * controlWidth + x) * 4;
      gradient[p] = uint8_t(x * 255 / 64);
      gradient[p + 1] = uint8_t(y * 255 / 64);
      gradient[p + 2] = 0;
    }
  for (uint32_t i = 0; i < 11; ++i) {
    uint32_t n = i == 4 || i == 8 ? controlWidth : 1;
    uint32_t imageHeight = i==10?256:n;
    textures[i] =
        f.image(n, imageHeight,
                VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    auto staging =
        f.buffer(n * imageHeight * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    std::memcpy(staging.mapped,
                i == 10 ? originalGeo.data()
                : i == 4   ? pixels.data()
                : i == 8 ? gradient.data()
                         : colors[i].data(),
                n * imageHeight * 4);
    f.transition(
        upload, textures[i], VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {n, imageHeight, 1};
    vkCmdCopyBufferToImage(upload, staging.handle, textures[i].handle,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    f.transition(upload, textures[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                 VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
  }
  auto clampMipImage=f.image(4,1,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,2);
  const uint8_t maskMips[]{0,0,0,255,85,0,0,255,170,0,0,255,255,0,0,255, 0,0,0,255,255,0,0,255};
  auto clampMipUpload=f.buffer(sizeof maskMips,VK_BUFFER_USAGE_TRANSFER_SRC_BIT);std::memcpy(clampMipUpload.mapped,maskMips,sizeof maskMips);
  f.transition(upload,clampMipImage,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
  std::array<VkBufferImageCopy,2> mipCopies{};
  for(uint32_t i=0;i<2;++i){mipCopies[i].bufferOffset=i?16:0;mipCopies[i].imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,i,0,1};mipCopies[i].imageExtent={4u>>i,1,1};}
  vkCmdCopyBufferToImage(upload,clampMipUpload.handle,clampMipImage.handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,2,mipCopies.data());
  f.transition(upload,clampMipImage,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
  f.submit(upload);
  constexpr size_t materialBytes =
      ae::renderer::MaterialExtensionFloats * sizeof(float);
  auto rows = f.buffer(materialBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
  auto surfaceRows = f.buffer(16384, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
  auto *row = static_cast<float *>(rows.mapped);
  ae::renderer::TerrainControlBinding control;
  control.uvScale[0] = float(controlWidth - 1) / controlWidth;
  control.uvScale[1] = float(controlWidth - 1) / controlWidth;
  control.uvOffset[0] = .5f / controlWidth;
  control.uvOffset[1] = .5f / controlWidth;
  ae::renderer::materialExtensionEntry(
      {}, ae::renderer::MapMaterialTerrain,
      ae::renderer::MaterialExtensionNoTexture, row,
      ae::renderer::MaterialExtensionNoTexture, &control, 4);
  VkSamplerCreateInfo sci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  sci.magFilter = sci.minFilter = VK_FILTER_LINEAR;
  sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  sci.addressModeU = sci.addressModeV = sci.addressModeW =
      VK_SAMPLER_ADDRESS_MODE_REPEAT;
  VkSampler sampler;
  check(vkCreateSampler(f.device, &sci, nullptr, &sampler),
        "create production-repeat sampler");
  sci.addressModeU=sci.addressModeV=sci.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  VkSampler clampSampler;check(vkCreateSampler(f.device,&sci,nullptr,&clampSampler),"create Snow mask clamp sampler");
  sci.minLod=sci.maxLod=1;VkSampler clampMipSampler;check(vkCreateSampler(f.device,&sci,nullptr,&clampMipSampler),"force Snow mask actual mip1 clamp sampling");
  VkDescriptorSetLayoutBinding textureBinding{
      0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 14,
      VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
  VkDescriptorSetLayoutBinding rowBindings[]{
      {16, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT,
       nullptr},
      {18, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT,
       nullptr}};
  std::array<VkDescriptorSetLayout, 2> layouts;
  VkDescriptorSetLayoutCreateInfo li{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  li.bindingCount = 1;
  li.pBindings = &textureBinding;
  check(vkCreateDescriptorSetLayout(f.device, &li, nullptr, &layouts[0]),
        "texture layout");
  li.bindingCount = 2;
  li.pBindings = rowBindings;
  check(vkCreateDescriptorSetLayout(f.device, &li, nullptr, &layouts[1]),
        "material row layout");
  VkDescriptorPoolSize sizes[]{{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 14},
                               {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2}};
  VkDescriptorPoolCreateInfo dpi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  dpi.maxSets = 2;
  dpi.poolSizeCount = 2;
  dpi.pPoolSizes = sizes;
  VkDescriptorPool descriptorPool;
  check(vkCreateDescriptorPool(f.device, &dpi, nullptr, &descriptorPool),
        "descriptor pool");
  VkDescriptorSetAllocateInfo dai{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  dai.descriptorPool = descriptorPool;
  dai.descriptorSetCount = 2;
  dai.pSetLayouts = layouts.data();
  std::array<VkDescriptorSet, 2> sets;
  check(vkAllocateDescriptorSets(f.device, &dai, sets.data()),
        "descriptor sets");
  std::array<VkDescriptorImageInfo, 14> infos;
  for (size_t i = 0; i < 11; ++i)
    infos[i] = {sampler, textures[i].view,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  infos[11]={clampSampler,textures[8].view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  infos[12]={clampSampler,textures[7].view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  infos[13]={clampMipSampler,clampMipImage.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  VkDescriptorBufferInfo rinfo{rows.handle, 0, materialBytes};
  VkDescriptorBufferInfo sinfo{surfaceRows.handle, 0, 16384};
  std::array<VkWriteDescriptorSet, 3> writes{};
  for (auto &w : writes)
    w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].dstSet = sets[0];
  writes[0].dstBinding = 0;
  writes[0].descriptorCount = 14;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  writes[0].pImageInfo = infos.data();
  writes[1].dstSet = sets[1];
  writes[1].dstBinding = 16;
  writes[1].descriptorCount = 1;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[1].pBufferInfo = &rinfo;
  writes[2] = writes[1];
  writes[2].dstBinding = 18;
  writes[2].pBufferInfo = &sinfo;
  vkUpdateDescriptorSets(f.device, 3, writes.data(), 0, nullptr);
  VkAttachmentDescription attachment{};
  attachment.format = VK_FORMAT_R8G8B8A8_UNORM;
  attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  VkAttachmentReference reference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &reference;
  VkRenderPassCreateInfo rpi{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
  rpi.attachmentCount = 1;
  rpi.pAttachments = &attachment;
  rpi.subpassCount = 1;
  rpi.pSubpasses = &subpass;
  VkRenderPass renderPass;
  check(vkCreateRenderPass(f.device, &rpi, nullptr, &renderPass),
        "offscreen render pass");
  auto target = f.image(width, height,
                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                            VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
  VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
  fi.renderPass = renderPass;
  fi.attachmentCount = 1;
  fi.pAttachments = &target.view;
  fi.width = width;
  fi.height = height;
  fi.layers = 1;
  VkFramebuffer framebuffer;
  check(vkCreateFramebuffer(f.device, &fi, nullptr, &framebuffer),
        "offscreen framebuffer");
  auto readback =
      f.buffer(width * height * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  VkPushConstantRange push{VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4};
  VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pl.setLayoutCount = 2;
  pl.pSetLayouts = layouts.data();
  pl.pushConstantRangeCount = 1;
  pl.pPushConstantRanges = &push;
  VkPipelineLayout pipelineLayout;
  check(vkCreatePipelineLayout(f.device, &pl, nullptr, &pipelineLayout),
        "pipeline layout");
  std::string directory = argv[1];
  VkShaderModule vert = shader(f.device, directory + "/control.vert.spv");
  std::array<VkPipeline, 3> pipelines;
  for (size_t i = 0; i < 3; ++i) {
    auto frag = shader(f.device, directory + (i == 2 ? "/pbr.frag.spv"
                                              : i    ? "/fallback.frag.spv"
                                                     : "/control.frag.spv"));
    std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
    for (auto &s : stages)
      s.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";
    VkPipelineVertexInputStateCreateInfo vi{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo ia{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkViewport viewport{0, 0, float(width), float(height), 0, 1};
    VkRect2D scissor{{0, 0}, {width, height}};
    VkPipelineViewportStateCreateInfo vp{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = vp.scissorCount = 1;
    vp.pViewports = &viewport;
    vp.pScissors = &scissor;
    VkPipelineRasterizationStateCreateInfo raster{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.lineWidth = 1;
    VkPipelineMultisampleStateCreateInfo ms{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = 15;
    VkPipelineColorBlendStateCreateInfo cb{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = 1;
    cb.pAttachments = &blend;
    VkGraphicsPipelineCreateInfo pi{
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pi.stageCount = 2;
    pi.pStages = stages.data();
    pi.pVertexInputState = &vi;
    pi.pInputAssemblyState = &ia;
    pi.pViewportState = &vp;
    pi.pRasterizationState = &raster;
    pi.pMultisampleState = &ms;
    pi.pColorBlendState = &cb;
    pi.layout = pipelineLayout;
    pi.renderPass = renderPass;
    check(vkCreateGraphicsPipelines(f.device, VK_NULL_HANDLE, 1, &pi, nullptr,
                                    &pipelines[i]),
          "production shader graphics pipeline");
    vkDestroyShaderModule(f.device, frag, nullptr);
  }
  auto expectPixel = [&](uint32_t x, std::array<uint8_t, 4> expected,
                         const char *label,int tolerance=3) {
    auto *p = static_cast<uint8_t *>(readback.mapped) + (16 * width + x) * 4;
    for (size_t c = 0; c < 4; ++c)
      if (std::abs(int(p[c]) - int(expected[c])) > tolerance) {
        std::fprintf(stderr, "%s at x=%u got RGBA %u,%u,%u,%u\n", label, x,
                     p[0], p[1], p[2], p[3]);
        std::fflush(stderr);
        require(false, label);
      }
  };
  VkQueryPool timestamps{};
  if(f.timestampBits){
    VkQueryPoolCreateInfo qi{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    qi.queryType=VK_QUERY_TYPE_TIMESTAMP;qi.queryCount=2;
    check(vkCreateQueryPool(f.device,&qi,nullptr,&timestamps),"create GPU timestamp queries");
  }
  std::array<std::vector<uint8_t>,312> modePixels;
  for (uint32_t mode = 0; mode < 312; ++mode) {
    if(mode==189&&argc!=3)continue;
    if (mode == 29 || mode==50) {
      auto staging = f.buffer(4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
      const uint8_t negativeZ[4]{uint8_t(mode==29?128:191), 128, uint8_t(mode==29?0:238), 255};
      std::memcpy(staging.mapped, negativeZ, 4);
      auto update = f.command();
      f.transition(
          update, textures[6], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
          VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
          VK_PIPELINE_STAGE_TRANSFER_BIT);
      VkBufferImageCopy copy{};
      copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
      copy.imageExtent = {1, 1, 1};
      vkCmdCopyBufferToImage(update, staging.handle, textures[6].handle,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
      f.transition(update, textures[6], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                   VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                   VK_PIPELINE_STAGE_TRANSFER_BIT,
                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
      f.submit(update);
    }
    if (mode >= 6) {
      const float span = float(controlWidth - 1) / controlWidth;
      control.uvScale[0] = .5f * span;
      control.uvScale[1] = .25f * span;
      control.uvOffset[0] = .25f * span + .5f / controlWidth;
      control.uvOffset[1] = (mode == 6 ? .375f * span : 0) + .5f / controlWidth;
      ae::renderer::materialExtensionEntry(
          {}, ae::renderer::MapMaterialTerrain,
          ae::renderer::MaterialExtensionNoTexture, row,
          ae::renderer::MaterialExtensionNoTexture, &control, 4);
    }
    if (mode >= 8) {
      using namespace ae::resources;
      using namespace ae::renderer;
      TerrainSurfaceBinding binding;
      binding.localMetersPerUv = 1;
      TerrainSurfaceLayer layer;
      layer.identity = {11, 1};
      layer.projection = TerrainProjection::UV;
      auto mapSlot = [&](TerrainSurfaceMap &map, uint32_t slot,
                         TerrainSurfaceChannel channel) {
        map.texture = {22, slot + 1};
        map.channel = channel;
        map.linear = true;
        map.factor = 1;
      };
      mapSlot(layer.maps[0], 0, TerrainSurfaceChannel::RGB);
      mapSlot(layer.maps[1], 5, TerrainSurfaceChannel::NormalRGB);
      mapSlot(layer.maps[2], 7, TerrainSurfaceChannel::A);
      mapSlot(layer.maps[3], 7, TerrainSurfaceChannel::B);
      mapSlot(layer.maps[4], 7, TerrainSurfaceChannel::R);
      layer.maps[4].remapMin = .1f;
      layer.maps[4].remapMax = .9f;
      layer.maps[4].invert = true;
      layer.maps[4].factor = .5f;
      mapSlot(layer.maps[5], 7, TerrainSurfaceChannel::G);
      layer.maps[5].factor = .5f;
      mapSlot(layer.maps[6], 9, TerrainSurfaceChannel::RGB);
      layer.maps[6].factor = .5f;
      if (mode == 8) {
        layer.saturation = 0;
        layer.tint = {.5f, 1, 1, 1};
      }
      if (mode == 12) {
        mapSlot(layer.maps[0], 8, TerrainSurfaceChannel::RGB);
        layer.scaleX = 2;
        layer.scaleY = .5f;
        layer.rotation = 90;
        layer.offsetX = 1.25f;
        layer.offsetY = .25f;
      }
      if (mode == 28) {
        mapSlot(layer.maps[0], 8, TerrainSurfaceChannel::RGB);
        layer.scaleX = 2;
        layer.scaleY = .5f;
        layer.offsetX = .5f;
        layer.offsetY = .25f;
      }
      if (mode == 18 || mode == 19) {
        mapSlot(layer.maps[0], 8, TerrainSurfaceChannel::RGB);
        layer.projection = mode == 18 ? TerrainProjection::LocalTriplanar
                                      : TerrainProjection::WorldTriplanar;
      }
      if (mode == 21) {
        mapSlot(layer.maps[0], 3, TerrainSurfaceChannel::RGB);
        auto &m = layer.maps[0];
        m.colorRemapMin = {.1f, .2f, .3f, .4f};
        m.colorRemapMax = {.8f, .7f, .6f, .9f};
        m.colorFactor = {.5f, .5f, .5f, 1};
        m.factor = .5f;
        m.invert = true;
      }
      if (mode == 16 || mode == 17) {
        mapSlot(layer.maps[1], 6, TerrainSurfaceChannel::NormalRGB);
        if (mode == 17)
          layer.maps[1].factor = 0;
      }
      if (mode == 29) {
        mapSlot(layer.maps[1], 6, TerrainSurfaceChannel::NormalRGB);
        layer.maps[1].factor = 0;
      }
      if (mode == 22)
        mapSlot(layer.maps[1], 6, TerrainSurfaceChannel::NormalRG);
      if (mode == 25) {
        auto &heightMap = layer.maps[2];
        heightMap.texture = {};
        heightMap.factor = .35f;
        heightMap.remapMin = .2f;
        heightMap.remapMax = .8f;
        heightMap.invert = true;
      }
      if (mode == 26) {
        auto &rough = layer.maps[4];
        rough.texture = {};
        rough.remapMin = .2f;
        rough.remapMax = .8f;
      }
      if (mode == 27) {
        layer.maps[2].texture = {};
        layer.maps[2].factor = 0;
        layer.maps[6].texture = {};
        layer.maps[6].factor = 0;
      }
      if (mode >= 30 && mode<50) {
        auto &n=binding.surface.detailNoise;
        n.enabled=mode!=30; n.projection=TerrainProjection::UV;
        n.fadeStart=10; n.fadeEnd=20;
        mapSlot(n.map,7,TerrainSurfaceChannel::R);
        if(mode==31)n.strength=0;
        if(mode==34||mode==35||mode==46){binding.surface.distanceNoise=n;n.enabled=false;}
        if(mode==36)layer.globalDetailParticipation=0;
        if(mode==37)layer.globalDetailParticipation=.5f;
        if(mode==38)layer.detailNoise=n;
        if(mode==39){
          mapSlot(layer.maps[0],3,TerrainSurfaceChannel::RGB);
          n.map.channel=TerrainSurfaceChannel::RGB;n.map.factor=.5f;
        }
        if(mode==40){n.map.remapMin=.1f;n.map.remapMax=.9f;n.map.invert=true;n.map.factor=.5f;}
        if(mode==41){n.map.remapMin=.5f;n.map.remapMax=.5f;}
        if(mode>=42&&mode<=44){mapSlot(n.map,8,TerrainSurfaceChannel::R);n.scale=mode==43?2:.5f;}
        if(mode==47){layer.detailNoise=n;layer.globalDetailParticipation=0;}
        if(mode==49)n.enabled=false;
        if(mode==44){n.projection=TerrainProjection::WorldTriplanar;n.scale=1;}
      }
      if(mode>=50&&mode<74){
        TerrainSurfaceNoise n;n.enabled=true;n.projection=TerrainProjection::UV;
        n.fadeStart=10;n.fadeEnd=20;mapSlot(n.map,6,TerrainSurfaceChannel::NormalRGB);
        const uint32_t index=mode<=55?(mode-50)%3:0;
        if(mode>=53 && mode<=55)layer.normalNoise[index]=n;
        else binding.surface.normalNoise[index]=n;
        if(mode==56)binding.surface.layers.clear();
        if(mode==56)layer.globalNormalParticipation[0]=0;
        if(mode==57)binding.surface.normalNoise[0].enabled=false;
        if(mode==58)binding.surface.normalNoise[0].map.factor=0;
        if(mode==59)binding.surface.normalNoise[0].strength=0;
        if(mode==60)mapSlot(binding.surface.normalNoise[0].map,5,TerrainSurfaceChannel::NormalRGB);
        if(mode==61)binding.surface.normalNoise[0].map.channel=TerrainSurfaceChannel::NormalRG;
        if(mode==63||mode==64){
          binding.surface.normalNoise={};
          for(uint32_t j=0;j<3;++j)(mode==63?binding.surface.normalNoise[j]:layer.normalNoise[j])=n;
        }
        if(mode>=65&&mode<=70){
          binding.surface.normalNoise={};
          TerrainAntiTileEntry entry;entry.identity={33,1};entry.normal=n;
          binding.surface.antiTileArray.enabled=mode!=66;
          if(mode==68||mode==69){entry.normal.enabled=false;auto &color=mode==68?entry.detail:entry.distance;
            color=n;mapSlot(color.map,7,TerrainSurfaceChannel::R);}
          binding.surface.antiTileArray.entries.push_back(entry);
          layer.antiTileEntry=entry.identity;
          if(mode==67)layer.antiTileParticipation[0]=0;
          if(mode==70){auto other=entry;other.identity={33,2};other.normal.strength=0;
            binding.surface.antiTileArray.entries.insert(binding.surface.antiTileArray.entries.begin(),other);}
        }
        if(mode==71)layer.normalNoise[0]=n;
        if(mode==73)layer.faceNormals=true;
      }
      if(mode>=74&&mode<88){
        mapSlot(layer.maps[0],8,TerrainSurfaceChannel::RGB);
        layer.resampleAlbedo=mode!=79&&mode!=80&&mode!=86&&mode!=87;
        layer.resampleNormal=mode==79;layer.resampleHeightBlend=mode==80||mode==81||mode==86;
        layer.resampleScale=mode==78?1:2;layer.resampleStart=10;layer.resampleEnd=20;
        layer.resampleBlend=(mode==74||mode==78||mode==79||mode==80||mode==87)?TerrainDistanceBlend::Constant:TerrainDistanceBlend::Crossfade;
        if(mode==79)mapSlot(layer.maps[1],8,TerrainSurfaceChannel::NormalRG);
        if(mode==80||mode==81||mode==86)mapSlot(layer.maps[2],8,TerrainSurfaceChannel::R);
        if(mode>=82&&mode<=84){
          layer.resampleBlend=TerrainDistanceBlend::Noise;
          layer.resampleNoise.enabled=true;layer.resampleNoise.fadeStart=100;layer.resampleNoise.fadeEnd=200;
          mapSlot(layer.resampleNoise.map,7,TerrainSurfaceChannel::R);
          if(mode==83)layer.resampleNoise.strength=0;
        }
        if(mode==86)layer.resampleBlend=TerrainDistanceBlend::Noise;
      }
      if(mode>=88&&mode<116){
        auto &c=layer.cluster;
        const bool baseStochastic=mode>=101&&mode<=104;
        c.enabled=!baseStochastic;c.stochastic=mode>=101&&mode<=114;
        c.quality=mode==88||mode>=94&&mode<=102||mode==115?TerrainSurfaceQuality::Low:mode==89||mode==103?TerrainSurfaceQuality::Medium:TerrainSurfaceQuality::High;
        const uint32_t count=(mode>=91&&mode<=93||mode>=105&&mode<=114)?3:2;
        if(!baseStochastic)for(uint32_t v=0;v<count;++v){
          TerrainSurfaceVariant variant;variant.identity={44,v+1};variant.maps=layer.maps;
          mapSlot(variant.maps[0],v,TerrainSurfaceChannel::RGB);
          if(v==1)mapSlot(variant.maps[1],6,TerrainSurfaceChannel::NormalRGB);
          c.variants.push_back(variant);
        }
        c.selection.enabled=!c.stochastic&&mode!=93;
        c.selection.strength=8;c.selection.fadeStart=100;c.selection.fadeEnd=200;
        mapSlot(c.selection.map,3,TerrainSurfaceChannel::R);
        if(mode==92)c.contrast=2;
        if(mode==93)c.noiseBoost=2;
        if(mode==95){auto &v=c.variants[1];v.maps[4].texture={};v.maps[4].remapMin=0;v.maps[4].remapMax=1;v.maps[4].invert=false;v.maps[4].factor=.8f;v.maps[5].texture={};v.maps[5].factor=.25f;v.maps[3].texture={};v.maps[3].factor=.6f;}
        if(mode==96){auto &v=c.variants[1];v.maps[2].texture={};v.maps[2].factor=.3f;v.maps[6].texture={};v.maps[6].factor=1;v.maps[6].colorFactor={.2f,.4f,.6f,1};}
        if(mode>=97&&mode<=100){
          auto &v=c.variants[1];mapSlot(v.maps[0],8,TerrainSurfaceChannel::RGB);
          v.scale=2;v.offsetX=.5f;v.offsetY=.25f;layer.scaleX=9;layer.scaleY=17;
          if(mode==98)v.rotation=90;
          if(mode==99){layer.offsetX=.5f;layer.offsetY=.25f;v.offsetX=v.offsetY=0;}
          if(mode==100)layer.rotation=90;
        }
        if(baseStochastic)mapSlot(layer.maps[0],8,TerrainSurfaceChannel::RGB);
        if(mode==107||mode==108||mode==109){layer.resampleAlbedo=true;layer.resampleNormal=mode==108;layer.resampleHeightBlend=mode==108;layer.resampleScale=2;layer.resampleBlend=TerrainDistanceBlend::Constant;}
        if(mode==109)mapSlot(c.variants[1].maps[0],8,TerrainSurfaceChannel::RGB);
      }
      if(mode>=116&&mode<199){
        mapSlot(layer.maps[0],9,TerrainSurfaceChannel::RGB);
        auto &geo=binding.surface.geo;
        auto setupGeo=[&](){geo.mode=TerrainGeoMode::Albedo;mapSlot(geo.albedo,7,TerrainSurfaceChannel::RGB);};
        if(mode<=145||mode>=182){setupGeo();}
        if(mode==116)geo.mode=TerrainGeoMode::Off;
        if(mode==118)geo.blendStrength=0;
        if(mode==119)layer.geoParticipation=0;
        if(mode==120)layer.geoParticipation=.5f;
        if(mode==121)geo.blend=TerrainColorBlend::LighterColor;
        if(mode>=122&&mode<=124){geo.rangeEnabled=true;geo.range=mode==122?std::array<float,4>{-10,10,20,30}:mode==123?std::array<float,4>{-30,-20,-10,10}:std::array<float,4>{10,20,30,40};}
        if(mode>=125&&mode<=127){layer.geoHeightFilterEnabled=true;layer.geoHeightThreshold=mode==126?-.5f:.5f;if(mode==127)layer.geoHeightStrength=.5f;}
        if(mode>=128&&mode<=132){geo.mode=TerrainGeoMode::AlbedoNormal;mapSlot(geo.normal,6,mode==129?TerrainSurfaceChannel::NormalRG:TerrainSurfaceChannel::NormalRGB);if(mode==130)geo.normalStrength=0;if(mode==131)geo.normal.factor=0;if(mode==132)geo.normal.invert=true;}
        if(mode>=133&&mode<=139||mode==144||mode==145||mode>=182&&mode<=187){
          mapSlot(layer.maps[0],3,TerrainSurfaceChannel::RGB);mapSlot(geo.albedo,8,TerrainSurfaceChannel::RGB);geo.blendStrength=.5f;geo.scaleMeters=2;geo.offsetMeters=.5f;
          if(mode==134)geo.offsetMeters=1;
          if(mode==135||mode==136){geo.offsetMeters=0;if(mode==136)geo.scaleMeters=4;}
          if(mode==137){geo.curveEnabled=true;geo.offsetMeters=0;for(auto &key:geo.curve.keys)key.value=.25f;}
          if(mode==139)geo.albedo.colorFactor[3]=.25f;
          if(mode==144)geo.offsetMeters=1.5f;
          if(mode==145){layer.maps[0].texture={};layer.maps[0].colorFactor={0,0,0,1};geo.blend=TerrainColorBlend::LighterColor;geo.blendStrength=1;geo.offsetMeters=0;}
          if(mode>=182){geo.curveEnabled=true;geo.offsetMeters=0;geo.curveScaleMeters=2;geo.curve.keys.back().value=.25f;geo.curve.keys.front().incoming=geo.curve.keys.front().outgoing=geo.curve.keys.back().incoming=geo.curve.keys.back().outgoing=AnimationTangentMode::Linear;
            if(mode==183)geo.curveScaleMeters=4;if(mode==184)geo.curveOffsetMeters=.5f;if(mode==185)geo.curveRotationDegrees=90;
            if(mode==186)for(auto &key:geo.curve.keys){key.incoming=key.outgoing=AnimationTangentMode::Free;key.inSlope=key.outSlope=0;}
            if(mode==187)geo.curveEnabled=false;
          }
        }
        if(mode>=140&&mode<=143){layer.geoHeightFilterEnabled=true;layer.geoHeightThreshold=mode==140?.8f:.7f;geo.heightContrast=mode==141?1:10;if(mode==143)layer.geoHeightStrength=0;}
        if(mode>=146&&mode<=161){auto &g=binding.surface.globalMaps[0];g.enabled=mode!=161;mapSlot(g.map,7,TerrainSurfaceChannel::RGB);
          if(mode==147)g.blend=TerrainColorBlend::Multiply2x;if(mode==148||mode==149)g.blend=TerrainColorBlend::Overlay;if(mode==160)g.blend=TerrainColorBlend::LighterColor;
          if(mode==149){layer.maps[0].texture={};layer.maps[0].colorFactor={.6f,.2f,.8f,1};}
          if(mode==150)layer.globalMapParticipation[0]=0;if(mode==151)g.intensity=0;
          if(mode>=152&&mode<=154){g.fadeStart=10;g.fadeEnd=20;g.nearStrength=.25f;g.farStrength=.75f;}
          if(mode==155){g.map.colorRemapMin=g.map.colorRemapMax={.1f,.2f,.3f,.6f};g.map.colorFactor={.5f,1,2,1};g.map.factor=.5f;g.map.invert=true;}
          if(mode>=156&&mode<=159){mapSlot(g.map,8,TerrainSurfaceChannel::RGB);binding.terrainDomain={-2,-4,8,16};if(mode==157){g.scaleX=2;g.scaleY=.5f;g.offsetX=.125f;g.offsetY=.25f;}if(mode==158)binding.localMetersPerUv=2;if(mode==159)binding.terrainDomain={-2,-4,16,8};}
        }
        if(mode>=162&&mode<=169){auto &g=binding.surface.globalMaps[1];g.enabled=true;mapSlot(g.map,6,mode==163?TerrainSurfaceChannel::NormalRG:TerrainSurfaceChannel::NormalRGB);
          if(mode==164)layer.globalMapParticipation[1]=0;if(mode==165)g.intensity=0;if(mode==166||mode==167)layer.globalMapParticipation[1]=2;if(mode==167)g.normalBlend=TerrainNormalBlend::Crossfade;
          if(mode==168){g.fadeStart=10;g.fadeEnd=20;g.nearStrength=.25f;g.farStrength=.75f;}if(mode==169)g.map.invert=true;
        }
        if(mode>=170&&mode<=175||mode==181){auto &g=binding.surface.globalMaps[2];g.enabled=true;mapSlot(g.smoothness,7,TerrainSurfaceChannel::R);mapSlot(g.ao,7,TerrainSurfaceChannel::G);mapSlot(g.metallic,7,TerrainSurfaceChannel::B);mapSlot(g.mask,7,TerrainSurfaceChannel::A);
          if(mode==171)layer.globalMapParticipation[2]=0;if(mode==172)layer.globalMapParticipation[2]=2;if(mode==173){g.mask.remapMin=.2f;g.mask.remapMax=.8f;g.mask.invert=true;g.mask.factor=.5f;}
          if(mode==174){g.smoothness.channel=TerrainSurfaceChannel::B;g.smoothness.invert=true;g.ao.channel=TerrainSurfaceChannel::R;g.metallic.channel=TerrainSurfaceChannel::G;}if(mode==175)g.intensity=0;
        }
        if(mode>=176&&mode<=180){auto &g=binding.surface.globalMaps[3];g.enabled=true;mapSlot(g.map,7,TerrainSurfaceChannel::RGB);if(mode==177)layer.globalMapParticipation[3]=2;if(mode==178)layer.globalMapParticipation[3]=0;
          if(mode==179){g.fadeStart=10;g.fadeEnd=20;g.nearStrength=.25f;g.farStrength=.75f;}if(mode==180){g.map.factor=.5f;g.map.invert=true;}
        }
        if(mode==181){setupGeo();geo.curveEnabled=true;binding.surface.globalMaps[0].enabled=true;mapSlot(binding.surface.globalMaps[0].map,7,TerrainSurfaceChannel::RGB);binding.surface.globalMaps[1].enabled=true;mapSlot(binding.surface.globalMaps[1].map,6,TerrainSurfaceChannel::NormalRGB);binding.surface.globalMaps[3].enabled=true;mapSlot(binding.surface.globalMaps[3].map,7,TerrainSurfaceChannel::RGB);}
        if(mode==189){mapSlot(geo.albedo,10,TerrainSurfaceChannel::RGB);geo.scaleMeters=1;geo.blend=TerrainColorBlend::LighterColor;layer.maps[0].texture={};layer.maps[0].colorFactor={0,0,0,1};}
        if(mode==190||mode==191){geo.mode=TerrainGeoMode::Off;auto &g=binding.surface.globalMaps[0];g.enabled=true;mapSlot(g.map,8,TerrainSurfaceChannel::RGB);binding.terrainDomain={-2,-4,8,16};g.scaleX=mode==190?-1.f:0.f;g.scaleY=mode==190?1.f:0.f;}
        if(mode==192){geo.mode=TerrainGeoMode::Off;auto &g=binding.surface.globalMaps[1];g.enabled=true;mapSlot(g.map,6,TerrainSurfaceChannel::NormalRGB);g.scaleX=g.scaleY=0;}
        if(mode>=193&&mode<=198){
          mapSlot(layer.maps[0],3,TerrainSurfaceChannel::RGB);mapSlot(geo.albedo,8,TerrainSurfaceChannel::RGB);geo.blendStrength=.5f;geo.scaleMeters=2;geo.offsetMeters=0;geo.curveEnabled=true;geo.curveScaleMeters=2;
          if(mode==193)geo.curve.keys.clear();
          if(mode==194||mode==197){geo.curve.keys.resize(1);geo.curve.keys[0].time=mode==194?-2.f:2.f;geo.curve.keys[0].value=mode==194?.25f:.125f;}
          if(mode==195){geo.curve.keys[0].time=-2;geo.curve.keys[0].value=-.5f;geo.curve.keys[1].time=3;geo.curve.keys[1].value=.75f;for(auto &key:geo.curve.keys)key.incoming=key.outgoing=AnimationTangentMode::Linear;}
          if(mode==196){geo.curve.keys[0].time=-1;geo.curve.keys[0].value=-.25f;geo.curve.keys[1].time=2;geo.curve.keys[1].value=.5f;for(auto &key:geo.curve.keys){key.incoming=key.outgoing=AnimationTangentMode::Free;key.inSlope=key.outSlope=0;}geo.curve.keys[0].weightedOut=true;geo.curve.keys[0].outWeight=.21f;geo.curve.keys[1].weightedIn=true;geo.curve.keys[1].inWeight=.43f;}
          if(mode==198){geo.curve.keys.clear();for(uint32_t i=0;i<4;++i){AnimationCurveKey key;key.id=i+1;key.time=float(i)-1;key.value=i==0?-.25f:i==1?0:i==2?.25f:1;key.incoming=key.outgoing=AnimationTangentMode::Auto;geo.curve.keys.push_back(key);}}
        }
        if(mode==188){geo.rangeEnabled=true;geo.range={0,0,1,1};}
      }
      if(mode>=199&&mode<238){
        binding.surface.geo.mode=TerrainGeoMode::Off;
        mapSlot(layer.maps[0],3,TerrainSurfaceChannel::RGB);mapSlot(layer.maps[1],5,TerrainSurfaceChannel::NormalRGB);
        layer.maps[4].factor=0;layer.maps[5].factor=0;layer.maps[6].factor=0;mapSlot(layer.maps[2],3,TerrainSurfaceChannel::R);layer.maps[2].factor=0;
        auto &s=binding.surface.snow;s.mode=mode==199?TerrainSnowMode::Off:TerrainSnowMode::Simple;s.amount=.5f;s.parameters={0,0,1,0};s.heightAngleRange={-100,-50,0,1};
        mapSlot(s.diffuse,0,TerrainSurfaceChannel::RGB);mapSlot(s.height,3,TerrainSurfaceChannel::A);s.height.factor=0;mapSlot(s.normal,7,TerrainSurfaceChannel::NormalGA);s.normal.factor=0;
        mapSlot(s.smoothness,3,TerrainSurfaceChannel::R);s.smoothness.factor=0;mapSlot(s.ao,3,TerrainSurfaceChannel::R);
        if(mode==200)s.amount=.25f;
        if(mode==202||mode==203||mode==204||mode==205)s.mode=TerrainSnowMode::Rich;
        if(mode==203){mapSlot(layer.maps[2],3,TerrainSurfaceChannel::R);s.parameters[0]=1;}
        if(mode==204||mode==205){s.perLayerEnabled=true;layer.snowParticipation=mode==204?0:.5f;}
        if(mode==206)s.heightAngleRange={0,100,0,1};
        if(mode==207)s.heightAngleRange={-100,100,0,1};
        if(mode==208){s.maskEnabled=true;mapSlot(s.mask,7,TerrainSurfaceChannel::RGB);}
        if(mode==209)s.tint={0,1,0,1};
        if(mode==210)s.diffuse.invert=true;
        if(mode==211)s.diffuse.factor=.5f;
        if(mode==212)s.normal.factor=1;
        if(mode==215){s.sssEnabled=true;s.sssTint={.2f,.4f,.6f,.5f};mapSlot(s.height,7,TerrainSurfaceChannel::A);}
        if(mode==216){s.rimEnabled=true;s.rimPower=0;}
        if(mode==217)s.amount=0;
        if(mode==219){s.worldSpaceUV=true;s.triplanarEnabled=true;s.uvScales={-2,0,.25f,.5f};}
        if(mode==220||mode==221){s.normalNoiseEnabled=true;mapSlot(s.normalNoiseMap,6,TerrainSurfaceChannel::NormalRGB);s.normalNoiseParams[1]=mode==220?0:.5f;}
        if(mode>=222&&mode<=225){s.distanceResampleEnabled=true;s.distanceResample={0,1,0,1};mapSlot(s.diffuse,8,TerrainSurfaceChannel::RGB);s.uvScales={0,0,.25f,.25f};
          if(mode==223)s.distanceResample[1]=0;if(mode==224)s.distanceResample[2]=1e6f,s.distanceResample[3]=2e6f;if(mode==225)s.distanceResample[0]=1;}
        if(mode==226){s.parameters[2]=0;}
        if(mode==227){mapSlot(s.smoothness,7,TerrainSurfaceChannel::R);mapSlot(s.ao,7,TerrainSurfaceChannel::B);}
        if(mode==228){s.stochasticEnabled=true;s.stochasticScale=3;s.stochasticContrast=.8f;}
        if(mode==229){s.sparkleEnabled=true;mapSlot(s.sparkleNoiseMap,7,TerrainSurfaceChannel::RGB);s.sparkleSize=65536;s.sparkleStrength=.5f;s.sparkleDensity=0;}
        if(mode>=230&&mode<=234){s.mode=TerrainSnowMode::Rich;s.amount=.6f;s.parameters[0]=1;mapSlot(layer.maps[2],7,TerrainSurfaceChannel::R);s.maskEnabled=true;mapSlot(s.mask,8,TerrainSurfaceChannel::RGB);}
        if(mode==235){s.amount=.1f;s.heightAngleRange={0,.0001f,0,1};}
        if(mode==236||mode==237){mapSlot(s.normal,6,mode==236?TerrainSurfaceChannel::NormalRGB:TerrainSurfaceChannel::NormalRG);s.normal.factor=1;}
      }
      if(mode>=238){
        for(auto &map:layer.maps){map.texture={};map.factor=1;map.invert=false;map.remapMin=0;map.remapMax=1;}
        layer.maps[0].colorFactor={1,1,1,1};layer.maps[2].factor=mode==245?.35f:0;layer.maps[3].factor=.25f;layer.maps[4].factor=.8f;layer.maps[5].factor=0;layer.maps[6].factor=0;
        layer.snowPorosity=.15f;auto &flow=binding.surface.flow;flow.wetness=mode!=240&&mode!=241&&mode!=245;flow.puddles=!flow.wetness;flow.puddleParams={1,1,0,0};flow.globalPorosity=.6f;
        auto &fx=binding.staticFx;fx.enabled=true;fx.textureIndex=12;fx.width=fx.height=1;fx.domain=binding.terrainDomain;fx.identity={71,2};fx.revision=1;fx.rgbaSha256=std::string(64,'a');
        if(mode==242){flow.heightWetness=true;flow.heightWetnessParams={.75f,1,0,0};}
        if(mode==243){flow.distanceWetness=true;flow.distanceWetnessParams={0,.5f,1000000,.5f};}
        if(mode>=250){fx.textureIndex=11;fx.width=fx.height=65;}
        if(mode==244)flow.perTexPorosity=true;
        if(mode==246)flow.wetnessParams={.5f,.6f,0,0};
        if(mode==247||mode==249){auto &snow=binding.surface.snow;snow.mode=TerrainSnowMode::Simple;snow.amount=.5f;snow.parameters={0,0,1,0};snow.heightAngleRange={-100,-50,0,1};mapSlot(snow.diffuse,0,TerrainSurfaceChannel::RGB);mapSlot(snow.height,3,TerrainSurfaceChannel::A);snow.height.factor=0;mapSlot(snow.normal,7,TerrainSurfaceChannel::NormalGA);snow.normal.factor=0;mapSlot(snow.smoothness,3,TerrainSurfaceChannel::R);snow.smoothness.factor=0;mapSlot(snow.ao,3,TerrainSurfaceChannel::R);if(mode==247)flow.wetnessMaskSnow=true;}
      }
      if(mode>=252){
        binding.staticFx.textureIndex=12;binding.staticFx.width=binding.staticFx.height=1;
        auto &f=binding.surface.flow;f.wetness=f.puddles=f.wetnessMaskSnow=false;f.streams=mode<=263||mode>=278;f.lava=!f.streams;
        f.streamMaximum=1;f.streamBlend=1;f.streamFlowParams={0,.5f,.3f,.2f};f.streamTint={.5f,.5f,.5f,1};f.globalPorosity=.6f;
        mapSlot(f.streamNormal,7,TerrainSurfaceChannel::NormalRG);mapSlot(f.lavaDiffuse,7,TerrainSurfaceChannel::RGB);
        f.lavaParams={4,1,.5f,.275f};f.lavaParams2={.05f,.017f,.15f,0};f.lavaColorLow={1,0,0,1};f.lavaColorHighlight={0,1,0,1};f.lavaEdgeColor={.1f,.2f,.3f,1};f.lavaEmissiveMultiplier=2;
        if(mode==254)f.streamTint={.25f,.5f,1,1};
        if(mode==256||mode==257){f.perTexFoam=true;layer.flow.foam=mode==256?0:2;}
        if(mode==258){f.heightDampStreams=true;f.streamFades={1,2,3,4};}
        if(mode==259)f.streamMaximum=0;
        if(mode==260)layer.maps[2].factor=.5f;
        if(mode==261||mode==262||mode==263||mode==278||mode==279){mapSlot(f.streamNormal,8,TerrainSurfaceChannel::NormalRG);mapSlot(layer.maps[4],8,TerrainSurfaceChannel::R);layer.maps[4].factor=1;f.streamFlowParams={mode==261?0.f:2.f,.5f,.8f,mode==263?1.f:0.f};f.streamUvScales={3,3,70,70};}
        if(mode==278||mode==279)mapSlot(f.streamNormal,4,TerrainSurfaceChannel::NormalRG); // Real nonlinear R/G strip; roughness remains gradient8.
        if(mode==265)f.lavaParams2[3]=.5f;
        if(mode==266)f.lavaParams[0]=1;
        if(mode==268)f.lavaParams[1]=0;
        if(mode==269)layer.maps[2].factor=.9f;
        if(mode==270){f.heightDampLava=true;f.lavaFades={1,2,3,4};}
        if(mode==271||mode==272||mode==273||mode==274){mapSlot(f.lavaDiffuse,8,TerrainSurfaceChannel::RGB);f.lavaUvScale={3,3,0,0};if(mode==273||mode==274){f.lavaStochastic=true;f.lavaStochasticContrast=mode==273?.5f:2;f.lavaStochasticSize=.8f;}}
        if(mode==275){f.lava=true;f.puddles=true;f.puddleParams={1,1,0,0};f.lavaParams[1]=0;f.lava=false;f.streams=false;f.puddles=true;}
        if(mode==276){f.puddles=true;f.puddleParams={1,1,0,0};f.lavaParams[1]=0;}
        if(mode==277){f.lavaEmissiveMultiplier=0;}
        if(mode==280||mode==281){f.streamMaximum=mode==280?.25f:.75f;}
        if(mode==282||mode==283){
          f.streams=f.lava=true;f.lavaParams[1]=mode==282?1:0;
          auto &snow=binding.surface.snow;snow.mode=TerrainSnowMode::Simple;snow.amount=.5f;snow.parameters={0,0,1,0};snow.heightAngleRange={-100,-50,0,1};
          mapSlot(snow.diffuse,0,TerrainSurfaceChannel::RGB);mapSlot(snow.height,3,TerrainSurfaceChannel::A);snow.height.factor=0;mapSlot(snow.normal,7,TerrainSurfaceChannel::NormalGA);snow.normal.factor=0;mapSlot(snow.smoothness,3,TerrainSurfaceChannel::R);snow.smoothness.factor=0;mapSlot(snow.ao,3,TerrainSurfaceChannel::R);
        }
      }
      if(mode>=284&&mode<=301){
        auto &f=binding.surface.flow;f.wetness=f.streams=f.lava=f.wetnessMaskSnow=false;f.puddles=true;f.puddleParams={2.5f,1,0,0};
        f.rainDrops=mode!=284;f.rainIntensityScale={mode==285?0.f:mode==287?.5f:1.f,3,0,0};mapSlot(f.rainDropTexture,0,TerrainSurfaceChannel::RGB);
        if(mode==289)f.puddleParams[1]=0;
        if(mode==290)f.rainDropTexture.factor=0;
        if(mode==291){f.rainDropTexture.colorRemapMin[1]=f.rainDropTexture.colorRemapMax[1]=.5f;f.rainDropTexture.colorRemapMin[2]=f.rainDropTexture.colorRemapMax[2]=.5f;}
        if(mode==292||mode==293){mapSlot(f.rainDropTexture,8,TerrainSurfaceChannel::RGB);f.rainIntensityScale[1]=mode==292?3:9;}
        if(mode==294)f.rainDropTexture.invert=true;
        if(mode>=296){
          f.specularFade=true;f.specularFades={1,2,0,0};layer.maps[5].factor=.6f;
          if(mode!=301){f.rainDrops=f.puddles=false;binding.staticFx={};f.streamNormal.texture=f.lavaDiffuse.texture=f.rainDropTexture.texture={};}
          if(mode==299||mode==300)f.specularFades={0,0,0,0};
        }
      }
      if(mode==310||mode==311){auto &f=binding.surface.flow;f.streams=true;f.lava=f.rainDrops=f.specularFade=f.wetness=f.puddles=false;f.heightDampStreams=true;f.streamFades={1,2,3,4};f.streamMaximum=f.streamBlend=1;f.streamFlowParams={2,.5f,.8f,0};mapSlot(f.streamNormal,0,TerrainSurfaceChannel::NormalRG);}
      binding.surface.layers.push_back(layer);
      if (mode == 13 || mode == 15) {
        auto second = layer;
        second.identity = {11, 2};
        mapSlot(second.maps[0], 1, TerrainSurfaceChannel::RGB);
        if (mode == 13) {
          binding.surface.layers[0].heightBlend = true;
          binding.surface.layers[0].heightWidth = .2f;
          binding.surface.layers[0].maps[2].channel = TerrainSurfaceChannel::R;
          second.heightBlend = true;
          second.heightWidth = .2f;
        }
        if (mode == 15)
          binding.surface.layers[0].active = false;
        binding.surface.layers.push_back(second);
      }
      for (auto &authored : binding.surface.layers)
        for (size_t role = 0; role < 7; ++role) {
          auto &map = authored.maps[role];
          if (map.texture.valid())
            binding.textures.push_back({map.texture, role == 1 || map.linear,
                                        role == 1,
                                        uint32_t(map.texture.low - 1)});
        }
      if(mode>=30){
        for(const auto &ref:terrainSurfaceTextureRefs(binding.surface))
          binding.textures.push_back({ref.identity,ref.linear,ref.normal,ref.clampSampling?(ref.identity.low==9?11u:12u):uint32_t(ref.identity.low-1),ref.normalChannel,ref.residencyCap,ref.clampSampling});
      }
      std::array<ae::u32, 13> slots{0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,mode>=250?11u:mode>=233?13u:11u,12};
      std::vector<float> published;
      ae::u32 offset = 0;
      std::string error;
      std::string cold;
      require(serializeTerrainSurface(binding.surface, cold, error),
              error.c_str());
      TerrainSurfaceAsset restored;
      require(deserializeTerrainSurface(cold, restored, error), error.c_str());
      binding.surface = std::move(restored);
      require(
          appendTerrainSurfaceTable(binding, slots, published, offset, error),
          error.c_str());
      if(mode==48||mode==110||mode==181||mode==218||mode==248||mode==281||mode==295){
        require(relocateTerrainSurfaceTable(published,3,error),error.c_str());
        published.insert(published.begin(),12,0.f);offset+=3;
      }
      if(mode>=296&&mode<=300){auto root=ae::u32(published[5])-1,dir=ae::u32(published[root*4+12])-1,snapshot=ae::u32(published[dir*4+2]);require(published[snapshot*4]==-1&&published[snapshot*4+3]==0,"spec-only placeholder has no valid texture slot or enabled flag");}
      require(published.size() * sizeof(float) <= 16384,
              "surface table fixture fits storage");
      std::memcpy(surfaceRows.mapped, published.data(),
                  published.size() * sizeof(float));
      row[13 * 4] = 0;
      row[14 * 4 + 2] = float(offset);
    }
    auto cmd = f.command();
    VkClearValue clear{};
    VkRenderPassBeginInfo bi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    bi.renderPass = renderPass;
    bi.framebuffer = framebuffer;
    bi.renderArea = {{0, 0}, {width, height}};
    bi.clearValueCount = 1;
    bi.pClearValues = &clear;
    if(timestamps){vkCmdResetQueryPool(cmd,timestamps,0,2);vkCmdWriteTimestamp(cmd,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,timestamps,0);}
    vkCmdBeginRenderPass(cmd, &bi, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipelines[mode >= 8 ? 2 : mode == 5]);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout, 0, 2, sets.data(), 0, nullptr);
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4,
                       &mode);
    vkCmdDraw(cmd, 6, 1, 0, 0);
    vkCmdEndRenderPass(cmd);
    if(timestamps)vkCmdWriteTimestamp(cmd,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,timestamps,1);
    f.transition(cmd, target, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                 VK_ACCESS_TRANSFER_READ_BIT,
                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                 VK_PIPELINE_STAGE_TRANSFER_BIT);
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {width, height, 1};
    vkCmdCopyImageToBuffer(cmd, target.handle,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           readback.handle, 1, &copy);
    VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex =
        VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = readback.handle;
    barrier.size = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &barrier,
                         0, nullptr);
    f.submit(cmd);
    if(timestamps){
      uint64_t measured[2]{};
      check(vkGetQueryPoolResults(f.device,timestamps,0,2,sizeof(measured),measured,sizeof(uint64_t),VK_QUERY_RESULT_64_BIT|VK_QUERY_RESULT_WAIT_BIT),"read GPU timestamps");
      const uint64_t mask=f.timestampBits>=64?~uint64_t(0):(uint64_t(1)<<f.timestampBits)-1;
      const double us=double((measured[1]-measured[0])&mask)*f.timestampPeriod/1000.0;
      std::printf("GPU mode %u: %.3f us (32x32 offscreen; host only)\n",mode,us);
    }
    std::string imagePath =
        directory + "/terrain-control-mode-" + std::to_string(mode) + ".ppm";
    std::ofstream evidence(imagePath, std::ios::binary);
    evidence << "P6\n" << width << ' ' << height << "\n255\n";
    const auto *rgba = static_cast<const uint8_t *>(readback.mapped);
    for (size_t p = 0; p < width * height; ++p)
      evidence.write(reinterpret_cast<const char *>(rgba + p * 4), 3);
    require(bool(evidence), "write real GPU evidence PPM");
    evidence.flush();
    modePixels[mode].assign(rgba,rgba+width*height*4);
    if(mode==310||mode==311){expectPixel(16,mode==310?std::array<uint8_t,4>{128,128,0,255}:std::array<uint8_t,4>{155,100,0,255},"Refraction height fade uses same world metre domain as coverage, while depth uses material height");continue;}
    if(mode>=302&&mode<=309){
      const std::array<std::array<uint8_t,4>,8> expected{{
        {145,128,0,255},{145,128,0,255},{139,128,0,255},{146,181,0,255},{128,128,0,255},{150,128,0,255},{139,128,0,255},{143,142,0,255}}};
      expectPixel(16,expected[mode-302],"WORLD source direction is pulled back once into LOCAL Flow phases, with finite vertical fallback");continue;
    }
    if(mode>=284){
      const auto byte=[](double x){return uint8_t(std::lround(std::clamp(x,0.0,1.0)*255));};
      const auto rainNormal=[&](double intensity,double time){
        double sum=0;const double frequencies[]{1,.85,.93,1.13},offsets[]{0,.2,.45,.7};
        for(int i=0;i<4;++i){double phase=(time*frequencies[i]+offsets[i])*1.6;phase-=std::floor(phase);double life=1+phase;life-=std::floor(life);double weight=intensity-i*.25;
          double amount=std::clamp(.2+weight*.8-life,0.0,1.0)*std::sin(std::clamp(life*9.0,0.0,3.0)*3.14159265359)*std::clamp(weight*4.0,0.0,1.0);sum-=amount;}
        double xy=sum/std::sqrt(1+2*sum*sum)*intensity*intensity,length=std::sqrt(1+2*xy*xy);
        return std::array<uint8_t,4>{byte(xy/length*.5+.5),byte(1/length*.5+.5),byte(xy/length*.5+.5),255};
      };
      if(mode==284||mode==285||mode==289||mode==290||mode==291||mode==294)expectPixel(16,{128,255,128,255},"Disabled rain, zero intensity/puddle/shape or neutral GB leaves actual normal unchanged");
      else if(mode==286||mode==287||mode==288||mode==295)expectPixel(16,rainNormal(mode==287?.5:1,mode==288?.125:0),"RGBA lifetime four phase source ripple math reaches actual world normal including relocation");
      else if(mode==292){} // Spatial UV baseline, actual gradient shape and direction.
      else if(mode==293)require(modePixels[293]!=modePixels[292],"Rain frequency changes actual packed texture samples");
      else if(mode==296||mode==299)expectPixel(16,{255,0,64,255},"Spec-only below/coincident height works without any FX texture");
      else if(mode==297||mode==300)expectPixel(16,{230,77,64,255},"Specular fade scales metallic and smoothness with linear or short source interval");
      else if(mode==298)expectPixel(16,{204,153,64,255},"Spec-only above range preserves roughness metallic and AO");
      else if(mode==301)expectPixel(16,{138,77,255,255},"Height specular fade consumes final wet material while preserving AO");
      continue;
    }
    if(mode>=252){
      const auto byte=[](double v){return uint8_t(std::lround(std::clamp(v,0.0,1.0)*255));};
      if(mode==282)expectPixel(16,{102,153,0,255},"Combined Stream Lava records clear Snow before mask with independent maximum");
      else if(mode==283)expectPixel(16,{218,218,218,255},"Stream clearing survives zero Lava and prevents Snow layer");
      else if(mode==252)expectPixel(16,{218,218,218,255},"Stream B amount reaches source nested water BRDF");
      else if(mode==253)expectPixel(16,{138,0,179,255},"Stream coverage roughness AO independent from RG normal");
      else if(mode==254)expectPixel(16,{153,218,255,255},"Stream tint is a real source multiplier");
      else if(mode==255)expectPixel(16,{221,221,221,255},"Raw B foam responds to slope and global foam strength");
      else if(mode==256)expectPixel(16,{218,218,218,255},"Per-layer foam zero preserves wet BRDF");
      else if(mode==257)expectPixel(16,{223,223,223,255},"Per-layer foam2 survives serialization table and shading");
      else if(mode==258||mode==259||mode==268||mode==269||mode==270)expectPixel(16,{255,255,255,255},"Height window or amount zero disables visual coverage");
      else if(mode==260)expectPixel(16,{254,254,254,255},"Stream subtracts actual material height before contrast");
      else if(mode==261){} // Amplitude baseline.
      else if(mode==278)expectPixel(16,{163,163,163,255},"Nonlinear normal source phase0 gives derived refraction offset .432",2);
      else if(mode==262||mode==263){require(modePixels[mode]!=modePixels[mode==263?262:261],"Source refraction amplitude or depth damping changes real material samples");}
      else if(mode==279){require(modePixels[279]!=modePixels[278],"Source time changes nonlinear normal samples and actual refraction");expectPixel(16,{127,127,127,255},"Nonlinear normal source phase.125 gives derived refraction offset .216",2);}
      else if(mode==264)expectPixel(16,{102,153,0,255},"Lava raw B heat interpolates authored low and high");
      else if(mode==265)expectPixel(16,{61,92,0,255},"Lava raw A crust darkens actual heat color");
      else if(mode==266)expectPixel(16,{71,92,112,255},"Partial lava blend retains base and authored edge color");
      else if(mode==267)expectPixel(16,{204,255,0,255},"Lava emission multiplier reaches actual HDR output");
      else if(mode==271){} // Static lava normal baseline.
      else if(mode==272||mode==273||mode==274){require(modePixels[mode]!=modePixels[mode==272?271:mode==273?272:273],"Lava time and real three-sample stochastic size/contrast alter sampled normal");}
      else if(mode==275||mode==276)expectPixel(16,{239,239,239,255},"Independent puddles survive enabled Lava with zero amount");
      else if(mode==277)expectPixel(16,{0,0,0,255},"Zero lava emission multiplier is consumed");
      else if(mode==280||mode==281){double amount=.6*(mode==280?.25:.75);auto c=byte(1-.4*amount*amount);expectPixel(16,{c,c,c,255},"Stream maximum applied once to surface coverage including relocation");}
      continue;
    }
    if(mode>=238){
      const auto byte=[](double value){return uint8_t(std::lround(std::clamp(value,0.0,1.0)*255));};
      double amount=mode==240||mode==241?.4:mode==242?.75:mode==243?.1:mode==245?.05:mode==246?.5:mode==250?0:mode==251?1:.2;
      double dark=mode==244?.2:.6,color=1-(1-dark)*amount*amount;
      if(mode==239||mode==241)expectPixel(16,{byte(.8+(.08-.8)*amount*amount),0,byte(.25+.75*amount),255},"StaticFX real R/G bytes alter roughness and AO independently");
      else if(mode==249){double snow=.216;expectPixel(16,{byte(color*(1-snow)+snow),byte(color*(1-snow)),byte(color*(1-snow)),255},"StaticFX clearing precedes real Snow cubic coverage");}
      else expectPixel(16,{byte(color),byte(color),byte(color),255},"StaticFX snapshot, height/distance, per-layer porosity, minmax, maskSnow and relocation");
      continue;
    }
    if(mode>=199){
      if(mode==199||mode==203||mode==204||mode==206||mode==217)expectPixel(16,{255,255,255,255},"Snow off/clearing/participation/height or zero level preserves base");
      else if(mode==200||mode==205||mode==207)expectPixel(16,{255,223,223,255},"Snow source cubic coverage is one eighth");
      else if(mode==208)expectPixel(16,{255,124,124,255},"Snow mask minimum G wins and yields coverage .512");
      else if(mode==209)expectPixel(16,{0,0,0,255},"Snow tint applies to actual diffuse");
      else if(mode==210)expectPixel(16,{0,255,255,255},"Snow diffuse inversion selects complementary channels");
      else if(mode==211)expectPixel(16,{128,0,0,255},"Snow diffuse factor scales material");
      else if(mode==212)expectPixel(16,{204,226,102,255},"Snow packed GA produces actual world normal");
      else if(mode==213)expectPixel(16,{128,255,128,255},"Snow normal factor zero is exactly flat");
      else if(mode==214)expectPixel(16,{89,3,255,255},"Snow sequential smoothness lerps and scalar AO roles");
      else if(mode==215)expectPixel(16,{23,46,69,255},"Snow thickness tint produces explicit thin transmission");
      else if(mode==216)expectPixel(16,{128,128,255,255},"Snow rim reaches actual emission with power zero");
      else if(mode==220)expectPixel(16,{128,255,128,255},"enabled Snow normal noise strength zero is flat");
      else if(mode==221)expectPixel(16,{160,251,128,255},"Snow normal noise strength produces reoriented world normal");
      else if(mode==222)expectPixel(16,{128,128,0,255},"far Snow resampling UV multiplier zero samples repeat boundary");
      else if(mode>=223&&mode<=225)expectPixel(16,{62,62,0,255},"Snow resampling strength/start/end/identity selects near UV consistently");
      else if(mode==226)expectPixel(16,{255,3,255,255},"Snow age zero removes both smoothness contributions");
      else if(mode==227)expectPixel(16,{140,3,153,255},"Snow smoothness R and AO B remain independent scalar roles");
      else if(mode==230||mode==232||mode==233)expectPixel(16,{255,255,255,255},"Snow mask Clamp UV zero, outside or actual mip1 never reads opposite border");
      // Rich H=.2, level=.6: coverage=(.6-.2)^3*(1-.2/2)*8=.4608.
      // Remaining white G/B=.5392 -> 137.496 in UNORM8, observed137 on GPU.
      else if(mode==231||mode==234)expectPixel(16,{255,137,137,255},"Snow mask Clamp UV one retains authored maximum at far edge, including actual mip1");
      // Coverage1e-6 is below one UNORM step. The fixture amplifies its
      // observable white-to-red delta by100000; float32 subtraction gives26.
      else if(mode==235)expectPixel(16,{26,26,26,255},"Snow short height denominator floor agrees with CPU source coverage");
      else if(mode==236||mode==237){const auto byte=[](double v){return uint8_t(std::lround(std::clamp(v,0.0,1.0)*255));};double x=191.0/127.5-1,y=128.0/127.5-1,z=mode==237?std::sqrt(1-x*x-y*y):238.0/127.5-1;double length=std::sqrt(x*x+y*y+z*z);expectPixel(16,{byte(x/length*.5+.5),byte(z/length*.5+.5),byte(y/length*.5+.5),255},"Snow explicitly authored RGB/RG normal interpretation reaches actual world normal");}
      else expectPixel(16,{255,0,0,255},"Snow source material survives reload, mode, projection or relocation");
      continue;
    }
    if(mode>=116){
      const auto byte=[](double value){return uint8_t(std::lround(std::clamp(value,0.0,1.0)*255));};
      const auto gradientChannel=[](double coordinate){double pixel=coordinate*65-.5;int first=int(std::floor(pixel));double t=pixel-first;auto texel=[](int i){i=(i%65+65)%65;return double(i*255/64)/255.0;};return texel(first)*(1-t)+texel(first+1)*t;};
      const auto geoColor=[&](double amount){return std::array<uint8_t,4>{byte(26.0/255*(1-amount+amount*.4)),byte(51.0/255*(1-amount+amount*.8)),byte(77.0/255*(1-amount+amount*1.2)),255};};
      const auto tintColor=[&](double amount){return std::array<uint8_t,4>{byte(26.0/255*(1-amount)+.2*amount),byte(51.0/255*(1-amount)+.4*amount),byte(77.0/255*(1-amount)+.6*amount),255};};
      if(mode==116||mode==118||mode==119||mode==124||mode==125||mode==142||mode==150||mode==151||mode==161)expectPixel(16,{26,51,77,255},"disabled module, range, filter or zero participation is exactly base");
      else if(mode==117||mode==126||mode==143||mode==188)expectPixel(16,geoColor(.8),"Geo Multiply2x, signed filter, zero filter strength and coincident range");
      else if(mode==120||mode==122||mode==123||mode==127||mode==140)expectPixel(16,geoColor(.4),"Geo linear range midpoint, participation and source max filter floor");
      else if(mode==121||mode==146||mode==160)expectPixel(16,tintColor(.8),"Geo LighterColor and global Tint full source contribution");
      else if(mode==141)expectPixel(16,geoColor(.32),"Geo height contrast changes real sampled height filter");
      else if(mode>=128&&mode<=132){
        if(mode==130||mode==131)expectPixel(16,{128,255,128,255},"zero Geo normal strength or map factor preserves flat surface");
        else{double x=191.0/127.5-1,y=128.0/127.5-1,z=mode==129?std::sqrt(1-x*x-y*y):238.0/127.5-1;if(mode==132){x=-x;y=-y;z=-z;}x*=.8;y*=.8;z=.2+.8*z;double length=std::sqrt(x*x+y*y+z*z);expectPixel(16,{byte(x/length*.5+.5),byte(z/length*.5+.5),byte(-y/length*.5+.5),255},"Geo RGB/RG strip normal affects stable geometric frame");}
      }
      else if(mode>=133&&mode<=139||mode==144||mode==145||mode>=182&&mode<=187){
        double v=.25,amount=.5;if(mode==134)v=.5;if(mode==136)v=.125;if(mode==139)amount=.125;if(mode==144)v=.75;if(mode==145)v=-.25;
        if(mode>=182&&mode<=186){double input=(1.03125+(mode==184?.5:0))/(mode==183?4:2);if(mode==185)input=1.546875/2;v=.25*(mode==186?input*input*(3-2*input):input);}
        if(mode==187)v=0;
        auto expected=mode==145?std::array<uint8_t,4>{byte(gradientChannel(0)),byte(gradientChannel(v)),0,255}:std::array<uint8_t,4>{byte(1-amount+amount*2*gradientChannel(0)),byte(1-amount+amount*2*gradientChannel(v)),byte(1-amount),255};
        expectPixel(16,expected,"Geo metres/offset/world height/curve LUT rotation or opacity follows original texture coordinates");
      }
      else if(mode==147||mode==148)expectPixel(16,geoColor(.8),"global Multiply2x and dark-base Overlay source arithmetic");
      else if(mode==149)expectPixel(16,{104,43,212,255},"Overlay chooses high-base branch independently by channel");
      else if(mode>=152&&mode<=154)expectPixel(16,tintColor(mode==152?.4:mode==153?.6:.2),"global distance midpoint and near/far strengths are independent linear fades");
      else if(mode==155)expectPixel(16,{32,61,97,255},"global color remap inversion factor and alpha each affect output");
      else if(mode>=156&&mode<=159){double u=(.515625*(mode==158?2:1)+2)/(mode==159?16:8),v=(.515625*(mode==158?2:1)+4)/(mode==159?8:16);if(mode==157){u=(u+.125)*2;v=(v+.25)*.5;}expectPixel(16,{byte(gradientChannel(u)),byte(gradientChannel(v)),0,255},"global UV derives actual nondefault terrain domain then offset-before-scale");}
      else if(mode>=162&&mode<=169){
        if(mode==164||mode==165)expectPixel(16,{128,255,128,255},"normal zero intensity/participation is base");
        else{double x=191.0/127.5-1,y=128.0/127.5-1,z=mode==163?std::sqrt(1-x*x-y*y):238.0/127.5-1;if(mode==169){x=-x;y=-y;z=-z;}if(mode==167){double len=std::sqrt(x*x+y*y+z*z);x=2*x/len;y=2*y/len;z=2*z/len-1;}else{double strength=mode==166?2:mode==168?.5:1;x*=strength;y*=strength;z=1+(z-1)*std::min(strength,1.0);}double len=std::sqrt(x*x+y*y+z*z);expectPixel(16,{byte(x/len*.5+.5),byte(z/len*.5+.5),byte(y/len*.5+.5),255},"global RGB/RG, over-one RNM/Crossfade, linear fade and inversion");}
      }
      else if(mode>=170&&mode<=175||mode==181){double amount=mode==171||mode==175?0:mode==172?1.6:mode==173?.16:.8;double rough=mode==174?.6:.8,metal=mode==174?.4:.6,ao=mode==174?.2:.4;expectPixel(16,{byte(.37*(1-amount)+rough*amount),byte(.2*(1-amount)+metal*amount),byte(.6*(1-amount)+ao*amount),255},"SAOM channel layout, smoothness inversion, mask remap and over-one participation");}
      else if(mode>=176&&mode<=180){double amount=mode==177?2:mode==178?0:mode==179?.5:1;double r=mode==180?.4:.2,g=mode==180?.3:.4,b=mode==180?.2:.6;expectPixel(16,{byte(13.0/255*(1-amount)+r*amount),byte(25.5/255*(1-amount)+g*amount),byte(38.5/255*(1-amount)+b*amount),204},"global emission fade, factor, inversion and over-one participation preserve radiance");}
      else if(mode==190||mode==191){double u=mode==190?-(.515625+2)/8:0,v=mode==190?(.515625+4)/16:0;expectPixel(16,{byte(gradientChannel(u)),byte(gradientChannel(v)),0,255},"signed mirrored and zero UV scale are authored global transforms");}
      else if(mode==192)expectPixel(16,{191,238,127,255},"zero global normal UV uses a finite stable geometric frame");
      else if(mode>=193&&mode<=198){
        double input=1.03125/2,v=0;
        if(mode==194)v=.25;if(mode==195)v=.25*input;if(mode==197)v=.125;
        if(mode==196){double low=0,high=1;for(int step=0;step<60;++step){double t=(low+high)/2,q=1-t,x=-q*q*q+3*(-.37)*q*q*t+3*.71*q*t*t+2*t*t*t;if(x<input)low=t;else high=t;}double t=(low+high)/2,q=1-t;v=-.25*q*q*q-3*.25*q*q*t+3*.5*q*t*t+.5*t*t*t;}
        if(mode==198){double t=input;v=.25*(-2*t*t*t+3*t*t)+.25*(t*t*t-2*t*t+t)+.5*(t*t*t-t*t);}
        expectPixel(16,{byte(.5+gradientChannel(0)),byte(.5+gradientChannel(v)),128,255},"signed/outside weighted keys, Auto tangents and empty/single Geo curves sample original authoring domain");
      }
      else if(mode==189){
        auto rgba=modePixels[mode].data();
        for(uint32_t y=0;y<height;++y){double point=(double(y)+.5)/height*256-.5;int a=int(std::floor(point)),b=a+1;double blend=point-a;a=(a%256+256)%256;b=(b%256+256)%256;
          for(int channel=0;channel<3;++channel){int expected=int(std::lround(originalGeo[a*4+channel]*(1-blend)+originalGeo[b*4+channel]*blend));require(std::abs(int(rgba[(y*width+16)*4+channel])-expected)<=3,"original Geo PNG full-resolution rows survive actual GPU sampling/readback");}
        }
      }
      continue;
    }
    if (mode == 0) {
      expectPixel(16, colors[1], "sub-vertex painted stripe");
      expectPixel(4, colors[0], "left exterior");
      expectPixel(28, colors[0], "right exterior");
    } else if (mode == 1)
      expectPixel(16, colors[2], "exact UV zero with repeat sampler");
    else if (mode == 2)
      expectPixel(16, colors[3], "exact UV one with repeat sampler");
    else if (mode == 6)
      expectPixel(16, colors[1], "asymmetric scale and XY offsets hit stripe");
    else if (mode == 7)
      expectPixel(16, colors[0], "asymmetric Y scale and offset leave stripe");
    else if (mode == 8)
      expectPixel(16, {27, 54, 54, 255}, "PBR albedo tint and saturation");
    else if (mode == 9 || mode == 17)
      expectPixel(16, {128, 255, 128, 255},
                  "neutral normal and zero normal strength");
    else if (mode == 10 || mode == 23)
      expectPixel(16, {94, 51, 153, 255},
                  "packed roughness remap/invert metal and AO");
    else if (mode == 11)
      expectPixel(16, {13, 26, 39, 204},
                  "emission RGB factor and height alpha mapping");
    else if (mode == 12)
      expectPixel(16, {119, 227, 0, 255},
                  "meter tiling rotation offset real gradient texture");
    else if (mode == 13 || mode == 15)
      expectPixel(16, colors[1], "height blend or active-layer normalization");
    else if (mode == 14)
      expectPixel(16, {37, 218, 128, 255},
                  "neutral normal on rotated nonuniform geometry");
    else if (mode == 16)
      expectPixel(16, {191, 238, 128, 255},
                  "normal texture changes world-space lighting normal");
    else if (mode == 18)
      expectPixel(16, {132, 132, 0, 255},
                  "local triplanar uses stored local meters");
    else if (mode == 19)
      expectPixel(16, {6, 140, 0, 255},
                  "world triplanar uses transformed world meters");
    else if (mode == 20)
      expectPixel(16, colors[0],
                  "ordinary PBR layer without advanced features");
    else if (mode == 21)
      expectPixel(16, {13, 19, 26, 13},
                  "RGBA remap inversion and color factors");
    else if (mode == 22)
      expectPixel(16, {191, 238, 128, 255},
                  "two-channel normal reconstructs positive Z");
    else if (mode == 24) {
      auto *p = static_cast<uint8_t *>(readback.mapped) + (16 * width + 16) * 4;
      require(p[0] > 178 && p[0] < 255 && p[1] == 0 && p[2] == 0,
              "macro color variation changes albedo only");
    } else if (mode == 25)
      expectPixel(
          16, {13, 26, 39, 18},
          "authored constant height applies remap inversion and factor");
    else if (mode == 26)
      expectPixel(16, {26, 51, 153, 255},
                  "constant roughness applies remap inversion and factor");
    else if (mode == 27)
      expectPixel(16, {0, 0, 0, 0},
                  "neutral missing height and emission remain zero");
    else if (mode == 28)
      expectPixel(16, {129, 135, 0, 255},
                  "meter offsets divide by asymmetric tile sizes");
    else if (mode == 29)
      expectPixel(16, {128, 255, 128, 255},
                  "zero strength neutralizes even negative-Z normal texture");
    else if(mode==30||mode==31||mode==33||mode==34||mode==36||mode==41||mode==49)
      expectPixel(16,colors[0],"noise disabled, zero strength/participation, fade or neutral equivalence");
    else if(mode==32||mode==35||mode==47||mode==48)
      expectPixel(16,{102,0,0,255},"detail near or distance far modulation");
    else if(mode==37||mode==45||mode==46)
      expectPixel(16,{179,0,0,255},"independent per-layer global participation");
    else if(mode==38)
      expectPixel(16,{41,0,0,255},"local and global detail contribute independently");
    else if(mode==39)
      expectPixel(16,{51,102,153,255},"RGB noise uses complete mapped color factor");
    else if(mode==40)
      expectPixel(16,{189,0,0,255},"noise scalar remap inversion and factor");
    else if(mode==42)
      expectPixel(16,{130,0,0,255},"noise frequency in local metres");
    else if(mode==43)
      expectPixel(16,{12,0,0,255},"noise frequency changes projected sampling");
    else if(mode==44)
      expectPixel(16,{12,0,0,255},"noise world triplanar uses transformed metres");
    else if(mode>=50&&mode<=55 || mode==61 || mode==65 || mode==70)
      expectPixel(16,{191,238,128,255},"each normal noise index, RGB/RG and anti identity");
    else if(mode>=56&&mode<=60 || mode==62 || mode==66 || mode==67)
      expectPixel(16,{128,255,128,255},"normal noise flat, zero, disabled, fade or participation identity");
    else if(mode==63||mode==64){
      auto *p=static_cast<uint8_t *>(readback.mapped)+(16*width+16)*4;
      require(p[0]>250 && p[1]>120 && p[1]<140,"three independent normal contributions blend with RNM");
    }
    else if(mode==68||mode==69)
      expectPixel(16,{102,0,0,255},"anti-tile uses independent detail and distance maps");
    else if(mode==72||mode==73)
      expectPixel(16,{94,251,128,255},"normal noise respects sloped and face-normal geometric frame");
    else if(mode==71)
      expectPixel(16,{238,192,128,255},"global and local normals compose in one geometric frame");
    else if(mode==74||mode==76||mode==84)
      expectPixel(16,{6,6,0,255},"constant or distant resampling uses authored far scale");
    else if(mode==75||mode==78||mode==85||mode==87)
      expectPixel(16,{132,132,0,255},"near, scale-one or disabled resampling preserves material");
    else if(mode==77||mode==83)
      expectPixel(16,{69,69,0,255},"camera-distance midpoint and neutral noise crossfade");
    else if(mode==79)
      expectPixel(16,{37,128,37,255},"normal-only resampling decodes distant RG normal");
    else if(mode==80)
      expectPixel(16,{13,26,39,6},"height-only resampling updates actual height channel");
    else if(mode==81)
      expectPixel(16,{90,90,0,255},"height-aware normalized near/far transition");
    else if(mode==82)
      expectPixel(16,{88,88,0,255},"authored noise modifies interior distance transition");
    else if(mode==86)
      expectPixel(16,{13,26,39,90},"legacy height-only disabled noise retains crossfade compatibility");
    else if(mode==88||mode==89)
      expectPixel(16,colors[1],"selection noise selects second complete variant at endpoint");
    else if(mode==90)
      expectPixel(16,{43,213,0,255},"high quality blends both distinct variants");
    else if(mode==91)
      expectPixel(16,{20,36,199,255},"three distinct variants contribute normalized PBR colors");
    else if(mode==92)
      expectPixel(16,{2,8,244,255},"cluster contrast changes normalized contributions");
    else if(mode==93){
      auto p=modePixels[mode].data()+(16*width+16)*4;
      require(p[0]>5&&p[1]>5&&p[2]>5 && modePixels[mode]!=modePixels[91],"procedural selection and noise boost change distribution");
    }
    else if(mode==94||mode==115)
      expectPixel(16,{191,238,128,255},"selected variant normal is sampled in correct frame");
    else if(mode==95)
      expectPixel(16,{204,64,153,255},"variant roughness metallic and AO remain distinct");
    else if(mode==96)
      expectPixel(16,{51,102,153,77},"variant emission and height channels remain distinct");
    else if(mode==97||mode==99)
      expectPixel(16,{129,97,0,255},"absolute variant metres override base tile and add layer offsets");
    else if(mode==98||mode==100)
      expectPixel(16,{158,129,0,255},"variant and layer rotations apply to every map");
    else if(mode==101||mode==103||mode==104||mode==105||mode==109||mode==111||mode==112||mode==114){
      require(modePixels[mode]!=modePixels[20],"stochastic and cluster plan changes real spatial samples");
      if(mode==103)require(modePixels[mode]!=modePixels[101],"two-contribution quality differs from single warp");
      if(mode==104)require(modePixels[mode]!=modePixels[103],"triangular quality differs from scalar lattice");
    }
    else if(mode==102)
      require(modePixels[mode]==modePixels[101],"stochastic base is deterministic across frames");
    else if(mode==106||mode==107||mode==108||mode==110)
      require(modePixels[mode]==modePixels[105],"camera, far scale and relocated cluster retain same spatial variant choices");
    else if(mode==113){
      const auto &a=modePixels[112];const auto &b=modePixels[113];
      for(size_t k=0;k<a.size();++k)require(std::abs(int(a[k])-int(b[k]))<=1,"triangular cell boundaries remain continuous under tiny coordinate motion");
    }
    else
      expectPixel(16, colors[0],
                  mode == 3   ? "closed side keeps vertex weights"
                  : mode == 4 ? "legacy material keeps vertex weights"
                              : "non-bindless shader keeps vertex weights");
  }
  std::puts(
      "PASS: production terrain fragment shader, narrow stripe/exterior, exact "
      "UV edges, closed side, legacy material and non-bindless fallback");
  if(timestamps)vkDestroyQueryPool(f.device,timestamps,nullptr);
  for (auto p : pipelines)
    vkDestroyPipeline(f.device, p, nullptr);
  vkDestroyShaderModule(f.device, vert, nullptr);
  vkDestroyPipelineLayout(f.device, pipelineLayout, nullptr);
  vkDestroyFramebuffer(f.device, framebuffer, nullptr);
  vkDestroyRenderPass(f.device, renderPass, nullptr);
  vkDestroyDescriptorPool(f.device, descriptorPool, nullptr);
  for (auto l : layouts)
    vkDestroyDescriptorSetLayout(f.device, l, nullptr);
  vkDestroySampler(f.device, sampler, nullptr);
  vkDestroySampler(f.device, clampSampler, nullptr);
  vkDestroySampler(f.device, clampMipSampler, nullptr);
  return 0;
}
