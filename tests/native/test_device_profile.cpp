#include "harness.h"
#include "rhi/device_profile.h"

using namespace ae;
using namespace ae::rhi;
using namespace ae::test;

AE_TEST(DeviceProfile_sem_vulkan_1_3_cai_para_C) {
  DeviceFeatures f;
  f.vulkan1_3 = false;
  f.descriptorIndexing = true;
  f.bindlessNonUniformIndexing = true;
  AE_EXPECT_TRUE(classifyDeviceProfile(f) == DeviceProfile::C, "sem vulkan 1.3 o dispositivo deve ficar no perfil mais conservador");
}

AE_TEST(DeviceProfile_sem_descriptor_indexing_cai_para_caminho_nao_bindless) {
  DeviceFeatures f;
  f.vulkan1_3 = true;
  f.descriptorIndexing = false;
  DeviceProfile profile = classifyDeviceProfile(f);
  EnabledPaths paths = derivePaths(profile, f);
  AE_EXPECT_TRUE(profile == DeviceProfile::C, "sem descriptor indexing o perfil deve ser C");
  AE_EXPECT_TRUE(paths.bindless == false, "sem descriptor indexing o caminho bindless nao pode ser habilitado");
}

AE_TEST(DeviceProfile_bindless_completo_sem_extras_da_perfil_B) {
  DeviceFeatures f;
  f.vulkan1_3 = true;
  f.descriptorIndexing = true;
  f.bindlessNonUniformIndexing = true;
  DeviceProfile profile = classifyDeviceProfile(f);
  EnabledPaths paths = derivePaths(profile, f);
  AE_EXPECT_TRUE(profile == DeviceProfile::B, "bindless sem mesh shader/VRS/ray query deveria ser perfil B");
  AE_EXPECT_TRUE(paths.bindless == true, "perfil B habilita bindless");
  AE_EXPECT_TRUE(paths.rayQueryGI == false, "perfil B nao tem ray query");
}

AE_TEST(DeviceProfile_bindless_mais_mesh_shader_da_perfil_A) {
  DeviceFeatures f;
  f.vulkan1_3 = true;
  f.descriptorIndexing = true;
  f.bindlessNonUniformIndexing = true;
  f.meshShader = true;
  DeviceProfile profile = classifyDeviceProfile(f);
  AE_EXPECT_TRUE(profile == DeviceProfile::A, "bindless + mesh shader sem ray query deveria ser perfil A");
}

AE_TEST(DeviceProfile_ray_query_habilita_GI_nivel_3_e_da_perfil_S) {
  DeviceFeatures f;
  f.vulkan1_3 = true;
  f.descriptorIndexing = true;
  f.bindlessNonUniformIndexing = true;
  f.rayQuery = true;
  f.meshShader = true;
  f.variableRateShading = true;
  DeviceProfile profile = classifyDeviceProfile(f);
  EnabledPaths paths = derivePaths(profile, f);
  AE_EXPECT_TRUE(profile == DeviceProfile::S, "topo completo de features deveria classificar como perfil S");
  AE_EXPECT_TRUE(paths.rayQueryGI == true, "ray query presente deveria habilitar GI nivel 3");
  AE_EXPECT_TRUE(paths.meshShaderPipeline == true, "perfil S deveria habilitar mesh shader pipeline");
  AE_EXPECT_TRUE(paths.variableRateShading == true, "perfil S deveria habilitar VRS");
}

AE_TEST(DeviceProfile_ray_query_isolado_sem_bindless_nao_promove_perfil) {
  // Um dispositivo exotico que reporta ray_query mas nao descriptor
  // indexing nao deveria ser tratado como S/A: bindless e pre-requisito
  // dos perfis acima de C neste modelo.
  DeviceFeatures f;
  f.vulkan1_3 = true;
  f.descriptorIndexing = false;
  f.rayQuery = true;
  DeviceProfile profile = classifyDeviceProfile(f);
  AE_EXPECT_TRUE(profile == DeviceProfile::C, "sem bindless o dispositivo fica em C mesmo com ray query isolado");
}

AE_TEST(DeviceProfile_memoryless_independe_do_perfil) {
  DeviceFeatures f;
  f.vulkan1_3 = true;
  f.descriptorIndexing = false;
  f.memorylessAttachments = true;
  EnabledPaths paths = derivePaths(classifyDeviceProfile(f), f);
  AE_EXPECT_TRUE(paths.memorylessGBuffer == true, "attachment memoryless e um recurso independente do perfil bindless");
}

AE_TEST(DeviceProfile_variable_rate_shading_isolado_da_perfil_A) {
  DeviceFeatures f;
  f.vulkan1_3 = true;
  f.descriptorIndexing = true;
  f.bindlessNonUniformIndexing = true;
  f.variableRateShading = true;
  DeviceProfile profile = classifyDeviceProfile(f);
  EnabledPaths paths = derivePaths(profile, f);
  AE_EXPECT_TRUE(profile == DeviceProfile::A, "VRS isolado (sem mesh shader/ray query) ja basta para perfil A");
  AE_EXPECT_TRUE(paths.variableRateShading == true, "VRS deveria estar habilitado");
}

AE_TEST(DeviceProfile_compute_e_habilitado_por_capacidade_nao_por_preset) {
  DeviceFeatures f;
  f.computeShaders = true;
  f.dedicatedComputeQueue = true;
  EnabledPaths paths = derivePaths(DeviceProfile::C, f);
  AE_EXPECT_TRUE(paths.compute, "compute deve funcionar tambem no perfil conservador quando o hardware suporta");
  AE_EXPECT_TRUE(paths.asyncCompute, "fila compute dedicada habilita o caminho assincrono");
}

AE_TEST(DeviceProfile_async_compute_nunca_liga_sem_compute) {
  DeviceFeatures f;
  f.computeShaders = false;
  f.dedicatedComputeQueue = true;
  EnabledPaths paths = derivePaths(DeviceProfile::A, f);
  AE_EXPECT_TRUE(!paths.compute, "compute ausente deve permanecer desabilitado");
  AE_EXPECT_TRUE(!paths.asyncCompute, "fila reportada isoladamente nao autoriza async compute");
}
