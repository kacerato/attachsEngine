// Skin por compute (G6-B). Contrato em rhi/shaders/skinning.comp.
//
// Ordem no quadro: publicação (poses e paletas vindas de MapDrawState) →
// Skinning (primeira classe de GPU, antes da prévia de câmera e das sombras)
// → qualquer desenho lê a pose atual pelo buffer de saída → o passe de
// movimento lê também a posição anterior. Um desenho sem paleta no quadro
// (pose não avaliada) é deformado pela identidade: pose de bind, nunca lixo.

namespace {
struct SkinningPush {
  u32 sourceVertex, targetVertex, vertexCount, influenceVertex;
  u32 currentPalette, previousPalette, jointCount, influenceLimit;
};
static_assert(sizeof(SkinningPush) == 32);
constexpr VkDeviceSize SkinVertexBytes = renderer::MapVertexStride;
constexpr VkDeviceSize SkinMatrixBytes = 16 * sizeof(float);
} // namespace

bool InstancedRenderer::createSkinningPipeline() {
  VkDescriptorSetLayoutBinding bindings[4]{};
  for(u32 i=0;i<4;++i) bindings[i]={i,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr};
  VkDescriptorSetLayoutCreateInfo layout{};layout.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout.bindingCount=4;layout.pBindings=bindings;
  if(vkCreateDescriptorSetLayout(device_,&layout,nullptr,&skinningSetLayout_)!=VK_SUCCESS) return false;
  VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,4};
  VkDescriptorPoolCreateInfo pool{};pool.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool.maxSets=1;pool.poolSizeCount=1;pool.pPoolSizes=&size;
  if(vkCreateDescriptorPool(device_,&pool,nullptr,&skinningPool_)!=VK_SUCCESS) return false;
  VkDescriptorSetAllocateInfo allocate{};allocate.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocate.descriptorPool=skinningPool_;allocate.descriptorSetCount=1;allocate.pSetLayouts=&skinningSetLayout_;
  if(vkAllocateDescriptorSets(device_,&allocate,&skinningSet_)!=VK_SUCCESS) return false;
  VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(SkinningPush)};
  VkPipelineLayoutCreateInfo pipelineLayout{};pipelineLayout.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayout.setLayoutCount=1;pipelineLayout.pSetLayouts=&skinningSetLayout_;
  pipelineLayout.pushConstantRangeCount=1;pipelineLayout.pPushConstantRanges=&push;
  if(vkCreatePipelineLayout(device_,&pipelineLayout,nullptr,&skinningLayout_)!=VK_SUCCESS) return false;
  VkShaderModule module=createShaderModule(device_,rhi::shaders::kSkinningCompSpirv,rhi::shaders::kSkinningCompSpirvSize);
  if(!module) return false;
  VkComputePipelineCreateInfo pipeline{};pipeline.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipeline.layout=skinningLayout_;
  pipeline.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  pipeline.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;pipeline.stage.module=module;pipeline.stage.pName="main";
  const bool ok=vkCreateComputePipelines(device_,VK_NULL_HANDLE,1,&pipeline,nullptr,&skinningPipeline_)==VK_SUCCESS;
  vkDestroyShaderModule(device_,module,nullptr);
  skinningDescriptorsDirty_=true;
  return ok;
}

void InstancedRenderer::destroySkinningResources() {
  if(device_!=VK_NULL_HANDLE) {
    if(skinningPipeline_) vkDestroyPipeline(device_,skinningPipeline_,nullptr);
    if(skinningLayout_) vkDestroyPipelineLayout(device_,skinningLayout_,nullptr);
    if(skinningPool_) vkDestroyDescriptorPool(device_,skinningPool_,nullptr);
    if(skinningSetLayout_) vkDestroyDescriptorSetLayout(device_,skinningSetLayout_,nullptr);
  }
  skinningPipeline_=VK_NULL_HANDLE;skinningLayout_=VK_NULL_HANDLE;skinningPool_=VK_NULL_HANDLE;
  skinningSetLayout_=VK_NULL_HANDLE;skinningSet_=VK_NULL_HANDLE;
  skinInfluences_.reset();skinVertices_.reset();skinPalettes_.reset();
  skinnedDraws_.clear();drawSkinSlot_.clear();pendingSkins_.clear();pendingSkinsValid_=false;
  sourceSkinJoints_.clear();sourceSkinVertices_.clear();skinInfluenceVertexBase_=0;
  skinningDescriptorsDirty_=true;skinnedDrawsThisFrame_=0;
}

// Uma vez por publicação da biblioteca: juntas e alcance de vértices de cada
// desenho do pacote, e as influências na GPU. Primitivas internas são estáticas.
bool InstancedRenderer::uploadSkinningLibrary(std::span<const u8> influences, std::span<const u32> drawJoints,
                                              usize extraVertices) {
  skinnedDraws_.clear();drawSkinSlot_.clear();pendingSkins_.clear();pendingSkinsValid_=false;
  skinInfluences_.reset();
  const auto &draws=dirtRoadResources_.draws();
  const auto &indices=dirtRoadResources_.pickingIndices();
  const usize vertices=dirtRoadResources_.pickingVertices().size()/renderer::MapVertexStride;
  sourceSkinJoints_.assign(draws.size(),0);sourceSkinVertices_.assign(draws.size(),0);
  if(drawJoints.size()>draws.size() || extraVertices>vertices) return false;
  skinInfluenceVertexBase_=static_cast<u32>(vertices-extraVertices);
  const usize firstExtra=draws.size()-drawJoints.size();
  bool any=false;
  for(usize i=0;i<drawJoints.size();++i) {
    if(!drawJoints[i]) continue;
    if(influences.size()!=extraVertices*resources::SkinInfluenceStride) return false;
    const auto &draw=draws[firstExtra+i];
    u32 highest=0;
    for(u32 k=0;k<draw.indexCount;++k) highest=std::max(highest,indices[draw.firstIndex+k]);
    if(draw.vertexOffset<skinInfluenceVertexBase_ || u64(draw.vertexOffset)+highest>=vertices) return false;
    sourceSkinJoints_[firstExtra+i]=drawJoints[i];
    sourceSkinVertices_[firstExtra+i]=highest+1;
    any=true;
  }
  skinningDescriptorsDirty_=true;
  if(!any) return true;
  rhi::BufferDesc buffer{};buffer.preferDeviceMemory=true;buffer.cpuAccess=rhi::CpuAccess::None;
  buffer.sizeBytes=influences.size();
  buffer.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  return memoryAllocator_->createBuffer(buffer,&skinInfluences_) &&
         uploadContext_.uploadBuffer(*memoryAllocator_,influences.data(),influences.size(),skinInfluences_,
                                     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_READ_BIT);
}

// Na publicação de cena: uma região por desenho publicado cujo desenho de
// origem tem skin. Duas instâncias do mesmo modelo têm regiões próprias.
bool InstancedRenderer::layoutSkinnedDraws() {
  std::vector<SkinnedDraw> previous=std::move(skinnedDraws_);
  skinnedDraws_.clear();
  const u32 count=static_cast<u32>(dirtRoadResources_.draws().size());
  drawSkinSlot_.assign(count,-1);
  u64 vertexCursor=0,paletteCursor=0;
  for(u32 i=0;i<count && i<authoredDrawIdentities_.size();++i) {
    const auto &identity=authoredDrawIdentities_[i];
    const u32 source=identity.sourceDrawIndex;
    if(identity.route || source>=sourceSkinJoints_.size() || !sourceSkinJoints_[source]) continue;
    SkinnedDraw slot;
    slot.drawIndex=i;slot.sourceVertex=sourceMapDraws_[source].vertexOffset;
    slot.vertexCount=sourceSkinVertices_[source];
    slot.influenceVertex=slot.sourceVertex-skinInfluenceVertexBase_;
    slot.targetVertex=static_cast<u32>(vertexCursor);slot.joints=sourceSkinJoints_[source];
    slot.paletteBase=static_cast<u32>(paletteCursor);
    // A pose anterior sobrevive à republicação quando o mesmo desenho continua
    // no mesmo índice; senão o primeiro quadro não tem movimento de skin.
    for(const auto &old:previous)
      if(old.drawIndex==i && old.joints==slot.joints && old.hasPrevious) {slot.previous=old.previous;slot.hasPrevious=true;}
    vertexCursor+=u64(slot.vertexCount)*2;paletteCursor+=u64(slot.joints)*2;
    drawSkinSlot_[i]=static_cast<i32>(skinnedDraws_.size());
    skinnedDraws_.push_back(std::move(slot));
  }
  if(skinnedDraws_.empty()) return true;
  if(!skinningPipeline_) {
    if(!skinningUnavailableReported_)
      __android_log_print(ANDROID_LOG_ERROR,LogTag,"[Skin] compute indisponível: %zu desenhos ficam na pose de bind.",
                          skinnedDraws_.size());
    skinningUnavailableReported_=true;
    skinnedDraws_.clear();drawSkinSlot_.assign(count,-1);
    return true;
  }
  const VkDeviceSize vertexBytes=vertexCursor*SkinVertexBytes,paletteBytes=paletteCursor*SkinMatrixBytes;
  // O quadro anterior já terminou (a publicação roda depois da cerca): trocar
  // os buffers aqui não pisa em leitura nenhuma.
  if(!skinVertices_.handle() || skinVertices_.sizeBytes()<vertexBytes) {
    skinVertices_.reset();
    rhi::BufferDesc buffer{};buffer.preferDeviceMemory=true;buffer.cpuAccess=rhi::CpuAccess::None;
    buffer.sizeBytes=vertexBytes+vertexBytes/4;
    buffer.usage=VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if(!memoryAllocator_->createBuffer(buffer,&skinVertices_)) return false;
    skinningDescriptorsDirty_=true;
  }
  if(!skinPalettes_.handle() || skinPalettes_.sizeBytes()<paletteBytes) {
    skinPalettes_.reset();
    rhi::BufferDesc buffer{};buffer.preferDeviceMemory=false;buffer.cpuAccess=rhi::CpuAccess::SequentialWrite;
    buffer.sizeBytes=paletteBytes+paletteBytes/4;buffer.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    buffer.memoryClass=rhi::MemoryClass::Buffer;
    if(!memoryAllocator_->createBuffer(buffer,&skinPalettes_) || !skinPalettes_.mappedData()) return false;
    skinningDescriptorsDirty_=true;
  }
  return true;
}

void InstancedRenderer::queueSkinPalettes(std::span<const renderer::MapDrawState> draws) {
  pendingSkins_.resize(draws.size());
  for(usize i=0;i<draws.size();++i)
    pendingSkins_[i]={draws[i].skinPalette,draws[i].skinInfluences,draws[i].skinnedMotion};
  pendingSkinsValid_=true;
}

InstancedRenderer::DrawGeometry InstancedRenderer::drawGeometry(u32 drawIndex) const {
  if(drawIndex<drawSkinSlot_.size() && drawSkinSlot_[drawIndex]>=0)
    return {skinVertices_.handle(),static_cast<i32>(skinnedDraws_[static_cast<usize>(drawSkinSlot_[drawIndex])].targetVertex)};
  return {dirtRoadResources_.vertexBuffer(),static_cast<i32>(dirtRoadResources_.draws()[drawIndex].vertexOffset)};
}

bool InstancedRenderer::recordSkinning() {
  skinnedDrawsThisFrame_=0;
  if(skinnedDraws_.empty()) {pendingSkinsValid_=false;return true;}
  if(skinningDescriptorsDirty_) {
    VkDescriptorBufferInfo buffers[4]{
      {dirtRoadResources_.vertexBuffer(),0,VK_WHOLE_SIZE},{skinInfluences_.handle(),0,VK_WHOLE_SIZE},
      {skinPalettes_.handle(),0,VK_WHOLE_SIZE},{skinVertices_.handle(),0,VK_WHOLE_SIZE}};
    VkWriteDescriptorSet writes[4]{};
    for(u32 i=0;i<4;++i) {
      writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[i].dstSet=skinningSet_;writes[i].dstBinding=i;
      writes[i].descriptorCount=1;writes[i].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;writes[i].pBufferInfo=&buffers[i];
    }
    vkUpdateDescriptorSets(device_,4,writes,0,nullptr);
    skinningDescriptorsDirty_=false;
  }
  auto *palettes=static_cast<float *>(skinPalettes_.mappedData());
  if(!palettes) return false;
  bool changed=false,uncovered=false;
  for(auto &slot:skinnedDraws_) {
    const usize floats=usize(slot.joints)*16;
    float *current=palettes+usize(slot.paletteBase)*16,*previous=current+floats;
    const PendingSkin *pending=pendingSkinsValid_ && slot.drawIndex<pendingSkins_.size()?&pendingSkins_[slot.drawIndex]:nullptr;
    if(pending) {slot.influences=pending->influences;slot.motion=pending->motion;}
    if(pending && pending->palette && pending->palette->size()==floats) std::copy(pending->palette->begin(),pending->palette->end(),current);
    else if(pending || !slot.hasPrevious) {
      // Sem pose avaliada: pose de bind (identidade por junta).
      std::fill(current,current+floats,0.0f);
      for(u32 j=0;j<slot.joints;++j) current[j*16]=current[j*16+5]=current[j*16+10]=current[j*16+15]=1;
    } else std::copy(slot.previous.begin(),slot.previous.end(),current);
    const bool moved=slot.hasPrevious && !std::equal(current,current+floats,slot.previous.begin());
    if(slot.hasPrevious) std::copy(slot.previous.begin(),slot.previous.end(),previous);
    else std::copy(current,current+floats,previous);
    slot.previous.assign(current,current+floats);
    slot.hasPrevious=true;
    if(!moved) continue;
    changed=true;
    // Deformação com vetor: entra no passe de movimento com a pose anterior.
    // Sem vetor de skin (autor desligou) o histórico fica, como na Unity; sem
    // passe de movimento no aparelho, o histórico não pode reprojetar o corpo.
    if(slot.motion && motionVectorsActive_) {
      const auto begin=motionDrawIndices_.begin(),end=begin+motionDrawCount_;
      if(std::find(begin,end,slot.drawIndex)==end) {
        if(motionDrawCount_<motionDrawIndices_.size()) motionDrawIndices_[motionDrawCount_]=slot.drawIndex;
        else motionDrawIndices_.push_back(slot.drawIndex);
        ++motionDrawCount_;
      }
    } else if(!motionVectorsActive_) uncovered=true;
  }
  pendingSkinsValid_=false;
  if(!memoryAllocator_->flushBuffer(skinPalettes_)) return false;
  if(changed) shadowCascadeDirtyMask_=0xffffffffu;
  if(uncovered) temporalHistoryInitialized_=false;
  vkCmdBindPipeline(commandBuffer_,VK_PIPELINE_BIND_POINT_COMPUTE,skinningPipeline_);
  vkCmdBindDescriptorSets(commandBuffer_,VK_PIPELINE_BIND_POINT_COMPUTE,skinningLayout_,0,1,&skinningSet_,0,nullptr);
  for(const auto &slot:skinnedDraws_) {
    const SkinningPush push{slot.sourceVertex,slot.targetVertex,slot.vertexCount,slot.influenceVertex,
                            slot.paletteBase,slot.paletteBase+slot.joints,slot.joints,slot.influences};
    vkCmdPushConstants(commandBuffer_,skinningLayout_,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),&push);
    vkCmdDispatch(commandBuffer_,(slot.vertexCount+63)/64,1,1);
    ++skinnedDrawsThisFrame_;
  }
  VkBufferMemoryBarrier barrier{};barrier.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
  barrier.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
  barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
  barrier.buffer=skinVertices_.handle();barrier.size=VK_WHOLE_SIZE;
  vkCmdPipelineBarrier(commandBuffer_,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                       0,0,nullptr,1,&barrier,0,nullptr);
  return true;
}
