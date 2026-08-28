#include "platform/android/instanced_renderer.h"

#include "rhi/shaders/instanced_spirv.h"
#include "rhi/shaders/instanced_fallback_spirv.h"
#include "rhi/shaders/scene_preview_spirv.h"
#include "rhi/shaders/material_preview_spirv.h"
#include "rhi/shaders/material_fallback_spirv.h"
#include "rhi/shaders/dirt_road_spirv.h"
#include "rhi/shaders/dirt_road_fallback_spirv.h"
#include "rhi/shaders/dirt_road_sky_spirv.h"
#include "renderer/sphere_mesh.h"

#include <android/log.h>
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstring>

namespace ae::platform::android {

namespace {
constexpr const char *LogTag = "Aether.Android";
constexpr u32 kTextureSize = 64;
// Shared diagnostic capacity: dummy plus checker, or five material maps.
// Unused slots are initialized by BindlessTextureRegistry before registration.
constexpr u32 kBindlessCapacity = 256;

struct FramePushConstants {
  float timeSeconds;
  float aspectRatio;
  float orbitYaw;
  float orbitPitch;
  float surfaceXX;
  float surfaceXY;
  float surfaceYX;
  float surfaceYY;
  u32 materialIndex;
  float exposure;
  float roughnessFactor;
  float metallicFactor;
  u32 encodeSrgb;
  float normalScale;
  float reserved[2];
};
static_assert(sizeof(FramePushConstants) == 64);
static_assert(offsetof(FramePushConstants, exposure) == 36);
static_assert(offsetof(FramePushConstants, encodeSrgb) == 48);

struct DirtRoadPushConstants {
  float cameraFrame[4];
  float surfaceTransform[4];
  float cameraPositionNear[4];
  float baseColorFactor[4];
  float emissiveFactorAndStrength[4];
  u32 textureIndices[4];
  u32 materialFlags[4];
  float materialFactors[4];
};
static_assert(sizeof(DirtRoadPushConstants) == 128);

bool formatSupportsDepthAttachment(VkPhysicalDevice physicalDevice, VkFormat format) {
  VkFormatProperties properties{};
  vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);
  return (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
}

VkFormat chooseDepthFormat(VkPhysicalDevice physicalDevice) {
  constexpr VkFormat candidates[] = {
      VK_FORMAT_D32_SFLOAT,
      VK_FORMAT_D24_UNORM_S8_UINT,
      VK_FORMAT_D16_UNORM,
  };
  for (VkFormat candidate : candidates) {
    if (formatSupportsDepthAttachment(physicalDevice, candidate)) return candidate;
  }
  return VK_FORMAT_UNDEFINED;
}

bool hasStencil(VkFormat format) {
  return format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D32_SFLOAT_S8_UINT;
}

VkShaderModule createShaderModule(VkDevice device, const uint32_t *code, uint32_t codeSize) {
  VkShaderModuleCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  info.codeSize = codeSize;
  info.pCode = code;
  VkShaderModule module = VK_NULL_HANDLE;
  if (vkCreateShaderModule(device, &info, nullptr, &module) != VK_SUCCESS) return VK_NULL_HANDLE;
  return module;
}

} // namespace

InstancedRenderer::~InstancedRenderer() {
  shutdown();
}

bool InstancedRenderer::createRenderPass() {
  VkAttachmentDescription colorAttachment{};
  colorAttachment.format = swapchain_->imageFormat();
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference colorRef{};
  colorRef.attachment = 0;
  colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentDescription depthAttachment{};
  depthAttachment.format = depthFormat_;
  depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthRef{};
  depthRef.attachment = 1;
  depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorRef;
  subpass.pDepthStencilAttachment = &depthRef;

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  const VkAttachmentDescription attachments[] = {colorAttachment, depthAttachment};

  VkRenderPassCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  info.attachmentCount = 2;
  info.pAttachments = attachments;
  info.subpassCount = 1;
  info.pSubpasses = &subpass;
  info.dependencyCount = 1;
  info.pDependencies = &dependency;

  return vkCreateRenderPass(device_, &info, nullptr, &renderPass_) == VK_SUCCESS;
}

bool InstancedRenderer::createPipeline() {
  VkShaderModule vertModule = dirtRoadPreview_
      ? createShaderModule(device_, rhi::shaders::kDirt_RoadVertSpirv, rhi::shaders::kDirt_RoadVertSpirvSize)
      : materialPreview_
      ? createShaderModule(device_, rhi::shaders::kMaterial_PreviewVertSpirv, rhi::shaders::kMaterial_PreviewVertSpirvSize)
      : scenePreview_
      ? createShaderModule(device_, rhi::shaders::kScene_PreviewVertSpirv, rhi::shaders::kScene_PreviewVertSpirvSize)
      : createShaderModule(device_, rhi::shaders::kInstancedVertSpirv, rhi::shaders::kInstancedVertSpirvSize);
  VkShaderModule fragModule = dirtRoadPreview_
      ? (useBindless_
          ? createShaderModule(device_, rhi::shaders::kDirt_RoadFragSpirv, rhi::shaders::kDirt_RoadFragSpirvSize)
          : createShaderModule(device_, rhi::shaders::kDirt_Road_FallbackFragSpirv,
                               rhi::shaders::kDirt_Road_FallbackFragSpirvSize))
      : materialPreview_
      ? (useBindless_
          ? createShaderModule(device_, rhi::shaders::kMaterial_PreviewFragSpirv, rhi::shaders::kMaterial_PreviewFragSpirvSize)
          : createShaderModule(device_, rhi::shaders::kMaterial_FallbackFragSpirv, rhi::shaders::kMaterial_FallbackFragSpirvSize))
      : useBindless_
      ? createShaderModule(device_, rhi::shaders::kInstancedFragSpirv, rhi::shaders::kInstancedFragSpirvSize)
      : createShaderModule(device_, rhi::shaders::kInstanced_FallbackFragSpirv,
                           rhi::shaders::kInstanced_FallbackFragSpirvSize);
  if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE) {
    if (vertModule != VK_NULL_HANDLE) vkDestroyShaderModule(device_, vertModule, nullptr);
    if (fragModule != VK_NULL_HANDLE) vkDestroyShaderModule(device_, fragModule, nullptr);
    return false;
  }

  VkPipelineShaderStageCreateInfo stages[2]{};
  stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = vertModule;
  stages[0].pName = "main";
  stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = fragModule;
  stages[1].pName = "main";

  // Um binding por instância (VK_VERTEX_INPUT_RATE_INSTANCE) — o cubo em si
  // não tem vertex buffer (gerado por gl_VertexIndex igual ao TriangleRenderer,
  // ver instanced.vert), só a posição/cor avança por instância, não por
  // vértice.
  VkVertexInputBindingDescription binding{};
  binding.binding = 1;
  binding.stride = instanceStride();
  binding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

  VkVertexInputAttributeDescription attributes[11]{};
  attributes[0].location = 0;
  attributes[0].binding = 1;
  attributes[0].format = VK_FORMAT_R32G32_SFLOAT;
  attributes[0].offset = 0;
  attributes[1].location = 1;
  attributes[1].binding = 1;
  attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
  attributes[1].offset = 2 * sizeof(float);
  if (scenePreview_) {
    for (u32 i = 0; i < 5; ++i) {
      attributes[i] = {i, 1, VK_FORMAT_R32G32B32A32_SFLOAT, i * 16u};
    }
  }
  VkVertexInputBindingDescription bindings[2]={binding,{0,static_cast<u32>(materialPreview_?sizeof(renderer::MeshVertex):renderer::MapVertexStride),VK_VERTEX_INPUT_RATE_VERTEX}};
  if (materialPreview_) {
    attributes[5]={5,0,VK_FORMAT_R32G32B32_SFLOAT,0};
    attributes[6]={6,0,VK_FORMAT_R32G32B32_SFLOAT,12};
    attributes[7]={7,0,VK_FORMAT_R32G32B32A32_SFLOAT,24};
    attributes[8]={8,0,VK_FORMAT_R32G32_SFLOAT,40};
  }
  if (dirtRoadPreview_) {
    attributes[0]={0,0,VK_FORMAT_R32G32B32_SFLOAT,0};
    attributes[1]={1,0,VK_FORMAT_R32G32B32_SFLOAT,12};
    attributes[2]={2,0,VK_FORMAT_R32G32B32A32_SFLOAT,24};
    attributes[3]={3,0,VK_FORMAT_R32G32_SFLOAT,40};
    attributes[4]={4,0,VK_FORMAT_R32G32_SFLOAT,48};
    attributes[5]={5,0,VK_FORMAT_R32G32B32A32_SFLOAT,56};
    for(u32 i=0;i<5;++i)attributes[6+i]={6+i,1,VK_FORMAT_R32G32B32A32_SFLOAT,i*16u};
  }

  VkPipelineVertexInputStateCreateInfo vertexInput{};
  vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInput.vertexBindingDescriptionCount = (materialPreview_ || dirtRoadPreview_) ? 2 : 1;
  vertexInput.pVertexBindingDescriptions = bindings;
  vertexInput.vertexAttributeDescriptionCount = dirtRoadPreview_ ? 11 : (materialPreview_ ? 9 : (scenePreview_ ? 5 : 2));
  vertexInput.pVertexAttributeDescriptions = attributes;

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicState{};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = 2;
  dynamicState.pDynamicStates = dynamicStates;

  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.cullMode = VK_CULL_MODE_NONE;
  rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rasterizer.lineWidth = 1.0f;

  VkPipelineMultisampleStateCreateInfo multisample{};
  multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineDepthStencilStateCreateInfo depthStencil{};
  depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.depthTestEnable = VK_TRUE;
  depthStencil.depthWriteEnable = VK_TRUE;
  depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
  depthStencil.minDepthBounds = 0.0f;
  depthStencil.maxDepthBounds = 1.0f;

  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo colorBlend{};
  colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlend.attachmentCount = 1;
  colorBlend.pAttachments = &colorBlendAttachment;

  const VkDescriptorSetLayout setLayouts[2] = {textureSetLayout_, environmentSetLayout_};
  VkPipelineLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layoutInfo.setLayoutCount = dirtRoadPreview_ ? 2u : 1u;
  layoutInfo.pSetLayouts = setLayouts;
  VkPushConstantRange pushRange{};
  // vertex lê timeSeconds/aspectRatio/órbita/surfaceTransform; fragment lê
  // materialIndex para indexar o array bindless (ver instanced.frag) — as
  // duas stages compartilham o mesmo range porque é um único struct.
  pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  pushRange.size = dirtRoadPreview_ ? sizeof(DirtRoadPushConstants) : sizeof(FramePushConstants);
  layoutInfo.pushConstantRangeCount = 1;
  layoutInfo.pPushConstantRanges = &pushRange;
  const bool layoutOk = vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &pipelineLayout_) == VK_SUCCESS;

  bool pipelineOk = false;
  if (layoutOk) {
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout_;
    pipelineInfo.renderPass = renderPass_;
    pipelineInfo.subpass = 0;

    pipelineOk = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                           &pipeline_) == VK_SUCCESS;
    if (pipelineOk && dirtRoadPreview_) {
      depthStencil.depthWriteEnable = VK_FALSE;
      colorBlendAttachment.blendEnable = VK_TRUE;
      colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
      colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
      colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
      colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
      pipelineOk = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                             &transparentPipeline_) == VK_SUCCESS;
    }
  }

  vkDestroyShaderModule(device_, vertModule, nullptr);
  vkDestroyShaderModule(device_, fragModule, nullptr);
  return layoutOk && pipelineOk;
}

bool InstancedRenderer::createEnvironmentDescriptors() {
  if (!dirtRoadPreview_) return true;
  rhi::BufferDesc buffer{};
  buffer.sizeBytes = sizeof(EnvironmentLighting);
  buffer.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  buffer.cpuAccess = rhi::CpuAccess::SequentialWrite;
  buffer.preferDeviceMemory = false;
  if (!memoryAllocator_->createBuffer(buffer, &environmentUniform_)) return false;
  std::memcpy(environmentUniform_.mappedData(), &dirtRoadResources_.environmentLighting(), sizeof(EnvironmentLighting));
  if (!memoryAllocator_->flushBuffer(environmentUniform_)) return false;

  VkDescriptorSetLayoutBinding bindings[2]{};
  bindings[0].binding = 0;
  bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  bindings[0].descriptorCount = 1;
  bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  bindings[1].binding = 1;
  bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  bindings[1].descriptorCount = 1;
  bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  VkDescriptorSetLayoutCreateInfo layout{};
  layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout.bindingCount = 2;
  layout.pBindings = bindings;
  if (vkCreateDescriptorSetLayout(device_, &layout, nullptr, &environmentSetLayout_) != VK_SUCCESS) return false;
  VkDescriptorPoolSize sizes[2] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
                                   {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}};
  VkDescriptorPoolCreateInfo pool{};
  pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool.maxSets = 1;
  pool.poolSizeCount = 2;
  pool.pPoolSizes = sizes;
  if (vkCreateDescriptorPool(device_, &pool, nullptr, &environmentPool_) != VK_SUCCESS) return false;
  VkDescriptorSetAllocateInfo allocation{};
  allocation.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocation.descriptorPool = environmentPool_;
  allocation.descriptorSetCount = 1;
  allocation.pSetLayouts = &environmentSetLayout_;
  if (vkAllocateDescriptorSets(device_, &allocation, &environmentSet_) != VK_SUCCESS) return false;
  VkDescriptorBufferInfo uniform{environmentUniform_.handle(), 0, sizeof(EnvironmentLighting)};
  VkDescriptorImageInfo image{dirtRoadResources_.environmentSampler(), dirtRoadResources_.environmentView(),
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  VkWriteDescriptorSet writes[2]{};
  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].dstSet = environmentSet_;
  writes[0].dstBinding = 0;
  writes[0].descriptorCount = 1;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[0].pBufferInfo = &uniform;
  writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[1].dstSet = environmentSet_;
  writes[1].dstBinding = 1;
  writes[1].descriptorCount = 1;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  writes[1].pImageInfo = &image;
  vkUpdateDescriptorSets(device_, 2, writes, 0, nullptr);
  return true;
}

bool InstancedRenderer::createSkyPipeline() {
  if (!dirtRoadPreview_) return true;
  VkShaderModule vert = createShaderModule(device_, rhi::shaders::kDirt_Road_SkyVertSpirv,
                                           rhi::shaders::kDirt_Road_SkyVertSpirvSize);
  VkShaderModule frag = createShaderModule(device_, rhi::shaders::kDirt_Road_SkyFragSpirv,
                                           rhi::shaders::kDirt_Road_SkyFragSpirvSize);
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
  assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  VkPipelineViewportStateCreateInfo viewport{};
  viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport.viewportCount = 1; viewport.scissorCount = 1;
  VkPipelineRasterizationStateCreateInfo raster{};
  raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  raster.polygonMode = VK_POLYGON_MODE_FILL; raster.cullMode = VK_CULL_MODE_NONE; raster.lineWidth = 1;
  VkPipelineMultisampleStateCreateInfo multisample{};
  multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  VkPipelineDepthStencilStateCreateInfo depth{};
  depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth.depthTestEnable = VK_FALSE; depth.depthWriteEnable = VK_FALSE;
  VkPipelineColorBlendAttachmentState attachment{};
  attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  VkPipelineColorBlendStateCreateInfo blend{};
  blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  blend.attachmentCount = 1; blend.pAttachments = &attachment;
  VkDynamicState states[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic{};
  dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic.dynamicStateCount = 2; dynamic.pDynamicStates = states;
  VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(DirtRoadPushConstants)};
  VkPipelineLayoutCreateInfo layout{};
  layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layout.setLayoutCount = 1; layout.pSetLayouts = &environmentSetLayout_;
  layout.pushConstantRangeCount = 1; layout.pPushConstantRanges = &push;
  bool ok = vkCreatePipelineLayout(device_, &layout, nullptr, &skyPipelineLayout_) == VK_SUCCESS;
  if (ok) {
    VkGraphicsPipelineCreateInfo pipeline{};
    pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline.stageCount = 2; pipeline.pStages = stages; pipeline.pVertexInputState = &vertex;
    pipeline.pInputAssemblyState = &assembly; pipeline.pViewportState = &viewport;
    pipeline.pRasterizationState = &raster; pipeline.pMultisampleState = &multisample;
    pipeline.pDepthStencilState = &depth; pipeline.pColorBlendState = &blend;
    pipeline.pDynamicState = &dynamic; pipeline.layout = skyPipelineLayout_;
    pipeline.renderPass = renderPass_; pipeline.subpass = 0;
    ok = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline, nullptr, &skyPipeline_) == VK_SUCCESS;
  }
  vkDestroyShaderModule(device_, vert, nullptr);
  vkDestroyShaderModule(device_, frag, nullptr);
  return ok;
}

bool InstancedRenderer::createFramebuffers() {
  framebufferCount_ = swapchain_->imageCount();
  for (u32 i = 0; i < framebufferCount_; ++i) {
    const VkImageView attachments[] = {swapchain_->imageView(i), depthImage_.view()};
    VkFramebufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.renderPass = renderPass_;
    info.attachmentCount = 2;
    info.pAttachments = attachments;
    info.width = swapchain_->width();
    info.height = swapchain_->height();
    info.layers = 1;
    if (vkCreateFramebuffer(device_, &info, nullptr, &framebuffers_[i]) != VK_SUCCESS) {
      return false;
    }
  }
  return true;
}

bool InstancedRenderer::createCommandResources() {
  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = graphicsQueueFamily_;
  if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_) != VK_SUCCESS) return false;

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = commandPool_;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;
  if (vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer_) != VK_SUCCESS) return false;
  return uploadContext_.initialize(device_, graphicsQueueFamily_);
}

void InstancedRenderer::destroyFramebuffers() {
  for (u32 i = 0; i < framebufferCount_; ++i) {
    if (framebuffers_[i] != VK_NULL_HANDLE) {
      vkDestroyFramebuffer(device_, framebuffers_[i], nullptr);
      framebuffers_[i] = VK_NULL_HANDLE;
    }
  }
  framebufferCount_ = 0;
}

bool InstancedRenderer::createInstanceBuffer() {
  rhi::BufferDesc desc{};
  desc.sizeBytes = static_cast<u64>(instanceCount_) * instanceStride();
  desc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  desc.memoryClass = rhi::MemoryClass::Buffer;
  desc.cpuAccess = rhi::CpuAccess::SequentialWrite;
  desc.preferDeviceMemory = false;
  if (memoryAllocator_ == nullptr || !memoryAllocator_->createBuffer(desc, &instanceBuffer_) ||
      instanceBuffer_.mappedData() == nullptr) return false;
  if (dirtRoadPreview_) {
    auto *instances = static_cast<renderer::RenderInstance *>(instanceBuffer_.mappedData());
    const auto &draws = dirtRoadResources_.draws();
    if (draws.size() != instanceCount_) return false;
    for (u32 index = 0; index < instanceCount_; ++index) {
      std::memcpy(instances[index].model, draws[index].model, sizeof(instances[index].model));
      instances[index].tint[0] = instances[index].tint[1] = instances[index].tint[2] =
          instances[index].tint[3] = 1.0f;
      instances[index].entityIndex = index;
      instances[index].entityGeneration = 1;
    }
    return memoryAllocator_->flushBuffer(instanceBuffer_);
  }
  return true;
}

bool InstancedRenderer::createDepthImage() {
  depthFormat_ = chooseDepthFormat(physicalDevice_);
  if (depthFormat_ == VK_FORMAT_UNDEFINED) return false;
  rhi::ImageDesc desc{};
  desc.width = swapchain_->width();
  desc.height = swapchain_->height();
  desc.format = depthFormat_;
  desc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  desc.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  if (hasStencil(depthFormat_)) desc.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
  desc.memoryClass = rhi::MemoryClass::RenderTarget;
  return memoryAllocator_->createImage(desc, &depthImage_);
}

bool InstancedRenderer::createTextureResources() {
  // Dummy 1x1 branca: exigida por BindlessTextureRegistry::initialize para
  // preencher todo slot do array antes de qualquer registro real (ver
  // comentário no header) — nunca é de fato amostrada em um caminho correto,
  // já que baseTextureIndex_ é sempre válido antes do primeiro drawFrame.
  const std::array<u8, 4> dummyPixels{255, 255, 255, 255};
  rhi::ImageDesc dummyDesc{};
  dummyDesc.width = 1;
  dummyDesc.height = 1;
  dummyDesc.format = VK_FORMAT_R8G8B8A8_SRGB;
  dummyDesc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  dummyDesc.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  dummyDesc.memoryClass = rhi::MemoryClass::Texture;
  if (!memoryAllocator_->createImage(dummyDesc, &dummyTexture_)) return false;
  if (!uploadContext_.uploadRgba8ToSampledImage(*memoryAllocator_, dummyPixels.data(),
                                                sizeof(dummyPixels), dummyTexture_)) {
    return false;
  }
  rhi::SamplerDesc dummySamplerDesc{};
  dummySamplerDesc.minFilter = VK_FILTER_NEAREST;
  dummySamplerDesc.magFilter = VK_FILTER_NEAREST;
  dummySamplerDesc.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  if (!dummySampler_.initialize(device_, dummySamplerDesc)) return false;

  std::array<u8, kTextureSize * kTextureSize * 4> pixels{};
  for (u32 y = 0; y < kTextureSize; ++y) {
    for (u32 x = 0; x < kTextureSize; ++x) {
      const bool light = ((x / 8u) + (y / 8u)) % 2u == 0;
      const usize offset = static_cast<usize>(y * kTextureSize + x) * 4;
      pixels[offset + 0] = light ? 220 : 24;
      pixels[offset + 1] = light ? 245 : 76;
      pixels[offset + 2] = light ? 245 : 86;
      pixels[offset + 3] = 255;
    }
  }

  rhi::ImageDesc imageDesc{};
  imageDesc.width = kTextureSize;
  imageDesc.height = kTextureSize;
  imageDesc.format = VK_FORMAT_R8G8B8A8_SRGB;
  imageDesc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  imageDesc.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  imageDesc.memoryClass = rhi::MemoryClass::Texture;
  if (!memoryAllocator_->createImage(imageDesc, &baseTexture_)) return false;
  if (!uploadContext_.uploadRgba8ToSampledImage(*memoryAllocator_, pixels.data(), sizeof(pixels),
                                                baseTexture_)) {
    return false;
  }

  rhi::SamplerDesc samplerDesc{};
  samplerDesc.minFilter = VK_FILTER_NEAREST;
  samplerDesc.magFilter = VK_FILTER_NEAREST;
  samplerDesc.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  return baseSampler_.initialize(device_, samplerDesc);
}

bool InstancedRenderer::createBindlessRegistry() {
  if (!bindlessRegistry_.initialize(device_, std::min(kBindlessCapacity, rhiDevice_->bindlessTextureCapacity()), dummyTexture_.view(),
                                    dummySampler_.handle())) {
    return false;
  }
  if (materialPreview_) {
    for(u32 i=0;i<MaterialPreviewResources::TextureCount;++i) {
      const u32 slot=bindlessRegistry_.registerTexture(materialResources_.view(i),materialResources_.sampler(i),
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
      if(slot==rhi::kBindlessIndexInvalid || (i>0 && slot!=baseTextureIndex_+i))return false;
      if(i==0)baseTextureIndex_=slot;
    }
    return true;
  }
  if (dirtRoadPreview_) {
    baseTextureIndex_ = bindlessRegistry_.registerTexture(
        baseTexture_.view(), baseSampler_.handle(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (baseTextureIndex_ == rhi::kBindlessIndexInvalid) return false;
    dirtTextureSlots_.resize(dirtRoadResources_.textureCount());
    for (u32 index = 0; index < dirtRoadResources_.textureCount(); ++index) {
      dirtTextureSlots_[index] = bindlessRegistry_.registerTexture(
          dirtRoadResources_.view(index), dirtRoadResources_.sampler(index),
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
      if (dirtTextureSlots_[index] == rhi::kBindlessIndexInvalid) return false;
    }
    return true;
  }
  baseTextureIndex_ = bindlessRegistry_.registerTexture(
      baseTexture_.view(), baseSampler_.handle(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  return baseTextureIndex_ != rhi::kBindlessIndexInvalid;
}

bool InstancedRenderer::createTextureDescriptors() {
  if (useBindless_) {
    if (!createBindlessRegistry()) return false;
    textureSetLayout_ = bindlessRegistry_.layout();
    textureSet_ = bindlessRegistry_.descriptorSet();
    return true;
  }
  if (dirtRoadPreview_) {
    VkDescriptorSetLayoutBinding bindings[4]{};
    for (u32 index = 0; index < 4; ++index) {
      bindings[index].binding = index;
      bindings[index].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      bindings[index].descriptorCount = 1;
      bindings[index].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    VkDescriptorSetLayoutCreateInfo layout{};
    layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout.bindingCount = 4;
    layout.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(device_, &layout, nullptr, &textureSetLayout_) != VK_SUCCESS) return false;
    const u32 materialCount = static_cast<u32>(dirtRoadResources_.materials().size());
    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, materialCount * 4};
    VkDescriptorPoolCreateInfo pool{};
    pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool.maxSets = materialCount;
    pool.poolSizeCount = 1;
    pool.pPoolSizes = &poolSize;
    if (vkCreateDescriptorPool(device_, &pool, nullptr, &texturePool_) != VK_SUCCESS) return false;
    dirtMaterialSets_.resize(materialCount);
    std::vector<VkDescriptorSetLayout> layouts(materialCount, textureSetLayout_);
    VkDescriptorSetAllocateInfo allocation{};
    allocation.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocation.descriptorPool = texturePool_;
    allocation.descriptorSetCount = materialCount;
    allocation.pSetLayouts = layouts.data();
    if (vkAllocateDescriptorSets(device_, &allocation, dirtMaterialSets_.data()) != VK_SUCCESS) return false;
    for (u32 materialIndex = 0; materialIndex < materialCount; ++materialIndex) {
      VkDescriptorImageInfo images[4]{};
      VkWriteDescriptorSet writes[4]{};
      const auto &material = dirtRoadResources_.materials()[materialIndex];
      for (u32 slot = 0; slot < 4; ++slot) {
        const u32 texture = material.textureIndices[slot];
        const bool valid = texture != renderer::InvalidMapTexture;
        images[slot] = {valid ? dirtRoadResources_.sampler(texture) : baseSampler_.handle(),
                        valid ? dirtRoadResources_.view(texture) : baseTexture_.view(),
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        writes[slot].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[slot].dstSet = dirtMaterialSets_[materialIndex];
        writes[slot].dstBinding = slot;
        writes[slot].descriptorCount = 1;
        writes[slot].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[slot].pImageInfo = &images[slot];
      }
      vkUpdateDescriptorSets(device_, 4, writes, 0, nullptr);
    }
    textureSet_ = dirtMaterialSets_.front();
    baseTextureIndex_ = 0;
    return true;
  }
  VkDescriptorSetLayoutBinding binding{};
  binding.binding = 0;
  binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  binding.descriptorCount = materialPreview_ ? MaterialPreviewResources::TextureCount : 1;
  binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  VkDescriptorSetLayoutCreateInfo layout{};
  layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout.bindingCount = 1;
  layout.pBindings = &binding;
  if (vkCreateDescriptorSetLayout(device_, &layout, nullptr, &textureSetLayout_) != VK_SUCCESS) return false;
  VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, binding.descriptorCount};
  VkDescriptorPoolCreateInfo pool{};
  pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool.maxSets = 1;
  pool.poolSizeCount = 1;
  pool.pPoolSizes = &size;
  if (vkCreateDescriptorPool(device_, &pool, nullptr, &texturePool_) != VK_SUCCESS) return false;
  VkDescriptorSetAllocateInfo allocation{};
  allocation.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocation.descriptorPool = texturePool_;
  allocation.descriptorSetCount = 1;
  allocation.pSetLayouts = &textureSetLayout_;
  if (vkAllocateDescriptorSets(device_, &allocation, &textureSet_) != VK_SUCCESS) return false;
  VkDescriptorImageInfo images[MaterialPreviewResources::TextureCount]{};
  for(u32 i=0;i<binding.descriptorCount;++i)
    images[i]={materialPreview_?materialResources_.sampler(i):baseSampler_.handle(),
               materialPreview_?materialResources_.view(i):baseTexture_.view(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = textureSet_;
  write.descriptorCount = binding.descriptorCount;
  write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  write.pImageInfo = images;
  vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
  baseTextureIndex_ = 0;
  return true;
}

bool InstancedRenderer::initialize(rhi::VulkanDevice &device, rhi::VulkanSwapchain &swapchain,
                                   DotNetHost &dotNetHost, u32 instanceCount, bool scenePreview,
                                   AAssetManager *materialAssets, bool forceTextureFallback,
                                   const std::atomic<bool> *cancel, bool dirtRoadPreview) {
  if (instanceCount == 0 || (!dotNetHost.isReady() && !dirtRoadPreview)) return false;

  device_ = device.handle();
  physicalDevice_ = device.physicalDevice();
  rhiDevice_ = &device;
  useBindless_ = device.enabledPaths().bindless;
  memoryAllocator_ = &device.memoryAllocator();
  swapchain_ = &swapchain;
  graphicsQueueFamily_ = device.graphicsQueueFamily();
  instanceCount_ = instanceCount;
  dirtRoadPreview_ = dirtRoadPreview;
  materialPreview_ = materialAssets != nullptr && !dirtRoadPreview_;
  scenePreview_ = !dirtRoadPreview_ && (scenePreview || materialPreview_);
  drawnInstanceCount_ = 0;
  lastExtractionStatus_ = 0;
  vkGetDeviceQueue(device_, graphicsQueueFamily_, 0, &graphicsQueue_);

  if (scenePreview_) {
    using InitializeSceneFn = int (*)();
    auto initializeScene = reinterpret_cast<InitializeSceneFn>(dotNetHost.getManagedFunctionPointer(
        "Aether.Rendering.Interop.SceneEntryPoints, Aether.Rendering", materialPreview_ ? "InitializeMaterialPreview" : "Initialize"));
    extractScene_ = reinterpret_cast<ExtractSceneFn>(dotNetHost.getManagedFunctionPointer(
        "Aether.Rendering.Interop.SceneEntryPoints, Aether.Rendering", "Extract"));
    if (initializeScene == nullptr || extractScene_ == nullptr || initializeScene() != 0) {
      __android_log_print(ANDROID_LOG_ERROR, LogTag, "[ScenePreview] Falha ao inicializar cena gerenciada.");
      return false;
    }
    if (materialPreview_) {
      using ReadMaterialFn=int (*)(MaterialParameters*,int);
      auto readMaterial=reinterpret_cast<ReadMaterialFn>(dotNetHost.getManagedFunctionPointer(
          "Aether.Rendering.Interop.SceneEntryPoints, Aether.Rendering","GetMaterialParameters"));
      if(readMaterial==nullptr || readMaterial(&materialParameters_,sizeof(MaterialParameters))!=0)return false;
    }
  } else if (!dirtRoadPreview_) {
    fillInstanceBuffer_ = reinterpret_cast<FillInstanceBufferFn>(dotNetHost.getManagedFunctionPointer(
        "Aether.Interop.NativeEntryPoints, Aether.Core", "FillInstanceBuffer"));
    if (fillInstanceBuffer_ == nullptr) {
      __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao resolver FillInstanceBuffer.");
      return false;
    }
  }

  if (!createCommandResources()) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar recursos de comando/upload.");
    return false;
  }
  if (dirtRoadPreview_) {
    if (!dirtRoadResources_.initialize(device, uploadContext_, materialAssets,
                                       forceTextureFallback, cancel)) return false;
    instanceCount_ = dirtRoadResources_.header().drawCount;
    for (u32 index = 0; index < instanceCount_; ++index) {
      const u32 material = dirtRoadResources_.draws()[index].materialIndex;
      if ((dirtRoadResources_.materials()[material].flags & 1u) != 0) transparentDrawOrder_.push_back(index);
    }
  }
  if (!createInstanceBuffer()) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar o buffer de instâncias.");
    return false;
  }
  if (!createTextureResources()) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar/upload da textura do cubo.");
    return false;
  }
  if (materialPreview_ && !materialResources_.initialize(device,uploadContext_,materialAssets,forceTextureFallback,cancel))return false;
  if (cancel && cancel->load()) return false;
  if (!createDepthImage()) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar o depth buffer.");
    return false;
  }
  if (!createTextureDescriptors()) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar os descritores de textura.");
    return false;
  }
  if (!createEnvironmentDescriptors()) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar descritores do ambiente HDRI.");
    return false;
  }
  if (!createRenderPass()) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar o render pass instanciado.");
    return false;
  }
  if (!createPipeline()) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar o pipeline instanciado.");
    return false;
  }
  if (!createSkyPipeline()) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar o pipeline do céu HDRI.");
    return false;
  }
  if (!createFramebuffers()) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar os framebuffers instanciados.");
    return false;
  }
  // Item 2.1.6 do plano: nomeia os objetos-chave deste renderer para que
  // captures de frame (RenderDoc/Android GPU Inspector) e mensagens de
  // validação mostrem "InstancedRenderer/pipeline" em vez de um handle
  // opaco. No-op em build release (setObjectName checa debugUtilsEnabled()
  // internamente via VulkanDevice).
  rhiDevice_->setObjectName(VK_OBJECT_TYPE_PIPELINE, reinterpret_cast<u64>(pipeline_),
                            "InstancedRenderer/pipeline");
  rhiDevice_->setObjectName(VK_OBJECT_TYPE_PIPELINE_LAYOUT, reinterpret_cast<u64>(pipelineLayout_),
                            "InstancedRenderer/pipelineLayout");
  rhiDevice_->setObjectName(VK_OBJECT_TYPE_DESCRIPTOR_SET,
                            reinterpret_cast<u64>(textureSet_),
                            "InstancedRenderer/textureSet");
  rhiDevice_->setObjectName(VK_OBJECT_TYPE_BUFFER, reinterpret_cast<u64>(instanceBuffer_.handle()),
                            "InstancedRenderer/instanceBuffer");
  rhiDevice_->setObjectName(VK_OBJECT_TYPE_IMAGE, reinterpret_cast<u64>(baseTexture_.handle()),
                            "InstancedRenderer/baseTexture");
  rhiDevice_->setObjectName(VK_OBJECT_TYPE_COMMAND_BUFFER, reinterpret_cast<u64>(commandBuffer_),
                            "InstancedRenderer/commandBuffer");

  const rhi::MemoryBudgetEntry bufferBudget =
      memoryAllocator_->budgetSnapshot().entries[static_cast<usize>(rhi::MemoryClass::Buffer)];
  __android_log_print(ANDROID_LOG_INFO, LogTag, "Descritores: caminho=%s, capacidade bindless=%u.",
                      useBindless_ ? "bindless" : "fallback", device.bindlessTextureCapacity());
  __android_log_print(ANDROID_LOG_INFO, LogTag,
                      "RHI/VMA: buffer de instâncias=%llu bytes; uso=%llu, limite=%llu bytes.",
                      static_cast<unsigned long long>(instanceBuffer_.sizeBytes()),
                      static_cast<unsigned long long>(bufferBudget.usedBytes),
                      static_cast<unsigned long long>(bufferBudget.limitBytes));

  __android_log_print(ANDROID_LOG_INFO, LogTag,
                      "InstancedRenderer pronto: cubo texturizado + depth, capacidade=%u, modo=%s.",
                      instanceCount_, dirtRoadPreview_ ? "dirt-road" :
                      (materialPreview_ ? "material-preview" : (scenePreview_ ? "scene-preview" : "PoC-A")));
  const rhi::MemoryBudgetSnapshot snapshot = memoryAllocator_->budgetSnapshot();
  for (usize i = 0; i < rhi::MemoryClassCount; ++i) {
    __android_log_print(ANDROID_LOG_INFO, LogTag, "RHI/VMA [%s]: uso=%llu, pico=%llu bytes.",
                        rhi::memoryClassName(static_cast<rhi::MemoryClass>(i)),
                        static_cast<unsigned long long>(snapshot.entries[i].usedBytes),
                        static_cast<unsigned long long>(snapshot.entries[i].peakBytes));
  }
  return true;
}

void InstancedRenderer::shutdown() {
  if (device_ == VK_NULL_HANDLE) return;
  vkDeviceWaitIdle(device_);

  if (commandPool_ != VK_NULL_HANDLE) {
    vkDestroyCommandPool(device_, commandPool_, nullptr);
    commandPool_ = VK_NULL_HANDLE;
    commandBuffer_ = VK_NULL_HANDLE;
  }
  destroyFramebuffers();
  if (pipeline_ != VK_NULL_HANDLE) {
    vkDestroyPipeline(device_, pipeline_, nullptr);
    pipeline_ = VK_NULL_HANDLE;
  }
  if (skyPipeline_ != VK_NULL_HANDLE) {
    vkDestroyPipeline(device_, skyPipeline_, nullptr);
    skyPipeline_ = VK_NULL_HANDLE;
  }
  if (transparentPipeline_ != VK_NULL_HANDLE) {
    vkDestroyPipeline(device_, transparentPipeline_, nullptr);
    transparentPipeline_ = VK_NULL_HANDLE;
  }
  if (pipelineLayout_ != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
    pipelineLayout_ = VK_NULL_HANDLE;
  }
  if (skyPipelineLayout_ != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device_, skyPipelineLayout_, nullptr);
    skyPipelineLayout_ = VK_NULL_HANDLE;
  }
  if (renderPass_ != VK_NULL_HANDLE) {
    vkDestroyRenderPass(device_, renderPass_, nullptr);
    renderPass_ = VK_NULL_HANDLE;
  }
  bindlessRegistry_.shutdown();
  if (environmentPool_ != VK_NULL_HANDLE) vkDestroyDescriptorPool(device_, environmentPool_, nullptr);
  if (environmentSetLayout_ != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device_, environmentSetLayout_, nullptr);
  environmentPool_ = VK_NULL_HANDLE;
  environmentSetLayout_ = VK_NULL_HANDLE;
  environmentSet_ = VK_NULL_HANDLE;
  environmentUniform_.reset();
  if (texturePool_ != VK_NULL_HANDLE) vkDestroyDescriptorPool(device_, texturePool_, nullptr);
  if (!useBindless_ && textureSetLayout_ != VK_NULL_HANDLE)
    vkDestroyDescriptorSetLayout(device_, textureSetLayout_, nullptr);
  texturePool_ = VK_NULL_HANDLE;
  textureSetLayout_ = VK_NULL_HANDLE;
  textureSet_ = VK_NULL_HANDLE;
  useBindless_ = false;
  baseTextureIndex_ = rhi::kBindlessIndexInvalid;
  materialResources_.shutdown();
  dirtRoadResources_.shutdown();
  dirtTextureSlots_.clear();
  dirtMaterialSets_.clear();
  transparentDrawOrder_.clear();
  baseSampler_.shutdown();
  baseTexture_.reset();
  dummySampler_.shutdown();
  dummyTexture_.reset();
  depthImage_.reset();
  instanceBuffer_.reset();
  uploadContext_.shutdown();
  device_ = VK_NULL_HANDLE;
  physicalDevice_ = VK_NULL_HANDLE;
  rhiDevice_ = nullptr;
  memoryAllocator_ = nullptr;
  swapchain_ = nullptr;
  fillInstanceBuffer_ = nullptr;
  extractScene_ = nullptr;
  dirtRoadPreview_ = false;
  materialPreview_ = false;
  scenePreview_ = false;
}

u64 InstancedRenderer::snapshotFingerprint() const {
  if (!scenePreview_ || instanceBuffer_.mappedData() == nullptr) return 0;
  const auto *bytes = static_cast<const u8 *>(instanceBuffer_.mappedData());
  u64 hash = 14695981039346656037ull;
  for (usize i = 0; i < static_cast<usize>(drawnInstanceCount_) * instanceStride(); ++i) {
    hash ^= bytes[i];
    hash *= 1099511628211ull;
  }
  return hash;
}

rhi::SwapchainStatus InstancedRenderer::drawFrame(float timeSeconds,
                                                  const platform::FreeCameraState &camera) {
  using Clock = std::chrono::steady_clock;
  const auto acquireStart = frameProfilingEnabled_ ? Clock::now() : Clock::time_point{};
  lastFrameTimings_ = {};
  u32 imageIndex = 0;
  const rhi::SwapchainStatus acquireStatus = swapchain_->acquireNextImage(&imageIndex);
  if (acquireStatus != rhi::SwapchainStatus::Ok &&
      acquireStatus != rhi::SwapchainStatus::SuboptimalNeedsRecreate) {
    return acquireStatus;
  }

  // O crossing C++→C# medido isoladamente: só o tempo de FillInstanceBuffer,
  // não o frame Vulkan inteiro — a pergunta da PoC-A é sobre o custo da
  // fronteira de interop especificamente.
  const auto fillStart = std::chrono::steady_clock::now();
  if (dirtRoadPreview_) {
    drawnInstanceCount_ = instanceCount_;
    lastExtractionStatus_ = 0;
  } else if (scenePreview_) {
    const int count = extractScene_(static_cast<renderer::RenderInstance *>(instanceBuffer_.mappedData()),
        static_cast<int>(instanceCount_), sizeof(renderer::RenderInstance), renderer::RenderInstanceAbiVersion);
    const int status = count >= 0 && static_cast<u32>(count) <= instanceCount_ ? 0 : (count < 0 ? count : -5);
    if (status != lastExtractionStatus_)
      __android_log_print(status == 0 ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR, LogTag,
          "[ScenePreview] extraction_status=%d capacity=%u", status, instanceCount_);
    lastExtractionStatus_ = status;
    drawnInstanceCount_ = status == 0 ? static_cast<u32>(count) : 0;
    // On invalid data submit a clear-only frame: acquire already owns a semaphore.
    // Never draw stale entities or abandon an unsignalled in-flight fence.
  } else {
    fillInstanceBuffer_(static_cast<float *>(instanceBuffer_.mappedData()),
                        static_cast<int>(instanceCount_), timeSeconds);
    drawnInstanceCount_ = instanceCount_;
  }
  const auto fillEnd = std::chrono::steady_clock::now();
  lastFillMicroseconds_ =
      std::chrono::duration<double, std::micro>(fillEnd - fillStart).count();
  if (frameProfilingEnabled_) {
    lastFrameTimings_.acquireMs = std::chrono::duration<double, std::milli>(fillStart - acquireStart).count();
    lastFrameTimings_.interopMs = lastFillMicroseconds_ / 1000.0;
  }
  if (!dirtRoadPreview_ && !memoryAllocator_->flushBuffer(instanceBuffer_)) return rhi::SwapchainStatus::FatalError;

  if (vkResetCommandBuffer(commandBuffer_, 0) != VK_SUCCESS) return rhi::SwapchainStatus::FatalError;
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  if (vkBeginCommandBuffer(commandBuffer_, &beginInfo) != VK_SUCCESS) {
    return rhi::SwapchainStatus::FatalError;
  }

  VkClearValue clearValues[2]{};
  clearValues[0].color = {{0.02f, 0.02f, 0.05f, 1.0f}};
  clearValues[1].depthStencil = {1.0f, 0};

  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = renderPass_;
  renderPassInfo.framebuffer = framebuffers_[imageIndex];
  renderPassInfo.renderArea.extent = {swapchain_->width(), swapchain_->height()};
  renderPassInfo.clearValueCount = 2;
  renderPassInfo.pClearValues = clearValues;
  vkCmdBeginRenderPass(commandBuffer_, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
  // Item 2.1.6 do plano: marcador de debug em volta do desenho do cubo —
  // aparece como um grupo nomeado em RenderDoc/Android GPU Inspector ao
  // capturar um frame. No-op em build release.
  rhiDevice_->cmdBeginDebugLabel(commandBuffer_, dirtRoadPreview_ ? "DirtRoad/map" : "InstancedRenderer/cube", 0.2f, 0.6f, 0.9f);

  vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
  const VkDescriptorSet bindlessSet = textureSet_;
  if (!dirtRoadPreview_ || useBindless_)
    vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1,
                            &bindlessSet, 0, nullptr);
  const VkExtent2D displayExtent = swapchain_->displayExtent();
  const rhi::SurfaceTransform &surfaceTransform = swapchain_->surfaceTransform();
  const FramePushConstants pushConstants{
      timeSeconds,
      static_cast<float>(displayExtent.width) / static_cast<float>(displayExtent.height),
      camera.yaw,
      camera.pitch,
      surfaceTransform.xx, surfaceTransform.xy, surfaceTransform.yx, surfaceTransform.yy,
      baseTextureIndex_,
      1.15f, materialParameters_.roughness, materialParameters_.metallic,
      (swapchain_->imageFormat()==VK_FORMAT_B8G8R8A8_SRGB ||
       swapchain_->imageFormat()==VK_FORMAT_R8G8B8A8_SRGB) ? 0u : 1u,
      materialParameters_.normalScale,
      {},
  };
  if (!dirtRoadPreview_)
    vkCmdPushConstants(commandBuffer_, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(pushConstants), &pushConstants);

  VkViewport viewport{};
  viewport.width = static_cast<float>(swapchain_->width());
  viewport.height = static_cast<float>(swapchain_->height());
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer_, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.extent = {swapchain_->width(), swapchain_->height()};
  vkCmdSetScissor(commandBuffer_, 0, 1, &scissor);

  if (dirtRoadPreview_) {
    DirtRoadPushConstants skyPush{};
    skyPush.cameraFrame[0] = static_cast<float>(displayExtent.width) / static_cast<float>(displayExtent.height);
    skyPush.cameraFrame[1] = camera.yaw;
    skyPush.cameraFrame[2] = camera.pitch;
    skyPush.surfaceTransform[0] = surfaceTransform.xx;
    skyPush.surfaceTransform[1] = surfaceTransform.xy;
    skyPush.surfaceTransform[2] = surfaceTransform.yx;
    skyPush.surfaceTransform[3] = surfaceTransform.yy;
    vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, skyPipeline_);
    vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, skyPipelineLayout_,
                            0, 1, &environmentSet_, 0, nullptr);
    vkCmdPushConstants(commandBuffer_, skyPipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT,
                       0, sizeof(skyPush), &skyPush);
    vkCmdDraw(commandBuffer_, 3, 1, 0, 0);
    vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_,
                            1, 1, &environmentSet_, 0, nullptr);
    if (useBindless_)
      vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_,
                              0, 1, &textureSet_, 0, nullptr);
  }

  VkDeviceSize offset = 0;
  const VkBuffer instanceBufferHandle = instanceBuffer_.handle();
  vkCmdBindVertexBuffers(commandBuffer_, 1, 1, &instanceBufferHandle, &offset);
  if (dirtRoadPreview_) {
    const VkBuffer mesh = dirtRoadResources_.vertexBuffer();
    vkCmdBindVertexBuffers(commandBuffer_, 0, 1, &mesh, &offset);
    vkCmdBindIndexBuffer(commandBuffer_, dirtRoadResources_.indexBuffer(), 0, VK_INDEX_TYPE_UINT32);
    const bool encodeSrgb = swapchain_->imageFormat()!=VK_FORMAT_B8G8R8A8_SRGB &&
                            swapchain_->imageFormat()!=VK_FORMAT_R8G8B8A8_SRGB;
    auto drawMapPrimitive = [&](u32 drawIndex) {
      const auto &draw = dirtRoadResources_.draws()[drawIndex];
      const auto &material = dirtRoadResources_.materials()[draw.materialIndex];
      if (!useBindless_) {
        const VkDescriptorSet set = dirtMaterialSets_[draw.materialIndex];
        vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_,
                                0, 1, &set, 0, nullptr);
      }
      DirtRoadPushConstants push{};
      push.cameraFrame[0] = static_cast<float>(displayExtent.width) / static_cast<float>(displayExtent.height);
      push.cameraFrame[1] = camera.yaw; push.cameraFrame[2] = camera.pitch; push.cameraFrame[3] = 1.25f;
      push.surfaceTransform[0]=surfaceTransform.xx;push.surfaceTransform[1]=surfaceTransform.xy;
      push.surfaceTransform[2]=surfaceTransform.yx;push.surfaceTransform[3]=surfaceTransform.yy;
      std::memcpy(push.cameraPositionNear, camera.position, sizeof(camera.position));
      push.cameraPositionNear[3] = dirtRoadResources_.header().nearPlane;
      std::memcpy(push.baseColorFactor, material.baseColorFactor, sizeof(push.baseColorFactor));
      std::memcpy(push.emissiveFactorAndStrength, material.emissiveFactorAndStrength,
                  sizeof(push.emissiveFactorAndStrength));
      for (u32 slot = 0; slot < 4; ++slot) {
        const u32 texture = material.textureIndices[slot];
        push.textureIndices[slot] = useBindless_ && texture != renderer::InvalidMapTexture
                                        ? dirtTextureSlots_[texture] : baseTextureIndex_;
      }
      push.materialFlags[0]=material.flags;push.materialFlags[1]=material.textureCoordinates;
      push.materialFlags[2]=encodeSrgb?1u:0u;
      push.materialFlags[3]=std::bit_cast<u32>(dirtRoadResources_.header().farPlane);
      push.materialFactors[0]=material.roughness;push.materialFactors[1]=material.metallic;
      push.materialFactors[2]=material.normalScale;push.materialFactors[3]=material.specular;
      vkCmdPushConstants(commandBuffer_,pipelineLayout_,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
                         0,sizeof(push),&push);
      vkCmdDrawIndexed(commandBuffer_,draw.indexCount,1,draw.firstIndex,
                       static_cast<i32>(draw.vertexOffset),drawIndex);
    };
    for (u32 drawIndex = 0; drawIndex < dirtRoadResources_.draws().size(); ++drawIndex) {
      const auto &draw = dirtRoadResources_.draws()[drawIndex];
      if ((dirtRoadResources_.materials()[draw.materialIndex].flags & 1u) == 0) drawMapPrimitive(drawIndex);
    }
    std::sort(transparentDrawOrder_.begin(), transparentDrawOrder_.end(), [&](u32 left, u32 right) {
      const auto &a=dirtRoadResources_.draws()[left];const auto &b=dirtRoadResources_.draws()[right];
      float da=0,db=0;for(u32 axis=0;axis<3;++axis){const float av=a.boundsCenter[axis]-camera.position[axis];
        const float bv=b.boundsCenter[axis]-camera.position[axis];da+=av*av;db+=bv*bv;}return da>db;
    });
    if (!transparentDrawOrder_.empty()) {
      vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, transparentPipeline_);
      for (u32 drawIndex : transparentDrawOrder_) drawMapPrimitive(drawIndex);
    }
  } else if (materialPreview_) {
    const VkBuffer mesh=materialResources_.vertexBuffer();
    vkCmdBindVertexBuffers(commandBuffer_,0,1,&mesh,&offset);
    vkCmdBindIndexBuffer(commandBuffer_,materialResources_.indexBuffer(),0,VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer_,materialResources_.indexCount(),drawnInstanceCount_,0,0,0);
  } else {
    vkCmdDraw(commandBuffer_, 36, drawnInstanceCount_, 0, 0);
  }

  rhiDevice_->cmdEndDebugLabel(commandBuffer_);
  vkCmdEndRenderPass(commandBuffer_);
  if (vkEndCommandBuffer(commandBuffer_) != VK_SUCCESS) return rhi::SwapchainStatus::FatalError;

  const VkSemaphore waitSemaphore = swapchain_->imageAvailableSemaphore();
  const VkSemaphore signalSemaphore = swapchain_->renderFinishedSemaphore(imageIndex);
  VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = &waitSemaphore;
  submitInfo.pWaitDstStageMask = &waitStage;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer_;
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = &signalSemaphore;

  if (vkQueueSubmit(graphicsQueue_, 1, &submitInfo, swapchain_->inFlightFence()) != VK_SUCCESS) {
    return rhi::SwapchainStatus::FatalError;
  }

  const auto presentStart = frameProfilingEnabled_ ? Clock::now() : Clock::time_point{};
  const rhi::SwapchainStatus presentStatus = swapchain_->present(imageIndex);
  if (frameProfilingEnabled_) {
    const auto presentEnd = Clock::now();
    lastFrameTimings_.recordSubmitMs = std::chrono::duration<double, std::milli>(presentStart - fillEnd).count();
    lastFrameTimings_.presentMs = std::chrono::duration<double, std::milli>(presentEnd - presentStart).count();
  }
  if (presentStatus != rhi::SwapchainStatus::Ok) return presentStatus;
  return acquireStatus == rhi::SwapchainStatus::SuboptimalNeedsRecreate
             ? rhi::SwapchainStatus::SuboptimalNeedsRecreate
             : rhi::SwapchainStatus::Ok;
}

} // namespace ae::platform::android
