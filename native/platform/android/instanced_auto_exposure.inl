// GPU histogram and adapted EV belong to each camera view, not the scene UBO.
namespace {
struct AutoExposurePush {
  float sourceSize[4];
  float viewport[4];
  float metering[4];
  float adaptation[4];
  float range[4];
};
static_assert(sizeof(AutoExposurePush)==80);
}

bool InstancedRenderer::createAutoExposureResources() {
  for(auto &view:autoExposureViews_) {
    rhi::BufferDesc histogram{};histogram.sizeBytes=512;
    histogram.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if(!memoryAllocator_->createBuffer(histogram,&view.histogram)) return false;
    rhi::BufferDesc state{};state.sizeBytes=16;
    state.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if(!memoryAllocator_->createBuffer(state,&view.state)) return false;
  }
  VkDescriptorSetLayoutBinding bindings[3]{};
  bindings[0]={0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr};
  bindings[1]={1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr};
  bindings[2]={2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr};
  VkDescriptorSetLayoutCreateInfo layout{};layout.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout.bindingCount=3;layout.pBindings=bindings;
  if(vkCreateDescriptorSetLayout(device_,&layout,nullptr,&autoExposureSetLayout_)!=VK_SUCCESS) return false;
  VkDescriptorPoolSize sizes[]{{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,2},
                               {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,4}};
  VkDescriptorPoolCreateInfo pool{};pool.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool.maxSets=2;pool.poolSizeCount=2;pool.pPoolSizes=sizes;
  if(vkCreateDescriptorPool(device_,&pool,nullptr,&autoExposurePool_)!=VK_SUCCESS) return false;
  VkDescriptorSetLayout layouts[]{autoExposureSetLayout_,autoExposureSetLayout_};
  VkDescriptorSet sets[2]{};VkDescriptorSetAllocateInfo allocate{};
  allocate.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;allocate.descriptorPool=autoExposurePool_;
  allocate.descriptorSetCount=2;allocate.pSetLayouts=layouts;
  if(vkAllocateDescriptorSets(device_,&allocate,sets)!=VK_SUCCESS) return false;
  for(u32 i=0;i<2;++i) {
    auto &view=autoExposureViews_[i];view.set=sets[i];
    VkDescriptorBufferInfo buffers[]{{view.histogram.handle(),0,512},{view.state.handle(),0,16}};
    VkWriteDescriptorSet writes[2]{};
    for(u32 binding=0;binding<2;++binding) {
      writes[binding].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[binding].dstSet=view.set;
      writes[binding].dstBinding=binding+1;writes[binding].descriptorCount=1;
      writes[binding].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      writes[binding].pBufferInfo=&buffers[binding];
    }
    vkUpdateDescriptorSets(device_,2,writes,0,nullptr);
  }
  bindAutoExposureSource(0,postSceneColor_.view());
  bindAutoExposureSource(1,postSceneColor_.view()); // Rebound on preview allocation.
  const auto &limits=rhiDevice_->computeLimits();
  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(physicalDevice_,&properties);
  const bool computeReady=limits.supported &&
      limits.maximumWorkGroupSize[0]>=16 && limits.maximumWorkGroupSize[1]>=16 &&
      limits.maximumWorkGroupSize[2]>=1 && limits.maximumWorkGroupInvocations>=256 &&
      limits.maximumWorkGroupCount[0]>=16 && limits.maximumWorkGroupCount[1]>=9 &&
      limits.maximumPushConstantBytes>=sizeof(AutoExposurePush) &&
      properties.limits.maxComputeSharedMemorySize>=512;
  if(!computeReady) {
    __android_log_print(ANDROID_LOG_WARN,LogTag,
        "[AutoExposure] Compute 16x16, 512 B shared ou push constants indisponíveis.");
    return true;
  }
  VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(AutoExposurePush)};
  VkPipelineLayoutCreateInfo pipelineLayout{};pipelineLayout.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayout.setLayoutCount=1;pipelineLayout.pSetLayouts=&autoExposureSetLayout_;
  pipelineLayout.pushConstantRangeCount=1;pipelineLayout.pPushConstantRanges=&push;
  if(vkCreatePipelineLayout(device_,&pipelineLayout,nullptr,&autoExposurePipelineLayout_)!=VK_SUCCESS) {
    __android_log_print(ANDROID_LOG_WARN,LogTag,"[AutoExposure] Pipeline compute indisponível.");
    return true;
  }
  const u32 *codes[]{rhi::shaders::kExposure_HistogramCompSpirv,
                     rhi::shaders::kExposure_ReduceCompSpirv};
  const usize bytes[]{rhi::shaders::kExposure_HistogramCompSpirvSize,
                      rhi::shaders::kExposure_ReduceCompSpirvSize};
  for(u32 i=0;i<2;++i) {
    VkShaderModule module=createShaderModule(device_,codes[i],bytes[i]);
    if(!module) {
      __android_log_print(ANDROID_LOG_WARN,LogTag,"[AutoExposure] Shader compute indisponível.");
      break;
    }
    VkComputePipelineCreateInfo pipeline{};pipeline.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline.layout=autoExposurePipelineLayout_;
    pipeline.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;pipeline.stage.module=module;
    pipeline.stage.pName="main";
    const bool ok=vkCreateComputePipelines(device_,VK_NULL_HANDLE,1,&pipeline,nullptr,
                                            &autoExposurePipelines_[i])==VK_SUCCESS;
    vkDestroyShaderModule(device_,module,nullptr);
    if(!ok) {
      __android_log_print(ANDROID_LOG_WARN,LogTag,"[AutoExposure] Pipeline compute indisponível.");
      break;
    }
  }
  if(!autoExposurePipelines_[0]||!autoExposurePipelines_[1]) {
    for(auto &pipeline:autoExposurePipelines_) {
      if(pipeline) vkDestroyPipeline(device_,pipeline,nullptr);
      pipeline=VK_NULL_HANDLE;
    }
  }
  return true;
}

void InstancedRenderer::destroyAutoExposureResources() {
  for(auto &pipeline:autoExposurePipelines_) {
    if(pipeline) vkDestroyPipeline(device_,pipeline,nullptr);
    pipeline=VK_NULL_HANDLE;
  }
  if(autoExposurePipelineLayout_) vkDestroyPipelineLayout(device_,autoExposurePipelineLayout_,nullptr);
  if(autoExposurePool_) vkDestroyDescriptorPool(device_,autoExposurePool_,nullptr);
  if(autoExposureSetLayout_) vkDestroyDescriptorSetLayout(device_,autoExposureSetLayout_,nullptr);
  autoExposurePipelineLayout_=VK_NULL_HANDLE;autoExposurePool_=VK_NULL_HANDLE;
  autoExposureSetLayout_=VK_NULL_HANDLE;
  for(auto &view:autoExposureViews_) {
    view.histogram.reset();view.state.reset();view.set=VK_NULL_HANDLE;
    view.initialized=view.enabled=view.cameraValid=false;
  }
}

void InstancedRenderer::bindAutoExposureSource(u32 index,VkImageView source) {
  if(index>=autoExposureViews_.size() || !autoExposureViews_[index].set) return;
  VkDescriptorImageInfo image{postSampler_.handle(),source,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  VkWriteDescriptorSet write{};write.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet=autoExposureViews_[index].set;write.dstBinding=0;
  write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  write.pImageInfo=&image;vkUpdateDescriptorSets(device_,1,&write,0,nullptr);
}

bool InstancedRenderer::recordAutoExposure(u32 index,const renderer::SceneEnvironment &look,
    const platform::FreeCameraState &camera,float timeSeconds,const ui::UiRect &viewport,
    u32 activeWidth,u32 activeHeight,u32 allocatedWidth,u32 allocatedHeight,
    u64 sceneEpoch,u32 cameraEntity) {
  if(index>=autoExposureViews_.size()) return false;
  auto &view=autoExposureViews_[index];
  if(!view.state.isReady()) return false;
  const bool enabled=look.active&&look.post&&look.autoExposure;
  if(enabled && (!hdrSceneColor_ || !autoExposurePipelines_[0] || !autoExposurePipelines_[1])) {
    autoExposureDiagnostic_="Exposição automática requer HDR e compute";
    if(index==1) previewDiagnostic_="Exposição automática requer HDR e compute";
    return false;
  }
  if(!enabled) {
    if(index==0) autoExposureDiagnostic_.clear();
    if(!view.initialized || view.enabled) {
      VkBufferMemoryBarrier before{};before.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
      before.srcAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT|
                           VK_ACCESS_TRANSFER_WRITE_BIT;
      before.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
      before.srcQueueFamilyIndex=before.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
      before.buffer=view.state.handle();before.size=16;
      vkCmdPipelineBarrier(commandBuffer_,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,1,&before,0,nullptr);
      vkCmdFillBuffer(commandBuffer_,view.state.handle(),0,16,0);
      before.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;before.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
      vkCmdPipelineBarrier(commandBuffer_,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                           0,0,nullptr,1,&before,0,nullptr);
      view.initialized=true;
    }
    view.enabled=false;view.cameraValid=false;
    return true;
  }
  const bool jumped=!std::isfinite(timeSeconds)||!view.cameraValid || timeSeconds<view.previousTime ||
      timeSeconds-view.previousTime>1.0f;
  const bool reset=!view.enabled||jumped||view.activeWidth!=activeWidth||
      view.activeHeight!=activeHeight||view.allocatedWidth!=allocatedWidth||
      view.allocatedHeight!=allocatedHeight||view.sceneEpoch!=sceneEpoch||
      view.cameraEntity!=cameraEntity||temporalCameraCut(camera,view.previousCamera)||
      (index==0&&view.paused!=autoExposurePaused_);
  VkBufferMemoryBarrier previous[2]{};
  for(u32 i=0;i<2;++i) {
    previous[i].sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    previous[i].srcAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT|
                              VK_ACCESS_TRANSFER_WRITE_BIT;
    previous[i].dstAccessMask=i?VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT:
                                VK_ACCESS_TRANSFER_WRITE_BIT;
    previous[i].srcQueueFamilyIndex=previous[i].dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    previous[i].buffer=i?view.state.handle():view.histogram.handle();
    previous[i].size=i?16:512;
  }
  vkCmdPipelineBarrier(commandBuffer_,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT|VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       0,0,nullptr,2,previous,0,nullptr);
  if(!view.initialized) vkCmdFillBuffer(commandBuffer_,view.state.handle(),0,16,0);
  vkCmdFillBuffer(commandBuffer_,view.histogram.handle(),0,512,0);
  VkBufferMemoryBarrier transfer[2]{};
  for(u32 i=0;i<2;++i) {
    transfer[i].sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    transfer[i].srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    transfer[i].dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;
    transfer[i].srcQueueFamilyIndex=transfer[i].dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    transfer[i].buffer=i?view.state.handle():view.histogram.handle();
    transfer[i].size=i?16:512;
  }
  vkCmdPipelineBarrier(commandBuffer_,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       0,0,nullptr,view.initialized?1u:2u,transfer,0,nullptr);
  VkImageMemoryBarrier source{};source.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  source.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  source.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
  source.oldLayout=source.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  source.srcQueueFamilyIndex=source.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
  source.image=index?previewSceneColor_.handle():postSceneColor_.handle();
  source.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
  vkCmdPipelineBarrier(commandBuffer_,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,0,nullptr,0,nullptr,1,&source);
  AutoExposurePush push{};
  push.sourceSize[0]=static_cast<float>(activeWidth);push.sourceSize[1]=static_cast<float>(activeHeight);
  push.sourceSize[2]=static_cast<float>(allocatedWidth);push.sourceSize[3]=static_cast<float>(allocatedHeight);
  push.viewport[0]=viewport.x;push.viewport[1]=viewport.y;
  push.viewport[2]=viewport.width;push.viewport[3]=viewport.height;
  const auto *frame=static_cast<const DirtRoadFrameUniform *>(
      index?previewUniform_.mappedData():environmentUniform_.mappedData());
  push.metering[0]=look.autoExposureMinEv;push.metering[1]=look.autoExposureMaxEv;
  push.metering[2]=look.autoExposureTargetGrey;
  push.metering[3]=frame->environment.parameters[0];
  push.adaptation[0]=jumped||(index==0&&autoExposurePaused_)?0.0f:
      std::max(0.0f,timeSeconds-view.previousTime);
  push.adaptation[1]=look.autoExposureSpeedUp;push.adaptation[2]=look.autoExposureSpeedDown;
  push.adaptation[3]=reset?1.0f:0.0f;
  push.range[0]=look.autoExposureLowPercent;push.range[1]=look.autoExposureHighPercent;
  push.range[2]=look.autoExposureCenterWeighted?1.0f:0.0f;
  const u32 gridX=std::clamp(static_cast<u32>(std::ceil(viewport.width*activeWidth)),1u,256u);
  const u32 gridY=std::clamp(static_cast<u32>(std::ceil(viewport.height*activeHeight)),1u,144u);
  vkCmdBindDescriptorSets(commandBuffer_,VK_PIPELINE_BIND_POINT_COMPUTE,autoExposurePipelineLayout_,
                          0,1,&view.set,0,nullptr);
  vkCmdPushConstants(commandBuffer_,autoExposurePipelineLayout_,VK_SHADER_STAGE_COMPUTE_BIT,
                     0,sizeof(push),&push);
  vkCmdBindPipeline(commandBuffer_,VK_PIPELINE_BIND_POINT_COMPUTE,autoExposurePipelines_[0]);
  vkCmdDispatch(commandBuffer_,(gridX+15)/16,(gridY+15)/16,1);
  VkBufferMemoryBarrier histogram{};histogram.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
  histogram.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;histogram.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
  histogram.srcQueueFamilyIndex=histogram.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
  histogram.buffer=view.histogram.handle();histogram.size=512;
  vkCmdPipelineBarrier(commandBuffer_,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       0,0,nullptr,1,&histogram,0,nullptr);
  vkCmdBindPipeline(commandBuffer_,VK_PIPELINE_BIND_POINT_COMPUTE,autoExposurePipelines_[1]);
  vkCmdDispatch(commandBuffer_,1,1,1);
  VkBufferMemoryBarrier state{};state.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
  state.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;state.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
  state.srcQueueFamilyIndex=state.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
  state.buffer=view.state.handle();state.size=16;
  vkCmdPipelineBarrier(commandBuffer_,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                       0,0,nullptr,1,&state,0,nullptr);
  view.initialized=true;view.enabled=true;view.cameraValid=true;
  view.previousTime=timeSeconds;view.previousCamera=camera;
  view.activeWidth=activeWidth;view.activeHeight=activeHeight;
  view.allocatedWidth=allocatedWidth;view.allocatedHeight=allocatedHeight;
  view.sceneEpoch=sceneEpoch;view.cameraEntity=cameraEntity;
  view.paused=index==0&&autoExposurePaused_;
  if(index==0) autoExposureDiagnostic_.clear();
  return true;
}
