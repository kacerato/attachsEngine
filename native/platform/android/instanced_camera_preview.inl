// Included inside the renderer namespace; keeps frame ABI in one translation unit.
void InstancedRenderer::destroyCameraPreview() {
  uiRenderer_.setCameraPreview(VK_NULL_HANDLE);
  if(previewFramebuffer_) vkDestroyFramebuffer(device_,previewFramebuffer_,nullptr);
  if(previewPool_) vkDestroyDescriptorPool(device_,previewPool_,nullptr);
  previewFramebuffer_=VK_NULL_HANDLE;previewPool_=VK_NULL_HANDLE;previewSet_=VK_NULL_HANDLE;
  previewUniform_.reset();previewColor_.reset();previewDepth_.reset();
}
bool InstancedRenderer::prepareCameraPreview() {
  if(!pendingPreview_.valid() || pendingPreview_.width>4096 || pendingPreview_.height>4096 ||
     !dirtRoadPreview_ || !environmentUniform_.isReady() || !previewRenderPass_) return false;
  if(!previewFramebuffer_ || previewColor_.width()!=pendingPreview_.width || previewColor_.height()!=pendingPreview_.height) {
    destroyCameraPreview();
    rhi::ImageDesc color{};color.width=pendingPreview_.width;color.height=pendingPreview_.height;
    color.format=swapchain_->imageFormat();color.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
    color.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;color.memoryClass=rhi::MemoryClass::RenderTarget;
    if(!memoryAllocator_->createImage(color,&previewColor_)) return false;
    auto depth=color;depth.format=depthFormat_;depth.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if(waterSubpassActive_) depth.usage|=VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    if(frameAttachmentPolicy_.depthSampled) depth.usage|=VK_IMAGE_USAGE_SAMPLED_BIT;
    depth.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;
    if(hasStencil(depthFormat_)) depth.aspectMask|=VK_IMAGE_ASPECT_STENCIL_BIT;
    if(!memoryAllocator_->createImage(depth,&previewDepth_)) return false;
    VkImageView attachments[]{previewColor_.view(),previewDepth_.view()};
    VkFramebufferCreateInfo fb{};fb.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb.renderPass=previewRenderPass_;fb.attachmentCount=2;fb.pAttachments=attachments;
    fb.width=color.width;fb.height=color.height;fb.layers=1;
    if(vkCreateFramebuffer(device_,&fb,nullptr,&previewFramebuffer_)!=VK_SUCCESS) return false;
    rhi::BufferDesc buffer{};buffer.sizeBytes=sizeof(DirtRoadFrameUniform);
    buffer.usage=VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;buffer.cpuAccess=rhi::CpuAccess::SequentialWrite;buffer.preferDeviceMemory=false;
    if(!memoryAllocator_->createBuffer(buffer,&previewUniform_)) {destroyCameraPreview();return false;}
    VkDescriptorPoolSize sizes[]{{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,9},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,6},{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,1}};
    VkDescriptorPoolCreateInfo pool{};pool.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool.maxSets=1;pool.poolSizeCount=4;pool.pPoolSizes=sizes;
    if(vkCreateDescriptorPool(device_,&pool,nullptr,&previewPool_)!=VK_SUCCESS) {destroyCameraPreview();return false;}
    VkDescriptorSetAllocateInfo allocate{};allocate.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate.descriptorPool=previewPool_;allocate.descriptorSetCount=1;allocate.pSetLayouts=&environmentSetLayout_;
    if(vkAllocateDescriptorSets(device_,&allocate,&previewSet_)!=VK_SUCCESS) {destroyCameraPreview();return false;}
    uiRenderer_.setCameraPreview(previewColor_.view());
  }
  std::vector<VkCopyDescriptorSet> copies;
  for(u32 binding=1;binding<=16;++binding) {
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
  auto frame=*static_cast<const DirtRoadFrameUniform*>(environmentUniform_.mappedData());
  const auto &f=pendingPreview_.frustum;const auto basis=renderer::buildCameraViewBasis(f.yaw,f.pitch,f.roll);
  std::copy(basis.row0,basis.row0+3,frame.worldToViewRow0);std::copy(basis.row1,basis.row1+3,frame.worldToViewRow1);
  std::copy(basis.row2,basis.row2+3,frame.worldToViewRow2);
  frame.worldToViewRow0[3]=renderer::isOrthographic(f)?f.orthographicHalfHeight:0;
  frame.shadowParameters[1]=0;frame.shadowFilterParameters[2]=frame.shadowFilterParameters[3]=0;
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
  const bool encodeSrgb=previewColor_.format()!=VK_FORMAT_B8G8R8A8_SRGB && previewColor_.format()!=VK_FORMAT_R8G8B8A8_SRGB;
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
  if(waterSubpassActive_) vkCmdNextSubpass(commandBuffer_,VK_SUBPASS_CONTENTS_INLINE);
  const auto distance=[&](u32 index){float result=0;const auto &r=dirtRoadResources_.draws()[index];
    for(u32 k=0;k<3;++k) {const float d=r.boundsCenter[k]-camera.position[k];result+=d*d;}return result;};
  std::sort(transparent.begin(),transparent.end(),[&](u32 a,u32 b){return distance(a)>distance(b);});
  for(auto index:transparent) draw(index,true);
  vkCmdEndRenderPass(commandBuffer_);
  VkMemoryBarrier after{};after.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER;
  after.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;after.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(commandBuffer_,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    0,1,&after,0,nullptr,0,nullptr);
  submittedPreview_=view;pendingPreview_={};
}
