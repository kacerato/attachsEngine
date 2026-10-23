// Incluído dentro do namespace do renderer, como os demais .inl: mantém o ABI
// do quadro (DirtRoadFrameUniform, push constants) numa unidade de tradução.
//
// PK-6 — Reflection Probes e Light Probes no aparelho.
//
// Atlas: uma imagem 2D array RGBA16F de 8 camadas, octaedro de 256 px com 6
// mips (256..8). Cada camada é uma sonda residente; o fragmento a lê pelo índice
// que o desenho carrega no buffer de sondas (binding 24), junto do SH L2 das
// sondas de luz.
//
// Captura: seis faces num cubemap (convenção do Vulkan, yaw/pitch de
// buildCameraViewBasis), com o render pass da prévia de câmera, os mesmos
// pipelines e o mesmo UBO do quadro com a vista trocada. Mips do cubo por blit,
// depois um compute GGX por mip num octaedro de rascunho, que só é copiado para
// a camada no fim — uma sonda fatiada no tempo nunca aparece pela metade.
//
// Bake: o mesmo caminho sem fatiamento, só objetos estáticos, e uma cópia para
// um buffer de leitura que volta ao editor no quadro seguinte (a cerca do quadro
// é a mesma que acquireNextImage espera). A captura de irradiância de uma sonda
// de luz copia as faces (mip 0) e o SH é projetado na CPU.

namespace {
constexpr u32 ProbeHeaderBytes = 528;
constexpr u32 ProbeDrawBytes = 160;
struct ProbeHeaderGpu {
  u32 header[4];
  float boxMin[8][4];
  float boxMax[8][4];
  float capture[8][4];
  float parameters[8][4];
};
static_assert(sizeof(ProbeHeaderGpu) == ProbeHeaderBytes);
struct ProbeDrawGpu {
  float sh[9][4];
  u32 reflection[4];
};
static_assert(sizeof(ProbeDrawGpu) == ProbeDrawBytes);
struct ProbeFilterPush {
  u32 side, faceSize;
  float roughness, sourceLods, clampValue, intensity, reserved0, reserved1;
};
// Faces do cubemap do Vulkan (+X,-X,+Y,-Y,+Z,-Z) como yaw/pitch da câmera.
constexpr float ProbeFaceYaw[6]{1.57079633f, -1.57079633f, 0.0f, 0.0f, 0.0f, 3.14159265f};
constexpr float ProbeFacePitch[6]{0.0f, 0.0f, -1.57079633f, 1.57079633f, 0.0f, 0.0f};
u64 probeLayerKey(u64 objectId, u64 instance) { return objectId * 1000003ull ^ instance; }
// Tudo o que muda a imagem do céu (sol, cores, atmosfera, astros, nuvens), sem
// o deslocamento do vento: as nuvens andando não refazem o reflexo a cada quadro.
std::array<float, renderer::SkyReflectionKeySize> skyReflectionKey(const DirtRoadFrameUniform &frame) {
  std::array<float, renderer::SkyReflectionKeySize> key{};
  const float *rows[16]{frame.environment.sunDirectionIntensity, frame.environment.sunColorAngularRadius,
                        frame.environment.skyZenithCloudCoverage, frame.environment.skyHorizonCloudDensity,
                        frame.environment.groundColorSaturation, frame.environment.cloudLightWindSpeed,
                        frame.sceneSky, frame.celestialParameters, frame.celestialStars, frame.skyParameters,
                        frame.atmosphereScattering, frame.atmosphereGeometry, frame.atmosphereDetail,
                        frame.cloudParameters, frame.cloudTint, frame.environment.parameters};
  for (u32 row = 0; row < 16; ++row) std::copy_n(rows[row], 4, key.data() + row * 4);
  // LOD e projeção do ambiente: quem decide é o próprio reflexo. No lugar, a
  // exposição do céu (sceneAoDetail.z: Exposure do Skybox ou do HDRI).
  key[62] = frame.sceneAoDetail[2];
  key[63] = 0;
  return key;
}

void probeImageBarrier(VkCommandBuffer command, VkImage image, VkImageLayout from, VkImageLayout to, VkAccessFlags srcAccess,
                  VkAccessFlags dstAccess, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, u32 baseMip,
                  u32 mips, u32 baseLayer, u32 layers) {
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = from;
  barrier.newLayout = to;
  barrier.srcAccessMask = srcAccess;
  barrier.dstAccessMask = dstAccess;
  barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, baseMip, mips, baseLayer, layers};
  vkCmdPipelineBarrier(command, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}
} // namespace

void InstancedRenderer::queueSceneReflectionProbes(std::span<const renderer::SceneReflectionProbe> probes, bool playing) {
  sceneProbes_.assign(probes.begin(), probes.end());
  // Entrar em Play acorda as sondas "Ao acordar" de novo, como o Awake da Unity.
  if (playing && !sceneProbesPlaying_)
    for (auto &layer : probeLayers_) layer.awakeDone = false;
  sceneProbesPlaying_ = playing;
}

void InstancedRenderer::setReflectionProbeTextures(std::vector<std::shared_ptr<const renderer::ReflectionProbeTexture>> textures) {
  probeTextures_ = std::move(textures);
  // Um recurso reassado chega com a mesma identidade e outro conteúdo: as
  // camadas assadas são recarregadas na próxima residência.
  for (auto &layer : probeLayers_)
    if (layer.used && layer.probe.type == renderer::ReflectionProbeType::Baked) layer.contentKey = 0;
}

void InstancedRenderer::setLightProbeSet(std::shared_ptr<const renderer::LightProbeSet> set) {
  lightProbeSet_ = std::move(set);
  ++lightProbeRevision_;
  editorSceneCacheValid_ = false;
}

bool InstancedRenderer::requestProbeCapture(const ProbeCaptureRequest &request) {
  if (!dirtRoadPreview_ || !probeAtlas_.isReady() || probeCaptureQueue_.size() >= 4096) return false;
  probeCaptureQueue_.push_back(request);
  return true;
}

bool InstancedRenderer::takeProbeCaptureResult(ProbeCaptureResult &out) {
  if (probeResults_.empty()) return false;
  out = std::move(probeResults_.front());
  probeResults_.pop_front();
  return true;
}

bool InstancedRenderer::createProbeResources() {
  if (!dirtRoadPreview_ || probeAtlas_.isReady()) return true;
  rhi::ImageDesc atlas{};
  atlas.width = atlas.height = renderer::ReflectionProbeAtlasSize;
  atlas.mipLevels = renderer::ReflectionProbeAtlasMips;
  atlas.arrayLayers = renderer::MaximumFrameReflectionProbes;
  atlas.format = VK_FORMAT_R16G16B16A16_SFLOAT;
  atlas.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  atlas.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  if (!memoryAllocator_->createImage(atlas, &probeAtlas_)) return false;
  rhi::ImageDesc sky = atlas;
  sky.arrayLayers = 1;
  sky.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  if (!memoryAllocator_->createImage(sky, &skyReflection_)) return false;
  skyReflectionReady_ = skyReflectionKeyValid_ = false;
  rhi::SamplerDesc sampler{};
  sampler.addressU = sampler.addressV = sampler.addressW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler.maxLod = static_cast<float>(renderer::ReflectionProbeAtlasMips - 1);
  if (!probeAtlasSampler_.initialize(device_, sampler)) return false;
  // O atlas nasce preto e legível no primeiro quadro (recordProbes): nenhuma
  // camada é usada antes de pronta, mas o descritor exige o layout de leitura.
  probeAtlasInitialized_ = false;
  rhi::BufferDesc disabled{};
  disabled.sizeBytes = ProbeHeaderBytes + ProbeDrawBytes;
  disabled.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  disabled.cpuAccess = rhi::CpuAccess::SequentialWrite;
  disabled.preferDeviceMemory = false;
  if (!memoryAllocator_->createBuffer(disabled, &probeDisabledBlock_)) return false;
  std::memset(probeDisabledBlock_.mappedData(), 0, static_cast<usize>(disabled.sizeBytes));
  if (!memoryAllocator_->flushBuffer(probeDisabledBlock_)) return false;
  if (!ensureProbeDrawCapacity(std::max<u32>(instanceCount_, 1))) return false;

  // Filtro: cubemap amostrado + imagem de saída, um conjunto por mip do octaedro.
  rhi::ImageDesc scratch{};
  scratch.width = scratch.height = renderer::ReflectionProbeAtlasSize;
  scratch.mipLevels = renderer::ReflectionProbeAtlasMips;
  scratch.format = VK_FORMAT_R16G16B16A16_SFLOAT;
  scratch.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  scratch.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  if (!memoryAllocator_->createImage(scratch, &probeScratch_)) return false;
  for (u32 mip = 0; mip < renderer::ReflectionProbeAtlasMips; ++mip) {
    VkImageViewCreateInfo view{};
    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.image = probeScratch_.handle();
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, mip, 1, 0, 1};
    if (vkCreateImageView(device_, &view, nullptr, &probeScratchViews_[mip]) != VK_SUCCESS) return false;
  }
  VkDescriptorSetLayoutBinding bindings[2]{};
  bindings[0] = {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
  bindings[1] = {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
  VkDescriptorSetLayoutCreateInfo setLayout{};
  setLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  setLayout.bindingCount = 2;
  setLayout.pBindings = bindings;
  if (vkCreateDescriptorSetLayout(device_, &setLayout, nullptr, &probeFilterSetLayout_) != VK_SUCCESS) return false;
  VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ProbeFilterPush)};
  VkPipelineLayoutCreateInfo layout{};
  layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layout.setLayoutCount = 1;
  layout.pSetLayouts = &probeFilterSetLayout_;
  layout.pushConstantRangeCount = 1;
  layout.pPushConstantRanges = &push;
  if (vkCreatePipelineLayout(device_, &layout, nullptr, &probeFilterLayout_) != VK_SUCCESS) return false;
  VkShaderModule module = createShaderModule(device_, rhi::shaders::kReflection_Probe_FilterCompSpirv,
                                             rhi::shaders::kReflection_Probe_FilterCompSpirvSize);
  if (!module) return false;
  VkComputePipelineCreateInfo pipeline{};
  pipeline.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipeline.layout = probeFilterLayout_;
  pipeline.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  pipeline.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  pipeline.stage.module = module;
  pipeline.stage.pName = "main";
  const bool created = vkCreateComputePipelines(device_, VK_NULL_HANDLE, 1, &pipeline, nullptr, &probeFilterPipeline_) == VK_SUCCESS;
  vkDestroyShaderModule(device_, module, nullptr);
  if (!created) return false;
  VkDescriptorPoolSize sizes[2]{{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, renderer::ReflectionProbeAtlasMips},
                                {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, renderer::ReflectionProbeAtlasMips}};
  VkDescriptorPoolCreateInfo pool{};
  pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool.maxSets = renderer::ReflectionProbeAtlasMips;
  pool.poolSizeCount = 2;
  pool.pPoolSizes = sizes;
  if (vkCreateDescriptorPool(device_, &pool, nullptr, &probeFilterPool_) != VK_SUCCESS) return false;
  std::array<VkDescriptorSetLayout, renderer::ReflectionProbeAtlasMips> layouts;
  layouts.fill(probeFilterSetLayout_);
  VkDescriptorSetAllocateInfo allocate{};
  allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocate.descriptorPool = probeFilterPool_;
  allocate.descriptorSetCount = renderer::ReflectionProbeAtlasMips;
  allocate.pSetLayouts = layouts.data();
  if (vkAllocateDescriptorSets(device_, &allocate, probeFilterSets_.data()) != VK_SUCCESS) return false;
  for (u32 mip = 0; mip < renderer::ReflectionProbeAtlasMips; ++mip) {
    VkDescriptorImageInfo storage{VK_NULL_HANDLE, probeScratchViews_[mip], VK_IMAGE_LAYOUT_GENERAL};
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = probeFilterSets_[mip];
    write.dstBinding = 1;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write.pImageInfo = &storage;
    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
  }
  // Blit linear para os mips do cubo: sem ele o filtro amostra só o mip 0.
  VkFormatProperties properties{};
  vkGetPhysicalDeviceFormatProperties(physicalDevice_, sceneColorFormat_, &properties);
  constexpr VkFormatFeatureFlags blit = VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT |
                                        VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
  probeBlitMips_ = (properties.optimalTilingFeatures & blit) == blit;
  __android_log_print(ANDROID_LOG_INFO, LogTag, "[Probes] atlas %ux%u x%u, %u mips, blit de mips %s.",
                      renderer::ReflectionProbeAtlasSize, renderer::ReflectionProbeAtlasSize,
                      renderer::MaximumFrameReflectionProbes, renderer::ReflectionProbeAtlasMips,
                      probeBlitMips_ ? "sim" : "não");
  return true;
}

void InstancedRenderer::destroyProbeCaptureTarget() {
  for (auto &framebuffer : probeFramebuffers_) {
    if (framebuffer) vkDestroyFramebuffer(device_, framebuffer, nullptr);
    framebuffer = VK_NULL_HANDLE;
  }
  for (auto &view : probeFaceViews_) {
    if (view) vkDestroyImageView(device_, view, nullptr);
    view = VK_NULL_HANDLE;
  }
  if (probeFacePool_) vkDestroyDescriptorPool(device_, probeFacePool_, nullptr);
  probeFacePool_ = VK_NULL_HANDLE;
  probeFaceSets_.fill(VK_NULL_HANDLE);
  for (auto &uniform : probeFaceUniforms_) uniform.reset();
  probeCapture_.reset();
  probeCaptureDepth_.reset();
  probeCaptureMaskA_.reset();
  probeCaptureMaskB_.reset();
  probeCaptureSampler_.shutdown();
  probeCaptureSize_ = 0;
}

void InstancedRenderer::destroyProbeResources() {
  destroyProbeCaptureTarget();
  for (auto &view : probeScratchViews_) {
    if (view) vkDestroyImageView(device_, view, nullptr);
    view = VK_NULL_HANDLE;
  }
  if (probeFilterPipeline_) vkDestroyPipeline(device_, probeFilterPipeline_, nullptr);
  if (probeFilterLayout_) vkDestroyPipelineLayout(device_, probeFilterLayout_, nullptr);
  if (probeFilterPool_) vkDestroyDescriptorPool(device_, probeFilterPool_, nullptr);
  if (probeFilterSetLayout_) vkDestroyDescriptorSetLayout(device_, probeFilterSetLayout_, nullptr);
  probeFilterPipeline_ = VK_NULL_HANDLE;
  probeFilterLayout_ = VK_NULL_HANDLE;
  probeFilterPool_ = VK_NULL_HANDLE;
  probeFilterSetLayout_ = VK_NULL_HANDLE;
  probeFilterSets_.fill(VK_NULL_HANDLE);
  probeScratch_.reset();
  probeAtlas_.reset();
  skyReflection_.reset();
  skyReflectionReady_ = skyReflectionKeyValid_ = false;
  probeAtlasSampler_.shutdown();
  probeBlock_.reset();
  probeDisabledBlock_.reset();
  probeReadback_.reset();
  probeUpload_.reset();
  probeBlockCapacity_ = 0;
  probeLayers_ = {};
  probeJob_ = {};
  probeReadbackPending_ = false;
}

bool InstancedRenderer::ensureProbeDrawCapacity(u32 draws) {
  if (probeBlock_.isReady() && draws <= probeBlockCapacity_) return true;
  const u32 capacity = std::max<u32>(draws, 64u) + draws / 4;
  rhi::BufferDesc desc{};
  desc.sizeBytes = ProbeHeaderBytes + static_cast<u64>(capacity) * ProbeDrawBytes;
  desc.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  desc.cpuAccess = rhi::CpuAccess::SequentialWrite;
  desc.preferDeviceMemory = false;
  rhi::VulkanBuffer replacement;
  if (!memoryAllocator_->createBuffer(desc, &replacement) || !replacement.mappedData()) return false;
  std::memset(replacement.mappedData(), 0, static_cast<usize>(desc.sizeBytes));
  // Chamado com a cerca do quadro anterior já cumprida: ninguém lê o buffer velho.
  probeBlock_ = std::move(replacement);
  probeBlockCapacity_ = capacity;
  probeDrawCache_.clear();
  if (environmentSet_ != VK_NULL_HANDLE) {
    VkDescriptorBufferInfo info{probeBlock_.handle(), 0, probeBlock_.sizeBytes()};
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = environmentSet_;
    write.dstBinding = 24;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = &info;
    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
  }
  return true;
}

bool InstancedRenderer::ensureProbeCaptureTarget(u32 size) {
  if (probeCaptureSize_ == size && probeCapture_.isReady()) return true;
  destroyProbeCaptureTarget();
  rhi::ImageDesc color{};
  color.width = color.height = size;
  color.format = sceneColorFormat_;
  color.arrayLayers = 6;
  color.cube = true;
  color.mipLevels = probeBlitMips_ ? static_cast<u32>(std::bit_width(size)) : 1u;
  color.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  color.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  color.memoryClass = rhi::MemoryClass::RenderTarget;
  if (!memoryAllocator_->createImage(color, &probeCapture_)) return false;
  probeCaptureMips_ = color.mipLevels;
  rhi::ImageDesc depth{};
  depth.width = depth.height = size;
  depth.format = depthFormat_;
  depth.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  if (waterSubpassActive_) depth.usage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
  depth.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | (hasStencil(depthFormat_) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u);
  depth.memoryClass = rhi::MemoryClass::RenderTarget;
  if (!memoryAllocator_->createImage(depth, &probeCaptureDepth_)) return false;
  if (temporalMasksActive_) {
    rhi::ImageDesc mask = depth;
    mask.format = VK_FORMAT_R8_UNORM;
    mask.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    mask.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    if (!memoryAllocator_->createImage(mask, &probeCaptureMaskA_) || !memoryAllocator_->createImage(mask, &probeCaptureMaskB_))
      return false;
  }
  for (u32 face = 0; face < 6; ++face) {
    VkImageViewCreateInfo view{};
    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.image = probeCapture_.handle();
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = sceneColorFormat_;
    view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, face, 1};
    if (vkCreateImageView(device_, &view, nullptr, &probeFaceViews_[face]) != VK_SUCCESS) return false;
    VkImageView attachments[]{probeFaceViews_[face], probeCaptureDepth_.view(), probeCaptureMaskA_.view(),
                              probeCaptureMaskB_.view()};
    VkFramebufferCreateInfo framebuffer{};
    framebuffer.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer.renderPass = previewRenderPass_;
    framebuffer.attachmentCount = temporalMasksActive_ ? 4u : 2u;
    framebuffer.pAttachments = attachments;
    framebuffer.width = framebuffer.height = size;
    framebuffer.layers = 1;
    if (vkCreateFramebuffer(device_, &framebuffer, nullptr, &probeFramebuffers_[face]) != VK_SUCCESS) return false;
    rhi::BufferDesc uniform{};
    uniform.sizeBytes = sizeof(DirtRoadFrameUniform);
    uniform.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    uniform.cpuAccess = rhi::CpuAccess::SequentialWrite;
    uniform.preferDeviceMemory = false;
    if (!memoryAllocator_->createBuffer(uniform, &probeFaceUniforms_[face])) return false;
  }
  VkDescriptorPoolSize sizes[]{{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 6}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6 * 12},
                               {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6 * 7}, {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 6}};
  VkDescriptorPoolCreateInfo pool{};
  pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool.maxSets = 6;
  pool.poolSizeCount = 4;
  pool.pPoolSizes = sizes;
  if (vkCreateDescriptorPool(device_, &pool, nullptr, &probeFacePool_) != VK_SUCCESS) return false;
  std::array<VkDescriptorSetLayout, 6> layouts;
  layouts.fill(environmentSetLayout_);
  VkDescriptorSetAllocateInfo allocate{};
  allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocate.descriptorPool = probeFacePool_;
  allocate.descriptorSetCount = 6;
  allocate.pSetLayouts = layouts.data();
  if (vkAllocateDescriptorSets(device_, &allocate, probeFaceSets_.data()) != VK_SUCCESS) return false;
  rhi::SamplerDesc sampler{};
  sampler.addressU = sampler.addressV = sampler.addressW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler.maxLod = static_cast<float>(probeCaptureMips_ - 1);
  if (!probeCaptureSampler_.initialize(device_, sampler)) return false;
  for (u32 mip = 0; mip < renderer::ReflectionProbeAtlasMips; ++mip) {
    VkDescriptorImageInfo cube{probeCaptureSampler_.handle(), probeCapture_.view(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = probeFilterSets_[mip];
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &cube;
    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
  }
  probeCaptureSize_ = size;
  return true;
}

bool InstancedRenderer::writeProbeFaceSets() {
  // Os conjuntos das faces copiam o conjunto do ambiente do quadro (mapas,
  // sombras, nuvens, luzes) e trocam três coisas: o UBO da face, o buffer de
  // sondas desligado (a captura não reflete sondas, como o bake da Unity, que
  // não vê a própria sonda) e a profundidade de entrada da água.
  for (u32 face = 0; face < 6; ++face) {
    std::vector<VkCopyDescriptorSet> copies;
    for (u32 binding = 1; binding <= 18; ++binding) {
      if ((binding == 5 || binding == 6 || binding == 15) && !waterSubpassActive_) continue;
      if (binding >= 7 && binding <= 14 && !spectralWaterCount_) continue;
      if (binding == 5) continue;
      VkCopyDescriptorSet copy{};
      copy.sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
      copy.srcSet = environmentSet_;
      copy.dstSet = probeFaceSets_[face];
      copy.srcBinding = copy.dstBinding = binding;
      copy.descriptorCount = 1;
      copies.push_back(copy);
    }
    VkCopyDescriptorSet atlas{};
    atlas.sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
    atlas.srcSet = environmentSet_;
    atlas.dstSet = probeFaceSets_[face];
    atlas.srcBinding = atlas.dstBinding = 25;
    atlas.descriptorCount = 1;
    copies.push_back(atlas);
    VkDescriptorBufferInfo uniform{probeFaceUniforms_[face].handle(), 0, sizeof(DirtRoadFrameUniform)};
    VkDescriptorBufferInfo disabled{probeDisabledBlock_.handle(), 0, probeDisabledBlock_.sizeBytes()};
    VkDescriptorImageInfo depth{VK_NULL_HANDLE, probeCaptureDepth_.view(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet writes[3]{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = probeFaceSets_[face];
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].pBufferInfo = &uniform;
    writes[1] = writes[0];
    writes[1].dstBinding = 24;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].pBufferInfo = &disabled;
    writes[2] = writes[0];
    writes[2].dstBinding = 5;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    writes[2].pBufferInfo = nullptr;
    writes[2].pImageInfo = &depth;
    vkUpdateDescriptorSets(device_, 0, nullptr, static_cast<u32>(copies.size()), copies.data());
    vkUpdateDescriptorSets(device_, waterSubpassActive_ ? 3u : 2u, writes, 0, nullptr);
  }
  return true;
}

void InstancedRenderer::collectProbeReadback() {
  if (!probeReadbackPending_) return;
  probeReadbackPending_ = false;
  auto result = std::move(probeReadbackResult_);
  probeReadbackResult_ = {};
  if (!memoryAllocator_->invalidateBuffer(probeReadback_)) {
    result.success = false;
    result.error = "Leitura da captura falhou";
    probeResults_.push_back(std::move(result));
    return;
  }
  const auto *halves = static_cast<const u16 *>(probeReadback_.mappedData());
  if (result.kind == 0) {
    auto &texture = result.texture;
    texture.texels.assign(halves, halves + texture.expectedTexels());
    result.success = texture.valid(&result.error);
  } else {
    auto &faces = result.faces;
    const usize texels = 6ull * faces.size * faces.size;
    faces.rgb.resize(texels * 3);
    for (usize i = 0; i < texels; ++i)
      for (u32 k = 0; k < 3; ++k) faces.rgb[i * 3 + k] = renderer::halfToFloat(halves[i * 4 + k]);
    result.success = faces.valid();
  }
  probeResults_.push_back(std::move(result));
}

void InstancedRenderer::updateProbeResidency(const float camera[3]) {
  std::array<u32, renderer::MaximumFrameReflectionProbes> chosen{};
  const u32 count = renderer::selectFrameReflectionProbes(sceneProbes_, camera, chosen);
  std::array<bool, renderer::MaximumFrameReflectionProbes> keep{};
  std::vector<u32> incoming;
  for (u32 i = 0; i < count; ++i) {
    const auto &probe = sceneProbes_[chosen[i]];
    const u64 key = probeLayerKey(probe.objectId, probe.instance);
    bool found = false;
    for (u32 layer = 0; layer < probeLayers_.size(); ++layer)
      if (probeLayers_[layer].used && probeLayers_[layer].key == key) {
        keep[layer] = true;
        auto &slot = probeLayers_[layer];
        // Mudou o que a imagem mostra: tempo real recaptura; assada só com bake novo.
        if (slot.probe.type != probe.type || slot.probe.baked != probe.baked ||
            (probe.type == renderer::ReflectionProbeType::Realtime && slot.probe.captureKey != probe.captureKey))
          slot.contentKey = 0;
        const bool geometry = std::memcmp(slot.probe.boxMin, probe.boxMin, sizeof(probe.boxMin)) != 0 ||
                              std::memcmp(slot.probe.boxMax, probe.boxMax, sizeof(probe.boxMax)) != 0 ||
                              slot.probe.importance != probe.importance || slot.probe.blendDistance != probe.blendDistance ||
                              slot.probe.intensity != probe.intensity || slot.probe.boxProjection != probe.boxProjection;
        if (geometry) ++probeLayerRevision_;
        slot.probe = probe;
        found = true;
        break;
      }
    if (!found) incoming.push_back(chosen[i]);
  }
  for (u32 layer = 0; layer < probeLayers_.size(); ++layer)
    if (probeLayers_[layer].used && !keep[layer]) {
      probeLayers_[layer] = {};
      ++probeLayerRevision_;
      if (probeJob_.active && probeJob_.layer == static_cast<i32>(layer) && !probeJob_.bake) probeJob_ = {};
    }
  for (u32 index : incoming)
    for (u32 layer = 0; layer < probeLayers_.size(); ++layer)
      if (!probeLayers_[layer].used) {
        auto &slot = probeLayers_[layer];
        slot = {};
        slot.used = true;
        slot.probe = sceneProbes_[index];
        slot.key = probeLayerKey(slot.probe.objectId, slot.probe.instance);
        slot.renderRequests = slot.probe.renderRequests;
        ++probeLayerRevision_;
        break;
      }
}

void InstancedRenderer::scheduleProbeWork() {
  // Conteúdo das camadas assadas: copia o recurso quando a camada não tem o
  // conteúdo dele (residência nova, recurso reassado).
  probePendingUploads_.clear();
  for (u32 layer = 0; layer < probeLayers_.size(); ++layer) {
    auto &slot = probeLayers_[layer];
    if (!slot.used || slot.probe.type != renderer::ReflectionProbeType::Baked || slot.contentKey != 0) continue;
    std::shared_ptr<const renderer::ReflectionProbeTexture> texture;
    for (const auto &candidate : probeTextures_)
      if (candidate && candidate->guid == slot.probe.baked) { texture = candidate; break; }
    slot.contentKey = 1; // sem recurso também é decisão: fica no céu até assar
    const bool wasReady = slot.ready;
    slot.ready = false;
    if (texture && texture->valid()) {
      probePendingUploads_.push_back({layer, texture});
      slot.baseMip = texture->baseMip;
      slot.hdr = texture->hdr;
    }
    if (wasReady || texture) ++probeLayerRevision_;
  }
  if (probeJob_.active) return;
  // O reflexo do céu antes de tudo: as sondas e todo material leem dele o que
  // sobra dos pesos. Só o céu, sem objetos, na resolução padrão da Unity (128).
  if (skyReflectionWanted_ && skyReflection_.isReady() && probeAtlasInitialized_) {
    const auto key = skyReflectionKey(*static_cast<const DirtRoadFrameUniform *>(environmentUniform_.mappedData()));
    // postIsolation 256: recaptura a cada quadro, o pior caso de um céu que
    // muda o tempo todo (ciclo de dia acelerado), para medir o custo.
    if (renderer::skyReflectionNeedsCapture(key, skyReflectionKey_, skyReflectionKeyValid_) || (postIsolation_ & 256u)) {
      ProbeJob job;
      job.active = true;
      job.request.kind = 2;
      job.request.cullingMask = 0;
      job.request.staticOnly = false;
      job.request.solidBackground = false;
      job.request.hdr = true;
      job.request.resolution = renderer::SkyReflectionResolution;
      job.size = renderer::SkyReflectionResolution;
      job.slicing = renderer::ReflectionProbeSlicing::NoTimeSlicing;
      job.baseMip = renderer::reflectionProbeBaseMip(job.size);
      job.nextMip = job.baseMip;
      skyReflectionKey_ = key;
      skyReflectionKeyValid_ = true;
      probeJob_ = job;
      return;
    }
  }
  // Pedidos do editor (bake) primeiro, depois as sondas de tempo real.
  if (!probeCaptureQueue_.empty()) {
    ProbeJob job;
    job.active = true;
    job.bake = true;
    job.request = probeCaptureQueue_.front();
    probeCaptureQueue_.pop_front();
    job.size = job.request.kind == 0 ? std::clamp<u32>(job.request.resolution, 16u, 256u) : std::clamp<u32>(job.request.resolution, 8u, 64u);
    job.slicing = renderer::ReflectionProbeSlicing::NoTimeSlicing;
    job.baseMip = renderer::reflectionProbeBaseMip(job.size);
    job.nextMip = job.baseMip;
    for (u32 layer = 0; layer < probeLayers_.size(); ++layer)
      if (job.request.kind == 0 && probeLayers_[layer].used &&
          probeLayers_[layer].key == probeLayerKey(job.request.objectId, job.request.instance))
        job.layer = static_cast<i32>(layer);
    probeJob_ = job;
    return;
  }
  for (u32 layer = 0; layer < probeLayers_.size(); ++layer) {
    auto &slot = probeLayers_[layer];
    if (!slot.used || slot.probe.type != renderer::ReflectionProbeType::Realtime) continue;
    bool capture = slot.contentKey != slot.probe.captureKey;
    if (sceneProbesPlaying_) {
      // Em Play o modo de atualização manda, como no Awake/Update da Unity.
      switch (slot.probe.refresh) {
      case renderer::ReflectionProbeRefresh::OnAwake: capture = !slot.awakeDone; break;
      case renderer::ReflectionProbeRefresh::EveryFrame: capture = true; break;
      case renderer::ReflectionProbeRefresh::ViaScripting:
        capture = slot.renderRequests != slot.probe.renderRequests || !slot.ready;
        break;
      }
    }
    if (!capture) continue;
    ProbeJob job;
    job.active = true;
    job.layer = static_cast<i32>(layer);
    job.request.kind = 0;
    job.request.objectId = slot.probe.objectId;
    job.request.instance = slot.probe.instance;
    std::copy(slot.probe.position, slot.probe.position + 3, job.request.position);
    job.request.resolution = slot.probe.resolution;
    job.request.nearClip = slot.probe.nearClip;
    job.request.farClip = slot.probe.farClip;
    job.request.cullingMask = slot.probe.cullingMask;
    job.request.staticOnly = false;
    job.request.solidBackground = slot.probe.solidBackground;
    std::copy(slot.probe.background, slot.probe.background + 3, job.request.background);
    job.request.hdr = slot.probe.hdr;
    job.size = std::clamp<u32>(slot.probe.resolution, 16u, 256u);
    // Fora do Play a edição pede o resultado já: sem fatiamento.
    job.slicing = sceneProbesPlaying_ ? slot.probe.slicing : renderer::ReflectionProbeSlicing::NoTimeSlicing;
    job.baseMip = renderer::reflectionProbeBaseMip(job.size);
    job.nextMip = job.baseMip;
    job.contentKey = slot.probe.captureKey;
    job.renderRequests = slot.probe.renderRequests;
    probeJob_ = job;
    return;
  }
}

bool InstancedRenderer::prepareProbes(const platform::FreeCameraState &camera) {
  using Clock = std::chrono::steady_clock;
  const auto start = Clock::now();
  probeStats_ = {};
  probeFrameFaces_ = 0;
  probeFrameMipGen_ = false;
  probeFrameMips_.clear();
  probeFrameFinish_ = false;
  if (!dirtRoadPreview_ || !probeAtlas_.isReady() || environmentSet_ == VK_NULL_HANDLE) return true;
  if (!ensureProbeDrawCapacity(std::max<u32>(instanceCount_, 1))) return false;
  updateProbeResidency(camera.position);
  scheduleProbeWork();

  // Trabalho deste quadro conforme o fatiamento (Time Slicing da Unity):
  // sem fatiamento tudo agora; todas as faces de uma vez, faces num quadro e um
  // mip por quadro depois; uma face por quadro, faces e mips um por quadro.
  if (probeJob_.active) {
    auto &job = probeJob_;
    if (!ensureProbeCaptureTarget(job.size) || !writeProbeFaceSets()) {
      ProbeCaptureResult failed;
      failed.id = job.request.id;
      failed.kind = job.request.kind;
      failed.error = "Alvo de captura indisponível";
      if (job.bake) probeResults_.push_back(std::move(failed));
      job = {};
    } else {
      using S = renderer::ReflectionProbeSlicing;
      const bool all = job.slicing == S::NoTimeSlicing;
      if (job.facesDone < 6) {
        const u32 faces = all || job.slicing == S::AllFacesAtOnce ? 6u - job.facesDone : 1u;
        for (u32 f = 0; f < faces; ++f) probeFrameFaces_ |= 1u << (job.facesDone + f);
      }
      const bool facesComplete = job.facesDone + static_cast<u32>(std::popcount(probeFrameFaces_)) >= 6;
      if (job.request.kind == 1) {
        probeFrameFinish_ = facesComplete;
      } else if (facesComplete) {
        const bool sameFrameFilter = all || probeFrameFaces_ == 0;
        if (sameFrameFilter) {
          if (!job.mipsGenerated) probeFrameMipGen_ = true;
          const u32 last = renderer::ReflectionProbeAtlasMips;
          const u32 mips = all ? last - job.nextMip : 1u;
          for (u32 m = 0; m < mips && job.nextMip + m < last; ++m) probeFrameMips_.push_back(job.nextMip + m);
          probeFrameFinish_ = job.nextMip + static_cast<u32>(probeFrameMips_.size()) >= last;
        }
      }
      // UBO de cada face: o do quadro com a vista da sonda.
      const auto &main = *static_cast<const DirtRoadFrameUniform *>(environmentUniform_.mappedData());
      for (u32 face = 0; face < 6; ++face) {
        if (!(probeFrameFaces_ & (1u << face))) continue;
        auto frame = main;
        const auto basis = renderer::buildCameraViewBasis(ProbeFaceYaw[face], ProbeFacePitch[face]);
        std::copy(basis.row0, basis.row0 + 3, frame.worldToViewRow0);
        std::copy(basis.row1, basis.row1 + 3, frame.worldToViewRow1);
        std::copy(basis.row2, basis.row2 + 3, frame.worldToViewRow2);
        frame.worldToViewRow0[3] = 0;          // perspectiva
        frame.shadowTransitionParameters[2] = 1; // 90°: focal 1
        frame.shadowFilterParameters[2] = frame.shadowFilterParameters[3] = 0; // sem jitter
        frame.postViewport[0] = frame.postViewport[1] = 0;
        frame.postViewport[2] = frame.postViewport[3] = 1;
        frame.scenePost[2] = hdrSceneColor_ ? 1.0f : 0.0f;
        // As cascatas são de mundo: a captura usa a maior delas (a que cobre a
        // distância de sombra da câmera), sem transições, pela profundidade da
        // sonda — sem isso o interior capturado sairia todo ensolarado.
        const int cascades = static_cast<int>(frame.shadowParameters[1] + 0.5f);
        if (cascades > 0) {
          for (int c = 0; c < cascades - 1; ++c) frame.shadowSplitDepths[c] = -1.0f;
          frame.shadowSplitDepths[cascades - 1] = 1e9f;
          frame.shadowTransitionParameters[0] = frame.shadowTransitionParameters[1] = 0;
        }
        if (job.request.kind == 1) {
          // Irradiância: o sol já chega ao objeto em tempo real com sombra; o
          // disco e a auréola no SH contariam a luz direta duas vezes.
          frame.sceneSky[2] = 0;
          frame.celestialParameters[0] = 0;
        }
        std::memcpy(probeFaceUniforms_[face].mappedData(), &frame, sizeof(frame));
        if (!memoryAllocator_->flushBuffer(probeFaceUniforms_[face])) return false;
      }
    }
  }

  // Registro por desenho: a mistura de reflexo e o SH das sondas de luz,
  // recalculados só para o desenho que andou ou quando sondas/camadas mudaram.
  auto *block = static_cast<u8 *>(probeBlock_.mappedData());
  auto &header = *reinterpret_cast<ProbeHeaderGpu *>(block);
  std::array<renderer::SceneReflectionProbe, renderer::MaximumFrameReflectionProbes> frameProbes{};
  u32 frameCount = 0;
  bool anyReady = false;
  for (u32 layer = 0; layer < probeLayers_.size(); ++layer) {
    const auto &slot = probeLayers_[layer];
    frameProbes[layer] = slot.probe;
    if (!slot.used || !slot.ready) {
      // Camada sem conteúdo não recebe objeto: caixa vazia (mínimo > máximo).
      for (u32 k = 0; k < 3; ++k) { frameProbes[layer].boxMin[k] = 1; frameProbes[layer].boxMax[k] = -1; }
      continue;
    }
    frameCount = layer + 1;
    anyReady = true;
    for (u32 k = 0; k < 3; ++k) {
      header.boxMin[layer][k] = slot.probe.boxMin[k];
      header.boxMax[layer][k] = slot.probe.boxMax[k];
      header.capture[layer][k] = slot.probe.position[k];
    }
    header.boxMin[layer][3] = slot.probe.intensity;
    header.boxMax[layer][3] = slot.probe.boxProjection ? 1.0f : 0.0f;
    header.capture[layer][3] = static_cast<float>(slot.baseMip);
    header.parameters[layer][0] = static_cast<float>(renderer::ReflectionProbeAtlasMips - 1);
    header.parameters[layer][1] = slot.hdr ? 0.0f : 1.0f;
    ++probeStats_.readyProbes;
  }
  const auto *set = lightProbeSet_.get();
  const bool lightProbes = set && !set->empty();
  header.header[0] = (anyReady ? 1u : 0u) | (lightProbes ? 2u : 0u);
  header.header[1] = frameCount;
  const auto &draws = dirtRoadResources_.draws();
  if (probeDrawCache_.size() != draws.size()) probeDrawCache_.assign(draws.size(), {});
  auto *entries = reinterpret_cast<ProbeDrawGpu *>(block + ProbeHeaderBytes);
  const std::span<const renderer::SceneReflectionProbe> frameSpan(frameProbes.data(), probeLayers_.size());
  for (u32 i = 0; i < draws.size() && i < probeBlockCapacity_; ++i) {
    auto &cache = probeDrawCache_[i];
    const auto &draw = draws[i];
    const renderer::DrawLightingKey key = i < authoredLighting_.size() ? authoredLighting_[i] : renderer::DrawLightingKey{};
    const bool moved = !cache.valid || std::memcmp(cache.center, draw.boundsCenter, sizeof(cache.center)) != 0 ||
                       cache.lightProbes != key.lightProbes || cache.reflectionProbes != key.reflectionProbes;
    const bool reflectionDirty = moved || cache.layerRevision != probeLayerRevision_;
    const bool lightDirty = moved || cache.lightRevision != lightProbeRevision_;
    if (!reflectionDirty && !lightDirty) continue;
    auto &entry = entries[i];
    if (reflectionDirty) {
      const auto blend = renderer::blendReflectionProbes(frameSpan, draw.boundsCenter,
                                                         static_cast<renderer::ReflectionProbeUsage>(key.reflectionProbes));
      entry.reflection[0] = renderer::encodeReflectionProbeBlend(blend);
      cache.layerRevision = probeLayerRevision_;
    }
    if (lightDirty) {
      // Como na Unity, as sondas de luz iluminam o que se move; o que é Static
      // recebe a luz indireta assada (aqui, sem lightmap, o ambiente do céu).
      if (lightProbes && key.lightProbes != 0 && !key.isStatic) {
        const auto where = renderer::locateProbes(*set, draw.boundsCenter, cache.lightRevision == lightProbeRevision_ ? cache.tetrahedron : -1);
        const auto sh = renderer::interpolateProbes(*set, where);
        cache.tetrahedron = where.tetrahedron;
        for (u32 c = 0; c < 9; ++c) {
          for (u32 k = 0; k < 3; ++k) entry.sh[c][k] = sh.c[c][k];
          entry.sh[c][3] = 0;
        }
        entry.sh[0][3] = where.count ? 1.0f : 0.0f;
      } else {
        entry.sh[0][3] = 0;
        cache.tetrahedron = -1;
      }
      cache.lightRevision = lightProbeRevision_;
    }
    std::copy(draw.boundsCenter, draw.boundsCenter + 3, cache.center);
    cache.lightProbes = key.lightProbes;
    cache.reflectionProbes = key.reflectionProbes;
    cache.valid = true;
    ++probeStats_.drawsUpdated;
  }
  if (probeStats_.drawsUpdated) editorSceneCacheValid_ = false;
  for (u32 i = 0; i < draws.size() && i < probeBlockCapacity_; ++i) {
    if (entries[i].sh[0][3] > 0.5f) ++probeStats_.drawsWithLightProbes;
    if ((entries[i].reflection[0] >> 8) != 0) ++probeStats_.drawsWithReflection;
  }
  if (!memoryAllocator_->flushBuffer(probeBlock_)) return false;

  // Subida dos recursos assados: um buffer de passagem por quadro.
  if (!probePendingUploads_.empty()) {
    u64 bytes = 0;
    for (const auto &[layer, texture] : probePendingUploads_) bytes += texture->texels.size() * sizeof(u16);
    if (probeUpload_.sizeBytes() < bytes) {
      probeUpload_.reset();
      rhi::BufferDesc staging{};
      staging.sizeBytes = bytes;
      staging.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
      staging.cpuAccess = rhi::CpuAccess::SequentialWrite;
      staging.preferDeviceMemory = false;
      if (!memoryAllocator_->createBuffer(staging, &probeUpload_)) return false;
    }
    u64 offset = 0;
    for (const auto &[layer, texture] : probePendingUploads_) {
      std::memcpy(static_cast<u8 *>(probeUpload_.mappedData()) + offset, texture->texels.data(), texture->texels.size() * sizeof(u16));
      offset += texture->texels.size() * sizeof(u16);
    }
    if (!memoryAllocator_->flushBuffer(probeUpload_)) return false;
  }
  for (const auto &slot : probeLayers_) if (slot.used) ++probeStats_.residentProbes;
  probeStats_.sceneProbes = static_cast<u32>(sceneProbes_.size());
  probeStats_.lightProbes = lightProbes ? static_cast<u32>(set->positions.size()) : 0;
  probeStats_.tetrahedra = lightProbes ? static_cast<u32>(set->tetrahedra.size()) : 0;
  probeStats_.cpuMs = std::chrono::duration<float, std::milli>(Clock::now() - start).count();
  return true;
}

void InstancedRenderer::recordProbeFace(u32 face, float timeSeconds) {
  const auto &request = probeJob_.request;
  const u32 size = probeCaptureSize_;
  const float yaw = ProbeFaceYaw[face], pitch = ProbeFacePitch[face];
  renderer::PerspectiveVisibilitySettings settings{};
  settings.verticalFieldOfViewRadians = 1.57079633f;
  settings.nearPlane = request.nearClip;
  settings.farPlane = request.farClip;
  const auto frustum = renderer::buildPerspectiveFrustum(request.position, yaw, pitch, 1.0f, settings);
  VkClearValue clear[4]{};
  clear[0].color = {{request.solidBackground ? request.background[0] : 0.0f, request.solidBackground ? request.background[1] : 0.0f,
                     request.solidBackground ? request.background[2] : 0.0f, 1.0f}};
  clear[1].depthStencil = {1, 0};
  VkRenderPassBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  begin.renderPass = previewRenderPass_;
  begin.framebuffer = probeFramebuffers_[face];
  begin.renderArea.extent = {size, size};
  begin.clearValueCount = temporalMasksActive_ ? 4u : 2u;
  begin.pClearValues = clear;
  vkCmdBeginRenderPass(commandBuffer_, &begin, VK_SUBPASS_CONTENTS_INLINE);
  const VkViewport viewport{0, 0, static_cast<float>(size), static_cast<float>(size), 0, 1};
  const VkRect2D scissor{{0, 0}, {size, size}};
  vkCmdSetViewport(commandBuffer_, 0, 1, &viewport);
  vkCmdSetScissor(commandBuffer_, 0, 1, &scissor);
  vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 1, 1, &probeFaceSets_[face], 0, nullptr);
  if (useBindless_)
    vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &textureSet_, 0, nullptr);
  const VkDeviceSize zero = 0;
  const VkBuffer instances = instanceBuffer_.handle();
  vkCmdBindVertexBuffers(commandBuffer_, 1, 1, &instances, &zero);
  auto pushMaterial = [&](u32 drawIndex, const renderer::MapMaterialRecord &material) {
    if (materialCulling_)
      rhiDevice_->cmdSetCullMode(commandBuffer_, (material.flags & renderer::MapMaterialCullBackFaces) != 0 &&
                                                         (material.flags & renderer::MapMaterialDoubleSided) == 0
                                                     ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE);
    if (!useBindless_) {
      const VkDescriptorSet set = dirtMaterialSets_[dirtRoadResources_.draws()[drawIndex].materialIndex];
      vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &set, 0, nullptr);
    }
    DirtRoadPushConstants push{};
    push.cameraFrame[0] = 1.0f;
    push.cameraFrame[1] = yaw;
    push.cameraFrame[2] = pitch;
    push.cameraFrame[3] = timeSeconds;
    push.surfaceTransform[0] = push.surfaceTransform[3] = 1.0f;
    std::memcpy(push.cameraPositionNear, request.position, sizeof(request.position));
    push.cameraPositionNear[3] = request.nearClip;
    std::memcpy(push.baseColorFactor, material.baseColorFactor, sizeof(push.baseColorFactor));
    std::memcpy(push.emissiveFactorAndStrength, material.emissiveFactorAndStrength, sizeof(push.emissiveFactorAndStrength));
    for (u32 slot = 0; slot < 4; ++slot) {
      const u32 texture = material.textureIndices[slot];
      push.textureIndices[slot] = useBindless_ && texture != renderer::InvalidMapTexture && texture < dirtTextureSlots_.size()
                                      ? dirtTextureSlots_[texture] : baseTextureIndex_;
    }
    push.materialFlags[0] = material.flags;
    const u32 alphaCutoff = static_cast<u32>(std::clamp(material.alphaCutoff, 0.0f, 1.0f) * 255.0f + .5f);
    push.materialFlags[1] = material.textureCoordinates | (alphaCutoff << 8u) | ((material.reserved & 0xffffu) << 16u);
    push.materialFlags[2] = (drawIndex < authoredUvTransformEntries_.size() ? authoredUvTransformEntries_[drawIndex] : 0u) << 1u;
    push.materialFlags[3] = std::bit_cast<u32>(request.farClip);
    push.materialFactors[0] = material.roughness;
    push.materialFactors[1] = material.metallic;
    push.materialFactors[2] = material.normalScale;
    push.materialFactors[3] = material.specular;
    vkCmdPushConstants(commandBuffer_, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
  };
  std::vector<u32> transparent;
  auto draw = [&](u32 index, bool blend) {
    const auto &record = dirtRoadResources_.draws()[index];
    auto material = dirtRoadResources_.materials()[record.materialIndex];
    if (index < authoredMaterials_.size())
      material = renderer::applyMaterialOverride(material, authoredMaterials_[index], static_cast<u32>(dirtRoadResources_.packageTextureCount()));
    const auto variant = renderer::materialFeatureVariant(material.flags);
    const auto *variants = blend ? transparentMaterialPipelines_ : opaqueMaterialPipelines_;
    const auto pipeline = variants[variant] ? variants[variant] : (blend ? transparentPipeline_ : pipeline_);
    vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    pushMaterial(index, material);
    const auto geometry = drawGeometry(index);
    vkCmdBindVertexBuffers(commandBuffer_, 0, 1, &geometry.buffer, &zero);
    vkCmdBindIndexBuffer(commandBuffer_, dirtRoadResources_.indexBuffer(), 0, VK_INDEX_TYPE_UINT32);
    const auto range = drawIndexRange(index);
    vkCmdDrawIndexed(commandBuffer_, range.indexCount, 1, range.firstIndex, geometry.vertexOffset, index);
  };
  for (u32 i = 0; request.kind != 2 && i < dirtRoadResources_.draws().size(); ++i) {
    if (!authoredVisibility_.empty() && !authoredVisibility_[i]) continue;
    const renderer::DrawLightingKey key = i < authoredLighting_.size() ? authoredLighting_[i] : renderer::DrawLightingKey{};
    // Culling Mask da sonda e, no bake, só o que é Static — como a Unity.
    const u32 layerBit = key.objectLayer < 32 ? 1u << key.objectLayer : 0u;
    if ((request.cullingMask & layerBit) == 0) continue;
    if (request.staticOnly && !authoredLighting_.empty() && !key.isStatic) continue;
    const auto &record = dirtRoadResources_.draws()[i];
    auto material = dirtRoadResources_.materials()[record.materialIndex];
    if (i < authoredMaterials_.size()) material = renderer::applyMaterialOverride(material, authoredMaterials_[i]);
    if (material.flags & renderer::MapMaterialWater) continue;
    if (!renderer::isSphereVisible(frustum, record.boundsCenter, record.boundsRadius)) continue;
    if (material.flags & renderer::MapMaterialBlend) transparent.push_back(i);
    else draw(i, false);
  }
  if (skyPipeline_ && !request.solidBackground) {
    DirtRoadPushConstants sky{};
    sky.cameraFrame[0] = 1.0f;
    sky.cameraFrame[1] = yaw;
    sky.cameraFrame[2] = pitch;
    sky.cameraFrame[3] = timeSeconds;
    sky.materialFactors[3] = 1.0f; // 1/tan(45°)
    sky.surfaceTransform[0] = sky.surfaceTransform[3] = 1.0f;
    vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, skyPipeline_);
    vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, skyPipelineLayout_, 0, 1, &probeFaceSets_[face], 0, nullptr);
    vkCmdPushConstants(commandBuffer_, skyPipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(sky), &sky);
    vkCmdDraw(commandBuffer_, 3, 1, 0, 0);
    vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 1, 1, &probeFaceSets_[face], 0, nullptr);
    if (useBindless_)
      vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &textureSet_, 0, nullptr);
  }
  if (waterSubpassActive_) vkCmdNextSubpass(commandBuffer_, VK_SUBPASS_CONTENTS_INLINE);
  const auto distance = [&](u32 index) {
    float result = 0;
    const auto &r = dirtRoadResources_.draws()[index];
    for (u32 k = 0; k < 3; ++k) { const float d = r.boundsCenter[k] - request.position[k]; result += d * d; }
    return result;
  };
  std::sort(transparent.begin(), transparent.end(), [&](u32 a, u32 b) { return distance(a) > distance(b); });
  for (u32 index : transparent) draw(index, true);
  vkCmdEndRenderPass(commandBuffer_);
}

void InstancedRenderer::recordProbes(float timeSeconds) {
  if (!probeAtlas_.isReady()) return;
  if (!probeAtlasInitialized_) {
    const u32 layers = renderer::MaximumFrameReflectionProbes, mips = renderer::ReflectionProbeAtlasMips;
    probeImageBarrier(commandBuffer_, probeAtlas_.handle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
                 VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, mips, 0, layers);
    const VkClearColorValue black{{0, 0, 0, 1}};
    const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, mips, 0, layers};
    vkCmdClearColorImage(commandBuffer_, probeAtlas_.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &black, 1, &range);
    probeImageBarrier(commandBuffer_, probeAtlas_.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                 VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, mips, 0, layers);
    probeImageBarrier(commandBuffer_, skyReflection_.handle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
                 VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, mips, 0, 1);
    const VkImageSubresourceRange skyRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, mips, 0, 1};
    vkCmdClearColorImage(commandBuffer_, skyReflection_.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &black, 1, &skyRange);
    probeImageBarrier(commandBuffer_, skyReflection_.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                 VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, mips, 0, 1);
    probeAtlasInitialized_ = true;
  }
  // Recursos assados que entraram na residência neste quadro.
  if (!probePendingUploads_.empty()) {
    u64 offset = 0;
    for (const auto &[layer, texture] : probePendingUploads_) {
      probeImageBarrier(commandBuffer_, probeAtlas_.handle(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                   VK_PIPELINE_STAGE_TRANSFER_BIT, 0, renderer::ReflectionProbeAtlasMips, layer, 1);
      std::vector<VkBufferImageCopy> regions;
      for (u32 mip = texture->baseMip; mip < texture->mips; ++mip) {
        VkBufferImageCopy region{};
        region.bufferOffset = offset;
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mip, layer, 1};
        region.imageExtent = {texture->mipSide(mip), texture->mipSide(mip), 1};
        regions.push_back(region);
        offset += static_cast<u64>(texture->mipSide(mip)) * texture->mipSide(mip) * 8;
      }
      vkCmdCopyBufferToImage(commandBuffer_, probeUpload_.handle(), probeAtlas_.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             static_cast<u32>(regions.size()), regions.data());
      probeImageBarrier(commandBuffer_, probeAtlas_.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                   VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, renderer::ReflectionProbeAtlasMips, layer, 1);
      probeLayers_[layer].ready = true;
      ++probeLayerRevision_;
    }
    probePendingUploads_.clear();
  }
  if (!probeJob_.active) return;
  auto &job = probeJob_;
  for (u32 face = 0; face < 6; ++face)
    if (probeFrameFaces_ & (1u << face)) {
      recordProbeFace(face, timeSeconds);
      ++job.facesDone;
      ++probeStats_.facesCaptured;
    }
  const VkImage capture = probeCapture_.handle();
  if (job.request.kind == 1) {
    if (!probeFrameFinish_) return;
    // Irradiância: as seis faces do mip 0 vão inteiras para a CPU.
    const u64 bytes = 6ull * probeCaptureSize_ * probeCaptureSize_ * 8;
    if (sceneColorFormat_ != VK_FORMAT_R16G16B16A16_SFLOAT || !ensureProbeReadback(bytes)) {
      ProbeCaptureResult failed;
      failed.id = job.request.id;
      failed.kind = 1;
      failed.error = sceneColorFormat_ != VK_FORMAT_R16G16B16A16_SFLOAT ? "Sondas de luz exigem cena em HDR neste aparelho"
                                                                       : "Buffer de leitura indisponível";
      probeResults_.push_back(std::move(failed));
      job = {};
      return;
    }
    probeImageBarrier(commandBuffer_, capture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                 VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, 0, 6);
    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 6};
    region.imageExtent = {probeCaptureSize_, probeCaptureSize_, 1};
    vkCmdCopyImageToBuffer(commandBuffer_, capture, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, probeReadback_.handle(), 1, &region);
    probeImageBarrier(commandBuffer_, capture, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                 VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, 0, 6);
    VkMemoryBarrier host{};
    host.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(commandBuffer_, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &host, 0, nullptr, 0, nullptr);
    probeReadbackResult_ = {};
    probeReadbackResult_.id = job.request.id;
    probeReadbackResult_.kind = 1;
    probeReadbackResult_.faces.size = probeCaptureSize_;
    probeReadbackPending_ = true;
    job = {};
    return;
  }
  if (probeFrameMipGen_) {
    // Mips do cubo por blit linear, face a face na mesma cadeia.
    if (probeCaptureMips_ > 1) {
      probeImageBarrier(commandBuffer_, capture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                   VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, 0, 6);
      for (u32 mip = 1; mip < probeCaptureMips_; ++mip) {
        probeImageBarrier(commandBuffer_, capture, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
                     VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, mip, 1, 0, 6);
        VkImageBlit blit{};
        const i32 src = static_cast<i32>(std::max<u32>(probeCaptureSize_ >> (mip - 1), 1u));
        const i32 dst = static_cast<i32>(std::max<u32>(probeCaptureSize_ >> mip, 1u));
        blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mip - 1, 0, 6};
        blit.srcOffsets[1] = {src, src, 1};
        blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mip, 0, 6};
        blit.dstOffsets[1] = {dst, dst, 1};
        vkCmdBlitImage(commandBuffer_, capture, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, capture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                       &blit, VK_FILTER_LINEAR);
        probeImageBarrier(commandBuffer_, capture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                     VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                     VK_PIPELINE_STAGE_TRANSFER_BIT, mip, 1, 0, 6);
      }
      probeImageBarrier(commandBuffer_, capture, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                   VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, probeCaptureMips_, 0, 6);
    } else {
      VkMemoryBarrier faces{};
      faces.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
      faces.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      faces.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      vkCmdPipelineBarrier(commandBuffer_, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
                           &faces, 0, nullptr, 0, nullptr);
    }
    job.mipsGenerated = true;
  }
  if (!probeFrameMips_.empty()) {
    const VkImage scratch = probeScratch_.handle();
    probeImageBarrier(commandBuffer_, scratch, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_SHADER_WRITE_BIT,
                 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, probeFrameMips_.front(),
                 static_cast<u32>(probeFrameMips_.size()), 0, 1);
    vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_COMPUTE, probeFilterPipeline_);
    const u32 last = renderer::ReflectionProbeAtlasMips - 1;
    for (u32 mip : probeFrameMips_) {
      vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_COMPUTE, probeFilterLayout_, 0, 1, &probeFilterSets_[mip], 0, nullptr);
      ProbeFilterPush push{};
      push.side = std::max<u32>(renderer::ReflectionProbeAtlasSize >> mip, 1u);
      push.faceSize = probeCaptureSize_;
      push.roughness = mip == job.baseMip || last == job.baseMip ? 0.0f
                       : static_cast<float>(mip - job.baseMip) / static_cast<float>(last - job.baseMip);
      push.sourceLods = static_cast<float>(probeCaptureMips_ - 1);
      push.clampValue = job.request.hdr ? 0.0f : 1.0f;
      vkCmdPushConstants(commandBuffer_, probeFilterLayout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);
      vkCmdDispatch(commandBuffer_, (push.side + 7) / 8, (push.side + 7) / 8, 1);
      ++probeStats_.mipsFiltered;
    }
    job.nextMip += static_cast<u32>(probeFrameMips_.size());
    VkMemoryBarrier written{};
    written.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    written.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    written.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(commandBuffer_, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &written, 0,
                         nullptr, 0, nullptr);
  }
  if (!probeFrameFinish_) return;
  const u32 base = job.baseMip, count = renderer::ReflectionProbeAtlasMips - base;
  const VkImage scratch = probeScratch_.handle();
  probeImageBarrier(commandBuffer_, scratch, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT,
               VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, base, count, 0, 1);
  if (job.layer >= 0) {
    const u32 layer = static_cast<u32>(job.layer);
    probeImageBarrier(commandBuffer_, probeAtlas_.handle(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                 VK_PIPELINE_STAGE_TRANSFER_BIT, 0, renderer::ReflectionProbeAtlasMips, layer, 1);
    std::vector<VkImageCopy> copies;
    for (u32 mip = base; mip < renderer::ReflectionProbeAtlasMips; ++mip) {
      VkImageCopy copy{};
      copy.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mip, 0, 1};
      copy.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mip, layer, 1};
      const u32 side = std::max<u32>(renderer::ReflectionProbeAtlasSize >> mip, 1u);
      copy.extent = {side, side, 1};
      copies.push_back(copy);
    }
    vkCmdCopyImage(commandBuffer_, scratch, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, probeAtlas_.handle(),
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<u32>(copies.size()), copies.data());
    probeImageBarrier(commandBuffer_, probeAtlas_.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                 VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, renderer::ReflectionProbeAtlasMips, layer, 1);
    auto &slot = probeLayers_[layer];
    slot.ready = true;
    slot.baseMip = base;
    slot.hdr = job.request.hdr;
    if (!job.bake) {
      slot.contentKey = job.contentKey;
      slot.awakeDone = true;
      slot.renderRequests = job.renderRequests;
    }
    ++probeLayerRevision_;
    editorSceneCacheValid_ = false;
  }
  if (job.request.kind == 2) {
    const VkImage sky = skyReflection_.handle();
    probeImageBarrier(commandBuffer_, sky, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                 VK_PIPELINE_STAGE_TRANSFER_BIT, 0, renderer::ReflectionProbeAtlasMips, 0, 1);
    std::vector<VkImageCopy> copies;
    for (u32 mip = base; mip < renderer::ReflectionProbeAtlasMips; ++mip) {
      VkImageCopy copy{};
      copy.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mip, 0, 1};
      copy.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mip, 0, 1};
      const u32 side = std::max<u32>(renderer::ReflectionProbeAtlasSize >> mip, 1u);
      copy.extent = {side, side, 1};
      copies.push_back(copy);
    }
    vkCmdCopyImage(commandBuffer_, scratch, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, sky, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   static_cast<u32>(copies.size()), copies.data());
    probeImageBarrier(commandBuffer_, sky, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                 VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, renderer::ReflectionProbeAtlasMips, 0, 1);
    skyReflectionReady_ = true;
    editorSceneCacheValid_ = false;
  }
  if (job.bake) {
    renderer::ReflectionProbeTexture texture;
    texture.guid = {};
    texture.baseMip = base;
    texture.hdr = job.request.hdr;
    const u64 bytes = texture.expectedTexels() * sizeof(u16);
    if (!ensureProbeReadback(bytes)) {
      ProbeCaptureResult failed;
      failed.id = job.request.id;
      failed.error = "Buffer de leitura indisponível";
      probeResults_.push_back(std::move(failed));
    } else {
      std::vector<VkBufferImageCopy> regions;
      u64 offset = 0;
      for (u32 mip = base; mip < renderer::ReflectionProbeAtlasMips; ++mip) {
        VkBufferImageCopy region{};
        region.bufferOffset = offset;
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mip, 0, 1};
        const u32 side = texture.mipSide(mip);
        region.imageExtent = {side, side, 1};
        regions.push_back(region);
        offset += static_cast<u64>(side) * side * 8;
      }
      vkCmdCopyImageToBuffer(commandBuffer_, scratch, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, probeReadback_.handle(),
                             static_cast<u32>(regions.size()), regions.data());
      VkMemoryBarrier host{};
      host.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
      host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
      vkCmdPipelineBarrier(commandBuffer_, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &host, 0, nullptr, 0, nullptr);
      probeReadbackResult_ = {};
      probeReadbackResult_.id = job.request.id;
      probeReadbackResult_.kind = 0;
      probeReadbackResult_.texture = std::move(texture);
      probeReadbackPending_ = true;
    }
  }
  job = {};
}

// Binding 3 do conjunto do quadro: o reflexo do céu quando pronto, senão o mapa
// especular do ambiente (empacotado ou HDRI autoral). Troca rara (o céu fica
// pronto, a cena troca de céu), por isso a espera do device, como na troca de HDRI.
bool InstancedRenderer::bindEnvironmentSpecular() {
  if (environmentSet_ == VK_NULL_HANDLE) return true;
  const bool sky = skyReflectionActive();
  const VkImageView view = sky ? skyReflection_.view() : dirtRoadResources_.environmentSpecularView();
  if (view == environmentSpecularBound_) return true;
  if (vkDeviceWaitIdle(device_) != VK_SUCCESS) return false;
  const VkDescriptorImageInfo image{sky ? probeAtlasSampler_.handle() : dirtRoadResources_.environmentSpecularSampler(), view,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = environmentSet_;
  write.dstBinding = 3;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  write.pImageInfo = &image;
  vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
  environmentSpecularBound_ = view;
  editorSceneCacheValid_ = false;
  __android_log_print(ANDROID_LOG_INFO, LogTag, "[SkyReflection] reflexo do ambiente: %s.",
                      sky ? "céu da cena (capturado, GGX)" : "mapa do ambiente");
  return true;
}

bool InstancedRenderer::ensureProbeReadback(u64 bytes) {
  if (probeReadback_.isReady() && probeReadback_.sizeBytes() >= bytes) return true;
  probeReadback_.reset();
  rhi::BufferDesc desc{};
  desc.sizeBytes = bytes;
  desc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  desc.cpuAccess = rhi::CpuAccess::Random;
  desc.preferDeviceMemory = false;
  return memoryAllocator_->createBuffer(desc, &probeReadback_) && probeReadback_.mappedData();
}
