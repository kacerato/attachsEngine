#include "platform/android/android_triangle_renderer.h"

#include "rhi/shaders/triangle_spirv.h"

#include <android/log.h>

namespace ae::platform::android {

namespace {
constexpr const char *LogTag = "Aether.Android";

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

TriangleRenderer::~TriangleRenderer() {
  shutdown();
}

bool TriangleRenderer::createRenderPass() {
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

  // A dependência implícita (VK_SUBPASS_EXTERNAL) é necessária porque o
  // layout transition UNDEFINED→COLOR_ATTACHMENT_OPTIMAL precisa esperar o
  // estágio de output de cor — sem isso, drivers mobile (tile-based, onde
  // isso importa mais que em desktop) podem começar a escrever antes da
  // imagem estar disponível vinda do acquire.
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

bool TriangleRenderer::createPipeline() {
  VkShaderModule vertModule = createShaderModule(device_, rhi::shaders::kTriangleVertSpirv,
                                                 rhi::shaders::kTriangleVertSpirvSize);
  VkShaderModule fragModule = createShaderModule(device_, rhi::shaders::kTriangleFragSpirv,
                                                 rhi::shaders::kTriangleFragSpirvSize);
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

  // Sem vertex buffer: o shader gera posição/cor a partir de gl_VertexIndex
  // (ver rhi/shaders/triangle.vert) — deliberado para este vertical slice,
  // não uma omissão.
  VkPipelineVertexInputStateCreateInfo vertexInput{};
  vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  // Viewport/scissor dinâmicos: evita recriar o pipeline inteiro a cada
  // resize/rotação de tela — só os framebuffers e a swapchain precisam ser
  // recriados (ver drawFrame/recreateSwapchain).
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

bool TriangleRenderer::createFramebuffers() {
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

void TriangleRenderer::destroyFramebuffers() {
  for (u32 i = 0; i < framebufferCount_; ++i) {
    if (framebuffers_[i] != VK_NULL_HANDLE) {
      vkDestroyFramebuffer(device_, framebuffers_[i], nullptr);
      framebuffers_[i] = VK_NULL_HANDLE;
    }
  }
  framebufferCount_ = 0;
}

bool TriangleRenderer::initialize(rhi::VulkanDevice &device, rhi::VulkanSwapchain &swapchain) {
  device_ = device.handle();
  swapchain_ = &swapchain;
  graphicsQueueFamily_ = device.graphicsQueueFamily();
  vkGetDeviceQueue(device_, graphicsQueueFamily_, 0, &graphicsQueue_);

  if (!createRenderPass()) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar o render pass do triângulo.");
    return false;
  }
  if (!createPipeline()) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar o pipeline gráfico do triângulo.");
    return false;
  }
  if (!createFramebuffers()) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar os framebuffers do triângulo.");
    return false;
  }

  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = graphicsQueueFamily_;
  if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_) != VK_SUCCESS) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar o command pool.");
    return false;
  }

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = commandPool_;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;
  if (vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer_) != VK_SUCCESS) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao alocar o command buffer.");
    return false;
  }

  __android_log_print(ANDROID_LOG_INFO, LogTag, "Pipeline do triângulo pronto.");
  return true;
}

void TriangleRenderer::shutdown() {
  if (device_ == VK_NULL_HANDLE) return;
  vkDeviceWaitIdle(device_);

  if (commandPool_ != VK_NULL_HANDLE) {
    vkDestroyCommandPool(device_, commandPool_, nullptr); // libera commandBuffer_ junto
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
  device_ = VK_NULL_HANDLE;
  swapchain_ = nullptr;
}

rhi::SwapchainStatus TriangleRenderer::drawFrame() {
  u32 imageIndex = 0;
  const rhi::SwapchainStatus acquireStatus = swapchain_->acquireNextImage(&imageIndex);
  if (acquireStatus != rhi::SwapchainStatus::Ok &&
      acquireStatus != rhi::SwapchainStatus::SuboptimalNeedsRecreate) {
    return acquireStatus;
  }
  // Suboptimal: ainda desenhamos este frame (a imagem é utilizável), o
  // caller recria depois — mesmo contrato documentado em ISwapchain.

  if (vkResetCommandBuffer(commandBuffer_, 0) != VK_SUCCESS) {
    return rhi::SwapchainStatus::FatalError;
  }
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

  vkCmdDraw(commandBuffer_, 3, 1, 0, 0);

  vkCmdEndRenderPass(commandBuffer_);
  if (vkEndCommandBuffer(commandBuffer_) != VK_SUCCESS) {
    return rhi::SwapchainStatus::FatalError;
  }

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
