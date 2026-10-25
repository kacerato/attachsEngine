// Incluído dentro do namespace do renderer, depois de instanced_probes.inl.
//
// G3c — lightmaps assados no aparelho.
//
// As páginas do recurso .aelightmap viram uma imagem 2D array (uma camada por
// página; RGBA8 com raiz quadrada ou RGBA16F), lida no binding 26 do conjunto
// de ambiente. Cada desenho que recebe lightmap carrega a página e o
// lightmapScaleOffset no registro de sondas (binding 24), ao lado do SH e da
// mistura de reflexo: o fragmento faz uma busca a mais e nenhuma pipeline nova
// existe. Sem lightmap, o binding aponta para uma camada 1x1 preta e o bit 2 do
// cabeçalho fica desligado — o shader cai exatamente no caminho de antes.

void InstancedRenderer::setLightmapData(std::shared_ptr<const renderer::LightmapData> data) {
  lightmapData_ = std::move(data);
}

bool InstancedRenderer::createLightmapResources() {
  if (lightmapAtlas_.isReady()) return true;
  rhi::ImageDesc desc{};
  desc.width = desc.height = 1;
  desc.mipLevels = 1;
  desc.arrayLayers = 1;
  desc.format = VK_FORMAT_R8G8B8A8_UNORM;
  desc.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  desc.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  if (!memoryAllocator_->createImage(desc, &lightmapAtlas_)) return false;
  if (!lightmapSampler_.isReady()) {
    rhi::SamplerDesc sampler{};
    sampler.minFilter = sampler.magFilter = VK_FILTER_LINEAR;
    sampler.addressU = sampler.addressV = sampler.addressW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.maxLod = 0;
    if (!lightmapSampler_.initialize(device_, sampler)) return false;
  }
  lightmapLayers_ = 0;
  lightmapUploadPending_ = true; // a camada vazia é limpa no primeiro quadro
  lightmapResident_.reset();
  return true;
}

void InstancedRenderer::destroyLightmapResources() {
  lightmapAtlas_.reset();
  lightmapRetired_.reset();
  lightmapUpload_.reset();
  lightmapSampler_.shutdown();
  lightmapResident_.reset();
  lightmapLayers_ = 0;
  lightmapUploadPending_ = false;
}

void InstancedRenderer::writeLightmapDescriptors() {
  if (!lightmapAtlas_.isReady()) return;
  VkDescriptorImageInfo info{lightmapSampler_.handle(), lightmapAtlas_.view(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  std::vector<VkWriteDescriptorSet> writes;
  const auto add = [&](VkDescriptorSet set) {
    if (set == VK_NULL_HANDLE) return;
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set;
    write.dstBinding = 26;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &info;
    writes.push_back(write);
  };
  add(environmentSet_);
  add(previewSet_);
  for (VkDescriptorSet set : probeFaceSets_) add(set);
  if (!writes.empty()) vkUpdateDescriptorSets(device_, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
}

// Chamado com a cerca do quadro anterior cumprida (prepareProbes): trocar a
// imagem e reescrever os descritores aqui não corre contra a GPU.
bool InstancedRenderer::prepareLightmaps() {
  lightmapStats_.drawsLightmapped = 0;
  if (lightmapData_ == lightmapResident_ || environmentSet_ == VK_NULL_HANDLE) return true;
  const auto start = std::chrono::steady_clock::now();
  const auto data = lightmapData_;
  lightmapRetired_ = std::move(lightmapAtlas_);
  rhi::ImageDesc desc{};
  const bool usable = data && !data->pages.empty() && data->valid();
  const renderer::LightmapPage *first = usable ? &data->pages.front() : nullptr;
  desc.width = first ? first->width : 1;
  desc.height = first ? first->height : 1;
  desc.mipLevels = 1;
  desc.arrayLayers = usable ? static_cast<u32>(data->pages.size()) : 1;
  desc.format = first && first->format == renderer::LightmapPage::FormatHalf ? VK_FORMAT_R16G16B16A16_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM;
  desc.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  desc.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  if (!memoryAllocator_->createImage(desc, &lightmapAtlas_)) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "[Lightmap] sem memória para %u página(s) %ux%u", desc.arrayLayers, desc.width, desc.height);
    lightmapAtlas_ = std::move(lightmapRetired_);
    lightmapData_ = lightmapResident_;
    return true;
  }
  if (usable) {
    const u64 bytes = data->gpuBytes();
    if (lightmapUpload_.sizeBytes() < bytes) {
      lightmapUpload_.reset();
      rhi::BufferDesc staging{};
      staging.sizeBytes = bytes;
      staging.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
      staging.cpuAccess = rhi::CpuAccess::SequentialWrite;
      staging.preferDeviceMemory = false;
      if (!memoryAllocator_->createBuffer(staging, &lightmapUpload_)) return false;
    }
    u64 offset = 0;
    for (const auto &page : data->pages) {
      std::memcpy(static_cast<u8 *>(lightmapUpload_.mappedData()) + offset, page.texels.data(), page.texels.size());
      offset += page.texels.size();
    }
    if (!memoryAllocator_->flushBuffer(lightmapUpload_)) return false;
  }
  lightmapLayers_ = usable ? desc.arrayLayers : 0;
  lightmapUploadPending_ = true;
  lightmapResident_ = data;
  ++lightmapRevision_;
  writeLightmapDescriptors();
  lightmapStats_.pages = lightmapLayers_;
  lightmapStats_.gpuBytes = usable ? data->gpuBytes() : 0;
  lightmapStats_.uploadMs = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start).count();
  __android_log_print(ANDROID_LOG_INFO, LogTag, "[Lightmap] residente: %u página(s) %ux%u %s, %.1f MB, %.2f ms",
                      lightmapLayers_, desc.width, desc.height, desc.format == VK_FORMAT_R8G8B8A8_UNORM ? "RGBA8" : "RGBA16F",
                      static_cast<double>(lightmapStats_.gpuBytes) / (1024.0 * 1024.0), static_cast<double>(lightmapStats_.uploadMs));
  return true;
}

void InstancedRenderer::recordLightmaps() {
  if (!lightmapUploadPending_ || !lightmapAtlas_.isReady()) return;
  const u32 layers = std::max<u32>(lightmapLayers_, 1);
  probeImageBarrier(commandBuffer_, lightmapAtlas_.handle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
                    VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, 0, layers);
  if (lightmapLayers_ && lightmapResident_) {
    std::vector<VkBufferImageCopy> regions;
    u64 offset = 0;
    for (u32 layer = 0; layer < lightmapLayers_; ++layer) {
      const auto &page = lightmapResident_->pages[layer];
      VkBufferImageCopy region{};
      region.bufferOffset = offset;
      region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, layer, 1};
      region.imageExtent = {page.width, page.height, 1};
      regions.push_back(region);
      offset += page.texels.size();
    }
    vkCmdCopyBufferToImage(commandBuffer_, lightmapUpload_.handle(), lightmapAtlas_.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           static_cast<u32>(regions.size()), regions.data());
  } else {
    const VkClearColorValue black{{0, 0, 0, 0}};
    const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdClearColorImage(commandBuffer_, lightmapAtlas_.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &black, 1, &range);
  }
  probeImageBarrier(commandBuffer_, lightmapAtlas_.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, 0, layers);
  lightmapUploadPending_ = false;
}
