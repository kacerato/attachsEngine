#pragma once

#include "physics/jolt_bridge.h"
#include <span>
#include <string>
#include <vector>

namespace ae::physics {

struct CookedConvexHull {
  std::vector<AetherVec3> vertices;
  std::vector<u32> indices;
  u32 faceCount = 0;
};

bool validMeshCooking(const AetherMeshCookingV1 &settings) noexcept;

// Produz exatamente as faces do ConvexHullShape criado pelo Jolt. Os pontos de
// saída voltam ao referencial autoral da entrada; `indices` triangula cada face
// apenas para visualização e diagnóstico, sem trocar o shape usado no solver.
bool cookConvexHull(std::span<const AetherVec3> points,
                    const AetherMeshCookingV1 &settings,
                    CookedConvexHull &out,std::string &diagnostic);

} // namespace ae::physics
