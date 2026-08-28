#include "platform/android/instanced_renderer.h"

#include "rhi/shaders/instanced_spirv.h"
#include "rhi/shaders/instanced_fallback_spirv.h"

#include <android/log.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>

namespace ae::platform::android {

namespace {
constexpr const char *LogTag = "Aether.Android";
constexpr u32 kBytesPerInstance = 5 * sizeof(float); // vec2 posição + vec3 cor, ver instanced.vert
constexpr u32 kTextureSize = 64;
// Capacidade do array bindless desta PoC: 1 textura real (o checker board do
// cubo) é suficiente hoje, mas o registro precisa caber pelo menos a dummy +
// a textura real: ver comentário de BindlessTextureRegistry::initialize sobre
// por que todo slot é preenchido com a dummy antes de qualquer registro.
constexpr u32 kBindlessCapacity = 64;

struct FramePushConstants {
  float timeSeconds;
  float aspectRatio;
  float orbitYaw;
  float orbitPitch;
  float surfaceXX;
  float surfaceXY;
  float surfaceYX;
  float surfaceYY;
  u32 materialIndex;
  float padding[3]; // mantém o struct múltiplo de 16 bytes (regra comum de alinhamento de push constant)
};
static_assert(sizeof(FramePushConstants) == 48);

bool formatSupportsDepthAttachment(VkPhysicalDevice physicalDevice, VkFormat format) {
  VkFormatProperties properties{};
  vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);
  return (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
}

VkFormat chooseDepthFormat(VkPhysicalDevice physicalDevice) {
  constexpr VkFormat candidates[] = {
      VK_FORMAT_D32_SFLOAT,
      VK_FORMAT_D24_UNORM_S8_UINT,
      VK_FORMAT_D16_UNORM,
  };
  for (VkFormat candidate : candidates) {
    if (formatSupportsDepthAttachment(physicalDevice, candidate)) return candidate;
  }
  return VK_FORMAT_UNDEFINED;
}

bool hasStencil(VkFormat format) {
  return format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D32_SFLOAT_S8_UINT;
}

VkShaderModule createShaderModule(VkDevice device, const uint32_t *code, uint32_t codeSize) {
  VkShaderModuleCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  info.codeSize = codeSize;
  info.pCode = code;
  VkShaderModule module = VK_NULL_HANDLE;
  if (vkCreateShaderModule(device, &info, nullptr, &module) != VK_SUCCESS) return VK_NULL_HANDLE;
  return module;
}

} // namespace

InstancedRenderer::~InstancedRenderer() {
  shutdown();
}

bool InstancedRenderer::createRenderPass() {
  VkAttachmentDescription colorAttachment{};
  colorAttachment.format = swapchain_->imageFormat();
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference colorRef{};
  colorRef.attachment = 0;
  colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentDescription depthAttachment{};
  depthAttachment.format = depthFormat_;
  depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthRef{};
  depthRef.attachment = 1;
  depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorRef;
  subpass.pDepthStencilAttachment = &depthRef;

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  const VkAttachmentDescription attachments[] = {colorAttachment, depthAttachment};

  VkRenderPassCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  info.attachmentCount = 2;
  info.pAttachments = attachments;
  info.subpassCount = 1;
  info.pSubpasses = &subpass;
  info.dependencyCount = 1;
  info.pDependencies = &dependency;

  return vkCreateRenderPass(device_, &info, nullptr, &renderPass_) == VK_SUCCESS;
}

bool InstancedRenderer::createPipeline() {
  VkShaderModule vertModule = createShaderModule(device_, rhi::shaders::kInstancedVertSpirv,
                                                 rhi::shaders::kInstancedVertSpirvSize);
  VkShaderModule fragModule = useBindless_
      ? createShaderModule(device_, rhi::shaders::kInstancedFragSpirv, rhi::shaders::kInstancedFragSpirvSize)
      : createShaderModule(device_, rhi::shaders::kInstanced_FallbackFragSpirv,
                           rhi::shaders::kInstanced_FallbackFragSpirvSize);
  if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE) {
    if (vertModule != VK_NULL_HANDLE) vkDestroyShaderModule(device_, vertModule, nullptr);
    if (fragModule != VK_NULL_HANDLE) vkDestroyShaderModule(device_, fragModule, nullptr);
    return false;
  }

  VkPipelineShaderStageCreateInfo stages[2]{};
  stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = vertModule;
  stages[0].pName = "main";
  stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = fragModule;
  stages[1].pName = "main";

  // Um binding por instância (VK_VERTEX_INPUT_RATE_INSTANCE) — o cubo em si
  // não tem vertex buffer (gerado por gl_VertexIndex igual ao TriangleRenderer,
  // ver instanced.vert), só a posição/cor avança por instância, não por
  // vértice.
  VkVertexInputBindingDescription binding{};
  binding.binding = 1;
  binding.stride = kBytesPerInstance;
  binding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

  VkVertexInputAttributeDescription attributes[2]{};
  attributes[0].location = 0;
  attributes[0].binding = 1;
  attributes[0].format = VK_FORMAT_R32G32_SFLOAT;
  attributes[0].offset = 0;
  attributes[1].location = 1;
  attributes[1].binding = 1;
  attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
  attributes[1].offset = 2 * sizeof(float);

  VkPipelineVertexInputStateCreateInfo vertexInput{};
  vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInput.vertexBindingDescriptionCount = 1;
  vertexInput.pVertexBindingDescriptions = &binding;
  vertexInput.vertexAttributeDescriptionCount = 2;
  vertexInput.pVertexAttributeDescriptions = attributes;

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicState{};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = 2;
  dynamicState.pDynamicStates = dynamicStates;

  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.cullMode = VK_CULL_MODE_NONE;
  rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rasterizer.lineWidth = 1.0f;

  VkPipelineMultisampleStateCreateInfo multisample{};
  multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineDepthStencilStateCreateInfo depthStencil{};
  depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.depthTestEnable = VK_TRUE;
  depthStencil.depthWriteEnable = VK_TRUE;
  depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
  depthStencil.minDepthBounds = 0.0f;
  depthStencil.maxDepthBounds = 1.0f;

  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo colorBlend{};
  colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlend.attachmentCount = 1;
  colorBlend.pAttachments = &colorBlendAttachment;

  const VkDescriptorSetLayout bindlessLayout = textureSetLayout_;
  VkPipelineLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layoutInfo.setLayoutCount = 1;
  layoutInfo.pSetLayouts = &bindlessLayout;
  VkPushConstantRange pushRange{};
  // vertex lê timeSeconds/aspectRatio/órbita/surfaceTransform; fragment lê
  // materialIndex para indexar o array bindless (ver instanced.frag) — as
  // duas stages compartilham o mesmo range porque é um único struct.
  pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  pushRange.size = sizeof(FramePushConstants);
  layoutInfo.pushConstantRangeCount = 1;
  layoutInfo.pPushConstantRanges = &pushRange;
  const bool layoutOk = vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &pipelineLayout_) == VK_SUCCESS;

  bool pipelineOk = false;
  if (layoutOk) {
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout_;
    pipelineInfo.renderPass = renderPass_;
    pipelineInfo.subpass = 0;

    pipelineOk = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                           &pipeline_) == VK_SUCCESS;
  }

  vkDestroyShaderModule(device_, vertModule, nullptr);
  vkDestroyShaderModule(device_, fragModule, nullptr);
  return layoutOk && pipelineOk;
}

bool InstancedRenderer::createFramebuffers() {
  framebufferCount_ = swapchain_->imageCount();
  for (u32 i = 0; i < framebufferCount_; ++i) {
    const VkImageView attachments[] = {swapchain_->imageView(i), depthImage_.view()};
    VkFramebufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.renderPass = renderPass_;
    info.attachmentCount = 2;
    info.pAttachments = attachments;
    info.width = swapchain_->width();
    info.height = swapchain_->height();
    info.layers = 1;
    if (vkCreateFramebuffer(device_, &info, nullptr, &framebuffers_[i]) != VK_SUCCESS) {
      return false;
    }
  }
  return true;
}

bool InstancedRenderer::createCommandResources() {
  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = graphicsQueueFamily_;
  if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_) != VK_SUCCESS) return false;

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = commandPool_;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;
  if (vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer_) != VK_SUCCESS) return false;
  return uploadContext_.initialize(device_, graphicsQueueFamily_);
}

void InstancedRenderer::destroyFramebuffers() {
  for (u32 i = 0; i < framebufferCount_; ++i) {
    if (framebuffers_[i] != VK_NULL_HANDLE) {
      vkDestroyFramebuffer(device_, framebuffers_[i], nullptr);
      framebuffers_[i] = VK_NULL_HANDLE;
    }
  }
  framebufferCount_ = 0;
}

bool InstancedRenderer::createInstanceBuffer() {
  rhi::BufferDesc desc{};
  desc.sizeBytes = static_cast<u64>(instanceCount_) * kBytesPerInstance;
  desc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  desc.memoryClass = rhi::MemoryClass::Buffer;
  desc.cpuAccess = rhi::CpuAccess::SequentialWrite;
  desc.preferDeviceMemory = false;
  return memoryAllocator_ != nullptr && memoryAllocator_->createBuffer(desc, &instanceBuffer_) &&
         instanceBuffer_.mappedData() != nullptr;
}

bool InstancedRenderer::createDepthImage() {
  depthFormat_ = chooseDepthFormat(physicalDevice_);
  if (depthFormat_ == VK_FORMAT_UNDEFINED) return false;
  rhi::ImageDesc desc{};
  desc.width = swapchain_->width();
  desc.height = swapchain_->height();
  desc.format = depthFormat_;
  desc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  desc.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  if (hasStencil(depthFormat_)) desc.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
  desc.memoryClass = rhi::MemoryClass::RenderTarget;
  return memoryAllocator_->createImage(desc, &depthImage_);
}

bool InstancedRenderer::createTextureResources() {
  // Dummy 1x1 branca: exigida por BindlessTextureRegistry::initialize para
  // preencher todo slot do array antes de qualquer registro real (ver
  // comentário no header) — nunca é de fato amostrada em um caminho correto,
  // já que baseTextureIndex_ é sempre válido antes do primeiro drawFrame.
  const std::array<u8, 4> dummyPixels{255, 255, 255, 255};
  rhi::ImageDesc dummyDesc{};
  dummyDesc.width = 1;
  dummyDesc.height = 1;
  dummyDesc.format = VK_FORMAT_R8G8B8A8_SRGB;
  dummyDesc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  dummyDesc.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  dummyDesc.memoryClass = rhi::MemoryClass::Texture;
  if (!memoryAllocator_->createImage(dummyDesc, &dummyTexture_)) return false;
  if (!uploadContext_.uploadRgba8ToSampledImage(*memoryAllocator_, dummyPixels.data(),
                                                sizeof(dummyPixels), dummyTexture_)) {
    return false;
  }
  rhi::SamplerDesc dummySamplerDesc{};
  dummySamplerDesc.minFilter = VK_FILTER_NEAREST;
  dummySamplerDesc.magFilter = VK_FILTER_NEAREST;
  dummySamplerDesc.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  if (!dummySampler_.initialize(device_, dummySamplerDesc)) return false;

  std::array<u8, kTextureSize * kTextureSize * 4> pixels{};
  for (u32 y = 0; y < kTextureSize; ++y) {
    for (u32 x = 0; x < kTextureSize; ++x) {
      const bool light = ((x / 8u) + (y / 8u)) % 2u == 0;
      const usize offset = static_cast<usize>(y * kTextureSize + x) * 4;
      pixels[offset + 0] = light ? 220 : 24;
      pixels[offset + 1] = light ? 245 : 76;
      pixels[offset + 2] = light ? 245 : 86;
      pixels[offset + 3] = 255;
    }
  }

  rhi::ImageDesc imageDesc{};
  imageDesc.width = kTextureSize;
  imageDesc.height = kTextureSize;
  imageDesc.format = VK_FORMAT_R8G8B8A8_SRGB;
  imageDesc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  imageDesc.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  imageDesc.memoryClass = rhi::MemoryClass::Texture;
  if (!memoryAllocator_->createImage(imageDesc, &baseTexture_)) return false;
  if (!uploadContext_.uploadRgba8ToSampledImage(*memoryAllocator_, pixels.data(), sizeof(pixels),
                                                baseTexture_)) {
    return false;
  }

  rhi::SamplerDesc samplerDesc{};
  samplerDesc.minFilter = VK_FILTER_NEAREST;
  samplerDesc.magFilter = VK_FILTER_NEAREST;
  samplerDesc.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  return baseSampler_.initialize(device_, samplerDesc);
}

bool InstancedRenderer::createBindlessRegistry() {
  if (!bindlessRegistry_.initialize(device_, std::min(kBindlessCapacity, rhiDevice_->bindlessTextureCapacity()), dummyTexture_.view(),
                                    dummySampler_.handle())) {
    return false;
  }
  baseTextureIndex_ = bindlessRegistry_.registerTexture(
      baseTexture_.view(), baseSampler_.handle(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  return baseTextureIndex_ != rhi::kBindlessIndexInvalid;
}

bool InstancedRenderer::createTextureDescriptors() {
  if (useBindless_) {
    if (!createBindlessRegistry()) return false;
    textureSetLayout_ = bindlessRegistry_.layout();
    textureSet_ = bindlessRegistry_.descriptorSet();
    return true;
  }
  VkDescriptorSetLayoutBinding binding{};
  binding.binding = 0;
  binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  binding.descriptorCount = 1;
  binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  VkDescriptorSetLayoutCreateInfo layout{};
  layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout.bindingCount = 1;
  layout.pBindings = &binding;
  if (vkCreateDescriptorSetLayout(device_, &layout, nullptr, &textureSetLayout_) != VK_SUCCESS) return false;
  VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
  VkDescriptorPoolCreateInfo pool{};
  pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool.maxSets = 1;
  pool.poolSizeCount = 1;
  pool.pPoolSizes = &size;
  if (vkCreateDescriptorPool(device_, &pool, nullptr, &texturePool_) != VK_SUCCESS) return false;
  VkDescriptorSetAllocateInfo allocation{};
  allocation.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocation.descriptorPool = texturePool_;
  allocation.descriptorSetCount = 1;
  allocation.pSetLayouts = &textureSetLayout_;
  if (vkAllocateDescriptorSets(device_, &allocation, &textureSet_) != VK_SUCCESS) return false;
  VkDescriptorImageInfo image{baseSampler_.handle(), baseTexture_.view(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = textureSet_;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  write.pImageInfo = &image;
  vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
  baseTextureIndex_ = 0;
  return true;
}

bool InstancedRenderer::initialize(rhi::VulkanDevice &device, rhi::VulkanSwapchain &swapchain,
                                   DotNetHost &dotNetHost, u32 instanceCount) {
  if (instanceCount == 0 || !dotNetHost.isReady()) return false;

  device_ = device.handle();
  physicalDevice_ = device.physicalDevice();
  rhiDevice_ = &device;
  useBindless_ = device.enabledPaths().bindless;
  memoryAllocator_ = &device.memoryAllocator();
  swapchain_ = &swapchain;
  graphicsQueueFamily_ = device.graphicsQueueFamily();
  instanceCount_ = instanceCount;
  vkGetDeviceQueue(device_, graphicsQueueFamily_, 0, &graphicsQueue_);

  fillInstanceBuffer_ = reinterpret_cast<FillInstanceBufferFn>(dotNetHost.getManagedFunctionPointer(
      "Aether.Interop.NativeEntryPoints, Aether.Core", "FillInstanceBuffer"));
  if (fillInstanceBuffer_ == nullptr) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao resolver FillInstanceBuffer.");
    return false;
  }

  if (!createCommandResources()) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar recursos de comando/upload.");
    return false;
  }
  if (!createInstanceBuffer()) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar o buffer de instâncias.");
    return false;
  }
  if (!createTextureResources()) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar/upload da textura do cubo.");
    return false;
  }
  if (!createDepthImage()) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar o depth buffer.");
    return false;
  }
  if (!createTextureDescriptors()) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar os descritores de textura.");
    return false;
  }
  if (!createRenderPass()) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar o render pass instanciado.");
    return false;
  }
  if (!createPipeline()) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar o pipeline instanciado.");
    return false;
  }
  if (!createFramebuffers()) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar os framebuffers instanciados.");
    return false;
  }
  // Item 2.1.6 do plano: nomeia os objetos-chave deste renderer para que
  // captures de frame (RenderDoc/Android GPU Inspector) e mensagens de
  // validação mostrem "InstancedRenderer/pipeline" em vez de um handle
  // opaco. No-op em build release (setObjectName checa debugUtilsEnabled()
  // internamente via VulkanDevice).
  rhiDevice_->setObjectName(VK_OBJECT_TYPE_PIPELINE, reinterpret_cast<u64>(pipeline_),
                            "InstancedRenderer/pipeline");
  rhiDevice_->setObjectName(VK_OBJECT_TYPE_PIPELINE_LAYOUT, reinterpret_cast<u64>(pipelineLayout_),
                            "InstancedRenderer/pipelineLayout");
  rhiDevice_->setObjectName(VK_OBJECT_TYPE_DESCRIPTOR_SET,
                            reinterpret_cast<u64>(textureSet_),
                            "InstancedRenderer/textureSet");
  rhiDevice_->setObjectName(VK_OBJECT_TYPE_BUFFER, reinterpret_cast<u64>(instanceBuffer_.handle()),
                            "InstancedRenderer/instanceBuffer");
  rhiDevice_->setObjectName(VK_OBJECT_TYPE_IMAGE, reinterpret_cast<u64>(baseTexture_.handle()),
                            "InstancedRenderer/baseTexture");
  rhiDevice_->setObjectName(VK_OBJECT_TYPE_COMMAND_BUFFER, reinterpret_cast<u64>(commandBuffer_),
                            "InstancedRenderer/commandBuffer");

  const rhi::MemoryBudgetEntry bufferBudget =
      memoryAllocator_->budgetSnapshot().entries[static_cast<usize>(rhi::MemoryClass::Buffer)];
  __android_log_print(ANDROID_LOG_INFO, LogTag, "Descritores: caminho=%s, capacidade bindless=%u.",
                      useBindless_ ? "bindless" : "fallback", device.bindlessTextureCapacity());
  __android_log_print(ANDROID_LOG_INFO, LogTag,
                      "RHI/VMA: buffer de instâncias=%llu bytes; uso=%llu, limite=%llu bytes.",
                      static_cast<unsigned long long>(instanceBuffer_.sizeBytes()),
                      static_cast<unsigned long long>(bufferBudget.usedBytes),
                      static_cast<unsigned long long>(bufferBudget.limitBytes));

  __android_log_print(ANDROID_LOG_INFO, LogTag,
                      "InstancedRenderer pronto: cubo texturizado + depth, %u instâncias (PoC-A).",
                      instanceCount_);
  const rhi::MemoryBudgetSnapshot snapshot = memoryAllocator_->budgetSnapshot();
  for (usize i = 0; i < rhi::MemoryClassCount; ++i) {
    __android_log_print(ANDROID_LOG_INFO, LogTag, "RHI/VMA [%s]: uso=%llu, pico=%llu bytes.",
                        rhi::memoryClassName(static_cast<rhi::MemoryClass>(i)),
                        static_cast<unsigned long long>(snapshot.entries[i].usedBytes),
                        static_cast<unsigned long long>(snapshot.entries[i].peakBytes));
  }
  return true;
}

void InstancedRenderer::shutdown() {
  if (device_ == VK_NULL_HANDLE) return;
  vkDeviceWaitIdle(device_);

  if (commandPool_ != VK_NULL_HANDLE) {
    vkDestroyCommandPool(device_, commandPool_, nullptr);
    commandPool_ = VK_NULL_HANDLE;
    commandBuffer_ = VK_NULL_HANDLE;
  }
  destroyFramebuffers();
  if (pipeline_ != VK_NULL_HANDLE) {
    vkDestroyPipeline(device_, pipeline_, nullptr);
    pipeline_ = VK_NULL_HANDLE;
  }
  if (pipelineLayout_ != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
    pipelineLayout_ = VK_NULL_HANDLE;
  }
  if (renderPass_ != VK_NULL_HANDLE) {
    vkDestroyRenderPass(device_, renderPass_, nullptr);
    renderPass_ = VK_NULL_HANDLE;
  }
  bindlessRegistry_.shutdown();
  if (texturePool_ != VK_NULL_HANDLE) vkDestroyDescriptorPool(device_, texturePool_, nullptr);
  if (!useBindless_ && textureSetLayout_ != VK_NULL_HANDLE)
    vkDestroyDescriptorSetLayout(device_, textureSetLayout_, nullptr);
  texturePool_ = VK_NULL_HANDLE;
  textureSetLayout_ = VK_NULL_HANDLE;
  textureSet_ = VK_NULL_HANDLE;
  useBindless_ = false;
  baseTextureIndex_ = rhi::kBindlessIndexInvalid;
  baseSampler_.shutdown();
  baseTexture_.reset();
  dummySampler_.shutdown();
  dummyTexture_.reset();
  depthImage_.reset();
  instanceBuffer_.reset();
  uploadContext_.shutdown();
  device_ = VK_NULL_HANDLE;
  physicalDevice_ = VK_NULL_HANDLE;
  rhiDevice_ = nullptr;
  memoryAllocator_ = nullptr;
  swapchain_ = nullptr;
  fillInstanceBuffer_ = nullptr;
}

rhi::SwapchainStatus InstancedRenderer::drawFrame(float timeSeconds, float orbitYaw,
                                                  float orbitPitch) {
  using Clock = std::chrono::steady_clock;
  const auto acquireStart = frameProfilingEnabled_ ? Clock::now() : Clock::time_point{};
  lastFrameTimings_ = {};
  u32 imageIndex = 0;
  const rhi::SwapchainStatus acquireStatus = swapchain_->acquireNextImage(&imageIndex);
  if (acquireStatus != rhi::SwapchainStatus::Ok &&
      acquireStatus != rhi::SwapchainStatus::SuboptimalNeedsRecreate) {
    return acquireStatus;
  }

  // O crossing C++→C# medido isoladamente: só o tempo de FillInstanceBuffer,
  // não o frame Vulkan inteiro — a pergunta da PoC-A é sobre o custo da
  // fronteira de interop especificamente.
  const auto fillStart = std::chrono::steady_clock::now();
  fillInstanceBuffer_(static_cast<float *>(instanceBuffer_.mappedData()),
                      static_cast<int>(instanceCount_), timeSeconds);
  const auto fillEnd = std::chrono::steady_clock::now();
  lastFillMicroseconds_ =
      std::chrono::duration<double, std::micro>(fillEnd - fillStart).count();
  if (frameProfilingEnabled_) {
    lastFrameTimings_.acquireMs = std::chrono::duration<double, std::milli>(fillStart - acquireStart).count();
    lastFrameTimings_.interopMs = lastFillMicroseconds_ / 1000.0;
  }
  if (!memoryAllocator_->flushBuffer(instanceBuffer_)) return rhi::SwapchainStatus::FatalError;

  if (vkResetCommandBuffer(commandBuffer_, 0) != VK_SUCCESS) return rhi::SwapchainStatus::FatalError;
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  if (vkBeginCommandBuffer(commandBuffer_, &beginInfo) != VK_SUCCESS) {
    return rhi::SwapchainStatus::FatalError;
  }

  VkClearValue clearValues[2]{};
  clearValues[0].color = {{0.02f, 0.02f, 0.05f, 1.0f}};
  clearValues[1].depthStencil = {1.0f, 0};

  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = renderPass_;
  renderPassInfo.framebuffer = framebuffers_[imageIndex];
  renderPassInfo.renderArea.extent = {swapchain_->width(), swapchain_->height()};
  renderPassInfo.clearValueCount = 2;
  renderPassInfo.pClearValues = clearValues;
  vkCmdBeginRenderPass(commandBuffer_, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
  // Item 2.1.6 do plano: marcador de debug em volta do desenho do cubo —
  // aparece como um grupo nomeado em RenderDoc/Android GPU Inspector ao
  // capturar um frame. No-op em build release.
  rhiDevice_->cmdBeginDebugLabel(commandBuffer_, "InstancedRenderer/cube", 0.2f, 0.6f, 0.9f);

  vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
  const VkDescriptorSet bindlessSet = textureSet_;
  vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1,
                          &bindlessSet, 0, nullptr);
  const VkExtent2D displayExtent = swapchain_->displayExtent();
  const rhi::SurfaceTransform &surfaceTransform = swapchain_->surfaceTransform();
  const FramePushConstants pushConstants{
      timeSeconds,
      static_cast<float>(displayExtent.width) / static_cast<float>(displayExtent.height),
      orbitYaw,
      orbitPitch,
      surfaceTransform.xx, surfaceTransform.xy, surfaceTransform.yx, surfaceTransform.yy,
      baseTextureIndex_,
      {},
  };
  vkCmdPushConstants(commandBuffer_, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                     0, sizeof(pushConstants), &pushConstants);

  VkViewport viewport{};
  viewport.width = static_cast<float>(swapchain_->width());
  viewport.height = static_cast<float>(swapchain_->height());
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer_, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.extent = {swapchain_->width(), swapchain_->height()};
  vkCmdSetScissor(commandBuffer_, 0, 1, &scissor);

  VkDeviceSize offset = 0;
  const VkBuffer instanceBufferHandle = instanceBuffer_.handle();
  vkCmdBindVertexBuffers(commandBuffer_, 1, 1, &instanceBufferHandle, &offset);
  vkCmdDraw(commandBuffer_, 36, instanceCount_, 0, 0); // 36 vértices/cubo (12 triângulos)

  rhiDevice_->cmdEndDebugLabel(commandBuffer_);
  vkCmdEndRenderPass(commandBuffer_);
  if (vkEndCommandBuffer(commandBuffer_) != VK_SUCCESS) return rhi::SwapchainStatus::FatalError;

  const VkSemaphore waitSemaphore = swapchain_->imageAvailableSemaphore();
  const VkSemaphore signalSemaphore = swapchain_->renderFinishedSemaphore(imageIndex);
  VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = &waitSemaphore;
  submitInfo.pWaitDstStageMask = &waitStage;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer_;
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = &signalSemaphore;

  if (vkQueueSubmit(graphicsQueue_, 1, &submitInfo, swapchain_->inFlightFence()) != VK_SUCCESS) {
    return rhi::SwapchainStatus::FatalError;
  }

  const auto presentStart = frameProfilingEnabled_ ? Clock::now() : Clock::time_point{};
  const rhi::SwapchainStatus presentStatus = swapchain_->present(imageIndex);
  if (frameProfilingEnabled_) {
    const auto presentEnd = Clock::now();
    lastFrameTimings_.recordSubmitMs = std::chrono::duration<double, std::milli>(presentStart - fillEnd).count();
    lastFrameTimings_.presentMs = std::chrono::duration<double, std::milli>(presentEnd - presentStart).count();
  }
  if (presentStatus != rhi::SwapchainStatus::Ok) return presentStatus;
  return acquireStatus == rhi::SwapchainStatus::SuboptimalNeedsRecreate
             ? rhi::SwapchainStatus::SuboptimalNeedsRecreate
             : rhi::SwapchainStatus::Ok;
}

} // namespace ae::platform::android
