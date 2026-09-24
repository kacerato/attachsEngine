// Entradas temporais do renderer (G6-B): máscaras R8 escritas pelo passe
// principal e o vetor 2D RG16F deste passe. Convenção única em
// rhi/shaders/temporal_projection.glsl; TAA nativo, Arm ASR e AMD FSR 2 leem
// exatamente estes recursos.

namespace {
bool formatSupportsColorTarget(VkPhysicalDevice physicalDevice, VkFormat format) {
  VkFormatProperties properties{};
  vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);
  const VkFormatFeatureFlags required = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
                                        VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT |
                                        VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
  return (properties.optimalTilingFeatures & required) == required;
}
} // namespace

bool InstancedRenderer::createTemporalInputResources() {
  temporalMasksActive_ = false;
  if(!temporalInputsActive_) return true;
  if(!rhiDevice_->deviceFeatures().independentBlend) {
    __android_log_print(ANDROID_LOG_WARN,LogTag,
        "[Temporal] sem independentBlend: máscaras R8 desligadas; reatividade usa a cobertura do alpha da cena.");
    return true;
  }
  // Blend MAX nas duas máscaras: o anexo precisa aceitar blend, não só escrita.
  if(!formatSupportsColorTarget(physicalDevice_,VK_FORMAT_R8_UNORM)) {
    __android_log_print(ANDROID_LOG_WARN,LogTag,
        "[Temporal] R8 sem blend neste aparelho; reatividade usa a cobertura do alpha da cena.");
    return true;
  }
  rhi::ImageDesc image{};
  image.width=renderTargetWidth();image.height=renderTargetHeight();
  image.format=VK_FORMAT_R8_UNORM;
  image.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
  image.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
  image.memoryClass=rhi::MemoryClass::RenderTarget;
  if(!memoryAllocator_->createImage(image,&reactiveMaskImage_) ||
     !memoryAllocator_->createImage(image,&compositionMaskImage_)) {
    destroyTemporalInputResources();
    return false;
  }
  temporalMasksActive_=true;
  return true;
}

void InstancedRenderer::destroyTemporalInputResources() {
  reactiveMaskImage_.reset();compositionMaskImage_.reset();
  temporalMasksActive_=false;
}

bool InstancedRenderer::createMotionResources() {
  motionVectorsActive_=false;
  if(!temporalInputsActive_ || !dirtRoadPreview_) return true;
  if(!formatSupportsColorTarget(physicalDevice_,VK_FORMAT_R16G16_SFLOAT)) {
    __android_log_print(ANDROID_LOG_WARN,LogTag,"[Temporal] RG16F indisponível; vetor de movimento desligado.");
    return true;
  }

  rhi::ImageDesc image{};
  image.width=renderTargetWidth();image.height=renderTargetHeight();
  image.format=VK_FORMAT_R16G16_SFLOAT;
  image.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
  image.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
  image.memoryClass=rhi::MemoryClass::RenderTarget;
  if(!memoryAllocator_->createImage(image,&motionImage_)) return false;

  VkAttachmentDescription attachments[2]{};
  attachments[0].format=image.format;
  attachments[0].samples=VK_SAMPLE_COUNT_1_BIT;
  // O passe de câmera cobre toda a vista; fora dela o clear diz "parado".
  attachments[0].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachments[0].storeOp=VK_ATTACHMENT_STORE_OP_STORE;
  attachments[0].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
  attachments[0].finalLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  attachments[1].format=depthFormat_;
  attachments[1].samples=VK_SAMPLE_COUNT_1_BIT;
  attachments[1].loadOp=VK_ATTACHMENT_LOAD_OP_LOAD;
  attachments[1].storeOp=VK_ATTACHMENT_STORE_OP_STORE;
  attachments[1].initialLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  attachments[1].finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  VkAttachmentReference color{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  // Somente leitura: a mesma imagem é amostrada pelo passe de câmera e testada
  // (EQUAL) pelo passe por objeto, sem laço de realimentação.
  VkAttachmentReference depth{1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount=1;subpass.pColorAttachments=&color;
  subpass.pDepthStencilAttachment=&depth;
  VkSubpassDependency dependencies[2]{};
  dependencies[0].srcSubpass=VK_SUBPASS_EXTERNAL;dependencies[0].dstSubpass=0;
  dependencies[0].srcStageMask=VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  dependencies[0].srcAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  dependencies[0].dstStageMask=VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|
                                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[0].dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|
                                 VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[1].srcSubpass=0;dependencies[1].dstSubpass=VK_SUBPASS_EXTERNAL;
  dependencies[1].srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[1].srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  // Consumidores: pós (fragmento), Arm ASR (fragmento) e FSR 2 (compute).
  dependencies[1].dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
  dependencies[1].dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
  VkRenderPassCreateInfo pass{};pass.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  pass.attachmentCount=2;pass.pAttachments=attachments;
  pass.subpassCount=1;pass.pSubpasses=&subpass;
  pass.dependencyCount=2;pass.pDependencies=dependencies;
  if(vkCreateRenderPass(device_,&pass,nullptr,&motionRenderPass_)!=VK_SUCCESS) return false;

  VkImageView views[]{motionImage_.view(),depthImage_.view()};
  VkFramebufferCreateInfo framebuffer{};
  framebuffer.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebuffer.renderPass=motionRenderPass_;
  framebuffer.attachmentCount=2;framebuffer.pAttachments=views;
  framebuffer.width=renderTargetWidth();framebuffer.height=renderTargetHeight();framebuffer.layers=1;
  if(vkCreateFramebuffer(device_,&framebuffer,nullptr,&motionFramebuffer_)!=VK_SUCCESS) return false;

  // --- Passe de câmera: profundidade + UBO do quadro ------------------------
  VkDescriptorSetLayoutBinding cameraBindings[2]{};
  cameraBindings[0]={0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr};
  cameraBindings[1]={1,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr};
  VkDescriptorSetLayoutCreateInfo cameraLayout{};
  cameraLayout.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  cameraLayout.bindingCount=2;cameraLayout.pBindings=cameraBindings;
  if(vkCreateDescriptorSetLayout(device_,&cameraLayout,nullptr,&motionCameraSetLayout_)!=VK_SUCCESS) return false;
  VkDescriptorPoolSize cameraSizes[]{{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1}};
  VkDescriptorPoolCreateInfo cameraPool{};cameraPool.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  cameraPool.maxSets=1;cameraPool.poolSizeCount=2;cameraPool.pPoolSizes=cameraSizes;
  if(vkCreateDescriptorPool(device_,&cameraPool,nullptr,&motionCameraPool_)!=VK_SUCCESS) return false;
  VkDescriptorSetAllocateInfo cameraAllocate{};cameraAllocate.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  cameraAllocate.descriptorPool=motionCameraPool_;cameraAllocate.descriptorSetCount=1;
  cameraAllocate.pSetLayouts=&motionCameraSetLayout_;
  if(vkAllocateDescriptorSets(device_,&cameraAllocate,&motionCameraSet_)!=VK_SUCCESS) return false;
  rhi::SamplerDesc nearest{};
  nearest.addressU=nearest.addressV=nearest.addressW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  nearest.minFilter=nearest.magFilter=VK_FILTER_NEAREST;
  if(!motionDepthSampler_.initialize(device_,nearest)) return false;
  VkDescriptorImageInfo depthInfo{motionDepthSampler_.handle(),depthImage_.view(),
                                  VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
  VkDescriptorBufferInfo frameInfo{environmentUniform_.handle(),0,sizeof(DirtRoadFrameUniform)};
  VkWriteDescriptorSet cameraWrites[2]{};
  cameraWrites[0].sType=cameraWrites[1].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  cameraWrites[0].dstSet=cameraWrites[1].dstSet=motionCameraSet_;
  cameraWrites[0].dstBinding=0;cameraWrites[0].descriptorCount=1;
  cameraWrites[0].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;cameraWrites[0].pImageInfo=&depthInfo;
  cameraWrites[1].dstBinding=1;cameraWrites[1].descriptorCount=1;
  cameraWrites[1].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;cameraWrites[1].pBufferInfo=&frameInfo;
  vkUpdateDescriptorSets(device_,2,cameraWrites,0,nullptr);
  VkPushConstantRange cameraPush{VK_SHADER_STAGE_FRAGMENT_BIT,0,16};
  VkPipelineLayoutCreateInfo cameraPipelineLayout{};
  cameraPipelineLayout.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  cameraPipelineLayout.setLayoutCount=1;cameraPipelineLayout.pSetLayouts=&motionCameraSetLayout_;
  cameraPipelineLayout.pushConstantRangeCount=1;cameraPipelineLayout.pPushConstantRanges=&cameraPush;
  if(vkCreatePipelineLayout(device_,&cameraPipelineLayout,nullptr,&motionCameraLayout_)!=VK_SUCCESS) return false;

  VkShaderModule vert=createShaderModule(device_,rhi::shaders::kDirt_RoadVertSpirv,
                                          rhi::shaders::kDirt_RoadVertSpirvSize);
  VkShaderModule frag=useBindless_
      ?createShaderModule(device_,rhi::shaders::kDirt_Road_MotionFragSpirv,
                         rhi::shaders::kDirt_Road_MotionFragSpirvSize)
      :createShaderModule(device_,rhi::shaders::kDirt_Road_Motion_FallbackFragSpirv,
                         rhi::shaders::kDirt_Road_Motion_FallbackFragSpirvSize);
  VkShaderModule fullscreen=createShaderModule(device_,rhi::shaders::kPost_ProcessVertSpirv,
                                               rhi::shaders::kPost_ProcessVertSpirvSize);
  VkShaderModule cameraFrag=createShaderModule(device_,rhi::shaders::kTemporal_Camera_MotionFragSpirv,
                                               rhi::shaders::kTemporal_Camera_MotionFragSpirvSize);
  VkShaderModule skinnedVert=createShaderModule(device_,rhi::shaders::kDirt_Road_Skinned_MotionVertSpirv,
                                                rhi::shaders::kDirt_Road_Skinned_MotionVertSpirvSize);
  const auto releaseModules=[&] {
    for(VkShaderModule module:{vert,frag,fullscreen,cameraFrag,skinnedVert})
      if(module) vkDestroyShaderModule(device_,module,nullptr);
  };
  if(!vert||!frag||!fullscreen||!cameraFrag||!skinnedVert) {releaseModules();return false;}
  VkPipelineShaderStageCreateInfo stages[2]{};
  stages[0].sType=stages[1].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[0].module=vert;stages[0].pName="main";
  stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[1].module=frag;stages[1].pName="main";
  VkVertexInputBindingDescription bindings[2]{{0,dirtRoadResources_.header().vertexStride,VK_VERTEX_INPUT_RATE_VERTEX},
      {1,sizeof(renderer::GpuMeshInstance),VK_VERTEX_INPUT_RATE_INSTANCE}};
  VkVertexInputAttributeDescription attributes[18]{};
  const bool packed=dirtRoadResources_.header().vertexStride==renderer::MapVertexStride;
  attributes[0]={0,0,VK_FORMAT_R32G32B32_SFLOAT,0};
  attributes[1]={1,0,packed?VK_FORMAT_R16G16B16A16_SNORM:VK_FORMAT_R32G32B32_SFLOAT,12};
  attributes[2]={2,0,packed?VK_FORMAT_R16G16B16A16_SNORM:VK_FORMAT_R32G32B32A32_SFLOAT,packed?20u:24u};
  attributes[3]={3,0,VK_FORMAT_R32G32_SFLOAT,packed?28u:40u};
  attributes[4]={4,0,VK_FORMAT_R32G32_SFLOAT,packed?36u:48u};
  attributes[5]={5,0,packed?VK_FORMAT_R8G8B8A8_UNORM:VK_FORMAT_R32G32B32A32_SFLOAT,packed?44u:56u};
  for(u32 i=0;i<5;++i) attributes[6+i]={6+i,1,VK_FORMAT_R32G32B32A32_SFLOAT,i*16u};
  for(u32 i=0;i<3;++i) attributes[11+i]={11+i,1,VK_FORMAT_R32G32B32A32_SFLOAT,80u+i*16u};
  for(u32 i=0;i<4;++i) attributes[14+i]={14+i,1,VK_FORMAT_R32G32B32A32_SFLOAT,128u+i*16u};
  VkPipelineVertexInputStateCreateInfo vertexInput{};
  vertexInput.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInput.vertexBindingDescriptionCount=2;vertexInput.pVertexBindingDescriptions=bindings;
  vertexInput.vertexAttributeDescriptionCount=18;vertexInput.pVertexAttributeDescriptions=attributes;
  VkPipelineInputAssemblyStateCreateInfo assembly{};
  assembly.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  VkPipelineViewportStateCreateInfo viewport{};
  viewport.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport.viewportCount=1;viewport.scissorCount=1;
  VkPipelineRasterizationStateCreateInfo raster{};
  raster.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  raster.polygonMode=VK_POLYGON_MODE_FILL;raster.cullMode=VK_CULL_MODE_NONE;
  raster.frontFace=VK_FRONT_FACE_CLOCKWISE;raster.lineWidth=1;
  VkPipelineMultisampleStateCreateInfo multisample{};
  multisample.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
  VkPipelineDepthStencilStateCreateInfo depthState{};
  depthState.sType=VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthState.depthTestEnable=VK_TRUE;depthState.depthWriteEnable=VK_FALSE;
  depthState.depthCompareOp=VK_COMPARE_OP_EQUAL;
  VkPipelineColorBlendAttachmentState blendAttachment{};
  blendAttachment.colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT;
  VkPipelineColorBlendStateCreateInfo blend{};
  blend.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  blend.attachmentCount=1;blend.pAttachments=&blendAttachment;
  VkDynamicState dynamicStates[]{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic{};
  dynamic.sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic.dynamicStateCount=2;dynamic.pDynamicStates=dynamicStates;
  VkGraphicsPipelineCreateInfo pipeline{};
  pipeline.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline.stageCount=2;pipeline.pStages=stages;
  pipeline.pVertexInputState=&vertexInput;pipeline.pInputAssemblyState=&assembly;
  pipeline.pViewportState=&viewport;pipeline.pRasterizationState=&raster;
  pipeline.pMultisampleState=&multisample;pipeline.pDepthStencilState=&depthState;
  pipeline.pColorBlendState=&blend;pipeline.pDynamicState=&dynamic;
  pipeline.layout=pipelineLayout_;pipeline.renderPass=motionRenderPass_;
  bool ready=vkCreateGraphicsPipelines(device_,VK_NULL_HANDLE,1,&pipeline,nullptr,
                                       &motionPipeline_)==VK_SUCCESS;
  // Variante de skin (G6-B): o atributo 2 (tangente, sem uso no vetor) vem do
  // binding 2 com a posição deformada ANTERIOR, n vértices depois da atual.
  {
    VkVertexInputBindingDescription skinnedBindings[3]{bindings[0],bindings[1],
        {2,renderer::MapVertexStride,VK_VERTEX_INPUT_RATE_VERTEX}};
    VkVertexInputAttributeDescription skinnedAttributes[18];
    std::copy(std::begin(attributes),std::end(attributes),skinnedAttributes);
    skinnedAttributes[2]={2,2,VK_FORMAT_R32G32B32_SFLOAT,0};
    VkPipelineVertexInputStateCreateInfo skinnedInput=vertexInput;
    skinnedInput.vertexBindingDescriptionCount=3;skinnedInput.pVertexBindingDescriptions=skinnedBindings;
    skinnedInput.pVertexAttributeDescriptions=skinnedAttributes;
    VkPipelineShaderStageCreateInfo skinnedStages[2]{stages[0],stages[1]};
    skinnedStages[0].module=skinnedVert;
    VkGraphicsPipelineCreateInfo skinned=pipeline;
    skinned.pStages=skinnedStages;skinned.pVertexInputState=&skinnedInput;
    // Pacote no formato largo (sem skin) não tem este pipeline; o vetor do
    // corpo cai para o rígido, e isso é dito no log.
    if(!packed || vkCreateGraphicsPipelines(device_,VK_NULL_HANDLE,1,&skinned,nullptr,
                                            &motionSkinnedPipeline_)!=VK_SUCCESS) {
      motionSkinnedPipeline_=VK_NULL_HANDLE;
      __android_log_print(ANDROID_LOG_WARN,LogTag,"[Motion] variante de skin indisponível; corpos usam vetor rígido.");
    }
  }
  // Tela cheia, sem teste de profundidade: todo pixel recebe o movimento de câmera.
  VkPipelineVertexInputStateCreateInfo noVertices{};
  noVertices.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  VkPipelineDepthStencilStateCreateInfo noDepth{};
  noDepth.sType=VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  stages[0].module=fullscreen;stages[1].module=cameraFrag;
  pipeline.pVertexInputState=&noVertices;pipeline.pDepthStencilState=&noDepth;
  pipeline.layout=motionCameraLayout_;
  ready=ready&&vkCreateGraphicsPipelines(device_,VK_NULL_HANDLE,1,&pipeline,nullptr,
                                         &motionCameraPipeline_)==VK_SUCCESS;
  releaseModules();
  motionVectorsActive_=ready;
  if(ready) motionDrawIndices_.reserve(instanceCount_);
  return ready;
}

void InstancedRenderer::destroyMotionResources() {
  if(device_!=VK_NULL_HANDLE) {
    if(motionFramebuffer_) vkDestroyFramebuffer(device_,motionFramebuffer_,nullptr);
    if(motionPipeline_) vkDestroyPipeline(device_,motionPipeline_,nullptr);
    if(motionCameraPipeline_) vkDestroyPipeline(device_,motionCameraPipeline_,nullptr);
    if(motionSkinnedPipeline_) vkDestroyPipeline(device_,motionSkinnedPipeline_,nullptr);
    if(motionCameraLayout_) vkDestroyPipelineLayout(device_,motionCameraLayout_,nullptr);
    if(motionCameraPool_) vkDestroyDescriptorPool(device_,motionCameraPool_,nullptr);
    if(motionCameraSetLayout_) vkDestroyDescriptorSetLayout(device_,motionCameraSetLayout_,nullptr);
    if(motionRenderPass_) vkDestroyRenderPass(device_,motionRenderPass_,nullptr);
  }
  motionFramebuffer_=VK_NULL_HANDLE;motionPipeline_=VK_NULL_HANDLE;motionCameraPipeline_=VK_NULL_HANDLE;
  motionSkinnedPipeline_=VK_NULL_HANDLE;
  motionCameraLayout_=VK_NULL_HANDLE;motionCameraPool_=VK_NULL_HANDLE;motionCameraSetLayout_=VK_NULL_HANDLE;
  motionCameraSet_=VK_NULL_HANDLE;motionRenderPass_=VK_NULL_HANDLE;motionImage_.reset();
  motionDepthSampler_.shutdown();
  motionVectorsActive_=false;motionDrawIndices_.clear();
  motionDrawCount_=0;
}

void InstancedRenderer::recordMotionPass(const platform::FreeCameraState &camera,float timeSeconds) {
  if(!motionVectorsActive_) {motionDrawCount_=0;return;}
  VkClearValue clear[2]{};
  VkRenderPassBeginInfo begin{};
  begin.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  begin.renderPass=motionRenderPass_;begin.framebuffer=motionFramebuffer_;
  begin.renderArea.extent={renderWidth(),renderHeight()};
  begin.clearValueCount=2;begin.pClearValues=clear;
  vkCmdBeginRenderPass(commandBuffer_,&begin,VK_SUBPASS_CONTENTS_INLINE);
  // 1) Câmera: toda a extensão renderizada; o shader zera fora da vista.
  {
    const VkViewport full{0,0,static_cast<float>(renderWidth()),static_cast<float>(renderHeight()),0,1};
    const VkRect2D all{{0,0},{renderWidth(),renderHeight()}};
    vkCmdSetViewport(commandBuffer_,0,1,&full);
    vkCmdSetScissor(commandBuffer_,0,1,&all);
    vkCmdBindPipeline(commandBuffer_,VK_PIPELINE_BIND_POINT_GRAPHICS,motionCameraPipeline_);
    vkCmdBindDescriptorSets(commandBuffer_,VK_PIPELINE_BIND_POINT_GRAPHICS,motionCameraLayout_,
                            0,1,&motionCameraSet_,0,nullptr);
    const float extent[4]{static_cast<float>(renderWidth())/static_cast<float>(renderTargetWidth()),
                          static_cast<float>(renderHeight())/static_cast<float>(renderTargetHeight()),0,0};
    vkCmdPushConstants(commandBuffer_,motionCameraLayout_,VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(extent),extent);
    vkCmdDraw(commandBuffer_,3,1,0,0);
  }
  // 2) Objetos que se moveram neste quadro, com a mesma cobertura (EQUAL) do
  //    passe de cor. Sem pose anterior válida o vetor seria inventado: não desenha.
  if(motionDrawCount_!=0 && temporalHistoryInitialized_) {
    const auto area=physicalSceneViewport();
    VkViewport viewport{area.x*renderWidth(),area.y*renderHeight(),
                        area.width*renderWidth(),area.height*renderHeight(),0,1};
    VkRect2D scissor{{static_cast<i32>(std::max(0.0f,std::floor(viewport.x))),
                      static_cast<i32>(std::max(0.0f,std::floor(viewport.y)))},
                     {static_cast<u32>(std::ceil(viewport.width)),
                      static_cast<u32>(std::ceil(viewport.height))}};
    scissor.extent.width=std::min(scissor.extent.width,renderWidth()-scissor.offset.x);
    scissor.extent.height=std::min(scissor.extent.height,renderHeight()-scissor.offset.y);
    vkCmdSetViewport(commandBuffer_,0,1,&viewport);
    vkCmdSetScissor(commandBuffer_,0,1,&scissor);
    vkCmdBindPipeline(commandBuffer_,VK_PIPELINE_BIND_POINT_GRAPHICS,motionPipeline_);
    vkCmdBindDescriptorSets(commandBuffer_,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelineLayout_,
                            1,1,&environmentSet_,0,nullptr);
    if(useBindless_)
      vkCmdBindDescriptorSets(commandBuffer_,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelineLayout_,
                              0,1,&textureSet_,0,nullptr);
    const VkDeviceSize zero=0;
    const VkBuffer instances=instanceBuffer_.handle();
    const VkBuffer geometry=dirtRoadResources_.vertexBuffer();
    vkCmdBindVertexBuffers(commandBuffer_,0,1,&geometry,&zero);
    vkCmdBindVertexBuffers(commandBuffer_,1,1,&instances,&zero);
    vkCmdBindIndexBuffer(commandBuffer_,dirtRoadResources_.indexBuffer(),0,VK_INDEX_TYPE_UINT32);
    const auto &transform=swapchain_->surfaceTransform();
    VkPipeline boundPipeline=motionPipeline_;
    VkBuffer boundGeometry=geometry;
    for(u32 i=0;i<motionDrawCount_;++i) {
      const u32 drawIndex=motionDrawIndices_[i];
      const auto &draw=dirtRoadResources_.draws()[drawIndex];
      if(drawIndex<authoredVisibility_.size()&&!authoredVisibility_[drawIndex]) continue;
      // Skin com vetor próprio: pose anterior do compute no binding 2. Skin sem
      // vetor (desligado pelo autor) só leva o movimento rígido do objeto.
      const auto source=drawGeometry(drawIndex);
      const i32 skinSlot=drawIndex<drawSkinSlot_.size()?drawSkinSlot_[drawIndex]:-1;
      const bool skinnedMotion=skinSlot>=0 && motionSkinnedPipeline_ && skinnedDraws_[static_cast<usize>(skinSlot)].motion;
      const VkPipeline wanted=skinnedMotion?motionSkinnedPipeline_:motionPipeline_;
      if(wanted!=boundPipeline) {vkCmdBindPipeline(commandBuffer_,VK_PIPELINE_BIND_POINT_GRAPHICS,wanted);boundPipeline=wanted;}
      if(source.buffer!=boundGeometry) {vkCmdBindVertexBuffers(commandBuffer_,0,1,&source.buffer,&zero);boundGeometry=source.buffer;}
      if(skinnedMotion) {
        const VkDeviceSize previous=VkDeviceSize(skinnedDraws_[static_cast<usize>(skinSlot)].vertexCount)*renderer::MapVertexStride;
        vkCmdBindVertexBuffers(commandBuffer_,2,1,&source.buffer,&previous);
      }
      auto material=dirtRoadResources_.materials()[draw.materialIndex];
      if(drawIndex<authoredMaterials_.size())
        material=renderer::applyMaterialOverride(material,authoredMaterials_[drawIndex],
                    static_cast<u32>(dirtRoadResources_.packageTextureCount()));
      if(!useBindless_) {
        const VkDescriptorSet set=dirtMaterialSets_[draw.materialIndex];
        vkCmdBindDescriptorSets(commandBuffer_,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelineLayout_,
                                0,1,&set,0,nullptr);
      }
      DirtRoadPushConstants push{};
      push.cameraFrame[0]=sceneAspectRatio();push.cameraFrame[1]=camera.yaw;
      push.cameraFrame[2]=camera.pitch;push.cameraFrame[3]=timeSeconds;
      push.surfaceTransform[0]=transform.xx;push.surfaceTransform[1]=transform.xy;
      push.surfaceTransform[2]=transform.yx;push.surfaceTransform[3]=transform.yy;
      std::copy(camera.position,camera.position+3,push.cameraPositionNear);
      push.cameraPositionNear[3]=sceneNearPlane();
      std::copy(std::begin(material.baseColorFactor),std::end(material.baseColorFactor),push.baseColorFactor);
      std::copy(std::begin(material.emissiveFactorAndStrength),
                std::end(material.emissiveFactorAndStrength),push.emissiveFactorAndStrength);
      for(u32 slot=0;slot<4;++slot) {
        const u32 texture=material.textureIndices[slot];
        push.textureIndices[slot]=useBindless_&&texture!=renderer::InvalidMapTexture&&
            texture<dirtTextureSlots_.size()?dirtTextureSlots_[texture]:baseTextureIndex_;
      }
      push.materialFlags[0]=material.flags;
      const u32 cutoff=static_cast<u32>(std::clamp(material.alphaCutoff,0.0f,1.0f)*255.0f+.5f);
      push.materialFlags[1]=material.textureCoordinates|(cutoff<<8u)|
                            ((material.reserved&0xffffu)<<16u);
      push.materialFlags[2]=(drawIndex<authoredUvTransformEntries_.size()?
          authoredUvTransformEntries_[drawIndex]:0u)<<1u;
      push.materialFlags[3]=std::bit_cast<u32>(sceneFarPlane());
      push.materialFactors[0]=material.roughness;push.materialFactors[1]=material.metallic;
      push.materialFactors[2]=material.normalScale;push.materialFactors[3]=material.specular;
      vkCmdPushConstants(commandBuffer_,pipelineLayout_,VK_SHADER_STAGE_VERTEX_BIT|
                         VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(push),&push);
      vkCmdDrawIndexed(commandBuffer_,draw.indexCount,1,draw.firstIndex,source.vertexOffset,drawIndex);
    }
  }
  vkCmdEndRenderPass(commandBuffer_);
  motionDrawCount_=0;
}
