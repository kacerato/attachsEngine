// FSR 1 is a display-referred spatial reconstruction: post/AA at internal
// resolution -> AMD EASU -> AMD RCAS/grain -> native-resolution UI.
bool InstancedRenderer::createFsrResources() {
  if(swapchain_->imageCount()>kMaxFramebuffers) return false;
  rhi::ImageDesc image{};
  image.width=swapchain_->width();image.height=swapchain_->height();
  image.format=swapchain_->imageFormat();
  image.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
  image.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
  image.memoryClass=rhi::MemoryClass::RenderTarget;
  if(!memoryAllocator_->createImage(image,&fsrSource_)) return false;
  // EASU/RCAS operate on perceptual RGB. An UNORM intermediate prevents an
  // automatic sRGB transfer between the two AMD filters.
  image.format=VK_FORMAT_R8G8B8A8_UNORM;
  if(!memoryAllocator_->createImage(image,&fsrUpscaled_)) return false;

  VkDescriptorSetLayoutBinding binding{};
  binding.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  binding.descriptorCount=1;binding.stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;
  VkDescriptorSetLayoutCreateInfo layout{};layout.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout.bindingCount=1;layout.pBindings=&binding;
  if(vkCreateDescriptorSetLayout(device_,&layout,nullptr,&fsrSetLayout_)!=VK_SUCCESS) return false;
  VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,2};
  VkDescriptorPoolCreateInfo pool{};pool.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool.maxSets=2;pool.poolSizeCount=1;pool.pPoolSizes=&poolSize;
  if(vkCreateDescriptorPool(device_,&pool,nullptr,&fsrPool_)!=VK_SUCCESS) return false;
  const VkDescriptorSetLayout layouts[]{fsrSetLayout_,fsrSetLayout_};
  VkDescriptorSetAllocateInfo allocate{};allocate.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocate.descriptorPool=fsrPool_;allocate.descriptorSetCount=2;allocate.pSetLayouts=layouts;
  if(vkAllocateDescriptorSets(device_,&allocate,fsrSets_)!=VK_SUCCESS) return false;
  VkDescriptorImageInfo images[]{
    {postSampler_.handle(),fsrSource_.view(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
    {postSampler_.handle(),fsrUpscaled_.view(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};
  VkWriteDescriptorSet writes[2]{};
  for(u32 i=0;i<2;++i) {
    writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[i].dstSet=fsrSets_[i];
    writes[i].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[i].descriptorCount=1;writes[i].pImageInfo=&images[i];
  }
  vkUpdateDescriptorSets(device_,2,writes,0,nullptr);
  VkPushConstantRange push{VK_SHADER_STAGE_FRAGMENT_BIT,0,64};
  VkPipelineLayoutCreateInfo pipelineLayout{};pipelineLayout.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayout.setLayoutCount=1;pipelineLayout.pSetLayouts=&fsrSetLayout_;
  pipelineLayout.pushConstantRangeCount=1;pipelineLayout.pPushConstantRanges=&push;
  if(vkCreatePipelineLayout(device_,&pipelineLayout,nullptr,&fsrPipelineLayout_)!=VK_SUCCESS) return false;

  for(u32 pass=0;pass<2;++pass) {
    VkAttachmentDescription attachment{};
    attachment.format=pass==0?VK_FORMAT_R8G8B8A8_UNORM:swapchain_->imageFormat();
    attachment.samples=VK_SAMPLE_COUNT_1_BIT;attachment.loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout=pass==0?VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference color{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount=1;subpass.pColorAttachments=&color;
    VkSubpassDependency dependency{};
    dependency.srcSubpass=VK_SUBPASS_EXTERNAL;dependency.dstSubpass=0;
    dependency.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo renderPass{};renderPass.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPass.attachmentCount=1;renderPass.pAttachments=&attachment;
    renderPass.subpassCount=1;renderPass.pSubpasses=&subpass;
    renderPass.dependencyCount=1;renderPass.pDependencies=&dependency;
    if(vkCreateRenderPass(device_,&renderPass,nullptr,&fsrRenderPasses_[pass])!=VK_SUCCESS) return false;

    VkShaderModule vert=createShaderModule(device_,rhi::shaders::kPost_ProcessVertSpirv,
                                           rhi::shaders::kPost_ProcessVertSpirvSize);
    VkShaderModule frag=pass==0
        ?createShaderModule(device_,rhi::shaders::kFsr_EasuFragSpirv,rhi::shaders::kFsr_EasuFragSpirvSize)
        :createShaderModule(device_,rhi::shaders::kFsr_RcasFragSpirv,rhi::shaders::kFsr_RcasFragSpirvSize);
    if(!vert||!frag) {
      if(vert) vkDestroyShaderModule(device_,vert,nullptr);
      if(frag) vkDestroyShaderModule(device_,frag,nullptr);
      return false;
    }
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
    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo colorBlend{};colorBlend.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount=1;colorBlend.pAttachments=&blend;
    const VkDynamicState dynamicStates[]{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{};dynamic.sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount=2;dynamic.pDynamicStates=dynamicStates;
    VkGraphicsPipelineCreateInfo pipeline{};pipeline.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline.stageCount=2;pipeline.pStages=stages;pipeline.pVertexInputState=&vertex;
    pipeline.pInputAssemblyState=&assembly;pipeline.pViewportState=&viewport;
    pipeline.pRasterizationState=&raster;pipeline.pMultisampleState=&multisample;
    pipeline.pColorBlendState=&colorBlend;pipeline.pDynamicState=&dynamic;
    pipeline.layout=fsrPipelineLayout_;pipeline.renderPass=fsrRenderPasses_[pass];
    const auto result=vkCreateGraphicsPipelines(device_,VK_NULL_HANDLE,1,&pipeline,nullptr,&fsrPipelines_[pass]);
    vkDestroyShaderModule(device_,vert,nullptr);vkDestroyShaderModule(device_,frag,nullptr);
    if(result!=VK_SUCCESS) return false;

    for(u32 i=0;i<(pass==0?1u:swapchain_->imageCount());++i) {
      const VkImageView view=pass==0?fsrUpscaled_.view():swapchain_->imageView(i);
      VkFramebufferCreateInfo framebuffer{};framebuffer.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebuffer.renderPass=fsrRenderPasses_[pass];framebuffer.attachmentCount=1;
      framebuffer.pAttachments=&view;framebuffer.width=swapchain_->width();
      framebuffer.height=swapchain_->height();framebuffer.layers=1;
      if(vkCreateFramebuffer(device_,&framebuffer,nullptr,
          pass==0?&fsrEasuFramebuffer_:&fsrPresentFramebuffers_[i])!=VK_SUCCESS) return false;
    }
  }
  return true;
}

void InstancedRenderer::recordFsr(u32 imageIndex,const platform::FreeCameraState &camera) {
  if(!fsrActive_) {
    // Keep timestamp checkpoints ordered even when this technique is off.
    beginGpuRegion(GpuPassClass::FsrEasu);endGpuRegion(GpuPassClass::FsrEasu);
    beginGpuRegion(GpuPassClass::FsrRcas);endGpuRegion(GpuPassClass::FsrRcas);
    return;
  }
  struct Push {float sourceSize[4],outputSize[4],viewport[4],presentation[4];};
  static_assert(sizeof(Push)==64);
  const bool srgb=swapchain_->imageFormat()==VK_FORMAT_R8G8B8A8_SRGB||
                  swapchain_->imageFormat()==VK_FORMAT_B8G8R8A8_SRGB;
  const bool effects=!editorBackground_||(editorSceneEffects_&&editorScenePost_);
  const auto look=sceneEnvironment_.active?sceneEnvironment_:
      (editorBackground_?renderer::defaultSceneViewEnvironment():renderer::SceneEnvironment{});
  const auto rect=physicalSceneViewport();
  Push push{{float(renderWidth()),float(renderHeight()),float(swapchain_->width()),float(swapchain_->height())},
            {float(swapchain_->width()),float(swapchain_->height()),effects?renderingPolicy_.post.sharpen:0.0f,srgb?1.0f:0.0f},
            {rect.x,rect.y,rect.width,rect.height},
            {camera.yaw*37.0f+temporalCurrentJitter_[0]*8192.0f,
             camera.pitch*37.0f+temporalCurrentJitter_[1]*8192.0f,
             effects&&look.active&&look.post&&look.filmGrain?look.filmGrainIntensity:0.0f,srgb?1.0f:0.0f}};
  for(u32 pass=0;pass<2;++pass) {
    const auto passClass=pass==0?GpuPassClass::FsrEasu:GpuPassClass::FsrRcas;
    beginGpuRegion(passClass);
    VkRenderPassBeginInfo begin{};begin.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin.renderPass=fsrRenderPasses_[pass];
    begin.framebuffer=pass==0?fsrEasuFramebuffer_:fsrPresentFramebuffers_[imageIndex];
    begin.renderArea.extent={swapchain_->width(),swapchain_->height()};
    vkCmdBeginRenderPass(commandBuffer_,&begin,VK_SUBPASS_CONTENTS_INLINE);
    const VkViewport viewport{0,0,float(swapchain_->width()),float(swapchain_->height()),0,1};
    const VkRect2D scissor{{0,0},{swapchain_->width(),swapchain_->height()}};
    vkCmdSetViewport(commandBuffer_,0,1,&viewport);vkCmdSetScissor(commandBuffer_,0,1,&scissor);
    vkCmdBindPipeline(commandBuffer_,VK_PIPELINE_BIND_POINT_GRAPHICS,fsrPipelines_[pass]);
    vkCmdBindDescriptorSets(commandBuffer_,VK_PIPELINE_BIND_POINT_GRAPHICS,fsrPipelineLayout_,0,1,&fsrSets_[pass],0,nullptr);
    vkCmdPushConstants(commandBuffer_,fsrPipelineLayout_,VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(push),&push);
    vkCmdDraw(commandBuffer_,3,1,0,0);
    vkCmdEndRenderPass(commandBuffer_);
    endGpuRegion(passClass);
  }
}

void InstancedRenderer::destroyFsrResources() {
  if(fsrEasuFramebuffer_) vkDestroyFramebuffer(device_,fsrEasuFramebuffer_,nullptr);
  fsrEasuFramebuffer_=VK_NULL_HANDLE;
  for(auto &framebuffer:fsrPresentFramebuffers_) {
    if(framebuffer) vkDestroyFramebuffer(device_,framebuffer,nullptr);
    framebuffer=VK_NULL_HANDLE;
  }
  for(auto &pipeline:fsrPipelines_) {
    if(pipeline) vkDestroyPipeline(device_,pipeline,nullptr);
    pipeline=VK_NULL_HANDLE;
  }
  if(fsrPipelineLayout_) vkDestroyPipelineLayout(device_,fsrPipelineLayout_,nullptr);
  if(fsrPool_) vkDestroyDescriptorPool(device_,fsrPool_,nullptr);
  if(fsrSetLayout_) vkDestroyDescriptorSetLayout(device_,fsrSetLayout_,nullptr);
  fsrPipelineLayout_=VK_NULL_HANDLE;fsrPool_=VK_NULL_HANDLE;fsrSetLayout_=VK_NULL_HANDLE;
  fsrSets_[0]=fsrSets_[1]=VK_NULL_HANDLE;
  for(auto &pass:fsrRenderPasses_) {
    if(pass) vkDestroyRenderPass(device_,pass,nullptr);
    pass=VK_NULL_HANDLE;
  }
  fsrSource_.reset();fsrUpscaled_.reset();fsrActive_=false;
}
