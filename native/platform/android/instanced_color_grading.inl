// G2 (DESEMPENHO-GRAFICOS-VIVOS-2026-10-17, item 2): LUT 32³ da gradação de cor.
//
// A tabela é montada na CPU (renderer/color_grading.cpp) só quando a aparência
// muda e sobe por cópia para uma imagem 3D RGBA16F de 256 KiB. O pós lê uma vez
// por pixel, depois do tonemap e da resolução temporal (bit 65536). Com a
// gradação neutra — padrão e cenas antigas — nada é montado nem lido e o pós é
// o de antes. Há uma fila de quadros em voo: depois do fence do acquire o quadro
// anterior já leu a imagem e a cópia, então a mesma memória de staging serve.

bool InstancedRenderer::createColorGradingResources() {
  destroyColorGradingResources();
  rhi::ImageDesc image{};
  image.width = image.height = image.depth = renderer::ColorGradingLutSize;
  image.format = VK_FORMAT_R16G16B16A16_SFLOAT;
  image.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  image.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  image.memoryClass = rhi::MemoryClass::Texture;
  rhi::BufferDesc staging{};
  staging.sizeBytes = static_cast<u64>(renderer::ColorGradingLutTexels) * 4u * sizeof(u16);
  staging.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  staging.cpuAccess = rhi::CpuAccess::SequentialWrite;
  staging.preferDeviceMemory = false;
  for (auto &slot : colorGradingSlots_) {
    if (!memoryAllocator_->createImage(image, &slot.image) ||
        !memoryAllocator_->createBuffer(staging, &slot.staging) || !slot.staging.mappedData()) {
      destroyColorGradingResources();
      return false;
    }
    slot.uploaded = false;slot.look = {};
  }
  return true;
}

void InstancedRenderer::destroyColorGradingResources() {
  for (auto &slot : colorGradingSlots_) {
    slot.image.reset();slot.staging.reset();
    slot.uploaded = false;slot.look = {};
  }
}

VkImageView InstancedRenderer::colorGradingView(u32 slot) const {
  return slot < colorGradingSlots_.size() && colorGradingSlots_[slot].image.view() != VK_NULL_HANDLE
             ? colorGradingSlots_[slot].image.view() : postSceneColor_.view();
}

bool InstancedRenderer::prepareColorGrading(u32 index, const renderer::SceneEnvironment &environment,
                                            bool authoredPost) {
  if (index >= colorGradingSlots_.size()) return false;
  auto &slot = colorGradingSlots_[index];
  if (!slot.image.isReady()) return false;
  const auto look = authoredPost ? renderer::colorGradingLook(environment) : renderer::ColorGradingLook{};
  const bool use = authoredPost && (renderer::colorGradingUsesLut(look) || colorGradingForced_);
  // A imagem precisa de layout amostrável antes do primeiro desenho, mesmo
  // quando o shader não a lê (o descritor é estático). Depois disso só sobe de
  // novo quando a aparência usada muda.
  if (slot.uploaded && (!use || slot.look == look)) return use;
  const auto target = use ? look : renderer::ColorGradingLook{};
  auto *texels = static_cast<u16 *>(slot.staging.mappedData());
  if (!renderer::buildColorGradingLutHalf(
          target, std::span<u16>(texels, static_cast<usize>(renderer::ColorGradingLutTexels) * 4u)) ||
      !memoryAllocator_->flushBuffer(slot.staging)) {
    __android_log_print(ANDROID_LOG_WARN, LogTag, "[ColorGrading] LUT recusada; pós sem gradação.");
    return false;
  }
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.srcAccessMask = slot.uploaded ? VK_ACCESS_SHADER_READ_BIT : 0;
  barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; // o conteúdo inteiro é trocado
  barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = slot.image.handle();
  barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  vkCmdPipelineBarrier(commandBuffer_, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       0, 0, nullptr, 0, nullptr, 1, &barrier);
  VkBufferImageCopy copy{};
  copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  copy.imageExtent = {renderer::ColorGradingLutSize, renderer::ColorGradingLutSize, renderer::ColorGradingLutSize};
  vkCmdCopyBufferToImage(commandBuffer_, slot.staging.handle(), slot.image.handle(),
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  vkCmdPipelineBarrier(commandBuffer_, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                       0, 0, nullptr, 0, nullptr, 1, &barrier);
  slot.look = target;slot.uploaded = true;
  if (use) ++colorGradingRebuilds_;
  return use;
}
