#include "editor/editor_water_body_component.h"
#include "editor/editor_route_component.h"
#include "editor/editor_map_scene.h"
#include "renderer/water_authoring_geometry.h"
#include "physics/collision_cooking.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <tuple>

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
  std::map<std::tuple<u32,u32,i32>,std::shared_ptr<const EditorPickMesh>> shared;
  for(u32 index=0;index<draws.size();++index) {
    const auto &draw=draws[index];
    const auto key=std::make_tuple(draw.firstIndex,draw.indexCount,static_cast<i32>(draw.vertexOffset));
    if(const auto found=shared.find(key);found!=shared.end()) {out[index]=found->second;continue;}
    triangleCount+=draw.indexCount/3;
    if(!draw.indexCount || draw.indexCount%3 || triangleCount>EditorPickMesh::MaximumTriangles ||
       u64(draw.firstIndex)+draw.indexCount>indices.size()) return false;
    std::vector<EditorPickMesh::Triangle> triangles(draw.indexCount/3);
    for(u32 t=0;t<triangles.size();++t) for(u32 point=0;point<3;++point) {
      const auto vertex=static_cast<long long>(draw.vertexOffset)+indices[draw.firstIndex+t*3+point];
      if(vertex<0 || static_cast<u64>(vertex)>=vertices.size()/renderer::MapVertexStride) return false;
      std::memcpy(triangles[t].data()+point*3,vertices.data()+vertex*renderer::MapVertexStride,3*sizeof(float));
    }
    auto mesh=std::make_shared<EditorPickMesh>();if(!mesh->build(std::move(triangles))) return false;
    out[index]=mesh;shared.emplace(key,std::move(mesh));
  }
  return true;
}
} // namespace

void EditorMapScene::pivotOf(u32 index, float out[3]) const {
  if(index>=source_.size()) {out[0]=out[1]=out[2]=0;return;}
  if(pivots_.size()>=static_cast<usize>(index)*3+3) {std::copy(pivots_.begin()+index*3,pivots_.begin()+index*3+3,out);return;}
  std::copy(source_[index].boundsCenter,source_[index].boundsCenter+3,out);
}

bool EditorMapScene::adoptPackage(EditorDocument &document, std::span<const renderer::MapDrawRecord> draws,
                                  std::span<const renderer::MapMaterialRecord> materials,
                                  std::span<const u8> vertices, std::span<const u32> indices,
                                  std::span<const resources::AssetGuid> identities, u64 packageFingerprint,
                                  std::span<const float> pivots, std::span<const std::string> names) {
  std::vector<std::shared_ptr<const EditorPickMesh>> meshes;
  if(!buildPickMeshes(draws,vertices,indices,meshes)) return false;
  std::vector<resources::AssetGuid> assets(draws.size());
  for(u32 index=0;index<draws.size();++index)
    assets[index]=index<identities.size() && identities[index].valid()
        ? identities[index] : packageAssetGuid(packageFingerprint,index);
  // Duas identidades iguais no mesmo pacote fariam `assetSlot` escolher pela
  // ordem da lista — exatamente o que a identidade existe para eliminar.
  for(u32 a=0;a<assets.size();++a) for(u32 b=0;b<a;++b) if(assets[a]==assets[b]) return false;
  if(!pivots.empty() && pivots.size()!=draws.size()*3) return false;
  if(!names.empty() && names.size()!=draws.size()) return false;
  pickMeshes_=std::move(meshes);
  source_.assign(draws.begin(),draws.end());
  materials_.assign(materials.begin(),materials.end());
  assets_=std::move(assets);
  assetNames_.assign(names.begin(),names.end());
  pivots_.assign(pivots.begin(),pivots.end());
  collisionHullCache_.clear();
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
  pivots_.clear();
  assetNames_.clear();
  collisionHullCache_.clear();
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
    bool changed=false;
    for(u32 slot=0;slot<render->slotCount();++slot) {
      auto *mesh=render->editSlotMesh(slot);auto *asset=render->editSlotAsset(slot);
      const auto previousSlot=*mesh;const auto previousAsset=*asset;
      if(asset->valid()) {
        // A identidade manda. Se ela não está neste pacote, o slot vira zero:
        // "referência ausente" é informação, e desenhar a malha que por acaso
        // ocupa o índice antigo seria corromper a cena em silêncio.
        *mesh=assetSlot(*asset);
      } else if(*mesh && *mesh<=assets_.size()) {
        // Cena anterior ao registro: ela já trazia um slot válido para ESTE
        // pacote, então a identidade derivada dele é a identidade correta.
        *asset=assets_[*mesh-1];
      }
      changed|=*mesh!=previousSlot || !(*asset==previousAsset);
    }
    if(changed) document.applyEntityValues(id,value);
  }
}
renderer::MaterialOverride EditorMapScene::materialForAsset(u32 index) const {
  if(index>=source_.size() || source_[index].materialIndex>=materials_.size()) return {};
  return renderer::materialOverrideFrom(materials_[source_[index].materialIndex]);
}
void EditorMapScene::hydrateMaterials(EditorDocument &document) const {
  std::vector<EditorEntityId> ids;document.collectSubtree(document.root(),ids);
  for(auto id:ids) {
    if(!meshRenderer(*document.find(id))) continue;
    auto value=*document.find(id);auto *render=editMeshRenderer(value);bool changed=false;
    for(u32 slot=0;slot<render->slotCount();++slot) {
      const auto mesh=render->slotMesh(slot);auto *material=render->editSlotMaterial(slot);
      if(!mesh || material->enabled) continue;
      const auto fresh=materialForAsset(mesh-1);
      if(!(fresh==*material)) {*material=fresh;changed=true;}
    }
    if(changed) document.applyEntityValues(id,value);
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
  const auto *render=meshRenderer(*entity);
  if(render->slotCount()==1) return slotBounds(document,id,meshAsset(*entity)-1,center,radius);
  // Vários slots: a esfera que envolve as esferas de cada slot.
  float low[3]{},high[3]{};bool first=true;
  for(u32 slot=0;slot<render->slotCount();++slot) {
    const auto mesh=render->slotMesh(slot);if(!mesh || mesh>source_.size()) continue;
    float c[3],r=0;if(!slotBounds(document,id,mesh-1,c,r)) return false;
    for(u32 axis=0;axis<3;++axis) {
      low[axis]=first?c[axis]-r:std::min(low[axis],c[axis]-r);
      high[axis]=first?c[axis]+r:std::max(high[axis],c[axis]+r);
    }
    first=false;
  }
  radius=0;
  for(u32 axis=0;axis<3;++axis) {center[axis]=(low[axis]+high[axis])*.5f;radius+=(high[axis]-center[axis])*(high[axis]-center[axis]);}
  radius=std::sqrt(radius);return !first && std::isfinite(radius);
}
bool EditorMapScene::slotBounds(const runtime::SceneGraph &document, EditorEntityId id, u32 index, float center[3], float &radius) const {
  if(index>=source_.size()) return false;
  float world[16];if(!editorWorldMatrix(document,id,world)) return false;
  float pivot[3];pivotOf(index,pivot);
  const auto &record=source_[index];
  // O centro é o centro do MESH levado ao mundo, e o pivô é a origem do objeto.
  // Para o pacote os dois coincidem e isto devolve exatamente o de sempre; para
  // um nó importado, usar a origem como centro deixaria o enquadramento e o
  // culling deslocados do que se vê.
  for(u32 axis=0;axis<3;++axis) {
    const float local[3]{record.boundsCenter[0]-pivot[0],record.boundsCenter[1]-pivot[1],record.boundsCenter[2]-pivot[2]};
    center[axis]=world[12+axis]+world[axis]*local[0]+world[4+axis]*local[1]+world[8+axis]*local[2];
  }
  float one=0,inf=0;
  for(u32 i=0;i<3;++i) {
    float col=0,row=0;
    for(u32 j=0;j<3;++j) {col+=std::abs(world[i*4+j]);row+=std::abs(world[j*4+i]);}
    one=std::max(one,col);inf=std::max(inf,row);
  }
  radius=source_[index].boundsRadius*std::sqrt(one*inf);
  return std::isfinite(radius);
}
bool EditorMapScene::localGeometry(u32 assetId,std::span<const EditorPickMesh::Triangle> &triangles,float relative[16]) const {
  if(!assetId||assetId>pickMeshes_.size()||!pickMeshes_[assetId-1]) return false;
  const auto &source=source_[assetId-1];std::copy(source.model,source.model+16,relative);
  float pivot[3];pivotOf(assetId-1,pivot);
  for(u32 axis=0;axis<3;++axis) relative[12+axis]-=pivot[axis];
  triangles=pickMeshes_[assetId-1]->triangles();return !triangles.empty();
}

bool EditorMapScene::collisionHullPreview(std::span<const u32> slots,float tolerance,
                                          CollisionHullPreview &out) const {
  out={};
  if(slots.empty()||!std::isfinite(tolerance)) return false;
  const auto cached=std::find_if(collisionHullCache_.begin(),collisionHullCache_.end(),[&](const auto &entry) {
    return entry.tolerance==tolerance && entry.slots.size()==slots.size() &&
           std::equal(entry.slots.begin(),entry.slots.end(),slots.begin());
  });
  const CollisionHullCacheEntry *entry=nullptr;
  if(cached!=collisionHullCache_.end()) entry=&*cached;
  else {
    CollisionHullCacheEntry created;created.slots.assign(slots.begin(),slots.end());created.tolerance=tolerance;
    std::vector<AetherVec3> points;
    for(const auto slot:slots) {
      std::span<const EditorPickMesh::Triangle> triangles;float relative[16];
      if(!localGeometry(slot,triangles,relative)) {created.diagnostic="A malha vinculada não tem geometria disponível";break;}
      points.reserve(points.size()+triangles.size()*3);
      for(const auto &triangle:triangles) for(u32 corner=0;corner<3;++corner) {
        const auto *p=triangle.data()+corner*3;
        points.push_back({relative[0]*p[0]+relative[4]*p[1]+relative[8]*p[2]+relative[12],
                          relative[1]*p[0]+relative[5]*p[1]+relative[9]*p[2]+relative[13],
                          relative[2]*p[0]+relative[6]*p[1]+relative[10]*p[2]+relative[14]});
      }
    }
    created.inputPointCount=static_cast<u32>(points.size());
    if(created.diagnostic.empty()) {
      auto settings=AetherMeshCookingDefaultsV1;settings.hullTolerance=tolerance;
      physics::CookedConvexHull hull;
      if(physics::cookConvexHull(points,settings,hull,created.diagnostic)) {
        created.vertexCount=static_cast<u32>(hull.vertices.size());created.faceCount=hull.faceCount;
        created.triangles.resize(hull.indices.size()/3);
        for(usize i=0;i<created.triangles.size();++i) for(u32 corner=0;corner<3;++corner) {
          const auto &p=hull.vertices[hull.indices[i*3+corner]];
          created.triangles[i][corner*3]=p.x;created.triangles[i][corner*3+1]=p.y;created.triangles[i][corner*3+2]=p.z;
        }
      }
    }
    // A UI pode experimentar muitos valores digitados; limite o cache sem
    // esconder estado autoral nem crescer indefinidamente.
    if(collisionHullCache_.size()>=32) collisionHullCache_.erase(collisionHullCache_.begin());
    collisionHullCache_.push_back(std::move(created));entry=&collisionHullCache_.back();
  }
  out.triangles=entry->triangles;out.inputPointCount=entry->inputPointCount;
  out.vertexCount=entry->vertexCount;out.faceCount=entry->faceCount;out.diagnostic=entry->diagnostic;
  return entry->diagnostic.empty()&&!entry->triangles.empty();
}
bool EditorMapScene::pickGeometry(const runtime::SceneGraph &document,EditorEntityId id,EditorPickCandidate &out) const {
  return pickSlotGeometry(document,id,0,out);
}
bool EditorMapScene::pickSlotGeometry(const runtime::SceneGraph &document,EditorEntityId id,u32 slot,EditorPickCandidate &out) const {
  const auto *entity=document.find(id);
  const auto *render=entity?meshRenderer(*entity):nullptr;
  if(!render || !render->slotMesh(slot) || render->slotMesh(slot)>pickMeshes_.size()) return false;
  const auto index=render->slotMesh(slot)-1;
  if(!pickMeshes_[index]) return false;
  float world[16],relative[16],pivot[3];
  if(!editorWorldMatrix(document,id,world)) return false;
  const auto &source=source_[index];std::copy(source.model,source.model+16,relative);
  // O MESMO pivo que `extract` usa para desenhar. Com `boundsCenter` aqui, a
  // malha de selecao ficava deslocada da malha desenhada em tudo que viesse de
  // um GLB com hierarquia, onde o pivo do no nao coincide com o centro dos
  // limites: tocar o objeto nao selecionava nada e tocar ao lado selecionava.
  pivotOf(index,pivot);
  for(u32 axis=0;axis<3;++axis) relative[12+axis]-=pivot[axis];
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
    const auto *render=meshRenderer(*entity);
    // Um desenho por slot, todos com o MESMO objeto: selecionar, esconder ou
    // mover o objeto age sobre todas as primitivas dele.
    for(u32 slot=0;slot<render->slotCount();++slot) {
      const u32 meshSlot=render->slotMesh(slot);
      if(!meshSlot) continue;
      const u32 index=meshSlot-1;
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
      float world[16],relative[16],pivot[3];
      if(!editorWorldMatrix(document,id,world)) return false;
      pivotOf(index,pivot);
      std::copy(source.model,source.model+16,relative);
      for(u32 a=0;a<3;++a) relative[12+a]-=pivot[a];
      multiply(world,relative,update.pose.draw.model);
      const float tint[4]{1,1,1,1};
      if(!renderer::buildGpuMeshInstance(update.pose.draw.model,tint,&update.pose.instance)) return false;
      const bool route=slot==0 && waterRoute(*entity).count;
      if(!(route?bounds(document,id,update.pose.draw.boundsCenter,update.pose.draw.boundsRadius)
                :slotBounds(document,id,index,update.pose.draw.boundsCenter,update.pose.draw.boundsRadius))) return false;
      update.visible=inheritedVisible(document,id) && render->enabled;
      update.castShadow=entity->castShadow;
      update.material=slotMaterial(*render,slot);
      if(isolateChannel_ && id==isolateEntity_ && slot==isolateSlot_) update.material.isolate=isolateChannel_;
      const auto &body=waterBody(*entity);
      update.waterLayers[0]=body.waveGain;update.waterLayers[1]=body.foamGain;
      update.waterLayers[2]=body.rippleGain;update.waterLayers[3]=body.opticalGain;
      update.waterFlowDepth[0]=waterBody(*entity).currentX;update.waterFlowDepth[1]=waterBody(*entity).currentZ;
      update.waterFlowDepth[2]=waterBody(*entity).depth;
      if(route) update.route=std::make_shared<const renderer::WaterRoute>(waterRoute(*entity));
    }
  }
  out=std::move(prepared);
  return true;
}
} // namespace ae::editor
