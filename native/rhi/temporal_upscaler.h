// Ampliadores temporais de terceiros atrás de um contrato só (G6-B).
//
// Arm ASR (Arm Accuracy Super Resolution, derivado do FSR 2.2.2 e otimizado
// para GPUs móveis, com passes de fragmento em 16 bits) e AMD FSR 2 (v2.2.1,
// passes de compute) recebem exatamente as mesmas entradas do renderer:
// cor HDR linear com jitter, profundidade [0,1], vetor RG16F atual->anterior
// sem jitter (temporal_projection.glsl), máscaras R8 de reatividade e de
// transparência/composição e a exposição 1x1 que o tonemap usa.
//
// O que este tipo NÃO faz: escolher fallback. Uma criação ou um dispatch que
// falha devolve falso com o motivo; o renderer decide recusar e informar,
// nunca trocar por TAA ou FSR 1 em silêncio.
//
// Fora do Android as bibliotecas não entram no binário (AETHER_ARM_ASR e
// AETHER_FSR2 ausentes): `compiledIn` é falso e a política recusa com
// NotBuilt, que é o que o host e os testes observam.
#pragma once

#include "core/base.h"

#include <vulkan/vulkan.h>

#include <memory>
#include <string>

namespace ae::rhi {

enum class TemporalUpscalerBackend : u32 { ArmAsr = 0, Fsr2 = 1 };

// Recursos que as bibliotecas exigem do dispositivo, consultados e HABILITADOS
// na criação do VkDevice (rhi/device.cpp). Suporte físico sem habilitação não
// conta: um shader com Float16 num device que não pediu a feature é inválido.
struct TemporalUpscalerDeviceFeatures {
  bool shaderFloat16 = false;
  bool shaderInt16 = false;
  bool storageImageExtendedFormats = false;
  bool storageImageWriteWithoutFormat = false;
  bool computeSubgroupBasic = false;
  bool computeSubgroupQuad = false;
};

struct TemporalUpscalerImage {
  VkImage image = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  VkFormat format = VK_FORMAT_UNDEFINED;
  u32 width = 0, height = 0;
};

struct TemporalUpscalerContextDesc {
  TemporalUpscalerBackend backend = TemporalUpscalerBackend::ArmAsr;
  u32 maxRenderWidth = 0, maxRenderHeight = 0;
  u32 displayWidth = 0, displayHeight = 0;
  // 0 Quality, 1 Balanced, 2 Performance, 3 Ultra Performance (só Arm ASR).
  u32 shaderQuality = 0;
  bool dynamicResolution = false;
};

struct TemporalUpscalerDispatch {
  VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
  TemporalUpscalerImage color, depth, velocity, reactive, composition, exposure, output;
  u32 renderWidth = 0, renderHeight = 0;
  // Jitter aplicado à projeção, em pixels da extensão renderizada (convenção
  // das bibliotecas: +y desce na imagem).
  float jitterX = 0, jitterY = 0;
  // Converte o vetor em pixels: extensão renderizada, pois o renderer publica UV.
  float motionScaleX = 0, motionScaleY = 0;
  float frameTimeDeltaMs = 16.67f;
  bool sharpen = false;
  float sharpness = 0;
  bool reset = false;
  float cameraNear = 0.1f, cameraFar = 1000.0f, cameraFovVertical = 1.0f;
};

class TemporalUpscaler final {
public:
  TemporalUpscaler();
  ~TemporalUpscaler();
  TemporalUpscaler(const TemporalUpscaler &) = delete;
  TemporalUpscaler &operator=(const TemporalUpscaler &) = delete;

  static bool compiledIn(TemporalUpscalerBackend backend) noexcept;
  // Layout em que a saída fica depois do dispatch. O Arm ASR devolve cada
  // recurso ao estado declarado; o FSR 2 deixa a saída em GENERAL.
  static VkImageLayout outputLayout(TemporalUpscalerBackend backend) noexcept;
  static VkImageUsageFlags outputUsage(TemporalUpscalerBackend backend) noexcept;
  // Sequência de jitter recomendada pela biblioteca (Halton 2,3).
  static u32 jitterPhaseCount(TemporalUpscalerBackend backend, u32 renderWidth, u32 displayWidth) noexcept;
  static void jitterOffset(TemporalUpscalerBackend backend, u32 index, u32 phaseCount,
                           float &x, float &y) noexcept;

  // A instância resolve as consultas Vulkan 1.1 que as bibliotecas fazem
  // (o stub libvulkan do NDK para minSdk 26 não as exporta estaticamente).
  bool create(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice,
              PFN_vkGetDeviceProcAddr getDeviceProcAddr,
              const TemporalUpscalerContextDesc &desc, std::string &diagnostic);
  // Grava os passes da biblioteca em `dispatch.commandBuffer`, fora de
  // qualquer render pass. As entradas precisam estar em SHADER_READ_ONLY e a
  // saída em `outputLayout(backend)`.
  bool dispatch(const TemporalUpscalerDispatch &dispatch, std::string &diagnostic);
  // Exige GPU ociosa, como documentam as duas bibliotecas.
  void destroy() noexcept;
  bool ready() const noexcept;
  TemporalUpscalerBackend backend() const noexcept { return desc_.backend; }
  const TemporalUpscalerContextDesc &description() const noexcept { return desc_; }

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  TemporalUpscalerContextDesc desc_{};
};

} // namespace ae::rhi
