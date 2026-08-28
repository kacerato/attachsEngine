#include "rhi/pipeline_cache.h"

namespace ae::rhi {

struct PipelineCache::Impl {
  DescriptorCache<RenderPassCacheDesc, VkRenderPass> renderPasses;
  DescriptorCache<PipelineLayoutCacheDesc, VkPipelineLayout> pipelineLayouts;
  DescriptorCache<GraphicsPipelineCacheDesc, VkPipeline> pipelines;

  Impl(VkDevice device)
      : renderPasses([device](const RenderPassCacheDesc &desc) -> VkRenderPass {
          return createRenderPassReal(device, desc);
        }),
        pipelineLayouts([device](const PipelineLayoutCacheDesc &desc) -> VkPipelineLayout {
          return createPipelineLayoutReal(device, desc);
        }),
        // A factory de pipeline recebe o VkGraphicsPipelineCreateInfo completo via
        // um ponteiro guardado fora da chave (ver getOrCreateGraphicsPipeline) —
        // DescriptorCache::Factory só recebe a Desc, então o createInfo do pedido
        // atual é passado por variável capturada, válida só durante a chamada.
        pipelines([this, device](const GraphicsPipelineCacheDesc &) -> VkPipeline {
          return createGraphicsPipelineReal(device, *pendingCreateInfo);
        }) {}

  const VkGraphicsPipelineCreateInfo *pendingCreateInfo = nullptr;

  static VkRenderPass createRenderPassReal(VkDevice device, const RenderPassCacheDesc &desc) {
    const bool hasDepth = desc.depthFormat != VK_FORMAT_UNDEFINED;

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = desc.colorFormat;
    colorAttachment.samples = desc.sampleCount;
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
    VkAttachmentReference depthRef{};
    if (hasDepth) {
      depthAttachment.format = desc.depthFormat;
      depthAttachment.samples = desc.sampleCount;
      depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      depthRef.attachment = 1;
      depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    if (hasDepth) subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    if (hasDepth) {
      dependency.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
      dependency.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
      dependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }

    const VkAttachmentDescription attachments[] = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = hasDepth ? 2 : 1;
    info.pAttachments = attachments;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dependency;

    VkRenderPass renderPass = VK_NULL_HANDLE;
    if (vkCreateRenderPass(device, &info, nullptr, &renderPass) != VK_SUCCESS) return VK_NULL_HANDLE;
    return renderPass;
  }

  static VkPipelineLayout createPipelineLayoutReal(VkDevice device,
                                                    const PipelineLayoutCacheDesc &desc) {
    VkPipelineLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    if (desc.setLayout != VK_NULL_HANDLE) {
      info.setLayoutCount = 1;
      info.pSetLayouts = &desc.setLayout;
    }
    VkPushConstantRange pushRange{};
    if (desc.pushConstantSize > 0) {
      pushRange.stageFlags = desc.pushConstantStageFlags;
      pushRange.size = desc.pushConstantSize;
      info.pushConstantRangeCount = 1;
      info.pPushConstantRanges = &pushRange;
    }
    VkPipelineLayout layout = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(device, &info, nullptr, &layout) != VK_SUCCESS) return VK_NULL_HANDLE;
    return layout;
  }

  static VkPipeline createGraphicsPipelineReal(VkDevice device,
                                               const VkGraphicsPipelineCreateInfo &createInfo) {
    VkPipeline pipeline = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline) !=
        VK_SUCCESS) {
      return VK_NULL_HANDLE;
    }
    return pipeline;
  }
};

PipelineCache::~PipelineCache() {
  shutdown();
}

void PipelineCache::initialize(VkDevice device) {
  device_ = device;
  impl_ = new Impl(device);
}

void PipelineCache::shutdown() {
  if (impl_ == nullptr) return;
  // DescriptorCache<Desc,Handle> é agnóstico ao tipo de Handle e não destrói
  // os objetos Vulkan sozinho — precisa ser feito aqui, na ordem correta
  // (pipeline antes de layout/render pass, já que pipelines os referenciam).
  impl_->pipelines.forEachHandle(
      [this](VkPipeline pipeline) { vkDestroyPipeline(device_, pipeline, nullptr); });
  impl_->pipelineLayouts.forEachHandle(
      [this](VkPipelineLayout layout) { vkDestroyPipelineLayout(device_, layout, nullptr); });
  impl_->renderPasses.forEachHandle(
      [this](VkRenderPass renderPass) { vkDestroyRenderPass(device_, renderPass, nullptr); });
  delete impl_;
  impl_ = nullptr;
  device_ = VK_NULL_HANDLE;
}

VkRenderPass PipelineCache::getOrCreateRenderPass(const RenderPassCacheDesc &desc) {
  if (impl_ == nullptr) return VK_NULL_HANDLE;
  return impl_->renderPasses.getOrCreate(desc);
}

VkPipelineLayout PipelineCache::getOrCreatePipelineLayout(const PipelineLayoutCacheDesc &desc) {
  if (impl_ == nullptr) return VK_NULL_HANDLE;
  return impl_->pipelineLayouts.getOrCreate(desc);
}

VkPipeline PipelineCache::getOrCreateGraphicsPipeline(
    const GraphicsPipelineCacheDesc &desc, const VkGraphicsPipelineCreateInfo &createInfoTemplate) {
  if (impl_ == nullptr) return VK_NULL_HANDLE;
  impl_->pendingCreateInfo = &createInfoTemplate;
  VkPipeline pipeline = impl_->pipelines.getOrCreate(desc);
  impl_->pendingCreateInfo = nullptr;
  return pipeline;
}

usize PipelineCache::renderPassCount() const {
  return impl_ == nullptr ? 0 : impl_->renderPasses.size();
}

usize PipelineCache::pipelineLayoutCount() const {
  return impl_ == nullptr ? 0 : impl_->pipelineLayouts.size();
}

usize PipelineCache::pipelineCount() const {
  return impl_ == nullptr ? 0 : impl_->pipelines.size();
}

} // namespace ae::rhi
