// Included inside the renderer namespace; keeps frame ABI in one translation unit.
void InstancedRenderer::releaseCameraPreviewEnvironment() {
  for(auto &sampler:previewEnvironmentSamplers_) sampler.shutdown();
  for(auto &image:previewEnvironmentImages_) image.reset();
  previewEnvironmentMap_.reset();
  previewEnvironmentReady_=false;
  previewEnvironmentUsesPrimary_=true;
}

bool InstancedRenderer::setCameraPreviewEnvironmentMap(renderer::SharedEnvironmentMap resource) {
  if(resource==activeEnvironmentMap_ || !resource) {
    if(previewEnvironmentMap_) {
      if(vkDeviceWaitIdle(device_)!=VK_SUCCESS) return false;
      releaseCameraPreviewEnvironment();
    }
    previewEnvironmentUsesPrimary_=resource==activeEnvironmentMap_;
    previewEnvironmentReady_=true;
    return true;
  }
  if(resource==previewEnvironmentMap_ && previewEnvironmentReady_) return true;
  if(!resource->valid() || vkDeviceWaitIdle(device_)!=VK_SUCCESS) return false;
  std::array<rhi::VulkanImage,3> images;
  std::array<rhi::VulkanSampler,3> samplers;
  const renderer::Rgba16fMipChain *chains[]{&resource->panorama,&resource->specular,&resource->brdf};
  for(u32 i=0;i<3;++i) {
    const auto &chain=*chains[i];
    rhi::ImageDesc desc{};desc.width=chain.width;desc.height=chain.height;desc.mipLevels=chain.levels;
    desc.format=VK_FORMAT_R16G16B16A16_SFLOAT;
    desc.usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
    desc.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
    if(!memoryAllocator_->createImage(desc,&images[i]) ||
       !uploadContext_.uploadSampledMipChain(*memoryAllocator_,chain.texels.data(),
                                             chain.texels.size()*sizeof(u16),images[i])) return false;
    rhi::SamplerDesc sampler{};sampler.minFilter=VK_FILTER_LINEAR;sampler.magFilter=VK_FILTER_LINEAR;
    sampler.mipmapMode=VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler.addressU=i==0?VK_SAMPLER_ADDRESS_MODE_REPEAT:VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.addressV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.maxLod=static_cast<float>(chain.levels-1);
    if(!samplers[i].initialize(device_,sampler)) return false;
  }
  previewEnvironmentImages_=std::move(images);
  previewEnvironmentSamplers_=std::move(samplers);
  previewEnvironmentMap_=std::move(resource);
  previewEnvironmentUsesPrimary_=false;
  previewEnvironmentReady_=true;
  return true;
}

bool InstancedRenderer::prepareCameraPreviewEnvironment() {
  const auto &view=pendingPreview_;
  previewSceneEnvironment_=sceneEnvironmentVolumes_.empty()?sceneEnvironment_:
      renderer::resolveSceneEnvironment(sceneEnvironmentVolumes_,view.frustum.cameraPosition,
                                        view.environmentLayerMask);
  if(editorBackground_&&(!editorSceneEffects_||!editorScenePost_))
    previewSceneEnvironment_.autoExposure=false;
  renderer::SharedEnvironmentMap selected;
  if(previewSceneEnvironment_.active && previewSceneEnvironment_.environmentMap.valid()) {
    for(const auto &[guid,resource]:environmentMaps_)
      if(guid==previewSceneEnvironment_.environmentMap) {selected=resource;break;}
    if(!selected) {
      previewDiagnostic_="HDRI da câmera indisponível";
      __android_log_print(ANDROID_LOG_ERROR,LogTag,"[CameraPreview] HDRI referenciado indisponível.");
      return false;
    }
  }
  if(!setCameraPreviewEnvironmentMap(selected)) {
    previewDiagnostic_="Falha ao carregar HDRI da câmera";
    __android_log_print(ANDROID_LOG_ERROR,LogTag,"[CameraPreview] Falha ao publicar HDRI na GPU.");
    return false;
  }
  VkDescriptorImageInfo images[3]{};
  if(previewEnvironmentMap_) {
    for(u32 i=0;i<3;++i)
      images[i]={previewEnvironmentSamplers_[i].handle(),previewEnvironmentImages_[i].view(),
                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  } else if(previewEnvironmentUsesPrimary_) {
    images[0]={dirtRoadResources_.environmentSampler(),dirtRoadResources_.environmentView(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    images[1]={dirtRoadResources_.environmentSpecularSampler(),dirtRoadResources_.environmentSpecularView(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    images[2]={dirtRoadResources_.environmentBrdfSampler(),dirtRoadResources_.environmentBrdfView(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  } else {
    images[0]={dirtRoadResources_.packagedEnvironmentSampler(),dirtRoadResources_.packagedEnvironmentView(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    images[1]={dirtRoadResources_.packagedEnvironmentSpecularSampler(),dirtRoadResources_.packagedEnvironmentSpecularView(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    images[2]={dirtRoadResources_.packagedEnvironmentBrdfSampler(),dirtRoadResources_.packagedEnvironmentBrdfView(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  }
  VkWriteDescriptorSet writes[3]{};constexpr u32 bindings[]{1,3,4};
  for(u32 i=0;i<3;++i) {
    writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[i].dstSet=previewSet_;
    writes[i].dstBinding=bindings[i];writes[i].descriptorCount=1;
    writes[i].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;writes[i].pImageInfo=&images[i];
  }
  vkUpdateDescriptorSets(device_,3,writes,0,nullptr);
  return true;
}

void InstancedRenderer::destroyCameraPreview() {
  uiRenderer_.setCameraPreview(VK_NULL_HANDLE);
  if(previewPostFramebuffer_) vkDestroyFramebuffer(device_,previewPostFramebuffer_,nullptr);
  if(previewFramebuffer_) vkDestroyFramebuffer(device_,previewFramebuffer_,nullptr);
  if(previewPostPool_) vkDestroyDescriptorPool(device_,previewPostPool_,nullptr);
  if(previewPool_) vkDestroyDescriptorPool(device_,previewPool_,nullptr);
  previewPostFramebuffer_=VK_NULL_HANDLE;previewFramebuffer_=VK_NULL_HANDLE;
  previewPostPool_=VK_NULL_HANDLE;previewPostSet_=VK_NULL_HANDLE;
  previewPool_=VK_NULL_HANDLE;previewSet_=VK_NULL_HANDLE;
  previewUniform_.reset();previewSceneColor_.reset();previewColor_.reset();previewDepth_.reset();previewResolved_.reset();
  releaseCameraPreviewEnvironment();
  autoExposureViews_[1].initialized=false;
  autoExposureViews_[1].enabled=false;
  autoExposureViews_[1].cameraValid=false;
}
bool InstancedRenderer::prepareCameraPreview() {
  previewDiagnostic_.clear();
  if(!pendingPreview_.valid() || pendingPreview_.width>4096 || pendingPreview_.height>4096 ||
     !dirtRoadPreview_ || !environmentUniform_.isReady() || !previewRenderPass_ ||
     !previewPostRenderPass_ || !postPipeline_ || !postSetLayout_) return false;
  if(!previewFramebuffer_ || previewColor_.width()!=pendingPreview_.width || previewColor_.height()!=pendingPreview_.height) {
    destroyCameraPreview();
    rhi::ImageDesc source{};source.width=pendingPreview_.width;source.height=pendingPreview_.height;
    source.format=sceneColorFormat_;source.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
    source.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;source.memoryClass=rhi::MemoryClass::RenderTarget;
    if(!memoryAllocator_->createImage(source,&previewSceneColor_)) return false;
    auto color=source;color.format=swapchain_->imageFormat();
    if(!memoryAllocator_->createImage(color,&previewColor_)) {destroyCameraPreview();return false;}
    if(temporalAaActive_ && !memoryAllocator_->createImage(color,&previewResolved_)) {destroyCameraPreview();return false;}
    auto depth=source;depth.format=depthFormat_;depth.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
    if(waterSubpassActive_) depth.usage|=VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    depth.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;
    if(hasStencil(depthFormat_)) depth.aspectMask|=VK_IMAGE_ASPECT_STENCIL_BIT;
    if(!memoryAllocator_->createImage(depth,&previewDepth_)) {destroyCameraPreview();return false;}
    VkImageView attachments[]{previewSceneColor_.view(),previewDepth_.view()};
    VkFramebufferCreateInfo fb{};fb.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb.renderPass=previewRenderPass_;fb.attachmentCount=2;fb.pAttachments=attachments;
    fb.width=source.width;fb.height=source.height;fb.layers=1;
    if(vkCreateFramebuffer(device_,&fb,nullptr,&previewFramebuffer_)!=VK_SUCCESS) {destroyCameraPreview();return false;}
    const VkImageView finalAttachments[]{previewColor_.view(),previewResolved_.view()};
    fb.renderPass=previewPostRenderPass_;fb.attachmentCount=temporalAaActive_?2u:1u;fb.pAttachments=finalAttachments;
    if(vkCreateFramebuffer(device_,&fb,nullptr,&previewPostFramebuffer_)!=VK_SUCCESS) {destroyCameraPreview();return false;}
    rhi::BufferDesc buffer{};buffer.sizeBytes=sizeof(DirtRoadFrameUniform);
    buffer.usage=VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;buffer.cpuAccess=rhi::CpuAccess::SequentialWrite;buffer.preferDeviceMemory=false;
    if(!memoryAllocator_->createBuffer(buffer,&previewUniform_)) {destroyCameraPreview();return false;}
    VkDescriptorPoolSize sizes[]{{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,10},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,6},{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,1}};
    VkDescriptorPoolCreateInfo pool{};pool.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool.maxSets=1;pool.poolSizeCount=4;pool.pPoolSizes=sizes;
    if(vkCreateDescriptorPool(device_,&pool,nullptr,&previewPool_)!=VK_SUCCESS) {destroyCameraPreview();return false;}
    VkDescriptorSetAllocateInfo allocate{};allocate.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate.descriptorPool=previewPool_;allocate.descriptorSetCount=1;allocate.pSetLayouts=&environmentSetLayout_;
    if(vkAllocateDescriptorSets(device_,&allocate,&previewSet_)!=VK_SUCCESS) {destroyCameraPreview();return false;}

    VkDescriptorPoolSize postSizes[]{{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,4},
                                     {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1},
                                     {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1}};
    pool.maxSets=1;pool.poolSizeCount=3;pool.pPoolSizes=postSizes;
    if(vkCreateDescriptorPool(device_,&pool,nullptr,&previewPostPool_)!=VK_SUCCESS) {destroyCameraPreview();return false;}
    allocate.descriptorPool=previewPostPool_;allocate.pSetLayouts=&postSetLayout_;
    if(vkAllocateDescriptorSets(device_,&allocate,&previewPostSet_)!=VK_SUCCESS) {destroyCameraPreview();return false;}
    VkDescriptorImageInfo postImages[4]{
      {postSampler_.handle(),previewSceneColor_.view(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
      {postSampler_.handle(),previewSceneColor_.view(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
      {postDepthSampler_.handle(),previewDepth_.view(),VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL},
      {postDepthSampler_.handle(),previewSceneColor_.view(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};
    VkDescriptorBufferInfo postBuffer{previewUniform_.handle(),0,sizeof(DirtRoadFrameUniform)};
    VkWriteDescriptorSet postWrites[6]{};
    for(u32 index=0;index<3;++index) {
      postWrites[index].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      postWrites[index].dstSet=previewPostSet_;postWrites[index].dstBinding=index;
      postWrites[index].descriptorCount=1;postWrites[index].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      postWrites[index].pImageInfo=&postImages[index];
    }
    postWrites[3].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;postWrites[3].dstSet=previewPostSet_;
    postWrites[3].dstBinding=3;postWrites[3].descriptorCount=1;
    postWrites[3].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;postWrites[3].pBufferInfo=&postBuffer;
    VkDescriptorBufferInfo exposureBuffer{autoExposureViews_[1].state.handle(),0,16};
    postWrites[4].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;postWrites[4].dstSet=previewPostSet_;
    postWrites[4].dstBinding=4;postWrites[4].descriptorCount=1;
    postWrites[4].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;postWrites[4].pBufferInfo=&exposureBuffer;
    postWrites[5].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;postWrites[5].dstSet=previewPostSet_;
    postWrites[5].dstBinding=5;postWrites[5].descriptorCount=1;
    postWrites[5].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    postWrites[5].pImageInfo=&postImages[3];
    vkUpdateDescriptorSets(device_,6,postWrites,0,nullptr);
    bindAutoExposureSource(1,previewSceneColor_.view());
    uiRenderer_.setCameraPreview(previewColor_.view());
  }
  std::vector<VkCopyDescriptorSet> copies;
  for(u32 binding=1;binding<=17;++binding) {
    if(binding==1||binding==3||binding==4) continue;
    if((binding==5||binding==6||binding==15)&&!waterSubpassActive_) continue;
    if(binding>=7&&binding<=14&&!spectralWaterCount_) continue;
    VkCopyDescriptorSet copy{};copy.sType=VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
    copy.srcSet=environmentSet_;copy.dstSet=previewSet_;copy.srcBinding=copy.dstBinding=binding;copy.descriptorCount=1;
    copies.push_back(copy);
  }
  vkUpdateDescriptorSets(device_,0,nullptr,static_cast<u32>(copies.size()),copies.data());
  VkDescriptorBufferInfo buffer{previewUniform_.handle(),0,sizeof(DirtRoadFrameUniform)};
  VkWriteDescriptorSet writes[2]{};writes[0].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].dstSet=previewSet_;writes[0].dstBinding=0;writes[0].descriptorCount=1;
  writes[0].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;writes[0].pBufferInfo=&buffer;
  VkDescriptorImageInfo depth{VK_NULL_HANDLE,previewDepth_.view(),VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
  writes[1]=writes[0];writes[1].dstBinding=5;writes[1].descriptorType=VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
  writes[1].pBufferInfo=nullptr;writes[1].pImageInfo=&depth;
  vkUpdateDescriptorSets(device_,waterSubpassActive_?2u:1u,writes,0,nullptr);
  if(!prepareCameraPreviewEnvironment()) return false;
  if(previewSceneEnvironment_.active && previewSceneEnvironment_.post &&
     previewSceneEnvironment_.autoExposure &&
     (!hdrSceneColor_ || !autoExposurePipelines_[0] || !autoExposurePipelines_[1])) {
    previewDiagnostic_="Exposição automática requer HDR e compute";
    return false;
  }
  auto frame=*static_cast<const DirtRoadFrameUniform*>(environmentUniform_.mappedData());
  // The preview owns its entire target; the editor's physical viewport
  // rectangle includes surrounding panels and must not crop this camera.
  frame.postViewport[0]=frame.postViewport[1]=0;
  frame.postViewport[2]=frame.postViewport[3]=1;
  // O primeiro alvo guarda a cena linear/HDR. A segunda etapa usa este mesmo
  // UBO para neblina, AO, bloom, gradação e tonemap.
  frame.scenePost[2]=hdrSceneColor_?1.0f:0.0f;
  const auto &mapDescription=previewEnvironmentMap_?previewEnvironmentMap_->description:
      previewEnvironmentUsesPrimary_?dirtRoadResources_.environmentMapDescription():
                                     dirtRoadResources_.packagedEnvironmentMapDescription();
  const bool editorFog=!editorBackground_||(editorSceneEffects_&&editorSceneFog_);
  const bool editorPost=!editorBackground_||(editorSceneEffects_&&editorScenePost_);
  const std::span<const renderer::SceneLight> lights=editorBackground_&&!editorSceneLighting_
      ? std::span<const renderer::SceneLight>{}:std::span<const renderer::SceneLight>{sceneLights_};
  if(!applySceneEnvironmentLook(frame,dirtRoadResources_.environmentLighting(),environmentAdjustment_,
      previewSceneEnvironment_,mapDescription,static_cast<bool>(previewEnvironmentMap_) ||
      (previewEnvironmentUsesPrimary_&&static_cast<bool>(activeEnvironmentMap_)),
      renderingPolicy_.ambient.splitSumBrdf,editorFog,editorPost,lights,previewAtmosphereGroundCache_)) {
    previewDiagnostic_="Ambiente da câmera inválido";
    return false;
  }
  renderer::LightBudgetReport previewLightBudget{};
  const u32 accepted=renderer::selectPunctualLights(lights,pendingPreview_.frustum.cameraPosition,
      frame.punctualLights,previewLightBudget);
  frame.punctualLightParameters[0]=static_cast<float>(accepted);
  for(u32 i=0;i<renderer::MaximumPunctualLights;++i) {
    if(i>=accepted) frame.punctualLights[i]={};
    else frame.punctualLights[i].shadow[1]=0;
  }
  // The primary camera's cascades and local atlas do not describe this view.
  frame.shadowParameters[1]=0;
  frame.localShadowParameters[0]=0;
  const auto &f=pendingPreview_.frustum;const auto basis=renderer::buildCameraViewBasis(f.yaw,f.pitch,f.roll);
  std::copy(basis.row0,basis.row0+3,frame.worldToViewRow0);std::copy(basis.row1,basis.row1+3,frame.worldToViewRow1);
  std::copy(basis.row2,basis.row2+3,frame.worldToViewRow2);
  frame.worldToViewRow0[3]=renderer::isOrthographic(f)?f.orthographicHalfHeight:0;
  frame.shadowFilterParameters[2]=frame.shadowFilterParameters[3]=0;
  frame.shadowTransitionParameters[2]=1.f/f.tangentHalfVertical;
  std::memcpy(previewUniform_.mappedData(),&frame,sizeof(frame));
  return memoryAllocator_->flushBuffer(previewUniform_);
}
void InstancedRenderer::recordCameraPreview(float timeSeconds) {
  if(!pendingPreview_.valid()||!previewFramebuffer_) return;
  const auto view=pendingPreview_;platform::FreeCameraState camera{};
  std::copy(view.frustum.cameraPosition,view.frustum.cameraPosition+3,camera.position);
  camera.yaw=view.frustum.yaw;camera.pitch=view.frustum.pitch;camera.roll=view.frustum.roll;
  const renderer::HzbScreenTransform surfaceTransform{};
  const bool encodeSrgb=false;
  VkMemoryBarrier before{};before.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER;
  before.srcAccessMask=VK_ACCESS_SHADER_READ_BIT;before.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  vkCmdPipelineBarrier(commandBuffer_,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,0,1,&before,0,nullptr,0,nullptr);
  VkClearValue clear[2]{};clear[0].color={{.028f,.032f,.039f,1}};clear[1].depthStencil={1,0};
  VkRenderPassBeginInfo begin{};begin.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  begin.renderPass=previewRenderPass_;begin.framebuffer=previewFramebuffer_;begin.renderArea.extent={view.width,view.height};
  begin.clearValueCount=2;begin.pClearValues=clear;
  vkCmdBeginRenderPass(commandBuffer_,&begin,VK_SUBPASS_CONTENTS_INLINE);
  const VkViewport viewport{0,0,static_cast<float>(view.width),static_cast<float>(view.height),0,1};
  const VkRect2D scissor{{0,0},{view.width,view.height}};
  vkCmdSetViewport(commandBuffer_,0,1,&viewport);vkCmdSetScissor(commandBuffer_,0,1,&scissor);
  vkCmdBindDescriptorSets(commandBuffer_,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelineLayout_,1,1,&previewSet_,0,nullptr);
  if(useBindless_) vkCmdBindDescriptorSets(commandBuffer_,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelineLayout_,0,1,&textureSet_,0,nullptr);
  const VkDeviceSize zero=0;const auto instances=instanceBuffer_.handle();
  vkCmdBindVertexBuffers(commandBuffer_,1,1,&instances,&zero);
    auto pushMapMaterial = [&](u32 materialIndex, u32 drawIndex=0xffffffffu) {
      auto material = dirtRoadResources_.materials()[materialIndex];
      // R4: texturas trocadas indexam a biblioteca importada, que começa depois
      // das texturas do pacote -- o mesmo deslocamento dos materiais importados.
      if(drawIndex<authoredMaterials_.size())
        material=renderer::applyMaterialOverride(material,authoredMaterials_[drawIndex],static_cast<u32>(dirtRoadResources_.packageTextureCount()));
      // R4: dupla face real. Só material que provou ordem de vértices (glTF de
      // uma face ou "Uma face" escolhida) descarta a face de trás.
      if(materialCulling_)
        rhiDevice_->cmdSetCullMode(commandBuffer_,
            (material.flags & renderer::MapMaterialCullBackFaces)!=0 && (material.flags & renderer::MapMaterialDoubleSided)==0
                ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE);
      if (!useBindless_) {
        const VkDescriptorSet set = dirtMaterialSets_[materialIndex];
        vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_,
                                0, 1, &set, 0, nullptr);
      }
      DirtRoadPushConstants push{};
      push.cameraFrame[0] = static_cast<float>(view.width)/view.height;
      push.cameraFrame[1] = camera.yaw; push.cameraFrame[2] = camera.pitch;
      push.cameraFrame[3] = timeSeconds;
      push.surfaceTransform[0]=surfaceTransform.xx;push.surfaceTransform[1]=surfaceTransform.xy;
      push.surfaceTransform[2]=surfaceTransform.yx;push.surfaceTransform[3]=surfaceTransform.yy;
      std::memcpy(push.cameraPositionNear, camera.position, sizeof(camera.position));
      push.cameraPositionNear[3] = view.frustum.nearPlane;
      std::memcpy(push.baseColorFactor, material.baseColorFactor, sizeof(push.baseColorFactor));
      std::memcpy(push.emissiveFactorAndStrength, material.emissiveFactorAndStrength,
                  sizeof(push.emissiveFactorAndStrength));
      if((material.flags & renderer::WaterAuthoringResource) && (material.flags & renderer::MapMaterialWater) && drawIndex<authoredWaterLayers_.size()) {
        std::copy(authoredWaterLayers_[drawIndex].begin(),authoredWaterLayers_[drawIndex].end(),push.emissiveFactorAndStrength);
        std::copy(authoredWaterFlowDepth_[drawIndex].begin(),authoredWaterFlowDepth_[drawIndex].end(),push.baseColorFactor);
      }
      for (u32 slot = 0; slot < 4; ++slot) {
        const u32 texture = material.textureIndices[slot];
        // O limite protege contra um material que chegue antes dos slots das
        // texturas importadas: branco neutro, nunca leitura fora do vetor.
        push.textureIndices[slot] = useBindless_ && texture != renderer::InvalidMapTexture && texture < dirtTextureSlots_.size()
                                        ? dirtTextureSlots_[texture] : baseTextureIndex_;
      }
      push.materialFlags[0]=material.flags;
      const u32 alphaCutoff = static_cast<u32>(std::clamp(material.alphaCutoff, 0.0f, 1.0f) * 255.0f + .5f);
      // High 16 bits carry optional material-class metadata. Multi-view
      // impostors use it for atlas/grid layout; legacy materials keep zero and
      // therefore preserve the old push-constant bit pattern.
      push.materialFlags[1]=material.textureCoordinates | (alphaCutoff << 8u) |
                            ((material.reserved & 0xffffu) << 16u);
      // Bit 0: saída sRGB. Bits 1..31: entrada da transformação de UV do desenho (0 = nenhuma).
      push.materialFlags[2]=(encodeSrgb?1u:0u) |
          ((drawIndex<authoredUvTransformEntries_.size()?authoredUvTransformEntries_[drawIndex]:0u)<<1u);
      push.materialFlags[3]=std::bit_cast<u32>(view.frustum.farPlane);
      push.materialFactors[0]=material.roughness;push.materialFactors[1]=material.metallic;
      push.materialFactors[2]=material.normalScale;push.materialFactors[3]=material.specular;
      vkCmdPushConstants(commandBuffer_,pipelineLayout_,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
                         0,sizeof(push),&push);
    };
  std::vector<u32> transparent;
  auto draw=[&](u32 index,bool blend) {
    const auto &record=dirtRoadResources_.draws()[index];
    auto material=dirtRoadResources_.materials()[record.materialIndex];
    if(index<authoredMaterials_.size()) material=renderer::applyMaterialOverride(material,authoredMaterials_[index]);
    const auto variant=renderer::materialFeatureVariant(material.flags);
    const auto *variants=blend?transparentMaterialPipelines_:opaqueMaterialPipelines_;
    const auto pipeline=variants[variant]?variants[variant]:(blend?transparentPipeline_:pipeline_);
    vkCmdBindPipeline(commandBuffer_,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
    pushMapMaterial(record.materialIndex,index);
    const auto vertices=dirtRoadResources_.vertexBuffer();
    vkCmdBindVertexBuffers(commandBuffer_,0,1,&vertices,&zero);
    vkCmdBindIndexBuffer(commandBuffer_,dirtRoadResources_.indexBuffer(),0,VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer_,record.indexCount,1,record.firstIndex,static_cast<i32>(record.vertexOffset),index);
  };
  for(u32 i=0;i<dirtRoadResources_.draws().size();++i) {
    if(!authoredVisibility_.empty()&&!authoredVisibility_[i]) continue;
    const auto &record=dirtRoadResources_.draws()[i];
    auto material=dirtRoadResources_.materials()[record.materialIndex];
    if(i<authoredMaterials_.size()) material=renderer::applyMaterialOverride(material,authoredMaterials_[i]);
    // Water depends on scene-specific simulation/depth; do not publish a fake surface.
    if(material.flags&renderer::MapMaterialWater) continue;
    if(!renderer::isSphereVisible(view.frustum,record.boundsCenter,record.boundsRadius)) continue;
    if(material.flags&renderer::MapMaterialBlend) transparent.push_back(i);else draw(i,false);
  }
  // The authored camera preview must draw the same environment background as
  // the game camera. Leaving the clear color here hid both HDRI and atmosphere.
  if(skyPipeline_) {
    DirtRoadPushConstants sky{};
    const float ortho=renderer::isOrthographic(view.frustum)?view.frustum.orthographicHalfHeight:0.0f;
    std::memcpy(&sky.textureIndices[3],&ortho,sizeof(ortho));
    sky.cameraFrame[0]=static_cast<float>(view.width)/view.height;
    sky.cameraFrame[1]=camera.yaw;sky.cameraFrame[2]=camera.pitch;sky.cameraFrame[3]=timeSeconds;
    sky.materialFactors[0]=camera.roll;sky.materialFactors[3]=1.0f/view.frustum.tangentHalfVertical;
    sky.surfaceTransform[0]=sky.surfaceTransform[3]=1.0f;
    vkCmdBindPipeline(commandBuffer_,VK_PIPELINE_BIND_POINT_GRAPHICS,skyPipeline_);
    vkCmdBindDescriptorSets(commandBuffer_,VK_PIPELINE_BIND_POINT_GRAPHICS,skyPipelineLayout_,0,1,&previewSet_,0,nullptr);
    vkCmdPushConstants(commandBuffer_,skyPipelineLayout_,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
                       0,sizeof(sky),&sky);
    vkCmdDraw(commandBuffer_,3,1,0,0);
    // Sky uses a different layout; restore the material descriptors for glass.
    vkCmdBindDescriptorSets(commandBuffer_,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelineLayout_,1,1,&previewSet_,0,nullptr);
    if(useBindless_) vkCmdBindDescriptorSets(commandBuffer_,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelineLayout_,0,1,&textureSet_,0,nullptr);
  }
  if(waterSubpassActive_) vkCmdNextSubpass(commandBuffer_,VK_SUBPASS_CONTENTS_INLINE);
  const auto distance=[&](u32 index){float result=0;const auto &r=dirtRoadResources_.draws()[index];
    for(u32 k=0;k<3;++k) {const float d=r.boundsCenter[k]-camera.position[k];result+=d*d;}return result;};
  std::sort(transparent.begin(),transparent.end(),[&](u32 a,u32 b){return distance(a)>distance(b);});
  for(auto index:transparent) draw(index,true);
  vkCmdEndRenderPass(commandBuffer_);
  VkMemoryBarrier after{};after.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER;
  after.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  after.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(commandBuffer_,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|
    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    0,1,&after,0,nullptr,0,nullptr);
  if(!recordAutoExposure(1,previewSceneEnvironment_,camera,timeSeconds,{0,0,1,1},
                         view.width,view.height,view.width,view.height,
                         view.sceneEpoch,view.cameraEntity)) {
    if(previewDiagnostic_.empty()) previewDiagnostic_="Falha ao medir exposição da câmera";
    completedPreview_=view;pendingPreview_={};previewCompletionSuccess_=false;
    return;
  }

  VkRenderPassBeginInfo postBegin{};postBegin.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  postBegin.renderPass=previewPostRenderPass_;postBegin.framebuffer=previewPostFramebuffer_;
  postBegin.renderArea.extent={view.width,view.height};
  vkCmdBeginRenderPass(commandBuffer_,&postBegin,VK_SUBPASS_CONTENTS_INLINE);
  vkCmdSetViewport(commandBuffer_,0,1,&viewport);vkCmdSetScissor(commandBuffer_,0,1,&scissor);
  vkCmdBindPipeline(commandBuffer_,VK_PIPELINE_BIND_POINT_GRAPHICS,postPipeline_);
  vkCmdBindDescriptorSets(commandBuffer_,VK_PIPELINE_BIND_POINT_GRAPHICS,postPipelineLayout_,0,1,&previewPostSet_,0,nullptr);
  PostPushConstants post{};
  const bool authoredPost=previewSceneEnvironment_.active&&previewSceneEnvironment_.post;
  post.texelFlags[0]=1.0f/view.width;post.texelFlags[1]=1.0f/view.height;
  post.texelFlags[2]=(authoredPost?previewSceneEnvironment_.bloom:renderingPolicy_.post.bloom)?1.0f:0.0f;
  post.texelFlags[3]=(renderingPolicy_.post.antiAliasing==renderer::AntiAliasingMode::Fxaa ||
                      renderingPolicy_.post.antiAliasing==renderer::AntiAliasingMode::Temporal)?1.0f:0.0f;
  post.bloom[0]=authoredPost?previewSceneEnvironment_.bloomThreshold:renderingPolicy_.post.bloomThreshold;
  post.bloom[1]=authoredPost?previewSceneEnvironment_.bloomIntensity:renderingPolicy_.post.bloomIntensity;
  post.bloom[2]=renderingPolicy_.post.sharpen;
  post.bloom[3]=authoredPost?(previewSceneEnvironment_.vignette?previewSceneEnvironment_.vignetteIntensity:0.0f):
      (renderingPolicy_.post.vignette?renderingPolicy_.post.vignetteIntensity:0.0f);
  post.grade[0]=authoredPost?previewSceneEnvironment_.contrast:renderingPolicy_.post.contrast;
  post.grade[1]=authoredPost?previewSceneEnvironment_.saturation:renderingPolicy_.post.saturation;
  post.grade[2]=(previewColor_.format()==VK_FORMAT_B8G8R8A8_SRGB ||
                 previewColor_.format()==VK_FORMAT_R8G8B8A8_SRGB)?0.0f:1.0f;
  post.grade[3]=static_cast<float>(packSurfaceTransform(rhi::SurfaceTransform{}));
  post.sourceTransform[0]=post.sourceTransform[1]=1.0f;
  post.sourceTransform[2]=renderer::isOrthographic(view.frustum)?-1.0f:1.0f/view.frustum.tangentHalfVertical;
  post.sourceTransform[3]=static_cast<float>(view.width)/view.height;
  post.currentCamera[0]=post.previousCamera[0]=camera.yaw;
  post.currentCamera[1]=post.previousCamera[1]=camera.pitch;
  post.previousCamera[2]=post.previousCamera[3]=camera.roll;
  std::memcpy(post.currentPositionNear,camera.position,sizeof(camera.position));
  std::memcpy(post.previousPositionFar,camera.position,sizeof(camera.position));
  post.currentPositionNear[3]=view.frustum.nearPlane;post.previousPositionFar[3]=view.frustum.farPlane;
  vkCmdPushConstants(commandBuffer_,postPipelineLayout_,VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(post),&post);
  vkCmdDraw(commandBuffer_,3,1,0,0);
  vkCmdEndRenderPass(commandBuffer_);
  VkMemoryBarrier postAfter{};postAfter.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER;
  postAfter.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;postAfter.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(commandBuffer_,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    0,1,&postAfter,0,nullptr,0,nullptr);
  submittedPreview_=view;pendingPreview_={};
}
