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
// A identidade de um desenho do pacote. Derivada e não sorteada de propósito --
// ver `EditorMapScene::assetGuid`. O prefixo textual evita que uma semente de
// outro domínio (um script, uma textura) colida com esta por acidente.
resources::AssetGuid packageAssetGuid(u64 fingerprint,u32 index) {
  char seed[64];
  std::snprintf(seed,sizeof(seed),"pacote:%llu:%u",static_cast<unsigned long long>(fingerprint),index);
  return resources::assetGuidFromSeed(std::string_view(seed));
}
void multiply(const float a[16], const float b[16], float out[16]) {
  float value[16]{};
  for (u32 c=0;c<4;++c) for (u32 r=0;r<4;++r)
    for (u32 k=0;k<4;++k) value[c*4+r] += a[k*4+r]*b[c*4+k];
  std::copy(value,value+16,out);
}
bool inheritedVisible(const runtime::SceneGraph &document, EditorEntityId id) {
  while (const auto *entity = document.find(id)) {
    if (!entity->active || !entity->visible) return false;
    id = entity->parent;
  }
  return true;
}
}
namespace {
bool buildPickMeshes(std::span<const renderer::MapDrawRecord> draws,std::span<const u8> vertices,
                     std::span<const u32> indices,std::vector<std::shared_ptr<const EditorPickMesh>> &out) {
  out.assign(draws.size(),nullptr);
  if(vertices.empty()!=indices.empty() || vertices.size()%renderer::MapVertexStride) return false;
  if(vertices.empty()) return true;
  usize triangleCount=0;
  for(u32 index=0;index<draws.size();++index) {
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
    out[index]=std::move(mesh);
  }
  return true;
}
} // namespace

bool EditorMapScene::adoptPackage(EditorDocument &document, std::span<const renderer::MapDrawRecord> draws,
                                  std::span<const renderer::MapMaterialRecord> materials,
                                  std::span<const u8> vertices, std::span<const u32> indices,
                                  std::span<const resources::AssetGuid> identities, u64 packageFingerprint) {
  std::vector<std::shared_ptr<const EditorPickMesh>> meshes;
  if(!buildPickMeshes(draws,vertices,indices,meshes)) return false;
  std::vector<resources::AssetGuid> assets(draws.size());
  for(u32 index=0;index<draws.size();++index)
    assets[index]=index<identities.size() && identities[index].valid()
        ? identities[index] : packageAssetGuid(packageFingerprint,index);
  // Duas identidades iguais no mesmo pacote fariam `assetSlot` escolher pela
  // ordem da lista — exatamente o que a identidade existe para eliminar.
  for(u32 a=0;a<assets.size();++a) for(u32 b=0;b<a;++b) if(assets[a]==assets[b]) return false;
  pickMeshes_=std::move(meshes);
  source_.assign(draws.begin(),draws.end());
  materials_.assign(materials.begin(),materials.end());
  assets_=std::move(assets);
  reconcileAssets(document);
  return true;
}

bool EditorMapScene::import(EditorDocument &document, std::span<const renderer::MapDrawRecord> draws, std::span<const renderer::MapMaterialRecord> materials, bool instantiate, std::span<const u8> vertices, std::span<const u32> indices, u64 packageFingerprint) {
  if(draws.size()+1>EditorDocument::kMaximumEntities) return false;
  std::vector<std::shared_ptr<const EditorPickMesh>> meshes;
  if(!buildPickMeshes(draws,vertices,indices,meshes)) return false;
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
    auto *render=editMeshRenderer(entity);if(!render) return false;
    render->mesh=index+1;render->asset=packageAssetGuid(packageFingerprint,index);entity.visible=draw.lodLevel==0;
    if(!setWaterBodyFlags(entity,true,(flags & renderer::MapMaterialWaterCameraGrid)!=0)) return false;
    if(draw.materialIndex<materials.size()) render->material=renderer::materialOverrideFrom(materials[draw.materialIndex]);
    std::copy(draw.boundsCenter,draw.boundsCenter+3,entity.transform.position);
    if(!prepared.applyEntityValues(id,entity)) return false;
  }
  pickMeshes_=std::move(meshes);
  source_.assign(draws.begin(),draws.end());
  materials_.assign(materials.begin(),materials.end());
  assets_.resize(draws.size());
  for(u32 index=0;index<draws.size();++index) assets_[index]=packageAssetGuid(packageFingerprint,index);
  document=std::move(prepared);
  return true;
}
u32 EditorMapScene::assetSlot(const resources::AssetGuid &guid) const {
  if(!guid.valid()) return 0;
  for(u32 index=0;index<assets_.size();++index) if(assets_[index]==guid) return index+1;
  return 0;
}
void EditorMapScene::reconcileAssets(EditorDocument &document) const {
  std::vector<EditorEntityId> ids;document.collectSubtree(document.root(),ids);
  for(auto id:ids) {
    auto value=*document.find(id);
    auto *render=editMeshRenderer(value);
    if(!render) continue;
    const auto previousSlot=render->mesh;
    const auto previousAsset=render->asset;
    if(render->asset.valid()) {
      // A identidade manda. Se ela não está neste pacote, o slot vira zero:
      // "referência ausente" é informação, e desenhar a malha que por acaso
      // ocupa o índice antigo seria corromper a cena em silêncio.
      render->mesh=assetSlot(render->asset);
    } else if(render->mesh && render->mesh<=assets_.size()) {
      // Cena anterior ao registro: ela já trazia um slot válido para ESTE
      // pacote, então a identidade derivada dele é a identidade correta.
      render->asset=assets_[render->mesh-1];
    }
    if(render->mesh!=previousSlot || !(render->asset==previousAsset)) document.applyEntityValues(id,value);
  }
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
bool EditorMapScene::bounds(const runtime::SceneGraph &document, EditorEntityId id, float center[3], float &radius) const {
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
bool EditorMapScene::pickGeometry(const runtime::SceneGraph &document,EditorEntityId id,EditorPickCandidate &out) const {
  const auto *entity=document.find(id);
  if(!entity || !meshAsset(*entity) || meshAsset(*entity)>pickMeshes_.size()) return false;
  const auto index=meshAsset(*entity)-1;
  if(!pickMeshes_[index]) return false;
  float world[16],relative[16];if(!editorWorldMatrix(document,id,world)) return false;
  const auto &source=source_[index];std::copy(source.model,source.model+16,relative);
  for(u32 axis=0;axis<3;++axis) relative[12+axis]-=source.boundsCenter[axis];
  multiply(world,relative,out.model);out.mesh=pickMeshes_[index];return true;
}
bool EditorMapScene::extract(const runtime::SceneGraph &document, std::vector<EditorMapUpdate> &out) const {
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
