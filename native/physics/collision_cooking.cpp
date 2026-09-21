#include <Jolt/Jolt.h>
#include "physics/collision_cooking_internal.h"
#include "physics/jolt_init.h"

#include <algorithm>
#include <cmath>

namespace ae::physics {

bool validMeshCooking(const AetherMeshCookingV1 &settings) noexcept {
  return settings.structSize >= sizeof(AetherMeshCookingV1) && settings.apiVersion == 1 &&
         (settings.flags & ~AetherMeshCookingOptimizeRuntime) == 0 &&
         std::isfinite(settings.hullTolerance) && settings.hullTolerance >= 0.00001f &&
         settings.hullTolerance <= 1.0f && std::isfinite(settings.activeEdgeAngleDegrees) &&
         settings.activeEdgeAngleDegrees >= 0.0f && settings.activeEdgeAngleDegrees <= 90.0f;
}

namespace detail {
JPH::ShapeSettings::ShapeResult createConvexHullShape(
    std::span<const AetherVec3> points,const AetherMeshCookingV1 &settings,u64 userData) {
  ensureJoltInitialized();
  JPH::ShapeSettings::ShapeResult failure;
  if(points.size()<4 || !validMeshCooking(settings)) {
    failure.SetError("Configuração ou pontos do casco inválidos");return failure;
  }
  JPH::Array<JPH::Vec3> native;native.reserve(static_cast<JPH::uint>(points.size()));
  for(const auto &point:points) {
    if(!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z))
      { failure.SetError("Ponto não finito no casco");return failure; }
    native.emplace_back(point.x,point.y,point.z);
  }
  JPH::ConvexHullShapeSettings hull(native);
  hull.mHullTolerance=settings.hullTolerance;
  hull.mUserData=userData;
  return hull.Create();
}
} // namespace detail

bool cookConvexHull(std::span<const AetherVec3> points,const AetherMeshCookingV1 &settings,
                    CookedConvexHull &out,std::string &diagnostic) {
  out={};diagnostic.clear();
  const auto result=detail::createConvexHullShape(points,settings);
  if(result.HasError()) {diagnostic=result.GetError().c_str();return false;}
  const auto *shape=static_cast<const JPH::ConvexHullShape *>(result.Get().GetPtr());
  const auto center=shape->GetCenterOfMass();
  out.vertices.reserve(shape->GetNumPoints());
  for(JPH::uint i=0;i<shape->GetNumPoints();++i) {
    const auto point=shape->GetPoint(i)+center;
    out.vertices.push_back(AetherVec3{point.GetX(),point.GetY(),point.GetZ()});
  }
  out.faceCount=shape->GetNumFaces();
  JPH::uint face[256];
  for(JPH::uint i=0;i<shape->GetNumFaces();++i) {
    const auto count=shape->GetFaceVertices(i,256,face);
    if(count<3 || count>256) {out={};diagnostic="Face inválida no casco cozido";return false;}
    for(JPH::uint corner=1;corner+1<count;++corner) {
      out.indices.push_back(face[0]);out.indices.push_back(face[corner]);out.indices.push_back(face[corner+1]);
    }
  }
  if(out.vertices.size()<4 || out.indices.empty()) {out={};diagnostic="O casco cozido não tem volume";return false;}
  return true;
}

} // namespace ae::physics
