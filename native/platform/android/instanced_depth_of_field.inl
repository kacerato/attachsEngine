// PK-5: Depth of Field da URP 17 (Gaussian e Bokeh) e a sujeira da lente.
//
// Gaussian (GaussianDepthOfField.shader): pré-filtro em 1/2 com o CoC do
// fundo (0 a 1 entre Start e End), desfoque separável de 9 pesos em 5
// leituras na horizontal e na vertical, composição pela mistura com o CoC
// do pixel. Três passes em meia resolução: o modo barato, como na URP.
//
// Bokeh (BokehDepthOfField.shader, herdado do PPv2): pré-filtro com o CoC
// com sinal (frente negativa), um passe de disco com o kernel de lâminas do
// PrepareBokehKernel (8, 22 ou 43 amostras pela Qualidade) e a tenda de 4
// leituras. A composição usa o CoC de resolução cheia, como o PPv2.
//
// Os passes são o shader do pós num modo (bits 131072..1048576), no mesmo
// alvo RGBA16F; o resultado termina sempre no alvo 0, que o passe principal
// lê (binding 11, bit 2097152). Não roda com o ampliador temporal (a entrada
// do pós já vem composta na resolução final) nem sem o passe dedicado.

VkImageView InstancedRenderer::depthOfFieldView() const {
  return depthOfFieldTargets_[0].view()!=VK_NULL_HANDLE?depthOfFieldTargets_[0].view():postSceneColor_.view();
}
VkImageView InstancedRenderer::lensDirtView() const {
  return lensDirtImage_.view()!=VK_NULL_HANDLE?lensDirtImage_.view():postSceneColor_.view();
}

bool InstancedRenderer::createDepthOfFieldResources() {
  if(postPipelineLayout_==VK_NULL_HANDLE||postDescriptorPool_==VK_NULL_HANDLE) return false;
  if(!createPostTargetRenderPass(VK_FORMAT_R16G16B16A16_SFLOAT,false,depthOfFieldRenderPass_)||
     !createPostShaderPipeline(depthOfFieldRenderPass_,false,depthOfFieldPipeline_)) return false;
  rhi::ImageDesc image{};
  image.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
  image.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
  image.memoryClass=rhi::MemoryClass::RenderTarget;
  image.format=VK_FORMAT_R16G16B16A16_SFLOAT;
  image.width=std::max(1u,(renderTargetWidth()+1)/2);image.height=std::max(1u,(renderTargetHeight()+1)/2);
  depthOfFieldExtent_={image.width,image.height};
  for(u32 index=0;index<2;++index) {
    if(!memoryAllocator_->createImage(image,&depthOfFieldTargets_[index])) return false;
    const VkImageView view=depthOfFieldTargets_[index].view();
    VkFramebufferCreateInfo framebuffer{};framebuffer.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer.renderPass=depthOfFieldRenderPass_;framebuffer.attachmentCount=1;framebuffer.pAttachments=&view;
    framebuffer.width=image.width;framebuffer.height=image.height;framebuffer.layers=1;
    if(vkCreateFramebuffer(device_,&framebuffer,nullptr,&depthOfFieldFramebuffers_[index])!=VK_SUCCESS) return false;
  }
  // Sujeira: 512² sRGB (o pós lê em linear) e o staging de 1 MiB da troca.
  rhi::ImageDesc dirt{};
  dirt.width=dirt.height=renderer::LensDirtSize;dirt.format=VK_FORMAT_R8G8B8A8_SRGB;
  dirt.usage=VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  dirt.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;dirt.memoryClass=rhi::MemoryClass::Texture;
  rhi::BufferDesc staging{};
  staging.sizeBytes=static_cast<u64>(renderer::LensDirtSize)*renderer::LensDirtSize*4u;
  staging.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT;staging.cpuAccess=rhi::CpuAccess::SequentialWrite;
  staging.preferDeviceMemory=false;
  if(!memoryAllocator_->createImage(dirt,&lensDirtImage_)||!memoryAllocator_->createBuffer(staging,&lensDirtStaging_)||
     !lensDirtStaging_.mappedData()) return false;
  lensDirtActive_={};lensDirtAspect_=0.0f;lensDirtPending_=false;
  depthOfFieldLayoutsReady_=false;lensDirtLayoutReady_=false;
  return true;
}

bool InstancedRenderer::createDepthOfFieldSets() {
  if(depthOfFieldTargets_[0].view()==VK_NULL_HANDLE) return true;
  const VkImageView dummy=postSceneColor_.view();
  if(!allocatePostSet(postSceneColor_.view(),dummy,dummy,depthOfFieldSceneSet_)) return false;
  if(fogResolveColor_.view()!=VK_NULL_HANDLE&&!allocatePostSet(fogResolveColor_.view(),dummy,dummy,depthOfFieldFogSet_)) return false;
  for(u32 index=0;index<2;++index)
    if(!allocatePostSet(depthOfFieldTargets_[index].view(),dummy,dummy,depthOfFieldSets_[index])) return false;
  return true;
}

void InstancedRenderer::destroyDepthOfFieldResources() {
  if(device_!=VK_NULL_HANDLE) {
    for(auto &framebuffer:depthOfFieldFramebuffers_) if(framebuffer) vkDestroyFramebuffer(device_,framebuffer,nullptr);
    if(depthOfFieldPipeline_) vkDestroyPipeline(device_,depthOfFieldPipeline_,nullptr);
    if(depthOfFieldRenderPass_) vkDestroyRenderPass(device_,depthOfFieldRenderPass_,nullptr);
  }
  depthOfFieldFramebuffers_.fill(VK_NULL_HANDLE);depthOfFieldPipeline_=VK_NULL_HANDLE;depthOfFieldRenderPass_=VK_NULL_HANDLE;
  // Conjuntos pertencem ao pool do pós.
  depthOfFieldSets_.fill(VK_NULL_HANDLE);depthOfFieldSceneSet_=depthOfFieldFogSet_=VK_NULL_HANDLE;
  for(auto &target:depthOfFieldTargets_) target.reset();
  lensDirtImage_.reset();lensDirtStaging_.reset();
  lensDirtActive_={};lensDirtAspect_=0.0f;lensDirtPending_=false;
  depthOfFieldThisFrame_=depthOfFieldLayoutsReady_=lensDirtLayoutReady_=false;
}

// Na preparação do quadro (CPU): escolhe a textura de sujeira do look e, se
// mudou, copia os texels para o staging. A cópia para a GPU sai no início do
// buffer de comandos (recordDepthOfFieldUploads).
float InstancedRenderer::prepareLensDirt(const renderer::SceneEnvironment &look) {
  if(!lensDirtStaging_.mappedData()) return 0.0f;
  if(!look.active||!look.post||!look.bloomDirt.valid()||look.bloomDirtIntensity<=0.0f) return 0.0f;
  if(look.bloomDirt==lensDirtActive_) return lensDirtAspect_;
  renderer::SharedLensDirt selected;
  for(const auto &[guid,texture]:lensDirtTextures_) if(guid==look.bloomDirt) {selected=texture;break;}
  if(!selected||!selected->valid()) return 0.0f;
  std::memcpy(lensDirtStaging_.mappedData(),selected->rgba.data(),selected->rgba.size());
  if(!memoryAllocator_->flushBuffer(lensDirtStaging_)) return 0.0f;
  lensDirtActive_=look.bloomDirt;lensDirtAspect_=selected->aspect;lensDirtPending_=true;
  // O quadro em que a textura sobe ainda lê a antiga; a sujeira aparece no próximo.
  return 0.0f;
}

void InstancedRenderer::recordDepthOfFieldUploads() {
  // Layout amostrável antes do primeiro desenho (os descritores são estáticos).
  if(!depthOfFieldLayoutsReady_) {
    for(auto &target:depthOfFieldTargets_) if(target.handle()!=VK_NULL_HANDLE)
      imageBarrier(commandBuffer_,target.handle(),VK_IMAGE_ASPECT_COLOR_BIT,VK_IMAGE_LAYOUT_UNDEFINED,
                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,0,
                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_ACCESS_SHADER_READ_BIT);
    depthOfFieldLayoutsReady_=true;
  }
  if(lensDirtImage_.handle()==VK_NULL_HANDLE) return;
  if(!lensDirtLayoutReady_&&!lensDirtPending_) {
    imageBarrier(commandBuffer_,lensDirtImage_.handle(),VK_IMAGE_ASPECT_COLOR_BIT,VK_IMAGE_LAYOUT_UNDEFINED,
                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,0,
                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_ACCESS_SHADER_READ_BIT);
    lensDirtLayoutReady_=true;
  }
  if(!lensDirtPending_) return;
  imageBarrier(commandBuffer_,lensDirtImage_.handle(),VK_IMAGE_ASPECT_COLOR_BIT,VK_IMAGE_LAYOUT_UNDEFINED,
               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
               lensDirtLayoutReady_?VK_ACCESS_SHADER_READ_BIT:0,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT);
  VkBufferImageCopy copy{};
  copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
  copy.imageExtent={renderer::LensDirtSize,renderer::LensDirtSize,1};
  vkCmdCopyBufferToImage(commandBuffer_,lensDirtStaging_.handle(),lensDirtImage_.handle(),
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
  imageBarrier(commandBuffer_,lensDirtImage_.handle(),VK_IMAGE_ASPECT_COLOR_BIT,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,
               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_ACCESS_SHADER_READ_BIT);
  lensDirtLayoutReady_=true;lensDirtPending_=false;
}

bool InstancedRenderer::recordDepthOfField(const platform::FreeCameraState &camera) {
  depthOfFieldThisFrame_=false;
  if(depthOfFieldPipeline_==VK_NULL_HANDLE||depthOfFieldSceneSet_==VK_NULL_HANDLE||!viewPostEnabled()) return false;
  const auto look=effectiveSceneLook();
  if(!look.active||!look.post||look.depthOfField==renderer::DepthOfFieldMode::Off) return false;
  const bool bokeh=look.depthOfField==renderer::DepthOfFieldMode::Bokeh;
  const VkExtent2D active{std::min(std::max(1u,(renderWidth()+1)/2),depthOfFieldExtent_.width),
                          std::min(std::max(1u,(renderHeight()+1)/2),depthOfFieldExtent_.height)};
  PostPushConstants push{};
  fillSceneAuxPush(camera,push);
  // Pré-filtro: a cena (com a névoa resolvida, quando houver) em 1/2.
  push.texelFlags[2]=131072.0f;
  recordPostChainPass(depthOfFieldRenderPass_,depthOfFieldFramebuffers_[0],depthOfFieldPipeline_,
                      fogResolvedThisFrame_&&depthOfFieldFogSet_?depthOfFieldFogSet_:depthOfFieldSceneSet_,
                      active.width,active.height,push);
  push.texelFlags[0]=1.0f/static_cast<float>(depthOfFieldExtent_.width);
  push.texelFlags[1]=1.0f/static_cast<float>(depthOfFieldExtent_.height);
  push.sourceTransform[0]=static_cast<float>(active.width)/static_cast<float>(depthOfFieldExtent_.width);
  push.sourceTransform[1]=static_cast<float>(active.height)/static_cast<float>(depthOfFieldExtent_.height);
  if(bokeh) {
    push.texelFlags[2]=524288.0f;
    recordPostChainPass(depthOfFieldRenderPass_,depthOfFieldFramebuffers_[1],depthOfFieldPipeline_,depthOfFieldSets_[0],
                        active.width,active.height,push);
    push.texelFlags[2]=1048576.0f;
    recordPostChainPass(depthOfFieldRenderPass_,depthOfFieldFramebuffers_[0],depthOfFieldPipeline_,depthOfFieldSets_[1],
                        active.width,active.height,push);
  } else {
    push.texelFlags[2]=262144.0f;
    push.bloom[0]=1.0f;push.bloom[1]=0.0f;
    recordPostChainPass(depthOfFieldRenderPass_,depthOfFieldFramebuffers_[1],depthOfFieldPipeline_,depthOfFieldSets_[0],
                        active.width,active.height,push);
    push.bloom[0]=0.0f;push.bloom[1]=1.0f;
    recordPostChainPass(depthOfFieldRenderPass_,depthOfFieldFramebuffers_[0],depthOfFieldPipeline_,depthOfFieldSets_[1],
                        active.width,active.height,push);
  }
  depthOfFieldThisFrame_=true;
  if(!depthOfFieldReported_) {
    depthOfFieldReported_=true;
    __android_log_print(ANDROID_LOG_INFO,LogTag,"[DepthOfField] %s em %ux%u.",bokeh?"bokeh":"gaussiano",active.width,active.height);
  }
  return true;
}
