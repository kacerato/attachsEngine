// Ampliação temporal por biblioteca (G6-B): Arm ASR e AMD FSR 2.
//
// Ordem do quadro com um ampliador ativo, a mesma de Unity (STP) e Unreal
// (TSR): cena com jitter -> vetor/máscaras -> exposição automática ->
// COMPOSIÇÃO (névoa e AO em HDR linear, na resolução interna, alinhadas à
// profundidade que as produziu) -> biblioteca -> pós de EXIBIÇÃO na resolução
// final (bloom, tonemap, grade, vinheta) -> interface. Nada disso existe com o
// TAA nativo, que continua no pós.

void InstancedRenderer::probeTemporalUpscalers(const rhi::VulkanDevice &device,
                                               renderer::TemporalUpscalerAvailability &armAsr,
                                               renderer::TemporalUpscalerAvailability &fsr2) {
  const auto &features=device.temporalUpscalerFeatures();
  renderer::TemporalUpscalerProbe probe{};
  probe.armAsrBuilt=rhi::TemporalUpscaler::compiledIn(rhi::TemporalUpscalerBackend::ArmAsr);
  probe.fsr2Built=rhi::TemporalUpscaler::compiledIn(rhi::TemporalUpscalerBackend::Fsr2);
  probe.shaderFloat16=features.shaderFloat16;
  probe.shaderInt16=features.shaderInt16;
  probe.storageImageExtendedFormats=features.storageImageExtendedFormats;
  probe.storageImageWriteWithoutFormat=features.storageImageWriteWithoutFormat;
  probe.computeSubgroupBasic=features.computeSubgroupBasic;
  probe.computeSubgroupQuad=features.computeSubgroupQuad;
  probe.hdrSceneColor=formatSupportsHdrSceneColor(device.physicalDevice(),VK_FORMAT_R16G16B16A16_SFLOAT);
  // A cena escolhe D32_SFLOAT primeiro quando a profundidade é amostrada; o
  // Arm ASR cria a própria vista D32 dessa imagem.
  probe.sampledDepth32=formatSupportsDepthAttachment(device.physicalDevice(),VK_FORMAT_D32_SFLOAT,true);
  armAsr=renderer::probeArmAsr(probe);
  fsr2=renderer::probeFsr2(probe);
}

namespace {
rhi::TemporalUpscalerBackend backendFor(renderer::UpscalingFilter filter) {
  return filter==renderer::UpscalingFilter::Fsr2?rhi::TemporalUpscalerBackend::Fsr2:
                                                 rhi::TemporalUpscalerBackend::ArmAsr;
}
void imageBarrier(VkCommandBuffer commandBuffer,VkImage image,VkImageAspectFlags aspect,
                  VkImageLayout oldLayout,VkImageLayout newLayout,
                  VkPipelineStageFlags sourceStage,VkAccessFlags sourceAccess,
                  VkPipelineStageFlags destinationStage,VkAccessFlags destinationAccess) {
  VkImageMemoryBarrier barrier{};
  barrier.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.srcAccessMask=sourceAccess;barrier.dstAccessMask=destinationAccess;
  barrier.oldLayout=oldLayout;barrier.newLayout=newLayout;
  barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
  barrier.image=image;barrier.subresourceRange={aspect,0,1,0,1};
  vkCmdPipelineBarrier(commandBuffer,sourceStage,destinationStage,0,0,nullptr,0,nullptr,1,&barrier);
}
} // namespace

bool InstancedRenderer::createTemporalUpscalerResources() {
  if(!renderer::isTemporalUpscaler(temporalUpscalerRequested_)) return true;
  const auto backend=backendFor(temporalUpscalerRequested_);
  const auto refuse=[&](renderer::TemporalUpscalerAvailability reason,const char *what) {
    executedUpscalerStatus_=reason;
    temporalUpscalerDiagnostic_=what;
    __android_log_print(ANDROID_LOG_WARN,LogTag,"[TemporalUpscaler] %s recusado: %s",
                        renderer::upscalingFilterName(temporalUpscalerRequested_),what);
    destroyTemporalUpscalerResources();
    return false;
  };
  if(!motionVectorsActive_ || !postPipeline_ || !postDescriptorPool_)
    return refuse(renderer::TemporalUpscalerAvailability::ContextCreationFailed,
                  "entradas temporais indisponíveis (vetor ou pós)");
  if(backend==rhi::TemporalUpscalerBackend::ArmAsr && depthFormat_!=VK_FORMAT_D32_SFLOAT)
    return refuse(renderer::TemporalUpscalerAvailability::MissingSampledDepth32,
                  "profundidade da cena não é D32_SFLOAT");

  rhi::ImageDesc image{};
  image.width=renderTargetWidth();image.height=renderTargetHeight();
  image.format=VK_FORMAT_R16G16B16A16_SFLOAT;
  image.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
  image.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
  image.memoryClass=rhi::MemoryClass::RenderTarget;
  if(!memoryAllocator_->createImage(image,&compositeColor_))
    return refuse(renderer::TemporalUpscalerAvailability::ContextCreationFailed,"cor composta não alocada");
  image.width=swapchain_->width();image.height=swapchain_->height();
  image.usage=rhi::TemporalUpscaler::outputUsage(backend);
  if(!memoryAllocator_->createImage(image,&upscaledColor_))
    return refuse(renderer::TemporalUpscalerAvailability::ContextCreationFailed,"saída ampliada não alocada");
  image.width=image.height=1;image.format=VK_FORMAT_R32_SFLOAT;
  image.usage=VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
  if(!memoryAllocator_->createImage(image,&exposureImage_))
    return refuse(renderer::TemporalUpscalerAvailability::ContextCreationFailed,"exposição 1x1 não alocada");
  upscaledColorLayout_=VK_IMAGE_LAYOUT_UNDEFINED;
  exposureImageInitialized_=false;

  // --- Composição: o shader do pós em modo HDR linear, alvo RGBA16F --------
  VkAttachmentDescription attachment{};
  attachment.format=VK_FORMAT_R16G16B16A16_SFLOAT;attachment.samples=VK_SAMPLE_COUNT_1_BIT;
  attachment.loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
  attachment.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachment.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachment.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;attachment.finalLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  VkAttachmentReference reference{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  VkSubpassDescription subpass{};subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount=1;subpass.pColorAttachments=&reference;
  VkSubpassDependency dependencies[2]{};
  dependencies[0].srcSubpass=VK_SUBPASS_EXTERNAL;dependencies[0].dstSubpass=0;
  dependencies[0].srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|
                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  dependencies[0].srcAccessMask=VK_ACCESS_SHADER_READ_BIT;
  dependencies[0].dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[0].dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[1].srcSubpass=0;dependencies[1].dstSubpass=VK_SUBPASS_EXTERNAL;
  dependencies[1].srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[1].srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[1].dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
  dependencies[1].dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
  VkRenderPassCreateInfo renderPass{};renderPass.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPass.attachmentCount=1;renderPass.pAttachments=&attachment;
  renderPass.subpassCount=1;renderPass.pSubpasses=&subpass;
  renderPass.dependencyCount=2;renderPass.pDependencies=dependencies;
  if(vkCreateRenderPass(device_,&renderPass,nullptr,&compositeRenderPass_)!=VK_SUCCESS)
    return refuse(renderer::TemporalUpscalerAvailability::ContextCreationFailed,"passe de composição");
  const VkImageView compositeView=compositeColor_.view();
  VkFramebufferCreateInfo framebuffer{};framebuffer.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebuffer.renderPass=compositeRenderPass_;framebuffer.attachmentCount=1;framebuffer.pAttachments=&compositeView;
  framebuffer.width=renderTargetWidth();framebuffer.height=renderTargetHeight();framebuffer.layers=1;
  if(vkCreateFramebuffer(device_,&framebuffer,nullptr,&compositeFramebuffer_)!=VK_SUCCESS)
    return refuse(renderer::TemporalUpscalerAvailability::ContextCreationFailed,"framebuffer de composição");
  VkShaderModule vert=createShaderModule(device_,rhi::shaders::kPost_ProcessVertSpirv,rhi::shaders::kPost_ProcessVertSpirvSize);
  VkShaderModule frag=createShaderModule(device_,rhi::shaders::kPost_ProcessFragSpirv,rhi::shaders::kPost_ProcessFragSpirvSize);
  bool pipelineReady=vert&&frag;
  if(pipelineReady) {
    VkPipelineShaderStageCreateInfo stages[]{
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_VERTEX_BIT,vert,"main",nullptr},
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_FRAGMENT_BIT,frag,"main",nullptr}};
    VkPipelineVertexInputStateCreateInfo vertex{};vertex.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VkPipelineInputAssemblyStateCreateInfo assembly{};assembly.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo viewport{};viewport.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount=1;viewport.scissorCount=1;
    VkPipelineRasterizationStateCreateInfo raster{};raster.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode=VK_POLYGON_MODE_FILL;raster.cullMode=VK_CULL_MODE_NONE;raster.lineWidth=1;
    VkPipelineMultisampleStateCreateInfo multisample{};multisample.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo depth{};depth.sType=VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo colorBlend{};colorBlend.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount=1;colorBlend.pAttachments=&blend;
    const VkDynamicState dynamicStates[]{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{};dynamic.sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount=2;dynamic.pDynamicStates=dynamicStates;
    VkGraphicsPipelineCreateInfo pipeline{};pipeline.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline.stageCount=2;pipeline.pStages=stages;pipeline.pVertexInputState=&vertex;
    pipeline.pInputAssemblyState=&assembly;pipeline.pViewportState=&viewport;pipeline.pRasterizationState=&raster;
    pipeline.pMultisampleState=&multisample;pipeline.pDepthStencilState=&depth;pipeline.pColorBlendState=&colorBlend;
    pipeline.pDynamicState=&dynamic;pipeline.layout=postPipelineLayout_;pipeline.renderPass=compositeRenderPass_;
    pipelineReady=vkCreateGraphicsPipelines(device_,VK_NULL_HANDLE,1,&pipeline,nullptr,&compositePipeline_)==VK_SUCCESS;
  }
  if(vert) vkDestroyShaderModule(device_,vert,nullptr);
  if(frag) vkDestroyShaderModule(device_,frag,nullptr);
  if(!pipelineReady) return refuse(renderer::TemporalUpscalerAvailability::ContextCreationFailed,"pipeline de composição");

  // --- Conjunto do pós de exibição: a cor de entrada é a saída ampliada -----
  VkDescriptorSetAllocateInfo allocate{};allocate.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocate.descriptorPool=postDescriptorPool_;allocate.descriptorSetCount=1;allocate.pSetLayouts=&postSetLayout_;
  if(vkAllocateDescriptorSets(device_,&allocate,&upscaledDescriptorSet_)!=VK_SUCCESS)
    return refuse(renderer::TemporalUpscalerAvailability::ContextCreationFailed,"descritores de exibição");
  {
    std::array<VkCopyDescriptorSet,8> copies{};
    for(u32 binding=0;binding<8;++binding) {
      copies[binding].sType=VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
      copies[binding].srcSet=postDescriptorSet_;copies[binding].dstSet=upscaledDescriptorSet_;
      copies[binding].srcBinding=copies[binding].dstBinding=binding;copies[binding].descriptorCount=1;
    }
    vkUpdateDescriptorSets(device_,0,nullptr,static_cast<u32>(copies.size()),copies.data());
    const VkDescriptorImageInfo upscaled{postSampler_.handle(),upscaledColor_.view(),
                                         rhi::TemporalUpscaler::outputLayout(backend)};
    VkWriteDescriptorSet writes[2]{};
    for(u32 binding=0;binding<2;++binding) {
      writes[binding].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[binding].dstSet=upscaledDescriptorSet_;
      writes[binding].dstBinding=binding;writes[binding].descriptorCount=1;
      writes[binding].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;writes[binding].pImageInfo=&upscaled;
    }
    vkUpdateDescriptorSets(device_,2,writes,0,nullptr);
  }

  // --- Exposição 1x1 ---------------------------------------------------------
  VkDescriptorSetLayoutBinding exposureBindings[3]{};
  exposureBindings[0]={0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr};
  exposureBindings[1]={1,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr};
  exposureBindings[2]={2,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr};
  VkDescriptorSetLayoutCreateInfo exposureLayout{};exposureLayout.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  exposureLayout.bindingCount=3;exposureLayout.pBindings=exposureBindings;
  if(vkCreateDescriptorSetLayout(device_,&exposureLayout,nullptr,&exposureExportSetLayout_)!=VK_SUCCESS)
    return refuse(renderer::TemporalUpscalerAvailability::ContextCreationFailed,"layout da exposição");
  VkDescriptorPoolSize exposureSizes[]{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1},
                                       {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1}};
  VkDescriptorPoolCreateInfo exposurePool{};exposurePool.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  exposurePool.maxSets=1;exposurePool.poolSizeCount=3;exposurePool.pPoolSizes=exposureSizes;
  if(vkCreateDescriptorPool(device_,&exposurePool,nullptr,&exposureExportPool_)!=VK_SUCCESS)
    return refuse(renderer::TemporalUpscalerAvailability::ContextCreationFailed,"pool da exposição");
  allocate.descriptorPool=exposureExportPool_;allocate.pSetLayouts=&exposureExportSetLayout_;
  if(vkAllocateDescriptorSets(device_,&allocate,&exposureExportSet_)!=VK_SUCCESS)
    return refuse(renderer::TemporalUpscalerAvailability::ContextCreationFailed,"conjunto da exposição");
  {
    const VkDescriptorBufferInfo state{autoExposureViews_[0].state.handle(),0,16};
    const VkDescriptorBufferInfo frame{environmentUniform_.handle(),0,sizeof(DirtRoadFrameUniform)};
    const VkDescriptorImageInfo target{VK_NULL_HANDLE,exposureImage_.view(),VK_IMAGE_LAYOUT_GENERAL};
    VkWriteDescriptorSet writes[3]{};
    for(u32 binding=0;binding<3;++binding) {
      writes[binding].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[binding].dstSet=exposureExportSet_;
      writes[binding].dstBinding=binding;writes[binding].descriptorCount=1;
    }
    writes[0].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;writes[0].pBufferInfo=&state;
    writes[1].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;writes[1].pBufferInfo=&frame;
    writes[2].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;writes[2].pImageInfo=&target;
    vkUpdateDescriptorSets(device_,3,writes,0,nullptr);
  }
  VkPipelineLayoutCreateInfo exposurePipelineLayout{};exposurePipelineLayout.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  exposurePipelineLayout.setLayoutCount=1;exposurePipelineLayout.pSetLayouts=&exposureExportSetLayout_;
  if(vkCreatePipelineLayout(device_,&exposurePipelineLayout,nullptr,&exposureExportLayout_)!=VK_SUCCESS)
    return refuse(renderer::TemporalUpscalerAvailability::ContextCreationFailed,"pipeline layout da exposição");
  VkShaderModule exposureModule=createShaderModule(device_,rhi::shaders::kTemporal_ExposureCompSpirv,
                                                   rhi::shaders::kTemporal_ExposureCompSpirvSize);
  bool exposureReady=exposureModule!=VK_NULL_HANDLE;
  if(exposureReady) {
    VkComputePipelineCreateInfo compute{};compute.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    compute.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_COMPUTE_BIT,
                   exposureModule,"main",nullptr};
    compute.layout=exposureExportLayout_;
    exposureReady=vkCreateComputePipelines(device_,VK_NULL_HANDLE,1,&compute,nullptr,&exposureExportPipeline_)==VK_SUCCESS;
    vkDestroyShaderModule(device_,exposureModule,nullptr);
  }
  if(!exposureReady) return refuse(renderer::TemporalUpscalerAvailability::ContextCreationFailed,"pipeline da exposição");

  // --- Contexto da biblioteca ------------------------------------------------
  rhi::TemporalUpscalerContextDesc description{};
  description.backend=backend;
  description.maxRenderWidth=renderTargetWidth();description.maxRenderHeight=renderTargetHeight();
  description.displayWidth=swapchain_->width();description.displayHeight=swapchain_->height();
  description.shaderQuality=renderingPolicy_.post.temporalUpscalerQuality==renderer::TemporalUpscalerQuality::Balanced?1u:
      renderingPolicy_.post.temporalUpscalerQuality==renderer::TemporalUpscalerQuality::Performance?2u:
      renderingPolicy_.post.temporalUpscalerQuality==renderer::TemporalUpscalerQuality::UltraPerformance?3u:0u;
  description.dynamicResolution=renderingPolicy_.dynamicResolution.enabled;
  std::string diagnostic;
  if(!temporalUpscaler_.create(rhiDevice_->instance(),device_,physicalDevice_,vkGetDeviceProcAddr,description,diagnostic))
    return refuse(renderer::TemporalUpscalerAvailability::ContextCreationFailed,diagnostic.c_str());
  executedUpscalerStatus_=renderer::TemporalUpscalerAvailability::Available;
  temporalUpscalerDiagnostic_.clear();
  temporalPreviousTime_=-1.0f;
  __android_log_print(ANDROID_LOG_INFO,LogTag,
      "[TemporalUpscaler] %s pronto: render %ux%u -> %ux%u, preset %u, resolução dinâmica %s.",
      renderer::upscalingFilterName(temporalUpscalerRequested_),renderTargetWidth(),renderTargetHeight(),
      swapchain_->width(),swapchain_->height(),description.shaderQuality,
      description.dynamicResolution?"sim":"não");
  return true;
}

void InstancedRenderer::destroyTemporalUpscalerResources() {
  temporalUpscaler_.destroy();
  if(device_!=VK_NULL_HANDLE) {
    if(exposureExportPipeline_) vkDestroyPipeline(device_,exposureExportPipeline_,nullptr);
    if(exposureExportLayout_) vkDestroyPipelineLayout(device_,exposureExportLayout_,nullptr);
    if(exposureExportPool_) vkDestroyDescriptorPool(device_,exposureExportPool_,nullptr);
    if(exposureExportSetLayout_) vkDestroyDescriptorSetLayout(device_,exposureExportSetLayout_,nullptr);
    if(compositePipeline_) vkDestroyPipeline(device_,compositePipeline_,nullptr);
    if(compositeFramebuffer_) vkDestroyFramebuffer(device_,compositeFramebuffer_,nullptr);
    if(compositeRenderPass_) vkDestroyRenderPass(device_,compositeRenderPass_,nullptr);
  }
  exposureExportPipeline_=VK_NULL_HANDLE;exposureExportLayout_=VK_NULL_HANDLE;
  exposureExportPool_=VK_NULL_HANDLE;exposureExportSet_=VK_NULL_HANDLE;exposureExportSetLayout_=VK_NULL_HANDLE;
  compositePipeline_=VK_NULL_HANDLE;compositeFramebuffer_=VK_NULL_HANDLE;compositeRenderPass_=VK_NULL_HANDLE;
  // O conjunto de exibição pertence ao pool do pós, destruído junto com ele.
  upscaledDescriptorSet_=VK_NULL_HANDLE;
  compositeColor_.reset();upscaledColor_.reset();exposureImage_.reset();
  upscaledColorLayout_=VK_IMAGE_LAYOUT_UNDEFINED;exposureImageInitialized_=false;
}

bool InstancedRenderer::recordTemporalUpscale(const platform::FreeCameraState &camera,float timeSeconds) {
  if(!temporalUpscaler_.ready()) return false;
  const auto backend=temporalUpscaler_.backend();

  // 1) Composição HDR na resolução interna.
  {
    VkRenderPassBeginInfo begin{};begin.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin.renderPass=compositeRenderPass_;begin.framebuffer=compositeFramebuffer_;
    begin.renderArea.extent={renderWidth(),renderHeight()};
    vkCmdBeginRenderPass(commandBuffer_,&begin,VK_SUBPASS_CONTENTS_INLINE);
    const VkViewport viewport{0,0,static_cast<float>(renderWidth()),static_cast<float>(renderHeight()),0,1};
    const VkRect2D scissor{{0,0},{renderWidth(),renderHeight()}};
    vkCmdSetViewport(commandBuffer_,0,1,&viewport);vkCmdSetScissor(commandBuffer_,0,1,&scissor);
    vkCmdBindPipeline(commandBuffer_,VK_PIPELINE_BIND_POINT_GRAPHICS,compositePipeline_);
    vkCmdBindDescriptorSets(commandBuffer_,VK_PIPELINE_BIND_POINT_GRAPHICS,postPipelineLayout_,0,1,
                            &postDescriptorSet_,0,nullptr);
    PostPushConstants push{};
    push.texelFlags[0]=1.0f/static_cast<float>(renderTargetWidth());
    push.texelFlags[1]=1.0f/static_cast<float>(renderTargetHeight());
    push.texelFlags[2]=64.0f; // só composição: névoa + AO, HDR linear
    push.grade[3]=static_cast<float>(packSurfaceTransform(swapchain_->surfaceTransform()));
    push.sourceTransform[0]=static_cast<float>(renderWidth())/static_cast<float>(renderTargetWidth());
    push.sourceTransform[1]=static_cast<float>(renderHeight())/static_cast<float>(renderTargetHeight());
    push.sourceTransform[2]=1.0f/std::tan(sceneFieldOfView()*.5f);
    if(sceneOrthographicHalfHeight_>0) push.sourceTransform[2]=-1.0f;
    push.sourceTransform[3]=sceneAspectRatio();
    push.currentCamera[0]=camera.yaw;push.currentCamera[1]=camera.pitch;
    std::memcpy(push.currentPositionNear,camera.position,sizeof(camera.position));
    push.currentPositionNear[3]=sceneNearPlane();
    push.previousCamera[2]=camera.roll;
    std::memcpy(push.previousPositionFar,camera.position,sizeof(camera.position));
    push.previousPositionFar[3]=sceneFarPlane();
    vkCmdPushConstants(commandBuffer_,postPipelineLayout_,VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(push),&push);
    vkCmdDraw(commandBuffer_,3,1,0,0);
    vkCmdEndRenderPass(commandBuffer_);
  }

  // 2) Exposição 1x1, depois que a adaptação automática escreveu o EV.
  {
    VkMemoryBarrier stateReady{};stateReady.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    stateReady.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;stateReady.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(commandBuffer_,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0,1,&stateReady,0,nullptr,0,nullptr);
    imageBarrier(commandBuffer_,exposureImage_.handle(),VK_IMAGE_ASPECT_COLOR_BIT,
                 exposureImageInitialized_?VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED,
                 VK_IMAGE_LAYOUT_GENERAL,
                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                 exposureImageInitialized_?VK_ACCESS_SHADER_READ_BIT:0u,
                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_WRITE_BIT);
    vkCmdBindPipeline(commandBuffer_,VK_PIPELINE_BIND_POINT_COMPUTE,exposureExportPipeline_);
    vkCmdBindDescriptorSets(commandBuffer_,VK_PIPELINE_BIND_POINT_COMPUTE,exposureExportLayout_,0,1,
                            &exposureExportSet_,0,nullptr);
    vkCmdDispatch(commandBuffer_,1,1,1);
    imageBarrier(commandBuffer_,exposureImage_.handle(),VK_IMAGE_ASPECT_COLOR_BIT,
                 VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_WRITE_BIT,
                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_READ_BIT);
    exposureImageInitialized_=true;
  }

  // 3) Layouts que cada biblioteca espera. O FSR 2 lê a profundidade com a
  //    vista que recebe, em SHADER_READ_ONLY; o Arm ASR a trata sozinho em
  //    DEPTH_STENCIL_READ_ONLY. A saída nasce no layout de retorno do backend.
  const VkImageAspectFlags depthAspect=VK_IMAGE_ASPECT_DEPTH_BIT;
  if(backend==rhi::TemporalUpscalerBackend::Fsr2)
    imageBarrier(commandBuffer_,depthImage_.handle(),depthAspect,
                 VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                 VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT|
                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_SHADER_READ_BIT,
                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_READ_BIT);
  const VkImageLayout outputLayout=rhi::TemporalUpscaler::outputLayout(backend);
  if(upscaledColorLayout_!=outputLayout) {
    imageBarrier(commandBuffer_,upscaledColor_.handle(),VK_IMAGE_ASPECT_COLOR_BIT,
                 VK_IMAGE_LAYOUT_UNDEFINED,outputLayout,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,0,
                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                 VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT);
    upscaledColorLayout_=outputLayout;
  } else {
    // A leitura do pós no quadro anterior precisa terminar antes da escrita.
    VkMemoryBarrier previousRead{};previousRead.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    previousRead.srcAccessMask=VK_ACCESS_SHADER_READ_BIT;
    previousRead.dstAccessMask=VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    vkCmdPipelineBarrier(commandBuffer_,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         0,1,&previousRead,0,nullptr,0,nullptr);
  }

  // 4) A biblioteca.
  const auto input=[](const rhi::VulkanImage &image,VkFormat format,u32 width,u32 height) {
    return rhi::TemporalUpscalerImage{image.handle(),image.view(),format,width,height};
  };
  rhi::TemporalUpscalerDispatch dispatch{};
  dispatch.commandBuffer=commandBuffer_;
  dispatch.color=input(compositeColor_,VK_FORMAT_R16G16B16A16_SFLOAT,renderTargetWidth(),renderTargetHeight());
  dispatch.depth=input(depthImage_,depthFormat_,renderTargetWidth(),renderTargetHeight());
  dispatch.velocity=input(motionImage_,VK_FORMAT_R16G16_SFLOAT,renderTargetWidth(),renderTargetHeight());
  if(temporalMasksActive_) {
    dispatch.reactive=input(reactiveMaskImage_,VK_FORMAT_R8_UNORM,renderTargetWidth(),renderTargetHeight());
    dispatch.composition=input(compositionMaskImage_,VK_FORMAT_R8_UNORM,renderTargetWidth(),renderTargetHeight());
  }
  dispatch.exposure=input(exposureImage_,VK_FORMAT_R32_SFLOAT,1,1);
  dispatch.output=input(upscaledColor_,VK_FORMAT_R16G16B16A16_SFLOAT,swapchain_->width(),swapchain_->height());
  dispatch.renderWidth=renderWidth();dispatch.renderHeight=renderHeight();
  dispatch.jitterX=temporalUpscalerJitterPixels_[0];dispatch.jitterY=temporalUpscalerJitterPixels_[1];
  // O vetor está em UV da extensão renderizada; as bibliotecas querem pixels.
  dispatch.motionScaleX=static_cast<float>(renderWidth());
  dispatch.motionScaleY=static_cast<float>(renderHeight());
  const float delta=temporalPreviousTime_>=0.0f?(timeSeconds-temporalPreviousTime_)*1000.0f:16.67f;
  dispatch.frameTimeDeltaMs=std::clamp(delta,0.1f,100.0f);
  temporalPreviousTime_=timeSeconds;
  const bool effects=!editorBackground_||(editorSceneEffects_&&editorScenePost_);
  dispatch.sharpness=effects?std::clamp(renderingPolicy_.post.sharpen,0.0f,1.0f):0.0f;
  dispatch.sharpen=dispatch.sharpness>0.0f;
  dispatch.reset=!temporalHistoryInitialized_;
  dispatch.cameraNear=sceneNearPlane();dispatch.cameraFar=sceneFarPlane();
  dispatch.cameraFovVertical=sceneFieldOfView();
  std::string diagnostic;
  const bool dispatched=temporalUpscaler_.dispatch(dispatch,diagnostic);

  if(backend==rhi::TemporalUpscalerBackend::Fsr2)
    imageBarrier(commandBuffer_,depthImage_.handle(),depthAspect,
                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_READ_BIT,
                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                 VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT);
  VkMemoryBarrier outputReady{};outputReady.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER;
  outputReady.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  outputReady.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(commandBuffer_,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,1,&outputReady,0,nullptr,0,nullptr);
  if(!dispatched) {
    executedUpscalerStatus_=renderer::TemporalUpscalerAvailability::ContextCreationFailed;
    if(temporalUpscalerDiagnostic_!=diagnostic)
      __android_log_print(ANDROID_LOG_ERROR,LogTag,"[TemporalUpscaler] %s",diagnostic.c_str());
    temporalUpscalerDiagnostic_=diagnostic;
    temporalHistoryInitialized_=false;
    return false;
  }
  // O histórico agora é da biblioteca; a pose e o jitter deste quadro viram
  // o "anterior" do próximo, exatamente como no TAA nativo.
  temporalPreviousCamera_=camera;
  temporalPreviousJitter_[0]=temporalCurrentJitter_[0];
  temporalPreviousJitter_[1]=temporalCurrentJitter_[1];
  temporalHistoryInitialized_=true;
  ++temporalFrameIndex_;
  executedUpscalerStatus_=renderer::TemporalUpscalerAvailability::Available;
  return true;
}
