#include "platform/android/instanced_renderer.h"

#include "rhi/shaders/instanced_spirv.h"

#include <android/log.h>
#include <chrono>
#include <cstring>

namespace ae::platform::android {

namespace {
constexpr const char *LogTag = "Aether.Android";
constexpr u32 kBytesPerInstance = 5 * sizeof(float); // vec2 posição + vec3 cor, ver instanced.vert

VkShaderModule createShaderModule(VkDevice device, const uint32_t *code, uint32_t codeSize) {
  VkShaderModuleCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  info.codeSize = codeSize;
  info.pCode = code;
  VkShaderModule module = VK_NULL_HANDLE;
  if (vkCreateShaderModule(device, &info, nullptr, &module) != VK_SUCCESS) return VK_NULL_HANDLE;
  return module;
}

// Mesma varredura padrão de tipo de memória que qualquer app Vulkan precisa
// fazer sem VMA (que só entra no RHI completo, Onda 3, item 7.1).
bool findMemoryType(VkPhysicalDevice physicalDevice, u32 typeFilter, VkMemoryPropertyFlags properties,
                    u32 *outIndex) {
  VkPhysicalDeviceMemoryProperties memoryProperties{};
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
  for (u32 i = 0; i < memoryProperties.memoryTypeCount; ++i) {
    if ((typeFilter & (1u << i)) &&
        (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
      *outIndex = i;
      return true;
    }
  }
  return false;
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

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorRef;

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  info.attachmentCount = 1;
  info.pAttachments = &colorAttachment;
  info.subpassCount = 1;
  info.pSubpasses = &subpass;
  info.dependencyCount = 1;
  info.pDependencies = &dependency;

  return vkCreateRenderPass(device_, &info, nullptr, &renderPass_) == VK_SUCCESS;
}

bool InstancedRenderer::createPipeline() {
  VkShaderModule vertModule = createShaderModule(device_, rhi::shaders::kInstancedVertSpirv,
                                                 rhi::shaders::kInstancedVertSpirvSize);
  VkShaderModule fragModule = createShaderModule(device_, rhi::shaders::kInstancedFragSpirv,
                                                 rhi::shaders::kInstancedFragSpirvSize);
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

  // Um binding por instância (VK_VERTEX_INPUT_RATE_INSTANCE) — o quad em si
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

  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo colorBlend{};
  colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlend.attachmentCount = 1;
  colorBlend.pAttachments = &colorBlendAttachment;

  VkPipelineLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
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
    VkImageView attachment = swapchain_->imageView(i);
    VkFramebufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.renderPass = renderPass_;
    info.attachmentCount = 1;
    info.pAttachments = &attachment;
    info.width = swapchain_->width();
    info.height = swapchain_->height();
    info.layers = 1;
    if (vkCreateFramebuffer(device_, &info, nullptr, &framebuffers_[i]) != VK_SUCCESS) {
      return false;
    }
  }
  return true;
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
  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = static_cast<VkDeviceSize>(instanceCount_) * kBytesPerInstance;
  bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  if (vkCreateBuffer(device_, &bufferInfo, nullptr, &instanceBuffer_) != VK_SUCCESS) return false;

  VkMemoryRequirements requirements{};
  vkGetBufferMemoryRequirements(device_, instanceBuffer_, &requirements);

  u32 memoryTypeIndex = 0;
  const VkMemoryPropertyFlags hostVisibleCoherent =
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  if (!findMemoryType(physicalDevice_, requirements.memoryTypeBits, hostVisibleCoherent,
                      &memoryTypeIndex)) {
    return false;
  }

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = requirements.size;
  allocInfo.memoryTypeIndex = memoryTypeIndex;
  if (vkAllocateMemory(device_, &allocInfo, nullptr, &instanceBufferMemory_) != VK_SUCCESS) {
    return false;
  }
  if (vkBindBufferMemory(device_, instanceBuffer_, instanceBufferMemory_, 0) != VK_SUCCESS) {
    return false;
  }

  // Mapeado uma vez pelo tempo de vida do buffer (host-coherent: sem
  // flush/invalidate manual) — FillInstanceBuffer escreve direto aqui a
  // cada frame, sem re-mapear.
  return vkMapMemory(device_, instanceBufferMemory_, 0, bufferInfo.size, 0, &instanceBufferMapped_) ==
         VK_SUCCESS;
}

bool InstancedRenderer::initialize(rhi::VulkanDevice &device, rhi::VulkanSwapchain &swapchain,
                                   DotNetHost &dotNetHost, u32 instanceCount) {
  if (instanceCount == 0 || !dotNetHost.isReady()) return false;

  device_ = device.handle();
  physicalDevice_ = device.physicalDevice();
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
  if (!createInstanceBuffer()) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar o buffer de instâncias.");
    return false;
  }

  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = graphicsQueueFamily_;
  if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_) != VK_SUCCESS) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar o command pool instanciado.");
    return false;
  }

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = commandPool_;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;
  if (vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer_) != VK_SUCCESS) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao alocar o command buffer instanciado.");
    return false;
  }

  __android_log_print(ANDROID_LOG_INFO, LogTag, "InstancedRenderer pronto: %u instâncias (PoC-A).",
                      instanceCount_);
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
  if (instanceBufferMemory_ != VK_NULL_HANDLE) {
    if (instanceBufferMapped_ != nullptr) vkUnmapMemory(device_, instanceBufferMemory_);
    vkFreeMemory(device_, instanceBufferMemory_, nullptr);
    instanceBufferMemory_ = VK_NULL_HANDLE;
    instanceBufferMapped_ = nullptr;
  }
  if (instanceBuffer_ != VK_NULL_HANDLE) {
    vkDestroyBuffer(device_, instanceBuffer_, nullptr);
    instanceBuffer_ = VK_NULL_HANDLE;
  }
  device_ = VK_NULL_HANDLE;
  physicalDevice_ = VK_NULL_HANDLE;
  swapchain_ = nullptr;
  fillInstanceBuffer_ = nullptr;
}

rhi::SwapchainStatus InstancedRenderer::drawFrame(float timeSeconds) {
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
  fillInstanceBuffer_(static_cast<float *>(instanceBufferMapped_),
                      static_cast<int>(instanceCount_), timeSeconds);
  const auto fillEnd = std::chrono::steady_clock::now();
  lastFillMicroseconds_ =
      std::chrono::duration<double, std::micro>(fillEnd - fillStart).count();

  if (vkResetCommandBuffer(commandBuffer_, 0) != VK_SUCCESS) return rhi::SwapchainStatus::FatalError;
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  if (vkBeginCommandBuffer(commandBuffer_, &beginInfo) != VK_SUCCESS) {
    return rhi::SwapchainStatus::FatalError;
  }

  VkClearValue clearColor{};
  clearColor.color = {{0.02f, 0.02f, 0.05f, 1.0f}};

  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = renderPass_;
  renderPassInfo.framebuffer = framebuffers_[imageIndex];
  renderPassInfo.renderArea.extent = {swapchain_->width(), swapchain_->height()};
  renderPassInfo.clearValueCount = 1;
  renderPassInfo.pClearValues = &clearColor;
  vkCmdBeginRenderPass(commandBuffer_, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

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
  vkCmdBindVertexBuffers(commandBuffer_, 1, 1, &instanceBuffer_, &offset);
  vkCmdDraw(commandBuffer_, 6, instanceCount_, 0, 0); // 6 vértices/quad (2 triângulos)

  vkCmdEndRenderPass(commandBuffer_);
  if (vkEndCommandBuffer(commandBuffer_) != VK_SUCCESS) return rhi::SwapchainStatus::FatalError;

  const VkSemaphore waitSemaphore = swapchain_->imageAvailableSemaphore();
  const VkSemaphore signalSemaphore = swapchain_->renderFinishedSemaphore();
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

  const rhi::SwapchainStatus presentStatus = swapchain_->present(imageIndex);
  if (presentStatus != rhi::SwapchainStatus::Ok) return presentStatus;
  return acquireStatus == rhi::SwapchainStatus::SuboptimalNeedsRecreate
             ? rhi::SwapchainStatus::SuboptimalNeedsRecreate
             : rhi::SwapchainStatus::Ok;
}

} // namespace ae::platform::android
