#pragma once
#include "editor/editor_map_scene.h"
#include "editor/editor_physics_body.h"

namespace ae::editor {
// Conservative geometric suggestion, not object-name/semantic recognition.
// All three candidates contain every vertex (and therefore each triangle).
inline bool fitEditorCollider(const EditorMapScene &resources,const EditorDocument &document,
                              EditorEntityId id,EditorCollider &out) {
  const auto *entity=document.find(id);if(!entity) return false;
  const auto *mesh=runtime::meshRenderer(*entity);if(!mesh||!mesh->slotCount())return false;
  std::vector<std::array<float,3>> points;
  for(u32 slot=0;slot<mesh->slotCount();++slot) {
    const auto asset=mesh->slotAsset(slot).valid()?resources.assetSlot(mesh->slotAsset(slot)):mesh->slotMesh(slot);
    std::span<const EditorPickMesh::Triangle> triangles;float relative[16];
    if(!asset||!resources.localGeometry(asset,triangles,relative)||triangles.empty())return false;
    std::vector<EditorPickMesh::Triangle> pose;std::string error;
    if(entity->components.find(scene::SkinnedMesh::descriptor)) {
      if(!resources.authoredGeometry(document,id,asset,pose,error))return false;
      triangles=pose;
    }
    for(const auto &triangle:triangles)for(u32 vertex=0;vertex<3;++vertex) {
      std::array<float,3> p;
      for(u32 k=0;k<3;++k) {p[k]=relative[12+k]+relative[k]*triangle[vertex*3]+relative[4+k]*triangle[vertex*3+1]+relative[8+k]*triangle[vertex*3+2];if(!std::isfinite(p[k]))return false;}
      points.push_back(p);
    }
  }
  float low[3]{1e30f,1e30f,1e30f},high[3]{-1e30f,-1e30f,-1e30f};
  const auto visit=[&](auto consumer) {
    for(const auto &p:points)consumer(p.data());
  };
  visit([&](const float *p){for(u32 k=0;k<3;++k) {low[k]=std::min(low[k],p[k]);high[k]=std::max(high[k],p[k]);}});
  EditorCollider candidate=out;float center[3],half[3];
  for(u32 k=0;k<3;++k) {center[k]=(low[k]+high[k])*.5f;half[k]=std::max(.01f,(high[k]-low[k])*.5f);}
  candidate.rotationX=candidate.rotationY=candidate.rotationZ=0;
  candidate.centerX=center[0];candidate.centerY=center[1];candidate.centerZ=center[2];
  candidate.halfX=half[0];candidate.halfY=half[1];candidate.halfZ=half[2];
  float sphereSquared=.0001f,radialSquared=.0001f;
  visit([&](const float *p){const float x=p[0]-center[0],y=p[1]-center[1],z=p[2]-center[2];sphereSquared=std::max(sphereSquared,x*x+y*y+z*z);radialSquared=std::max(radialSquared,x*x+z*z);});
  const float sphere=std::sqrt(sphereSquared);
  candidate.halfHeight=std::max(.01f,half[1]-std::sqrt(radialSquared));
  float capsuleSquared=radialSquared;
  visit([&](const float *p){const float x=p[0]-center[0],z=p[2]-center[2],y=std::max(0.0f,std::abs(p[1]-center[1])-candidate.halfHeight);capsuleSquared=std::max(capsuleSquared,x*x+y*y+z*z);});
  const float capsule=std::sqrt(capsuleSquared);
  candidate.shape=scene::ColliderShape::Box;candidate.radius=sphere;
  float world[16],identity[16]{};identity[0]=identity[5]=identity[10]=identity[15]=1;EditorTransform transform;
  if(!editorWorldMatrix(document,id,world)||!editorLocalTransformForWorld(world,identity,transform)) return false;
  const float sx=std::abs(transform.scale[0]),sy=std::abs(transform.scale[1]),sz=std::abs(transform.scale[2]);
  if(std::abs(sx-sy)<=1e-4f*sx && std::abs(sx-sz)<=1e-4f*sx) {
    constexpr float pi=3.14159265359f;
    float volume=8*half[0]*half[1]*half[2];const float sv=4*pi*sphere*sphere*sphere/3;
    if(sv<volume) {volume=sv;candidate.shape=scene::ColliderShape::Sphere;}
    const float cv=2*pi*capsule*capsule*candidate.halfHeight+4*pi*capsule*capsule*capsule/3;
    if(cv<volume) {candidate.shape=scene::ColliderShape::Capsule;candidate.radius=capsule;}
  }
  if(!candidate.valid()) return false;
  out=candidate;return true;
}
}
