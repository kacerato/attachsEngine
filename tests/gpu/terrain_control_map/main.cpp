// Standalone real-device offscreen test. Uses Vulkan directly so it does not
// depend on editor state, Android, renderer mocks, image codecs or a window.
#define VK_NO_PROTOTYPES
#include "renderer/material_override.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
#define VK_FUNCTIONS(X)                                                           \
  X(CreateInstance)                                                               \
  X(DestroyInstance)                                                              \
  X(EnumeratePhysicalDevices)                                                     \
  X(GetPhysicalDeviceProperties)                                                  \
  X(GetPhysicalDeviceQueueFamilyProperties) X(                                    \
      GetPhysicalDeviceMemoryProperties) X(CreateDevice) X(DestroyDevice)         \
      X(GetDeviceQueue) X(CreateBuffer) X(DestroyBuffer) X(                       \
          GetBufferMemoryRequirements) X(AllocateMemory) X(FreeMemory)            \
          X(BindBufferMemory) X(MapMemory) X(UnmapMemory) X(CreateImage) X(       \
              DestroyImage) X(GetImageMemoryRequirements) X(BindImageMemory)      \
              X(CreateImageView) X(DestroyImageView) X(CreateSampler) X(          \
                  DestroySampler) X(CreateDescriptorSetLayout)                    \
                  X(DestroyDescriptorSetLayout) X(CreateDescriptorPool) X(        \
                      DestroyDescriptorPool) X(AllocateDescriptorSets)            \
                      X(UpdateDescriptorSets) X(CreateRenderPass) X(              \
                          DestroyRenderPass) X(CreateFramebuffer)                 \
                          X(DestroyFramebuffer) X(CreateShaderModule) X(          \
                              DestroyShaderModule) X(CreatePipelineLayout)        \
                              X(DestroyPipelineLayout) X(                         \
                                  CreateGraphicsPipelines) X(DestroyPipeline)     \
                                  X(CreateCommandPool) X(DestroyCommandPool) X(   \
                                      AllocateCommandBuffers)                     \
                                      X(BeginCommandBuffer) X(                    \
                                          EndCommandBuffer) X(CmdPipelineBarrier) \
                                          X(CmdCopyBufferToImage) X(              \
                                              CmdCopyImageToBuffer)               \
                                              X(CmdBeginRenderPass) X(            \
                                                  CmdEndRenderPass)               \
                                                  X(CmdBindPipeline) X(           \
                                                      CmdBindDescriptorSets)      \
                                                      X(CmdPushConstants) X(      \
                                                          CmdDraw)                \
                                                          X(QueueSubmit) X(       \
                                                              QueueWaitIdle)      \
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
};
struct Fixture {
  VkInstance instance{};
  VkPhysicalDevice physical{};
  VkDevice device{};
  VkQueue queue{};
  uint32_t family{};
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
          break;
        }
      if (physical)
        break;
    }
    require(physical != VK_NULL_HANDLE, "graphics Vulkan device unavailable");
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physical, &properties);
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
  Image image(uint32_t width, uint32_t height, VkImageUsageFlags usage) {
    Image im;
    im.width = width;
    VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = VK_FORMAT_R8G8B8A8_UNORM;
    ci.extent = {width, height, 1};
    ci.mipLevels = ci.arrayLayers = 1;
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
    vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
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
    b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
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
  require(argc == 2,
          "usage: astra_terrain_control_gpu <compiled shader directory>");
  Fixture f;
  constexpr uint32_t width = 32, height = 32, controlWidth = 65;
  std::array<Image, 5> textures;
  std::array<std::array<uint8_t, 4>, 4> colors{{{255, 0, 0, 255},
                                                {0, 255, 0, 255},
                                                {0, 0, 255, 255},
                                                {255, 255, 255, 255}}};
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
  for (uint32_t i = 0; i < 5; ++i) {
    uint32_t n = i == 4 ? controlWidth : 1;
    uint32_t imageHeight = i == 4 ? controlWidth : 1;
    textures[i] =
        f.image(n, imageHeight,
                VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    auto staging =
        f.buffer(n * imageHeight * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    std::memcpy(staging.mapped, i == 4 ? pixels.data() : colors[i].data(),
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
  f.submit(upload);
  constexpr size_t materialBytes =
      ae::renderer::MaterialExtensionFloats * sizeof(float);
  auto rows = f.buffer(materialBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
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
  VkDescriptorSetLayoutBinding textureBinding{
      0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5,
      VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
  VkDescriptorSetLayoutBinding rowBinding{16, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                          1, VK_SHADER_STAGE_FRAGMENT_BIT,
                                          nullptr};
  std::array<VkDescriptorSetLayout, 2> layouts;
  VkDescriptorSetLayoutCreateInfo li{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  li.bindingCount = 1;
  li.pBindings = &textureBinding;
  check(vkCreateDescriptorSetLayout(f.device, &li, nullptr, &layouts[0]),
        "texture layout");
  li.pBindings = &rowBinding;
  check(vkCreateDescriptorSetLayout(f.device, &li, nullptr, &layouts[1]),
        "material row layout");
  VkDescriptorPoolSize sizes[]{{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5},
                               {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}};
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
  std::array<VkDescriptorImageInfo, 5> infos;
  for (size_t i = 0; i < 5; ++i)
    infos[i] = {sampler, textures[i].view,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  VkDescriptorBufferInfo rinfo{rows.handle, 0, materialBytes};
  std::array<VkWriteDescriptorSet, 2> writes{};
  for (auto &w : writes)
    w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].dstSet = sets[0];
  writes[0].dstBinding = 0;
  writes[0].descriptorCount = 5;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  writes[0].pImageInfo = infos.data();
  writes[1].dstSet = sets[1];
  writes[1].dstBinding = 16;
  writes[1].descriptorCount = 1;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[1].pBufferInfo = &rinfo;
  vkUpdateDescriptorSets(f.device, 2, writes.data(), 0, nullptr);
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
  std::array<VkPipeline, 2> pipelines;
  for (size_t i = 0; i < 2; ++i) {
    auto frag = shader(
        f.device, directory + (i ? "/fallback.frag.spv" : "/control.frag.spv"));
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
                         const char *label) {
    auto *p = static_cast<uint8_t *>(readback.mapped) + (16 * width + x) * 4;
    for (size_t c = 0; c < 4; ++c)
      if (std::abs(int(p[c]) - int(expected[c])) > 3) {
        std::fprintf(stderr, "%s at x=%u got RGBA %u,%u,%u,%u\n", label, x,
                     p[0], p[1], p[2], p[3]);
        require(false, label);
      }
  };
  for (uint32_t mode = 0; mode < 8; ++mode) {
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
    auto cmd = f.command();
    VkClearValue clear{};
    VkRenderPassBeginInfo bi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    bi.renderPass = renderPass;
    bi.framebuffer = framebuffer;
    bi.renderArea = {{0, 0}, {width, height}};
    bi.clearValueCount = 1;
    bi.pClearValues = &clear;
    vkCmdBeginRenderPass(cmd, &bi, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipelines[mode == 5]);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout, 0, 2, sets.data(), 0, nullptr);
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4,
                       &mode);
    vkCmdDraw(cmd, 6, 1, 0, 0);
    vkCmdEndRenderPass(cmd);
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
    std::string imagePath =
        directory + "/terrain-control-mode-" + std::to_string(mode) + ".ppm";
    std::ofstream evidence(imagePath, std::ios::binary);
    evidence << "P6\n" << width << ' ' << height << "\n255\n";
    const auto *rgba = static_cast<const uint8_t *>(readback.mapped);
    for (size_t p = 0; p < width * height; ++p)
      evidence.write(reinterpret_cast<const char *>(rgba + p * 4), 3);
    require(bool(evidence), "write real GPU evidence PPM");
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
    else
      expectPixel(16, colors[0],
                  mode == 3   ? "closed side keeps vertex weights"
                  : mode == 4 ? "legacy material keeps vertex weights"
                              : "non-bindless shader keeps vertex weights");
  }
  std::puts(
      "PASS: production terrain fragment shader, narrow stripe/exterior, exact "
      "UV edges, closed side, legacy material and non-bindless fallback");
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
  return 0;
}
