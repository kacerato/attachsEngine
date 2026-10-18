// G1 (DESEMPENHO-GRAFICOS-VIVOS-2026-10-17): névoa resolvida uma vez por texel.
//
// O passe de pós chama sampleScene() em cada amostra de FXAA, bloom e
// reconstrução, e cada chamada reavalia a névoa (profundidade + log/exp). No
// POCO F7 a atribuição por máscara mostrou a névoa como ~45% do passe de pós.
// Este pré-passe grava cor + névoa na resolução interna; o passe principal lê
// essa cor com a névoa desligada (bit 512). AO, FXAA, bloom e grade não mudam.
//
// Limites deliberados: só em tela cheia (Play/jogo), sem TAA/ampliador temporal
// (que já têm a própria composição) e com névoa ativa no quadro. Fora disso, o
// caminho original. Promovida no A/B de 17/10/2026 (POCO F7, Vila em Play):
// pós -38%, quadro -17%; imagem difere só no contorno de 1 px das silhuetas.
//
// Os auxiliares abaixo também servem à cadeia de bloom e à AO em 1/2
// (instanced_post_chain.inl): todos são o shader do pós em um modo, num alvo
// próprio, com o conjunto do pós copiado e a entrada (binding 0) trocada.

bool InstancedRenderer::createPostTargetRenderPass(VkFormat format,bool load,VkRenderPass &out) {
  VkAttachmentDescription attachment{};
  attachment.format=format;attachment.samples=VK_SAMPLE_COUNT_1_BIT;
  // load: a subida do bloom soma sobre o conteúdo do nível, já amostrável.
  attachment.loadOp=load?VK_ATTACHMENT_LOAD_OP_LOAD:VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
  attachment.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachment.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachment.initialLayout=load?VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED;
  attachment.finalLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  VkAttachmentReference reference{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  VkSubpassDescription subpass{};subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount=1;subpass.pColorAttachments=&reference;
  VkSubpassDependency dependencies[2]{};
  // Leituras anteriores (quadro anterior ou passe anterior) terminam antes da escrita.
  dependencies[0].srcSubpass=VK_SUBPASS_EXTERNAL;dependencies[0].dstSubpass=0;
  dependencies[0].srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  dependencies[0].srcAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[0].dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[0].dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[1].srcSubpass=0;dependencies[1].dstSubpass=VK_SUBPASS_EXTERNAL;
  dependencies[1].srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[1].srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[1].dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  dependencies[1].dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
  VkRenderPassCreateInfo renderPass{};renderPass.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPass.attachmentCount=1;renderPass.pAttachments=&attachment;
  renderPass.subpassCount=1;renderPass.pSubpasses=&subpass;
  renderPass.dependencyCount=2;renderPass.pDependencies=dependencies;
  return vkCreateRenderPass(device_,&renderPass,nullptr,&out)==VK_SUCCESS;
}

bool InstancedRenderer::createPostShaderPipeline(VkRenderPass renderPass,bool additive,VkPipeline &out) {
  VkShaderModule vert=createShaderModule(device_,rhi::shaders::kPost_ProcessVertSpirv,rhi::shaders::kPost_ProcessVertSpirvSize);
  VkShaderModule frag=createShaderModule(device_,rhi::shaders::kPost_ProcessFragSpirv,rhi::shaders::kPost_ProcessFragSpirvSize);
  bool ready=vert&&frag;
  if(ready) {
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
    if(additive) {
      blend.blendEnable=VK_TRUE;
      blend.srcColorBlendFactor=blend.dstColorBlendFactor=VK_BLEND_FACTOR_ONE;blend.colorBlendOp=VK_BLEND_OP_ADD;
      blend.srcAlphaBlendFactor=VK_BLEND_FACTOR_ZERO;blend.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE;blend.alphaBlendOp=VK_BLEND_OP_ADD;
    }
    VkPipelineColorBlendStateCreateInfo colorBlend{};colorBlend.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount=1;colorBlend.pAttachments=&blend;
    const VkDynamicState dynamicStates[]{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{};dynamic.sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount=2;dynamic.pDynamicStates=dynamicStates;
    VkGraphicsPipelineCreateInfo pipeline{};pipeline.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline.stageCount=2;pipeline.pStages=stages;pipeline.pVertexInputState=&vertex;
    pipeline.pInputAssemblyState=&assembly;pipeline.pViewportState=&viewport;pipeline.pRasterizationState=&raster;
    pipeline.pMultisampleState=&multisample;pipeline.pDepthStencilState=&depth;pipeline.pColorBlendState=&colorBlend;
    pipeline.pDynamicState=&dynamic;pipeline.layout=postPipelineLayout_;pipeline.renderPass=renderPass;
    ready=vkCreateGraphicsPipelines(device_,VK_NULL_HANDLE,1,&pipeline,nullptr,&out)==VK_SUCCESS;
  }
  if(vert) vkDestroyShaderModule(device_,vert,nullptr);
  if(frag) vkDestroyShaderModule(device_,frag,nullptr);
  return ready;
}

bool InstancedRenderer::allocatePostSet(VkImageView input,VkImageView bloom,VkImageView ambientOcclusion,
                                        VkDescriptorSet &out) {
  VkDescriptorSetAllocateInfo allocate{};allocate.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocate.descriptorPool=postDescriptorPool_;allocate.descriptorSetCount=1;allocate.pSetLayouts=&postSetLayout_;
  if(vkAllocateDescriptorSets(device_,&allocate,&out)!=VK_SUCCESS) {out=VK_NULL_HANDLE;return false;}
  std::array<VkCopyDescriptorSet,kPostBindingCount> copies{};
  for(u32 binding=0;binding<kPostBindingCount;++binding) {
    copies[binding].sType=VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
    copies[binding].srcSet=postDescriptorSet_;copies[binding].dstSet=out;
    copies[binding].srcBinding=copies[binding].dstBinding=binding;copies[binding].descriptorCount=1;
  }
  vkUpdateDescriptorSets(device_,0,nullptr,static_cast<u32>(copies.size()),copies.data());
  const VkDescriptorImageInfo images[3]{
      {postSampler_.handle(),input,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
      {postSampler_.handle(),bloom,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
      {postSampler_.handle(),ambientOcclusion,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};
  constexpr u32 bindings[3]{0,8,9};
  VkWriteDescriptorSet writes[3]{};
  for(u32 index=0;index<3;++index) {
    writes[index].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[index].dstSet=out;
    writes[index].dstBinding=bindings[index];writes[index].descriptorCount=1;
    writes[index].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;writes[index].pImageInfo=&images[index];
  }
  vkUpdateDescriptorSets(device_,3,writes,0,nullptr);
  return true;
}

void InstancedRenderer::fillSceneAuxPush(const platform::FreeCameraState &camera,PostPushConstants &push) const {
  // Os mesmos campos que o passe principal envia para névoa e AO.
  push.texelFlags[0]=1.0f/static_cast<float>(renderTargetWidth());
  push.texelFlags[1]=1.0f/static_cast<float>(renderTargetHeight());
  push.grade[3]=static_cast<float>(packSurfaceTransform(swapchain_->surfaceTransform()));
  push.sourceTransform[0]=static_cast<float>(renderWidth())/static_cast<float>(renderTargetWidth());
  push.sourceTransform[1]=static_cast<float>(renderHeight())/static_cast<float>(renderTargetHeight());
  push.sourceTransform[2]=dirtRoadPreview_?1.0f/std::tan(sceneFieldOfView()*.5f):1.732050808f;
  if(sceneOrthographicHalfHeight_>0) push.sourceTransform[2]=-1.0f;
  push.sourceTransform[3]=sceneAspectRatio();
  push.currentCamera[0]=camera.yaw;push.currentCamera[1]=camera.pitch;
  std::memcpy(push.currentPositionNear,camera.position,sizeof(camera.position));
  push.currentPositionNear[3]=dirtRoadPreview_?sceneNearPlane():0.1f;
  push.previousCamera[2]=camera.roll;
  std::memcpy(push.previousPositionFar,camera.position,sizeof(camera.position));
  push.previousPositionFar[3]=dirtRoadPreview_?sceneFarPlane():1000.0f;
}

bool InstancedRenderer::createFogResolveResources() {
  if(postPipelineLayout_==VK_NULL_HANDLE||postDescriptorPool_==VK_NULL_HANDLE||
     postDescriptorSet_==VK_NULL_HANDLE) return false;
  rhi::ImageDesc image{};
  image.width=renderTargetWidth();image.height=renderTargetHeight();
  image.format=VK_FORMAT_R16G16B16A16_SFLOAT;
  image.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
  image.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
  image.memoryClass=rhi::MemoryClass::RenderTarget;
  if(!memoryAllocator_->createImage(image,&fogResolveColor_)) return false;
  if(!createPostTargetRenderPass(VK_FORMAT_R16G16B16A16_SFLOAT,false,fogResolveRenderPass_)) return false;
  const VkImageView view=fogResolveColor_.view();
  VkFramebufferCreateInfo framebuffer{};framebuffer.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebuffer.renderPass=fogResolveRenderPass_;framebuffer.attachmentCount=1;framebuffer.pAttachments=&view;
  framebuffer.width=renderTargetWidth();framebuffer.height=renderTargetHeight();framebuffer.layers=1;
  if(vkCreateFramebuffer(device_,&framebuffer,nullptr,&fogResolveFramebuffer_)!=VK_SUCCESS) return false;
  if(!createPostShaderPipeline(fogResolveRenderPass_,false,fogResolvePipeline_)) return false;
  // O passe principal lê esta cor; bloom e AO continuam os do conjunto do pós.
  return allocatePostSet(fogResolveColor_.view(),postBloomView(),postAmbientOcclusionView(),fogResolveDescriptorSet_);
}

void InstancedRenderer::destroyFogResolveResources() {
  if(device_!=VK_NULL_HANDLE) {
    if(fogResolvePipeline_) vkDestroyPipeline(device_,fogResolvePipeline_,nullptr);
    if(fogResolveFramebuffer_) vkDestroyFramebuffer(device_,fogResolveFramebuffer_,nullptr);
    if(fogResolveRenderPass_) vkDestroyRenderPass(device_,fogResolveRenderPass_,nullptr);
  }
  fogResolvePipeline_=VK_NULL_HANDLE;fogResolveFramebuffer_=VK_NULL_HANDLE;fogResolveRenderPass_=VK_NULL_HANDLE;
  // O conjunto pertence ao pool do pós, liberado junto com ele.
  fogResolveDescriptorSet_=VK_NULL_HANDLE;
  fogResolveColor_.reset();
  fogResolvedThisFrame_=false;
}

bool InstancedRenderer::recordFogResolve(const platform::FreeCameraState &camera) {
  fogResolvedThisFrame_=false;
  if(!fogResolveEnabled_||fogResolvePipeline_==VK_NULL_HANDLE||!fogActiveThisFrame_||
     editorBackground_||temporalAaActive_||temporalUpscaler_.ready()) return false;
  VkRenderPassBeginInfo begin{};begin.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  begin.renderPass=fogResolveRenderPass_;begin.framebuffer=fogResolveFramebuffer_;
  begin.renderArea.extent={renderWidth(),renderHeight()};
  vkCmdBeginRenderPass(commandBuffer_,&begin,VK_SUBPASS_CONTENTS_INLINE);
  const VkViewport viewport{0,0,static_cast<float>(renderWidth()),static_cast<float>(renderHeight()),0,1};
  const VkRect2D scissor{{0,0},{renderWidth(),renderHeight()}};
  vkCmdSetViewport(commandBuffer_,0,1,&viewport);vkCmdSetScissor(commandBuffer_,0,1,&scissor);
  vkCmdBindPipeline(commandBuffer_,VK_PIPELINE_BIND_POINT_GRAPHICS,fogResolvePipeline_);
  vkCmdBindDescriptorSets(commandBuffer_,VK_PIPELINE_BIND_POINT_GRAPHICS,postPipelineLayout_,0,1,
                          &postPrepassDescriptorSet_,0,nullptr);
  PostPushConstants push{};
  fillSceneAuxPush(camera,push);
  push.texelFlags[2]=256.0f;
  vkCmdPushConstants(commandBuffer_,postPipelineLayout_,VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(push),&push);
  vkCmdDraw(commandBuffer_,3,1,0,0);
  vkCmdEndRenderPass(commandBuffer_);
  fogResolvedThisFrame_=true;
  return true;
}
