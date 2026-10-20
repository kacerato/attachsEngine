// Passe de partículas do InstancedRenderer (G5 P1). Incluído por
// instanced_renderer.cpp, como os outros instanced_*.inl.
//
// Fica num render pass PRÓPRIO, logo depois do passe da cena: carrega a cor e a
// profundidade que a cena deixou, testa contra a profundidade sem escrever e a
// AMOSTRA no mesmo passe para a partícula suave (anexo e textura em layout
// somente leitura). Não muda a topologia do passe principal: o bloco G1 pode
// mexer nele sem conhecer partículas, e o P5 troca este passe por um em meia
// resolução com composição sem tocar no resto.
//
// Limites declarados: partículas desenham depois de toda a transparência da
// malha (vidro na frente de fogo não cobre o fogo) e não escrevem as máscaras
// de reatividade do TAA.

struct InstancedRenderer::ParticleResources {
  rhi::VulkanImage atlas;
  rhi::VulkanSampler atlasSampler, depthSampler;
  VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
  VkDescriptorPool pool = VK_NULL_HANDLE;
  VkDescriptorSet set = VK_NULL_HANDLE;
  VkPipelineLayout layout = VK_NULL_HANDLE;
  VkPipeline pipelines[2]{};  // mistura, aditivo
  VkRenderPass renderPass = VK_NULL_HANDLE;
  VkFramebuffer framebuffers[kMaxFramebuffers]{};
  u32 framebufferCount = 0;
  rhi::VulkanBuffer instances;
  u64 instanceCapacity = 0;
  VkQueryPool timestamps = VK_NULL_HANDLE;
  float timestampPeriod = 0;
  bool timestampPending = false;
  bool softAvailable = false;
  renderer::particles::ParticleFrame frame;
  renderer::particles::ParticleRenderStats stats;
};

void InstancedRenderer::queueParticles(renderer::particles::ParticleFrame &frame) {
  if (!particles_) return;
  std::swap(particles_->frame, frame);
  particles_->stats.systems = particles_->frame.systems;
  particles_->stats.alive = particles_->frame.alive;
  particles_->stats.budget = particles_->frame.budget;
  particles_->stats.simulationMs = particles_->frame.simulationMs;
  particles_->stats.buildMs = particles_->frame.buildMs;
}

const renderer::particles::ParticleRenderStats &InstancedRenderer::particleStats() const {
  static const renderer::particles::ParticleRenderStats empty{};
  return particles_ ? particles_->stats : empty;
}

bool InstancedRenderer::particlesBlockSceneReuse() const {
  return particles_ && !particles_->frame.batches.empty();
}

bool InstancedRenderer::createParticleResources() {
  if (!dirtRoadPreview_) return true;
  // Sem profundidade armazenada o passe não teria contra o que testar: a cena
  // segue sem partículas e o motivo vai para o log (capacidade DeviceLimited).
  if (!frameAttachmentPolicy_.depthStored) {
    __android_log_print(ANDROID_LOG_WARN, LogTag, "[Particles] profundidade da cena não é armazenada; partículas desligadas.");
    return true;
  }
  auto resources = std::make_unique<ParticleResources>();
  auto &p = *resources;
  p.softAvailable = frameAttachmentPolicy_.depthSampled;

  // Atlas próprio da engine (renderer/particles/particle_textures.cpp).
  const auto atlas = renderer::particles::generateParticleTextures();
  rhi::ImageDesc image{};
  image.width = image.height = atlas.size;
  image.mipLevels = atlas.levels;
  image.format = VK_FORMAT_R8G8B8A8_UNORM;
  image.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  image.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  image.memoryClass = rhi::MemoryClass::Texture;
  if (!memoryAllocator_->createImage(image, &p.atlas) ||
      !uploadContext_.uploadSampledMipChain(*memoryAllocator_, atlas.texels.data(), atlas.texels.size(), p.atlas)) return false;
  rhi::SamplerDesc sampler{};
  sampler.addressU = sampler.addressV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler.maxLod = static_cast<float>(atlas.levels - 1);
  if (!p.atlasSampler.initialize(device_, sampler)) return false;
  rhi::SamplerDesc depth{};
  depth.minFilter = depth.magFilter = VK_FILTER_NEAREST;
  depth.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  depth.addressU = depth.addressV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  if (!p.depthSampler.initialize(device_, depth)) return false;

  VkDescriptorSetLayoutBinding bindings[2]{};
  bindings[0] = {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
  bindings[1] = {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
  VkDescriptorSetLayoutCreateInfo setInfo{};
  setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  setInfo.bindingCount = 2;
  setInfo.pBindings = bindings;
  if (vkCreateDescriptorSetLayout(device_, &setInfo, nullptr, &p.setLayout) != VK_SUCCESS) return false;
  VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2};
  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.maxSets = 1;
  poolInfo.poolSizeCount = 1;
  poolInfo.pPoolSizes = &size;
  if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &p.pool) != VK_SUCCESS) return false;
  VkDescriptorSetAllocateInfo allocate{};
  allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocate.descriptorPool = p.pool;
  allocate.descriptorSetCount = 1;
  allocate.pSetLayouts = &p.setLayout;
  if (vkAllocateDescriptorSets(device_, &allocate, &p.set) != VK_SUCCESS) return false;
  // Com profundidade amostrável, o mesmo depth do passe (layout somente leitura).
  // Sem ela, o atlas ocupa o lugar só para o descritor ficar válido; a flag de
  // partícula suave nunca liga.
  const VkImageLayout depthLayout = p.softAvailable ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                                    : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  VkDescriptorImageInfo images[2]{
    {p.atlasSampler.handle(), p.atlas.view(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
    {p.depthSampler.handle(), p.softAvailable ? depthImage_.view() : p.atlas.view(), depthLayout}};
  VkWriteDescriptorSet writes[2]{};
  for (u32 i = 0; i < 2; ++i) {
    writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[i].dstSet = p.set;
    writes[i].dstBinding = i;
    writes[i].descriptorCount = 1;
    writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[i].pImageInfo = &images[i];
  }
  vkUpdateDescriptorSets(device_, 2, writes, 0, nullptr);

  // Render pass: carrega cor e profundidade nos layouts finais do passe da cena.
  const VkImageLayout colorLayout = renderingPolicy_.post.dedicatedPass ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                                                        : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  const VkImageLayout depthFinal = frameAttachmentPolicy_.depthSampled ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                                                       : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  VkAttachmentDescription attachments[2]{};
  attachments[0].format = sceneColorFormat_;
  attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
  attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[0].initialLayout = attachments[0].finalLayout = colorLayout;
  attachments[1].format = depthFormat_;
  attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
  attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  // STORE preserva a profundidade para pós, HZB e TAA que a leem depois.
  attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[1].initialLayout = attachments[1].finalLayout = depthFinal;
  VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  VkAttachmentReference depthRef{1, depthFinal};
  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorRef;
  subpass.pDepthStencilAttachment = &depthRef;
  VkSubpassDependency dependencies[2]{};
  dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[0].dstSubpass = 0;
  dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  dependencies[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                 VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
  dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
  dependencies[1].srcSubpass = 0;
  dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
  VkRenderPassCreateInfo passInfo{};
  passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  passInfo.attachmentCount = 2;
  passInfo.pAttachments = attachments;
  passInfo.subpassCount = 1;
  passInfo.pSubpasses = &subpass;
  passInfo.dependencyCount = 2;
  passInfo.pDependencies = dependencies;
  if (vkCreateRenderPass(device_, &passInfo, nullptr, &p.renderPass) != VK_SUCCESS) return false;
  p.framebufferCount = framebufferCount_;
  for (u32 i = 0; i < framebufferCount_; ++i) {
    const VkImageView views[2]{renderingPolicy_.post.dedicatedPass ? postSceneColor_.view() : swapchain_->imageView(i), depthImage_.view()};
    VkFramebufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.renderPass = p.renderPass;
    info.attachmentCount = 2;
    info.pAttachments = views;
    info.width = renderTargetWidth();
    info.height = renderTargetHeight();
    info.layers = 1;
    if (vkCreateFramebuffer(device_, &info, nullptr, &p.framebuffers[i]) != VK_SUCCESS) return false;
  }

  // Pipelines: um por família de mistura, mesmo vértice e mesmo fragmento.
  VkShaderModule vert = createShaderModule(device_, rhi::shaders::kParticle_BillboardVertSpirv,
                                           rhi::shaders::kParticle_BillboardVertSpirvSize);
  VkShaderModule frag = createShaderModule(device_, rhi::shaders::kParticle_MaterialFragSpirv,
                                           rhi::shaders::kParticle_MaterialFragSpirvSize);
  const auto releaseModules = [&] {
    if (vert != VK_NULL_HANDLE) vkDestroyShaderModule(device_, vert, nullptr);
    if (frag != VK_NULL_HANDLE) vkDestroyShaderModule(device_, frag, nullptr);
  };
  if (vert == VK_NULL_HANDLE || frag == VK_NULL_HANDLE) { releaseModules(); return false; }
  const VkDescriptorSetLayout setLayouts[2]{environmentSetLayout_, p.setLayout};
  VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(renderer::particles::ParticlePushConstants)};
  VkPipelineLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layoutInfo.setLayoutCount = 2;
  layoutInfo.pSetLayouts = setLayouts;
  layoutInfo.pushConstantRangeCount = 1;
  layoutInfo.pPushConstantRanges = &push;
  if (vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &p.layout) != VK_SUCCESS) { releaseModules(); return false; }
  VkPipelineShaderStageCreateInfo stages[2]{};
  stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vert, "main", nullptr};
  stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, frag, "main", nullptr};
  VkVertexInputBindingDescription binding{0, sizeof(renderer::particles::ParticleInstance), VK_VERTEX_INPUT_RATE_INSTANCE};
  VkVertexInputAttributeDescription attributes[4]{
    {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0},
    {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 16},
    {2, 0, VK_FORMAT_R16G16B16A16_SFLOAT, 32},
    {3, 0, VK_FORMAT_R32_UINT, 40}};
  VkPipelineVertexInputStateCreateInfo vertex{};
  vertex.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex.vertexBindingDescriptionCount = 1;
  vertex.pVertexBindingDescriptions = &binding;
  vertex.vertexAttributeDescriptionCount = 4;
  vertex.pVertexAttributeDescriptions = attributes;
  VkPipelineInputAssemblyStateCreateInfo assembly{};
  assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  VkPipelineViewportStateCreateInfo viewport{};
  viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport.viewportCount = viewport.scissorCount = 1;
  VkPipelineRasterizationStateCreateInfo raster{};
  raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  raster.polygonMode = VK_POLYGON_MODE_FILL;
  raster.cullMode = VK_CULL_MODE_NONE;
  raster.lineWidth = 1;
  VkPipelineMultisampleStateCreateInfo multisample{};
  multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  VkPipelineDepthStencilStateCreateInfo depthState{};
  depthState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthState.depthTestEnable = VK_TRUE;
  depthState.depthWriteEnable = VK_FALSE;
  depthState.depthCompareOp = VK_COMPARE_OP_LESS;
  VkDynamicState states[2]{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic{};
  dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic.dynamicStateCount = 2;
  dynamic.pDynamicStates = states;
  bool ok = true;
  for (u32 family = 0; family < 2 && ok; ++family) {
    VkPipelineColorBlendAttachmentState blend{};
    blend.blendEnable = VK_TRUE;
    blend.colorBlendOp = blend.alphaBlendOp = VK_BLEND_OP_ADD;
    if (family == 0) {
      blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
      blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
      blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    } else {
      blend.srcColorBlendFactor = blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
      blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
      blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    }
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blendState{};
    blendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blendState.attachmentCount = 1;
    blendState.pAttachments = &blend;
    VkGraphicsPipelineCreateInfo pipeline{};
    pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline.stageCount = 2;
    pipeline.pStages = stages;
    pipeline.pVertexInputState = &vertex;
    pipeline.pInputAssemblyState = &assembly;
    pipeline.pViewportState = &viewport;
    pipeline.pRasterizationState = &raster;
    pipeline.pMultisampleState = &multisample;
    pipeline.pDepthStencilState = &depthState;
    pipeline.pColorBlendState = &blendState;
    pipeline.pDynamicState = &dynamic;
    pipeline.layout = p.layout;
    pipeline.renderPass = p.renderPass;
    ok = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline, nullptr, &p.pipelines[family]) == VK_SUCCESS;
  }
  releaseModules();
  if (!ok) return false;

  // Medição própria do passe: dois timestamps, lidos no quadro seguinte.
  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(physicalDevice_, &properties);
  p.timestampPeriod = properties.limits.timestampPeriod;
  if (properties.limits.timestampComputeAndGraphics && p.timestampPeriod > 0) {
    VkQueryPoolCreateInfo query{};
    query.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    query.queryType = VK_QUERY_TYPE_TIMESTAMP;
    query.queryCount = 2;
    if (vkCreateQueryPool(device_, &query, nullptr, &p.timestamps) != VK_SUCCESS) p.timestamps = VK_NULL_HANDLE;
  }
  p.stats.softAvailable = p.softAvailable;
  particles_ = resources.release();
  __android_log_print(ANDROID_LOG_INFO, LogTag, "[Particles] passe pronto: atlas %ux%u, suave=%s, timestamps=%s.",
                      atlas.size, atlas.size, p.softAvailable ? "sim" : "não", p.timestamps ? "sim" : "não");
  return true;
}

void InstancedRenderer::destroyParticleResources() {
  if (!particles_) return;
  auto &p = *particles_;
  for (u32 i = 0; i < p.framebufferCount; ++i)
    if (p.framebuffers[i]) vkDestroyFramebuffer(device_, p.framebuffers[i], nullptr);
  for (auto pipeline : p.pipelines) if (pipeline) vkDestroyPipeline(device_, pipeline, nullptr);
  if (p.layout) vkDestroyPipelineLayout(device_, p.layout, nullptr);
  if (p.renderPass) vkDestroyRenderPass(device_, p.renderPass, nullptr);
  if (p.pool) vkDestroyDescriptorPool(device_, p.pool, nullptr);
  if (p.setLayout) vkDestroyDescriptorSetLayout(device_, p.setLayout, nullptr);
  if (p.timestamps) vkDestroyQueryPool(device_, p.timestamps, nullptr);
  p.instances.reset();
  p.atlas.reset();
  delete particles_;
  particles_ = nullptr;
}

void InstancedRenderer::recordParticlePass(u32 imageIndex, float timeSeconds) {
  if (!particles_) return;
  auto &p = *particles_;
  // O quadro anterior já terminou (o buffer de comando único foi reaproveitado):
  // o resultado dos timestamps dele está disponível sem esperar.
  if (p.timestamps && p.timestampPending) {
    u64 ticks[2]{};
    if (vkGetQueryPoolResults(device_, p.timestamps, 0, 2, sizeof(ticks), ticks, sizeof(u64), VK_QUERY_RESULT_64_BIT) == VK_SUCCESS)
      p.stats.gpuMs = static_cast<float>(static_cast<double>(ticks[1] - ticks[0]) * p.timestampPeriod * 1.0e-6);
    p.timestampPending = false;
  }
  const auto &frame = p.frame;
  p.stats.drawn = 0;
  p.stats.batches = 0;
  if (frame.batches.empty() || frame.instances.empty() || imageIndex >= p.framebufferCount) {
    p.stats.gpuMs = 0;
    return;
  }
  const u64 bytes = static_cast<u64>(frame.instances.size()) * sizeof(renderer::particles::ParticleInstance);
  if (bytes > p.instanceCapacity) {
    // Cresce em potência de dois; o quadro anterior terminou, então trocar é seguro.
    u64 capacity = 64 * 1024;
    while (capacity < bytes) capacity *= 2;
    p.instances.reset();
    rhi::BufferDesc desc{};
    desc.sizeBytes = capacity;
    desc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    desc.memoryClass = rhi::MemoryClass::Buffer;
    desc.cpuAccess = rhi::CpuAccess::SequentialWrite;
    desc.preferDeviceMemory = false;
    if (!memoryAllocator_->createBuffer(desc, &p.instances) || !p.instances.mappedData()) {
      p.instanceCapacity = 0;
      return;
    }
    p.instanceCapacity = capacity;
  }
  std::memcpy(p.instances.mappedData(), frame.instances.data(), static_cast<usize>(bytes));
  if (!memoryAllocator_->flushBuffer(p.instances)) return;

  if (p.timestamps) {
    vkCmdResetQueryPool(commandBuffer_, p.timestamps, 0, 2);
    vkCmdWriteTimestamp(commandBuffer_, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, p.timestamps, 0);
  }
  rhiDevice_->cmdBeginDebugLabel(commandBuffer_, "Particles", 0.98f, 0.55f, 0.18f);
  VkRenderPassBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  begin.renderPass = p.renderPass;
  begin.framebuffer = p.framebuffers[imageIndex];
  begin.renderArea.extent = {renderTargetWidth(), renderTargetHeight()};
  vkCmdBeginRenderPass(commandBuffer_, &begin, VK_SUBPASS_CONTENTS_INLINE);
  // A mesma vista física do passe da cena (resolução dinâmica e recorte do editor).
  VkViewport viewport{};
  const auto physicalViewport = physicalSceneViewport();
  viewport.x = physicalViewport.x * renderWidth();
  viewport.y = physicalViewport.y * renderHeight();
  viewport.width = physicalViewport.width * renderWidth();
  viewport.height = physicalViewport.height * renderHeight();
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer_, 0, 1, &viewport);
  VkRect2D scissor{};
  scissor.offset = {static_cast<i32>(std::max(0.0f, std::floor(viewport.x))), static_cast<i32>(std::max(0.0f, std::floor(viewport.y)))};
  scissor.extent = {std::min(renderWidth() - static_cast<u32>(scissor.offset.x), static_cast<u32>(std::ceil(viewport.width))),
                    std::min(renderHeight() - static_cast<u32>(scissor.offset.y), static_cast<u32>(std::ceil(viewport.height)))};
  vkCmdSetScissor(commandBuffer_, 0, 1, &scissor);
  const VkDescriptorSet sets[2]{environmentSet_, p.set};
  vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, p.layout, 0, 2, sets, 0, nullptr);
  const VkBuffer buffer = p.instances.handle();
  const VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(commandBuffer_, 0, 1, &buffer, &offset);
  VkPipeline bound = VK_NULL_HANDLE;
  for (const auto &batch : frame.batches) {
    if (!batch.instanceCount) continue;
    const VkPipeline pipeline = p.pipelines[batch.material.blend == 1 ? 1 : 0];
    if (pipeline != bound) {
      vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
      bound = pipeline;
    }
    const auto push = renderer::particles::particlePushConstants(batch.material, timeSeconds,
        static_cast<float>(renderTargetWidth()), static_cast<float>(renderTargetHeight()), p.softAvailable);
    vkCmdPushConstants(commandBuffer_, p.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
    vkCmdDraw(commandBuffer_, 6, batch.instanceCount, 0, batch.firstInstance);
    p.stats.drawn += batch.instanceCount;
    ++p.stats.batches;
  }
  vkCmdEndRenderPass(commandBuffer_);
  rhiDevice_->cmdEndDebugLabel(commandBuffer_);
  if (p.timestamps) {
    vkCmdWriteTimestamp(commandBuffer_, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, p.timestamps, 1);
    p.timestampPending = true;
  }
}
