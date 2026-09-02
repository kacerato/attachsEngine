#include "rhi/device_profile.h"

namespace ae::rhi {

DeviceProfile classifyDeviceProfile(const DeviceFeatures &f) {
  // Vulkan 1.3 é a linha de base do plano; sem isso o dispositivo nem entra
  // na escada de perfis (tratamos como C — caminho mais conservador).
  if (!f.vulkan1_3) return DeviceProfile::C;

  bool hasBindless = f.descriptorIndexing && f.bindlessNonUniformIndexing;
  if (!hasBindless) return DeviceProfile::C;

  // S exige o topo completo: bindless + ray query + mesh shader + VRS.
  if (f.rayQuery && f.meshShader && f.variableRateShading) {
    return DeviceProfile::S;
  }

  // A: bindless com pelo menos um recurso avançado (mesh shader ou VRS),
  // mas sem ray query completo.
  if (f.meshShader || f.variableRateShading) {
    return DeviceProfile::A;
  }

  // B: bindless disponível, sem os recursos avançados de A/S.
  return DeviceProfile::B;
}

EnabledPaths derivePaths(DeviceProfile profile, const DeviceFeatures &f) {
  EnabledPaths paths;

  // Bindless liga junto com o próprio requisito que define B/A/S.
  paths.bindless = (profile != DeviceProfile::C) && f.descriptorIndexing &&
                    f.bindlessNonUniformIndexing;

  // GI nível 3 via ray query só liga se a feature realmente existe — o
  // perfil S normalmente implica isso, mas nunca inferimos além do que o
  // dispositivo reportou.
  paths.rayQueryGI = f.rayQuery;

  paths.meshShaderPipeline = f.meshShader;
  paths.variableRateShading = f.variableRateShading;

  // Memoryless G-buffer depende só da feature de attachment memoryless,
  // independente do perfil — mesmo um dispositivo perfil C pode suportar
  // TRANSIENT_ATTACHMENT (é um recurso comum em TBDR mobile).
  paths.memorylessGBuffer = f.memorylessAttachments;
  paths.compute = f.computeShaders;
  paths.asyncCompute = f.computeShaders && f.dedicatedComputeQueue;

  return paths;
}

} // namespace ae::rhi
