#include "harness.h"

#include "physics/jolt_bridge.h"

#include <cstddef>

namespace {

AetherPhysicsWorldDescV2 DenseWorldDesc(ae::u32 dynamicBodies,
                                        AetherPhysicsOverflowPolicy policy =
                                            AetherPhysicsOverflowPolicy::BuildDefault) {
  AetherPhysicsWorldDescV2 desc{};
  desc.structSize = sizeof(desc);
  desc.apiVersion = AetherPhysicsWorldApiVersionV2;
  desc.gravity = {0.0f, -9.81f, 0.0f};
  desc.maxBodies = dynamicBodies + 1;
  desc.maxBodyPairs = dynamicBodies * 16;
  desc.maxContactConstraints = dynamicBodies * 8;
  desc.maxBroadPhasePairs = dynamicBodies < 1024 ? 16384 : dynamicBodies * 16;
  desc.overflowPolicy = policy;
  return desc;
}

AetherBodyHandle AddSphere(AetherPhysicsWorld *world, AetherVec3 position) {
  AetherBodyDesc desc{};
  desc.shape.kind = AetherShapeKind::Sphere;
  desc.shape.sphereRadius = 0.5f;
  desc.position = position;
  desc.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
  desc.motionType = AetherMotionType::Dynamic;
  desc.friction = 0.5f;
  return AetherPhysics_CreateBody(world, &desc);
}

bool RunDenseStack(ae::u32 bodyCount) {
  AetherPhysicsWorldDescV2 desc = DenseWorldDesc(bodyCount);
  AetherPhysicsWorld *world = AetherPhysics_CreateWorldV2(&desc);
  if (world == nullptr) return false;

  AetherBodyDesc floor{};
  floor.shape.kind = AetherShapeKind::Box;
  floor.shape.boxHalfExtent = {30.0f, 0.5f, 30.0f};
  floor.position = {0.0f, -0.5f, 0.0f};
  floor.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
  floor.motionType = AetherMotionType::Static;
  if (AetherPhysics_CreateBody(world, &floor) == AetherBodyHandle_Invalid) {
    AetherPhysics_DestroyWorld(world);
    return false;
  }

  constexpr ae::u32 perRow = 50;
  for (ae::u32 i = 0; i < bodyCount; ++i) {
    const float x = static_cast<float>(i % perRow) * 0.98f - 24.5f;
    const float y = 0.6f + static_cast<float>(i / perRow) * 0.98f;
    if (AddSphere(world, {x, y, 0.0f}) == AetherBodyHandle_Invalid) {
      AetherPhysics_DestroyWorld(world);
      return false;
    }
  }

  ae::u32 combinedFlags = 0;
  for (int step = 0; step < 3; ++step) {
    combinedFlags |= AetherPhysics_StepV2(world, 1.0f / 60.0f, 1);
  }

  AetherPhysicsStepStatsV2 stats{};
  stats.structSize = sizeof(stats);
  stats.apiVersion = AetherPhysicsWorldApiVersionV2;
  const bool statsOk = AetherPhysics_GetStepStatsV2(world, &stats) == 1;
  const bool result = combinedFlags == 0 && statsOk && stats.totalSteps == 3 &&
                      stats.overflowSteps == 0 && stats.lastErrorFlags == 0;
  AetherPhysics_DestroyWorld(world);
  return result;
}

} // namespace

static_assert(offsetof(AetherPhysicsWorldDescV2, structSize) == 0);
static_assert(offsetof(AetherPhysicsWorldDescV2, apiVersion) == sizeof(ae::u32));

AE_TEST(physics_world_desc_v2_rejeita_layout_ou_limite_invalido) {
  AetherPhysicsWorldDescV2 desc = DenseWorldDesc(16);
  desc.structSize = sizeof(desc) - 1;
  AE_EXPECT_TRUE(AetherPhysics_CreateWorldV2(&desc) == nullptr,
                 "V2 deve rejeitar descritor truncado em vez de reinterpretar ABI");

  desc = DenseWorldDesc(16);
  desc.apiVersion = 99;
  AE_EXPECT_TRUE(AetherPhysics_CreateWorldV2(&desc) == nullptr,
                 "V2 deve rejeitar versão desconhecida em vez de assumir layout");

  desc = DenseWorldDesc(16);
  desc.maxBroadPhasePairs = 1;
  AE_EXPECT_TRUE(AetherPhysics_CreateWorldV2(&desc) == nullptr,
                 "V2 deve rejeitar buffer broad phase inseguro");
}

AE_TEST(physics_v1_permanece_funcional_com_defaults_conservadores) {
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, 8);
  AE_EXPECT_TRUE(world != nullptr, "símbolo V1 deve continuar criando mundo durante migração");
  AE_EXPECT_TRUE(AddSphere(world, {0.0f, 2.0f, 0.0f}) != AetherBodyHandle_Invalid,
                 "mundo V1 deve continuar aceitando corpos");
  AetherPhysics_Step(world, 1.0f / 60.0f, 1);
  AetherPhysics_DestroyWorld(world);
}

AE_TEST(physics_v2_cenarios_densos_500_1000_5000_sem_overflow) {
  AE_EXPECT_TRUE(RunDenseStack(500), "pilha densa de 500 corpos não deve perder contatos");
  AE_EXPECT_TRUE(RunDenseStack(1000), "pilha densa de 1.000 corpos não deve perder contatos");
  AE_EXPECT_TRUE(RunDenseStack(5000), "pilha densa de 5.000 corpos não deve perder contatos");
}
