#include "platform/android/instanced_renderer.h"
#include "platform/android/android_texture_loader.h"
#include "renderer/water_detail_texture.h"
#include "renderer/water_authoring_geometry.h"

#include "rhi/shaders/editor_grid_spirv.h"
#include "rhi/shaders/instanced_spirv.h"
#include "rhi/shaders/instanced_fallback_spirv.h"
#include "rhi/shaders/scene_preview_spirv.h"
#include "rhi/shaders/material_preview_spirv.h"
#include "rhi/shaders/material_fallback_spirv.h"
#include "rhi/shaders/dirt_road_spirv.h"
#include "rhi/shaders/dirt_road_fallback_spirv.h"
#include "rhi/shaders/dirt_road_coverage_spirv.h"
#include "rhi/shaders/dirt_road_coverage_fallback_spirv.h"
#include "rhi/shaders/dirt_road_coverage_shade_spirv.h"
#include "rhi/shaders/dirt_road_coverage_shade_fallback_spirv.h"
#include "rhi/shaders/dirt_road_sky_spirv.h"
#include "rhi/shaders/runtime_hud_spirv.h"
#include "rhi/shaders/post_process_spirv.h"
#include "rhi/shaders/post_process_temporal_spirv.h"
#include "rhi/shaders/shadow_depth_spirv.h"
#include "rhi/shaders/shadow_depth_masked_spirv.h"
#include "rhi/shaders/shadow_depth_masked_fallback_spirv.h"
#include "rhi/shaders/hzb_reduce_first_spirv.h"
#include "rhi/shaders/hzb_reduce_spirv.h"
#include "rhi/shaders/draw_compact_spirv.h"
#include "rhi/shaders/draw_cull_spirv.h"
#include "rhi/shaders/hzb_reduce_compute_spirv.h"
#include "rhi/shaders/water_surface_spirv.h"
#include "rhi/shaders/water_spectral_spirv.h"
#include "renderer/sphere_mesh.h"
#include "renderer/material_distance.h"

#include <android/log.h>
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
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
  // Budgets resolvidos, nunca nomes de preset: normal distance, specular
  // distance, ambiente hemisférico e sonda especular.
  float quality[4]{};
  float shadowViewProjection[renderer::MaximumShadowCascades][16]{};
  float shadowSplitDepths[4]{};
  float shadowParameters[4]{}; // inv atlas, cascade count, reserved, normal offset
  float shadowWorldUnitsPerTexel[4]{};
  float shadowFilterParameters[4]{}; // near radius, far radius, reserved
  float shadowTransitionParameters[4]{}; // cascade blend, distance fade, reserved
  float materialDistanceParameters[4]{}; // MR map, emissive map, reserved
  float waterParameters[4]{}; // wave count, base height, time, reserved
  float waterOptics[4]{}; // IOR, roughness, turbidity, foam threshold
  float waterDeepColorFoam[4]{}; // linear RGB, foam decay
  float waterShallowColorDistance[4]{}; // linear RGB, maximum distance
  float waterAbsorption[4]{}; // Beer-Lambert coefficients, reserved
  float waterWaveShape[renderer::MaximumWaterWaves][4]{}; // dir.xy, amplitude, wave number
  float waterWaveMotion[renderer::MaximumWaterWaves][4]{}; // speed, steepness, phase, reserved
  float waterInteractionParameters[4]{}; // active count, reserved
  float waterInteractionShape[renderer::MaximumWaterInteractions][4]{}; // center.xy, start, amplitude
  float waterInteractionMotion[renderer::MaximumWaterInteractions][4]{}; // wavelength, speed, decay, duration
  // centro x, centro z, lado da área em metros, resolução da grade. Resolução
  // zero significa "não há ondulação" e o vértice pula a leitura inteira.
  float waterRippleArea[4]{};
  float waterSurfaceDetail[4]{}; // foam elevation/coverage, micro height/wavelength
  // Luzes pontuais e spot da cena. `x` é quantas valem neste quadro; o resto do
  // vetor existe para manter o alinhamento std140 do array que vem depois.
  float punctualLightParameters[4]{};
  renderer::PunctualLight punctualLights[renderer::MaximumPunctualLights]{};
};
static_assert(sizeof(DirtRoadFrameUniform) == 1584);

struct ShadowPushConstants {
  float lightViewProjection[16]{};
  float alphaCutoffUvSlot[4]{};
  u32 baseTextureIndex[4]{};
};
static_assert(sizeof(ShadowPushConstants) == 96);

// Teto de instancias da interface por frame. A tela cheia do editor com a
// hierarquia aberta usa por volta de 500; o teto e folgado o bastante para uma
// lista longa e pequeno o bastante para o buffer caber em 1,3 MiB.
constexpr u32 kUiInstanceCapacity = 16384;

struct RuntimeHudPushConstants {
  float centerHalfSize[4];
  float displaySize[4];
  float surfaceTransform[4];
  u32 parameters[4];
};
static_assert(sizeof(RuntimeHudPushConstants) == 64);

struct PostPushConstants {
  float texelFlags[4]{};
  float bloom[4]{};
  float grade[4]{};
  float sourceTransform[4]{}; // active UV scale, focal length, display aspect
  float currentCamera[4]{};
  float currentPositionNear[4]{};
  float previousCamera[4]{};
  float previousPositionFar[4]{};
};
static_assert(sizeof(PostPushConstants) == 128);

float halton(u64 index, u32 base) {
  float result = 0.0f;
  float fraction = 1.0f;
  while (index > 0) {
    fraction /= static_cast<float>(base);
    result += fraction * static_cast<float>(index % base);
    index /= base;
  }
  return result;
}

u32 packSurfaceTransform(const rhi::SurfaceTransform &transform) {
  const auto component = [](float value) -> u32 {
    return static_cast<u32>(std::clamp(std::lround(value), -1l, 1l) + 1l);
  };
  return component(transform.xx) | (component(transform.xy) << 2u) |
         (component(transform.yx) << 4u) | (component(transform.yy) << 6u);
}

float wrappedAngleDistance(float left, float right) {
  return std::abs(std::remainder(left - right, 6.28318530718f));
}

bool temporalCameraCut(const platform::FreeCameraState &current,
                       const platform::FreeCameraState &previous) {
  constexpr float MaximumPositionDeltaSquared = 25.0f;
  constexpr float MaximumAngularDelta = 0.45f;
  const float dx = current.position[0] - previous.position[0];
  const float dy = current.position[1] - previous.position[1];
  const float dz = current.position[2] - previous.position[2];
  return !std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(dz) ||
         dx * dx + dy * dy + dz * dz > MaximumPositionDeltaSquared ||
         wrappedAngleDistance(current.yaw, previous.yaw) > MaximumAngularDelta ||
         wrappedAngleDistance(current.pitch, previous.pitch) > MaximumAngularDelta;
}

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

void InstancedRenderer::applyRuntimeRenderingPolicy(
    const renderer::ResolvedRenderingPolicy &policy, bool preserveDynamicScale) {
  renderingPolicy_ = policy;
  if (preserveDynamicScale)
    dynamicResolution_.reconfigure(policy.dynamicResolution, policy.frame.gpuLaneBudgetMs);
  else
    dynamicResolution_.reset(policy.dynamicResolution, policy.frame.gpuLaneBudgetMs);
  invalidateStaticShadowCache();
  lodSelectionEnabled_ = policy.geometry.lodSelection;
  lodPixelErrorBudget_ = policy.visibility.lodPixelErrorBudget;
  coverageLodPixelErrorBudget_ = policy.visibility.coverageLodPixelErrorBudget;
  lodHysteresisBandRatio_ = policy.visibility.lodHysteresisBandRatio;
}

void InstancedRenderer::setRuntimeRenderingPolicy(
    const renderer::ResolvedRenderingPolicy &policy) {
  renderer::ResolvedRenderingPolicy active = policy;

  // Render-pass topology and immutable sampler/atlas allocation belong to the
  // resource epoch. Runtime pressure may consume less, never request resources
  // that were not built. Keeping the dedicated post pass alive while all its
  // optional filters are off makes recovery allocation-free.
  active.post.dedicatedPass = resourceRenderingPolicy_.post.dedicatedPass;
  if (!resourceRenderingPolicy_.post.dedicatedPass) {
    active.post.bloom = false;
    active.post.antiAliasing = renderer::AntiAliasingMode::Off;
    active.post.vignette = false;
    active.post.sharpen = 0.0f;
    active.post.contrast = 1.0f;
    active.post.saturation = 1.0f;
  }
  active.textures = resourceRenderingPolicy_.textures;
  active.shadows.enabled = active.shadows.enabled && resourceRenderingPolicy_.shadows.enabled;
  active.shadows.cascadeCount =
      std::min(active.shadows.cascadeCount, resourceRenderingPolicy_.shadows.cascadeCount);
  active.shadows.cascadeResolution =
      std::min(active.shadows.cascadeResolution, resourceRenderingPolicy_.shadows.cascadeResolution);
  active.resolutionScale = std::min(active.resolutionScale,
                                    resourceRenderingPolicy_.resolutionScale);
  active.dynamicResolution.maximumScale =
      std::min(active.dynamicResolution.maximumScale, active.resolutionScale);
  active.dynamicResolution.minimumScale =
      std::min(active.dynamicResolution.minimumScale,
               active.dynamicResolution.maximumScale);
  applyRuntimeRenderingPolicy(active, true);
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
  colorAttachment.finalLayout = renderingPolicy_.post.dedicatedPass
                                    ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                    : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

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
  depthAttachment.finalLayout = frameAttachmentPolicy_.depthSampled
                                    ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                    : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthRef{};
  depthRef.attachment = 1;
  depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference waterDepthInput{};
  waterDepthInput.attachment = 1;
  waterDepthInput.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  VkAttachmentReference waterDepthRef = waterDepthInput;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorRef;
  subpass.pDepthStencilAttachment = &depthRef;

  VkSubpassDescription subpasses[2] = {subpass, {}};
  if (waterSubpassActive_) {
    subpasses[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses[1].inputAttachmentCount = 1;
    subpasses[1].pInputAttachments = &waterDepthInput;
    subpasses[1].colorAttachmentCount = 1;
    subpasses[1].pColorAttachments = &colorRef;
    subpasses[1].pDepthStencilAttachment = &waterDepthRef;
  }

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
  info.subpassCount = waterSubpassActive_ ? 2u : 1u;
  info.pSubpasses = subpasses;
  VkSubpassDependency dependencies[3] = {dependency, {}, {}};
  u32 dependencyCount = 1;
  if (waterSubpassActive_) {
    dependencies[dependencyCount].srcSubpass = 0;
    dependencies[dependencyCount].dstSubpass = 1;
    dependencies[dependencyCount].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[dependencyCount].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[dependencyCount].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                                 VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[dependencyCount].dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT |
                                                  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    dependencies[dependencyCount].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    ++dependencyCount;
  }
  if (renderingPolicy_.post.dedicatedPass || frameAttachmentPolicy_.depthSampled) {
    VkSubpassDependency &exit = dependencies[dependencyCount++];
    exit.srcSubpass = waterSubpassActive_ ? 1u : 0u;
    exit.dstSubpass = VK_SUBPASS_EXTERNAL;
    exit.srcStageMask =
        (renderingPolicy_.post.dedicatedPass
             ? static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT)
             : VkPipelineStageFlags{0}) |
        (frameAttachmentPolicy_.depthSampled
             ? static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT)
             : VkPipelineStageFlags{0});
    exit.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    exit.srcAccessMask =
        (renderingPolicy_.post.dedicatedPass
             ? static_cast<VkAccessFlags>(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
             : VkAccessFlags{0}) |
        (frameAttachmentPolicy_.depthSampled
             ? static_cast<VkAccessFlags>(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
             : VkAccessFlags{0});
    exit.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  }
  info.dependencyCount = dependencyCount;
  info.pDependencies = dependencies;

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
  VkShaderModule coverageShadeFragModule = dirtRoadPreview_
      ? (useBindless_
          ? createShaderModule(device_, rhi::shaders::kDirt_Road_Coverage_ShadeFragSpirv,
                               rhi::shaders::kDirt_Road_Coverage_ShadeFragSpirvSize)
          : createShaderModule(device_,
                               rhi::shaders::kDirt_Road_Coverage_Shade_FallbackFragSpirv,
                               rhi::shaders::kDirt_Road_Coverage_Shade_FallbackFragSpirvSize))
      : VK_NULL_HANDLE;
  VkShaderModule waterFragModule = waterSubpassActive_
      ? (spectralWaterCount_>0
         ? createShaderModule(device_,rhi::shaders::kWater_SpectralFragSpirv,rhi::shaders::kWater_SpectralFragSpirvSize)
         : createShaderModule(device_, rhi::shaders::kWater_SurfaceFragSpirv,
                           rhi::shaders::kWater_SurfaceFragSpirvSize))
      : VK_NULL_HANDLE;
  VkShaderModule waterVertModule = waterSubpassActive_
      ? (spectralWaterCount_ > 0
         ? createShaderModule(device_, rhi::shaders::kWater_SpectralVertSpirv,
                              rhi::shaders::kWater_SpectralVertSpirvSize)
         : createShaderModule(device_, rhi::shaders::kWater_SurfaceVertSpirv,
                              rhi::shaders::kWater_SurfaceVertSpirvSize))
      : VK_NULL_HANDLE;
  if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE ||
      (dirtRoadPreview_ &&
       (coverageFragModule == VK_NULL_HANDLE || coverageShadeFragModule == VK_NULL_HANDLE)) ||
      (waterSubpassActive_ &&
       (waterFragModule == VK_NULL_HANDLE || waterVertModule == VK_NULL_HANDLE))) {
    if (vertModule != VK_NULL_HANDLE) vkDestroyShaderModule(device_, vertModule, nullptr);
    if (fragModule != VK_NULL_HANDLE) vkDestroyShaderModule(device_, fragModule, nullptr);
    if (coverageFragModule != VK_NULL_HANDLE)
      vkDestroyShaderModule(device_, coverageFragModule, nullptr);
    if (coverageShadeFragModule != VK_NULL_HANDLE)
      vkDestroyShaderModule(device_, coverageShadeFragModule, nullptr);
    if (waterFragModule != VK_NULL_HANDLE)
      vkDestroyShaderModule(device_, waterFragModule, nullptr);
    if (waterVertModule != VK_NULL_HANDLE)
      vkDestroyShaderModule(device_, waterVertModule, nullptr);
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
  struct MaterialSpecialization final {
    u32 isolation;
    u32 featureMask;
    // 0 = decide em runtime, 1 = equiretangular, 2 = octaedrica. Resolver a
    // projecao aqui apaga o ramo nao usado do binario: enquanto era um uniform,
    // o atan/acos/fract do caminho legado custava registradores em TODO
    // fragmento, mesmo com o pacote octaedrico carregado. Uma spec constant
    // deixa o compilador do driver eliminar o ramo morto (ver o guia de boas
    // praticas do Adreno sobre GPR e ocupacao de wave).
    u32 environmentProjection;
  };
  MaterialSpecialization materialSpecialization{
      static_cast<u32>(gpuCostIsolation_), renderer::DynamicMaterialFeatureMask,
      dirtRoadPreview_ && dirtRoadResources_.environmentMapDescription().specularProjection ==
              renderer::EnvironmentProjection::Octahedral
          ? 2u : 1u};
  VkSpecializationMapEntry specializationEntries[3]{};
  specializationEntries[0].constantID = 0;
  specializationEntries[0].offset = offsetof(MaterialSpecialization, isolation);
  specializationEntries[0].size = sizeof(u32);
  specializationEntries[1].constantID = 1;
  specializationEntries[1].offset = offsetof(MaterialSpecialization, featureMask);
  specializationEntries[1].size = sizeof(u32);
  specializationEntries[2].constantID = 2;
  specializationEntries[2].offset = offsetof(MaterialSpecialization, environmentProjection);
  specializationEntries[2].size = sizeof(u32);
  VkSpecializationInfo gpuIsolationInfo{};
  gpuIsolationInfo.mapEntryCount = 3;
  gpuIsolationInfo.pMapEntries = specializationEntries;
  gpuIsolationInfo.dataSize = sizeof(materialSpecialization);
  gpuIsolationInfo.pData = &materialSpecialization;
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

      stages[1].module = coverageShadeFragModule;
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
      pipelineInfo.subpass = waterSubpassActive_ ? 1u : 0u;
      if (pipelineOk)
        pipelineOk = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                               &transparentPipeline_) == VK_SUCCESS;
      pipelineInfo.subpass = 0;

      // Material detail LOD is a compiled pipeline choice, not merely a
      // per-fragment branch. Draw bounds wholly past the configured normal-map
      // radius use these variants; nearby bounds remain on the full pipeline.
      // Three state families are sufficient and avoid a combinatorial set per
      // distance × material.
      auto createDistantPipeline = [&](u32 family, VkPipeline *destination) {
        materialSpecialization.isolation =
            static_cast<u32>(renderer::GpuCostIsolation::NoNormalMap);
        materialSpecialization.featureMask = renderer::DynamicMaterialFeatureMask;
        stages[1].module = family == 1 ? coverageShadeFragModule : fragModule;
        stages[1].pSpecializationInfo = &gpuIsolationInfo;
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = family == 2 ? VK_TRUE : VK_FALSE;
        depthStencil.depthWriteEnable = family == 0 ? VK_TRUE : VK_FALSE;
        depthStencil.depthCompareOp = family == 1 ? VK_COMPARE_OP_EQUAL : VK_COMPARE_OP_LESS;
        pipelineInfo.subpass = family == 2 && waterSubpassActive_ ? 1u : 0u;
        const bool created = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                         destination) == VK_SUCCESS;
        pipelineInfo.subpass = 0;
        return created;
      };
      if (gpuCostIsolation_ == renderer::GpuCostIsolation::Full &&
          resourceRenderingPolicy_.materialDistance.normalMapMaximumDistance > 0.0f) {
        if (pipelineOk) pipelineOk = createDistantPipeline(0, &opaqueDistantPipeline_);
        if (pipelineOk) pipelineOk = createDistantPipeline(1, &coverageDistantPipeline_);
        if (pipelineOk) pipelineOk = createDistantPipeline(2, &transparentDistantPipeline_);
      }
      materialSpecialization.isolation = static_cast<u32>(gpuCostIsolation_);
      materialSpecialization.featureMask = renderer::DynamicMaterialFeatureMask;

      // Build only combinations actually referenced by this package. The key
      // excludes alpha mode, so the same compact feature space serves opaque,
      // coverage and transparent state families without a combinatorial cache.
      bool used[3][renderer::MaterialFeatureVariantCount]{};
      for (const auto &material : dirtRoadResources_.materials()) {
        const u32 variant = renderer::materialFeatureVariant(material.flags);
        const u32 family = (material.flags & renderer::MapMaterialAlphaMask) != 0 ? 1u
                           : (material.flags & renderer::MapMaterialBlend) != 0 ? 2u : 0u;
        used[family][variant] = true;
      }
      auto createMaterialVariants = [&](u32 family, VkPipeline *destination) {
        stages[1].module = family == 1 ? coverageShadeFragModule : fragModule;
        stages[1].pSpecializationInfo = &gpuIsolationInfo;
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = family == 2 ? VK_TRUE : VK_FALSE;
        depthStencil.depthWriteEnable = family == 0 ? VK_TRUE : VK_FALSE;
        depthStencil.depthCompareOp = family == 1 ? VK_COMPARE_OP_EQUAL : VK_COMPARE_OP_LESS;
        pipelineInfo.subpass = family == 2 && waterSubpassActive_ ? 1u : 0u;
        for (u32 variant = 0; variant < renderer::MaterialFeatureVariantCount; ++variant) {
          if (!used[family][variant]) continue;
          materialSpecialization.featureMask = renderer::materialFeatureMaskForVariant(variant);
          if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                        &destination[variant]) != VK_SUCCESS) {
            pipelineInfo.subpass = 0;
            return false;
          }
        }
        pipelineInfo.subpass = 0;
        materialSpecialization.featureMask = renderer::DynamicMaterialFeatureMask;
        return true;
      };
      if (resourceRenderingPolicy_.geometry.materialShaderVariants) {
        if (pipelineOk) pipelineOk = createMaterialVariants(0, opaqueMaterialPipelines_);
        if (pipelineOk) pipelineOk = createMaterialVariants(1, coverageMaterialPipelines_);
        if (pipelineOk) pipelineOk = createMaterialVariants(2, transparentMaterialPipelines_);
      }
      if (pipelineOk && waterSubpassActive_) {
        // Água tem vertex shader próprio mesmo no provedor analítico: é ele
        // que declara a grade de ondulação. Reusar o vertex geral obrigava a
        // declarar o binding 15 em todas as pipelines da cena ou fazia a
        // interação simplesmente desaparecer quando FFT estava desligada.
        stages[0].module = waterVertModule;
        stages[1].module = waterFragModule;
        stages[1].pSpecializationInfo = &gpuIsolationInfo;
        pipelineInfo.subpass = 1;
        depthStencil.depthWriteEnable = VK_FALSE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        colorBlendAttachment.blendEnable = VK_TRUE;
        // water_surface.frag returns premultiplied radiance. The destination
        // already contains opaque/sky color from subpass 0.
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        pipelineOk = pipelineOk && vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                               &waterPipeline_) == VK_SUCCESS;
        stages[0].module=vertModule;
        pipelineInfo.subpass = 0;
      }
    }
  }

  vkDestroyShaderModule(device_, vertModule, nullptr);
  vkDestroyShaderModule(device_, fragModule, nullptr);
  if (coverageFragModule != VK_NULL_HANDLE)
    vkDestroyShaderModule(device_, coverageFragModule, nullptr);
  if (coverageShadeFragModule != VK_NULL_HANDLE)
    vkDestroyShaderModule(device_, coverageShadeFragModule, nullptr);
  if (waterFragModule != VK_NULL_HANDLE)
    vkDestroyShaderModule(device_, waterFragModule, nullptr);
  if (waterVertModule != VK_NULL_HANDLE)
    vkDestroyShaderModule(device_, waterVertModule, nullptr);
  return layoutOk && pipelineOk;
}

bool InstancedRenderer::setWaterRipples(const renderer::WaterRippleField &field) noexcept {
  if (!field.isReady() || waterRippleBuffer_.mappedData() == nullptr) return false;
  const u32 resolution = field.settings().resolution;
  if (resolution == 0 || resolution > renderer::MaximumWaterGridSegments) return false;

  // Snapshot em lote: o campo preserva ownership e o renderer não paga quatro
  // amostras bilineares para reconstruir valores que já existem na grade.
  auto *destination = static_cast<float *>(waterRippleBuffer_.mappedData());
  const float area = field.settings().areaSize;
  const usize cellCount = static_cast<usize>(resolution) * resolution;
  if (!field.copyHeightsTo(std::span<float>(destination, cellCount))) return false;
  if (!memoryAllocator_->flushBuffer(waterRippleBuffer_)) return false;

  waterRippleResolution_ = resolution;
  waterRippleCentre_[0] = field.centreX();
  waterRippleCentre_[1] = field.centreZ();
  waterRippleArea_ = area;
  waterRippleGain_ = 1.0f;

  // Um relato periódico do que foi publicado. Sem ele, "a ondulação não
  // aparece" não distingue campo vazio de campo que o vértice não leu.
  static u32 reportCountdown = 0;
  if (reportCountdown == 0) {
    reportCountdown = 120;
    float peak = 0.0f;
    for (usize cell = 0; cell < cellCount; ++cell)
      peak = std::max(peak, std::abs(destination[cell]));
    __android_log_print(ANDROID_LOG_INFO, LogTag,
        "[WaterRipple] %ux%u area=%.0f m centro=(%.1f,%.1f) pico=%.4f m.",
        resolution, resolution, static_cast<double>(area),
        static_cast<double>(waterRippleCentre_[0]), static_cast<double>(waterRippleCentre_[1]),
        static_cast<double>(peak));
  }
  --reportCountdown;
  return true;
}

bool InstancedRenderer::createEnvironmentDescriptors() {
  if (!dirtRoadPreview_) return true;
  rhi::BufferDesc buffer{};
  buffer.sizeBytes = sizeof(DirtRoadFrameUniform);
  buffer.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  buffer.cpuAccess = rhi::CpuAccess::SequentialWrite;
  buffer.preferDeviceMemory = false;
  if (!memoryAllocator_->createBuffer(buffer, &environmentUniform_)) return false;
  // Capacidade fixa no teto do contrato da grade: realocar no meio do laço de
  // quadro obrigaria a reescrever o descritor, e o tamanho aqui é modesto.
  if(waterSubpassActive_) {
    rhi::BufferDesc ripple{};
    ripple.sizeBytes = static_cast<u64>(renderer::MaximumWaterGridSegments) *
                       renderer::MaximumWaterGridSegments * sizeof(float);
    ripple.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    ripple.cpuAccess = rhi::CpuAccess::SequentialWrite;
    ripple.preferDeviceMemory = false;
    if (!memoryAllocator_->createBuffer(ripple, &waterRippleBuffer_)) return false;
    std::memset(waterRippleBuffer_.mappedData(), 0, static_cast<usize>(ripple.sizeBytes));
    if (!memoryAllocator_->flushBuffer(waterRippleBuffer_)) return false;
  }
  DirtRoadFrameUniform initialFrame{};
  initialFrame.environment = dirtRoadResources_.environmentLighting();
  if (initialFrame.environment.parameters[3] > 0.5f)
    initialFrame.environment.parameters[3] = renderingPolicy_.ambient.splitSumBrdf ? 3.0f : 1.0f;
  initialFrame.quality[0] = renderingPolicy_.materialDistance.normalMapMaximumDistance;
  initialFrame.quality[1] = renderingPolicy_.materialDistance.specularProbeMaximumDistance;
  initialFrame.quality[2] = renderingPolicy_.ambient.hemispheric ? 1.0f : 0.0f;
  initialFrame.quality[3] = renderingPolicy_.ambient.specularProbe ? 1.0f : 0.0f;
  initialFrame.materialDistanceParameters[0] =
      renderingPolicy_.materialDistance.metallicRoughnessMaximumDistance;
  initialFrame.materialDistanceParameters[1] =
      renderingPolicy_.materialDistance.emissiveMaximumDistance;
  initialFrame.materialDistanceParameters[2] =
      renderingPolicy_.materialDistance.fadeBandRatio;
  std::memcpy(environmentUniform_.mappedData(), &initialFrame, sizeof(initialFrame));
  if (!memoryAllocator_->flushBuffer(environmentUniform_)) return false;

  VkDescriptorSetLayoutBinding bindings[16]{};
  bindings[0].binding = 0;
  bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  bindings[0].descriptorCount = 1;
  bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  bindings[1].binding = 1;
  bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  bindings[1].descriptorCount = 1;
  bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  bindings[2].binding = 2;
  bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  bindings[2].descriptorCount = 1;
  bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  bindings[3] = bindings[1];
  bindings[3].binding = 3;
  bindings[4] = bindings[1];
  bindings[4].binding = 4;
  if (waterSubpassActive_) {
    bindings[5].binding = 5;
    bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    bindings[5].descriptorCount = 1;
    bindings[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[6] = bindings[1];
    bindings[6].binding = 6;
  }
  VkDescriptorSetLayoutCreateInfo layout{};
  layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  if(spectralWaterCount_>0) for(u32 i=7;i<11;++i)
    bindings[i]={i,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT,nullptr};
  if(spectralWaterCount_>0) for(u32 i=11;i<15;++i)
    bindings[i]={i,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr};
  u32 bindingCount = spectralWaterCount_>0 ? 15u : (waterSubpassActive_ ? 7u : 5u);
  if (waterSubpassActive_)
    bindings[bindingCount++]={15,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,
     VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,nullptr};
  layout.bindingCount = bindingCount;
  layout.pBindings = bindings;
  if (vkCreateDescriptorSetLayout(device_, &layout, nullptr, &environmentSetLayout_) != VK_SUCCESS) return false;
  VkDescriptorPoolSize sizes[4] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
                                   {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                    spectralWaterCount_>0 ? 9u : (waterSubpassActive_ ? 5u : 4u)},
                                   {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1},
                                   {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                    (spectralWaterCount_>0 ? 4u : 0u) + (waterSubpassActive_ ? 1u : 0u)}};
  VkDescriptorPoolCreateInfo pool{};
  pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool.maxSets = 1;
  pool.poolSizeCount = spectralWaterCount_>0 ? 4u : (waterSubpassActive_ ? 4u : 2u);
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
  VkDescriptorImageInfo shadow{
      shadowSampler_.isReady() ? shadowSampler_.handle() : dirtRoadResources_.environmentSampler(),
      shadowAtlas_.isReady() ? shadowAtlas_.view() : dirtRoadResources_.environmentView(),
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  VkDescriptorImageInfo specular{dirtRoadResources_.environmentSpecularSampler(),
                                 dirtRoadResources_.environmentSpecularView(),
                                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  VkDescriptorImageInfo brdf{dirtRoadResources_.environmentBrdfSampler(),
                             dirtRoadResources_.environmentBrdfView(),
                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  VkDescriptorImageInfo sceneDepth{VK_NULL_HANDLE, depthImage_.view(),
                                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
  VkDescriptorImageInfo waterNormal{waterDetailSampler_.handle(), waterDetailTexture_.view(),
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  if (waterSubpassActive_) {
    for (const auto &material : dirtRoadResources_.materials()) {
      if ((material.flags & renderer::MapMaterialWater) == 0 ||
          material.textureIndices[0] == renderer::InvalidMapTexture) continue;
      const u32 texture = material.textureIndices[0];
      waterNormal = {dirtRoadResources_.sampler(texture), dirtRoadResources_.view(texture),
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
      break;
    }
  }
  VkWriteDescriptorSet writes[16]{};
  VkDescriptorBufferInfo rippleBuffer{};
  VkDescriptorImageInfo spectralImages[4]{};
  VkDescriptorBufferInfo spectralBuffers[4]{};
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
  writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[2].dstSet = environmentSet_;
  writes[2].dstBinding = 2;
  writes[2].descriptorCount = 1;
  writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  writes[2].pImageInfo = &shadow;
  writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[3].dstSet = environmentSet_;
  writes[3].dstBinding = 3;
  writes[3].descriptorCount = 1;
  writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  writes[3].pImageInfo = &specular;
  writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[4].dstSet = environmentSet_;
  writes[4].dstBinding = 4;
  writes[4].descriptorCount = 1;
  writes[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  writes[4].pImageInfo = &brdf;
  if (waterSubpassActive_) {
    writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[5].dstSet = environmentSet_;
    writes[5].dstBinding = 5;
    writes[5].descriptorCount = 1;
    writes[5].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    writes[5].pImageInfo = &sceneDepth;
    writes[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[6].dstSet = environmentSet_;
    writes[6].dstBinding = 6;
    writes[6].descriptorCount = 1;
    writes[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[6].pImageInfo = &waterNormal;
  }
  if(spectralWaterCount_>0) for(u32 i=0;i<4;++i) {
    // Unused statically declared bindings share cascade zero; never read when
    // count excludes them, but Vulkan descriptors remain fully valid.
    const auto &buffer=(*waterSpectralCompute_)[i<spectralWaterCount_?i:0].output();
    spectralBuffers[i]={buffer.handle(),0,buffer.sizeBytes()};
    writes[7+i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[7+i].dstSet=environmentSet_; writes[7+i].dstBinding=7+i;
    writes[7+i].descriptorCount=1; writes[7+i].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[7+i].pBufferInfo=&spectralBuffers[i];
    const auto &cascade=(*waterSpectralCompute_)[i<spectralWaterCount_?i:0];
    spectralImages[i]={cascade.slopeSampler(),cascade.slopeView(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    writes[11+i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[11+i].dstSet=environmentSet_; writes[11+i].dstBinding=11+i;
    writes[11+i].descriptorCount=1; writes[11+i].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[11+i].pImageInfo=&spectralImages[i];
  }
  u32 writeCount = spectralWaterCount_>0 ? 15u : (waterSubpassActive_ ? 7u : 5u);
  if (waterSubpassActive_) {
    rippleBuffer = {waterRippleBuffer_.handle(), 0, waterRippleBuffer_.sizeBytes()};
    writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[writeCount].dstSet = environmentSet_;
    writes[writeCount].dstBinding = 15;
    writes[writeCount].descriptorCount = 1;
    writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[writeCount].pBufferInfo = &rippleBuffer;
    ++writeCount;
  }
  vkUpdateDescriptorSets(device_, writeCount, writes, 0, nullptr);
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

bool InstancedRenderer::createEditorGridPipeline() {
  if (!dirtRoadPreview_) return true;
  VkShaderModule vert = createShaderModule(device_, rhi::shaders::kEditor_GridVertSpirv,
                                           rhi::shaders::kEditor_GridVertSpirvSize);
  VkShaderModule frag = createShaderModule(device_, rhi::shaders::kEditor_GridFragSpirv,
                                           rhi::shaders::kEditor_GridFragSpirvSize);
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
  // Testa contra a cena e NÃO escreve: uma caixa opaca esconde as linhas atrás
  // dela, e a grade não passa a ocluir o que vier depois. O fragmento escreve
  // `gl_FragDepth` a partir da interseção, então a comparação é a mesma que
  // ordena a geometria.
  depth.depthTestEnable = VK_TRUE; depth.depthWriteEnable = VK_FALSE;
  // `LESS` e não `LESS_OR_EQUAL`: num empate a geometria ganha. Com o alcance
  // de profundidade que a câmera editorial usa — de centímetros a quilômetros —
  // o chão atrás de um objeto e a face desse objeto caem no mesmo valor
  // quantizado com frequência, e "menor ou igual" deixava a grade atravessar
  // justamente as faces mais próximas.
  depth.depthCompareOp = VK_COMPARE_OP_LESS;
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
                           0, sizeof(DirtRoadPushConstants)};
  VkPipelineLayoutCreateInfo layout{};
  layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layout.setLayoutCount = 1; layout.pSetLayouts = &environmentSetLayout_;
  layout.pushConstantRangeCount = 1; layout.pPushConstantRanges = &push;
  bool ok = vkCreatePipelineLayout(device_, &layout, nullptr, &editorGridPipelineLayout_) == VK_SUCCESS;
  if (ok) {
    VkGraphicsPipelineCreateInfo pipeline{};
    pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline.stageCount = 2; pipeline.pStages = stages; pipeline.pVertexInputState = &vertex;
    pipeline.pInputAssemblyState = &assembly; pipeline.pViewportState = &viewport;
    pipeline.pRasterizationState = &raster; pipeline.pMultisampleState = &multisample;
    pipeline.pDepthStencilState = &depth; pipeline.pColorBlendState = &blend;
    pipeline.pDynamicState = &dynamic; pipeline.layout = editorGridPipelineLayout_;
    pipeline.renderPass = renderPass_; pipeline.subpass = 0;
    ok = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline, nullptr, &editorGridPipeline_) == VK_SUCCESS;
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
    pipeline.renderPass = renderPass_; pipeline.subpass = waterSubpassActive_ ? 1u : 0u;
    ok = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline, nullptr,
                                   &runtimeHudPipeline_) == VK_SUCCESS;
  }
  vkDestroyShaderModule(device_, vert, nullptr);
  vkDestroyShaderModule(device_, frag, nullptr);
  return ok;
}

void InstancedRenderer::createUiRenderer(AAssetManager *assets) {
  if (assets == nullptr) return;
  if (!platform::android::readAndroidAsset(assets, "ui/astra-ui-font.aeuf", uiFontBytes_) ||
      !platform::android::readAndroidAsset(assets, "ui/astra-ui-icons.aeui", uiIconBytes_)) {
    __android_log_print(ANDROID_LOG_WARN, LogTag,
        "[UI] assets ausentes no APK; o editor roda sem interface.");
    return;
  }
  if (!uiFont_.load(uiFontBytes_) || !uiIcons_.load(uiIconBytes_)) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag,
        "[UI] atlas recusado pelo leitor: fonte=%s icones=%s.",
        uiFont_.isReady() ? "ok" : "nao", uiIcons_.isReady() ? "ok" : "nao");
    return;
  }
  // O upload acontece uma vez, no mesmo contexto sincrono que as texturas do
  // mapa usam; ele nao pertence ao laco de frame.
  rhi::VulkanUploadContext upload;
  if (!upload.initialize(device_, rhiDevice_->graphicsQueueFamily())) return;
  VkAttachmentDescription attachment{};attachment.format=swapchain_->imageFormat();attachment.samples=VK_SAMPLE_COUNT_1_BIT;
  attachment.loadOp=VK_ATTACHMENT_LOAD_OP_LOAD;attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
  attachment.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachment.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachment.initialLayout=attachment.finalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  VkAttachmentReference color{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};VkSubpassDescription subpass{};
  subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;subpass.colorAttachmentCount=1;subpass.pColorAttachments=&color;
  VkSubpassDependency dependency{};dependency.srcSubpass=VK_SUBPASS_EXTERNAL;dependency.dstSubpass=0;
  dependency.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT;
  dependency.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_TRANSFER_READ_BIT;
  dependency.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  VkRenderPassCreateInfo pass{};pass.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;pass.attachmentCount=1;pass.pAttachments=&attachment;
  pass.subpassCount=1;pass.pSubpasses=&subpass;pass.dependencyCount=1;pass.pDependencies=&dependency;
  if(vkCreateRenderPass(device_,&pass,nullptr,&uiRenderPass_)!=VK_SUCCESS) {upload.shutdown();return;}
  uiFramebuffers_.resize(swapchain_->imageCount(),VK_NULL_HANDLE);
  for(u32 i=0;i<uiFramebuffers_.size();++i) {
    const auto view=swapchain_->imageView(i);VkFramebufferCreateInfo fb{};fb.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb.renderPass=uiRenderPass_;fb.attachmentCount=1;fb.pAttachments=&view;fb.width=swapchain_->width();fb.height=swapchain_->height();fb.layers=1;
    if(vkCreateFramebuffer(device_,&fb,nullptr,&uiFramebuffers_[i])!=VK_SUCCESS) {upload.shutdown();return;}
  }
  if (!uiRenderer_.initialize(device_, *memoryAllocator_, upload, uiRenderPass_, 0,
                              rhiDevice_->pipelineCache().driverHandle(), uiFont_, uiIcons_,
                              kUiInstanceCapacity)) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "[UI] pipeline recusada; sem interface.");
    upload.shutdown();
    return;
  }
  upload.shutdown();
  __android_log_print(ANDROID_LOG_INFO, LogTag,
      "[UI] pronta: fonte %ux%u icones %ux%u capacidade=%u.",
      uiFont_.atlasWidth(), uiFont_.atlasHeight(), uiIcons_.width(), uiIcons_.height(),
      uiRenderer_.capacity());
}

void InstancedRenderer::recordUiOverlay(u32 imageIndex) {
  if(!uiRenderer_.isReady() || uiInstances_.empty() || imageIndex>=uiFramebuffers_.size()) return;
  VkRenderPassBeginInfo begin{};begin.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;begin.renderPass=uiRenderPass_;
  begin.framebuffer=uiFramebuffers_[imageIndex];begin.renderArea.extent={swapchain_->width(),swapchain_->height()};
  vkCmdBeginRenderPass(commandBuffer_,&begin,VK_SUBPASS_CONTENTS_INLINE);
  const VkViewport viewport{0,0,float(swapchain_->width()),float(swapchain_->height()),0,1};
  const VkRect2D scissor{{0,0},{swapchain_->width(),swapchain_->height()}};
  vkCmdSetViewport(commandBuffer_,0,1,&viewport);vkCmdSetScissor(commandBuffer_,0,1,&scissor);
  const auto display=swapchain_->displayExtent();
  const bool srgb=swapchain_->imageFormat()==VK_FORMAT_B8G8R8A8_SRGB || swapchain_->imageFormat()==VK_FORMAT_R8G8B8A8_SRGB;
  uiRenderer_.record(commandBuffer_,uiInstances_,uiSurfaceWidth_>0?uiSurfaceWidth_:float(display.width),
                     uiSurfaceHeight_>0?uiSurfaceHeight_:float(display.height),swapchain_->surfaceTransform(),srgb);
  vkCmdEndRenderPass(commandBuffer_);
}

float InstancedRenderer::sceneAspectRatio() const {
  const auto extent = swapchain_->displayExtent();
  const float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
  return sceneViewport_.isEmpty() ? aspect : aspect * sceneViewport_.width / sceneViewport_.height;
}

void InstancedRenderer::setUiInstances(std::span<const ui::UiInstance> instances) {
  uiInstances_.assign(instances.begin(), instances.end());
}

void InstancedRenderer::setUiSurfaceSize(float width, float height) {
  uiSurfaceWidth_ = width > 0.0f ? width : 0.0f;
  uiSurfaceHeight_ = height > 0.0f ? height : 0.0f;
}

bool InstancedRenderer::createPostResources() {
  if (!renderingPolicy_.post.dedicatedPass) return true;

  rhi::ImageDesc image{};
  image.width = renderTargetWidth();
  image.height = renderTargetHeight();
  image.format = swapchain_->imageFormat();
  image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  image.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  image.memoryClass = rhi::MemoryClass::RenderTarget;
  if (!memoryAllocator_->createImage(image, &postSceneColor_)) return false;

  if (temporalAaActive_) {
    rhi::ImageDesc history{};
    history.width = swapchain_->width();
    history.height = swapchain_->height();
    history.format = swapchain_->imageFormat();
    history.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    history.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    history.memoryClass = rhi::MemoryClass::RenderTarget;
    if (!memoryAllocator_->createImage(history, &postHistory_)) {
      temporalAaActive_ = false;
      renderingPolicy_.post.antiAliasing = renderer::AntiAliasingMode::Fxaa;
      resourceRenderingPolicy_.post.antiAliasing = renderer::AntiAliasingMode::Fxaa;
      __android_log_print(ANDROID_LOG_WARN, LogTag,
                          "[TAA] histórico não pôde ser alocado; fallback FXAA ativo.");
    }
  }

  rhi::SamplerDesc sampler{};
  sampler.addressU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler.addressV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler.addressW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  if (!postSampler_.initialize(device_, sampler)) return false;
  if (temporalAaActive_) {
    rhi::SamplerDesc depthSampler = sampler;
    depthSampler.minFilter = VK_FILTER_NEAREST;
    depthSampler.magFilter = VK_FILTER_NEAREST;
    if (!postDepthSampler_.initialize(device_, depthSampler)) {
      temporalAaActive_ = false;
      postHistory_.reset();
      renderingPolicy_.post.antiAliasing = renderer::AntiAliasingMode::Fxaa;
      resourceRenderingPolicy_.post.antiAliasing = renderer::AntiAliasingMode::Fxaa;
      __android_log_print(ANDROID_LOG_WARN, LogTag,
                          "[TAA] sampler de depth indisponível; fallback FXAA ativo.");
    }
  }

  VkAttachmentDescription attachment{};
  attachment.format = swapchain_->imageFormat();
  attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  VkAttachmentReference color{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color;
  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            (temporalAaActive_
                                 ? static_cast<VkPipelineStageFlags>(
                                       VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT)
                                 : VkPipelineStageFlags{0});
  dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                             (temporalAaActive_
                                  ? static_cast<VkAccessFlags>(
                                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
                                  : VkAccessFlags{0});
  dependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  VkRenderPassCreateInfo renderPass{};
  renderPass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPass.attachmentCount = 1;
  renderPass.pAttachments = &attachment;
  renderPass.subpassCount = 1;
  renderPass.pSubpasses = &subpass;
  renderPass.dependencyCount = 1;
  renderPass.pDependencies = &dependency;
  if (vkCreateRenderPass(device_, &renderPass, nullptr, &postRenderPass_) != VK_SUCCESS) return false;

  VkDescriptorSetLayoutBinding bindings[3]{};
  const u32 bindingCount = temporalAaActive_ ? 3u : 1u;
  for (u32 index = 0; index < bindingCount; ++index) {
    bindings[index].binding = index;
    bindings[index].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[index].descriptorCount = 1;
    bindings[index].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  }
  VkDescriptorSetLayoutCreateInfo setLayout{};
  setLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  setLayout.bindingCount = bindingCount;
  setLayout.pBindings = bindings;
  if (vkCreateDescriptorSetLayout(device_, &setLayout, nullptr, &postSetLayout_) != VK_SUCCESS)
    return false;
  VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, bindingCount};
  VkDescriptorPoolCreateInfo pool{};
  pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool.maxSets = 1;
  pool.poolSizeCount = 1;
  pool.pPoolSizes = &poolSize;
  if (vkCreateDescriptorPool(device_, &pool, nullptr, &postDescriptorPool_) != VK_SUCCESS)
    return false;
  VkDescriptorSetAllocateInfo allocate{};
  allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocate.descriptorPool = postDescriptorPool_;
  allocate.descriptorSetCount = 1;
  allocate.pSetLayouts = &postSetLayout_;
  if (vkAllocateDescriptorSets(device_, &allocate, &postDescriptorSet_) != VK_SUCCESS) return false;
  VkDescriptorImageInfo images[3]{};
  images[0] = {postSampler_.handle(), postSceneColor_.view(),
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  if (temporalAaActive_) {
    images[1] = {postSampler_.handle(), postHistory_.view(),
                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    images[2] = {postDepthSampler_.handle(), depthImage_.view(),
                 VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
  }
  VkWriteDescriptorSet writes[3]{};
  for (u32 index = 0; index < bindingCount; ++index) {
    writes[index].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[index].dstSet = postDescriptorSet_;
    writes[index].dstBinding = index;
    writes[index].descriptorCount = 1;
    writes[index].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[index].pImageInfo = &images[index];
  }
  vkUpdateDescriptorSets(device_, bindingCount, writes, 0, nullptr);

  VkShaderModule vert = createShaderModule(device_, rhi::shaders::kPost_ProcessVertSpirv,
                                            rhi::shaders::kPost_ProcessVertSpirvSize);
  VkShaderModule frag = temporalAaActive_
      ? createShaderModule(device_, rhi::shaders::kPost_Process_TemporalFragSpirv,
                           rhi::shaders::kPost_Process_TemporalFragSpirvSize)
      : createShaderModule(device_, rhi::shaders::kPost_ProcessFragSpirv,
                           rhi::shaders::kPost_ProcessFragSpirvSize);
  if (vert == VK_NULL_HANDLE || frag == VK_NULL_HANDLE) {
    if (vert != VK_NULL_HANDLE) vkDestroyShaderModule(device_, vert, nullptr);
    if (frag != VK_NULL_HANDLE) vkDestroyShaderModule(device_, frag, nullptr);
    return false;
  }
  VkPipelineShaderStageCreateInfo stages[2] = {
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
       VK_SHADER_STAGE_VERTEX_BIT, vert, "main", nullptr},
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
       VK_SHADER_STAGE_FRAGMENT_BIT, frag, "main", nullptr}};
  VkPushConstantRange push{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PostPushConstants)};
  VkPipelineLayoutCreateInfo pipelineLayout{};
  pipelineLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayout.setLayoutCount = 1;
  pipelineLayout.pSetLayouts = &postSetLayout_;
  pipelineLayout.pushConstantRangeCount = 1;
  pipelineLayout.pPushConstantRanges = &push;
  bool ok = vkCreatePipelineLayout(device_, &pipelineLayout, nullptr, &postPipelineLayout_) == VK_SUCCESS;

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
  raster.polygonMode = VK_POLYGON_MODE_FILL;
  raster.cullMode = VK_CULL_MODE_NONE;
  raster.lineWidth = 1.0f;
  VkPipelineMultisampleStateCreateInfo multisample{};
  multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  VkPipelineDepthStencilStateCreateInfo depth{};
  depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  VkPipelineColorBlendStateCreateInfo colorBlend{};
  colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlend.attachmentCount = 1;
  colorBlend.pAttachments = &colorBlendAttachment;
  const VkDynamicState dynamicStates[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic{};
  dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic.dynamicStateCount = 2;
  dynamic.pDynamicStates = dynamicStates;
  if (ok) {
    VkGraphicsPipelineCreateInfo pipeline{};
    pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline.stageCount = 2; pipeline.pStages = stages;
    pipeline.pVertexInputState = &vertex; pipeline.pInputAssemblyState = &assembly;
    pipeline.pViewportState = &viewport; pipeline.pRasterizationState = &raster;
    pipeline.pMultisampleState = &multisample; pipeline.pDepthStencilState = &depth;
    pipeline.pColorBlendState = &colorBlend; pipeline.pDynamicState = &dynamic;
    pipeline.layout = postPipelineLayout_; pipeline.renderPass = postRenderPass_;
    ok = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline, nullptr,
                                   &postPipeline_) == VK_SUCCESS;
  }
  vkDestroyShaderModule(device_, vert, nullptr);
  vkDestroyShaderModule(device_, frag, nullptr);
  return ok;
}

void InstancedRenderer::recordPostProcess(u32 imageIndex,
                                           const platform::FreeCameraState &camera) {
  if (!renderingPolicy_.post.dedicatedPass || postPipeline_ == VK_NULL_HANDLE) return;
  if (temporalAaActive_ && !temporalHistoryLayoutInitialized_) {
    // The temporal descriptor is statically referenced by the shader. Dynamic
    // control flow that rejects an invalid first-frame history does not make an
    // UNDEFINED descriptor layout legal, so establish the sampled layout once
    // before the first post draw without pretending its pixels are valid.
    VkImageMemoryBarrier historyReady{};
    historyReady.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    historyReady.srcAccessMask = 0;
    historyReady.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    historyReady.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    historyReady.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    historyReady.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    historyReady.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    historyReady.image = postHistory_.handle();
    historyReady.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(commandBuffer_, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &historyReady);
    temporalHistoryLayoutInitialized_ = true;
  }
  VkRenderPassBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  begin.renderPass = postRenderPass_;
  begin.framebuffer = postFramebuffers_[imageIndex];
  begin.renderArea.extent = {swapchain_->width(), swapchain_->height()};
  vkCmdBeginRenderPass(commandBuffer_, &begin, VK_SUBPASS_CONTENTS_INLINE);
  VkViewport viewport{0.0f, 0.0f, static_cast<float>(swapchain_->width()),
                      static_cast<float>(swapchain_->height()), 0.0f, 1.0f};
  VkRect2D scissor{{0, 0}, {swapchain_->width(), swapchain_->height()}};
  vkCmdSetViewport(commandBuffer_, 0, 1, &viewport);
  vkCmdSetScissor(commandBuffer_, 0, 1, &scissor);
  vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, postPipeline_);
  vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, postPipelineLayout_,
                          0, 1, &postDescriptorSet_, 0, nullptr);
  PostPushConstants push{};
  push.texelFlags[0] = 1.0f / static_cast<float>(renderTargetWidth());
  push.texelFlags[1] = 1.0f / static_cast<float>(renderTargetHeight());
  push.texelFlags[2] = renderingPolicy_.post.bloom ? 1.0f : 0.0f;
  if (temporalAaActive_ && temporalHistoryInitialized_ &&
      temporalCameraCut(camera, temporalPreviousCamera_)) {
    temporalHistoryInitialized_ = false;
  }
  push.texelFlags[3] = temporalAaActive_
                           ? (temporalHistoryInitialized_ ? 3.0f : 2.0f)
                           : renderingPolicy_.post.antiAliasing ==
                                     renderer::AntiAliasingMode::Fxaa ? 1.0f : 0.0f;
  push.bloom[0] = renderingPolicy_.post.bloomThreshold;
  push.bloom[1] = renderingPolicy_.post.bloomIntensity;
  push.bloom[2] = renderingPolicy_.post.sharpen;
  push.bloom[3] = renderingPolicy_.post.vignette ? renderingPolicy_.post.vignetteIntensity : 0.0f;
  push.grade[0] = renderingPolicy_.post.contrast;
  push.grade[1] = renderingPolicy_.post.saturation;
  push.grade[2] = (swapchain_->imageFormat() == VK_FORMAT_B8G8R8A8_SRGB ||
                   swapchain_->imageFormat() == VK_FORMAT_R8G8B8A8_SRGB) ? 0.0f : 1.0f;
  const rhi::SurfaceTransform &surfaceTransform = swapchain_->surfaceTransform();
  push.grade[3] = static_cast<float>(packSurfaceTransform(surfaceTransform)) +
                  renderingPolicy_.post.temporalHistoryWeight;
  push.sourceTransform[0] = static_cast<float>(renderWidth()) /
                            static_cast<float>(renderTargetWidth());
  push.sourceTransform[1] = static_cast<float>(renderHeight()) /
                            static_cast<float>(renderTargetHeight());
  push.sourceTransform[2] = dirtRoadPreview_?1.0f/std::tan(sceneFieldOfView()*.5f):1.732050808f;
  const VkExtent2D displayExtent = swapchain_->displayExtent();
  push.sourceTransform[3] = static_cast<float>(displayExtent.width) /
                            static_cast<float>(displayExtent.height);
  push.currentCamera[0] = camera.yaw;
  push.currentCamera[1] = camera.pitch;
  push.currentCamera[2] = temporalCurrentJitter_[0];
  push.currentCamera[3] = temporalCurrentJitter_[1];
  std::memcpy(push.currentPositionNear, camera.position, sizeof(camera.position));
  push.currentPositionNear[3] = dirtRoadPreview_ ? sceneNearPlane() : 0.1f;
  const platform::FreeCameraState &previous = temporalHistoryInitialized_
                                                   ? temporalPreviousCamera_ : camera;
  push.previousCamera[0] = previous.yaw;
  push.previousCamera[1] = previous.pitch;
  push.previousCamera[2] = temporalHistoryInitialized_ ? temporalPreviousJitter_[0]
                                                       : temporalCurrentJitter_[0];
  push.previousCamera[3] = temporalHistoryInitialized_ ? temporalPreviousJitter_[1]
                                                       : temporalCurrentJitter_[1];
  std::memcpy(push.previousPositionFar, previous.position, sizeof(previous.position));
  push.previousPositionFar[3] = dirtRoadPreview_ ? sceneFarPlane() : 1000.0f;
  vkCmdPushConstants(commandBuffer_, postPipelineLayout_, VK_SHADER_STAGE_FRAGMENT_BIT,
                     0, sizeof(push), &push);
  vkCmdDraw(commandBuffer_, 3, 1, 0, 0);
  vkCmdEndRenderPass(commandBuffer_);
  if (temporalAaActive_) {
    if (recordTemporalHistoryCopy(imageIndex)) {
      temporalPreviousCamera_ = camera;
      temporalPreviousJitter_[0] = temporalCurrentJitter_[0];
      temporalPreviousJitter_[1] = temporalCurrentJitter_[1];
      temporalHistoryInitialized_ = true;
      ++temporalFrameIndex_;
    } else {
      temporalHistoryInitialized_ = false;
    }
  }
}

bool InstancedRenderer::recordTemporalHistoryCopy(u32 imageIndex) {
  if (!temporalAaActive_ || !postHistory_.isReady() ||
      swapchain_->image(imageIndex) == VK_NULL_HANDLE ||
      !temporalHistoryLayoutInitialized_) return false;

  VkImageMemoryBarrier toTransfer[2]{};
  toTransfer[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  toTransfer[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  toTransfer[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  toTransfer[0].oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  toTransfer[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  toTransfer[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  toTransfer[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  toTransfer[0].image = swapchain_->image(imageIndex);
  toTransfer[0].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  toTransfer[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  toTransfer[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  toTransfer[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  toTransfer[1].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  toTransfer[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  toTransfer[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  toTransfer[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  toTransfer[1].image = postHistory_.handle();
  toTransfer[1].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  vkCmdPipelineBarrier(commandBuffer_,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, toTransfer);

  VkImageCopy copy{};
  copy.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  copy.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  copy.extent = {swapchain_->width(), swapchain_->height(), 1};
  vkCmdCopyImage(commandBuffer_, swapchain_->image(imageIndex),
                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, postHistory_.handle(),
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

  VkImageMemoryBarrier fromTransfer[2]{};
  fromTransfer[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  fromTransfer[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  fromTransfer[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  fromTransfer[0].newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  fromTransfer[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  fromTransfer[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  fromTransfer[0].image = swapchain_->image(imageIndex);
  fromTransfer[0].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  fromTransfer[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  fromTransfer[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  fromTransfer[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  fromTransfer[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  fromTransfer[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  fromTransfer[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  fromTransfer[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  fromTransfer[1].image = postHistory_.handle();
  fromTransfer[1].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  vkCmdPipelineBarrier(commandBuffer_, VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
      0, 0, nullptr, 0, nullptr, 2, fromTransfer);
  return true;
}

void InstancedRenderer::destroyPostResources() {
  if (postPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, postPipeline_, nullptr);
  if (postPipelineLayout_ != VK_NULL_HANDLE)
    vkDestroyPipelineLayout(device_, postPipelineLayout_, nullptr);
  if (postDescriptorPool_ != VK_NULL_HANDLE)
    vkDestroyDescriptorPool(device_, postDescriptorPool_, nullptr);
  if (postSetLayout_ != VK_NULL_HANDLE)
    vkDestroyDescriptorSetLayout(device_, postSetLayout_, nullptr);
  if (postRenderPass_ != VK_NULL_HANDLE) vkDestroyRenderPass(device_, postRenderPass_, nullptr);
  postPipeline_ = VK_NULL_HANDLE;
  postPipelineLayout_ = VK_NULL_HANDLE;
  postDescriptorPool_ = VK_NULL_HANDLE;
  postDescriptorSet_ = VK_NULL_HANDLE;
  postSetLayout_ = VK_NULL_HANDLE;
  postRenderPass_ = VK_NULL_HANDLE;
  postDepthSampler_.shutdown();
  postSampler_.shutdown();
  postHistory_.reset();
  postSceneColor_.reset();
  temporalHistoryLayoutInitialized_ = false;
  temporalHistoryInitialized_ = false;
  temporalAaActive_ = false;
  temporalFrameIndex_ = 0;
  temporalCurrentJitter_[0] = temporalCurrentJitter_[1] = 0.0f;
  temporalPreviousJitter_[0] = temporalPreviousJitter_[1] = 0.0f;
}

bool InstancedRenderer::createShadowResources() {
  if (!dirtRoadPreview_) return true;
  shadowDepthFormat_ = chooseDepthFormat(physicalDevice_, true);
  if (shadowDepthFormat_ == VK_FORMAT_UNDEFINED) {
    renderingPolicy_.shadows = {};
    __android_log_print(ANDROID_LOG_WARN, LogTag,
                        "[Shadow] depth amostrável indisponível; sol mantém N.L sem oclusão.");
    return true;
  }
  const u32 grid = renderingPolicy_.shadows.enabled &&
                           renderingPolicy_.shadows.cascadeCount > 1 ? 2u : 1u;
  const u32 resolution = renderingPolicy_.shadows.enabled
                             ? renderingPolicy_.shadows.cascadeResolution : 1u;
  rhi::ImageDesc image{};
  image.width = resolution * grid;
  image.height = resolution * grid;
  image.format = shadowDepthFormat_;
  image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  image.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  image.memoryClass = rhi::MemoryClass::RenderTarget;
  if (!memoryAllocator_->createImage(image, &shadowAtlas_)) return false;
  rhi::SamplerDesc sampler{};
  // PCF por hardware: o Adreno resolve compare + bilinear 2x2 numa busca so.
  // Exige filtro LINEAR e o feature bit correspondente para o formato de
  // profundidade escolhido; sem ele, NEAREST ainda entrega o compare em
  // hardware (uma amostra por busca, sem o filtro), que continua sendo melhor
  // que comparar a mao no shader.
  VkFormatProperties shadowFormatProperties{};
  vkGetPhysicalDeviceFormatProperties(physicalDevice_, shadowDepthFormat_,
                                      &shadowFormatProperties);
  const bool linearShadowFilter =
      (shadowFormatProperties.optimalTilingFeatures &
       VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0;
  const VkFilter shadowFilter = linearShadowFilter ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
  sampler.minFilter = shadowFilter;
  sampler.magFilter = shadowFilter;
  sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  sampler.addressU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler.addressV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler.addressW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler.enableCompare = true;
  // O shader comparava `projected.z <= stored`; LESS_OR_EQUAL preserva
  // exatamente essa semantica, inclusive na igualdade.
  sampler.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  if (!shadowSampler_.initialize(device_, sampler)) return false;
  __android_log_print(ANDROID_LOG_INFO, LogTag,
      "[Shadow] PCF de hardware ativo: filtro=%s compare=LESS_OR_EQUAL.",
      linearShadowFilter ? "linear" : "nearest");

  VkAttachmentReference depthReference{0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.pDepthStencilAttachment = &depthReference;
  const VkImageView atlasView = shadowAtlas_.view();
  const auto createPassAndFramebuffer = [&](bool preserve, VkRenderPass &outPass,
                                             VkFramebuffer &outFramebuffer) {
    VkAttachmentDescription attachment{};
    attachment.format = shadowDepthFormat_;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = preserve ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = preserve ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                        : VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkSubpassDependency dependencies[2]{};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = preserve ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                                            : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = preserve ? VK_ACCESS_SHADER_READ_BIT : 0;
    dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    VkRenderPassCreateInfo pass{};
    pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    pass.attachmentCount = 1; pass.pAttachments = &attachment;
    pass.subpassCount = 1; pass.pSubpasses = &subpass;
    pass.dependencyCount = 2; pass.pDependencies = dependencies;
    if (vkCreateRenderPass(device_, &pass, nullptr, &outPass) != VK_SUCCESS) return false;
    VkFramebufferCreateInfo framebuffer{};
    framebuffer.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer.renderPass = outPass;
    framebuffer.attachmentCount = 1;
    framebuffer.pAttachments = &atlasView;
    framebuffer.width = image.width; framebuffer.height = image.height; framebuffer.layers = 1;
    return vkCreateFramebuffer(device_, &framebuffer, nullptr, &outFramebuffer) == VK_SUCCESS;
  };
  if (!createPassAndFramebuffer(false, shadowRenderPass_, shadowFramebuffer_) ||
      !createPassAndFramebuffer(true, shadowCachedRenderPass_, shadowCachedFramebuffer_))
    return false;
  invalidateStaticShadowCache();

  // Mesmo quando a sombra está desligada, o atlas 1x1 acima é limpo uma vez por
  // frame e mantém o descriptor sempre válido. Pipelines de caster só existem
  // quando há trabalho de fato.
  if (!renderingPolicy_.shadows.enabled) return true;
  VkShaderModule vert = createShaderModule(device_, rhi::shaders::kShadow_DepthVertSpirv,
                                            rhi::shaders::kShadow_DepthVertSpirvSize);
  VkShaderModule masked = useBindless_
      ? createShaderModule(device_, rhi::shaders::kShadow_Depth_MaskedFragSpirv,
                           rhi::shaders::kShadow_Depth_MaskedFragSpirvSize)
      : createShaderModule(device_, rhi::shaders::kShadow_Depth_Masked_FallbackFragSpirv,
                           rhi::shaders::kShadow_Depth_Masked_FallbackFragSpirvSize);
  if (vert == VK_NULL_HANDLE || masked == VK_NULL_HANDLE) {
    if (vert != VK_NULL_HANDLE) vkDestroyShaderModule(device_, vert, nullptr);
    if (masked != VK_NULL_HANDLE) vkDestroyShaderModule(device_, masked, nullptr);
    return false;
  }
  VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(ShadowPushConstants)};
  VkPipelineLayoutCreateInfo layout{};
  layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layout.setLayoutCount = 1;
  layout.pSetLayouts = &textureSetLayout_;
  layout.pushConstantRangeCount = 1;
  layout.pPushConstantRanges = &push;
  bool ok = vkCreatePipelineLayout(device_, &layout, nullptr, &shadowPipelineLayout_) == VK_SUCCESS;

  VkVertexInputBindingDescription bindings[2] = {
      {0, dirtRoadResources_.header().vertexStride, VK_VERTEX_INPUT_RATE_VERTEX},
      {1, sizeof(renderer::GpuMeshInstance), VK_VERTEX_INPUT_RATE_INSTANCE}};
  VkVertexInputAttributeDescription attributes[14]{};
  const bool packed = dirtRoadResources_.header().vertexStride == renderer::MapVertexStride;
  attributes[0]={0,0,VK_FORMAT_R32G32B32_SFLOAT,0};
  attributes[1]={1,0,packed?VK_FORMAT_R16G16B16A16_SNORM:VK_FORMAT_R32G32B32_SFLOAT,12};
  attributes[2]={2,0,packed?VK_FORMAT_R16G16B16A16_SNORM:VK_FORMAT_R32G32B32A32_SFLOAT,packed?20u:24u};
  attributes[3]={3,0,VK_FORMAT_R32G32_SFLOAT,packed?28u:40u};
  attributes[4]={4,0,VK_FORMAT_R32G32_SFLOAT,packed?36u:48u};
  attributes[5]={5,0,packed?VK_FORMAT_R8G8B8A8_UNORM:VK_FORMAT_R32G32B32A32_SFLOAT,packed?44u:56u};
  for(u32 i=0;i<5;++i) attributes[6+i]={6+i,1,VK_FORMAT_R32G32B32A32_SFLOAT,i*16u};
  for(u32 i=0;i<3;++i) attributes[11+i]={11+i,1,VK_FORMAT_R32G32B32A32_SFLOAT,80u+i*16u};
  VkPipelineVertexInputStateCreateInfo vertexInput{};
  vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInput.vertexBindingDescriptionCount = 2; vertexInput.pVertexBindingDescriptions = bindings;
  vertexInput.vertexAttributeDescriptionCount = 14; vertexInput.pVertexAttributeDescriptions = attributes;
  VkPipelineInputAssemblyStateCreateInfo assembly{};
  assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  VkPipelineViewportStateCreateInfo viewport{};
  viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport.viewportCount = 1; viewport.scissorCount = 1;
  VkPipelineRasterizationStateCreateInfo raster{};
  raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  raster.polygonMode = VK_POLYGON_MODE_FILL; raster.cullMode = VK_CULL_MODE_NONE;
  raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; raster.lineWidth = 1.0f;
  raster.depthBiasEnable = VK_TRUE;
  VkPipelineMultisampleStateCreateInfo multisample{};
  multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  VkPipelineDepthStencilStateCreateInfo depth{};
  depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth.depthTestEnable = VK_TRUE; depth.depthWriteEnable = VK_TRUE;
  depth.depthCompareOp = VK_COMPARE_OP_LESS;
  const VkDynamicState states[3] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
                                    VK_DYNAMIC_STATE_DEPTH_BIAS};
  VkPipelineDynamicStateCreateInfo dynamic{};
  dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic.dynamicStateCount = 3; dynamic.pDynamicStates = states;
  VkPipelineColorBlendStateCreateInfo blend{};
  blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  VkPipelineShaderStageCreateInfo stages[2] = {
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_VERTEX_BIT,
       vert,"main",nullptr},
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_FRAGMENT_BIT,
       masked,"main",nullptr}};
  if (ok) {
    VkGraphicsPipelineCreateInfo pipeline{};
    pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline.stageCount = 1; pipeline.pStages = stages;
    pipeline.pVertexInputState=&vertexInput; pipeline.pInputAssemblyState=&assembly;
    pipeline.pViewportState=&viewport; pipeline.pRasterizationState=&raster;
    pipeline.pMultisampleState=&multisample; pipeline.pDepthStencilState=&depth;
    pipeline.pColorBlendState=&blend; pipeline.pDynamicState=&dynamic;
    pipeline.layout=shadowPipelineLayout_; pipeline.renderPass=shadowRenderPass_;
    ok = vkCreateGraphicsPipelines(device_,VK_NULL_HANDLE,1,&pipeline,nullptr,
                                   &shadowOpaquePipeline_)==VK_SUCCESS;
    pipeline.stageCount = 2;
    if (ok) ok = vkCreateGraphicsPipelines(device_,VK_NULL_HANDLE,1,&pipeline,nullptr,
                                           &shadowMaskedPipeline_)==VK_SUCCESS;
  }
  vkDestroyShaderModule(device_, vert, nullptr);
  vkDestroyShaderModule(device_, masked, nullptr);
  return ok;
}

void InstancedRenderer::recordShadowPass(const platform::FreeCameraState &) {
  shadowCandidateDraws_ = 0;
  shadowSubmittedDraws_ = 0;
  shadowRenderedCascades_ = 0;
  if (!dirtRoadPreview_ || shadowRenderPass_ == VK_NULL_HANDLE) return;
  const u32 activeMask = shadowCascadeCount_ == 0 ? 0u : (1u << shadowCascadeCount_) - 1u;
  const bool initializeAtlas = !shadowCacheInitialized_;
  const u32 dirtyMask = initializeAtlas ? activeMask : (shadowCascadeDirtyMask_ & activeMask);
  if (!initializeAtlas && dirtyMask == 0) {
    ++shadowCacheHitFrames_;
    return;
  }
  VkClearValue clear{}; clear.depthStencil = {1.0f, 0};
  VkRenderPassBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  begin.renderPass = initializeAtlas ? shadowRenderPass_ : shadowCachedRenderPass_;
  begin.framebuffer = initializeAtlas ? shadowFramebuffer_ : shadowCachedFramebuffer_;
  begin.renderArea.extent = {shadowAtlas_.width(), shadowAtlas_.height()};
  begin.clearValueCount = initializeAtlas ? 1u : 0u;
  begin.pClearValues = initializeAtlas ? &clear : nullptr;
  vkCmdBeginRenderPass(commandBuffer_, &begin, VK_SUBPASS_CONTENTS_INLINE);
  if (shadowCascadeCount_ == 0 || shadowOpaquePipeline_ == VK_NULL_HANDLE) {
    vkCmdEndRenderPass(commandBuffer_);
    shadowCacheInitialized_ = true;
    shadowCascadeDirtyMask_ = 0;
    return;
  }
  VkDeviceSize offset = 0;
  const VkBuffer mesh = dirtRoadResources_.vertexBuffer();
  const VkBuffer instances = instanceBuffer_.handle();
  vkCmdBindVertexBuffers(commandBuffer_,0,1,&mesh,&offset);
  vkCmdBindVertexBuffers(commandBuffer_,1,1,&instances,&offset);
  vkCmdBindIndexBuffer(commandBuffer_,dirtRoadResources_.indexBuffer(),0,VK_INDEX_TYPE_UINT32);
  const u32 resolution = renderingPolicy_.shadows.cascadeResolution;
  if (!initializeAtlas) {
    VkClearAttachment depthClear{};
    depthClear.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthClear.clearValue.depthStencil = {1.0f, 0};
    for (u32 cascade = 0; cascade < shadowCascadeCount_; ++cascade) {
      if ((dirtyMask & (1u << cascade)) == 0) continue;
      VkClearRect rect{};
      rect.rect.offset = {static_cast<i32>((cascade & 1u) * resolution),
                          static_cast<i32>((cascade >> 1u) * resolution)};
      rect.rect.extent = {resolution, resolution};
      rect.baseArrayLayer = 0;
      rect.layerCount = 1;
      vkCmdClearAttachments(commandBuffer_, 1, &depthClear, 1, &rect);
    }
  }
  auto pushAndDraw = [&](u32 cascade, u32 drawIndex, bool masked) {
    if (!authoredVisibility_.empty() && (!authoredVisibility_[drawIndex] || !authoredShadows_[drawIndex])) return;
    const auto &draw=dirtRoadResources_.draws()[drawIndex];
    const auto &material=dirtRoadResources_.materials()[draw.materialIndex];
    // The animated receiver must not enter the static-caster cache. Ocean
    // self-shadowing is represented by its analytic normal; terrain and props
    // can still cast onto the water through the same cascade atlas.
    if ((material.flags & renderer::MapMaterialWater) != 0) return;
    ++shadowCandidateDraws_;
    if (!renderer::isShadowCasterVisible(shadowCascades_[cascade], draw.boundsCenter,
                                         draw.boundsRadius)) return;
    if (masked && !useBindless_) {
      const VkDescriptorSet set=dirtMaterialSets_[draw.materialIndex];
      vkCmdBindDescriptorSets(commandBuffer_,VK_PIPELINE_BIND_POINT_GRAPHICS,
                              shadowPipelineLayout_,0,1,&set,0,nullptr);
    }
    ShadowPushConstants push{};
    std::memcpy(push.lightViewProjection,shadowCascades_[cascade].viewProjection,
                sizeof(push.lightViewProjection));
    push.alphaCutoffUvSlot[0]=material.alphaCutoff;
    push.alphaCutoffUvSlot[1]=(material.textureCoordinates&3u)==1u?1.0f:0.0f;
    const u32 texture=material.textureIndices[0];
    push.baseTextureIndex[0]=useBindless_ && texture!=renderer::InvalidMapTexture
                                 ? dirtTextureSlots_[texture] : baseTextureIndex_;
    vkCmdPushConstants(commandBuffer_,shadowPipelineLayout_,
                       VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
                       0,sizeof(push),&push);
    vkCmdDrawIndexed(commandBuffer_,draw.indexCount,1,draw.firstIndex,
                     static_cast<i32>(draw.vertexOffset),drawIndex);
    ++shadowSubmittedDraws_;
  };
  for(u32 cascade=0;cascade<shadowCascadeCount_;++cascade) {
    if ((dirtyMask & (1u << cascade)) == 0) continue;
    ++shadowRenderedCascades_;
    VkViewport viewport{static_cast<float>((cascade&1u)*resolution),
                        static_cast<float>((cascade>>1u)*resolution),
                        static_cast<float>(resolution),static_cast<float>(resolution),0.0f,1.0f};
    VkRect2D scissor{{static_cast<i32>((cascade&1u)*resolution),
                      static_cast<i32>((cascade>>1u)*resolution)}, {resolution,resolution}};
    vkCmdSetViewport(commandBuffer_,0,1,&viewport);
    vkCmdSetScissor(commandBuffer_,0,1,&scissor);
    vkCmdSetDepthBias(commandBuffer_,renderingPolicy_.shadows.depthBiasConstant,0.0f,
                      renderingPolicy_.shadows.depthBiasSlope);
    vkCmdBindPipeline(commandBuffer_,VK_PIPELINE_BIND_POINT_GRAPHICS,shadowOpaquePipeline_);
    // Packages store every discrete LOD as draw records. Shadowing all stored
    // levels would duplicate the same caster and turn a content optimization
    // into a shadow spike. Until shadow LOD gets its own stable selection
    // epoch, use the authoritative LOD0 set: correct silhouettes, no duplicate
    // geometry, and deterministic static-cache invalidation.
    const std::vector<u32> &shadowSolidDraws =
        lodGroups_.empty() ? solidDrawOrder_ : levelZeroSolidDrawOrder_;
    const std::vector<u32> &shadowCoverageDraws =
        coverageLodGroups_.empty() ? coverageDrawOrder_ : levelZeroCoverageDrawOrder_;
    for(u32 drawIndex:shadowSolidDraws) pushAndDraw(cascade,drawIndex,false);
    if(!shadowCoverageDraws.empty()) {
      vkCmdBindPipeline(commandBuffer_,VK_PIPELINE_BIND_POINT_GRAPHICS,shadowMaskedPipeline_);
      if(useBindless_)
        vkCmdBindDescriptorSets(commandBuffer_,VK_PIPELINE_BIND_POINT_GRAPHICS,
                                shadowPipelineLayout_,0,1,&textureSet_,0,nullptr);
      for(u32 drawIndex:shadowCoverageDraws) pushAndDraw(cascade,drawIndex,true);
    }
  }
  vkCmdEndRenderPass(commandBuffer_);
  shadowCacheInitialized_ = true;
  shadowCascadeDirtyMask_ = 0;
}

void InstancedRenderer::destroyShadowResources() {
  if (shadowOpaquePipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_,shadowOpaquePipeline_,nullptr);
  if (shadowMaskedPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_,shadowMaskedPipeline_,nullptr);
  if (shadowPipelineLayout_ != VK_NULL_HANDLE) vkDestroyPipelineLayout(device_,shadowPipelineLayout_,nullptr);
  if (shadowFramebuffer_ != VK_NULL_HANDLE) vkDestroyFramebuffer(device_,shadowFramebuffer_,nullptr);
  if (shadowCachedFramebuffer_ != VK_NULL_HANDLE)
    vkDestroyFramebuffer(device_,shadowCachedFramebuffer_,nullptr);
  if (shadowRenderPass_ != VK_NULL_HANDLE) vkDestroyRenderPass(device_,shadowRenderPass_,nullptr);
  if (shadowCachedRenderPass_ != VK_NULL_HANDLE)
    vkDestroyRenderPass(device_,shadowCachedRenderPass_,nullptr);
  shadowOpaquePipeline_=VK_NULL_HANDLE; shadowMaskedPipeline_=VK_NULL_HANDLE;
  shadowPipelineLayout_=VK_NULL_HANDLE; shadowFramebuffer_=VK_NULL_HANDLE;
  shadowCachedFramebuffer_=VK_NULL_HANDLE; shadowRenderPass_=VK_NULL_HANDLE;
  shadowCachedRenderPass_=VK_NULL_HANDLE; shadowCascadeCount_=0;
  shadowCacheInitialized_=false; shadowCascadeDirtyMask_=0xffffffffu;
  shadowCacheHitFrames_=0;
  shadowSampler_.shutdown(); shadowAtlas_.reset(); shadowDepthFormat_=VK_FORMAT_UNDEFINED;
}

bool InstancedRenderer::createFramebuffers() {
  framebufferCount_ = swapchain_->imageCount();
  for (u32 i = 0; i < framebufferCount_; ++i) {
    const VkImageView colorView = renderingPolicy_.post.dedicatedPass
                                      ? postSceneColor_.view() : swapchain_->imageView(i);
    const VkImageView attachments[] = {colorView, depthImage_.view()};
    VkFramebufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.renderPass = renderPass_;
    info.attachmentCount = 2;
    info.pAttachments = attachments;
    info.width = renderTargetWidth();
    info.height = renderTargetHeight();
    info.layers = 1;
    if (vkCreateFramebuffer(device_, &info, nullptr, &framebuffers_[i]) != VK_SUCCESS) {
      return false;
    }
    if (renderingPolicy_.post.dedicatedPass) {
      const VkImageView presentView = swapchain_->imageView(i);
      info.renderPass = postRenderPass_;
      info.attachmentCount = 1;
      info.pAttachments = &presentView;
      info.width = swapchain_->width();
      info.height = swapchain_->height();
      if (vkCreateFramebuffer(device_, &info, nullptr, &postFramebuffers_[i]) != VK_SUCCESS)
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
    if (postFramebuffers_[i] != VK_NULL_HANDLE) {
      vkDestroyFramebuffer(device_, postFramebuffers_[i], nullptr);
      postFramebuffers_[i] = VK_NULL_HANDLE;
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
    dynamicMapDraws_.assign(instanceCount_, 0);
    pendingMapPoseCount_ = 0;
    const float tint[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    for (u32 index = 0; index < instanceCount_; ++index) {
      // Matriz singular/NaN rejeita o pacote inteiro em vez de escrever um
      // registro parcial que o shader leria como lixo.
      if (!renderer::buildGpuMeshInstance(draws[index].model, tint, &instances[index])) return false;
    }
    if (!memoryAllocator_->flushBuffer(instanceBuffer_)) return false;
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice_, &properties);
    const auto &enabledFeatures = rhiDevice_->deviceFeatures();
    useMultiDrawIndirect_ = enabledFeatures.multiDrawIndirect &&
                            enabledFeatures.drawIndirectFirstInstance &&
                            properties.limits.maxDrawIndirectCount >= instanceCount_;
    if (useMultiDrawIndirect_) {
      rhi::BufferDesc indirect{};
      indirect.sizeBytes = static_cast<u64>(instanceCount_) *
                           sizeof(VkDrawIndexedIndirectCommand);
      // STORAGE alem de INDIRECT: o kernel de oclusao escreve o instanceCount
      // destes mesmos comandos (ver createDrawCullResources). Declarar o usage
      // sempre mantem um unico buffer para os dois caminhos.
      indirect.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
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
  graphInputs.width = renderTargetWidth();
  graphInputs.height = renderTargetHeight();
  graphInputs.hzbEnabled = hzbWorkloadEligible_;
  graphInputs.temporalAaEnabled = temporalAaActive_;
  graphInputs.waterDepthInputEnabled = waterSubpassActive_;
  frameAttachmentPolicy_ = renderer::resolveFrameAttachmentPolicy(graphInputs);
  if (!frameAttachmentPolicy_.valid) {
    // Falha aberta: sem política compilada, armazenar é o comportamento que
    // nunca produz conteúdo indefinido.
    frameAttachmentPolicy_.depthStored = true;
    frameAttachmentPolicy_.depthSampled = hzbWorkloadEligible_ || temporalAaActive_;
    frameAttachmentPolicy_.depthMemoryless = false;
    __android_log_print(ANDROID_LOG_WARN, LogTag,
        "[FrameGraph] política de anexos não compilou; usando store conservador.");
  }
  depthFormat_ = chooseDepthFormat(physicalDevice_, frameAttachmentPolicy_.depthSampled);
  if (depthFormat_ == VK_FORMAT_UNDEFINED && frameAttachmentPolicy_.depthSampled) {
    // Optional capability failure must not take the renderer down. Fall back
    // to ordinary depth, disable HZB and replace temporal AA with FXAA.
    __android_log_print(ANDROID_LOG_WARN, LogTag,
                        "[FrameGraph] nenhum depth sampled; HZB off e TAA usa FXAA.");
    hzbWorkloadEligible_ = false;
    graphInputs.hzbEnabled = false;
    temporalAaActive_ = false;
    graphInputs.temporalAaEnabled = false;
    if (renderingPolicy_.post.antiAliasing == renderer::AntiAliasingMode::Temporal) {
      renderingPolicy_.post.antiAliasing = renderer::AntiAliasingMode::Fxaa;
      resourceRenderingPolicy_.post.antiAliasing = renderer::AntiAliasingMode::Fxaa;
    }
    frameAttachmentPolicy_ = renderer::resolveFrameAttachmentPolicy(graphInputs);
    depthFormat_ = chooseDepthFormat(physicalDevice_, false);
  }
  if (depthFormat_ == VK_FORMAT_UNDEFINED) return false;
  rhi::ImageDesc desc{};
  desc.width = renderTargetWidth();
  desc.height = renderTargetHeight();
  desc.format = depthFormat_;
  desc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
               (frameAttachmentPolicy_.depthSampled ? VK_IMAGE_USAGE_SAMPLED_BIT : 0u) |
               (frameAttachmentPolicy_.depthInputAttachment
                    ? VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT : 0u);
  desc.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  if (hasStencil(depthFormat_)) desc.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
  desc.memoryClass = rhi::MemoryClass::RenderTarget;
  // Sem leitor fora do render pass, o depth nunca precisa de lastro em DRAM.
  // A chave diagnóstica só desliga o transitório; ela não mexe em store/sampled,
  // para que o A/B isole exatamente uma variável.
  desc.transient = frameAttachmentPolicy_.depthMemoryless && !disableTransientDepth_;
  if (!memoryAllocator_->createImage(desc, &depthImage_)) return false;
  // A política pede; o driver concede ou não. Reportar as duas coisas separadas
  // evita afirmar economia de banda que talvez não tenha acontecido.
  const bool lazyGranted = depthImage_.isLazilyAllocated();
  __android_log_print(ANDROID_LOG_INFO, LogTag,
      "[FrameGraph] depth %ux%u: store=%s sampled=%s memoryless_pedido=%s "
      "lazily_allocated_concedido=%s.",
      desc.width, desc.height, frameAttachmentPolicy_.depthStored ? "sim" : "nao",
      frameAttachmentPolicy_.depthSampled ? "sim" : "nao",
      desc.transient ? "sim" : "nao", lazyGranted ? "sim" : "nao");
  if (desc.transient && !lazyGranted) {
    __android_log_print(ANDROID_LOG_WARN, LogTag,
        "[FrameGraph] anexo transitório sem memória LAZILY_ALLOCATED neste device; "
        "correto, porém sem a economia de banda que o transitório existe para dar.");
  }
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
struct HzbReduceComputePushConstants {
  u32 sourceWidth;
  u32 sourceHeight;
  u32 destinationWidth;
  u32 destinationHeight;
  u32 firstLevel;
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
  // O consumidor referencia as imagens e o sampler destruidos abaixo.
  destroyDrawCompactionResources();
  destroyDrawCullResources();
  for (HzbLevelResources &level : hzbLevels_) {
    level.computeKernel.shutdown();
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
  hzbComputeActive_ = false;
  hzbComputeImagesInitialized_ = false;
  hzbReadbackRecordedThisFrame_ = false;
  hzbComputeValidationLogged_ = false;
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
  u32 width = std::max(renderTargetWidth() / 8u, 4u);
  u32 height = std::max(renderTargetHeight() / 8u, 4u);
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
  VkFormatProperties hzbFormatProperties{};
  vkGetPhysicalDeviceFormatProperties(physicalDevice_, kHzbFormat, &hzbFormatProperties);
  const bool storageImageSupported =
      (hzbFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) != 0;
  hzbComputeActive_ = hzbComputeEnabled_ && rhiDevice_ != nullptr &&
                      rhiDevice_->computeLimits().supported && storageImageSupported;
  if (hzbComputeEnabled_ && !hzbComputeActive_) {
    __android_log_print(ANDROID_LOG_WARN, LogTag,
        "[HZB/Compute] indisponível: compute=%s R32_SFLOAT_storage=%s; fallback raster preservado.",
        rhiDevice_ != nullptr && rhiDevice_->computeLimits().supported ? "sim" : "nao",
        storageImageSupported ? "sim" : "nao");
  }
  if (!hzbComputeActive_) {
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

  if (!hzbComputeActive_) {
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
  if (!hzbComputeActive_) {
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

  if (!hzbComputeActive_) {
    if (!createHzbPipeline(rhi::shaders::kHzb_Reduce_FirstVertSpirv,
                           rhi::shaders::kHzb_Reduce_FirstVertSpirvSize,
                           rhi::shaders::kHzb_Reduce_FirstFragSpirv,
                           rhi::shaders::kHzb_Reduce_FirstFragSpirvSize,
                           sizeof(HzbReduceFirstPushConstants), hzbFirstPipelineLayout_,
                           hzbFirstPipeline_)) return false;
    if (!createHzbPipeline(rhi::shaders::kHzb_ReduceVertSpirv,
                           rhi::shaders::kHzb_ReduceVertSpirvSize,
                           rhi::shaders::kHzb_ReduceFragSpirv,
                           rhi::shaders::kHzb_ReduceFragSpirvSize,
                           sizeof(HzbReducePushConstants), hzbReducePipelineLayout_,
                           hzbReducePipeline_)) return false;
  }

  const usize readbackFloats = readbackOffsetFloats;
  rhi::BufferDesc readbackDesc{};
  readbackDesc.sizeBytes = readbackFloats * sizeof(float);
  readbackDesc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  readbackDesc.memoryClass = rhi::MemoryClass::RenderTarget;
  readbackDesc.cpuAccess = rhi::CpuAccess::Random; // GPU writes, CPU reads repeatedly -- see invalidateBuffer.
  readbackDesc.preferDeviceMemory = false;
  if (!hzbComputeActive_ || hzbComputeReadbackValidationEnabled_) {
    if (!memoryAllocator_->createBuffer(readbackDesc, &hzbReadbackBuffer_) ||
        hzbReadbackBuffer_.mappedData() == nullptr) return false;
  }

  for (u32 level = 0; level < kHzbLevelCount; ++level) {
    HzbLevelResources &resources = hzbLevels_[level];
    rhi::ImageDesc imageDesc{};
    imageDesc.width = resources.width;
    imageDesc.height = resources.height;
    imageDesc.format = kHzbFormat;
    imageDesc.usage = VK_IMAGE_USAGE_SAMPLED_BIT |
        (hzbComputeActive_ ? VK_IMAGE_USAGE_STORAGE_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) |
        ((!hzbComputeActive_ || hzbComputeReadbackValidationEnabled_)
             ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0u);
    imageDesc.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageDesc.memoryClass = rhi::MemoryClass::RenderTarget;
    if (!memoryAllocator_->createImage(imageDesc, &resources.image)) return false;

    const VkImageView sourceView = level == 0 ? depthImage_.view()
                                              : hzbLevels_[level - 1].image.view();
    if (hzbComputeActive_) {
      const rhi::ComputeBindingDesc bindings[] = {
          {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
          {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
      };
      rhi::ComputeKernelDesc kernelDesc{};
      kernelDesc.spirv = rhi::shaders::kHzb_Reduce_ComputeCompSpirv;
      kernelDesc.spirvBytes = rhi::shaders::kHzb_Reduce_ComputeCompSpirvSize;
      kernelDesc.bindings = bindings;
      kernelDesc.bindingCount = 2;
      kernelDesc.pushConstantBytes = sizeof(HzbReduceComputePushConstants);
      kernelDesc.debugName = "HZB/ReduceCompute";
      if (!resources.computeKernel.initialize(
              device_, rhiDevice_->computeLimits(), kernelDesc,
              rhiDevice_->pipelineCache().driverHandle()) ||
          !resources.computeKernel.writeImage(
              0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, sourceView,
              level == 0 ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                         : VK_IMAGE_LAYOUT_GENERAL,
              hzbSampler_.handle()) ||
          !resources.computeKernel.writeImage(
              1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, resources.image.view(),
              VK_IMAGE_LAYOUT_GENERAL)) return false;
      rhiDevice_->setObjectName(
          VK_OBJECT_TYPE_PIPELINE,
          reinterpret_cast<u64>(resources.computeKernel.handle()),
          "HZB/ReduceComputePipeline");
      rhiDevice_->setObjectName(VK_OBJECT_TYPE_IMAGE,
          reinterpret_cast<u64>(resources.image.handle()), "HZB/ComputeLevel");
    } else {
      const VkImageView ownView = resources.image.view();
      VkFramebufferCreateInfo fbInfo{};
      fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      fbInfo.renderPass = hzbRenderPass_;
      fbInfo.attachmentCount = 1;
      fbInfo.pAttachments = &ownView;
      fbInfo.width = resources.width;
      fbInfo.height = resources.height;
      fbInfo.layers = 1;
      if (vkCreateFramebuffer(device_, &fbInfo, nullptr, &resources.framebuffer) != VK_SUCCESS)
        return false;

      VkDescriptorSetAllocateInfo allocInfo{};
      allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
      allocInfo.descriptorPool = hzbDescriptorPool_;
      allocInfo.descriptorSetCount = 1;
      allocInfo.pSetLayouts = &hzbDescriptorSetLayout_;
      if (vkAllocateDescriptorSets(device_, &allocInfo, &resources.descriptorSet) != VK_SUCCESS)
        return false;
      VkDescriptorImageInfo imageInfo{};
      imageInfo.sampler = hzbSampler_.handle();
      imageInfo.imageView = sourceView;
      imageInfo.imageLayout = level == 0
                                  ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                  : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      VkWriteDescriptorSet write{};
      write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.dstSet = resources.descriptorSet;
      write.dstBinding = 0;
      write.descriptorCount = 1;
      write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      write.pImageInfo = &imageInfo;
      vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
    }
  }

  hzbResourcesReady_ = true;
  // O consumidor GPU e criado por ultimo: ele precisa das imagens, do sampler e
  // do buffer indireto ja prontos, e nunca falha a inicializacao do renderer.
  if (!createDrawCullResources()) return false;
  // A compactacao depende do culling estar ativo: sem instanceCount zerado por
  // alguem nao ha buraco nenhum para remover.
  if (!createDrawCompactionResources()) return false;
  __android_log_print(ANDROID_LOG_INFO, LogTag,
      "[HZB] produtor=%s níveis=%u base=%ux%u readback=%s.",
      hzbComputeActive_ ? "compute" : "raster", kHzbLevelCount,
      hzbLevels_[0].width, hzbLevels_[0].height,
      (!hzbComputeActive_ || hzbComputeReadbackValidationEnabled_) ? "sim" : "nao");
  return true;
}

void InstancedRenderer::recordHzbReductionPass(const platform::FreeCameraState &camera) {
  if (!hzbResourcesReady_) return;
  hzbReadbackRecordedThisFrame_ = false;

  // The frame graph selected STORE+SAMPLED before render-pass creation, so the
  // main pass already ends in DEPTH_STENCIL_READ_ONLY_OPTIMAL and publishes
  // its LATE_FRAGMENT_TESTS writes. TAA may have sampled the same immutable
  // depth immediately before this pass; read->read needs no second transition.

  if (hzbComputeActive_) {
    // A render-pass external dependency targets fragment consumers (TAA and
    // raster HZB). Compute is recorded outside render passes, so publish the
    // same immutable depth explicitly to its own stage without a layout turn.
    VkImageMemoryBarrier depthReady{};
    depthReady.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    depthReady.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    depthReady.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    depthReady.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    depthReady.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    depthReady.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    depthReady.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    depthReady.image = depthImage_.handle();
    depthReady.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(commandBuffer_, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &depthReady);
    constexpr u32 localSize = 8;
    for (u32 level = 0; level < kHzbLevelCount; ++level) {
      HzbLevelResources &resources = hzbLevels_[level];
      const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
      rhi::cmdComputeImageBarrier(
          commandBuffer_, resources.image.handle(), range,
          hzbComputeImagesInitialized_ ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED,
          VK_IMAGE_LAYOUT_GENERAL,
          hzbComputeImagesInitialized_ ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                                       : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
          hzbComputeImagesInitialized_ ? (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)
                                       : 0,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT);
      const u32 sourceWidth = level == 0 ? renderWidth() : hzbLevels_[level - 1].width;
      const u32 sourceHeight = level == 0 ? renderHeight() : hzbLevels_[level - 1].height;
      const HzbReduceComputePushConstants push{
          sourceWidth, sourceHeight, resources.width, resources.height,
          level == 0 ? 1u : 0u};
      const rhi::ComputeDispatch dispatch{
          (resources.width + localSize - 1) / localSize,
          (resources.height + localSize - 1) / localSize, 1};
      if (!resources.computeKernel.recordDispatch(commandBuffer_, dispatch, &push,
                                                   sizeof(push))) {
        __android_log_print(ANDROID_LOG_ERROR, LogTag,
                            "[HZB/Compute] dispatch inválido no nível %u.", level);
        hzbComputeActive_ = false;
        hzbFrameEligible_ = false;
        return;
      }
      // The next level samples this image in the same queue. GENERAL avoids a
      // layout round-trip; the explicit write->read dependency is the actual
      // correctness requirement.
      rhi::cmdComputeImageBarrier(
          commandBuffer_, resources.image.handle(), range,
          VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
    }
    hzbComputeImagesInitialized_ = true;

    if (hzbComputeReadbackValidationEnabled_) {
      VkImageMemoryBarrier toTransfer[kHzbLevelCount]{};
      for (u32 level = 0; level < kHzbLevelCount; ++level) {
        VkImageMemoryBarrier &barrier = toTransfer[level];
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = hzbLevels_[level].image.handle();
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
      }
      vkCmdPipelineBarrier(commandBuffer_, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr,
                           kHzbLevelCount, toTransfer);
      for (u32 level = 0; level < kHzbLevelCount; ++level) {
        VkBufferImageCopy region{};
        region.bufferOffset = static_cast<VkDeviceSize>(
            hzbLevels_[level].readbackOffsetFloats) * sizeof(float);
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageExtent = {hzbLevels_[level].width, hzbLevels_[level].height, 1};
        vkCmdCopyImageToBuffer(commandBuffer_, hzbLevels_[level].image.handle(),
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               hzbReadbackBuffer_.handle(), 1, &region);
      }
      for (u32 level = 0; level < kHzbLevelCount; ++level) {
        VkImageMemoryBarrier &barrier = toTransfer[level];
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
      }
      vkCmdPipelineBarrier(commandBuffer_, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                           kHzbLevelCount, toTransfer);
      hzbReadbackRecordedThisFrame_ = true;
    }
    hzbRecordedCamera_ = camera;
    hzbRecordedCameraValid_ = true;
    return;
  }

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
      const HzbReduceFirstPushConstants push{renderWidth(), renderHeight(), resources.width,
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
  hzbReadbackRecordedThisFrame_ = true;
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
  if (hzbPyramidValid_ && hzbComputeActive_) {
    hzbPyramidValid_ = renderer::validateHzbMaxReductionChain(hzbPyramid_);
    if (!hzbComputeValidationLogged_) {
      __android_log_print(hzbPyramidValid_ ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR,
          LogTag, "[HZB/ComputeValidation] cadeia_max_2x2=%s texels=%zu níveis=%u.",
          hzbPyramidValid_ ? "valida" : "invalida", floatCount, kHzbLevelCount);
      hzbComputeValidationLogged_ = true;
    }
  }
  hzbPyramidCameraValid_ = hzbPyramidValid_ && hzbRecordedCameraValid_;
  if (hzbPyramidCameraValid_) hzbPyramidCamera_ = hzbRecordedCamera_;
}

renderer::PerspectiveFrustum InstancedRenderer::buildFrameFrustum(
    const platform::FreeCameraState &camera) const {
  renderer::PerspectiveVisibilitySettings settings = visibilitySettings_;
  settings.verticalFieldOfViewRadians = sceneFieldOfView();
  settings.nearPlane = sceneNearPlane();
  settings.farPlane = sceneFarPlane();
  return renderer::buildPerspectiveFrustum(
      camera.position, camera.yaw, camera.pitch,
      sceneAspectRatio(),
      settings);
}

void InstancedRenderer::destroyDrawCullResources() {
  drawCullKernel_.shutdown();
  drawCullRecordBuffer_.reset();
  drawCullStateBuffer_.reset();
  drawCullTelemetryBuffer_.reset();
  drawCullCapacity_ = 0;
  hzbGpuCullingActive_ = false;
  drawCullDispatchedThisFrame_ = false;
  drawCullContractLogged_ = false;
  drawCullTelemetry_ = {};
}

bool InstancedRenderer::createDrawCullResources() {
  // draw_cull.comp declara um binding por nivel porque a reflexao do RHI so
  // aceita descriptorCount 1 (ver rhi/compute_validation.cpp). Mudar a altura da
  // piramide sem mudar o shader ficaria silencioso sem esta assercao.
  static_assert(kHzbLevelCount == 6, "draw_cull.comp declara exatamente seis niveis de HZB.");
  destroyDrawCullResources();
  if (!hzbGpuCullingEnabled_) return true;
  // O consumidor e opcional: sua ausencia nao invalida o produtor. Qualquer
  // pre-requisito faltando desliga so o culling, e o frame continua correto
  // desenhando tudo o que sobreviveu ao frustum na CPU.
  if (!hzbComputeActive_ || !useMultiDrawIndirect_ || !dirtRoadPreview_ ||
      instanceCount_ == 0 || !indirectBuffer_.isReady() ||
      rhiDevice_ == nullptr || !rhiDevice_->computeLimits().supported) {
    __android_log_print(ANDROID_LOG_WARN, LogTag,
        "[Culling] indisponivel: produtor_compute=%s multi_draw=%s mapa=%s draws=%u.",
        hzbComputeActive_ ? "sim" : "nao", useMultiDrawIndirect_ ? "sim" : "nao",
        dirtRoadPreview_ ? "sim" : "nao", instanceCount_);
    return true;
  }

  const u32 capacity = instanceCount_;
  rhi::BufferDesc recordDesc{};
  recordDesc.sizeBytes = static_cast<u64>(capacity) * sizeof(renderer::GpuCullDrawRecord);
  recordDesc.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  recordDesc.memoryClass = rhi::MemoryClass::Buffer;
  recordDesc.cpuAccess = rhi::CpuAccess::SequentialWrite;
  recordDesc.preferDeviceMemory = false;

  rhi::BufferDesc stateDesc = recordDesc;
  stateDesc.sizeBytes = static_cast<u64>(capacity) * sizeof(u32);
  // A histerese e lida e escrita pela GPU todo frame e zerada pela CPU quando o
  // estagio sai do ar; Random e o acesso honesto, nao SequentialWrite.
  stateDesc.cpuAccess = rhi::CpuAccess::Random;

  rhi::BufferDesc telemetryDesc = recordDesc;
  telemetryDesc.sizeBytes = sizeof(u32) * 4;
  telemetryDesc.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  telemetryDesc.cpuAccess = rhi::CpuAccess::Random;

  const bool buffersReady =
      memoryAllocator_->createBuffer(recordDesc, &drawCullRecordBuffer_) &&
      drawCullRecordBuffer_.mappedData() != nullptr &&
      memoryAllocator_->createBuffer(stateDesc, &drawCullStateBuffer_) &&
      drawCullStateBuffer_.mappedData() != nullptr &&
      memoryAllocator_->createBuffer(telemetryDesc, &drawCullTelemetryBuffer_) &&
      drawCullTelemetryBuffer_.mappedData() != nullptr;
  if (!buffersReady) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag,
        "[Culling] falha ao alocar buffers do kernel; oclusao GPU permanece desligada.");
    destroyDrawCullResources();
    return true;
  }
  // Estado e registros comecam zerados: nenhum objeto entra com streak herdado
  // de uma epoca anterior, e nenhum slot de sobra entra marcado como presente.
  std::memset(drawCullStateBuffer_.mappedData(), 0, static_cast<usize>(stateDesc.sizeBytes));
  std::memset(drawCullRecordBuffer_.mappedData(), 0, static_cast<usize>(recordDesc.sizeBytes));
  if (!memoryAllocator_->flushBuffer(drawCullStateBuffer_) ||
      !memoryAllocator_->flushBuffer(drawCullRecordBuffer_)) {
    destroyDrawCullResources();
    return true;
  }

  rhi::ComputeBindingDesc bindings[4 + kHzbLevelCount]{};
  for (u32 index = 0; index < 4; ++index)
    bindings[index] = {index, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};
  for (u32 level = 0; level < kHzbLevelCount; ++level)
    bindings[4 + level] = {4 + level, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};

  rhi::ComputeKernelDesc kernelDesc{};
  kernelDesc.spirv = rhi::shaders::kDraw_CullCompSpirv;
  kernelDesc.spirvBytes = rhi::shaders::kDraw_CullCompSpirvSize;
  kernelDesc.bindings = bindings;
  kernelDesc.bindingCount = 4 + kHzbLevelCount;
  kernelDesc.pushConstantBytes = sizeof(renderer::GpuCullParameters);
  kernelDesc.debugName = "Culling/DrawCull";
  bool ready = drawCullKernel_.initialize(device_, rhiDevice_->computeLimits(), kernelDesc,
                                          rhiDevice_->pipelineCache().driverHandle()) &&
      drawCullKernel_.writeBuffer(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, drawCullRecordBuffer_) &&
      drawCullKernel_.writeBuffer(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, indirectBuffer_) &&
      drawCullKernel_.writeBuffer(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, drawCullStateBuffer_) &&
      drawCullKernel_.writeBuffer(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, drawCullTelemetryBuffer_);
  for (u32 level = 0; ready && level < kHzbLevelCount; ++level) {
    // GENERAL e nao SHADER_READ_ONLY_OPTIMAL: e o layout em que a cadeia de
    // reducao em compute deixa cada nivel, e uma ida e volta de layout por
    // frame custaria mais do que a amostragem em GENERAL.
    ready = drawCullKernel_.writeImage(4 + level, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                       hzbLevels_[level].image.view(), VK_IMAGE_LAYOUT_GENERAL,
                                       hzbSampler_.handle());
  }
  if (!ready) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag,
        "[Culling] contrato do kernel recusado; oclusao GPU permanece desligada.");
    destroyDrawCullResources();
    return true;
  }
  rhiDevice_->setObjectName(VK_OBJECT_TYPE_PIPELINE,
      reinterpret_cast<u64>(drawCullKernel_.handle()), "Culling/DrawCullPipeline");
  rhiDevice_->setObjectName(VK_OBJECT_TYPE_BUFFER,
      reinterpret_cast<u64>(drawCullRecordBuffer_.handle()), "Culling/DrawRecords");
  drawCullCapacity_ = capacity;
  hzbGpuCullingActive_ = true;
  __android_log_print(ANDROID_LOG_INFO, LogTag,
      "[Culling] consumidor=compute capacidade=%u histerese=%u bias=%.6f base=%ux%u niveis=%u.",
      drawCullCapacity_, hzbHysteresisFrames_, static_cast<double>(hzbNormalizedDepthBias_),
      hzbLevels_[0].width, hzbLevels_[0].height, kHzbLevelCount);
  return true;
}

void InstancedRenderer::recordDrawCullDispatch(const platform::FreeCameraState &camera) {
  drawCullDispatchedThisFrame_ = false;
  if (!sceneViewport_.isEmpty() || !hzbGpuCullingActive_ || !hzbComputeImagesInitialized_) return;

  // A view descreve a FORMA da piramide, nao seu conteudo: os texels vivem em
  // seis imagens da GPU e nunca sao mapeados neste caminho. O ponteiro nulo e
  // deliberado -- so a referencia de CPU (teste e validacao) o le.
  renderer::GpuCullHzbView hzbView{};
  hzbView.baseWidth = hzbLevels_[0].width;
  hzbView.baseHeight = hzbLevels_[0].height;
  hzbView.levelCount = kHzbLevelCount;

  const rhi::SurfaceTransform &surfaceTransform = swapchain_->surfaceTransform();
  const renderer::HzbScreenTransform screenTransform{
      surfaceTransform.xx, surfaceTransform.xy, surfaceTransform.yx, surfaceTransform.yy};
  const renderer::PerspectiveFrustum frustum = buildFrameFrustum(camera);
  // A piramide foi construida no fim de um frame anterior, com a pose gravada
  // em hzbRecordedCamera_. A guarda converte a diferenca entre as duas poses em
  // folga; ela nunca torna o teste mais agressivo.
  const renderer::GpuCullMotionGuard guard = renderer::buildGpuCullMotionGuard(
      frustum, hzbRecordedCamera_.position, hzbRecordedCamera_.yaw, hzbRecordedCamera_.pitch);
  renderer::GpuCullParameters parameters{};
  if (!renderer::buildGpuCullParameters(frustum, screenTransform, hzbView, guard,
                                        hzbNormalizedDepthBias_, hzbHysteresisFrames_,
                                        drawCullCapacity_, hzbRecordedCameraValid_, parameters)) {
    return;
  }

  // Contadores zerados na GPU: le-los na CPU exigiria um ponto de sincronismo
  // que este caminho existe justamente para nao ter.
  vkCmdFillBuffer(commandBuffer_, drawCullTelemetryBuffer_.handle(), 0, VK_WHOLE_SIZE, 0);
  rhi::cmdComputeBufferBarrier(commandBuffer_, drawCullTelemetryBuffer_.handle(), 0, VK_WHOLE_SIZE,
      VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

  // Os niveis foram escritos pela reducao do frame anterior, em outra submissao
  // da mesma fila: a ordem esta garantida, a visibilidade da memoria nao.
  const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  for (u32 level = 0; level < kHzbLevelCount; ++level) {
    rhi::cmdComputeImageBarrier(commandBuffer_, hzbLevels_[level].image.handle(), range,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
  }
  // O passe opaco do frame anterior leu este buffer como argumentos indiretos;
  // escreve-lo agora e um hazard write-after-read, nao read-after-write.
  rhi::cmdComputeBufferBarrier(commandBuffer_, indirectBuffer_.handle(), 0, VK_WHOLE_SIZE,
      VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT);

  const rhi::ComputeDispatch dispatch{renderer::gpuCullGroupCount(drawCullCapacity_), 1, 1};
  if (!drawCullKernel_.recordDispatch(commandBuffer_, dispatch, &parameters, sizeof(parameters))) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag,
        "[Culling] dispatch invalido para %u candidatos; estagio desligado.", drawCullCapacity_);
    hzbGpuCullingActive_ = false;
    return;
  }
  rhi::cmdComputeBufferBarrier(commandBuffer_, indirectBuffer_.handle(), 0, VK_WHOLE_SIZE,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
      VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT);
  drawCullDispatchedThisFrame_ = true;
  if (!drawCullContractLogged_) {
    __android_log_print(ANDROID_LOG_INFO, LogTag,
        "[Culling] primeiro dispatch: grupos=%u guarda_tela=%.5f guarda_profundidade=%.3f "
        "piramide_utilizavel=%s.",
        dispatch.x, static_cast<double>(parameters.screenDilation),
        static_cast<double>(parameters.viewDepthGuard),
        (parameters.flags & renderer::GpuCullPyramidUsable) != 0 ? "sim" : "nao");
    drawCullContractLogged_ = true;
  }
}

void InstancedRenderer::destroyDrawCompactionResources() {
  drawCompactKernel_.shutdown();
  compactedIndirectBuffer_.reset();
  compactBatchBuffer_.reset();
  compactCountBuffer_.reset();
  drawCompactionBatchCapacity_ = 0;
  drawCompactionBatchCount_ = 0;
  drawCompactionActive_ = false;
  drawCompactionDispatchedThisFrame_ = false;
  drawCompactionContractLogged_ = false;
}

bool InstancedRenderer::createDrawCompactionResources() {
  destroyDrawCompactionResources();
  // Consumidor opcional de um consumidor opcional: sem oclusao nao ha buraco
  // para remover, e sem a extensao nao ha como a GPU dizer quantos comandos
  // sobraram. Nos dois casos o frame continua correto pela lista original.
  if (!hzbGpuCullingActive_ || drawCullCapacity_ == 0) return true;
  if (rhiDevice_ == nullptr || !rhiDevice_->drawIndirectCountSupported()) {
    __android_log_print(ANDROID_LOG_INFO, LogTag,
        "[Culling] compactacao indisponivel: draw_indirect_count=nao. O passe opaco "
        "continua submetendo os ocluidos com instanceCount zero.");
    return true;
  }

  // Um lote por combinacao de material e variante de distancia, nas duas
  // familias (solida e cobertura). E um teto, nao uma previsao: o frame publica
  // apenas os lotes que existirem e zera o resto.
  const u64 materialCount = static_cast<u64>(dirtRoadResources_.materials().size());
  const u64 batchCapacity = std::max<u64>(1, materialCount * 4);
  if (batchCapacity > 0xffffffffull) return true;

  rhi::BufferDesc compactedDesc{};
  compactedDesc.sizeBytes =
      static_cast<u64>(drawCullCapacity_) * sizeof(VkDrawIndexedIndirectCommand);
  // Escrito e lido so pela GPU: nao ha razao para ficar em memoria host-visible
  // como indirectBuffer_, que a CPU precisa preencher todo frame.
  compactedDesc.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
  compactedDesc.memoryClass = rhi::MemoryClass::Buffer;
  compactedDesc.cpuAccess = rhi::CpuAccess::None;
  compactedDesc.preferDeviceMemory = true;

  rhi::BufferDesc countDesc = compactedDesc;
  countDesc.sizeBytes = batchCapacity * sizeof(u32);

  rhi::BufferDesc batchDesc{};
  batchDesc.sizeBytes = batchCapacity * sizeof(renderer::GpuCompactBatch);
  batchDesc.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  batchDesc.memoryClass = rhi::MemoryClass::Buffer;
  batchDesc.cpuAccess = rhi::CpuAccess::SequentialWrite;
  batchDesc.preferDeviceMemory = false;

  if (!memoryAllocator_->createBuffer(compactedDesc, &compactedIndirectBuffer_) ||
      !memoryAllocator_->createBuffer(countDesc, &compactCountBuffer_) ||
      !memoryAllocator_->createBuffer(batchDesc, &compactBatchBuffer_) ||
      compactBatchBuffer_.mappedData() == nullptr) {
    __android_log_print(ANDROID_LOG_WARN, LogTag,
        "[Culling] recursos de compactacao recusados; estagio permanece desligado.");
    destroyDrawCompactionResources();
    return true;
  }
  // Nenhum lote entra herdado de uma epoca anterior: um slot com lixo viraria
  // um intervalo que o kernel recusaria caso a caso, mas o custo de zerar uma
  // vez e menor do que depender dessa recusa.
  std::memset(compactBatchBuffer_.mappedData(), 0, static_cast<usize>(batchDesc.sizeBytes));
  if (!memoryAllocator_->flushBuffer(compactBatchBuffer_)) {
    destroyDrawCompactionResources();
    return true;
  }

  const rhi::ComputeBindingDesc bindings[4] = {
      {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
      {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
      {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
      {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
  };
  rhi::ComputeKernelDesc kernelDesc{};
  kernelDesc.spirv = rhi::shaders::kDraw_CompactCompSpirv;
  kernelDesc.spirvBytes = rhi::shaders::kDraw_CompactCompSpirvSize;
  kernelDesc.bindings = bindings;
  kernelDesc.bindingCount = 4;
  kernelDesc.pushConstantBytes = sizeof(renderer::GpuCompactParameters);
  kernelDesc.debugName = "Culling/DrawCompact";
  const bool ready =
      drawCompactKernel_.initialize(device_, rhiDevice_->computeLimits(), kernelDesc,
                                    rhiDevice_->pipelineCache().driverHandle()) &&
      drawCompactKernel_.writeBuffer(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, indirectBuffer_) &&
      drawCompactKernel_.writeBuffer(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                     compactedIndirectBuffer_) &&
      drawCompactKernel_.writeBuffer(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, compactBatchBuffer_) &&
      drawCompactKernel_.writeBuffer(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, compactCountBuffer_);
  if (!ready) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag,
        "[Culling] contrato do kernel de compactacao recusado; estagio desligado.");
    destroyDrawCompactionResources();
    return true;
  }
  rhiDevice_->setObjectName(VK_OBJECT_TYPE_BUFFER,
      reinterpret_cast<u64>(compactedIndirectBuffer_.handle()), "Culling/CompactedCommands");
  rhiDevice_->setObjectName(VK_OBJECT_TYPE_BUFFER,
      reinterpret_cast<u64>(compactCountBuffer_.handle()), "Culling/CompactedCounts");
  drawCompactionBatchCapacity_ = static_cast<u32>(batchCapacity);
  drawCompactionActive_ = true;
  __android_log_print(ANDROID_LOG_INFO, LogTag,
      "[Culling] compactacao=compute lotes=%u comandos=%u bloco=%u.",
      drawCompactionBatchCapacity_, drawCullCapacity_,
      static_cast<unsigned>(renderer::kGpuCompactionGroupSize));
  return true;
}

void InstancedRenderer::recordDrawCompactDispatch() {
  drawCompactionDispatchedThisFrame_ = false;
  // Sem oclusao gravada neste frame nao ha nada a compactar: o buffer indireto
  // ainda tem o instanceCount=1 que a CPU escreveu, e compactar isso so copiaria
  // a lista inteira gastando um dispatch.
  if (!drawCompactionActive_ || !drawCullDispatchedThisFrame_) return;

  // O passe opaco do frame anterior leu os dois como argumentos indiretos.
  rhi::cmdComputeBufferBarrier(commandBuffer_, compactedIndirectBuffer_.handle(), 0, VK_WHOLE_SIZE,
      VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT);
  rhi::cmdComputeBufferBarrier(commandBuffer_, compactCountBuffer_.handle(), 0, VK_WHOLE_SIZE,
      VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT);
  // A leitura do kernel acontece depois da escrita do culling no mesmo buffer.
  rhi::cmdComputeBufferBarrier(commandBuffer_, indirectBuffer_.handle(), 0, VK_WHOLE_SIZE,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);

  renderer::GpuCompactParameters parameters{};
  // O dispatch cobre a capacidade inteira porque os lotes deste frame ainda nao
  // foram montados -- a CPU os escreve depois, antes do submit, exatamente como
  // ja faz com os registros do culling. Um slot zerado publica contagem zero.
  parameters.batchCount = drawCompactionBatchCapacity_;
  parameters.sourceCapacity = drawCullCapacity_;
  parameters.compactedCapacity = drawCullCapacity_;

  const rhi::ComputeDispatch dispatch{
      renderer::gpuCompactionGroupCount(drawCompactionBatchCapacity_), 1, 1};
  if (!drawCompactKernel_.recordDispatch(commandBuffer_, dispatch, &parameters,
                                         sizeof(parameters))) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag,
        "[Culling] dispatch de compactacao invalido para %u lotes; estagio desligado.",
        drawCompactionBatchCapacity_);
    drawCompactionActive_ = false;
    return;
  }
  rhi::cmdComputeBufferBarrier(commandBuffer_, compactedIndirectBuffer_.handle(), 0, VK_WHOLE_SIZE,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
      VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT);
  rhi::cmdComputeBufferBarrier(commandBuffer_, compactCountBuffer_.handle(), 0, VK_WHOLE_SIZE,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
      VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT);
  drawCompactionDispatchedThisFrame_ = true;
  if (!drawCompactionContractLogged_) {
    __android_log_print(ANDROID_LOG_INFO, LogTag,
        "[Culling] primeira compactacao: grupos=%u origem=%u destino=%u.",
        dispatch.x, parameters.sourceCapacity, parameters.compactedCapacity);
    drawCompactionContractLogged_ = true;
  }
}

bool InstancedRenderer::publishCompactionBatches() {
  drawCompactionBatchCount_ = 0;
  if (!drawCompactionDispatchedThisFrame_ || compactBatchBuffer_.mappedData() == nullptr)
    return false;

  compactionBatchScratch_.clear();
  const auto append = [&](std::vector<IndirectBatch> &batches) {
    for (IndirectBatch &batch : batches) {
      batch.compactionSlot = static_cast<u32>(compactionBatchScratch_.size());
      renderer::GpuCompactBatch entry{};
      entry.firstCommand = batch.firstCommand;
      entry.commandCount = batch.commandCount;
      // Destino igual a origem: os lotes ja particionam a lista de comandos em
      // intervalos disjuntos, entao cada um cabe compactado no proprio espaco.
      entry.compactedBase = batch.firstCommand;
      compactionBatchScratch_.push_back(entry);
    }
  };
  append(indirectSolidBatches_);
  append(indirectCoverageBatches_);

  if (compactionBatchScratch_.size() > drawCompactionBatchCapacity_) return false;
  // A mesma guarda que o kernel repete por conta propria. Recusar aqui evita
  // gravar um frame inteiro cuja submissao leria contagens que ninguem escreveu.
  if (!renderer::validateGpuCompactBatches(compactionBatchScratch_, drawCullCapacity_,
                                           drawCullCapacity_))
    return false;

  auto *entries = static_cast<renderer::GpuCompactBatch *>(compactBatchBuffer_.mappedData());
  for (usize index = 0; index < compactionBatchScratch_.size(); ++index)
    entries[index] = compactionBatchScratch_[index];
  // Sobras zeradas: um lote antigo aqui compactaria um intervalo que este frame
  // nao publicou, escrevendo por cima do destino de um lote real.
  for (u32 index = static_cast<u32>(compactionBatchScratch_.size());
       index < drawCompactionBatchCapacity_; ++index)
    entries[index] = renderer::GpuCompactBatch{};
  if (!memoryAllocator_->flushBuffer(compactBatchBuffer_)) return false;
  drawCompactionBatchCount_ = static_cast<u32>(compactionBatchScratch_.size());
  return true;
}

void InstancedRenderer::readDrawCullTelemetryFromPreviousFrame() {
  if (!hzbGpuCullingActive_ || !drawCullTelemetryBuffer_.isReady()) return;
  // Mesmo raciocinio de collectPrevious(): a espera de fence do acquire ja
  // garantiu que o dispatch do frame anterior terminou. Nenhum stall novo.
  if (!memoryAllocator_->invalidateBuffer(drawCullTelemetryBuffer_)) return;
  const auto *counters = static_cast<const u32 *>(drawCullTelemetryBuffer_.mappedData());
  if (counters == nullptr) return;
  for (u32 index = 0; index < 4; ++index) drawCullTelemetry_[index] = counters[index];
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
      // Untextured map materials multiply their authored color by neutral white.
      // Keep the checker solely for the standalone diagnostic cube.
      pixels[offset + 0] = dirtRoadPreview_ ? 255 : (light ? 220 : 24);
      pixels[offset + 1] = dirtRoadPreview_ ? 255 : (light ? 245 : 76);
      pixels[offset + 2] = dirtRoadPreview_ ? 255 : (light ? 245 : 86);
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
  if(!baseSampler_.initialize(device_, samplerDesc)) return false;
  if(waterSubpassActive_) {
    std::vector<u8> detail;
    if(!renderer::buildWaterDetailTexture(64,1,detail)) return false;
    imageDesc.width=imageDesc.height=64;imageDesc.mipLevels=7;
    imageDesc.format=VK_FORMAT_R8G8B8A8_UNORM;
    if(!memoryAllocator_->createImage(imageDesc,&waterDetailTexture_) ||
       !uploadContext_.uploadSampledMipChain(*memoryAllocator_,detail.data(),detail.size(),waterDetailTexture_)) return false;
    samplerDesc.minFilter=samplerDesc.magFilter=VK_FILTER_LINEAR;
    samplerDesc.mipmapMode=VK_SAMPLER_MIPMAP_MODE_LINEAR;samplerDesc.maxLod=6;
    samplerDesc.addressU=samplerDesc.addressV=VK_SAMPLER_ADDRESS_MODE_REPEAT;
    if(!waterDetailSampler_.initialize(device_,samplerDesc)) return false;
  }
  return true;
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
                                   const std::atomic<bool> *cancel, bool dirtRoadPreview,
                                   const char *mapAssetRoot, bool emptyScene) {
  if (instanceCount == 0 || (!dotNetHost.isReady() && !dirtRoadPreview && !emptyScene)) return false;
  if (emptyScene && (dirtRoadPreview || scenePreview)) return false;
  emptyScene_ = emptyScene;

  device_ = device.handle();
  physicalDevice_ = device.physicalDevice();
  rhiDevice_ = &device;
  useBindless_ = device.enabledPaths().bindless;
  memoryAllocator_ = &device.memoryAllocator();
  swapchain_ = &swapchain;
  dirtRoadPreview_ = dirtRoadPreview || emptyScene;
  materialPreview_ = materialAssets != nullptr && !dirtRoadPreview_ && !emptyScene_;
  scenePreview_ = !dirtRoadPreview_ && (scenePreview || materialPreview_);

  // The temporal shader reprojects from FreeCameraState and the matching
  // projection jitter is currently part of the packaged-world vertex path.
  // Other scene pipelines do not yet expose that camera-matrix contract, so
  // enabling history there would accumulate an unjittered/mismatched image.
  temporalAaActive_ =
      renderingPolicy_.post.antiAliasing == renderer::AntiAliasingMode::Temporal &&
      dirtRoadPreview_ && swapchain.supportsTransferSource();
  if (renderingPolicy_.post.antiAliasing == renderer::AntiAliasingMode::Temporal &&
      !temporalAaActive_) {
    // History is copied from the resolved swapchain image. Surfaces that do
    // not expose TRANSFER_SRC cannot execute that contract; degrade visibly
    // and explicitly to the universal spatial path.
    renderingPolicy_.post.antiAliasing = renderer::AntiAliasingMode::Fxaa;
    resourceRenderingPolicy_.post.antiAliasing = renderer::AntiAliasingMode::Fxaa;
    __android_log_print(
        ANDROID_LOG_WARN, LogTag,
        dirtRoadPreview_ ? "[TAA] swapchain sem TRANSFER_SRC; fallback FXAA ativo."
                         : "[TAA] pipeline sem contrato temporal de camera; fallback FXAA ativo.");
  }
  graphicsQueueFamily_ = device.graphicsQueueFamily();
  instanceCount_ = instanceCount;
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
  } else if (!dirtRoadPreview_ && !emptyScene_) {
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
    // A densidade da malha de água sai da política, não do cozimento. Vento
    // ainda vem do espectro; quando a cena passar a autorá-lo, é de lá que sai.
    const u32 waterGridSegments = renderer::selectWaterGrid(
        renderingPolicy_.geometry.waterMesh,
        renderer::WaterSpectrumSettings{}.windSpeed, 8000.0f).segments;
    if (emptyScene_) {
      if(!dirtRoadResources_.initializePrimitives(device,uploadContext_)) return false;
    } else if (!dirtRoadResources_.initialize(device, uploadContext_, materialAssets,
                                       forceTextureFallback,
                                       waterDisplacementCapacity_,
                                       cancel, mapAssetRoot,
                                       waterGridSegments, waterAuthoringEnabled_)) return false;
    if (!rebuildDrawOrders()) return false;
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
  if (!createShadowResources()) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar recursos de sombra direcional.");
    return false;
  }
  if (!createSpectralWaterResources()) return false;
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
  if (!createEditorGridPipeline()) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar o pipeline da grade editorial.");
    return false;
  }
  if (!createSkyPipeline()) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar o pipeline do céu HDRI.");
    return false;
  }
  if (!createPostResources()) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar o pós-processamento.");
    return false;
  }
  createUiRenderer(materialAssets);
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
                      "InstancedRenderer pronto: capacidade=%u, modo=%s.",
                      instanceCount_, emptyScene_ ? "empty-scene" : dirtRoadPreview_ ? "dirt-road" :
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
  // The UI owns VMA buffers and images too. Release them before its render pass
  // and before the surface destroys the allocator/device (including resume).
  uiRenderer_.shutdown();
  for(auto framebuffer:uiFramebuffers_) if(framebuffer) vkDestroyFramebuffer(device_,framebuffer,nullptr);
  uiFramebuffers_.clear();if(uiRenderPass_) vkDestroyRenderPass(device_,uiRenderPass_,nullptr);uiRenderPass_=VK_NULL_HANDLE;
  uiInstances_.clear();

  for(auto &cascade:*waterSpectralCompute_) cascade.shutdown();
  spectralWaterCount_=0; spectralWaterBoundsExpansion_=0; waterSpectrumRevision_=0;

  if (commandPool_ != VK_NULL_HANDLE) {
    vkDestroyCommandPool(device_, commandPool_, nullptr);
    commandPool_ = VK_NULL_HANDLE;
    commandBuffer_ = VK_NULL_HANDLE;
  }
  destroyFramebuffers();
  destroyHzbResources();
  destroyPostResources();
  destroyShadowResources();
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
  if (editorGridPipeline_ != VK_NULL_HANDLE) {
    vkDestroyPipeline(device_, editorGridPipeline_, nullptr);
    editorGridPipeline_ = VK_NULL_HANDLE;
  }
  if (editorGridPipelineLayout_ != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device_, editorGridPipelineLayout_, nullptr);
    editorGridPipelineLayout_ = VK_NULL_HANDLE;
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
  if (waterPipeline_ != VK_NULL_HANDLE) {
    vkDestroyPipeline(device_, waterPipeline_, nullptr);
    waterPipeline_ = VK_NULL_HANDLE;
  }
  if (opaqueDistantPipeline_ != VK_NULL_HANDLE) {
    vkDestroyPipeline(device_, opaqueDistantPipeline_, nullptr);
    opaqueDistantPipeline_ = VK_NULL_HANDLE;
  }
  if (coverageDistantPipeline_ != VK_NULL_HANDLE) {
    vkDestroyPipeline(device_, coverageDistantPipeline_, nullptr);
    coverageDistantPipeline_ = VK_NULL_HANDLE;
  }
  if (transparentDistantPipeline_ != VK_NULL_HANDLE) {
    vkDestroyPipeline(device_, transparentDistantPipeline_, nullptr);
    transparentDistantPipeline_ = VK_NULL_HANDLE;
  }
  for (u32 variant = 0; variant < renderer::MaterialFeatureVariantCount; ++variant) {
    if (opaqueMaterialPipelines_[variant] != VK_NULL_HANDLE)
      vkDestroyPipeline(device_, opaqueMaterialPipelines_[variant], nullptr);
    if (coverageMaterialPipelines_[variant] != VK_NULL_HANDLE)
      vkDestroyPipeline(device_, coverageMaterialPipelines_[variant], nullptr);
    if (transparentMaterialPipelines_[variant] != VK_NULL_HANDLE)
      vkDestroyPipeline(device_, transparentMaterialPipelines_[variant], nullptr);
    opaqueMaterialPipelines_[variant] = VK_NULL_HANDLE;
    coverageMaterialPipelines_[variant] = VK_NULL_HANDLE;
    transparentMaterialPipelines_[variant] = VK_NULL_HANDLE;
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
  waterRippleBuffer_.reset();
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
  waterDrawOrder_.clear();
  visibleSolidDrawOrder_.clear();
  visibleCoverageDrawOrder_.clear();
  visibleTransparentDrawOrder_.clear();
  waterSubpassActive_ = false;
  cameraWaterHorizonFillActive_ = false;
  visibilityTelemetry_ = {};
  renderedFrameCount_ = 0;
  baseSampler_.shutdown();
  baseTexture_.reset();
  waterDetailSampler_.shutdown();waterDetailTexture_.reset();
  routeVertices_.reset();routeIndices_.reset();authoredWaterLayers_.clear();authoredWaterFlowDepth_.clear();authoredWaterTime_=-1;
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
  pendingScene_.clear();sourceMapDraws_.clear();authoredVisibility_.clear();authoredShadows_.clear();authoredMaterials_.clear();
  fillInstanceBuffer_ = nullptr;
  extractScene_ = nullptr;
  dirtRoadPreview_ = false;
  emptyScene_ = false;
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

// A montagem que depende da lista de desenhos do pacote: ordens por tipo de
// material, grupos de LOD e elegibilidade de HZB. Extraida de `initialize`
// porque a importacao de um modelo no aparelho acrescenta desenhos ao pacote --
// e refazer isso pela metade deixaria um desenho novo fora de toda fila,
// invisivel sem nenhum erro.
bool InstancedRenderer::rebuildAuthoringGeometry(std::span<const u8> vertices, std::span<const u32> indices,
                                                std::span<const renderer::MapDrawRecord> draws,
                                                std::span<const renderer::MapMaterialRecord> materials) {
  if(!dirtRoadPreview_ || !rhiDevice_ || device_==VK_NULL_HANDLE) {
    __android_log_print(ANDROID_LOG_ERROR,LogTag,"[Import] renderer sem contexto para absorver geometria.");
    return false;
  }
  // Trocar buffers de vértice com um quadro em voo desenharia memória já
  // liberada. Importar é raro e explícito: esperar é a resposta certa aqui, e
  // não uma fila de destruição diferida que ninguém mais no renderer usa.
  vkDeviceWaitIdle(device_);
  if(!dirtRoadResources_.rebuildAuthoringLibrary(*rhiDevice_,uploadContext_,vertices,indices,draws,materials)) {
    __android_log_print(ANDROID_LOG_ERROR,LogTag,
        "[Import] biblioteca recusada: vertices=%zu indices=%zu desenhos=%zu materiais=%zu",
        vertices.size()/renderer::MapVertexStride,indices.size(),draws.size(),materials.size());
    return false;
  }
  // A cena publicada descreve a lista ANTERIOR de desenhos. Descartá-la obriga
  // o próximo quadro a republicar tudo, em vez de casar poses novas com
  // topologia velha.
  pendingScene_.clear();pendingMapPoseCount_=0;pendingAuthoredStateValid_=false;
  authoredMaterials_.clear();authoredVisibility_.clear();authoredShadows_.clear();
  authoredWaterLayers_.clear();authoredWaterFlowDepth_.clear();
  shadowCascadeDirtyMask_=0xffffffffu;
  if(!rebuildDrawOrders()) {
    __android_log_print(ANDROID_LOG_ERROR,LogTag,"[Import] filas de desenho recusaram o pacote novo.");
    return false;
  }
  // Os buffers antigos precisam sair antes: o alocador não sobrescreve uma
  // alça viva, e sem isto a recriação falharia com o pacote já publicado.
  instanceBuffer_.reset();
  indirectBuffer_.reset();
  if(!createInstanceBuffer()) {
    __android_log_print(ANDROID_LOG_ERROR,LogTag,"[Import] buffer de instancias recusado para %u desenhos.",instanceCount_);
    return false;
  }
  // Os descritores de culling e compactação apontavam para o buffer indireto
  // que acabou de ser substituído. Recriá-los é obrigatório; deixá-los velhos
  // faria a GPU escrever contagens de desenho em memória liberada.
  if(!createDrawCullResources() || !createDrawCompactionResources()) {
    __android_log_print(ANDROID_LOG_ERROR,LogTag,"[Import] recursos de culling recusaram o pacote novo.");
    return false;
  }
  __android_log_print(ANDROID_LOG_INFO,LogTag,"[Import] pacote absorvido: %u desenhos, %zu vertices.",
      instanceCount_,dirtRoadResources_.pickingVertices().size()/renderer::MapVertexStride);
  return true;
}

bool InstancedRenderer::rebuildDrawOrders() {
  solidDrawOrder_.clear();coverageDrawOrder_.clear();transparentDrawOrder_.clear();waterDrawOrder_.clear();
  lodGroups_.clear();coverageLodGroups_.clear();
  ungroupedSolidDrawOrder_.clear();ungroupedCoverageDrawOrder_.clear();
  levelZeroSolidDrawOrder_.clear();levelZeroCoverageDrawOrder_.clear();
  cameraWaterHorizonFillActive_=false;
  sourceMapDraws_=dirtRoadResources_.draws();
  if(emptyScene_) { authoredVisibility_.assign(sourceMapDraws_.size(),0);authoredShadows_.assign(sourceMapDraws_.size(),0); }
  instanceCount_ = static_cast<u32>(dirtRoadResources_.draws().size());
  for (u32 index = 0; index < instanceCount_; ++index) {
    const u32 material = dirtRoadResources_.draws()[index].materialIndex;
    const u32 flags = dirtRoadResources_.materials()[material].flags;
    // Um impostor descartado aqui some da cadeia inteira: sem draw nas filas,
    // buildLodRenderGroups nunca ve o nivel e o grupo termina no nivel
    // simplificado anterior. E o controle do A/B, nao um caminho de qualidade.
    if (!foliageImpostors_ && (flags & renderer::MapMaterialImpostor) != 0) continue;
    if ((flags & renderer::MapMaterialWater) != 0) {
      waterDrawOrder_.push_back(index);
      const u32 cameraWater = renderer::MapMaterialWater |
                              renderer::MapMaterialWaterCameraGrid;
      cameraWaterHorizonFillActive_ |= (flags & cameraWater) == cameraWater;
    } else if ((flags & renderer::MapMaterialBlend) != 0)
      transparentDrawOrder_.push_back(index);
    else if ((flags & renderer::MapMaterialAlphaMask) != 0)
      coverageDrawOrder_.push_back(index);
    else
      solidDrawOrder_.push_back(index);
  }
  waterSubpassActive_ = !waterDrawOrder_.empty();
  visibleSolidDrawOrder_.reserve(solidDrawOrder_.size());
  visibleCoverageDrawOrder_.reserve(coverageDrawOrder_.size());
  visibleTransparentDrawOrder_.reserve(transparentDrawOrder_.size() + waterDrawOrder_.size());
  // LOD groups (see renderer::selectLodLevel): spatial chunking happens
  // after import and can produce MANY draws for each imported LOD level.
  // Bucket by (lodGroupId,lodLevel); treating group.size() as level count
  // would select arbitrary level-0 chunks and silently drop the rest.
  if (!renderer::buildLodRenderGroups(dirtRoadResources_.draws(), solidDrawOrder_,
                                      lodGroups_, ungroupedSolidDrawOrder_)) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "[LOD] grupos opacos de runtime inválidos.");
    return false;
  }
  if (!renderer::buildLodRenderGroups(dirtRoadResources_.draws(), coverageDrawOrder_,
                                      coverageLodGroups_, ungroupedCoverageDrawOrder_)) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "[LOD] grupos alpha-test de runtime inválidos.");
    return false;
  }
  renderer::buildLodLevelZeroDrawOrder(lodGroups_, ungroupedSolidDrawOrder_,
                                       levelZeroSolidDrawOrder_);
  renderer::buildLodLevelZeroDrawOrder(coverageLodGroups_, ungroupedCoverageDrawOrder_,
                                       levelZeroCoverageDrawOrder_);
  // Every frame contributes chunks for one active level and optionally
  // one neighbor, still a subset of all levels already in solidDrawOrder_.
  lodFilteredSolidDrawOrder_.reserve(solidDrawOrder_.size());
  lodFilteredCoverageDrawOrder_.reserve(coverageDrawOrder_.size());
  const u32 maximumHzbCandidates = static_cast<u32>(
      solidDrawOrder_.size() + coverageDrawOrder_.size());
  hzbWorkloadEligible_ = (hzbOcclusionEnabled_ || hzbComputeEnabled_) &&
      !renderingPolicy_.dynamicResolution.enabled &&
      renderer::shouldRunHzb(maximumHzbCandidates, hzbMinimumCandidateDraws_);
  if ((hzbOcclusionEnabled_ || hzbComputeEnabled_) && !hzbWorkloadEligible_) {
    __android_log_print(ANDROID_LOG_INFO, LogTag,
        "[HZB] solicitado, mas dispensado antes da alocação: candidatos máximos=%u limiar=%u.",
        maximumHzbCandidates, hzbMinimumCandidateDraws_);
  }

  return true;
}

bool InstancedRenderer::queueMapScene(std::span<const renderer::MapDrawState> draws) {
  if (draws.size() < sourceMapDraws_.size() || draws.size()>65536) return false;
  for (u32 i=0; i<draws.size(); ++i) {
    if(draws[i].sourceDrawIndex>=sourceMapDraws_.size()) return false;
    const auto &a=draws[i].pose.draw;const auto &b=sourceMapDraws_[draws[i].sourceDrawIndex];
    if(draws[i].pose.drawIndex!=i || a.firstIndex!=b.firstIndex || a.indexCount!=b.indexCount ||
       a.vertexOffset!=b.vertexOffset || a.materialIndex!=b.materialIndex) return false;
  }
  pendingScene_.assign(draws.begin(),draws.end());
  return true;
}

bool InstancedRenderer::queueAuthoredPoses(std::span<const renderer::MapDrawState> draws) {
  const auto &current=dirtRoadResources_.draws();
  if(draws.size()!=current.size()) return false;
  u32 count=0;
  for(u32 i=0;i<draws.size();++i) {
    if(std::memcmp(draws[i].pose.draw.model,current[i].model,sizeof(current[i].model))==0) continue;
    if(count==pendingMapPoses_.size() || draws[i].route) return false;
    auto update=draws[i].pose;
    // Topology remains owned by the committed scene, including generated meshes.
    update.draw.firstIndex=current[i].firstIndex;update.draw.indexCount=current[i].indexCount;
    update.draw.vertexOffset=current[i].vertexOffset;update.draw.materialIndex=current[i].materialIndex;
    pendingMapPoses_[count++]=update;
  }
  // Material, visibilidade e sombra não são pose e mudam sem que a hierarquia
  // mude. Durante o Play o documento autoral não é escrito, então o caminho de
  // "só poses" é o ÚNICO que roda: sem isto, um script que escreve `base_color`
  // altera o componente e a tela nunca muda. Transacional como o resto: só
  // publica depois de todo o lote ter sido aceito.
  pendingAuthoredState_.resize(draws.size());
  for(u32 i=0;i<draws.size();++i) {
    auto &state=pendingAuthoredState_[i];
    state.material=draws[i].material;
    state.visible=draws[i].visible;
    state.castShadow=draws[i].castShadow;
  }
  pendingAuthoredStateValid_=true;
  pendingMapPoseCount_=count;return true;
}

bool InstancedRenderer::commitAuthoredScene() {
  const u32 count=static_cast<u32>(pendingScene_.size());
  struct RouteVertex {float position[3];i16 normal[4],tangent[4];float uv[2],flow[2];u8 color[4];};
  static_assert(sizeof(RouteVertex)==renderer::MapVertexStride);
  std::vector<RouteVertex> routeVertices;std::vector<u32> routeIndices;
  for(auto &state:pendingScene_) if(state.route) {
    std::vector<renderer::WaterRouteVertex> geometry;std::vector<u32> topology;
    if(!renderer::buildWaterRouteMesh(*state.route,geometry,topology)) return false;
    auto &draw=state.pose.draw;draw.firstIndex=static_cast<u32>(routeIndices.size());
    draw.vertexOffset=static_cast<u32>(routeVertices.size());draw.indexCount=static_cast<u32>(topology.size());
    for(const auto &source:geometry) {
      RouteVertex vertex{};std::copy(source.position,source.position+3,vertex.position);
      for(u32 axis=0;axis<3;++axis) vertex.normal[axis]=static_cast<i16>(std::clamp(source.normal[axis],-1.0f,1.0f)*32767);
      vertex.normal[3]=static_cast<i16>(std::clamp(source.spacing/512,0.0f,1.0f)*32767);
      vertex.tangent[0]=vertex.tangent[3]=32767;std::copy(source.uv,source.uv+2,vertex.uv);
      std::copy(source.flow,source.flow+2,vertex.flow);
      vertex.color[0]=static_cast<u8>(std::clamp(source.foam/10,0.0f,1.0f)*255);
      vertex.color[1]=vertex.color[2]=vertex.color[3]=255;routeVertices.push_back(vertex);
    }
    routeIndices.insert(routeIndices.end(),topology.begin(),topology.end());
  }
  rhi::VulkanBuffer nextVertices,nextIndices;
  if(!routeVertices.empty()) {
    rhi::BufferDesc desc{};desc.cpuAccess=rhi::CpuAccess::SequentialWrite;desc.preferDeviceMemory=false;
    desc.usage=VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;desc.sizeBytes=routeVertices.size()*sizeof(RouteVertex);
    if(!memoryAllocator_->createBuffer(desc,&nextVertices) || !nextVertices.mappedData()) return false;
    std::memcpy(nextVertices.mappedData(),routeVertices.data(),desc.sizeBytes);
    desc.usage=VK_BUFFER_USAGE_INDEX_BUFFER_BIT;desc.sizeBytes=routeIndices.size()*sizeof(u32);
    if(!memoryAllocator_->createBuffer(desc,&nextIndices) || !nextIndices.mappedData()) return false;
    std::memcpy(nextIndices.mappedData(),routeIndices.data(),desc.sizeBytes);
    if(!memoryAllocator_->flushBuffer(nextVertices) || !memoryAllocator_->flushBuffer(nextIndices)) return false;
  }
  if(count!=instanceCount_) {
    rhi::BufferDesc desc{};desc.sizeBytes=static_cast<u64>(count)*sizeof(renderer::GpuMeshInstance);
    desc.usage=VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;desc.memoryClass=rhi::MemoryClass::Buffer;
    desc.cpuAccess=rhi::CpuAccess::SequentialWrite;desc.preferDeviceMemory=false;
    rhi::VulkanBuffer replacement;
    if(!memoryAllocator_->createBuffer(desc,&replacement) || !replacement.mappedData()) return false;
    // Acquired frame fence has retired every previous read of these buffers.
    destroyDrawCullResources();
    instanceBuffer_=std::move(replacement);
    useMultiDrawIndirect_=false;
    instanceCount_=count;
    hzbHysteresis_.assign(count,{});dynamicMapDraws_.assign(count,1);
  }
  auto *instances=static_cast<renderer::GpuMeshInstance *>(instanceBuffer_.mappedData());
  if(!instances) return false;
  std::vector<renderer::MapDrawRecord> records;records.reserve(count);
  authoredVisibility_.resize(count);authoredShadows_.resize(count);authoredMaterials_.resize(count);
  routeVertices_=std::move(nextVertices);routeIndices_=std::move(nextIndices);authoredWaterLayers_.resize(count);
  authoredWaterFlowDepth_.resize(count);
  // Scalar per-instance overrides require individual push constants.
  useMultiDrawIndirect_=false;
  solidDrawOrder_.clear();coverageDrawOrder_.clear();transparentDrawOrder_.clear();waterDrawOrder_.clear();
  lodGroups_.clear();coverageLodGroups_.clear();
  cameraWaterHorizonFillActive_=false;
  for(u32 i=0;i<count;++i) {
    const auto &state=pendingScene_[i];records.push_back(state.pose.draw);instances[i]=state.pose.instance;
    authoredVisibility_[i]=state.visible;authoredShadows_[i]=state.castShadow;authoredMaterials_[i]=state.material;
    std::copy(state.waterLayers,state.waterLayers+4,authoredWaterLayers_[i].begin());
    std::copy(state.waterFlowDepth,state.waterFlowDepth+4,authoredWaterFlowDepth_[i].begin());
    dynamicMapDraws_[i]=1;hzbHysteresis_[i]={};
    // Authored view uses LOD0; derived package bounds are no longer a valid LOD selection cache.
    if(state.pose.draw.lodLevel!=0) continue;
    const auto flags=dirtRoadResources_.materials()[state.pose.draw.materialIndex].flags;
    if(state.visible && (flags & renderer::MapMaterialWaterCameraGrid)) cameraWaterHorizonFillActive_=true;
    if(flags & renderer::MapMaterialWater) waterDrawOrder_.push_back(i);
    else if(flags & renderer::MapMaterialBlend) transparentDrawOrder_.push_back(i);
    else if(flags & renderer::MapMaterialAlphaMask) coverageDrawOrder_.push_back(i);
    else solidDrawOrder_.push_back(i);
  }
  dirtRoadResources_.setAuthoredDraws(std::move(records));
  if(!memoryAllocator_->flushBuffer(instanceBuffer_)) return false;
  pendingScene_.clear();shadowCascadeDirtyMask_=0xffffffffu;
  return true;
}

bool InstancedRenderer::queueMapDrawPose(u32 drawIndex, const float *model,
                                        const float *localCenter, float localRadius) {
  if (!dirtRoadPreview_ || drawIndex >= dirtRoadResources_.draws().size() ||
      drawIndex >= dynamicMapDraws_.size()) return false;
  const auto &draw = dirtRoadResources_.draws()[drawIndex];
  if ((dirtRoadResources_.materials()[draw.materialIndex].flags & renderer::MapMaterialWater) != 0)
    return false;
  auto belongsToLod = [&](const auto &groups) {
    for (const auto &group : groups) if (group.levelCount > 1)
      for (u32 level = 0; level < group.levelCount; ++level)
        for (u32 index : group.levels[level].drawIndices) if (index == drawIndex) return true;
    return false;
  };
  if (belongsToLod(lodGroups_) || belongsToLod(coverageLodGroups_)) return false;
  renderer::MapDrawUpdate update;
  if (!renderer::prepareMapDrawUpdate(drawIndex, draw, model, localCenter, localRadius, update)) return false;
  for (u32 i = 0; i < pendingMapPoseCount_; ++i) if (pendingMapPoses_[i].drawIndex == drawIndex) {
    pendingMapPoses_[i] = update;
    return true;
  }
  if (pendingMapPoseCount_ == pendingMapPoses_.size()) return false;
  pendingMapPoses_[pendingMapPoseCount_++] = update;
  return true;
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
      const renderer::DynamicResolutionUpdate resolution =
          dynamicResolution_.observe(static_cast<float>(gpuTimings.frameMs));
      if (resolution.changed) {
        hzbPyramidValid_ = false;
        hzbPyramidCameraValid_ = false;
        temporalHistoryInitialized_ = false;
        __android_log_print(ANDROID_LOG_INFO, LogTag,
            "[DynamicResolution] scale=%.3f render=%ux%u gpu=%.3fms budget=%.3fms.",
            static_cast<double>(resolution.scale), renderWidth(), renderHeight(),
            gpuTimings.frameMs, static_cast<double>(dynamicResolution_.targetGpuMilliseconds()));
      }
    }
  }
  // Same "safe without a new stall" reasoning as collectPrevious() above --
  // see readHzbPyramidFromPreviousFrame()'s own comment.
  readDrawCullTelemetryFromPreviousFrame();
  const bool mapPosesChanged = pendingMapPoseCount_ != 0 || !pendingScene_.empty();
  if (!pendingScene_.empty() && !commitAuthoredScene()) return rhi::SwapchainStatus::FatalError;
  if (pendingAuthoredStateValid_) {
    // `commitAuthoredScene` já escreve estes vetores a partir de pendingScene_;
    // aqui é o caminho de Play, em que a cena publicada não mudou.
    const u32 states = static_cast<u32>(
        std::min<usize>(pendingAuthoredState_.size(), authoredMaterials_.size()));
    for (u32 i = 0; i < states; ++i) {
      const auto &state = pendingAuthoredState_[i];
      if (!(authoredMaterials_[i] == state.material) || authoredVisibility_[i] != state.visible ||
          authoredShadows_[i] != state.castShadow)
        shadowCascadeDirtyMask_ = 0xffffffffu;
      authoredMaterials_[i] = state.material;
      authoredVisibility_[i] = state.visible;
      authoredShadows_[i] = state.castShadow;
    }
    pendingAuthoredStateValid_ = false;
  }
  if (pendingMapPoseCount_ != 0) {
    auto *instances = static_cast<renderer::GpuMeshInstance *>(instanceBuffer_.mappedData());
    if (instances == nullptr) return rhi::SwapchainStatus::FatalError;
    for (u32 i = 0; i < pendingMapPoseCount_; ++i) {
      const auto &update = pendingMapPoses_[i];
      if (!dirtRoadResources_.updateDrawPose(update.drawIndex, update.draw))
        return rhi::SwapchainStatus::FatalError;
      instances[update.drawIndex] = update.instance;
      dynamicMapDraws_[update.drawIndex] = 1;
      hzbHysteresis_[update.drawIndex] = {};
    }
    if (!memoryAllocator_->flushBuffer(instanceBuffer_)) return rhi::SwapchainStatus::FatalError;
    pendingMapPoseCount_ = 0;
    shadowCascadeDirtyMask_ = 0xffffffffu;
  }
  if (hzbPreviousFrameEligible_) readHzbPyramidFromPreviousFrame();
  else {
    hzbPyramidValid_ = false;
    hzbPyramidCameraValid_ = false;
  }
  const float projection[]{sceneFieldOfView(),sceneNearPlane(),sceneFarPlane(),sceneAspectRatio()};
  const bool projectionChanged=!std::equal(std::begin(projection),std::end(projection),previousSceneProjection_);
  std::copy(std::begin(projection),std::end(projection),previousSceneProjection_);
  if(projectionChanged) {
    // A depth/history image from another projection cannot occlude or reproject
    // this frame even when the camera position has not moved.
    hzbPyramidValid_=false;hzbPyramidCameraValid_=false;hzbRecordedCameraValid_=false;
    temporalHistoryInitialized_=false;
    for(auto &state:hzbHysteresis_) state={};
    invalidateStaticShadowCache();
  }
  hzbFrameEligible_ = false;
  if (mapPosesChanged) {
    // Old occluders can hide OTHER draws at their previous position. Discard
    // that frame's history, not only the moving draw's hysteresis.
    hzbPyramidValid_ = false;
    hzbPyramidCameraValid_ = false;
    hzbRecordedCameraValid_ = false;
    // No per-object velocity buffer in this path yet: old temporal color
    // would ghost behind moving objects. Fail open until motion is available.
    temporalHistoryInitialized_ = false;
  }
  hzbReadbackRecordedThisFrame_ = false;
  if (temporalAaActive_) {
    const u64 sample = temporalFrameIndex_ % 8u + 1u;
    temporalCurrentJitter_[0] =
        (halton(sample, 2u) - 0.5f) * 2.0f / static_cast<float>(renderWidth());
    temporalCurrentJitter_[1] =
        (halton(sample, 3u) - 0.5f) * 2.0f / static_cast<float>(renderHeight());
  } else {
    temporalCurrentJitter_[0] = temporalCurrentJitter_[1] = 0.0f;
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
  if (dirtRoadPreview_) {
    auto *frame = static_cast<DirtRoadFrameUniform *>(environmentUniform_.mappedData());
    if(!renderer::adjustEnvironmentLighting(dirtRoadResources_.environmentLighting(),environmentAdjustment_,frame->environment))
      return rhi::SwapchainStatus::FatalError;
    const float cy = std::cos(camera.yaw), sy = std::sin(camera.yaw);
    const float cp = std::cos(camera.pitch), sp = std::sin(camera.pitch);
    const float row0[4] = {cy, 0.0f, -sy, 0.0f};
    const float row1[4] = {sy * sp, cp, cy * sp, 0.0f};
    const float row2[4] = {sy * cp, -sp, cy * cp, 0.0f};
    std::memcpy(frame->worldToViewRow0, row0, sizeof(row0));
    std::memcpy(frame->worldToViewRow1, row1, sizeof(row1));
    std::memcpy(frame->worldToViewRow2, row2, sizeof(row2));
    // Estes eixos podem mudar em runtime por pressão térmica. Atualizar o UBO
    // já mapeado evita recriar descritores/pipelines e mantém custo O(1).
    frame->quality[0] = renderingPolicy_.materialDistance.normalMapMaximumDistance;
    frame->quality[1] = renderingPolicy_.materialDistance.specularProbeMaximumDistance;
    frame->quality[2] = renderingPolicy_.ambient.hemispheric ? 1.0f : 0.0f;
    frame->quality[3] = renderingPolicy_.ambient.specularProbe ? 1.0f : 0.0f;
    if (frame->environment.parameters[3] > 0.5f)
      frame->environment.parameters[3] = renderingPolicy_.ambient.splitSumBrdf ? 3.0f : 1.0f;
    frame->materialDistanceParameters[0] =
        renderingPolicy_.materialDistance.metallicRoughnessMaximumDistance;
    frame->materialDistanceParameters[1] =
        renderingPolicy_.materialDistance.emissiveMaximumDistance;
    frame->materialDistanceParameters[2] =
        renderingPolicy_.materialDistance.fadeBandRatio;
    frame->waterParameters[0] = static_cast<float>(waterProfile_.waveCount);
    frame->waterParameters[1] = waterBaseHeight_;
    frame->waterParameters[2] = authoredWaterTime_>=0?authoredWaterTime_:timeSeconds;
    frame->waterParameters[3] = waterProfile_.microWaveStrength;
    frame->waterOptics[0] = waterProfile_.refractiveIndex;
    frame->waterOptics[1] = waterProfile_.roughness;
    frame->waterOptics[2] = waterProfile_.turbidity;
    frame->waterOptics[3] = waterProfile_.foamThreshold;
    frame->waterDeepColorFoam[0] = waterProfile_.deepColor.x;
    frame->waterDeepColorFoam[1] = waterProfile_.deepColor.y;
    frame->waterDeepColorFoam[2] = waterProfile_.deepColor.z;
    frame->waterDeepColorFoam[3] = waterProfile_.foamDecay;
    frame->waterShallowColorDistance[0] = waterProfile_.shallowColor.x;
    frame->waterShallowColorDistance[1] = waterProfile_.shallowColor.y;
    frame->waterShallowColorDistance[2] = waterProfile_.shallowColor.z;
    frame->waterShallowColorDistance[3] = waterProfile_.maximumDistance;
    frame->waterAbsorption[0] = waterProfile_.absorption.x;
    frame->waterAbsorption[1] = waterProfile_.absorption.y;
    frame->waterAbsorption[2] = waterProfile_.absorption.z;
    frame->waterAbsorption[3] = waterProfile_.surfaceOpacity;
    for (u32 waveIndex = 0; waveIndex < renderer::MaximumWaterWaves; ++waveIndex) {
      const auto &wave = waterProfile_.waves[waveIndex];
      frame->waterWaveShape[waveIndex][0] = wave.direction.x;
      frame->waterWaveShape[waveIndex][1] = wave.direction.y;
      frame->waterWaveShape[waveIndex][2] = wave.amplitude;
      frame->waterWaveShape[waveIndex][3] = 6.28318530718f / wave.wavelength;
      frame->waterWaveMotion[waveIndex][0] = wave.speed;
      frame->waterWaveMotion[waveIndex][1] = wave.steepness;
      frame->waterWaveMotion[waveIndex][2] = wave.phase;
    }
    const auto &waterImpulses = waterInteractions_.impulses();
    if(spectralWaterCount_>0) {
      frame->waterParameters[0]=static_cast<float>(spectralWaterCount_);
      for(u32 i=0;i<spectralWaterCount_;++i) {
        const auto &c=waterCascadeSettings_[i];
        frame->waterWaveShape[i][0]=static_cast<float>(c.spectrum.resolution);
        frame->waterWaveShape[i][1]=c.spectrum.patchLength;
        frame->waterWaveShape[i][2]=c.displacementScale*waterSpectralControls_.displacement;
        frame->waterWaveShape[i][3]=c.choppiness*waterSpectralControls_.choppiness;
        frame->waterWaveMotion[i][0]=c.spectrum.minimumWavelength;
        frame->waterWaveMotion[i][1]=std::cos(waterSpectralControls_.directionRadians);
        frame->waterWaveMotion[i][2]=std::sin(waterSpectralControls_.directionRadians);
      }
    }
    u32 activeWaterInteractionCount = 0;
    for (u32 index = 0; index < renderer::MaximumWaterInteractions; ++index) {
      const auto &impulse = waterImpulses[index];
      const float age = timeSeconds - impulse.startTime;
      if (impulse.amplitude == 0.0f || age < 0.0f || age > impulse.duration) {
        continue;
      }
      const u32 packedIndex = activeWaterInteractionCount++;
      frame->waterInteractionShape[packedIndex][0] = impulse.center.x;
      frame->waterInteractionShape[packedIndex][1] = impulse.center.y;
      frame->waterInteractionShape[packedIndex][2] = impulse.startTime;
      frame->waterInteractionShape[packedIndex][3] = impulse.amplitude;
      frame->waterInteractionMotion[packedIndex][0] = impulse.wavelength;
      frame->waterInteractionMotion[packedIndex][1] = impulse.speed;
      frame->waterInteractionMotion[packedIndex][2] = impulse.decay;
      frame->waterInteractionMotion[packedIndex][3] = impulse.duration;
    }
    // Ganho zero apaga a resolução também: o vértice testa um número só antes
    // de decidir ler o buffer, e deixar resolução sem ganho o faria ler dados
    // que ninguém prometeu estar atualizados.
    frame->waterRippleArea[0] = waterRippleCentre_[0];
    frame->waterRippleArea[1] = waterRippleCentre_[1];
    frame->waterRippleArea[2] = waterRippleArea_;
    frame->waterRippleArea[3] = waterRippleGain_ > 0.0f
        ? static_cast<float>(waterRippleResolution_) : 0.0f;
    // Luzes da cena: a escolha do orçamento acontece aqui, com a câmera deste
    // quadro, e o que não coube fica contado em `lightBudget_`.
    {
      const u32 accepted = renderer::selectPunctualLights(
          sceneLights_, camera.position, frame->punctualLights, lightBudget_);
      frame->punctualLightParameters[0] = static_cast<float>(accepted);
      for (u32 i = accepted; i < renderer::MaximumPunctualLights; ++i)
        frame->punctualLights[i] = {};
      // Uma direcional autorada na cena passa a ser o sol: ela é a modalidade
      // que já tem consumidor com cascatas de sombra. Sem nenhuma, o sol do
      // recurso de ambiente continua valendo, como antes deste caminho existir.
      if (const auto *sun = renderer::selectDirectionalLight(sceneLights_)) {
        float direction[3];
        renderer::detail::normalized(sun->direction, direction);
        // `sunDirectionIntensity.xyz` é a direção PARA a luz, que é o oposto da
        // direção de emissão do objeto.
        for (u32 axis = 0; axis < 3; ++axis)
          frame->environment.sunDirectionIntensity[axis] = -direction[axis];
        frame->environment.sunDirectionIntensity[3] = sun->intensity;
        for (u32 axis = 0; axis < 3; ++axis)
          frame->environment.sunColorAngularRadius[axis] = sun->color[axis];
      }
    }
    frame->waterSurfaceDetail[0] = waterShading_.foamElevation;
    frame->waterSurfaceDetail[1] = waterShading_.foamCoverage;
    frame->waterSurfaceDetail[2] = waterShading_.microDisplacement;
    frame->waterSurfaceDetail[3] = waterShading_.microWavelength;
    frame->waterInteractionParameters[0] = static_cast<float>(activeWaterInteractionCount);
    frame->waterInteractionParameters[1] = static_cast<float>(waterCostIsolation_);
    frame->waterInteractionParameters[2] = waterShading_.specularAntialiasing;
    frame->waterInteractionParameters[3] = waterShading_.contactFoamWidth;
    frame->shadowParameters[0] = shadowAtlas_.width() > 0
                                     ? 1.0f / static_cast<float>(shadowAtlas_.width()) : 1.0f;
    frame->shadowParameters[1] = 0.0f;
    frame->shadowParameters[3] = renderingPolicy_.shadows.normalOffsetTexels;
    frame->shadowFilterParameters[0] = renderingPolicy_.shadows.filterTaps >= 25u ? 2.0f
                                           : renderingPolicy_.shadows.filterTaps >= 9u ? 1.0f
                                                                                        : 0.0f;
    frame->shadowFilterParameters[1] = renderingPolicy_.shadows.farFilterTaps >= 25u ? 2.0f
                                          : renderingPolicy_.shadows.farFilterTaps >= 9u ? 1.0f
                                                                                       : 0.0f;
    frame->shadowFilterParameters[2] = temporalCurrentJitter_[0];
    frame->shadowFilterParameters[3] = temporalCurrentJitter_[1];
    frame->shadowTransitionParameters[0] = renderingPolicy_.shadows.cascadeBlendRatio;
    frame->shadowTransitionParameters[1] = renderingPolicy_.shadows.distanceFadeRatio;
    frame->shadowTransitionParameters[2] = 1.0f/std::tan(sceneFieldOfView()*.5f);
    const u32 previousShadowCascadeCount = shadowCascadeCount_;
    shadowCascadeCount_ = 0;
    if (renderingPolicy_.shadows.enabled) {
      renderer::ShadowCascadeInput input{};
      std::memcpy(input.cameraPosition, camera.position, sizeof(input.cameraPosition));
      std::memcpy(input.cameraForward, row2, sizeof(input.cameraForward));
      std::memcpy(input.cameraUp, row1, sizeof(input.cameraUp));
      input.aspectRatio = sceneAspectRatio();
      input.verticalFovRadians = sceneFieldOfView();
      input.nearPlane = sceneNearPlane();
      input.shadowDistance = std::min(renderingPolicy_.shadows.maximumDistance,
                                      sceneFarPlane());
      const auto &environment = frame->environment;
      input.lightDirection[0] = -environment.sunDirectionIntensity[0];
      input.lightDirection[1] = -environment.sunDirectionIntensity[1];
      input.lightDirection[2] = -environment.sunDirectionIntensity[2];
      input.casterExtrusion = input.shadowDistance;
      input.cascadeResolution = renderingPolicy_.shadows.cascadeResolution;
      input.receiverGuardBandRatio = renderingPolicy_.shadows.cacheGuardBandRatio;
      input.cascadeBlendRatio = renderingPolicy_.shadows.cascadeBlendRatio;
      renderer::ShadowCascade desiredCascades[renderer::MaximumShadowCascades]{};
      const u32 desiredCascadeCount = renderer::computeShadowCascades(
          input, renderingPolicy_.shadows.cascadeCount, renderer::DefaultCascadeSplitLambda,
          desiredCascades);

      const bool topologyChanged = desiredCascadeCount != previousShadowCascadeCount;
      if (topologyChanged) invalidateStaticShadowCache();
      const u32 activeMask = desiredCascadeCount == 0 ? 0u : (1u << desiredCascadeCount) - 1u;
      if (!renderingPolicy_.shadows.staticCasterCache || !shadowCacheInitialized_ ||
          topologyChanged) {
        std::memcpy(shadowCascades_, desiredCascades,
                    sizeof(renderer::ShadowCascade) * desiredCascadeCount);
        shadowCascadeDirtyMask_ |= activeMask;
      } else {
        for (u32 cascade = 0; cascade < desiredCascadeCount; ++cascade) {
          if (!renderer::canReuseStaticShadowCascade(
                  shadowCascades_[cascade], desiredCascades[cascade],
                  renderingPolicy_.shadows.cacheGuardBandRatio)) {
            shadowCascades_[cascade] = desiredCascades[cascade];
            shadowCascadeDirtyMask_ |= 1u << cascade;
          }
        }
      }
      shadowCascadeCount_ = desiredCascadeCount;
      for (u32 cascade = 0; cascade < shadowCascadeCount_; ++cascade) {
        std::memcpy(frame->shadowViewProjection[cascade], shadowCascades_[cascade].viewProjection,
                    sizeof(shadowCascades_[cascade].viewProjection));
        frame->shadowSplitDepths[cascade] = shadowCascades_[cascade].farDistance;
        frame->shadowWorldUnitsPerTexel[cascade] = shadowCascades_[cascade].worldUnitsPerTexel;
      }
      frame->shadowParameters[1] = static_cast<float>(shadowCascadeCount_);
    }
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
  beginGpuRegion(GpuPassClass::WaterSimulation);
  const float spectralTime=spectralWaterCount_>0?
    (authoredWaterTime_>=0?authoredWaterTime_*waterSpectralControls_.timeScale:
    static_cast<float>(waterSpectralClock_.advance(timeSeconds,waterSpectralControls_.timeScale))):0;
  const bool simulateWater=authoredVisibility_.empty() ||
      std::any_of(waterDrawOrder_.begin(),waterDrawOrder_.end(),[&](u32 draw){return authoredVisibility_[draw]!=0;});
  for(u32 cascade=0;simulateWater && cascade<spectralWaterCount_;++cascade) {
    const auto &c=waterCascadeSettings_[cascade];
    const auto &foamSettings=waterSpectralControls_.overrideFoam?waterSpectralControls_.foam:c.foam;
    const rhi::WaterFoamComputeParameters foam{c.displacementScale*c.choppiness*
      waterSpectralControls_.displacement*waterSpectralControls_.choppiness,
      foamSettings.compressionThreshold,foamSettings.growth,foamSettings.decay};
    if(!(*waterSpectralCompute_)[cascade].record(commandBuffer_,spectralTime,foam)) return rhi::SwapchainStatus::FatalError;
  }

  endGpuRegion(GpuPassClass::WaterSimulation);
  beginGpuRegion(GpuPassClass::Shadow);
  recordShadowPass(camera);
  endGpuRegion(GpuPassClass::Shadow);

  // Oclusao GPU-driven: tem de ficar fora de qualquer render pass e antes do
  // passe opaco que consome os argumentos indiretos que ela escreve. No-op
  // quando o consumidor nao esta ativo -- a regiao ainda e fechada para que a
  // classe seguinte nao absorva o custo dela.
  beginGpuRegion(GpuPassClass::Culling);
  recordDrawCullDispatch(camera);
  // Segundo estagio do mesmo trabalho: remove da lista o que o primeiro acabou
  // de marcar como invisivel. Fica na mesma regiao de GPU porque e o custo do
  // mesmo estagio, e atribui-lo a outra classe falsearia a medicao das duas.
  recordDrawCompactDispatch();
  endGpuRegion(GpuPassClass::Culling);

  VkClearValue clearValues[2]{};
  clearValues[0].color = editorBackground_ ? VkClearColorValue{{0.028f,0.032f,0.039f,1.0f}} : VkClearColorValue{{0.02f,0.02f,0.05f,1.0f}};
  clearValues[1].depthStencil = {1.0f, 0};

  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = renderPass_;
  renderPassInfo.framebuffer = framebuffers_[imageIndex];
  renderPassInfo.renderArea.extent = {renderWidth(), renderHeight()};
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
      sceneAspectRatio(),
      camera.yaw,
      camera.pitch,
      surfaceTransform.xx, surfaceTransform.xy, surfaceTransform.yx, surfaceTransform.yy,
      baseTextureIndex_,
      1.15f, materialParameters_.roughness, materialParameters_.metallic,
      (!renderingPolicy_.post.dedicatedPass &&
       (swapchain_->imageFormat()==VK_FORMAT_B8G8R8A8_SRGB ||
        swapchain_->imageFormat()==VK_FORMAT_R8G8B8A8_SRGB)) ? 0u : 1u,
      materialParameters_.normalScale,
      {},
  };
  if (!dirtRoadPreview_)
    vkCmdPushConstants(commandBuffer_, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(pushConstants), &pushConstants);

  VkViewport viewport{};
  viewport.width = static_cast<float>(renderWidth());
  viewport.height = static_cast<float>(renderHeight());
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  if (!sceneViewport_.isEmpty()) {
    const float x = 2.0f * (sceneViewport_.x + sceneViewport_.width * 0.5f) - 1.0f;
    const float y = 2.0f * (sceneViewport_.y + sceneViewport_.height * 0.5f) - 1.0f;
    const float w = std::abs(surfaceTransform.xx) * sceneViewport_.width +
                    std::abs(surfaceTransform.xy) * sceneViewport_.height;
    const float h = std::abs(surfaceTransform.yx) * sceneViewport_.width +
                    std::abs(surfaceTransform.yy) * sceneViewport_.height;
    viewport.x = (surfaceTransform.xx*x + surfaceTransform.xy*y + 1.0f - w) * 0.5f * renderWidth();
    viewport.y = (surfaceTransform.yx*x + surfaceTransform.yy*y + 1.0f - h) * 0.5f * renderHeight();
    viewport.width *= w;
    viewport.height *= h;
  }
  vkCmdSetViewport(commandBuffer_, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {static_cast<i32>(std::max(0.0f, std::floor(viewport.x))),
                    static_cast<i32>(std::max(0.0f, std::floor(viewport.y)))};
  scissor.extent = {std::min(renderWidth() - static_cast<u32>(scissor.offset.x), static_cast<u32>(std::ceil(viewport.width))),
                    std::min(renderHeight() - static_cast<u32>(scissor.offset.y), static_cast<u32>(std::ceil(viewport.height)))};
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
    const bool encodeSrgb = !renderingPolicy_.post.dedicatedPass &&
                            swapchain_->imageFormat()!=VK_FORMAT_B8G8R8A8_SRGB &&
                            swapchain_->imageFormat()!=VK_FORMAT_R8G8B8A8_SRGB;
    visibilityTelemetry_ = {};
    auto pushMapMaterial = [&](u32 materialIndex, u32 drawIndex=0xffffffffu) {
      auto material = dirtRoadResources_.materials()[materialIndex];
      if(drawIndex<authoredMaterials_.size()) material=renderer::applyMaterialOverride(material,authoredMaterials_[drawIndex]);
      if (!useBindless_) {
        const VkDescriptorSet set = dirtMaterialSets_[materialIndex];
        vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_,
                                0, 1, &set, 0, nullptr);
      }
      DirtRoadPushConstants push{};
      push.cameraFrame[0] = sceneAspectRatio();
      push.cameraFrame[1] = camera.yaw; push.cameraFrame[2] = camera.pitch;
      push.cameraFrame[3] = timeSeconds;
      push.surfaceTransform[0]=surfaceTransform.xx;push.surfaceTransform[1]=surfaceTransform.xy;
      push.surfaceTransform[2]=surfaceTransform.yx;push.surfaceTransform[3]=surfaceTransform.yy;
      std::memcpy(push.cameraPositionNear, camera.position, sizeof(camera.position));
      push.cameraPositionNear[3] = sceneNearPlane();
      std::memcpy(push.baseColorFactor, material.baseColorFactor, sizeof(push.baseColorFactor));
      std::memcpy(push.emissiveFactorAndStrength, material.emissiveFactorAndStrength,
                  sizeof(push.emissiveFactorAndStrength));
      if((material.flags & renderer::WaterAuthoringResource) && (material.flags & renderer::MapMaterialWater) && drawIndex<authoredWaterLayers_.size()) {
        std::copy(authoredWaterLayers_[drawIndex].begin(),authoredWaterLayers_[drawIndex].end(),push.emissiveFactorAndStrength);
        std::copy(authoredWaterFlowDepth_[drawIndex].begin(),authoredWaterFlowDepth_[drawIndex].end(),push.baseColorFactor);
      }
      for (u32 slot = 0; slot < 4; ++slot) {
        const u32 texture = material.textureIndices[slot];
        push.textureIndices[slot] = useBindless_ && texture != renderer::InvalidMapTexture
                                        ? dirtTextureSlots_[texture] : baseTextureIndex_;
      }
      push.materialFlags[0]=material.flags;
      const u32 alphaCutoff = static_cast<u32>(std::clamp(material.alphaCutoff, 0.0f, 1.0f) * 255.0f + .5f);
      // High 16 bits carry optional material-class metadata. Multi-view
      // impostors use it for atlas/grid layout; legacy materials keep zero and
      // therefore preserve the old push-constant bit pattern.
      push.materialFlags[1]=material.textureCoordinates | (alphaCutoff << 8u) |
                            ((material.reserved & 0xffffu) << 16u);
      push.materialFlags[2]=encodeSrgb?1u:0u;
      push.materialFlags[3]=std::bit_cast<u32>(sceneFarPlane());
      push.materialFactors[0]=material.roughness;push.materialFactors[1]=material.metallic;
      push.materialFactors[2]=material.normalScale;push.materialFactors[3]=material.specular;
      vkCmdPushConstants(commandBuffer_,pipelineLayout_,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
                         0,sizeof(push),&push);
    };
    auto materialPipeline = [&](u32 materialIndex, const VkPipeline *variants,
                                VkPipeline fallback) {
      if (variants == nullptr) return fallback;
      const u32 variant = renderer::materialFeatureVariant(
          dirtRoadResources_.materials()[materialIndex].flags);
      return variants[variant] != VK_NULL_HANDLE ? variants[variant] : fallback;
    };
    // The generic pipeline was bound immediately before entering the map path.
    // Keep a command-buffer-local state cache so disabling material variants is
    // actually zero extra pipeline binds. With variants enabled this also
    // avoids rebinding when consecutive material batches share the same key.
    VkPipeline boundMapPipeline = pipeline_;
    auto bindMapPipeline = [&](VkPipeline pipeline) {
      if (pipeline == boundMapPipeline) return;
      vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
      boundMapPipeline = pipeline;
    };
    auto usesDistantMaterialPipeline = [&](u32 drawIndex) {
      const auto &draw = dirtRoadResources_.draws()[drawIndex];
      const auto &material = dirtRoadResources_.materials()[draw.materialIndex];
      // The impostor normal atlas is replacement geometry, not optional
      // high-frequency material detail. Stripping it in the distant pipeline
      // recreates the white camera-facing cards the multiview bake removes.
      return (material.flags & renderer::MapMaterialNormalMap) != 0 &&
             (material.flags & renderer::MapMaterialImpostor) == 0 &&
             renderer::boundsEntirelyPastDistance(
                 camera.position, draw.boundsCenter, draw.boundsRadius,
                 renderingPolicy_.materialDistance.normalMapMaximumDistance);
    };
    auto drawMapPrimitive = [&](u32 drawIndex, const VkPipeline *variants, VkPipeline fallback,
                                VkPipeline distantFallback) {
      const auto &draw = dirtRoadResources_.draws()[drawIndex];
      const bool distant = distantFallback != VK_NULL_HANDLE &&
                           usesDistantMaterialPipeline(drawIndex);
      bindMapPipeline(distant ? distantFallback
                              : materialPipeline(draw.materialIndex, variants, fallback));
      pushMapMaterial(draw.materialIndex,drawIndex);
      const bool route=(dirtRoadResources_.materials()[draw.materialIndex].flags & renderer::WaterRouteResource)!=0;
      const VkDeviceSize zero=0;
      const VkBuffer geometry=route?routeVertices_.handle():dirtRoadResources_.vertexBuffer();
      vkCmdBindVertexBuffers(commandBuffer_,0,1,&geometry,&zero);
      vkCmdBindIndexBuffer(commandBuffer_,route?routeIndices_.handle():dirtRoadResources_.indexBuffer(),0,VK_INDEX_TYPE_UINT32);
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
    visibilitySettings.verticalFieldOfViewRadians = sceneFieldOfView();
    visibilitySettings.nearPlane = sceneNearPlane();
    visibilitySettings.farPlane = sceneFarPlane();
    // Mesmo volume que recordDrawCullDispatch ja usou antes do render pass: as
    // duas etapas nao podem enxergar frustums diferentes no mesmo frame.
    const renderer::PerspectiveFrustum frustum = buildFrameFrustum(camera);
    visibleSolidDrawOrder_.clear();
    visibleCoverageDrawOrder_.clear();
    visibleTransparentDrawOrder_.clear();

    // LOD selection (opt-in, see setLodSelectionEnabled): runs before
    // frustum culling because it decides WHICH draws are even candidates
    // this frame -- only the active level's chunks (plus, mid-transition,
    // the neighbor level fading in/out) enter their pipeline-specific scratch
    // list, never every level of every group at once. Alpha-tested vegetation
    // writes depth and uses the same dither contract, so it is filtered here
    // independently from opaque geometry. True blend remains untouched.
    const std::vector<u32> *solidCandidates = lodGroups_.empty()
        ? &solidDrawOrder_ : &levelZeroSolidDrawOrder_;
    const std::vector<u32> *coverageCandidates = coverageLodGroups_.empty()
        ? &coverageDrawOrder_ : &levelZeroCoverageDrawOrder_;
    if (authoredVisibility_.empty() && lodSelectionEnabled_ && (!lodGroups_.empty() || !coverageLodGroups_.empty())) {
      auto *instances = static_cast<renderer::GpuMeshInstance *>(instanceBuffer_.mappedData());
      // Error budgets describe final display pixels. Internal resolution here
      // allowed a DRS change to remove geometry with a stationary camera.
      const float activeVerticalPixels = std::max(
          1.0f, static_cast<float>(displayExtent.height));
      auto selectGroups = [&](std::vector<renderer::LodRenderGroup> &groups,
                              const std::vector<u32> &ungrouped,
                              std::vector<u32> &filtered,
                              float pixelErrorBudget) {
        filtered.clear();
        filtered.insert(filtered.end(), ungrouped.begin(), ungrouped.end());
        for (renderer::LodRenderGroup &group : groups) {
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
              activeVerticalPixels, pixelErrorBudget, lodHysteresisBandRatio_,
              group.hysteresis);
          // Clear every draw in every level so a group leaving a transition
          // cannot retain stale coverage in its instance record.
          for (u32 level = 0; level < levelCount; ++level)
            for (u32 drawIndex : group.levels[level].drawIndices)
              instances[drawIndex].normalColumns[7] = 0.0f;
          const renderer::LodRenderLevel &active = group.levels[selection.level];
          filtered.insert(filtered.end(), active.drawIndices.begin(), active.drawIndices.end());
          if (selection.ditherToCoarserFactor > 0.0f && selection.level + 1 < levelCount) {
            const renderer::LodRenderLevel &neighbor = group.levels[selection.level + 1];
            const renderer::LodDitherPair dither =
                renderer::encodeLodDither(selection.ditherToCoarserFactor);
            for (u32 drawIndex : active.drawIndices)
              instances[drawIndex].normalColumns[7] = dither.outgoing;
            // Negative means incoming: keep the exact pixels the positive
            // outgoing mask discards, including in alpha-tested vegetation.
            for (u32 drawIndex : neighbor.drawIndices)
              instances[drawIndex].normalColumns[7] = dither.incoming;
            filtered.insert(filtered.end(), neighbor.drawIndices.begin(), neighbor.drawIndices.end());
          }
        }
      };
      if (!lodGroups_.empty()) {
        selectGroups(lodGroups_, ungroupedSolidDrawOrder_, lodFilteredSolidDrawOrder_,
                     lodPixelErrorBudget_);
        solidCandidates = &lodFilteredSolidDrawOrder_;
      }
      if (!coverageLodGroups_.empty()) {
        selectGroups(coverageLodGroups_, ungroupedCoverageDrawOrder_,
                     lodFilteredCoverageDrawOrder_, coverageLodPixelErrorBudget_);
        coverageCandidates = &lodFilteredCoverageDrawOrder_;
      }
      if (!memoryAllocator_->flushBuffer(instanceBuffer_)) return rhi::SwapchainStatus::FatalError;
      lodSelectionAppliedLastFrame_ = true;
    } else if (lodSelectionAppliedLastFrame_) {
      // Runtime/project settings may disable LOD after a transition. Clear the
      // signed coverage factors once so LOD0 cannot inherit a stale fade mask.
      auto *instances = static_cast<renderer::GpuMeshInstance *>(instanceBuffer_.mappedData());
      auto clearDither = [&](const std::vector<renderer::LodRenderGroup> &groups) {
        for (const renderer::LodRenderGroup &group : groups)
          for (u32 level = 0; level < group.levelCount; ++level)
            for (u32 drawIndex : group.levels[level].drawIndices)
              instances[drawIndex].normalColumns[7] = 0.0f;
      };
      clearDither(lodGroups_);
      clearDither(coverageLodGroups_);
      if (!memoryAllocator_->flushBuffer(instanceBuffer_)) return rhi::SwapchainStatus::FatalError;
      lodSelectionAppliedLastFrame_ = false;
    }

    auto collectVisible = [&](const std::vector<u32> &source, std::vector<u32> &destination) {
      for (u32 drawIndex : source) {
        if (!authoredVisibility_.empty() && !authoredVisibility_[drawIndex]) continue;
        const auto &draw = dirtRoadResources_.draws()[drawIndex];
        ++visibilityTelemetry_.candidateDraws;
        visibilityTelemetry_.candidateTriangles += draw.indexCount / 3;
        const bool cameraWater = (dirtRoadResources_.materials()[draw.materialIndex].flags &
            (renderer::MapMaterialWater | renderer::MapMaterialWaterCameraGrid)) ==
            (renderer::MapMaterialWater | renderer::MapMaterialWaterCameraGrid);
        // Camera-relative water cannot be culled by its original world bounds.
        // A single bounded grid is conservatively submitted, clipped by the GPU.
        const bool water=(dirtRoadResources_.materials()[draw.materialIndex].flags&renderer::MapMaterialWater)!=0;
        const float spectralExpansion=water?spectralWaterBoundsExpansion_+
            std::abs(waterBaseHeight_)+renderer::maximumWaterDisplacement(waterProfile_)+
            renderer::maximumWaterDetailDisplacement(waterShading_):0;
        if (!cameraWater && !renderer::isSphereVisible(frustum, draw.boundsCenter, draw.boundsRadius+spectralExpansion)) continue;
        destination.push_back(drawIndex);
      }
    };
    collectVisible(*solidCandidates, visibleSolidDrawOrder_);
    collectVisible(*coverageCandidates, visibleCoverageDrawOrder_);
    collectVisible(transparentDrawOrder_, visibleTransparentDrawOrder_);
    collectVisible(waterDrawOrder_, visibleTransparentDrawOrder_);

    const u32 hzbCandidateDraws = static_cast<u32>(
        visibleSolidDrawOrder_.size() + visibleCoverageDrawOrder_.size());
    hzbFrameEligible_ = sceneViewport_.isEmpty() && (hzbOcclusionEnabled_ || hzbComputeEnabled_) &&
        renderer::shouldRunHzb(hzbCandidateDraws, hzbMinimumCandidateDraws_);
    if ((hzbOcclusionEnabled_ || hzbComputeEnabled_) && !hzbFrameEligible_) {
      visibilityTelemetry_.hzbSkippedBudgetDraws = hzbCandidateDraws;
      for (u32 drawIndex : visibleSolidDrawOrder_) hzbHysteresis_[drawIndex] = {};
      for (u32 drawIndex : visibleCoverageDrawOrder_) hzbHysteresis_[drawIndex] = {};
    }

    // HZB occlusion (opt-in, see setHzbOcclusionEnabled): a second,
    // conservative filter over what already survived frustum culling above.
    // Blend/transparent draws are never tested here -- their back-to-front
    // ordering and visibility contract stay exactly as O1 defined them.
    auto filterByHzb = [&](std::vector<u32> &visible) {
      if (!hzbOcclusionEnabled_ || !hzbFrameEligible_ || !hzbPyramidValid_) return;
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
        if (dynamicMapDraws_[drawIndex]) {
          visible[writeIndex++] = drawIndex;
          continue;
        }
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
      if (depthOrderedBatches_) {
        // Run-length sobre a lista JA ordenada front-to-back: fecha um lote a
        // cada troca de material. A ordem de profundidade chega intacta ao
        // tiler, que e a condicao para o early-Z rejeitar o fragmento distante
        // antes de sombra-lo. Ver setDepthOrderedBatchesEnabled.
        for (u32 drawIndex : visible) {
          const auto &draw = dirtRoadResources_.draws()[drawIndex];
          const bool distantMaterial = usesDistantMaterialPipeline(drawIndex);
          if (batches.empty() || batches.back().materialIndex != draw.materialIndex ||
              batches.back().distantMaterial != distantMaterial) {
            batches.push_back({draw.materialIndex,
                               static_cast<u32>(indirectCommands_.size()), 0, 0,
                               distantMaterial});
          }
          indirectCommands_.push_back({draw.indexCount, 1, draw.firstIndex,
                                       static_cast<i32>(draw.vertexOffset), drawIndex});
          ++batches.back().commandCount;
          batches.back().triangles += draw.indexCount / 3;
        }
        return;
      }
      for (u32 drawIndex : visible) {
        const u32 materialIndex = dirtRoadResources_.draws()[drawIndex].materialIndex;
        const bool distantMaterial = usesDistantMaterialPipeline(drawIndex);
        const auto existing = std::find_if(batches.begin(), batches.end(),
            [&](const IndirectBatch &batch) {
              return batch.materialIndex == materialIndex &&
                     batch.distantMaterial == distantMaterial;
            });
        if (existing == batches.end())
          batches.push_back({materialIndex, 0, 0, 0, distantMaterial});
      }
      for (IndirectBatch &batch : batches) {
        batch.firstCommand = static_cast<u32>(indirectCommands_.size());
        for (u32 drawIndex : visible) {
          const auto &draw = dirtRoadResources_.draws()[drawIndex];
          if (draw.materialIndex != batch.materialIndex ||
              usesDistantMaterialPipeline(drawIndex) != batch.distantMaterial) continue;
          indirectCommands_.push_back({draw.indexCount, 1, draw.firstIndex,
                                       static_cast<i32>(draw.vertexOffset), drawIndex});
          ++batch.commandCount;
          batch.triangles += draw.indexCount / 3;
        }
      }
    };
    // Preenchido depois de os lotes existirem, mas antes de qualquer submissao;
    // as lambdas abaixo capturam por referencia e leem o valor ja resolvido.
    bool compactedSubmission = false;
    auto submitIndirectBatches = [&](const std::vector<IndirectBatch> &batches,
                                     const VkPipeline *variants, VkPipeline fallback,
                                     VkPipeline distantFallback) {
      for (const IndirectBatch &batch : batches) {
        bindMapPipeline(batch.distantMaterial && distantFallback != VK_NULL_HANDLE
                            ? distantFallback
                            : materialPipeline(batch.materialIndex, variants, fallback));
        pushMapMaterial(batch.materialIndex);
        if (compactedSubmission) {
          // maxDrawCount continua sendo o tamanho do lote: e o teto que a spec
          // exige, e a contagem real vem do buffer que o kernel escreveu. O
          // offset e o mesmo do lote porque o destino da compactacao coincide
          // com a origem (ver publishCompactionBatches).
          rhiDevice_->cmdDrawIndexedIndirectCount(commandBuffer_,
              compactedIndirectBuffer_.handle(),
              static_cast<VkDeviceSize>(batch.firstCommand) * sizeof(VkDrawIndexedIndirectCommand),
              compactCountBuffer_.handle(),
              static_cast<VkDeviceSize>(batch.compactionSlot) * sizeof(u32),
              batch.commandCount, sizeof(VkDrawIndexedIndirectCommand));
        } else {
          vkCmdDrawIndexedIndirect(commandBuffer_, indirectBuffer_.handle(),
              static_cast<VkDeviceSize>(batch.firstCommand) * sizeof(VkDrawIndexedIndirectCommand),
              batch.commandCount, sizeof(VkDrawIndexedIndirectCommand));
        }
        ++visibilityTelemetry_.submittedDrawCalls;
        visibilityTelemetry_.submittedTriangles += batch.triangles;
      }
    };
    // Registros do kernel de oclusao, um por slot de comando e na MESMA ordem.
    // Escritos aqui, depois do dispatch ja gravado: o que importa e a ordem dos
    // comandos na GPU, nao a ordem das escritas da CPU, e todas elas terminam
    // antes do vkQueueSubmit deste frame.
    auto publishDrawCullRecords = [&]() {
      if (!hzbGpuCullingActive_ || drawCullRecordBuffer_.mappedData() == nullptr) return true;
      auto *records =
          static_cast<renderer::GpuCullDrawRecord *>(drawCullRecordBuffer_.mappedData());
      const u32 published =
          std::min(static_cast<u32>(indirectCommands_.size()), drawCullCapacity_);
      for (u32 slot = 0; slot < published; ++slot) {
        // firstInstance carrega o drawIndex desde que o caminho indireto existe
        // (e o mesmo valor que o caminho direto passa a vkCmdDrawIndexed), o que
        // torna a lista de comandos a unica fonte da correspondencia slot->draw.
        const u32 drawIndex = indirectCommands_[slot].firstInstance;
        const auto &draw = dirtRoadResources_.draws()[drawIndex];
        renderer::GpuCullDrawRecord record{};
        std::memcpy(record.boundsCenter, draw.boundsCenter, sizeof(record.boundsCenter));
        record.boundsRadius = draw.boundsRadius;
        record.stateIndex = drawIndex;
        record.flags = renderer::GpuCullRecordPresent |
            (dynamicMapDraws_[drawIndex] ? 0u : renderer::GpuCullRecordTestable);
        records[slot] = record;
      }
      // Sobras marcadas ausentes: o dispatch cobre a capacidade inteira e um
      // slot com lixo do frame anterior escreveria estado de outro draw.
      for (u32 slot = published; slot < drawCullCapacity_; ++slot)
        records[slot] = renderer::GpuCullDrawRecord{};
      return memoryAllocator_->flushBuffer(drawCullRecordBuffer_);
    };
    if (useMultiDrawIndirect_) {
      indirectCommands_.clear();
      buildIndirectBatches(visibleSolidDrawOrder_, indirectSolidBatches_);
      buildIndirectBatches(visibleCoverageDrawOrder_, indirectCoverageBatches_);
      if (!indirectCommands_.empty())
        std::memcpy(indirectBuffer_.mappedData(), indirectCommands_.data(),
                    indirectCommands_.size() * sizeof(VkDrawIndexedIndirectCommand));
      if (!memoryAllocator_->flushBuffer(indirectBuffer_) || !publishDrawCullRecords())
        return rhi::SwapchainStatus::FatalError;
      // Recusa aqui nao e erro de frame: significa que este frame submete pela
      // lista original, que continua correta. O dispatch de compactacao ja
      // gravado escreve num destino que ninguem le, e isso e barato o bastante
      // para nao valer uma segunda passagem de gravacao do command buffer.
      compactedSubmission = publishCompactionBatches();
      submitIndirectBatches(indirectSolidBatches_, opaqueMaterialPipelines_, pipeline_,
                            opaqueDistantPipeline_);
    } else {
      for (u32 drawIndex : visibleSolidDrawOrder_)
        drawMapPrimitive(drawIndex, opaqueMaterialPipelines_, pipeline_, opaqueDistantPipeline_);
    }
    // Vegetação alpha-mask sai do mesmo balde que os opacos sólidos. As duas
    // classes têm custo de fragment muito diferente e o programa de margem
    // atribui uma faixa própria à folhagem; medi-las somadas tornava essa
    // atribuição impossível de verificar.
    endGpuRegion(GpuPassClass::Opaque);
    beginGpuRegion(GpuPassClass::Coverage);
    if (!visibleCoverageDrawOrder_.empty()) {
      if (coveragePrepassEnabled_) {
        bindMapPipeline(coveragePipeline_);
        if (useMultiDrawIndirect_)
          submitIndirectBatches(indirectCoverageBatches_, nullptr, coveragePipeline_,
                                VK_NULL_HANDLE);
        else
          for (u32 drawIndex : visibleCoverageDrawOrder_)
            drawMapPrimitive(drawIndex, nullptr, coveragePipeline_, VK_NULL_HANDLE);
        if (useMultiDrawIndirect_)
          submitIndirectBatches(indirectCoverageBatches_, coverageMaterialPipelines_,
                                coverageShadePipeline_, coverageDistantPipeline_);
        else
          for (u32 drawIndex : visibleCoverageDrawOrder_)
            drawMapPrimitive(drawIndex, coverageMaterialPipelines_, coverageShadePipeline_,
                             coverageDistantPipeline_);
      } else if (useMultiDrawIndirect_) {
        submitIndirectBatches(indirectCoverageBatches_, nullptr, pipeline_, opaqueDistantPipeline_);
      } else {
        for (u32 drawIndex : visibleCoverageDrawOrder_)
          drawMapPrimitive(drawIndex, nullptr, pipeline_, opaqueDistantPipeline_);
      }
    }
    endGpuRegion(GpuPassClass::Coverage);
    beginGpuRegion(GpuPassClass::Sky);

    DirtRoadPushConstants skyPush{};
    skyPush.cameraFrame[0] = sceneAspectRatio();
    skyPush.materialFactors[3] = 1.0f/std::tan(sceneFieldOfView()*.5f);
    skyPush.cameraFrame[1] = camera.yaw;
    skyPush.cameraFrame[2] = camera.pitch;
    skyPush.cameraFrame[3] = timeSeconds;
    skyPush.surfaceTransform[0] = surfaceTransform.xx;
    skyPush.surfaceTransform[1] = surfaceTransform.xy;
    skyPush.surfaceTransform[2] = surfaceTransform.yx;
    skyPush.surfaceTransform[3] = surfaceTransform.yy;
    skyPush.materialFlags[0] = cameraWaterHorizonFillActive_ ? 1u : 0u;
    skyPush.materialFlags[2] = encodeSrgb ? 1u : 0u;
    vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, skyPipeline_);
    boundMapPipeline = VK_NULL_HANDLE;
    vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, skyPipelineLayout_,
                            0, 1, &environmentSet_, 0, nullptr);
    vkCmdPushConstants(commandBuffer_, skyPipelineLayout_,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(skyPush), &skyPush);
    if(!editorBackground_) vkCmdDraw(commandBuffer_, 3, 1, 0, 0);

    // A grade editorial, depois da geometria opaca e antes do transparente: ela
    // precisa da profundidade da cena já escrita para ser escondida por ela, e
    // não deve ocluir água nem vidro.
    if (editorGrid_.valid() && editorGridPipeline_ != VK_NULL_HANDLE) {
      DirtRoadPushConstants gridPush{};
      gridPush.cameraFrame[0] = sceneAspectRatio();
      gridPush.cameraFrame[1] = camera.yaw;
      gridPush.cameraFrame[2] = camera.pitch;
      gridPush.cameraFrame[3] = 1.0f; // opacidade global da grade
      gridPush.materialFactors[3] = 1.0f/std::tan(sceneFieldOfView()*.5f);
      gridPush.surfaceTransform[0] = surfaceTransform.xx;
      gridPush.surfaceTransform[1] = surfaceTransform.xy;
      gridPush.surfaceTransform[2] = surfaceTransform.yx;
      gridPush.surfaceTransform[3] = surfaceTransform.yy;
      std::memcpy(gridPush.cameraPositionNear, camera.position, sizeof(camera.position));
      gridPush.cameraPositionNear[3] = sceneNearPlane();
      const float farPlane = sceneFarPlane();
      std::memcpy(&gridPush.materialFlags[3], &farPlane, sizeof(farPlane));
      gridPush.materialFlags[2] = encodeSrgb ? 1u : 0u;
      // O mesmo deslocamento temporal que o vertice da cena soma ao NDC. O
      // vertice da grade o subtrai antes de desfazer a projecao; sem isso a
      // grade anda meio pixel por quadro contra a geometria.
      gridPush.baseColorFactor[0] = temporalCurrentJitter_[0];
      gridPush.baseColorFactor[1] = temporalCurrentJitter_[1];
      gridPush.baseColorFactor[3] = editorGrid_.planeHeight;
      gridPush.emissiveFactorAndStrength[0] = editorGrid_.minorSpacing;
      gridPush.emissiveFactorAndStrength[1] = editorGrid_.majorSpacing;
      gridPush.emissiveFactorAndStrength[2] = editorGrid_.minorOpacity;
      gridPush.emissiveFactorAndStrength[3] = editorGrid_.fadeDistance;
      // Cinza neutro: a grade é referência espacial, não um elemento de cena.
      gridPush.materialFactors[0] = gridPush.materialFactors[1] = gridPush.materialFactors[2] = .55f;
      vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, editorGridPipeline_);
      boundMapPipeline = VK_NULL_HANDLE;
      vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, editorGridPipelineLayout_,
                              0, 1, &environmentSet_, 0, nullptr);
      vkCmdPushConstants(commandBuffer_, editorGridPipelineLayout_,
                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                         0, sizeof(gridPush), &gridPush);
      vkCmdDraw(commandBuffer_, 3, 1, 0, 0);
    }
    endGpuRegion(GpuPassClass::Sky);
    beginGpuRegion(GpuPassClass::Transparent);

    if (waterSubpassActive_) {
      vkCmdNextSubpass(commandBuffer_, VK_SUBPASS_CONTENTS_INLINE);
      boundMapPipeline = VK_NULL_HANDLE;
    }

    std::sort(visibleTransparentDrawOrder_.begin(), visibleTransparentDrawOrder_.end(), [&](u32 left, u32 right) {
      const auto &a=dirtRoadResources_.draws()[left];const auto &b=dirtRoadResources_.draws()[right];
      float da=0,db=0;for(u32 axis=0;axis<3;++axis){const float av=a.boundsCenter[axis]-camera.position[axis];
        const float bv=b.boundsCenter[axis]-camera.position[axis];da+=av*av;db+=bv*bv;}return da>db;
    });
    if (!visibleTransparentDrawOrder_.empty()) {
      vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_,
                              1, 1, &environmentSet_, 0, nullptr);
      if (useBindless_)
        vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_,
                                0, 1, &textureSet_, 0, nullptr);
      for (u32 drawIndex : visibleTransparentDrawOrder_) {
        const auto &draw = dirtRoadResources_.draws()[drawIndex];
        const bool water = (dirtRoadResources_.materials()[draw.materialIndex].flags &
                            renderer::MapMaterialWater) != 0;
        if (water) {
          // SkipDraw remove a água inteira: é a única forma de saber quanto do
          // passe é água num GPU que colapsa timestamps por subpasse.
          if (waterCostIsolation_ != renderer::WaterCostIsolation::SkipDraw)
            drawMapPrimitive(drawIndex, nullptr, waterPipeline_, VK_NULL_HANDLE);
        }
        else {
          drawMapPrimitive(drawIndex, transparentMaterialPipelines_, transparentPipeline_,
                           transparentDistantPipeline_);
        }
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
    if (drawnInstanceCount_ != 0) vkCmdDraw(commandBuffer_, 36, drawnInstanceCount_, 0, 0);
    endGpuRegion(GpuPassClass::Opaque);
  }

  // A região abre fora do if: o HUD desenhava sem nenhuma marca e seu custo
  // caía no intervalo não atribuído entre a última classe e o fim do frame.
  beginGpuRegion(GpuPassClass::Ui);
  const VkViewport uiViewport{0,0,static_cast<float>(renderWidth()),static_cast<float>(renderHeight()),0,1};
  const VkRect2D uiScissor{{0,0},{renderWidth(),renderHeight()}};
  vkCmdSetViewport(commandBuffer_,0,1,&uiViewport);
  vkCmdSetScissor(commandBuffer_,0,1,&uiScissor);
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
  beginGpuRegion(GpuPassClass::Post);
  recordPostProcess(imageIndex, camera);
  recordUiOverlay(imageIndex);
  endGpuRegion(GpuPassClass::Post);
  // Must run after the main pass ends (depthImage_ needs its final write
  // landed, in DEPTH_STENCIL_READ_ONLY_OPTIMAL) and before submit; a no-op
  // when HZB occlusion is disabled or its resources failed to initialize.
  beginGpuRegion(GpuPassClass::Hzb);
  if (hzbFrameEligible_) recordHzbReductionPass(camera);
  endGpuRegion(GpuPassClass::Hzb);
  hzbPreviousFrameEligible_ = hzbFrameEligible_ && hzbReadbackRecordedThisFrame_;
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
        "[Visibility] render=%ux%u scale=%.3f candidates=%u visible=%u culled=%u submitted_draws=%u triangles_visible=%llu/%llu triangles_submitted=%llu shadow_draws=%u/%u shadow_cascades=%u/%u shadow_cache_hit_frames=%llu hzb_tested=%u hzb_occluded=%u hzb_revived=%u hzb_motion_skip=%u hzb_budget_skip=%u gpu_cull=%s gpu_cull_tested=%u gpu_cull_occluded=%u gpu_cull_revived=%u gpu_cull_visible=%u",
        renderWidth(), renderHeight(), static_cast<double>(dynamicResolution_.scale()),
        visibilityTelemetry_.candidateDraws, visibilityTelemetry_.visibleDraws,
        visibilityTelemetry_.culledDraws, visibilityTelemetry_.submittedDrawCalls,
        static_cast<unsigned long long>(visibilityTelemetry_.visibleTriangles),
        static_cast<unsigned long long>(visibilityTelemetry_.candidateTriangles),
        static_cast<unsigned long long>(visibilityTelemetry_.submittedTriangles),
        shadowSubmittedDraws_, shadowCandidateDraws_, shadowRenderedCascades_,
        shadowCascadeCount_, static_cast<unsigned long long>(shadowCacheHitFrames_),
        visibilityTelemetry_.hzbTestedDraws, visibilityTelemetry_.hzbOccludedDraws,
        visibilityTelemetry_.hzbRevivedDraws,
        visibilityTelemetry_.hzbSkippedCameraMotionDraws,
        visibilityTelemetry_.hzbSkippedBudgetDraws,
        // Os quatro contadores abaixo vem do dispatch do frame ANTERIOR e sao
        // os unicos numeros validos quando o consumidor GPU esta ativo:
        // submitted_draws/submitted_triangles contam o que a CPU submeteu, que
        // por construcao ainda inclui o que a GPU zerou.
        hzbGpuCullingActive_ ? (drawCullDispatchedThisFrame_ ? "ativo" : "inativo") : "off",
        drawCullTelemetry_[0], drawCullTelemetry_[1], drawCullTelemetry_[2],
        drawCullTelemetry_[3]);
  }
  return acquireStatus == rhi::SwapchainStatus::SuboptimalNeedsRecreate
             ? rhi::SwapchainStatus::SuboptimalNeedsRecreate
             : rhi::SwapchainStatus::Ok;
}

} // namespace ae::platform::android
