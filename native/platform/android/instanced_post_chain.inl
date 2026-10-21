// G1/G2 (DESEMPENHO-GRAFICOS-VIVOS-2026-10-17): bloom em cadeia de mips e AO em 1/2.
//
// Bloom: o pós antigo somava 8 vizinhos a 2 px em resolução cheia, o que só
// borrava o ponto e custava 8 leituras por pixel. Aqui o brilho é pré-filtrado
// em 1/2 e desce até 1/32 com o filtro duplo de Kawase (Bjørge, "Bandwidth-
// Efficient Rendering", ARM/SIGGRAPH 2015: 5 leituras na descida, 8 na subida),
// e cada nível de subida soma sobre o de cima. O pós lê só o nível 1/2.
//
// AO: o mesmo SSAO de 12 amostras roda em 1/2 num alvo R8; o pós reconstrói com
// as 4 amostras bilineares ponderadas pela profundidade (shader, bit 4096).
//
// Os dois são chaves de diagnóstico até o A/B no aparelho. Nenhum roda depois
// do ampliador temporal, que tem composição própria.

InstancedRenderer::PostBloomState InstancedRenderer::postBloomState() const {
  // Mesma decisão que o passe principal toma para o bit de bloom.
  const bool postEffectsEnabled=!editorBackground_||(editorSceneEffects_&&editorScenePost_);
  const auto look=effectiveSceneLook();
  const bool authoredPost=postEffectsEnabled&&look.active&&look.post;
  PostBloomState state;
  state.active=postEffectsEnabled&&!(postIsolation_&4u)&&(authoredPost?look.bloom:renderingPolicy_.post.bloom);
  state.threshold=authoredPost?look.bloomThreshold:renderingPolicy_.post.bloomThreshold;
  state.intensity=authoredPost?look.bloomIntensity:renderingPolicy_.post.bloomIntensity;
  return state;
}

VkImageView InstancedRenderer::postBloomView() const {
  return bloomLevels_[0].view()!=VK_NULL_HANDLE?bloomLevels_[0].view():postSceneColor_.view();
}
VkImageView InstancedRenderer::postAmbientOcclusionView() const {
  return halfAmbientOcclusion_.view()!=VK_NULL_HANDLE?halfAmbientOcclusion_.view():postSceneColor_.view();
}

bool InstancedRenderer::createPostChainResources() {
  if(postPipelineLayout_==VK_NULL_HANDLE||postDescriptorPool_==VK_NULL_HANDLE) return false;
  const auto createFramebuffer=[&](VkRenderPass pass,VkImageView view,u32 width,u32 height,VkFramebuffer &out) {
    VkFramebufferCreateInfo info{};info.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.renderPass=pass;info.attachmentCount=1;info.pAttachments=&view;
    info.width=width;info.height=height;info.layers=1;
    return vkCreateFramebuffer(device_,&info,nullptr,&out)==VK_SUCCESS;
  };
  rhi::ImageDesc image{};
  image.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
  image.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
  image.memoryClass=rhi::MemoryClass::RenderTarget;

  if(bloomChainEnabled_) {
    if(!createPostTargetRenderPass(VK_FORMAT_R16G16B16A16_SFLOAT,false,bloomDownRenderPass_)||
       !createPostTargetRenderPass(VK_FORMAT_R16G16B16A16_SFLOAT,true,bloomUpRenderPass_)||
       !createPostShaderPipeline(bloomDownRenderPass_,false,bloomDownPipeline_)||
       !createPostShaderPipeline(bloomUpRenderPass_,true,bloomUpPipeline_)) return false;
    u32 width=renderTargetWidth(),height=renderTargetHeight();
    image.format=VK_FORMAT_R16G16B16A16_SFLOAT;
    for(u32 level=0;level<kBloomLevelCount;++level) {
      width=std::max(1u,(width+1)/2);height=std::max(1u,(height+1)/2);
      image.width=width;image.height=height;
      if(!memoryAllocator_->createImage(image,&bloomLevels_[level])) return false;
      bloomLevelExtents_[level]={width,height};
      if(!createFramebuffer(bloomDownRenderPass_,bloomLevels_[level].view(),width,height,bloomFramebuffers_[level])) return false;
    }
  }
  if(halfAmbientOcclusionEnabled_) {
    if(!createPostTargetRenderPass(VK_FORMAT_R8_UNORM,false,halfAmbientOcclusionRenderPass_)||
       !createPostShaderPipeline(halfAmbientOcclusionRenderPass_,false,halfAmbientOcclusionPipeline_)) return false;
    image.format=VK_FORMAT_R8_UNORM;
    image.width=std::max(1u,(renderTargetWidth()+1)/2);image.height=std::max(1u,(renderTargetHeight()+1)/2);
    if(!memoryAllocator_->createImage(image,&halfAmbientOcclusion_)) return false;
    if(!createFramebuffer(halfAmbientOcclusionRenderPass_,halfAmbientOcclusion_.view(),image.width,image.height,
                          halfAmbientOcclusionFramebuffer_)) return false;
  }
  postChainLayoutsReady_=false;
  return true;
}

bool InstancedRenderer::createPostChainSets() {
  // Conjuntos de pré-passe: bloom e AO apontam para a cor da cena, nunca para
  // o próprio alvo que o passe está escrevendo.
  const VkImageView dummy=postSceneColor_.view();
  if(!allocatePostSet(dummy,dummy,dummy,postPrepassDescriptorSet_)) return false;
  if(bloomLevels_[0].view()==VK_NULL_HANDLE) return true;
  if(!allocatePostSet(dummy,dummy,dummy,bloomSceneSet_)) return false;
  if(fogResolveColor_.view()!=VK_NULL_HANDLE&&!allocatePostSet(fogResolveColor_.view(),dummy,dummy,bloomFogSet_)) return false;
  for(u32 level=0;level<kBloomLevelCount;++level)
    if(!allocatePostSet(bloomLevels_[level].view(),dummy,dummy,bloomLevelSets_[level])) return false;
  return true;
}

void InstancedRenderer::destroyPostChainResources() {
  if(device_!=VK_NULL_HANDLE) {
    for(auto &framebuffer:bloomFramebuffers_) if(framebuffer) vkDestroyFramebuffer(device_,framebuffer,nullptr);
    if(halfAmbientOcclusionFramebuffer_) vkDestroyFramebuffer(device_,halfAmbientOcclusionFramebuffer_,nullptr);
    for(VkPipeline pipeline:{bloomDownPipeline_,bloomUpPipeline_,halfAmbientOcclusionPipeline_})
      if(pipeline) vkDestroyPipeline(device_,pipeline,nullptr);
    for(VkRenderPass pass:{bloomDownRenderPass_,bloomUpRenderPass_,halfAmbientOcclusionRenderPass_})
      if(pass) vkDestroyRenderPass(device_,pass,nullptr);
  }
  bloomFramebuffers_.fill(VK_NULL_HANDLE);halfAmbientOcclusionFramebuffer_=VK_NULL_HANDLE;
  bloomDownPipeline_=bloomUpPipeline_=halfAmbientOcclusionPipeline_=VK_NULL_HANDLE;
  bloomDownRenderPass_=bloomUpRenderPass_=halfAmbientOcclusionRenderPass_=VK_NULL_HANDLE;
  // Conjuntos pertencem ao pool do pós.
  bloomLevelSets_.fill(VK_NULL_HANDLE);bloomSceneSet_=bloomFogSet_=postPrepassDescriptorSet_=VK_NULL_HANDLE;
  for(auto &level:bloomLevels_) level.reset();
  halfAmbientOcclusion_.reset();
  bloomChainThisFrame_=halfAmbientOcclusionThisFrame_=false;
  postChainLayoutsReady_=false;
}

void InstancedRenderer::preparePostChainTargets() {
  // O conjunto do pós referencia o nível 1/2 e a AO desde a criação; antes da
  // primeira escrita eles precisam de um layout amostrável válido.
  if(postChainLayoutsReady_) return;
  for(auto &level:bloomLevels_)
    if(level.handle()!=VK_NULL_HANDLE)
      imageBarrier(commandBuffer_,level.handle(),VK_IMAGE_ASPECT_COLOR_BIT,VK_IMAGE_LAYOUT_UNDEFINED,
                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,0,
                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_ACCESS_SHADER_READ_BIT);
  if(halfAmbientOcclusion_.handle()!=VK_NULL_HANDLE)
    imageBarrier(commandBuffer_,halfAmbientOcclusion_.handle(),VK_IMAGE_ASPECT_COLOR_BIT,VK_IMAGE_LAYOUT_UNDEFINED,
                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,0,
                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_ACCESS_SHADER_READ_BIT);
  postChainLayoutsReady_=true;
}

void InstancedRenderer::recordPostChainPass(VkRenderPass pass,VkFramebuffer framebuffer,VkPipeline pipeline,
                                            VkDescriptorSet set,u32 width,u32 height,const PostPushConstants &push) {
  VkRenderPassBeginInfo begin{};begin.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  begin.renderPass=pass;begin.framebuffer=framebuffer;begin.renderArea.extent={width,height};
  vkCmdBeginRenderPass(commandBuffer_,&begin,VK_SUBPASS_CONTENTS_INLINE);
  const VkViewport viewport{0,0,static_cast<float>(width),static_cast<float>(height),0,1};
  const VkRect2D scissor{{0,0},{width,height}};
  vkCmdSetViewport(commandBuffer_,0,1,&viewport);vkCmdSetScissor(commandBuffer_,0,1,&scissor);
  vkCmdBindPipeline(commandBuffer_,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
  vkCmdBindDescriptorSets(commandBuffer_,VK_PIPELINE_BIND_POINT_GRAPHICS,postPipelineLayout_,0,1,&set,0,nullptr);
  vkCmdPushConstants(commandBuffer_,postPipelineLayout_,VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(push),&push);
  vkCmdDraw(commandBuffer_,3,1,0,0);
  vkCmdEndRenderPass(commandBuffer_);
}

bool InstancedRenderer::recordHalfAmbientOcclusion(const platform::FreeCameraState &camera) {
  halfAmbientOcclusionThisFrame_=false;
  if(!halfAmbientOcclusionEnabled_||halfAmbientOcclusionPipeline_==VK_NULL_HANDLE||!aoActiveThisFrame_||
     postPrepassDescriptorSet_==VK_NULL_HANDLE) return false;
  PostPushConstants push{};
  fillSceneAuxPush(camera,push);
  push.texelFlags[2]=2048.0f;
  recordPostChainPass(halfAmbientOcclusionRenderPass_,halfAmbientOcclusionFramebuffer_,halfAmbientOcclusionPipeline_,
                      postPrepassDescriptorSet_,std::max(1u,(renderWidth()+1)/2),std::max(1u,(renderHeight()+1)/2),push);
  halfAmbientOcclusionThisFrame_=true;
  return true;
}

bool InstancedRenderer::recordBloomChain(const platform::FreeCameraState &camera,float threshold) {
  bloomChainThisFrame_=false;
  if(!bloomChainEnabled_||bloomDownPipeline_==VK_NULL_HANDLE||bloomSceneSet_==VK_NULL_HANDLE) return false;
  // Extensão ativa de cada nível segue a resolução dinâmica; a alocação, não.
  std::array<VkExtent2D,kBloomLevelCount> active{};
  u32 width=renderWidth(),height=renderHeight();
  for(u32 level=0;level<kBloomLevelCount;++level) {
    width=std::max(1u,(width+1)/2);height=std::max(1u,(height+1)/2);
    active[level]={std::min(width,bloomLevelExtents_[level].width),std::min(height,bloomLevelExtents_[level].height)};
  }
  PostPushConstants push{};
  fillSceneAuxPush(camera,push);
  push.bloom[0]=threshold;
  // Descida: a fonte do nível 0 é a cena (com névoa resolvida, quando houver).
  for(u32 level=0;level<kBloomLevelCount;++level) {
    VkDescriptorSet source;
    if(level==0) {
      source=fogResolvedThisFrame_&&bloomFogSet_?bloomFogSet_:bloomSceneSet_;
      push.texelFlags[0]=1.0f/static_cast<float>(renderTargetWidth());
      push.texelFlags[1]=1.0f/static_cast<float>(renderTargetHeight());
      push.sourceTransform[0]=static_cast<float>(renderWidth())/static_cast<float>(renderTargetWidth());
      push.sourceTransform[1]=static_cast<float>(renderHeight())/static_cast<float>(renderTargetHeight());
      push.texelFlags[2]=8192.0f+16384.0f;
    } else {
      source=bloomLevelSets_[level-1];
      push.texelFlags[0]=1.0f/static_cast<float>(bloomLevelExtents_[level-1].width);
      push.texelFlags[1]=1.0f/static_cast<float>(bloomLevelExtents_[level-1].height);
      push.sourceTransform[0]=static_cast<float>(active[level-1].width)/static_cast<float>(bloomLevelExtents_[level-1].width);
      push.sourceTransform[1]=static_cast<float>(active[level-1].height)/static_cast<float>(bloomLevelExtents_[level-1].height);
      push.texelFlags[2]=8192.0f;
    }
    recordPostChainPass(bloomDownRenderPass_,bloomFramebuffers_[level],bloomDownPipeline_,source,
                        active[level].width,active[level].height,push);
  }
  // Subida: cada nível soma a tenda do nível de baixo, até o nível 1/2.
  push.texelFlags[2]=32768.0f;
  for(u32 level=kBloomLevelCount-1;level>0;--level) {
    push.texelFlags[0]=1.0f/static_cast<float>(bloomLevelExtents_[level].width);
    push.texelFlags[1]=1.0f/static_cast<float>(bloomLevelExtents_[level].height);
    push.sourceTransform[0]=static_cast<float>(active[level].width)/static_cast<float>(bloomLevelExtents_[level].width);
    push.sourceTransform[1]=static_cast<float>(active[level].height)/static_cast<float>(bloomLevelExtents_[level].height);
    recordPostChainPass(bloomUpRenderPass_,bloomFramebuffers_[level-1],bloomUpPipeline_,bloomLevelSets_[level],
                        active[level-1].width,active[level-1].height,push);
  }
  bloomChainThisFrame_=true;
  return true;
}
