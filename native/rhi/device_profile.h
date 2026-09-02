// Detecção de capacidades e perfis de dispositivo (S/A/B/C).
//
// Lógica pura: recebe uma estrutura de features/limits já preenchida
// (normalmente por vkGetPhysicalDeviceFeatures2/vkGetPhysicalDeviceProperties2,
// mas isso é responsabilidade do caller — aqui não tocamos VkPhysicalDevice)
// e decide o perfil e quais caminhos ficam habilitados. Por não depender de
// device, é 100% testável nesta máquina sem GPU.
#pragma once

#include "core/base.h"

namespace ae::rhi {

// Subconjunto de features/limits Vulkan relevantes para a decisão de perfil.
// Preenchido a partir de VkPhysicalDeviceFeatures2/VkPhysicalDeviceLimits
// reais no código de inicialização do device (não testado aqui); os testes
// preenchem esta struct à mão.
struct DeviceFeatures {
  // Núcleo Vulkan 1.3 (linha de base assumida pelo plano).
  bool vulkan1_3 = false;

  // Extensões / features que definem o teto de cada perfil.
  bool descriptorIndexing = false;   // bindless
  bool bindlessNonUniformIndexing = false;
  bool rayQuery = false;             // VK_KHR_ray_query
  bool meshShader = false;           // VK_EXT_mesh_shader
  bool variableRateShading = false;  // VRS / fragment shading rate
  bool memorylessAttachments = false; // lazily-allocated (TRANSIENT_ATTACHMENT)
  bool computeShaders = false;
  bool dedicatedComputeQueue = false;
  bool multiDrawIndirect = false;
  bool drawIndirectFirstInstance = false;

  // Limites relevantes.
  u32 maxBoundDescriptorSets = 4;
  u64 deviceMemoryBudgetBytes = 0; // 0 = desconhecido/não informado
};

enum class DeviceProfile : u32 {
  // C: linha de base — sem bindless, sem RT, geometria simples.
  C = 0,
  // B: bindless disponível, sem ray tracing.
  B = 1,
  // A: bindless + mesh shader e/ou VRS, sem ray query.
  A = 2,
  // S: topo — bindless, ray query, mesh shader e VRS.
  S = 3,
};

// Quais caminhos de renderização ficam habilitados para o perfil detectado.
// Corresponde à Parte 2.4 do plano: cada perfil liga um subconjunto de
// caminhos, nunca infere features que a struct não reportou.
struct EnabledPaths {
  bool bindless = false;
  bool rayQueryGI = false;      // GI nível 3 (ray query)
  bool meshShaderPipeline = false;
  bool variableRateShading = false;
  bool memorylessGBuffer = false; // anexos memoryless para o G-buffer
  bool compute = false;
  bool asyncCompute = false;
};

// Decide o perfil a partir das features reportadas. Puramente combinacional
// — sem I/O, sem estado global — para ser trivial de testar exaustivamente.
DeviceProfile classifyDeviceProfile(const DeviceFeatures &features);

// Deriva quais caminhos ficam habilitados para um perfil (e, quando o
// caminho depende de uma feature que o perfil normalmente tem mas o
// dispositivo real não reportou, respeita a feature real — perfil é um
// resumo, não uma promessa).
EnabledPaths derivePaths(DeviceProfile profile, const DeviceFeatures &features);

} // namespace ae::rhi
