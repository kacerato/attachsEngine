#include "platform/android/instanced_renderer.h"

#include "rhi/shaders/instanced_spirv.h"
#include "rhi/shaders/instanced_fallback_spirv.h"
#include "rhi/shaders/scene_preview_spirv.h"
#include "rhi/shaders/material_preview_spirv.h"
#include "rhi/shaders/material_fallback_spirv.h"
#include "rhi/shaders/dirt_road_spirv.h"
#include "rhi/shaders/dirt_road_fallback_spirv.h"
#include "rhi/shaders/dirt_road_coverage_spirv.h"
#include "rhi/shaders/dirt_road_coverage_fallback_spirv.h"
#include "rhi/shaders/dirt_road_sky_spirv.h"
#include "rhi/shaders/runtime_hud_spirv.h"
#include "rhi/shaders/hzb_reduce_first_spirv.h"
#include "rhi/shaders/hzb_reduce_spirv.h"
#include "renderer/sphere_mesh.h"

#include <android/log.h>
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
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

// Per-frame data is updated once after the in-flight fence is acquired. Camera
// trigonometry belongs on the CPU once per frame, not in every vertex
// invocation. The first 128 bytes intentionally preserve EnvironmentLighting's
// serialized ABI; the trailing rows are transient runtime data.
struct DirtRoadFrameUniform {
  EnvironmentLighting environment{};
  float worldToViewRow0[4]{1.0f, 0.0f, 0.0f, 0.0f};
  float worldToViewRow1[4]{0.0f, 1.0f, 0.0f, 0.0f};
  float worldToViewRow2[4]{0.0f, 0.0f, 1.0f, 0.0f};
};
static_assert(sizeof(DirtRoadFrameUniform) == 176);

struct RuntimeHudPushConstants {
  float centerHalfSize[4];
  float displaySize[4];
  float surfaceTransform[4];
  u32 parameters[4];
};
static_assert(sizeof(RuntimeHudPushConstants) == 64);

bool formatSupportsDepthAttachment(VkPhysicalDevice physicalDevice, VkFormat format,
                                   bool requireSampling) {
  VkFormatProperties properties{};
  vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);
  const VkFormatFeatureFlags required = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT |
      (requireSampling ? VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT : 0u);
  return (properties.optimalTilingFeatures & required) == required;
}

VkFormat chooseDepthFormat(VkPhysicalDevice physicalDevice, bool requireSampling) {
  // HZB samples the depth aspect. Prefer stencil-free formats in that path so
  // the single image view has an unambiguous sampled aspect on every driver.
  constexpr VkFormat sampledCandidates[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D16_UNORM};
  constexpr VkFormat attachmentCandidates[] = {
      VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM};
  const VkFormat *candidates = requireSampling ? sampledCandidates : attachmentCandidates;
  const usize candidateCount = requireSampling ? std::size(sampledCandidates)
                                               : std::size(attachmentCandidates);
  for (usize index = 0; index < candidateCount; ++index) {
    if (formatSupportsDepthAttachment(physicalDevice, candidates[index], requireSampling))
      return candidates[index];
  }
  return VK_FORMAT_UNDEFINED;
}

bool hasStencil(VkFormat format) {
  return format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D32_SFLOAT_S8_UINT;
}

bool sameCameraPose(const platform::FreeCameraState &left,
                    const platform::FreeCameraState &right) {
  return left.position[0] == right.position[0] &&
         left.position[1] == right.position[1] &&
         left.position[2] == right.position[2] &&
         left.yaw == right.yaw && left.pitch == right.pitch;
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
  // Quem decide é o render graph, não este arquivo: HZB amostra o depth depois
  // do pass principal, e DONT_CARE nesse caso deixa o conteúdo indefinido —
  // causa raiz da oclusão aparentemente aleatória. A mesma política resolve o
  // usage e o memoryless da imagem, para que os três não possam divergir.
  depthAttachment.storeOp = frameAttachmentPolicy_.depthStored
                                ? VK_ATTACHMENT_STORE_OP_STORE
                                : VK_ATTACHMENT_STORE_OP_DONT_CARE;
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
  VkShaderModule coverageFragModule = dirtRoadPreview_
      ? (useBindless_
          ? createShaderModule(device_, rhi::shaders::kDirt_Road_CoverageFragSpirv,
                               rhi::shaders::kDirt_Road_CoverageFragSpirvSize)
          : createShaderModule(device_, rhi::shaders::kDirt_Road_Coverage_FallbackFragSpirv,
                               rhi::shaders::kDirt_Road_Coverage_FallbackFragSpirvSize))
      : VK_NULL_HANDLE;
  if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE ||
      (dirtRoadPreview_ && coverageFragModule == VK_NULL_HANDLE)) {
    if (vertModule != VK_NULL_HANDLE) vkDestroyShaderModule(device_, vertModule, nullptr);
    if (fragModule != VK_NULL_HANDLE) vkDestroyShaderModule(device_, fragModule, nullptr);
    if (coverageFragModule != VK_NULL_HANDLE)
      vkDestroyShaderModule(device_, coverageFragModule, nullptr);
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
  // Diagnostic variants are true pipeline variants. A specialization
  // constant lets the driver eliminate the isolated shader path, avoiding
  // register pressure and instruction scheduling from a runtime uniform
  // branch. Normal rendering specializes to Full (zero).
  const u32 gpuIsolationValue = static_cast<u32>(gpuCostIsolation_);
  VkSpecializationMapEntry gpuIsolationEntry{};
  gpuIsolationEntry.constantID = 0;
  gpuIsolationEntry.offset = 0;
  gpuIsolationEntry.size = sizeof(gpuIsolationValue);
  VkSpecializationInfo gpuIsolationInfo{};
  gpuIsolationInfo.mapEntryCount = 1;
  gpuIsolationInfo.pMapEntries = &gpuIsolationEntry;
  gpuIsolationInfo.dataSize = sizeof(gpuIsolationValue);
  gpuIsolationInfo.pData = &gpuIsolationValue;
  stages[1].pSpecializationInfo = dirtRoadPreview_ ? &gpuIsolationInfo : nullptr;

  // Um binding por instância (VK_VERTEX_INPUT_RATE_INSTANCE) — o cubo em si
  // não tem vertex buffer (gerado por gl_VertexIndex igual ao TriangleRenderer,
  // ver instanced.vert), só a posição/cor avança por instância, não por
  // vértice.
  VkVertexInputBindingDescription binding{};
  binding.binding = 1;
  binding.stride = instanceStride();
  binding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

  VkVertexInputAttributeDescription attributes[14]{};
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
  const u32 mapVertexStride = dirtRoadPreview_ ? dirtRoadResources_.header().vertexStride
                                                : renderer::MapVertexStride;
  VkVertexInputBindingDescription bindings[2]={binding,{0,static_cast<u32>(materialPreview_?sizeof(renderer::MeshVertex):mapVertexStride),VK_VERTEX_INPUT_RATE_VERTEX}};
  if (materialPreview_) {
    attributes[5]={5,0,VK_FORMAT_R32G32B32_SFLOAT,0};
    attributes[6]={6,0,VK_FORMAT_R32G32B32_SFLOAT,12};
    attributes[7]={7,0,VK_FORMAT_R32G32B32A32_SFLOAT,24};
    attributes[8]={8,0,VK_FORMAT_R32G32_SFLOAT,40};
  }
  if (dirtRoadPreview_) {
    attributes[0]={0,0,VK_FORMAT_R32G32B32_SFLOAT,0};
    const bool packed = mapVertexStride == renderer::MapVertexStride;
    attributes[1]={1,0,packed?VK_FORMAT_R16G16B16A16_SNORM:VK_FORMAT_R32G32B32_SFLOAT,12};
    attributes[2]={2,0,packed?VK_FORMAT_R16G16B16A16_SNORM:VK_FORMAT_R32G32B32A32_SFLOAT,
                   packed?20u:24u};
    attributes[3]={3,0,VK_FORMAT_R32G32_SFLOAT,packed?28u:40u};
    attributes[4]={4,0,VK_FORMAT_R32G32_SFLOAT,packed?36u:48u};
    attributes[5]={5,0,packed?VK_FORMAT_R8G8B8A8_UNORM:VK_FORMAT_R32G32B32A32_SFLOAT,
                   packed?44u:56u};
    for(u32 i=0;i<5;++i)attributes[6+i]={6+i,1,VK_FORMAT_R32G32B32A32_SFLOAT,i*16u};
    for(u32 i=0;i<3;++i)attributes[11+i]={11+i,1,VK_FORMAT_R32G32B32A32_SFLOAT,80u+i*16u};
  }

  VkPipelineVertexInputStateCreateInfo vertexInput{};
  vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInput.vertexBindingDescriptionCount = (materialPreview_ || dirtRoadPreview_) ? 2 : 1;
  vertexInput.pVertexBindingDescriptions = bindings;
  vertexInput.vertexAttributeDescriptionCount = dirtRoadPreview_ ? 14 : (materialPreview_ ? 9 : (scenePreview_ ? 5 : 2));
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
  // glTF uses counter-clockwise front faces. This was irrelevant while every
  // pipeline disabled culling, but becomes part of the correctness contract
  // as soon as solid geometry enables backface rejection.
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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
      // MASK global em duas fases: primeiro apenas alpha+depth; depois PBR só
      // na camada visível (EQUAL). Preserva pixels e evita sombrear as muitas
      // folhas ocultas por outras folhas.
      stages[1].module = coverageFragModule;
      stages[1].pSpecializationInfo = nullptr;
      colorBlendAttachment.colorWriteMask = 0;
      pipelineOk = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                             &coveragePipeline_) == VK_SUCCESS;

      stages[1].module = fragModule;
      stages[1].pSpecializationInfo = &gpuIsolationInfo;
      colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
      depthStencil.depthWriteEnable = VK_FALSE;
      depthStencil.depthCompareOp = VK_COMPARE_OP_EQUAL;
      if (pipelineOk)
        pipelineOk = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                               &coverageShadePipeline_) == VK_SUCCESS;

      depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
      colorBlendAttachment.blendEnable = VK_TRUE;
      colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
      colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
      colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
      colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
      if (pipelineOk)
        pipelineOk = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                               &transparentPipeline_) == VK_SUCCESS;
    }
  }

  vkDestroyShaderModule(device_, vertModule, nullptr);
  vkDestroyShaderModule(device_, fragModule, nullptr);
  if (coverageFragModule != VK_NULL_HANDLE)
    vkDestroyShaderModule(device_, coverageFragModule, nullptr);
  return layoutOk && pipelineOk;
}

bool InstancedRenderer::createEnvironmentDescriptors() {
  if (!dirtRoadPreview_) return true;
  rhi::BufferDesc buffer{};
  buffer.sizeBytes = sizeof(DirtRoadFrameUniform);
  buffer.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  buffer.cpuAccess = rhi::CpuAccess::SequentialWrite;
  buffer.preferDeviceMemory = false;
  if (!memoryAllocator_->createBuffer(buffer, &environmentUniform_)) return false;
  DirtRoadFrameUniform initialFrame{};
  initialFrame.environment = dirtRoadResources_.environmentLighting();
  std::memcpy(environmentUniform_.mappedData(), &initialFrame, sizeof(initialFrame));
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
  VkDescriptorBufferInfo uniform{environmentUniform_.handle(), 0, sizeof(DirtRoadFrameUniform)};
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
  // The sky is submitted after depth-writing opaque geometry. At depth 1 it
  // shades only clear pixels, avoiding a full-resolution fragment pass behind
  // the scene while producing the exact same color where the sky is visible.
  depth.depthTestEnable = VK_TRUE; depth.depthWriteEnable = VK_FALSE;
  depth.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
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
  VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(DirtRoadPushConstants)};
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

bool InstancedRenderer::createRuntimeHudPipeline() {
  if (!runtimeHudEnabled_) return true;
  VkShaderModule vert = createShaderModule(device_, rhi::shaders::kRuntime_HudVertSpirv,
                                           rhi::shaders::kRuntime_HudVertSpirvSize);
  VkShaderModule frag = createShaderModule(device_, rhi::shaders::kRuntime_HudFragSpirv,
                                           rhi::shaders::kRuntime_HudFragSpirvSize);
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
  raster.polygonMode = VK_POLYGON_MODE_FILL; raster.cullMode = VK_CULL_MODE_NONE; raster.lineWidth = 1.0f;
  VkPipelineMultisampleStateCreateInfo multisample{};
  multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  VkPipelineDepthStencilStateCreateInfo depth{};
  depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth.depthTestEnable = VK_FALSE; depth.depthWriteEnable = VK_FALSE;
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
  blend.attachmentCount = 1; blend.pAttachments = &attachment;
  VkDynamicState states[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic{};
  dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic.dynamicStateCount = 2; dynamic.pDynamicStates = states;
  VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(RuntimeHudPushConstants)};
  VkPipelineLayoutCreateInfo layout{};
  layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layout.pushConstantRangeCount = 1; layout.pPushConstantRanges = &push;
  bool ok = vkCreatePipelineLayout(device_, &layout, nullptr, &runtimeHudPipelineLayout_) == VK_SUCCESS;
  if (ok) {
    VkGraphicsPipelineCreateInfo pipeline{};
    pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline.stageCount = 2; pipeline.pStages = stages; pipeline.pVertexInputState = &vertex;
    pipeline.pInputAssemblyState = &assembly; pipeline.pViewportState = &viewport;
    pipeline.pRasterizationState = &raster; pipeline.pMultisampleState = &multisample;
    pipeline.pDepthStencilState = &depth; pipeline.pColorBlendState = &blend;
    pipeline.pDynamicState = &dynamic; pipeline.layout = runtimeHudPipelineLayout_;
    pipeline.renderPass = renderPass_; pipeline.subpass = 0;
    ok = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline, nullptr,
                                   &runtimeHudPipeline_) == VK_SUCCESS;
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
    auto *instances = static_cast<renderer::GpuMeshInstance *>(instanceBuffer_.mappedData());
    const auto &draws = dirtRoadResources_.draws();
    if (draws.size() != instanceCount_) return false;
    // One hysteresis slot per draw, indexed directly by drawIndex -- render
    // chunks are built once at load and never reordered, so this stays
    // stable across every frame this renderer instance is alive.
    hzbHysteresis_.assign(instanceCount_, renderer::HzbHysteresisState{});
    const float tint[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    for (u32 index = 0; index < instanceCount_; ++index) {
      // Matriz singular/NaN rejeita o pacote inteiro em vez de escrever um
      // registro parcial que o shader leria como lixo.
      if (!renderer::buildGpuMeshInstance(draws[index].model, tint, &instances[index])) return false;
    }
    if (!memoryAllocator_->flushBuffer(instanceBuffer_)) return false;
    VkPhysicalDeviceFeatures features{};
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceFeatures(physicalDevice_, &features);
    vkGetPhysicalDeviceProperties(physicalDevice_, &properties);
    useMultiDrawIndirect_ = features.multiDrawIndirect && features.drawIndirectFirstInstance &&
                            properties.limits.maxDrawIndirectCount >= instanceCount_;
    if (useMultiDrawIndirect_) {
      rhi::BufferDesc indirect{};
      indirect.sizeBytes = static_cast<u64>(instanceCount_) *
                           sizeof(VkDrawIndexedIndirectCommand);
      indirect.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
      indirect.memoryClass = rhi::MemoryClass::Buffer;
      indirect.cpuAccess = rhi::CpuAccess::SequentialWrite;
      indirect.preferDeviceMemory = false;
      if (!memoryAllocator_->createBuffer(indirect, &indirectBuffer_) ||
          indirectBuffer_.mappedData() == nullptr) return false;
      indirectCommands_.reserve(instanceCount_);
      indirectSolidBatches_.reserve(dirtRoadResources_.materials().size());
      indirectCoverageBatches_.reserve(dirtRoadResources_.materials().size());
    }
    return true;
  }
  return true;
}

bool InstancedRenderer::createDepthImage() {
  // Resolvida uma vez por (re)criação de recursos, antes de qualquer decisão de
  // formato/usage/anexo, para que todas as três leiam o mesmo resultado.
  renderer::FrameGraphInputs graphInputs{};
  graphInputs.width = swapchain_->width();
  graphInputs.height = swapchain_->height();
  graphInputs.hzbEnabled = hzbWorkloadEligible_;
  frameAttachmentPolicy_ = renderer::resolveFrameAttachmentPolicy(graphInputs);
  if (!frameAttachmentPolicy_.valid) {
    // Falha aberta: sem política compilada, armazenar é o comportamento que
    // nunca produz conteúdo indefinido.
    frameAttachmentPolicy_.depthStored = true;
    frameAttachmentPolicy_.depthSampled = hzbWorkloadEligible_;
    frameAttachmentPolicy_.depthMemoryless = false;
    __android_log_print(ANDROID_LOG_WARN, LogTag,
        "[FrameGraph] política de anexos não compilou; usando store conservador.");
  }
  depthFormat_ = chooseDepthFormat(physicalDevice_, frameAttachmentPolicy_.depthSampled);
  if (depthFormat_ == VK_FORMAT_UNDEFINED && frameAttachmentPolicy_.depthSampled) {
    // Optional capability failure must not take the renderer down. Fall back
    // to the ordinary depth attachment and keep HZB disabled for this device.
    __android_log_print(ANDROID_LOG_WARN, LogTag,
                        "[HZB] nenhum formato depth sampled suportado; fallback visível ativo.");
    hzbWorkloadEligible_ = false;
    graphInputs.hzbEnabled = false;
    frameAttachmentPolicy_ = renderer::resolveFrameAttachmentPolicy(graphInputs);
    depthFormat_ = chooseDepthFormat(physicalDevice_, false);
  }
  if (depthFormat_ == VK_FORMAT_UNDEFINED) return false;
  rhi::ImageDesc desc{};
  desc.width = swapchain_->width();
  desc.height = swapchain_->height();
  desc.format = depthFormat_;
  desc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
               (frameAttachmentPolicy_.depthSampled ? VK_IMAGE_USAGE_SAMPLED_BIT : 0u);
  desc.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  if (hasStencil(depthFormat_)) desc.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
  desc.memoryClass = rhi::MemoryClass::RenderTarget;
  // Sem leitor fora do render pass, o depth nunca precisa de lastro em DRAM.
  desc.transient = frameAttachmentPolicy_.depthMemoryless;
  if (!memoryAllocator_->createImage(desc, &depthImage_)) return false;
  __android_log_print(ANDROID_LOG_INFO, LogTag,
      "[FrameGraph] depth %ux%u: store=%s sampled=%s memoryless=%s.",
      desc.width, desc.height, frameAttachmentPolicy_.depthStored ? "sim" : "nao",
      frameAttachmentPolicy_.depthSampled ? "sim" : "nao",
      frameAttachmentPolicy_.depthMemoryless ? "sim" : "nao");
  return true;
}

namespace {
struct HzbReduceFirstPushConstants {
  u32 sourceWidth;
  u32 sourceHeight;
  u32 destWidth;
  u32 destHeight;
};
struct HzbReducePushConstants {
  u32 previousWidth;
  u32 previousHeight;
};
} // namespace

bool InstancedRenderer::createHzbPipeline(const u32 *vertSpirv, u32 vertSpirvSize, const u32 *fragSpirv,
                                          u32 fragSpirvSize, u32 pushConstantBytes,
                                          VkPipelineLayout &outLayout, VkPipeline &outPipeline) {
  VkShaderModule vert = createShaderModule(device_, vertSpirv, vertSpirvSize);
  VkShaderModule frag = createShaderModule(device_, fragSpirv, fragSpirvSize);
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
  // No vertex buffer (fullscreen triangle from gl_VertexIndex, matches
  // hzb_reduce_first.vert/hzb_reduce.vert) and no depth/stencil attachment
  // (hzbRenderPass_ has none) -- the smallest possible fixed-function state.
  VkPipelineVertexInputStateCreateInfo vertex{};
  vertex.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  VkPipelineInputAssemblyStateCreateInfo assembly{};
  assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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
  VkPipelineColorBlendAttachmentState attachment{};
  attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT; // R32_SFLOAT: single channel.
  VkPipelineColorBlendStateCreateInfo blend{};
  blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  blend.attachmentCount = 1;
  blend.pAttachments = &attachment;
  VkDynamicState states[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic{};
  dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic.dynamicStateCount = 2;
  dynamic.pDynamicStates = states;
  VkPushConstantRange push{VK_SHADER_STAGE_FRAGMENT_BIT, 0, pushConstantBytes};
  VkPipelineLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layoutInfo.setLayoutCount = 1;
  layoutInfo.pSetLayouts = &hzbDescriptorSetLayout_;
  layoutInfo.pushConstantRangeCount = 1;
  layoutInfo.pPushConstantRanges = &push;
  bool ok = vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &outLayout) == VK_SUCCESS;
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
    pipeline.pColorBlendState = &blend;
    pipeline.pDynamicState = &dynamic;
    pipeline.layout = outLayout;
    pipeline.renderPass = hzbRenderPass_;
    pipeline.subpass = 0;
    ok = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline, nullptr, &outPipeline) == VK_SUCCESS;
  }
  vkDestroyShaderModule(device_, vert, nullptr);
  vkDestroyShaderModule(device_, frag, nullptr);
  return ok;
}

void InstancedRenderer::destroyHzbResources() {
  for (HzbLevelResources &level : hzbLevels_) {
    if (level.framebuffer != VK_NULL_HANDLE) {
      vkDestroyFramebuffer(device_, level.framebuffer, nullptr);
      level.framebuffer = VK_NULL_HANDLE;
    }
    level.descriptorSet = VK_NULL_HANDLE; // Freed implicitly with hzbDescriptorPool_ below.
    level.image.reset();
    level.width = 0;
    level.height = 0;
    level.readbackOffsetFloats = 0;
  }
  hzbReadbackBuffer_.reset();
  if (hzbDescriptorPool_ != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(device_, hzbDescriptorPool_, nullptr);
    hzbDescriptorPool_ = VK_NULL_HANDLE;
  }
  if (hzbDescriptorSetLayout_ != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(device_, hzbDescriptorSetLayout_, nullptr);
    hzbDescriptorSetLayout_ = VK_NULL_HANDLE;
  }
  hzbSampler_.shutdown();
  if (hzbFirstPipeline_ != VK_NULL_HANDLE) {
    vkDestroyPipeline(device_, hzbFirstPipeline_, nullptr);
    hzbFirstPipeline_ = VK_NULL_HANDLE;
  }
  if (hzbFirstPipelineLayout_ != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device_, hzbFirstPipelineLayout_, nullptr);
    hzbFirstPipelineLayout_ = VK_NULL_HANDLE;
  }
  if (hzbReducePipeline_ != VK_NULL_HANDLE) {
    vkDestroyPipeline(device_, hzbReducePipeline_, nullptr);
    hzbReducePipeline_ = VK_NULL_HANDLE;
  }
  if (hzbReducePipelineLayout_ != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device_, hzbReducePipelineLayout_, nullptr);
    hzbReducePipelineLayout_ = VK_NULL_HANDLE;
  }
  if (hzbRenderPass_ != VK_NULL_HANDLE) {
    vkDestroyRenderPass(device_, hzbRenderPass_, nullptr);
    hzbRenderPass_ = VK_NULL_HANDLE;
  }
  hzbResourcesReady_ = false;
  hzbPyramid_ = renderer::HzbPyramid{};
  hzbPyramidValid_ = false;
  hzbRecordedCameraValid_ = false;
  hzbPyramidCameraValid_ = false;
  hzbFrameEligible_ = false;
  hzbPreviousFrameEligible_ = false;
  hzbLevelDims_ = {};
}

bool InstancedRenderer::createHzbResources() {
  if (!hzbWorkloadEligible_ || !dirtRoadPreview_) return true; // Nothing to build; not an error.
  destroyHzbResources(); // Idempotent re-entry guard.

  // Level 0 is a coarse fraction of the real swapchain resolution; every
  // later level halves via ceiling-divide, the exact reduction
  // hzb_reduce.frag/renderer::buildHzbPyramid implement, so the GPU chain's
  // shape always matches what the CPU pure layer expects/validates.
  u32 width = std::max(swapchain_->width() / 8u, 4u);
  u32 height = std::max(swapchain_->height() / 8u, 4u);
  u32 readbackOffsetFloats = 0;
  for (u32 level = 0; level < kHzbLevelCount; ++level) {
    hzbLevels_[level].width = width;
    hzbLevels_[level].height = height;
    hzbLevels_[level].readbackOffsetFloats = readbackOffsetFloats;
    hzbLevelDims_[level] = {width, height};
    readbackOffsetFloats += width * height;
    width = (width + 1) / 2;
    height = (height + 1) / 2;
  }

  constexpr VkFormat kHzbFormat = VK_FORMAT_R32_SFLOAT;
  {
    VkAttachmentDescription attachment{};
    attachment.format = kHzbFormat;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // Every texel is fully overwritten.
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // Always exits sampled-ready: the very next level's pass (or, for the
    // last level, the batched copy-to-buffer after the whole chain) can
    // consume it with zero extra barriers for the "was this just written"
    // hazard -- only the copy needs one more transition, applied once for
    // all levels together in recordHzbReductionPass().
    attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    // Explicit entry/exit dependencies rather than relying on the render
    // pass's implicit external ones. With a single frame in flight this is a
    // formality (acquireNextImage's fence wait already guarantees any prior
    // use of these images fully completed before this frame starts
    // recording), kept explicit because that guarantee is exactly the kind
    // of assumption this comment exists to make checkable later.
    VkSubpassDependency dependencies[2]{};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;

    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 1;
    info.pAttachments = &attachment;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 2;
    info.pDependencies = dependencies;
    if (vkCreateRenderPass(device_, &info, nullptr, &hzbRenderPass_) != VK_SUCCESS) return false;
  }

  {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 1;
    info.pBindings = &binding;
    if (vkCreateDescriptorSetLayout(device_, &info, nullptr, &hzbDescriptorSetLayout_) != VK_SUCCESS)
      return false;
  }
  {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = kHzbLevelCount;
    VkDescriptorPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.maxSets = kHzbLevelCount;
    info.poolSizeCount = 1;
    info.pPoolSizes = &poolSize;
    if (vkCreateDescriptorPool(device_, &info, nullptr, &hzbDescriptorPool_) != VK_SUCCESS) return false;
  }

  rhi::SamplerDesc samplerDesc{};
  samplerDesc.minFilter = VK_FILTER_NEAREST;
  samplerDesc.magFilter = VK_FILTER_NEAREST;
  samplerDesc.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  samplerDesc.addressU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerDesc.addressV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerDesc.addressW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerDesc.minLod = 0.0f;
  samplerDesc.maxLod = 0.0f;
  // Filtering is moot regardless: both hzb_reduce* fragment shaders sample
  // via texelFetch, which bypasses the sampler's filter state entirely.
  // Nearest is chosen only so the sampler object's own intent is unambiguous.
  if (!hzbSampler_.initialize(device_, samplerDesc)) return false;

  if (!createHzbPipeline(rhi::shaders::kHzb_Reduce_FirstVertSpirv, rhi::shaders::kHzb_Reduce_FirstVertSpirvSize,
                         rhi::shaders::kHzb_Reduce_FirstFragSpirv, rhi::shaders::kHzb_Reduce_FirstFragSpirvSize,
                         sizeof(HzbReduceFirstPushConstants), hzbFirstPipelineLayout_, hzbFirstPipeline_))
    return false;
  if (!createHzbPipeline(rhi::shaders::kHzb_ReduceVertSpirv, rhi::shaders::kHzb_ReduceVertSpirvSize,
                         rhi::shaders::kHzb_ReduceFragSpirv, rhi::shaders::kHzb_ReduceFragSpirvSize,
                         sizeof(HzbReducePushConstants), hzbReducePipelineLayout_, hzbReducePipeline_))
    return false;

  const usize readbackFloats = readbackOffsetFloats;
  rhi::BufferDesc readbackDesc{};
  readbackDesc.sizeBytes = readbackFloats * sizeof(float);
  readbackDesc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  readbackDesc.memoryClass = rhi::MemoryClass::RenderTarget;
  readbackDesc.cpuAccess = rhi::CpuAccess::Random; // GPU writes, CPU reads repeatedly -- see invalidateBuffer.
  readbackDesc.preferDeviceMemory = false;
  if (!memoryAllocator_->createBuffer(readbackDesc, &hzbReadbackBuffer_) ||
      hzbReadbackBuffer_.mappedData() == nullptr)
    return false;

  for (u32 level = 0; level < kHzbLevelCount; ++level) {
    HzbLevelResources &resources = hzbLevels_[level];
    rhi::ImageDesc imageDesc{};
    imageDesc.width = resources.width;
    imageDesc.height = resources.height;
    imageDesc.format = kHzbFormat;
    imageDesc.usage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageDesc.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageDesc.memoryClass = rhi::MemoryClass::RenderTarget;
    if (!memoryAllocator_->createImage(imageDesc, &resources.image)) return false;

    const VkImageView ownView = resources.image.view();
    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = hzbRenderPass_;
    fbInfo.attachmentCount = 1;
    fbInfo.pAttachments = &ownView;
    fbInfo.width = resources.width;
    fbInfo.height = resources.height;
    fbInfo.layers = 1;
    if (vkCreateFramebuffer(device_, &fbInfo, nullptr, &resources.framebuffer) != VK_SUCCESS) return false;

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = hzbDescriptorPool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &hzbDescriptorSetLayout_;
    if (vkAllocateDescriptorSets(device_, &allocInfo, &resources.descriptorSet) != VK_SUCCESS) return false;

    const VkImageView sourceView = level == 0 ? depthImage_.view() : hzbLevels_[level - 1].image.view();
    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = hzbSampler_.handle();
    imageInfo.imageView = sourceView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = resources.descriptorSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
  }

  hzbResourcesReady_ = true;
  return true;
}

void InstancedRenderer::recordHzbReductionPass(const platform::FreeCameraState &camera) {
  if (!hzbResourcesReady_) return;

  // depthImage_ just finished being written by the main pass's LATE fragment
  // tests -- transition it for sampling before the first reduction pass
  // reads it. See PLANO-OTIMIZACAO-GLOBAL-GRAFICOS.md for why LATE (not
  // EARLY) fragment tests is the correct source stage here: LATE is where
  // the final depth *write* actually lands, and this barrier depends on that
  // write, not on the early depth *test*.
  VkImageMemoryBarrier depthToSampled{};
  depthToSampled.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  depthToSampled.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  depthToSampled.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  depthToSampled.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  depthToSampled.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  depthToSampled.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  depthToSampled.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  depthToSampled.image = depthImage_.handle();
  depthToSampled.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
  vkCmdPipelineBarrier(commandBuffer_, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &depthToSampled);

  for (u32 level = 0; level < kHzbLevelCount; ++level) {
    const HzbLevelResources &resources = hzbLevels_[level];
    VkRenderPassBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = hzbRenderPass_;
    beginInfo.framebuffer = resources.framebuffer;
    beginInfo.renderArea = {{0, 0}, {resources.width, resources.height}};
    vkCmdBeginRenderPass(commandBuffer_, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

    const VkViewport viewport{0.0f, 0.0f, static_cast<float>(resources.width),
                              static_cast<float>(resources.height), 0.0f, 1.0f};
    const VkRect2D scissor{{0, 0}, {resources.width, resources.height}};
    vkCmdSetViewport(commandBuffer_, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer_, 0, 1, &scissor);

    if (level == 0) {
      vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, hzbFirstPipeline_);
      vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, hzbFirstPipelineLayout_, 0, 1,
                              &resources.descriptorSet, 0, nullptr);
      const HzbReduceFirstPushConstants push{swapchain_->width(), swapchain_->height(), resources.width,
                                             resources.height};
      vkCmdPushConstants(commandBuffer_, hzbFirstPipelineLayout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push),
                         &push);
    } else {
      const HzbLevelResources &previous = hzbLevels_[level - 1];
      vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, hzbReducePipeline_);
      vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, hzbReducePipelineLayout_, 0, 1,
                              &resources.descriptorSet, 0, nullptr);
      const HzbReducePushConstants push{previous.width, previous.height};
      vkCmdPushConstants(commandBuffer_, hzbReducePipelineLayout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push),
                         &push);
    }
    vkCmdDraw(commandBuffer_, 3, 1, 0, 0);
    vkCmdEndRenderPass(commandBuffer_);
  }

  // Batched transition of every level from SHADER_READ_ONLY_OPTIMAL (each
  // level's own finalLayout, whether or not anything actually sampled it --
  // conservative src masks cover both cases uniformly) to TRANSFER_SRC so
  // the copies below can read them. One vkCmdPipelineBarrier for all levels,
  // recorded after the full chain instead of interleaved per-level, avoids
  // any level needing two round-trip layout transitions.
  VkImageMemoryBarrier toTransferSrc[kHzbLevelCount]{};
  for (u32 level = 0; level < kHzbLevelCount; ++level) {
    VkImageMemoryBarrier &barrier = toTransferSrc[level];
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = hzbLevels_[level].image.handle();
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  }
  vkCmdPipelineBarrier(commandBuffer_, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, kHzbLevelCount, toTransferSrc);

  for (u32 level = 0; level < kHzbLevelCount; ++level) {
    VkBufferImageCopy region{};
    region.bufferOffset = static_cast<VkDeviceSize>(hzbLevels_[level].readbackOffsetFloats) *
                          sizeof(float);
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {hzbLevels_[level].width, hzbLevels_[level].height, 1};
    vkCmdCopyImageToBuffer(commandBuffer_, hzbLevels_[level].image.handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           hzbReadbackBuffer_.handle(), 1, &region);
  }
  hzbRecordedCamera_ = camera;
  hzbRecordedCameraValid_ = true;
}

void InstancedRenderer::readHzbPyramidFromPreviousFrame() {
  hzbPyramidValid_ = false;
  if (!hzbResourcesReady_) return;
  // Safe without a new stall: acquireNextImage() already waited on this
  // frame-in-flight's fence, so the copy recorded by the PREVIOUS call to
  // recordHzbReductionPass() is guaranteed complete by the time this runs --
  // the same reasoning gpuFrameTimer_.collectPrevious() (called at the same
  // point in drawFrame) already relies on.
  if (!memoryAllocator_->invalidateBuffer(hzbReadbackBuffer_)) return;
  const usize floatCount = hzbReadbackBuffer_.sizeBytes() / sizeof(float);
  hzbPyramidValid_ = renderer::hzbPyramidFromLevels(static_cast<const float *>(hzbReadbackBuffer_.mappedData()),
                                                    floatCount, hzbLevelDims_.data(), kHzbLevelCount,
                                                    hzbPyramid_);
  hzbPyramidCameraValid_ = hzbPyramidValid_ && hzbRecordedCameraValid_;
  if (hzbPyramidCameraValid_) hzbPyramidCamera_ = hzbRecordedCamera_;
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
  if (!gpuFrameTimer_.initialize(device_, physicalDevice_, graphicsQueueFamily_)) {
    __android_log_print(ANDROID_LOG_WARN, LogTag,
                        "[FrameProfile] timestamps GPU indisponíveis nesta fila.");
  }

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
    instanceCount_ = static_cast<u32>(dirtRoadResources_.draws().size());
    for (u32 index = 0; index < instanceCount_; ++index) {
      const u32 material = dirtRoadResources_.draws()[index].materialIndex;
      const u32 flags = dirtRoadResources_.materials()[material].flags;
      if ((flags & renderer::MapMaterialBlend) != 0)
        transparentDrawOrder_.push_back(index);
      else if ((flags & renderer::MapMaterialAlphaMask) != 0)
        coverageDrawOrder_.push_back(index);
      else
        solidDrawOrder_.push_back(index);
    }
    visibleSolidDrawOrder_.reserve(solidDrawOrder_.size());
    visibleCoverageDrawOrder_.reserve(coverageDrawOrder_.size());
    visibleTransparentDrawOrder_.reserve(transparentDrawOrder_.size());
    // LOD groups (see renderer::selectLodLevel): spatial chunking happens
    // after import and can produce MANY draws for each imported LOD level.
    // Bucket by (lodGroupId,lodLevel); treating group.size() as level count
    // would select arbitrary level-0 chunks and silently drop the rest.
    if (!renderer::buildLodRenderGroups(dirtRoadResources_.draws(), solidDrawOrder_,
                                        lodGroups_, ungroupedSolidDrawOrder_)) {
      __android_log_print(ANDROID_LOG_ERROR, LogTag, "[LOD] grupos de runtime inválidos.");
      return false;
    }
    // Every frame contributes chunks for one active level and optionally
    // one neighbor, still a subset of all levels already in solidDrawOrder_.
    lodFilteredSolidDrawOrder_.reserve(solidDrawOrder_.size());
    const u32 maximumHzbCandidates = static_cast<u32>(
        solidDrawOrder_.size() + coverageDrawOrder_.size());
    hzbWorkloadEligible_ = hzbOcclusionEnabled_ &&
        renderer::shouldRunHzb(maximumHzbCandidates, hzbMinimumCandidateDraws_);
    if (hzbOcclusionEnabled_ && !hzbWorkloadEligible_) {
      __android_log_print(ANDROID_LOG_INFO, LogTag,
          "[HZB] solicitado, mas dispensado antes da alocação: candidatos máximos=%u limiar=%u.",
          maximumHzbCandidates, hzbMinimumCandidateDraws_);
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
  if (!createRuntimeHudPipeline()) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar o pipeline do HUD runtime.");
    return false;
  }
  if (!createFramebuffers()) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar os framebuffers instanciados.");
    return false;
  }
  // Opt-in and off by default (see setHzbOcclusionEnabled) -- a no-op that
  // returns true when disabled, so this never blocks initialization for the
  // renderer's existing, already-validated path.
  if (!createHzbResources()) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar recursos de HZB.");
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
  if (dirtRoadPreview_)
    __android_log_print(ANDROID_LOG_INFO, LogTag,
                        "[DirtRoad] passes solid=%zu coverage=%zu transparent=%zu coverage_prepass=%s; frustum_culling=conservador indirect=%s.",
                        solidDrawOrder_.size(), coverageDrawOrder_.size(), transparentDrawOrder_.size(),
                        coveragePrepassEnabled_ ? "on" : "off",
                        useMultiDrawIndirect_ ? "multi-draw" : "cpu-fallback");
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
  destroyHzbResources();
  hzbHysteresis_.clear();
  hzbWorkloadEligible_ = false;
  lodGroups_.clear();
  ungroupedSolidDrawOrder_.clear();
  lodFilteredSolidDrawOrder_.clear();
  if (pipeline_ != VK_NULL_HANDLE) {
    vkDestroyPipeline(device_, pipeline_, nullptr);
    pipeline_ = VK_NULL_HANDLE;
  }
  if (coveragePipeline_ != VK_NULL_HANDLE) {
    vkDestroyPipeline(device_, coveragePipeline_, nullptr);
    coveragePipeline_ = VK_NULL_HANDLE;
  }
  if (coverageShadePipeline_ != VK_NULL_HANDLE) {
    vkDestroyPipeline(device_, coverageShadePipeline_, nullptr);
    coverageShadePipeline_ = VK_NULL_HANDLE;
  }
  if (skyPipeline_ != VK_NULL_HANDLE) {
    vkDestroyPipeline(device_, skyPipeline_, nullptr);
    skyPipeline_ = VK_NULL_HANDLE;
  }
  if (runtimeHudPipeline_ != VK_NULL_HANDLE) {
    vkDestroyPipeline(device_, runtimeHudPipeline_, nullptr);
    runtimeHudPipeline_ = VK_NULL_HANDLE;
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
  if (runtimeHudPipelineLayout_ != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device_, runtimeHudPipelineLayout_, nullptr);
    runtimeHudPipelineLayout_ = VK_NULL_HANDLE;
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
  indirectBuffer_.reset();
  indirectCommands_.clear();
  indirectSolidBatches_.clear();
  indirectCoverageBatches_.clear();
  useMultiDrawIndirect_ = false;
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
  solidDrawOrder_.clear();
  coverageDrawOrder_.clear();
  transparentDrawOrder_.clear();
  visibleSolidDrawOrder_.clear();
  visibleCoverageDrawOrder_.clear();
  visibleTransparentDrawOrder_.clear();
  visibilityTelemetry_ = {};
  renderedFrameCount_ = 0;
  baseSampler_.shutdown();
  baseTexture_.reset();
  dummySampler_.shutdown();
  dummyTexture_.reset();
  depthImage_.reset();
  instanceBuffer_.reset();
  uploadContext_.shutdown();
  gpuFrameTimer_.shutdown();
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

void InstancedRenderer::beginGpuRegion(GpuPassClass pass) {
  if (commandBuffer_ == VK_NULL_HANDLE) return;
  const float *color = GpuPassClassLabelColors[static_cast<u32>(pass)];
  // O marcador não depende de frameProfilingEnabled_: ele é o que torna uma
  // captura AGI/RenderDoc legível, e captura acontece fora de uma sessão de
  // profiling. cmdBeginDebugLabel já é no-op sem VK_EXT_debug_utils.
  rhiDevice_->cmdBeginDebugLabel(commandBuffer_, gpuPassClassLabel(pass), color[0], color[1],
                                 color[2]);
}

void InstancedRenderer::endGpuRegion(GpuPassClass pass) {
  if (commandBuffer_ == VK_NULL_HANDLE) return;
  rhiDevice_->cmdEndDebugLabel(commandBuffer_);
  if (gpuTimingEnabled()) gpuFrameTimer_.markPassEnd(commandBuffer_, pass);
}

rhi::SwapchainStatus InstancedRenderer::drawFrame(float timeSeconds,
                                                  const platform::FreeCameraState &camera,
                                                  const renderer::RuntimeHudState &hud) {
  using Clock = std::chrono::steady_clock;
  const auto acquireStart = frameProfilingEnabled_ ? Clock::now() : Clock::time_point{};
  lastFrameTimings_ = {};
  u32 imageIndex = 0;
  const rhi::SwapchainStatus acquireStatus = swapchain_->acquireNextImage(&imageIndex);
  if (acquireStatus != rhi::SwapchainStatus::Ok &&
      acquireStatus != rhi::SwapchainStatus::SuboptimalNeedsRecreate) {
    return acquireStatus;
  }
  if (gpuTimingEnabled()) {
    rhi::GpuFrameTimings gpuTimings{};
    if (gpuFrameTimer_.collectPrevious(gpuTimings)) {
      lastFrameTimings_.gpuFrameMs = gpuTimings.frameMs;
      lastFrameTimings_.gpuPassMs = gpuTimings.passesMs;
    }
  }
  // Same "safe without a new stall" reasoning as collectPrevious() above --
  // see readHzbPyramidFromPreviousFrame()'s own comment.
  if (hzbPreviousFrameEligible_) readHzbPyramidFromPreviousFrame();
  else {
    hzbPyramidValid_ = false;
    hzbPyramidCameraValid_ = false;
  }
  hzbFrameEligible_ = false;

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
  if (dirtRoadPreview_) {
    auto *frame = static_cast<DirtRoadFrameUniform *>(environmentUniform_.mappedData());
    const float cy = std::cos(camera.yaw), sy = std::sin(camera.yaw);
    const float cp = std::cos(camera.pitch), sp = std::sin(camera.pitch);
    const float row0[4] = {cy, 0.0f, -sy, 0.0f};
    const float row1[4] = {sy * sp, cp, cy * sp, 0.0f};
    const float row2[4] = {sy * cp, -sp, cy * cp, 0.0f};
    std::memcpy(frame->worldToViewRow0, row0, sizeof(row0));
    std::memcpy(frame->worldToViewRow1, row1, sizeof(row1));
    std::memcpy(frame->worldToViewRow2, row2, sizeof(row2));
    if (!memoryAllocator_->flushBuffer(environmentUniform_))
      return rhi::SwapchainStatus::FatalError;
  }

  if (vkResetCommandBuffer(commandBuffer_, 0) != VK_SUCCESS) return rhi::SwapchainStatus::FatalError;
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  if (vkBeginCommandBuffer(commandBuffer_, &beginInfo) != VK_SUCCESS) {
    return rhi::SwapchainStatus::FatalError;
  }
  if (gpuTimingEnabled()) gpuFrameTimer_.begin(commandBuffer_);

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
  // Opaque abre aqui e não no primeiro draw: os binds de pipeline/descritor
  // abaixo são custo da geometria opaca, e deixá-los fora da região só moveria
  // esse custo para o buraco entre o início do frame e a primeira marca.
  beginGpuRegion(GpuPassClass::Opaque);

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
    visibilityTelemetry_ = {};
    auto pushMapMaterial = [&](u32 materialIndex) {
      const auto &material = dirtRoadResources_.materials()[materialIndex];
      if (!useBindless_) {
        const VkDescriptorSet set = dirtMaterialSets_[materialIndex];
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
      push.materialFlags[0]=material.flags;
      const u32 alphaCutoff = static_cast<u32>(std::clamp(material.alphaCutoff, 0.0f, 1.0f) * 255.0f + .5f);
      push.materialFlags[1]=material.textureCoordinates | (alphaCutoff << 8u);
      push.materialFlags[2]=encodeSrgb?1u:0u;
      push.materialFlags[3]=std::bit_cast<u32>(dirtRoadResources_.header().farPlane);
      push.materialFactors[0]=material.roughness;push.materialFactors[1]=material.metallic;
      push.materialFactors[2]=material.normalScale;push.materialFactors[3]=material.specular;
      vkCmdPushConstants(commandBuffer_,pipelineLayout_,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
                         0,sizeof(push),&push);
    };
    auto drawMapPrimitive = [&](u32 drawIndex) {
      const auto &draw = dirtRoadResources_.draws()[drawIndex];
      pushMapMaterial(draw.materialIndex);
      vkCmdDrawIndexed(commandBuffer_,draw.indexCount,1,draw.firstIndex,
                       static_cast<i32>(draw.vertexOffset),drawIndex);
      ++visibilityTelemetry_.submittedDrawCalls;
      visibilityTelemetry_.submittedTriangles += draw.indexCount / 3;
    };
    auto nearestBoundsDistanceSquared = [&](u32 drawIndex) {
      const auto &draw=dirtRoadResources_.draws()[drawIndex];float distanceSquared=0;
      for(u32 axis=0;axis<3;++axis){const float delta=draw.boundsCenter[axis]-camera.position[axis];
        distanceSquared+=delta*delta;}
      const float distance=std::sqrt(distanceSquared);
      const float nearest=std::max(0.0f,distance-draw.boundsRadius);
      return nearest*nearest;
    };
    const auto frontToBack = [&](u32 left, u32 right) {
      return nearestBoundsDistanceSquared(left)<nearestBoundsDistanceSquared(right);
    };
    renderer::PerspectiveVisibilitySettings visibilitySettings = visibilitySettings_;
    visibilitySettings.nearPlane = dirtRoadResources_.header().nearPlane;
    visibilitySettings.farPlane = dirtRoadResources_.header().farPlane;
    const renderer::PerspectiveFrustum frustum = renderer::buildPerspectiveFrustum(
        camera.position, camera.yaw, camera.pitch,
        static_cast<float>(displayExtent.width) / static_cast<float>(displayExtent.height),
        visibilitySettings);
    visibleSolidDrawOrder_.clear();
    visibleCoverageDrawOrder_.clear();
    visibleTransparentDrawOrder_.clear();

    // LOD selection (opt-in, see setLodSelectionEnabled): runs before
    // frustum culling because it decides WHICH draws are even candidates
    // this frame -- only the active level's chunks (plus, mid-transition,
    // the neighbor level fading in/out) enter lodFilteredSolidDrawOrder_,
    // never every level of every group at once. Blend/coverage draws are
    // never grouped (the cooker excludes them from simplification), so they
    // are untouched here.
    const std::vector<u32> *solidCandidates = &solidDrawOrder_;
    if (lodSelectionEnabled_ && !lodGroups_.empty()) {
      lodFilteredSolidDrawOrder_.clear();
      lodFilteredSolidDrawOrder_.insert(lodFilteredSolidDrawOrder_.end(),
                                        ungroupedSolidDrawOrder_.begin(), ungroupedSolidDrawOrder_.end());
      auto *instances = static_cast<renderer::GpuMeshInstance *>(instanceBuffer_.mappedData());
      for (renderer::LodRenderGroup &group : lodGroups_) {
        renderer::LodLevelInfo levelInfos[renderer::LodMaximumLevelsPerGroup]{};
        const u32 levelCount = group.levelCount;
        for (u32 level = 0; level < levelCount; ++level)
          levelInfos[level] = group.levels[level].selection;
        float distanceSquared = 0.0f;
        for (u32 axis = 0; axis < 3; ++axis) {
          const float delta = group.boundsCenter[axis] - camera.position[axis];
          distanceSquared += delta * delta;
        }
        // Nearest point of the conservative group sphere, not its center:
        // screen-space error must never underestimate a large object that
        // reaches close to the camera.
        const float distance = std::max(std::sqrt(distanceSquared) - group.boundsRadius, 1.0e-3f);
        const renderer::LodSelection selection = renderer::selectLodLevel(
            levelInfos, levelCount, distance, visibilitySettings.verticalFieldOfViewRadians,
            static_cast<float>(displayExtent.height), lodPixelErrorBudget_, lodHysteresisBandRatio_,
            group.hysteresis);
        // Clear every chunk in every level so a group leaving a transition
        // cannot retain stale coverage in its instance record.
        for (u32 level = 0; level < levelCount; ++level)
          for (u32 drawIndex : group.levels[level].drawIndices)
            instances[drawIndex].normalColumns[7] = 0.0f;
        const renderer::LodRenderLevel &active = group.levels[selection.level];
        lodFilteredSolidDrawOrder_.insert(lodFilteredSolidDrawOrder_.end(),
                                          active.drawIndices.begin(), active.drawIndices.end());
        if (selection.ditherToCoarserFactor > 0.0f && selection.level + 1 < levelCount) {
          const renderer::LodRenderLevel &neighbor = group.levels[selection.level + 1];
          const renderer::LodDitherPair dither =
              renderer::encodeLodDither(selection.ditherToCoarserFactor);
          for (u32 drawIndex : active.drawIndices)
            instances[drawIndex].normalColumns[7] = dither.outgoing;
          // Negative sign means "incoming": shader keeps the exact pixels
          // the positive outgoing mask discards. 1-factor produced overlap,
          // not complementary coverage.
          for (u32 drawIndex : neighbor.drawIndices)
            instances[drawIndex].normalColumns[7] = dither.incoming;
          lodFilteredSolidDrawOrder_.insert(lodFilteredSolidDrawOrder_.end(),
                                            neighbor.drawIndices.begin(), neighbor.drawIndices.end());
        }
      }
      if (!memoryAllocator_->flushBuffer(instanceBuffer_)) return rhi::SwapchainStatus::FatalError;
      solidCandidates = &lodFilteredSolidDrawOrder_;
    }

    auto collectVisible = [&](const std::vector<u32> &source, std::vector<u32> &destination) {
      for (u32 drawIndex : source) {
        const auto &draw = dirtRoadResources_.draws()[drawIndex];
        ++visibilityTelemetry_.candidateDraws;
        visibilityTelemetry_.candidateTriangles += draw.indexCount / 3;
        if (!renderer::isSphereVisible(frustum, draw.boundsCenter, draw.boundsRadius)) continue;
        destination.push_back(drawIndex);
      }
    };
    collectVisible(*solidCandidates, visibleSolidDrawOrder_);
    collectVisible(coverageDrawOrder_, visibleCoverageDrawOrder_);
    collectVisible(transparentDrawOrder_, visibleTransparentDrawOrder_);

    const u32 hzbCandidateDraws = static_cast<u32>(
        visibleSolidDrawOrder_.size() + visibleCoverageDrawOrder_.size());
    hzbFrameEligible_ = hzbOcclusionEnabled_ &&
        renderer::shouldRunHzb(hzbCandidateDraws, hzbMinimumCandidateDraws_);
    if (hzbOcclusionEnabled_ && !hzbFrameEligible_) {
      visibilityTelemetry_.hzbSkippedBudgetDraws = hzbCandidateDraws;
      for (u32 drawIndex : visibleSolidDrawOrder_) hzbHysteresis_[drawIndex] = {};
      for (u32 drawIndex : visibleCoverageDrawOrder_) hzbHysteresis_[drawIndex] = {};
    }

    // HZB occlusion (opt-in, see setHzbOcclusionEnabled): a second,
    // conservative filter over what already survived frustum culling above.
    // Blend/transparent draws are never tested here -- their back-to-front
    // ordering and visibility contract stay exactly as O1 defined them.
    auto filterByHzb = [&](std::vector<u32> &visible) {
      if (!hzbFrameEligible_ || !hzbPyramidValid_) return;
      if (!hzbPyramidCameraValid_ || !sameCameraPose(camera, hzbPyramidCamera_)) {
        visibilityTelemetry_.hzbSkippedCameraMotionDraws += static_cast<u32>(visible.size());
        // A draw culled from an older viewpoint must revive immediately when
        // the camera moves. The one-frame CPU readback path cannot safely
        // reproject newly exposed pixels, so movement fails open until the
        // same-frame GPU-driven HZB stage is implemented.
        for (u32 drawIndex : visible) hzbHysteresis_[drawIndex] = {};
        return;
      }
      const renderer::HzbScreenTransform hzbScreenTransform{
          surfaceTransform.xx, surfaceTransform.xy,
          surfaceTransform.yx, surfaceTransform.yy};
      usize writeIndex = 0;
      for (usize readIndex = 0; readIndex < visible.size(); ++readIndex) {
        const u32 drawIndex = visible[readIndex];
        const auto &draw = dirtRoadResources_.draws()[drawIndex];
        const renderer::HzbScreenRect rect =
            renderer::projectBoundsToHzbScreenRect(frustum, draw.boundsCenter, draw.boundsRadius,
                                                   hzbScreenTransform);
        const bool occludedThisFrame = renderer::isOccludedByHzb(
            hzbPyramid_, rect, hzbNormalizedDepthBias_);
        ++visibilityTelemetry_.hzbTestedDraws;
        renderer::HzbHysteresisState &state = hzbHysteresis_[drawIndex];
        const bool wasOccluded = state.occludedStreak > 0;
        const bool stillVisible = renderer::updateHzbHysteresis(state, occludedThisFrame, hzbHysteresisFrames_);
        if (!stillVisible) {
          ++visibilityTelemetry_.hzbOccludedDraws;
          continue; // Drop from the visible list -- writeIndex does not advance.
        }
        if (wasOccluded && state.occludedStreak == 0) ++visibilityTelemetry_.hzbRevivedDraws;
        visible[writeIndex++] = drawIndex;
      }
      visible.resize(writeIndex);
    };
    filterByHzb(visibleSolidDrawOrder_);
    filterByHzb(visibleCoverageDrawOrder_);

    auto sumTriangles = [&](const std::vector<u32> &visible) {
      u64 triangles = 0;
      for (u32 drawIndex : visible) triangles += dirtRoadResources_.draws()[drawIndex].indexCount / 3;
      return triangles;
    };
    visibilityTelemetry_.visibleTriangles = sumTriangles(visibleSolidDrawOrder_) +
        sumTriangles(visibleCoverageDrawOrder_) + sumTriangles(visibleTransparentDrawOrder_);
    visibilityTelemetry_.visibleDraws = static_cast<u32>(
        visibleSolidDrawOrder_.size() + visibleCoverageDrawOrder_.size() + visibleTransparentDrawOrder_.size());
    visibilityTelemetry_.culledDraws = visibilityTelemetry_.candidateDraws -
                                       visibilityTelemetry_.visibleDraws;

    std::sort(visibleSolidDrawOrder_.begin(), visibleSolidDrawOrder_.end(), frontToBack);
    std::sort(visibleCoverageDrawOrder_.begin(), visibleCoverageDrawOrder_.end(), frontToBack);
    auto buildIndirectBatches = [&](const std::vector<u32> &visible,
                                    std::vector<IndirectBatch> &batches) {
      batches.clear();
      for (u32 drawIndex : visible) {
        const u32 materialIndex = dirtRoadResources_.draws()[drawIndex].materialIndex;
        const auto existing = std::find_if(batches.begin(), batches.end(),
            [&](const IndirectBatch &batch) { return batch.materialIndex == materialIndex; });
        if (existing == batches.end()) batches.push_back({materialIndex, 0, 0, 0});
      }
      for (IndirectBatch &batch : batches) {
        batch.firstCommand = static_cast<u32>(indirectCommands_.size());
        for (u32 drawIndex : visible) {
          const auto &draw = dirtRoadResources_.draws()[drawIndex];
          if (draw.materialIndex != batch.materialIndex) continue;
          indirectCommands_.push_back({draw.indexCount, 1, draw.firstIndex,
                                       static_cast<i32>(draw.vertexOffset), drawIndex});
          ++batch.commandCount;
          batch.triangles += draw.indexCount / 3;
        }
      }
    };
    auto submitIndirectBatches = [&](const std::vector<IndirectBatch> &batches) {
      for (const IndirectBatch &batch : batches) {
        pushMapMaterial(batch.materialIndex);
        vkCmdDrawIndexedIndirect(commandBuffer_, indirectBuffer_.handle(),
            static_cast<VkDeviceSize>(batch.firstCommand) * sizeof(VkDrawIndexedIndirectCommand),
            batch.commandCount, sizeof(VkDrawIndexedIndirectCommand));
        ++visibilityTelemetry_.submittedDrawCalls;
        visibilityTelemetry_.submittedTriangles += batch.triangles;
      }
    };
    if (useMultiDrawIndirect_) {
      indirectCommands_.clear();
      buildIndirectBatches(visibleSolidDrawOrder_, indirectSolidBatches_);
      buildIndirectBatches(visibleCoverageDrawOrder_, indirectCoverageBatches_);
      if (!indirectCommands_.empty())
        std::memcpy(indirectBuffer_.mappedData(), indirectCommands_.data(),
                    indirectCommands_.size() * sizeof(VkDrawIndexedIndirectCommand));
      if (!memoryAllocator_->flushBuffer(indirectBuffer_))
        return rhi::SwapchainStatus::FatalError;
      submitIndirectBatches(indirectSolidBatches_);
    } else {
      for (u32 drawIndex : visibleSolidDrawOrder_) drawMapPrimitive(drawIndex);
    }
    // Vegetação alpha-mask sai do mesmo balde que os opacos sólidos. As duas
    // classes têm custo de fragment muito diferente e o programa de margem
    // atribui uma faixa própria à folhagem; medi-las somadas tornava essa
    // atribuição impossível de verificar.
    endGpuRegion(GpuPassClass::Opaque);
    beginGpuRegion(GpuPassClass::Coverage);
    if (!visibleCoverageDrawOrder_.empty()) {
      if (coveragePrepassEnabled_)
        vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, coveragePipeline_);
      if (useMultiDrawIndirect_)
        submitIndirectBatches(indirectCoverageBatches_);
      else
        for (u32 drawIndex : visibleCoverageDrawOrder_) drawMapPrimitive(drawIndex);
      if (coveragePrepassEnabled_) {
        vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, coverageShadePipeline_);
        if (useMultiDrawIndirect_)
          submitIndirectBatches(indirectCoverageBatches_);
        else
          for (u32 drawIndex : visibleCoverageDrawOrder_) drawMapPrimitive(drawIndex);
      }
    }
    endGpuRegion(GpuPassClass::Coverage);
    beginGpuRegion(GpuPassClass::Sky);

    DirtRoadPushConstants skyPush{};
    skyPush.cameraFrame[0] = static_cast<float>(displayExtent.width) / static_cast<float>(displayExtent.height);
    skyPush.cameraFrame[1] = camera.yaw;
    skyPush.cameraFrame[2] = camera.pitch;
    skyPush.cameraFrame[3] = timeSeconds;
    skyPush.surfaceTransform[0] = surfaceTransform.xx;
    skyPush.surfaceTransform[1] = surfaceTransform.xy;
    skyPush.surfaceTransform[2] = surfaceTransform.yx;
    skyPush.surfaceTransform[3] = surfaceTransform.yy;
    skyPush.materialFlags[2] = encodeSrgb ? 1u : 0u;
    vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, skyPipeline_);
    vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, skyPipelineLayout_,
                            0, 1, &environmentSet_, 0, nullptr);
    vkCmdPushConstants(commandBuffer_, skyPipelineLayout_,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(skyPush), &skyPush);
    vkCmdDraw(commandBuffer_, 3, 1, 0, 0);
    endGpuRegion(GpuPassClass::Sky);
    beginGpuRegion(GpuPassClass::Transparent);

    std::sort(visibleTransparentDrawOrder_.begin(), visibleTransparentDrawOrder_.end(), [&](u32 left, u32 right) {
      const auto &a=dirtRoadResources_.draws()[left];const auto &b=dirtRoadResources_.draws()[right];
      float da=0,db=0;for(u32 axis=0;axis<3;++axis){const float av=a.boundsCenter[axis]-camera.position[axis];
        const float bv=b.boundsCenter[axis]-camera.position[axis];da+=av*av;db+=bv*bv;}return da>db;
    });
    if (!visibleTransparentDrawOrder_.empty()) {
      vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, transparentPipeline_);
      vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_,
                              1, 1, &environmentSet_, 0, nullptr);
      if (useBindless_)
        vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_,
                                0, 1, &textureSet_, 0, nullptr);
      for (u32 drawIndex : visibleTransparentDrawOrder_) {
        drawMapPrimitive(drawIndex);
      }
    }
    endGpuRegion(GpuPassClass::Transparent);
  } else if (materialPreview_) {
    const VkBuffer mesh=materialResources_.vertexBuffer();
    vkCmdBindVertexBuffers(commandBuffer_,0,1,&mesh,&offset);
    vkCmdBindIndexBuffer(commandBuffer_,materialResources_.indexBuffer(),0,VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer_,materialResources_.indexCount(),drawnInstanceCount_,0,0,0);
    // Cena sem folhagem/céu/transparência: essas classes ficam com zero em vez
    // de somarem seu tempo à classe seguinte (ver VulkanGpuFrameTimer).
    endGpuRegion(GpuPassClass::Opaque);
  } else {
    vkCmdDraw(commandBuffer_, 36, drawnInstanceCount_, 0, 0);
    endGpuRegion(GpuPassClass::Opaque);
  }

  // A região abre fora do if: o HUD desenhava sem nenhuma marca e seu custo
  // caía no intervalo não atribuído entre a última classe e o fim do frame.
  beginGpuRegion(GpuPassClass::Ui);
  if (runtimeHudPipeline_ != VK_NULL_HANDLE && hud.visible) {
    vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, runtimeHudPipeline_);
    const float displayWidth = static_cast<float>(displayExtent.width);
    const float displayHeight = static_cast<float>(displayExtent.height);
    const float shortEdge = std::min(displayWidth, displayHeight);
    const float radius = hud.joystickRadiusPixels > 1.0f
                             ? hud.joystickRadiusPixels : shortEdge * 0.105f;
    const float centerX = hud.joystickActive ? hud.joystickCenterX : displayWidth * 0.145f;
    const float centerY = hud.joystickActive ? hud.joystickCenterY : displayHeight * 0.78f;
    const float knobX = hud.joystickActive ? hud.joystickKnobX : centerX;
    const float knobY = hud.joystickActive ? hud.joystickKnobY : centerY;
    auto drawElement = [&](float x, float y, float halfWidth, float halfHeight,
                           u32 kind, u32 value) {
      RuntimeHudPushConstants push{};
      push.centerHalfSize[0]=x; push.centerHalfSize[1]=y;
      push.centerHalfSize[2]=halfWidth; push.centerHalfSize[3]=halfHeight;
      push.displaySize[0]=displayWidth; push.displaySize[1]=displayHeight;
      push.surfaceTransform[0]=surfaceTransform.xx; push.surfaceTransform[1]=surfaceTransform.xy;
      push.surfaceTransform[2]=surfaceTransform.yx; push.surfaceTransform[3]=surfaceTransform.yy;
      push.parameters[0]=kind; push.parameters[1]=value;
      vkCmdPushConstants(commandBuffer_,runtimeHudPipelineLayout_,
                         VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
                         0,sizeof(push),&push);
      vkCmdDraw(commandBuffer_,6,1,0,0);
    };
    drawElement(centerX,centerY,radius,radius,0,0);
    drawElement(knobX,knobY,radius*.42f,radius*.42f,1,0);
    drawElement(displayWidth-shortEdge*.105f,shortEdge*.075f,
                shortEdge*.083f,shortEdge*.040f,2,hud.framesPerSecond);
  }

  endGpuRegion(GpuPassClass::Ui);

  rhiDevice_->cmdEndDebugLabel(commandBuffer_);
  vkCmdEndRenderPass(commandBuffer_);
  // Must run after the main pass ends (depthImage_ needs its final write
  // landed, in DEPTH_STENCIL_ATTACHMENT_OPTIMAL) and before submit; a no-op
  // when HZB occlusion is disabled or its resources failed to initialize.
  beginGpuRegion(GpuPassClass::Hzb);
  if (hzbFrameEligible_) recordHzbReductionPass(camera);
  endGpuRegion(GpuPassClass::Hzb);
  hzbPreviousFrameEligible_ = hzbFrameEligible_;
  if (gpuTimingEnabled()) gpuFrameTimer_.end(commandBuffer_);
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
  if (dirtRoadPreview_ && (++renderedFrameCount_ == 1 || renderedFrameCount_ % 240 == 0)) {
    __android_log_print(ANDROID_LOG_INFO, LogTag,
        "[Visibility] candidates=%u visible=%u culled=%u submitted_draws=%u triangles_visible=%llu/%llu triangles_submitted=%llu hzb_tested=%u hzb_occluded=%u hzb_revived=%u hzb_motion_skip=%u hzb_budget_skip=%u",
        visibilityTelemetry_.candidateDraws, visibilityTelemetry_.visibleDraws,
        visibilityTelemetry_.culledDraws, visibilityTelemetry_.submittedDrawCalls,
        static_cast<unsigned long long>(visibilityTelemetry_.visibleTriangles),
        static_cast<unsigned long long>(visibilityTelemetry_.candidateTriangles),
        static_cast<unsigned long long>(visibilityTelemetry_.submittedTriangles),
        visibilityTelemetry_.hzbTestedDraws, visibilityTelemetry_.hzbOccludedDraws,
        visibilityTelemetry_.hzbRevivedDraws,
        visibilityTelemetry_.hzbSkippedCameraMotionDraws,
        visibilityTelemetry_.hzbSkippedBudgetDraws);
  }
  return acquireStatus == rhi::SwapchainStatus::SuboptimalNeedsRecreate
             ? rhi::SwapchainStatus::SuboptimalNeedsRecreate
             : rhi::SwapchainStatus::Ok;
}

} // namespace ae::platform::android
