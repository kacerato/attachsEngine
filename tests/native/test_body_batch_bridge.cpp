#include "harness.h"
#include "physics/jolt_bridge.h"

#include <cmath>
#include <vector>

using namespace ae::test;

namespace {

AetherPhysicsWorld *LargeWorld(ae::u32 maxBodies) {
  AetherPhysicsWorldDescV2 desc{};
  desc.structSize = sizeof(desc);
  desc.apiVersion = AetherPhysicsWorldApiVersionV2;
  desc.gravity = {0.0f, 0.0f, 0.0f};
  desc.maxBodies = maxBodies;
  desc.maxBodyPairs = std::max<ae::u32>(1024, maxBodies * 2);
  desc.maxContactConstraints = std::max<ae::u32>(1024, maxBodies);
  desc.maxBroadPhasePairs = std::max<ae::u32>(16384, maxBodies * 2);
  desc.overflowPolicy = AetherPhysicsOverflowPolicy::BuildDefault;
  return AetherPhysics_CreateWorldV2(&desc);
}

AetherBodyDescV2 StaticSphere(float x) {
  AetherBodyDescV2 desc{};
  desc.structSize = sizeof(desc);
  desc.apiVersion = AetherBodyApiVersionV2;
  desc.shape.kind = AetherShapeKind::Sphere;
  desc.shape.sphereRadius = 0.1f;
  desc.position = {x, 0.0f, 0.0f};
  desc.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
  desc.motionType = AetherMotionType::Static;
  desc.friction = 0.5f;
  desc.eventLayerMask = static_cast<ae::u32>(AetherQueryLayerMask::All);
  return desc;
}

} // namespace

AE_TEST(create_bodies_v2_preserva_ordem_e_destroy_bodies_libera_capacidade) {
  constexpr int count = 128;
  AetherPhysicsWorld *world = LargeWorld(count);
  std::vector<AetherBodyDescV2> descs;
  std::vector<AetherBodyHandle> handles(count, AetherBodyHandle_Invalid);
  descs.reserve(count);
  for (int i = 0; i < count; ++i) descs.push_back(StaticSphere(static_cast<float>(i) * 0.5f));

  AE_EXPECT_TRUE(AetherPhysics_CreateBodiesV2(world, descs.data(), handles.data(), count) == count,
                 "lote válido deve criar todos os corpos");
  for (int i = 0; i < count; ++i) {
    AE_EXPECT_TRUE(handles[i] != AetherBodyHandle_Invalid, "cada posição deve receber handle válido");
    AetherVec3 position{};
    AetherPhysics_GetTransform(world, handles[i], &position, nullptr);
    AE_EXPECT_TRUE(std::fabs(position.x - descs[i].position.x) < 0.0001f,
                   "ordem dos handles precisa corresponder à ordem dos descritores");
  }

  AetherPhysics_DestroyBodies(world, handles.data(), count);
  std::fill(handles.begin(), handles.end(), AetherBodyHandle_Invalid);
  AE_EXPECT_TRUE(AetherPhysics_CreateBodiesV2(world, descs.data(), handles.data(), count) == count,
                 "destroy em lote deve devolver toda a capacidade ao mundo");
  AetherPhysics_DestroyWorld(world);
}

AE_TEST(create_bodies_v2_falha_atomicamente_quando_capacidade_acaba) {
  AetherPhysicsWorld *world = LargeWorld(4);
  std::vector<AetherBodyDescV2> five(5);
  std::vector<AetherBodyHandle> handles(5, 0);
  for (int i = 0; i < 5; ++i) five[i] = StaticSphere(static_cast<float>(i));

  AE_EXPECT_TRUE(AetherPhysics_CreateBodiesV2(world, five.data(), handles.data(), 5) == 0,
                 "lote maior que a capacidade deve falhar inteiro");
  for (AetherBodyHandle handle : handles)
    AE_EXPECT_TRUE(handle == AetherBodyHandle_Invalid,
                   "falha transacional deve invalidar todas as saídas");

  handles.resize(4);
  AE_EXPECT_TRUE(AetherPhysics_CreateBodiesV2(world, five.data(), handles.data(), 4) == 4,
                 "rollback do lote falho não pode deixar o mundo parcialmente ocupado");
  AetherPhysics_DestroyWorld(world);
}

AE_TEST(create_destroy_bodies_v2_escala_ate_dez_mil_corpos) {
  for (int count : {100, 1000, 10000}) {
    AetherPhysicsWorld *world = LargeWorld(static_cast<ae::u32>(count));
    std::vector<AetherBodyDescV2> descs;
    std::vector<AetherBodyHandle> handles(count, AetherBodyHandle_Invalid);
    descs.reserve(count);
    for (int i = 0; i < count; ++i)
      descs.push_back(StaticSphere(static_cast<float>(i % 100) * 0.4f));
    AE_EXPECT_TRUE(AetherPhysics_CreateBodiesV2(world, descs.data(), handles.data(), count) == count,
                   "cada escala contratada deve caber num único lote");
    AetherPhysics_DestroyBodies(world, handles.data(), count);
    AetherPhysics_DestroyWorld(world);
  }
}
