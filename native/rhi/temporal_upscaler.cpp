#include "rhi/temporal_upscaler.h"

#include <cmath>
#include <cstring>
#include <vector>

#if AETHER_ARM_ASR
#include <host/ffxm_fsr2.h>
#include <host/backends/vk/ffxm_vk.h>
#endif
#if AETHER_FSR2
#include <ffx_fsr2.h>
#include <vk/ffx_fsr2_vk.h>
#endif

#if AETHER_ARM_ASR || AETHER_FSR2
// As duas bibliotecas chamam vkGetPhysicalDeviceProperties2/Features2 como
// símbolos estáticos. O stub libvulkan.so do NDK para minSdk 26 não os exporta
// (ver rhi/device.cpp); o CMake renomeia essas duas chamadas nos alvos das
// bibliotecas para estas funções, que despacham pelo ponteiro resolvido na
// instância. Nenhum arquivo de terceiros foi editado.
namespace {
PFN_vkGetPhysicalDeviceProperties2 gPhysicalDeviceProperties2 = nullptr;
PFN_vkGetPhysicalDeviceFeatures2 gPhysicalDeviceFeatures2 = nullptr;
} // namespace
extern "C" VKAPI_ATTR void VKAPI_CALL aetherVkGetPhysicalDeviceProperties2(
    VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties2 *properties) {
  if (gPhysicalDeviceProperties2) gPhysicalDeviceProperties2(physicalDevice, properties);
}
extern "C" VKAPI_ATTR void VKAPI_CALL aetherVkGetPhysicalDeviceFeatures2(
    VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2 *features) {
  if (gPhysicalDeviceFeatures2) gPhysicalDeviceFeatures2(physicalDevice, features);
}
#endif

namespace ae::rhi {

struct TemporalUpscaler::Impl {
  std::vector<u8> scratch;
  bool created = false;
#if AETHER_ARM_ASR
  arm::FfxmFsr2Context asr{};
#endif
#if AETHER_FSR2
  FfxFsr2Context fsr2{};
#endif
};

TemporalUpscaler::TemporalUpscaler() : impl_(std::make_unique<Impl>()) {}
TemporalUpscaler::~TemporalUpscaler() { destroy(); }

bool TemporalUpscaler::compiledIn(TemporalUpscalerBackend backend) noexcept {
#if AETHER_ARM_ASR
  if (backend == TemporalUpscalerBackend::ArmAsr) return true;
#endif
#if AETHER_FSR2
  if (backend == TemporalUpscalerBackend::Fsr2) return true;
#endif
  (void)backend;
  return false;
}

VkImageLayout TemporalUpscaler::outputLayout(TemporalUpscalerBackend backend) noexcept {
  // Arm ASR devolve cada recurso registrado ao estado declarado no fim do
  // dispatch; o FSR 2 deixa a UAV de saída em GENERAL.
  return backend == TemporalUpscalerBackend::ArmAsr ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                                    : VK_IMAGE_LAYOUT_GENERAL;
}

VkImageUsageFlags TemporalUpscaler::outputUsage(TemporalUpscalerBackend backend) noexcept {
  // Arm ASR escreve a saída num passe de fragmento; FSR 2, num compute.
  return VK_IMAGE_USAGE_SAMPLED_BIT |
         (backend == TemporalUpscalerBackend::ArmAsr ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                                                     : VK_IMAGE_USAGE_STORAGE_BIT);
}

u32 TemporalUpscaler::jitterPhaseCount(TemporalUpscalerBackend backend, u32 renderWidth,
                                       u32 displayWidth) noexcept {
#if AETHER_ARM_ASR
  if (backend == TemporalUpscalerBackend::ArmAsr)
    return static_cast<u32>(arm::ffxmFsr2GetJitterPhaseCount(static_cast<i32>(renderWidth),
                                                             static_cast<i32>(displayWidth)));
#endif
#if AETHER_FSR2
  if (backend == TemporalUpscalerBackend::Fsr2)
    return static_cast<u32>(ffxFsr2GetJitterPhaseCount(static_cast<i32>(renderWidth),
                                                       static_cast<i32>(displayWidth)));
#endif
  // Mesma regra das duas bibliotecas: 8 * (display / render)^2 amostras.
  (void)backend;
  const float ratio = renderWidth ? static_cast<float>(displayWidth) / static_cast<float>(renderWidth) : 1.0f;
  return static_cast<u32>(std::ceil(8.0f * ratio * ratio));
}

void TemporalUpscaler::jitterOffset(TemporalUpscalerBackend backend, u32 index, u32 phaseCount,
                                    float &x, float &y) noexcept {
  x = y = 0.0f;
#if AETHER_ARM_ASR
  if (backend == TemporalUpscalerBackend::ArmAsr) {
    arm::ffxmFsr2GetJitterOffset(&x, &y, static_cast<i32>(index), static_cast<i32>(phaseCount));
    return;
  }
#endif
#if AETHER_FSR2
  if (backend == TemporalUpscalerBackend::Fsr2) {
    ffxFsr2GetJitterOffset(&x, &y, static_cast<i32>(index), static_cast<i32>(phaseCount));
    return;
  }
#endif
  (void)backend;(void)index;(void)phaseCount;
}

bool TemporalUpscaler::ready() const noexcept { return impl_ && impl_->created; }

bool TemporalUpscaler::create(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice,
                              PFN_vkGetDeviceProcAddr getDeviceProcAddr,
                              const TemporalUpscalerContextDesc &desc, std::string &diagnostic) {
  destroy();
  desc_ = desc;
#if AETHER_ARM_ASR || AETHER_FSR2
  if (instance) {
    gPhysicalDeviceProperties2 = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
        vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2"));
    if (!gPhysicalDeviceProperties2)
      gPhysicalDeviceProperties2 = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
          vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2KHR"));
    gPhysicalDeviceFeatures2 = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
        vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures2"));
    if (!gPhysicalDeviceFeatures2)
      gPhysicalDeviceFeatures2 = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
          vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures2KHR"));
  }
  if (!gPhysicalDeviceProperties2 || !gPhysicalDeviceFeatures2) {
    diagnostic = "Instância Vulkan sem consultas 1.1 de propriedades/recursos.";
    return false;
  }
#else
  (void)instance;
#endif
  if (!device || !physicalDevice || !getDeviceProcAddr || !desc.maxRenderWidth || !desc.maxRenderHeight ||
      !desc.displayWidth || !desc.displayHeight) {
    diagnostic = "Parâmetros inválidos para o contexto do ampliador temporal.";
    return false;
  }
  if (!compiledIn(desc.backend)) {
    diagnostic = "Biblioteca do ampliador temporal ausente deste binário.";
    return false;
  }
#if AETHER_ARM_ASR
  if (desc.backend == TemporalUpscalerBackend::ArmAsr) {
    impl_->scratch.assign(arm::ffxmGetScratchMemorySizeVK(physicalDevice, FFXM_FSR2_CONTEXT_COUNT), 0);
    arm::VkDeviceContext context{device, physicalDevice, getDeviceProcAddr};
    arm::FfxmFsr2ContextDescription description{};
    const arm::FfxmFsr2ShaderQualityMode qualities[]{
        arm::FFXM_FSR2_SHADER_QUALITY_MODE_QUALITY, arm::FFXM_FSR2_SHADER_QUALITY_MODE_BALANCED,
        arm::FFXM_FSR2_SHADER_QUALITY_MODE_PERFORMANCE, arm::FFXM_FSR2_SHADER_QUALITY_MODE_ULTRA_PERFORMANCE};
    description.qualityMode = qualities[desc.shaderQuality < 4 ? desc.shaderQuality : 0];
    description.flags = arm::FFXM_FSR2_ENABLE_HIGH_DYNAMIC_RANGE |
                        (desc.dynamicResolution ? arm::FFXM_FSR2_ENABLE_DYNAMIC_RESOLUTION : 0u);
    description.maxRenderSize = {desc.maxRenderWidth, desc.maxRenderHeight};
    description.displaySize = {desc.displayWidth, desc.displayHeight};
    if (arm::ffxmGetInterfaceVK(&description.backendInterface, arm::ffxmGetDeviceVK(&context),
                                impl_->scratch.data(), impl_->scratch.size(), FFXM_FSR2_CONTEXT_COUNT) != arm::FFXM_OK) {
      diagnostic = "Arm ASR recusou a interface Vulkan.";
      impl_->scratch.clear();
      return false;
    }
    const auto result = arm::ffxmFsr2ContextCreate(&impl_->asr, &description);
    if (result != arm::FFXM_OK) {
      diagnostic = "Arm ASR recusou o contexto (código " + std::to_string(static_cast<long long>(result)) + ").";
      impl_->scratch.clear();
      return false;
    }
    impl_->created = true;
    return true;
  }
#endif
#if AETHER_FSR2
  if (desc.backend == TemporalUpscalerBackend::Fsr2) {
    impl_->scratch.assign(ffxFsr2GetScratchMemorySizeVK(physicalDevice), 0);
    FfxFsr2ContextDescription description{};
    if (ffxFsr2GetInterfaceVK(&description.callbacks, impl_->scratch.data(), impl_->scratch.size(),
                              physicalDevice, getDeviceProcAddr) != FFX_OK) {
      diagnostic = "AMD FSR 2 recusou a interface Vulkan.";
      impl_->scratch.clear();
      return false;
    }
    description.device = ffxGetDeviceVK(device);
    description.flags = FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE |
                        (desc.dynamicResolution ? FFX_FSR2_ENABLE_DYNAMIC_RESOLUTION : 0u);
    description.maxRenderSize = {desc.maxRenderWidth, desc.maxRenderHeight};
    description.displaySize = {desc.displayWidth, desc.displayHeight};
    const auto result = ffxFsr2ContextCreate(&impl_->fsr2, &description);
    if (result != FFX_OK) {
      diagnostic = "AMD FSR 2 recusou o contexto (código " + std::to_string(static_cast<long long>(result)) + ").";
      impl_->scratch.clear();
      return false;
    }
    impl_->created = true;
    return true;
  }
#endif
  diagnostic = "Biblioteca do ampliador temporal ausente deste binário.";
  return false;
}

#if AETHER_ARM_ASR
namespace {
arm::FfxmSurfaceFormat asrFormat(VkFormat format) {
  switch (format) {
    case VK_FORMAT_R16G16B16A16_SFLOAT: return arm::FFXM_SURFACE_FORMAT_R16G16B16A16_FLOAT;
    case VK_FORMAT_R16G16_SFLOAT: return arm::FFXM_SURFACE_FORMAT_R16G16_FLOAT;
    case VK_FORMAT_R8_UNORM: return arm::FFXM_SURFACE_FORMAT_R8_UNORM;
    case VK_FORMAT_R32_SFLOAT: return arm::FFXM_SURFACE_FORMAT_R32_FLOAT;
    case VK_FORMAT_D32_SFLOAT: return arm::FFXM_SURFACE_FORMAT_R32_FLOAT;
    default: return arm::FFXM_SURFACE_FORMAT_UNKNOWN;
  }
}
arm::FfxmResource asrResource(const TemporalUpscalerImage &image, arm::FfxmResourceUsage usage,
                              arm::FfxmResourceStates state) {
  if (!image.image) return {};
  arm::FfxmResourceDescription description{};
  description.type = arm::FFXM_RESOURCE_TYPE_TEXTURE2D;
  description.format = asrFormat(image.format);
  description.width = image.width;
  description.height = image.height;
  description.depth = 1;
  description.mipCount = 1;
  description.flags = arm::FFXM_RESOURCE_FLAGS_NONE;
  description.usage = usage;
  return arm::ffxmGetResourceVK(reinterpret_cast<void *>(image.image), description, nullptr, state);
}
} // namespace
#endif

bool TemporalUpscaler::dispatch(const TemporalUpscalerDispatch &d, std::string &diagnostic) {
  if (!ready() || !d.commandBuffer) {
    diagnostic = "Ampliador temporal sem contexto.";
    return false;
  }
#if AETHER_ARM_ASR
  if (desc_.backend == TemporalUpscalerBackend::ArmAsr) {
    using namespace arm;
    FfxmFsr2DispatchDescription description{};
    description.commandList = ffxmGetCommandListVK(d.commandBuffer);
    const auto read = FFXM_RESOURCE_STATE_PIXEL_COMPUTE_READ;
    description.color = asrResource(d.color, FFXM_RESOURCE_USAGE_READ_ONLY, read);
    description.depth = asrResource(d.depth, FFXM_RESOURCE_USAGE_DEPTHTARGET, read);
    description.motionVectors = asrResource(d.velocity, FFXM_RESOURCE_USAGE_READ_ONLY, read);
    description.exposure = asrResource(d.exposure, FFXM_RESOURCE_USAGE_READ_ONLY, read);
    description.reactive = asrResource(d.reactive, FFXM_RESOURCE_USAGE_READ_ONLY, read);
    description.transparencyAndComposition = asrResource(d.composition, FFXM_RESOURCE_USAGE_READ_ONLY, read);
    description.output = asrResource(d.output, FFXM_RESOURCE_USAGE_RENDERTARGET, read);
    description.jitterOffset = {d.jitterX, d.jitterY};
    description.motionVectorScale = {d.motionScaleX, d.motionScaleY};
    description.renderSize = {d.renderWidth, d.renderHeight};
    description.enableSharpening = d.sharpen;
    description.sharpness = d.sharpness;
    description.frameTimeDelta = d.frameTimeDeltaMs;
    description.preExposure = 1.0f;
    description.reset = d.reset;
    description.cameraNear = d.cameraNear;
    description.cameraFar = d.cameraFar;
    description.cameraFovAngleVertical = d.cameraFovVertical;
    description.viewSpaceToMetersFactor = 1.0f;
    const auto result = ffxmFsr2ContextDispatch(&impl_->asr, &description);
    if (result != FFXM_OK) {
      diagnostic = "Arm ASR recusou o dispatch (código " + std::to_string(static_cast<long long>(result)) + ").";
      return false;
    }
    return true;
  }
#endif
#if AETHER_FSR2
  if (desc_.backend == TemporalUpscalerBackend::Fsr2) {
    const auto resource = [&](const TemporalUpscalerImage &image, FfxResourceStates state) {
      if (!image.image) return FfxResource{};
      return ffxGetTextureResourceVK(&impl_->fsr2, image.image, image.view, image.width, image.height,
                                     image.format, nullptr, state);
    };
    FfxFsr2DispatchDescription description{};
    description.commandList = ffxGetCommandListVK(d.commandBuffer);
    description.color = resource(d.color, FFX_RESOURCE_STATE_COMPUTE_READ);
    description.depth = resource(d.depth, FFX_RESOURCE_STATE_COMPUTE_READ);
    description.motionVectors = resource(d.velocity, FFX_RESOURCE_STATE_COMPUTE_READ);
    description.exposure = resource(d.exposure, FFX_RESOURCE_STATE_COMPUTE_READ);
    description.reactive = resource(d.reactive, FFX_RESOURCE_STATE_COMPUTE_READ);
    description.transparencyAndComposition = resource(d.composition, FFX_RESOURCE_STATE_COMPUTE_READ);
    description.output = resource(d.output, FFX_RESOURCE_STATE_UNORDERED_ACCESS);
    description.jitterOffset = {d.jitterX, d.jitterY};
    description.motionVectorScale = {d.motionScaleX, d.motionScaleY};
    description.renderSize = {d.renderWidth, d.renderHeight};
    description.enableSharpening = d.sharpen;
    description.sharpness = d.sharpness;
    description.frameTimeDelta = d.frameTimeDeltaMs;
    description.preExposure = 1.0f;
    description.reset = d.reset;
    description.cameraNear = d.cameraNear;
    description.cameraFar = d.cameraFar;
    description.cameraFovAngleVertical = d.cameraFovVertical;
    description.viewSpaceToMetersFactor = 1.0f;
    const auto result = ffxFsr2ContextDispatch(&impl_->fsr2, &description);
    if (result != FFX_OK) {
      diagnostic = "AMD FSR 2 recusou o dispatch (código " + std::to_string(static_cast<long long>(result)) + ").";
      return false;
    }
    return true;
  }
#endif
  (void)d;
  diagnostic = "Biblioteca do ampliador temporal ausente deste binário.";
  return false;
}

void TemporalUpscaler::destroy() noexcept {
  if (!impl_ || !impl_->created) return;
#if AETHER_ARM_ASR
  if (desc_.backend == TemporalUpscalerBackend::ArmAsr) arm::ffxmFsr2ContextDestroy(&impl_->asr);
#endif
#if AETHER_FSR2
  if (desc_.backend == TemporalUpscalerBackend::Fsr2) ffxFsr2ContextDestroy(&impl_->fsr2);
#endif
  impl_->created = false;
  impl_->scratch.clear();
}

} // namespace ae::rhi
