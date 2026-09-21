#pragma once

#include "physics/collision_cooking.h"
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>

namespace ae::physics::detail {

JPH::ShapeSettings::ShapeResult createConvexHullShape(
    std::span<const AetherVec3> points,const AetherMeshCookingV1 &settings,u64 userData=0);

} // namespace ae::physics::detail
