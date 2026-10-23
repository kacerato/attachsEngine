// G4d: Volumetric Clouds (HDRP 17). O raymarch (rhi/shaders/cloud_volume.frag)
// roda num alvo de meia resolução em cada eixo e marcha 1 pixel de cada bloco
// 2x2 por quadro — 1/16 dos pixels da tela, como o modo de desempenho do HDRP —
// antes do passe da cena. O próprio
// fragmento mistura a amostra nova com o histórico reprojetado; o resultado é
// copiado para o histórico e o céu o lê (binding 19) já integrado.
//
// Imagens fixas, descritores fixos: alvo (19) e histórico (20) nunca trocam de
// papel, então o conjunto de ambiente é escrito uma vez. Há um quadro em voo
// (ver instanced_color_grading.inl), o que torna a cópia segura.

bool InstancedRenderer::createVolumetricCloudResources() {
  if (cloudVolumeTarget_.isReady()) return true;
  const u32 width = std::max(1u, renderWidth() / 2u), height = std::max(1u, renderHeight() / 2u);
  rhi::ImageDesc target{};
  target.width = width; target.height = height;
  target.format = VK_FORMAT_R16G16B16A16_SFLOAT;
  target.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  target.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  target.memoryClass = rhi::MemoryClass::RenderTarget;
  if (!memoryAllocator_->createImage(target, &cloudVolumeTarget_)) return false;
  rhi::ImageDesc history = target;
  history.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  history.memoryClass = rhi::MemoryClass::Texture;
  if (!memoryAllocator_->createImage(history, &cloudVolumeHistory_)) return false;

  // Ruídos 3D de forma e erosão (renderer/cloud_noise.cpp), sobem no 1º quadro.
  std::vector<u8> shape, erosion;
  std::vector<u8> shapeLevel0, erosionLevel0;
  if (!renderer::buildCloudShapeNoise(renderer::CloudShapeNoiseSize, 7u, shapeLevel0) ||
      !renderer::buildCloudErosionNoise(renderer::CloudErosionNoiseSize, 13u, erosionLevel0) ||
      !renderer::buildCloudNoiseMips(shapeLevel0, renderer::CloudShapeNoiseSize, shape) ||
      !renderer::buildCloudNoiseMips(erosionLevel0, renderer::CloudErosionNoiseSize, erosion)) return false;
  const auto noiseImage = [&](u32 size, rhi::VulkanImage &image, rhi::VulkanBuffer &staging,
                              const std::vector<u8> &texels) {
    rhi::ImageDesc desc{};
    desc.width = desc.height = desc.depth = size;
    desc.mipLevels = renderer::cloudNoiseMipCount(size);
    desc.format = VK_FORMAT_R8G8B8A8_UNORM;
    desc.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    desc.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    desc.memoryClass = rhi::MemoryClass::Texture;
    rhi::BufferDesc buffer{};
    buffer.sizeBytes = texels.size();
    buffer.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buffer.cpuAccess = rhi::CpuAccess::SequentialWrite;
    buffer.preferDeviceMemory = false;
    if (!memoryAllocator_->createImage(desc, &image) || !memoryAllocator_->createBuffer(buffer, &staging) ||
        !staging.mappedData()) return false;
    std::memcpy(staging.mappedData(), texels.data(), texels.size());
    return memoryAllocator_->flushBuffer(staging);
  };
  if (!noiseImage(renderer::CloudShapeNoiseSize, cloudShapeNoise_, cloudShapeStaging_, shape) ||
      !noiseImage(renderer::CloudErosionNoiseSize, cloudErosionNoise_, cloudErosionStaging_, erosion)) return false;
  cloudNoiseUploaded_ = false;

  rhi::SamplerDesc clamp{};
  clamp.minFilter = clamp.magFilter = VK_FILTER_LINEAR;
  clamp.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  clamp.addressU = clamp.addressV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  if (!cloudVolumeSampler_.initialize(device_, clamp)) return false;
  rhi::SamplerDesc repeat = clamp;
  repeat.addressU = repeat.addressV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  repeat.addressW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  repeat.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  repeat.maxLod = static_cast<float>(renderer::cloudNoiseMipCount(renderer::CloudShapeNoiseSize) - 1);
  if (!cloudNoiseSampler_.initialize(device_, repeat)) return false;

  VkAttachmentDescription color{};
  color.format = VK_FORMAT_R16G16B16A16_SFLOAT;
  color.samples = VK_SAMPLE_COUNT_1_BIT;
  color.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  VkAttachmentReference reference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1; subpass.pColorAttachments = &reference;
  VkSubpassDependency dependencies[2]{};
  dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL; dependencies[0].dstSubpass = 0;
  dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
  dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
  dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[1].srcSubpass = 0; dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[1].dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
  dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  VkRenderPassCreateInfo pass{};
  pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  pass.attachmentCount = 1; pass.pAttachments = &color;
  pass.subpassCount = 1; pass.pSubpasses = &subpass;
  pass.dependencyCount = 2; pass.pDependencies = dependencies;
  if (vkCreateRenderPass(device_, &pass, nullptr, &cloudVolumeRenderPass_) != VK_SUCCESS) return false;
  const VkImageView view = cloudVolumeTarget_.view();
  VkFramebufferCreateInfo framebuffer{};
  framebuffer.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebuffer.renderPass = cloudVolumeRenderPass_;
  framebuffer.attachmentCount = 1; framebuffer.pAttachments = &view;
  framebuffer.width = width; framebuffer.height = height; framebuffer.layers = 1;
  if (vkCreateFramebuffer(device_, &framebuffer, nullptr, &cloudVolumeFramebuffer_) != VK_SUCCESS) return false;
  __android_log_print(ANDROID_LOG_INFO, LogTag, "[Clouds] volumétricas: alvo %ux%u (meia resolução, 2x2 por quadro), ruído %u³ + %u³ com mips.",
                      width, height, renderer::CloudShapeNoiseSize, renderer::CloudErosionNoiseSize);
  return true;
}

bool InstancedRenderer::createVolumetricCloudPipeline() {
  if (!dirtRoadPreview_ || cloudVolumeRenderPass_ == VK_NULL_HANDLE || skyPipelineLayout_ == VK_NULL_HANDLE) return true;
  VkShaderModule vert = createShaderModule(device_, rhi::shaders::kDirt_Road_SkyVertSpirv,
                                           rhi::shaders::kDirt_Road_SkyVertSpirvSize);
  VkShaderModule frag = createShaderModule(device_, rhi::shaders::kCloud_VolumeFragSpirv,
                                           rhi::shaders::kCloud_VolumeFragSpirvSize);
  if (vert == VK_NULL_HANDLE || frag == VK_NULL_HANDLE) {
    if (vert != VK_NULL_HANDLE) vkDestroyShaderModule(device_, vert, nullptr);
    if (frag != VK_NULL_HANDLE) vkDestroyShaderModule(device_, frag, nullptr);
    return false;
  }
  VkPipelineShaderStageCreateInfo stages[2]{};
  stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vert, "main", nullptr};
  stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, frag, "main", nullptr};
  VkPipelineVertexInputStateCreateInfo vertex{};
  vertex.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  VkPipelineInputAssemblyStateCreateInfo assembly{};
  assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  VkPipelineViewportStateCreateInfo viewport{};
  viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport.viewportCount = 1; viewport.scissorCount = 1;
  VkPipelineRasterizationStateCreateInfo raster{};
  raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  raster.polygonMode = VK_POLYGON_MODE_FILL; raster.cullMode = VK_CULL_MODE_NONE; raster.lineWidth = 1;
  VkPipelineMultisampleStateCreateInfo multisample{};
  multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  VkPipelineDepthStencilStateCreateInfo depth{};
  depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  VkPipelineColorBlendAttachmentState attachment{};
  attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  VkPipelineColorBlendStateCreateInfo blend{};
  blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  blend.attachmentCount = 1; blend.pAttachments = &attachment;
  VkDynamicState states[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic{};
  dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic.dynamicStateCount = 2; dynamic.pDynamicStates = states;
  VkGraphicsPipelineCreateInfo pipeline{};
  pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline.stageCount = 2; pipeline.pStages = stages; pipeline.pVertexInputState = &vertex;
  pipeline.pInputAssemblyState = &assembly; pipeline.pViewportState = &viewport;
  pipeline.pRasterizationState = &raster; pipeline.pMultisampleState = &multisample;
  pipeline.pDepthStencilState = &depth; pipeline.pColorBlendState = &blend;
  pipeline.pDynamicState = &dynamic; pipeline.layout = skyPipelineLayout_;
  pipeline.renderPass = cloudVolumeRenderPass_; pipeline.subpass = 0;
  const bool ok = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline, nullptr, &cloudVolumePipeline_) == VK_SUCCESS;
  vkDestroyShaderModule(device_, vert, nullptr);
  vkDestroyShaderModule(device_, frag, nullptr);
  return ok;
}

void InstancedRenderer::destroyVolumetricCloudResources() {
  if (cloudVolumePipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, cloudVolumePipeline_, nullptr);
  if (cloudVolumeFramebuffer_ != VK_NULL_HANDLE) vkDestroyFramebuffer(device_, cloudVolumeFramebuffer_, nullptr);
  if (cloudVolumeRenderPass_ != VK_NULL_HANDLE) vkDestroyRenderPass(device_, cloudVolumeRenderPass_, nullptr);
  cloudVolumePipeline_ = VK_NULL_HANDLE; cloudVolumeFramebuffer_ = VK_NULL_HANDLE; cloudVolumeRenderPass_ = VK_NULL_HANDLE;
  cloudVolumeSampler_.shutdown(); cloudNoiseSampler_.shutdown();
  cloudVolumeTarget_.reset(); cloudVolumeHistory_.reset();
  cloudShapeNoise_.reset(); cloudErosionNoise_.reset();
  cloudShapeStaging_.reset(); cloudErosionStaging_.reset();
  cloudNoiseUploaded_ = false; cloudHistoryValid_ = false; cloudImagesInitialized_ = false;
}

bool InstancedRenderer::volumetricCloudsReady() const {
  return cloudVolumePipeline_ != VK_NULL_HANDLE && cloudVolumeTarget_.isReady();
}

void InstancedRenderer::recordVolumetricClouds(const platform::FreeCameraState &camera, float timeSeconds) {
  const auto *frame = static_cast<const DirtRoadFrameUniform *>(environmentUniform_.mappedData());
  if (!frame || !volumetricCloudsReady() || frame->cloudShadow[2] < 0.5f || frame->cloudParameters[0] <= 0.0f) {
    cloudHistoryValid_ = false;
    return;
  }
  const auto imageBarrier = [&](VkImage image, VkImageLayout from, VkImageLayout to, VkAccessFlags srcAccess,
                                VkAccessFlags dstAccess, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = srcAccess; barrier.dstAccessMask = dstAccess;
    barrier.oldLayout = from; barrier.newLayout = to;
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(commandBuffer_, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
  };
  if (!cloudNoiseUploaded_) {
    const auto upload = [&](rhi::VulkanImage &image, rhi::VulkanBuffer &staging, u32 size) {
      const u32 levels = renderer::cloudNoiseMipCount(size);
      VkImageMemoryBarrier barrier{};
      barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.image = image.handle();
      barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, levels, 0, 1};
      vkCmdPipelineBarrier(commandBuffer_, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           0, 0, nullptr, 0, nullptr, 1, &barrier);
      std::vector<VkBufferImageCopy> copies(levels);
      VkDeviceSize offset = 0;
      for (u32 level = 0, n = size; level < levels; ++level, n = std::max(1u, n / 2)) {
        copies[level].bufferOffset = offset;
        copies[level].imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level, 0, 1};
        copies[level].imageExtent = {n, n, n};
        offset += static_cast<VkDeviceSize>(n) * n * n * 4;
      }
      vkCmdCopyBufferToImage(commandBuffer_, staging.handle(), image.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             levels, copies.data());
      barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      vkCmdPipelineBarrier(commandBuffer_, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                           0, 0, nullptr, 0, nullptr, 1, &barrier);
    };
    upload(cloudShapeNoise_, cloudShapeStaging_, renderer::CloudShapeNoiseSize);
    upload(cloudErosionNoise_, cloudErosionStaging_, renderer::CloudErosionNoiseSize);
    cloudNoiseUploaded_ = true;
  }
  if (!cloudImagesInitialized_) {
    // O histórico começa limpo e amostrável; o shader só o lê com o bit de
    // histórico válido, que vem do quadro seguinte.
    imageBarrier(cloudVolumeHistory_.handle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
                 VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    VkClearColorValue clear{};
    VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdClearColorImage(commandBuffer_, cloudVolumeHistory_.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &range);
    imageBarrier(cloudVolumeHistory_.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                 VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    cloudImagesInitialized_ = true;
  }

  const u32 width = cloudVolumeTarget_.width(), height = cloudVolumeTarget_.height();
  VkRenderPassBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  begin.renderPass = cloudVolumeRenderPass_;
  begin.framebuffer = cloudVolumeFramebuffer_;
  begin.renderArea.extent = {width, height};
  vkCmdBeginRenderPass(commandBuffer_, &begin, VK_SUBPASS_CONTENTS_INLINE);
  const VkViewport viewport{0, 0, static_cast<float>(width), static_cast<float>(height), 0, 1};
  const VkRect2D scissor{{0, 0}, {width, height}};
  vkCmdSetViewport(commandBuffer_, 0, 1, &viewport);
  vkCmdSetScissor(commandBuffer_, 0, 1, &scissor);
  // Mesmo push do céu: o vértice reconstrói a direção de cada pixel igual.
  const rhi::SurfaceTransform &surfaceTransform = swapchain_->surfaceTransform();
  DirtRoadPushConstants push{};
  std::memcpy(&push.textureIndices[3], &sceneOrthographicHalfHeight_, sizeof(float));
  push.cameraFrame[0] = sceneAspectRatio();
  push.materialFactors[3] = 1.0f / std::tan(sceneFieldOfView() * .5f);
  push.materialFactors[0] = camera.roll;
  push.cameraFrame[1] = camera.yaw;
  push.cameraFrame[2] = camera.pitch;
  push.cameraFrame[3] = timeSeconds;
  push.surfaceTransform[0] = surfaceTransform.xx;
  push.surfaceTransform[1] = surfaceTransform.xy;
  push.surfaceTransform[2] = surfaceTransform.yx;
  push.surfaceTransform[3] = surfaceTransform.yy;
  vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, cloudVolumePipeline_);
  vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, skyPipelineLayout_, 0, 1, &environmentSet_, 0, nullptr);
  vkCmdPushConstants(commandBuffer_, skyPipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                     0, sizeof(push), &push);
  vkCmdDraw(commandBuffer_, 3, 1, 0, 0);
  vkCmdEndRenderPass(commandBuffer_);

  // O alvo vira o histórico do próximo quadro e fica amostrável para o céu.
  imageBarrier(cloudVolumeHistory_.handle(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
               VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
               VK_PIPELINE_STAGE_TRANSFER_BIT);
  VkImageCopy copy{};
  copy.srcSubresource = copy.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  copy.extent = {width, height, 1};
  vkCmdCopyImage(commandBuffer_, cloudVolumeTarget_.handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                 cloudVolumeHistory_.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
  imageBarrier(cloudVolumeHistory_.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
               VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
  imageBarrier(cloudVolumeTarget_.handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
               VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
  cloudHistoryValid_ = true;
}
