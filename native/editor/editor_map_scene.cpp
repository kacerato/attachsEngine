#include "editor/editor_water_body_component.h"
#include "editor/editor_route_component.h"
#include "editor/editor_map_scene.h"
#include "renderer/water_authoring_geometry.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace ae::editor {
namespace {
void multiply(const float a[16], const float b[16], float out[16]) {
  float value[16]{};
  for (u32 c=0;c<4;++c) for (u32 r=0;r<4;++r)
    for (u32 k=0;k<4;++k) value[c*4+r] += a[k*4+r]*b[c*4+k];
  std::copy(value,value+16,out);
}
bool inheritedVisible(const EditorDocument &document, EditorEntityId id) {
  while (const auto *entity = document.find(id)) {
    if (!entity->active || !entity->visible) return false;
    id = entity->parent;
  }
  return true;
}
}
void editorTransformMatrix(const EditorTransform &t, float out[16]) {
  constexpr float radians = 0.0174532925199433f;
  const float x=t.rotationDegrees[0]*radians, y=t.rotationDegrees[1]*radians, z=t.rotationDegrees[2]*radians;
  const float cx=std::cos(x),sx=std::sin(x),cy=std::cos(y),sy=std::sin(y),cz=std::cos(z),sz=std::sin(z);
  const float matrix[16]{cy*cz,cy*sz,-sy,0, sx*sy*cz-cx*sz,sx*sy*sz+cx*cz,sx*cy,0,
    cx*sy*cz+sx*sz,cx*sy*sz-sx*cz,cx*cy,0,t.position[0],t.position[1],t.position[2],1};
  std::copy(matrix,matrix+16,out);
  for(u32 c=0;c<3;++c) for(u32 r=0;r<3;++r) out[c*4+r]*=t.scale[c];
}
bool editorWorldMatrix(const EditorDocument &document, EditorEntityId id, float out[16]) {
  const auto *entity=document.find(id);
  if(!entity) return false;
  editorTransformMatrix(entity->transform,out);
  for(u32 depth=0;entity->parent!=kInvalidEntity && depth<document.entityCount();++depth) {
    entity=document.find(entity->parent);
    if(!entity) return false;
    float parent[16];editorTransformMatrix(entity->transform,parent);multiply(parent,out,out);
  }
  for(u32 i=0;i<16;++i) if(!std::isfinite(out[i])) return false;
  return true;
}
bool editorLocalTransformForWorld(const float world[16],const float parent[16],EditorTransform &out) {
  float normal[12];if(!renderer::buildNormalMatrix(parent,normal)) return false;
  float inverse[16]{};inverse[15]=1;
  for(u32 r=0;r<3;++r) for(u32 c=0;c<3;++c) inverse[c*4+r]=normal[r*4+c];
  for(u32 r=0;r<3;++r) for(u32 c=0;c<3;++c) inverse[12+r]-=inverse[c*4+r]*parent[12+c];
  float local[16];multiply(inverse,world,local);
  EditorTransform value;
  for(u32 c=0;c<3;++c) {
    value.position[c]=local[12+c];
    value.scale[c]=std::sqrt(local[c*4]*local[c*4]+local[c*4+1]*local[c*4+1]+local[c*4+2]*local[c*4+2]);
    if(!std::isfinite(value.scale[c]) || value.scale[c]<.001f) return false;
  }
  const float pitch=std::asin(std::clamp(-local[2]/value.scale[0],-1.0f,1.0f));
  const bool pole=std::abs(std::cos(pitch))<.00001f;
  constexpr float degrees=57.29577951308232f;
  value.rotationDegrees[1]=pitch*degrees;
  value.rotationDegrees[0]=(pole?std::atan2(-local[9]/value.scale[2],local[5]/value.scale[1]):
                                 std::atan2(local[6]/value.scale[1],local[10]/value.scale[2]))*degrees;
  value.rotationDegrees[2]=pole?0:std::atan2(local[1]/value.scale[0],local[0]/value.scale[0])*degrees;
  float reconstructed[16];editorTransformMatrix(value,reconstructed);
  // Reject shear/reflection instead of silently losing the old world transform.
  for(u32 i=0;i<16;++i)
    if(!std::isfinite(local[i]) || std::abs(local[i]-reconstructed[i])>.0001f*std::max(1.0f,std::abs(local[i]))) return false;
  if(!isTransformValid(value)) return false;
  out=value;return true;
}

bool EditorMapScene::import(EditorDocument &document, std::span<const renderer::MapDrawRecord> draws, std::span<const renderer::MapMaterialRecord> materials, bool instantiate, std::span<const u8> vertices, std::span<const u32> indices) {
  if(draws.size()+1>EditorDocument::kMaximumEntities) return false;
  std::vector<std::shared_ptr<const EditorPickMesh>> meshes(draws.size());
  if(vertices.empty()!=indices.empty() || vertices.size()%renderer::MapVertexStride) return false;
  usize triangleCount=0;
  if(!vertices.empty()) for(u32 index=0;index<draws.size();++index) {
    const auto &draw=draws[index];triangleCount+=draw.indexCount/3;
    if(!draw.indexCount || draw.indexCount%3 || triangleCount>EditorPickMesh::MaximumTriangles ||
       u64(draw.firstIndex)+draw.indexCount>indices.size()) return false;
    std::vector<EditorPickMesh::Triangle> triangles(draw.indexCount/3);
    for(u32 t=0;t<triangles.size();++t) for(u32 point=0;point<3;++point) {
      const auto vertex=static_cast<long long>(draw.vertexOffset)+indices[draw.firstIndex+t*3+point];
      if(vertex<0 || static_cast<u64>(vertex)>=vertices.size()/renderer::MapVertexStride) return false;
      std::memcpy(triangles[t].data()+point*3,vertices.data()+vertex*renderer::MapVertexStride,3*sizeof(float));
    }
    auto mesh=std::make_shared<EditorPickMesh>();if(!mesh->build(std::move(triangles))) return false;
    meshes[index]=std::move(mesh);
  }
  EditorDocument prepared;
  for(u32 index=0;instantiate && index<draws.size();++index) {
    const auto &draw=draws[index];
    if(draw.materialIndex<materials.size() && (materials[draw.materialIndex].flags & renderer::WaterAuthoringResource)) continue;
    const u32 flags=draw.materialIndex<materials.size()?materials[draw.materialIndex].flags:0;
    const bool water=(flags & renderer::MapMaterialWater)!=0;
    char name[64];std::snprintf(name,sizeof(name),"Mesh %u / material %u / LOD %u",index,draw.materialIndex,draw.lodLevel);
    const auto id=prepared.createEntity(prepared.root(),water?EditorEntityKind::Water:EditorEntityKind::Mesh,name);
    if(!id) return false;
    auto entity=*prepared.find(id);
    auto *render=editMeshRenderer(entity);if(!render) return false;render->mesh=index+1;entity.visible=draw.lodLevel==0;
    if(!setWaterBodyFlags(entity,true,(flags & renderer::MapMaterialWaterCameraGrid)!=0)) return false;
    if(draw.materialIndex<materials.size()) render->material=renderer::materialOverrideFrom(materials[draw.materialIndex]);
    std::copy(draw.boundsCenter,draw.boundsCenter+3,entity.transform.position);
    if(!prepared.applyEntityValues(id,entity)) return false;
  }
  pickMeshes_=std::move(meshes);
  source_.assign(draws.begin(),draws.end());
  materials_.assign(materials.begin(),materials.end());
  document=std::move(prepared);
  return true;
}
renderer::MaterialOverride EditorMapScene::materialForAsset(u32 index) const {
  if(index>=source_.size() || source_[index].materialIndex>=materials_.size()) return {};
  return renderer::materialOverrideFrom(materials_[source_[index].materialIndex]);
}
void EditorMapScene::hydrateMaterials(EditorDocument &document) const {
  std::vector<EditorEntityId> ids;document.collectSubtree(document.root(),ids);
  for(auto id:ids) {
    auto value=*document.find(id);
    if(!meshAsset(value) || meshMaterial(value).enabled) continue;
    auto *render=editMeshRenderer(value);if(!render) continue;render->material=materialForAsset(meshAsset(value)-1);document.applyEntityValues(id,value);
  }
}
bool EditorMapScene::bounds(const EditorDocument &document, EditorEntityId id, float center[3], float &radius) const {
  const auto *entity=document.find(id);
  if(!entity || !meshAsset(*entity) || meshAsset(*entity)>source_.size()) return false;
  float world[16];if(!editorWorldMatrix(document,id,world)) return false;
  if(waterRoute(*entity).count) {
      if(!renderer::validateWaterRoute(waterRoute(*entity))) return false;
      float low[3]{1e30f,1e30f,1e30f},high[3]{-1e30f,-1e30f,-1e30f};
      for(u32 segment=0;segment+1<waterRoute(*entity).count;++segment) for(u32 step=0;step<=renderer::WaterRouteSteps;++step) {
        const auto sample=renderer::evaluateWaterRoute(waterRoute(*entity),segment,float(step)/renderer::WaterRouteSteps);
        for(float side:{-.5f,.5f}) for(u32 axis=0;axis<3;++axis) {
        const float x=sample.center.x+sample.tangent.z*sample.width*side,z=sample.center.z-sample.tangent.x*sample.width*side;
        const float p=world[12+axis]+world[axis]*x+world[4+axis]*sample.center.y+world[8+axis]*z;
        low[axis]=std::min(low[axis],p);high[axis]=std::max(high[axis],p);
        }
      }
    radius=0;for(u32 axis=0;axis<3;++axis) {center[axis]=(low[axis]+high[axis])*.5f;radius+=(high[axis]-low[axis])*(high[axis]-low[axis])*.25f;}
    radius=std::sqrt(radius);return true;
  }
  std::copy(world+12,world+15,center);
  float one=0,inf=0;
  for(u32 i=0;i<3;++i) {
    float col=0,row=0;
    for(u32 j=0;j<3;++j) {col+=std::abs(world[i*4+j]);row+=std::abs(world[j*4+i]);}
    one=std::max(one,col);inf=std::max(inf,row);
  }
  radius=source_[meshAsset(*entity)-1].boundsRadius*std::sqrt(one*inf);
  return std::isfinite(radius);
}
bool EditorMapScene::localGeometry(u32 assetId,std::span<const EditorPickMesh::Triangle> &triangles,float relative[16]) const {
  if(!assetId||assetId>pickMeshes_.size()||!pickMeshes_[assetId-1]) return false;
  const auto &source=source_[assetId-1];std::copy(source.model,source.model+16,relative);
  for(u32 axis=0;axis<3;++axis) relative[12+axis]-=source.boundsCenter[axis];
  triangles=pickMeshes_[assetId-1]->triangles();return !triangles.empty();
}
bool EditorMapScene::pickGeometry(const EditorDocument &document,EditorEntityId id,EditorPickCandidate &out) const {
  const auto *entity=document.find(id);
  if(!entity || !meshAsset(*entity) || meshAsset(*entity)>pickMeshes_.size()) return false;
  const auto index=meshAsset(*entity)-1;
  if(!pickMeshes_[index]) return false;
  float world[16],relative[16];if(!editorWorldMatrix(document,id,world)) return false;
  const auto &source=source_[index];std::copy(source.model,source.model+16,relative);
  for(u32 axis=0;axis<3;++axis) relative[12+axis]-=source.boundsCenter[axis];
  multiply(world,relative,out.model);out.mesh=pickMeshes_[index];return true;
}
bool EditorMapScene::extract(const EditorDocument &document, std::vector<EditorMapUpdate> &out) const {
  std::vector<EditorMapUpdate> prepared(source_.size());
  for(u32 i=0;i<source_.size();++i) {
    prepared[i].sourceDrawIndex=i;prepared[i].pose.drawIndex=i;prepared[i].pose.draw=source_[i];prepared[i].visible=false;
    const float tint[4]{1,1,1,1};
    if(!renderer::buildGpuMeshInstance(source_[i].model,tint,&prepared[i].pose.instance)) return false;
  }
  std::vector<EditorEntityId> ids;document.collectSubtree(document.root(),ids);
  std::vector<bool> seen(source_.size());
  for(auto id:ids) {
    const auto *entity=document.find(id);
    if(!entity || !meshAsset(*entity)) continue;
    const u32 index=meshAsset(*entity)-1;
    if(index>=source_.size()) return false;
    u32 target=index;
    if(seen[index]) {
      target=static_cast<u32>(prepared.size());
      prepared.push_back(prepared[index]);
    }
    seen[index]=true;
    auto &update=prepared[target];const auto &source=source_[index];
    update.objectId=id;
    update.sourceDrawIndex=index;update.pose.drawIndex=target;
    update.pose.draw.lodGroupId=target;update.pose.draw.lodLevel=0;
    float world[16],relative[16];
    if(!editorWorldMatrix(document,id,world)) return false;
    std::copy(source.model,source.model+16,relative);
    for(u32 a=0;a<3;++a) relative[12+a]-=source.boundsCenter[a];
    multiply(world,relative,update.pose.draw.model);
    const float tint[4]{1,1,1,1};
    if(!renderer::buildGpuMeshInstance(update.pose.draw.model,tint,&update.pose.instance)) return false;
    if(!bounds(document,id,update.pose.draw.boundsCenter,update.pose.draw.boundsRadius)) return false;
    update.visible=inheritedVisible(document,id) && meshRenderer(*entity)->enabled;
    update.castShadow=entity->castShadow;
    update.material=meshMaterial(*entity);
    const auto &body=waterBody(*entity);
    update.waterLayers[0]=body.waveGain;update.waterLayers[1]=body.foamGain;
    update.waterLayers[2]=body.rippleGain;update.waterLayers[3]=body.opticalGain;
    update.waterFlowDepth[0]=waterBody(*entity).currentX;update.waterFlowDepth[1]=waterBody(*entity).currentZ;
    update.waterFlowDepth[2]=waterBody(*entity).depth;
    if(waterRoute(*entity).count) update.route=std::make_shared<const renderer::WaterRoute>(waterRoute(*entity));
  }
  out=std::move(prepared);
  return true;
}
} // namespace ae::editor
