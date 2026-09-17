#include "rhi/ui_renderer.h"

#include "rhi/shaders/astra_ui_spirv.h"
#include "rhi/upload_context.h"

#include <cstring>
#include <vector>

namespace ae::rhi {
namespace {

VkShaderModule createModule(VkDevice device, const u32 *spirv, usize bytes) {
  VkShaderModuleCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  info.codeSize = bytes;
  info.pCode = spirv;
  VkShaderModule module = VK_NULL_HANDLE;
  if (vkCreateShaderModule(device, &info, nullptr, &module) != VK_SUCCESS) return VK_NULL_HANDLE;
  return module;
}

// O campo de distância é de canal único, mas o upload tipado do RHI fala RGBA8.
// Expandir aqui custa meio MiB temporário na inicialização e evita um segundo
// caminho de upload — o atlas é enviado uma vez e o buffer some em seguida.
std::vector<u8> expandToRgba(std::span<const u8> single) {
  std::vector<u8> expanded(single.size() * 4);
  for (usize index = 0; index < single.size(); ++index) {
    const u8 value = single[index];
    expanded[index * 4 + 0] = value;
    expanded[index * 4 + 1] = value;
    expanded[index * 4 + 2] = value;
    expanded[index * 4 + 3] = 255;
  }
  return expanded;
}

} // namespace

VulkanUiRenderer::~VulkanUiRenderer() { shutdown(); }

void VulkanUiRenderer::shutdown() {
  if (device_ != VK_NULL_HANDLE) {
    if (pipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, pipeline_, nullptr);
    if (pipelineLayout_ != VK_NULL_HANDLE)
      vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
    if (descriptorPool_ != VK_NULL_HANDLE)
      vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
    if (descriptorLayout_ != VK_NULL_HANDLE)
      vkDestroyDescriptorSetLayout(device_, descriptorLayout_, nullptr);
  }
  pipeline_ = VK_NULL_HANDLE;
  pipelineLayout_ = VK_NULL_HANDLE;
  descriptorPool_ = VK_NULL_HANDLE;
  descriptorLayout_ = VK_NULL_HANDLE;
  descriptorSet_ = VK_NULL_HANDLE;
  sampler_.shutdown();
  fontAtlas_.reset();
  iconAtlas_.reset();
  previewAtlas_.reset();
  cameraPreviewView_=VK_NULL_HANDLE;
  instanceBuffer_.reset();
  allocator_ = nullptr;
  device_ = VK_NULL_HANDLE;
  capacity_ = 0;
}

bool VulkanUiRenderer::createAtlas(VulkanMemoryAllocator &allocator, VulkanUploadContext &upload,
                                   const ui::UiFont &font, const ui::UiIconAtlas &icons) {
  ImageDesc fontDesc{};
  fontDesc.width = font.atlasWidth();
  fontDesc.height = font.atlasHeight();
  // O campo é um número, não uma cor: UNORM e nunca SRGB. Aplicar a curva sRGB
  // a uma distância distorceria a borda de todo glifo, e o efeito seria um texto
  // sistematicamente fino demais que ninguém saberia atribuir ao formato.
  fontDesc.format = VK_FORMAT_R8G8B8A8_UNORM;
  fontDesc.mipLevels = 1;
  fontDesc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  fontDesc.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  if (!allocator.createImage(fontDesc, &fontAtlas_)) return false;
  const std::vector<u8> fontPixels = expandToRgba(font.atlasPixels());
  if (!upload.uploadRgba8ToSampledImage(allocator, fontPixels.data(), fontPixels.size(),
                                        fontAtlas_))
    return false;

  ImageDesc iconDesc = fontDesc;
  iconDesc.width = icons.width();
  iconDesc.height = icons.height();
  // UI tokens and PNG pixels share encoded sRGB values. astra_ui.frag performs
  // the single conversion required by the presentation target. Sampling SRGB
  // here decoded icons twice, making their dark pixels almost black.
  iconDesc.format = VK_FORMAT_R8G8B8A8_UNORM;
  if (!allocator.createImage(iconDesc, &iconAtlas_)) return false;
  if (!upload.uploadRgba8ToSampledImage(allocator, icons.pixels().data(), icons.pixels().size(),
                                        iconAtlas_))
    return false;

  // R4: atlas de prévia começa transparente e mínimo; a sessão envia o de verdade
  // quando alguma prévia de textura é aberta.
  ImageDesc previewDesc = iconDesc;
  previewDesc.width = 4;
  previewDesc.height = 4;
  if (!allocator.createImage(previewDesc, &previewAtlas_)) return false;
  const std::vector<u8> emptyPreview(4 * 4 * 4, 0);
  if (!upload.uploadRgba8ToSampledImage(allocator, emptyPreview.data(), emptyPreview.size(), previewAtlas_))
    return false;
  previewAtlasSize_[0] = 4.0f;
  previewAtlasSize_[1] = 4.0f;

  SamplerDesc sampler{};
  // Preso à borda: repetir faria o último texel de um glifo amostrar o glifo do
  // outro lado do atlas quando a interpolação passa meio texel do retângulo.
  sampler.addressU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler.addressV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler.addressW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  sampler.maxLod = 0.0f;
  if (!sampler_.initialize(device_, sampler)) return false;

  fontAtlasSize_[0] = static_cast<float>(font.atlasWidth());
  fontAtlasSize_[1] = static_cast<float>(font.atlasHeight());
  iconAtlasSize_[0] = static_cast<float>(icons.width());
  iconAtlasSize_[1] = static_cast<float>(icons.height());
  return true;
}

bool VulkanUiRenderer::createPipeline(VkRenderPass renderPass, u32 subpass,
                                      VkPipelineCache pipelineCache) {
  VkShaderModule vert =
      createModule(device_, shaders::kAstra_UiVertSpirv, shaders::kAstra_UiVertSpirvSize);
  VkShaderModule frag =
      createModule(device_, shaders::kAstra_UiFragSpirv, shaders::kAstra_UiFragSpirvSize);
  if (vert == VK_NULL_HANDLE || frag == VK_NULL_HANDLE) {
    if (vert != VK_NULL_HANDLE) vkDestroyShaderModule(device_, vert, nullptr);
    if (frag != VK_NULL_HANDLE) vkDestroyShaderModule(device_, frag, nullptr);
    return false;
  }

  VkPipelineShaderStageCreateInfo stages[2]{};
  stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
               VK_SHADER_STAGE_VERTEX_BIT, vert, "main", nullptr};
  stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
               VK_SHADER_STAGE_FRAGMENT_BIT, frag, "main", nullptr};

  VkPipelineVertexInputStateCreateInfo vertex{};
  vertex.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  VkPipelineInputAssemblyStateCreateInfo assembly{};
  assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  // Faixa de triângulos: quatro vértices por quad, sem índices e sem buffer.
  assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
  VkPipelineViewportStateCreateInfo viewport{};
  viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport.viewportCount = 1;
  viewport.scissorCount = 1;
  VkPipelineRasterizationStateCreateInfo raster{};
  raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  raster.polygonMode = VK_POLYGON_MODE_FILL;
  raster.cullMode = VK_CULL_MODE_NONE;
  raster.lineWidth = 1.0f;
  VkPipelineMultisampleStateCreateInfo multisample{};
  multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  VkPipelineDepthStencilStateCreateInfo depth{};
  depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  // A interface é a última coisa do frame e vive por cima de tudo. Testar
  // profundidade contra a cena a faria sumir atrás de um objeto próximo.
  depth.depthTestEnable = VK_FALSE;
  depth.depthWriteEnable = VK_FALSE;

  VkPipelineColorBlendAttachmentState attachment{};
  attachment.blendEnable = VK_TRUE;
  attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  attachment.colorBlendOp = VK_BLEND_OP_ADD;
  attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  attachment.alphaBlendOp = VK_BLEND_OP_ADD;
  attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  VkPipelineColorBlendStateCreateInfo blend{};
  blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  blend.attachmentCount = 1;
  blend.pAttachments = &attachment;

  VkDynamicState states[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic{};
  dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic.dynamicStateCount = 2;
  dynamic.pDynamicStates = states;

  VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(PushConstants)};
  VkPipelineLayoutCreateInfo layout{};
  layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layout.setLayoutCount = 1;
  layout.pSetLayouts = &descriptorLayout_;
  layout.pushConstantRangeCount = 1;
  layout.pPushConstantRanges = &push;

  bool ok = vkCreatePipelineLayout(device_, &layout, nullptr, &pipelineLayout_) == VK_SUCCESS;
  if (ok) {
    VkGraphicsPipelineCreateInfo pipeline{};
    pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline.stageCount = 2;
    pipeline.pStages = stages;
    pipeline.pVertexInputState = &vertex;
    pipeline.pInputAssemblyState = &assembly;
    pipeline.pViewportState = &viewport;
    pipeline.pRasterizationState = &raster;
    pipeline.pMultisampleState = &multisample;
    pipeline.pDepthStencilState = &depth;
    pipeline.pColorBlendState = &blend;
    pipeline.pDynamicState = &dynamic;
    pipeline.layout = pipelineLayout_;
    pipeline.renderPass = renderPass;
    pipeline.subpass = subpass;
    ok = vkCreateGraphicsPipelines(device_, pipelineCache, 1, &pipeline, nullptr, &pipeline_) ==
         VK_SUCCESS;
  }
  vkDestroyShaderModule(device_, vert, nullptr);
  vkDestroyShaderModule(device_, frag, nullptr);
  return ok;
}

bool VulkanUiRenderer::initialize(VkDevice device, VulkanMemoryAllocator &allocator,
                                  VulkanUploadContext &upload, VkRenderPass renderPass,
                                  u32 subpass, VkPipelineCache pipelineCache,
                                  const ui::UiFont &font, const ui::UiIconAtlas &icons,
                                  u32 maximumInstances) {
  shutdown();
  if (device == VK_NULL_HANDLE || renderPass == VK_NULL_HANDLE) return false;
  if (!font.isReady() || !icons.isReady() || maximumInstances == 0) return false;
  device_ = device;
  allocator_ = &allocator;
  capacity_ = maximumInstances;

  BufferDesc instances{};
  instances.sizeBytes = static_cast<u64>(maximumInstances) * sizeof(ui::UiInstance);
  instances.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  instances.memoryClass = MemoryClass::Buffer;
  // A CPU reescreve a lista inteira todo frame e a GPU só lê. Um buffer
  // device-local exigiria staging por frame para o mesmo resultado.
  instances.cpuAccess = CpuAccess::SequentialWrite;
  instances.preferDeviceMemory = false;
  if (!allocator.createBuffer(instances, &instanceBuffer_) ||
      instanceBuffer_.mappedData() == nullptr) {
    shutdown();
    return false;
  }

  if (!createAtlas(allocator, upload, font, icons)) {
    shutdown();
    return false;
  }

  const VkDescriptorSetLayoutBinding bindings[5] = {
      {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
      {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
      {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
      {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
      {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
  };
  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = 5;
  layoutInfo.pBindings = bindings;
  if (vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &descriptorLayout_) !=
      VK_SUCCESS) {
    shutdown();
    return false;
  }

  const VkDescriptorPoolSize sizes[2] = {{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
                                         {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4}};
  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.maxSets = 1;
  poolInfo.poolSizeCount = 2;
  poolInfo.pPoolSizes = sizes;
  if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descriptorPool_) != VK_SUCCESS) {
    shutdown();
    return false;
  }
  VkDescriptorSetAllocateInfo allocate{};
  allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocate.descriptorPool = descriptorPool_;
  allocate.descriptorSetCount = 1;
  allocate.pSetLayouts = &descriptorLayout_;
  if (vkAllocateDescriptorSets(device_, &allocate, &descriptorSet_) != VK_SUCCESS) {
    shutdown();
    return false;
  }

  // O conjunto é escrito UMA vez: o buffer e as duas imagens não mudam de
  // identidade pela vida do renderer, só de conteúdo.
  VkDescriptorBufferInfo bufferInfo{instanceBuffer_.handle(), 0, VK_WHOLE_SIZE};
  VkDescriptorImageInfo fontInfo{sampler_.handle(), fontAtlas_.view(),
                                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  VkDescriptorImageInfo previewInfo{sampler_.handle(), previewAtlas_.view(),
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  VkDescriptorImageInfo iconInfo{sampler_.handle(), iconAtlas_.view(),
                                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  VkWriteDescriptorSet writes[5]{};
  for (u32 index = 0; index < 5; ++index) {
    writes[index].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[index].dstSet = descriptorSet_;
    writes[index].dstBinding = index;
    writes[index].descriptorCount = 1;
  }
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[0].pBufferInfo = &bufferInfo;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  writes[1].pImageInfo = &fontInfo;
  writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  writes[2].pImageInfo = &iconInfo;
  writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  writes[3].pImageInfo = &previewInfo;
  writes[4].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  writes[4].pImageInfo=&previewInfo;
  vkUpdateDescriptorSets(device_, 5, writes, 0, nullptr);

  if (!createPipeline(renderPass, subpass, pipelineCache)) {
    shutdown();
    return false;
  }
  return true;
}

bool VulkanUiRenderer::record(VkCommandBuffer commandBuffer,
                              std::span<const ui::UiInstance> instances, float surfaceWidth,
                              float surfaceHeight, const SurfaceTransform &surfaceTransform, bool srgbTarget) {
  if (!isReady() || commandBuffer == VK_NULL_HANDLE) return false;
  if (instances.empty()) return true;
  if (surfaceWidth <= 0.0f || surfaceHeight <= 0.0f) return false;

  const u32 count = static_cast<u32>(std::min<usize>(instances.size(), capacity_));
  std::memcpy(instanceBuffer_.mappedData(), instances.data(),
              static_cast<usize>(count) * sizeof(ui::UiInstance));
  if (allocator_ == nullptr || !allocator_->flushBuffer(instanceBuffer_)) return false;

  PushConstants push{};
  push.outputFlags[0]=srgbTarget?1.0f:0.0f;
  push.outputFlags[2]=previewAtlasSize_[0];
  push.outputFlags[3]=previewAtlasSize_[1];
  push.surface[0] = surfaceWidth;
  push.surface[1] = surfaceHeight;
  push.surface[2] = 1.0f / surfaceWidth;
  push.surface[3] = 1.0f / surfaceHeight;
  push.surfaceTransform[0] = surfaceTransform.xx;
  push.surfaceTransform[1] = surfaceTransform.xy;
  push.surfaceTransform[2] = surfaceTransform.yx;
  push.surfaceTransform[3] = surfaceTransform.yy;
  push.atlasSizes[0] = fontAtlasSize_[0];
  push.atlasSizes[1] = fontAtlasSize_[1];
  push.atlasSizes[2] = iconAtlasSize_[0];
  push.atlasSizes[3] = iconAtlasSize_[1];

  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1,
                          &descriptorSet_, 0, nullptr);
  vkCmdPushConstants(commandBuffer, pipelineLayout_,
                     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                     sizeof(push), &push);
  // Quatro vértices, uma instância por quad. É o frame inteiro da interface.
  vkCmdDraw(commandBuffer, 4, count, 0, 0);
  return true;
}

bool VulkanUiRenderer::setPreviewAtlas(VulkanUploadContext &upload, std::span<const u8> rgba, u32 width, u32 height) {
  if (!isReady() || allocator_ == nullptr || width == 0 || height == 0 ||
      rgba.size() != static_cast<usize>(width) * height * 4)
    return false;
  ImageDesc desc{};
  desc.width = width;
  desc.height = height;
  // Mesmo contrato do atlas de ícones: bytes sRGB codificados, uma conversão só
  // no fragmento (astra_ui.frag).
  desc.format = VK_FORMAT_R8G8B8A8_UNORM;
  desc.mipLevels = 1;
  desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  desc.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  VulkanImage next{};
  if (!allocator_->createImage(desc, &next) ||
      !upload.uploadRgba8ToSampledImage(*allocator_, rgba.data(), rgba.size(), next))
    return false;
  // Imagem nova em vez de reescrever a antiga: o contexto de upload só aceita
  // imagem ainda não usada por outro comando.
  VkDescriptorImageInfo info{sampler_.handle(), next.view(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = descriptorSet_;
  write.dstBinding = 3;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  write.pImageInfo = &info;
  vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
  previewAtlas_ = std::move(next);
  if(!cameraPreviewView_) setCameraPreview(VK_NULL_HANDLE);
  previewAtlasSize_[0] = static_cast<float>(width);
  previewAtlasSize_[1] = static_cast<float>(height);
  return true;
}

void VulkanUiRenderer::setCameraPreview(VkImageView view) {
  if(!device_ || !descriptorSet_) return;
  cameraPreviewView_=view;
  VkDescriptorImageInfo image{sampler_.handle(),view?view:previewAtlas_.view(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  VkWriteDescriptorSet write{};write.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet=descriptorSet_;write.dstBinding=4;write.descriptorCount=1;
  write.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;write.pImageInfo=&image;
  vkUpdateDescriptorSets(device_,1,&write,0,nullptr);
}

} // namespace ae::rhi
