// Probe de Release para GAP-PHY-01. Em Debug o próprio Jolt aborta antes de
// Update devolver EPhysicsUpdateError; em Release precisamos provar o outro
// contrato: o processo continua, a flag retorna e os contadores são atualizados.
#include "physics/jolt_bridge.h"

#include <cstdio>

int main() {
  AetherPhysicsWorldDescV2 desc{};
  desc.structSize = sizeof(desc);
  desc.apiVersion = AetherPhysicsWorldApiVersionV2;
  desc.gravity = {0.0f, -9.81f, 0.0f};
  desc.maxBodies = 64;
  desc.maxBodyPairs = 4;
  desc.maxContactConstraints = 4;
  desc.maxBroadPhasePairs = 1024;
  desc.overflowPolicy = AetherPhysicsOverflowPolicy::Warning;

  AetherPhysicsWorld *world = AetherPhysics_CreateWorldV2(&desc);
  if (world == nullptr) return 2;

  AetherBodyDesc body{};
  body.shape.kind = AetherShapeKind::Sphere;
  body.shape.sphereRadius = 0.5f;
  body.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
  body.motionType = AetherMotionType::Dynamic;
  body.friction = 0.5f;
  for (ae::u32 i = 0; i < desc.maxBodies; ++i) {
    body.position = {0.0f, static_cast<float>(i % 2) * 0.01f, 0.0f};
    if (AetherPhysics_CreateBody(world, &body) == AetherBodyHandle_Invalid) {
      AetherPhysics_DestroyWorld(world);
      return 3;
    }
  }

  const ae::u32 flags = AetherPhysics_StepV2(world, 1.0f / 60.0f, 1);
  AetherPhysicsStepStatsV2 stats{};
  stats.structSize = sizeof(stats);
  stats.apiVersion = AetherPhysicsWorldApiVersionV2;
  const bool statsOk = AetherPhysics_GetStepStatsV2(world, &stats) == 1;

  std::printf("flags=0x%08x totalSteps=%llu overflowSteps=%llu manifold=%llu bodyPairs=%llu contacts=%llu\n",
              flags, static_cast<unsigned long long>(stats.totalSteps),
              static_cast<unsigned long long>(stats.overflowSteps),
              static_cast<unsigned long long>(stats.manifoldCacheFullCount),
              static_cast<unsigned long long>(stats.bodyPairCacheFullCount),
              static_cast<unsigned long long>(stats.contactConstraintsFullCount));

  const bool passed = flags != 0 && statsOk && stats.totalSteps == 1 && stats.overflowSteps == 1 &&
                      stats.lastErrorFlags == flags &&
                      stats.manifoldCacheFullCount + stats.bodyPairCacheFullCount +
                              stats.contactConstraintsFullCount >
                          0;
  AetherPhysics_DestroyWorld(world);
  return passed ? 0 : 4;
}
