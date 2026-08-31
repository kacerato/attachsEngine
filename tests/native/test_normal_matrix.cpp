#include "harness.h"
#include "renderer/gpu_cost_isolation.h"
#include "renderer/gpu_mesh_instance.h"
#include "renderer/normal_matrix.h"

#include <cmath>
#include <limits>
#include <string_view>

using namespace ae::renderer;

namespace {
bool near(float actual, float expected, float tolerance = 1.0e-5f) {
  return std::fabs(actual - expected) <= tolerance;
}
}

AE_TEST(Normal_matrix_preserves_identity_and_handedness) {
  const float model[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 3,4,5,1};
  float normal[12]{};
  AE_EXPECT_TRUE(buildNormalMatrix(model, normal), "identity transform");
  AE_EXPECT_TRUE(near(normal[0],1) && near(normal[5],1) && near(normal[10],1), "identity diagonal");
  AE_EXPECT_TRUE(near(normal[1],0) && near(normal[4],0) && near(normal[8],0), "identity off diagonal");
  AE_EXPECT_EQ(normal[3], 1.0f, "positive handedness");
}

AE_TEST(Normal_matrix_handles_non_uniform_and_tiny_scale) {
  const float model[16] = {2,0,0,0, 0,4,0,0, 0,0,0.5f,0, 0,0,0,1};
  float normal[12]{};
  AE_EXPECT_TRUE(buildNormalMatrix(model, normal), "non-uniform scale");
  AE_EXPECT_TRUE(near(normal[0],.5f) && near(normal[5],.25f) && near(normal[10],2.0f), "reciprocal scale");

  const float tiny[16] = {1e-6f,0,0,0, 0,1e-6f,0,0, 0,0,1e-6f,0, 0,0,0,1};
  AE_EXPECT_TRUE(buildNormalMatrix(tiny, normal), "scale-aware singularity check");
  AE_EXPECT_TRUE(near(normal[0],1e6f,1.0f), "tiny reciprocal scale");
}

AE_TEST(Normal_matrix_preserves_mirror_sign_and_rejects_invalid_input) {
  const float mirrored[16] = {-2,0,0,0, 0,3,0,0, 0,0,4,0, 0,0,0,1};
  float normal[12]{};
  AE_EXPECT_TRUE(buildNormalMatrix(mirrored, normal), "mirrored transform");
  AE_EXPECT_EQ(normal[3], -1.0f, "negative handedness");
  AE_EXPECT_TRUE(near(normal[0],-.5f) && near(normal[5],1.0f/3.0f) && near(normal[10],.25f), "mirrored inverse transpose");

  float untouched[12];
  for (float &value : untouched) value = 7.0f;
  const float singular[16] = {1,0,0,0, 0,0,0,0, 0,0,1,0, 0,0,0,1};
  AE_EXPECT_TRUE(!buildNormalMatrix(singular, untouched), "singular transform rejected");
  AE_EXPECT_EQ(untouched[0], 7.0f, "failure is atomic");
  float invalid[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
  invalid[5] = std::numeric_limits<float>::quiet_NaN();
  AE_EXPECT_TRUE(!buildNormalMatrix(invalid, untouched), "non-finite transform rejected");
}

AE_TEST(Gpu_mesh_instance_build_is_generic_and_atomic) {
  const float model[16] = {2,0,0,0, 0,4,0,0, 0,0,.5f,0, 7,8,9,1};
  const float tint[4] = {.1f,.2f,.3f,.4f};
  GpuMeshInstance instance{};
  AE_EXPECT_TRUE(buildGpuMeshInstance(model, tint, &instance), "valid generic GPU instance");
  AE_EXPECT_TRUE(near(instance.model[12], 7.0f) && near(instance.tint[2], .3f), "source data preserved");
  AE_EXPECT_TRUE(near(instance.normalColumns[0], .5f) && near(instance.normalColumns[5], .25f) &&
                 near(instance.normalColumns[10], 2.0f), "derived normal matrix prepared once");

  for (float &value : instance.model) value = 9.0f;
  const float singular[16] = {1,0,0,0, 0,0,0,0, 0,0,1,0, 0,0,0,1};
  AE_EXPECT_TRUE(!buildGpuMeshInstance(singular, tint, &instance), "invalid generic instance rejected");
  AE_EXPECT_EQ(instance.model[0], 9.0f, "failed conversion leaves destination untouched");
}

AE_TEST(Gpu_cost_isolation_is_explicit_and_fails_to_full_quality) {
  AE_EXPECT_EQ(sanitizeGpuCostIsolation(0), GpuCostIsolation::Full,
               "zero is production quality");
  AE_EXPECT_EQ(sanitizeGpuCostIsolation(2), GpuCostIsolation::NoSpecularEnvironment,
               "known diagnostic mode");
  AE_EXPECT_EQ(sanitizeGpuCostIsolation(99), GpuCostIsolation::Full,
               "unknown input cannot reduce quality");
  AE_EXPECT_TRUE(std::string_view(gpuCostIsolationName(GpuCostIsolation::BaseColorOnly)) ==
                     "base-color",
                 "stable report identity");
}
