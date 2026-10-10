#pragma once
#include "renderer/primitive_geometry.h"
#include "renderer/gpu_mesh_instance.h"
#include <memory>
#include <string>

namespace ae::renderer {
enum class MaterialPreviewShape : u8 { Sphere, Cube, Plane };

// Preview-owned geometry never enters the authored package or scene document.
// Uses the same packed vertex/normal/tangent/UV contract as the PBR scene pass.
struct MaterialPreviewGeometry {
  MaterialPreviewShape shape=MaterialPreviewShape::Sphere;
  std::vector<u8> vertices;
  std::vector<u32> indices;
  MapDrawRecord draw{};
  MapMaterialRecord base{};
  GpuMeshInstance instance{};
};

inline std::shared_ptr<const MaterialPreviewGeometry> makeMaterialPreviewGeometry(MaterialPreviewShape shape) {
  const auto primitive=shape==MaterialPreviewShape::Sphere?scene::PrimitiveType::Sphere:
    shape==MaterialPreviewShape::Cube?scene::PrimitiveType::Cube:
    shape==MaterialPreviewShape::Plane?scene::PrimitiveType::Plane:scene::PrimitiveType::Count;
  if(!scene::validPrimitive(primitive)) return {};
  auto geometry=std::make_shared<MaterialPreviewGeometry>();geometry->shape=shape;
  std::vector<MapDrawRecord> draws;std::vector<MapMaterialRecord> materials;
  if(!appendPrimitiveGeometry(primitive,MapVertexStride,geometry->vertices,geometry->indices,draws,materials)||
     draws.size()!=1||materials.size()!=1||!draws[0].indexCount||draws[0].boundsRadius<=0) return {};
  geometry->draw=draws[0];geometry->base=materials[0];
  // Both genuine UV streams are authored for preview shapes. UV1 intentionally
  // matches UV0, so changing the material's coordinate selection remains useful.
  for(usize offset=0;offset<geometry->vertices.size();offset+=MapVertexStride)
    std::memcpy(geometry->vertices.data()+offset+36,geometry->vertices.data()+offset+28,8);
  // Give each shape comparable apparent size. A plane retains genuine UV 0..1,
  // normals and tangents; scaling is an instance transform, never UV rewriting.
  const float scale=.75f/geometry->draw.boundsRadius;
  float model[16]{},tint[4]{1,1,1,1};model[0]=model[5]=model[10]=scale;model[15]=1;
  if(!buildGpuMeshInstance(model,tint,&geometry->instance)) return {};
  std::copy(model,model+16,geometry->draw.model);geometry->draw.boundsRadius=.75f;
  geometry->base.flags=MapMaterialCullBackFaces;
  geometry->base.baseColorFactor[0]=geometry->base.baseColorFactor[1]=geometry->base.baseColorFactor[2]=1;
  geometry->base.baseColorFactor[3]=1;
  return geometry;
}
} // namespace ae::renderer
