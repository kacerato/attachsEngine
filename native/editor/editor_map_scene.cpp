#include "editor/editor_water_body_component.h"
#include "editor/editor_route_component.h"
#include "editor/editor_map_scene.h"
#include "renderer/water_authoring_geometry.h"
#include "renderer/primitive_geometry.h"
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
resources::AssetGuid libraryAssetGuid(u64 fingerprint,u32 index,std::span<const renderer::MapDrawRecord> draws,
    std::span<const renderer::MapMaterialRecord> materials) {
  const auto material=draws[index].materialIndex;
  const auto type=renderer::primitiveFromFlags(material<materials.size()?materials[material].flags:0);
  // Keep the legacy cube identity; the five new resources are independent of package order.
  if(scene::validPrimitive(type) && type!=scene::PrimitiveType::Cube)
    return resources::assetGuidFromSeed(std::string("astra:builtin:")+std::string(scene::primitiveIds[static_cast<u32>(type)])+":v1");
  return packageAssetGuid(fingerprint,index);
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
// Modelo do desenho: mundo do objeto vezes o modelo relativo ao pivô, o mesmo
// produto que `extract` desenha.
void drawModelOf(const float world[16],const renderer::MapDrawRecord &source,const float pivot[3],float out[16]) {
  float relative[16];std::copy(source.model,source.model+16,relative);
  for(u32 a=0;a<3;++a) relative[12+a]-=pivot[a];
  multiply(world,relative,out);
}
float matrixNormBound(const float m[16]) {
  float one=0,inf=0;
  for(u32 i=0;i<3;++i) {
    float col=0,row=0;
    for(u32 k=0;k<3;++k) {col+=std::abs(m[i*4+k]);row+=std::abs(m[k*4+i]);}
    one=std::max(one,col);inf=std::max(inf,row);
  }
  return std::sqrt(one*inf);
}
} // namespace

// Valida o pacote para seleção e guarda só o necessário para construir cada
// BVH depois: posições (3 floats por vértice) e índices. Recusa aqui o que a
// construção recusaria mais tarde — intervalo, orçamento, posição não finita —
// para um pacote ruim continuar falhando na adoção, não no primeiro toque.
bool EditorMapScene::preparePickGeometry(std::span<const renderer::MapDrawRecord> draws,std::span<const u8> vertices,
                                         std::span<const u32> indices,std::shared_ptr<const PickGeometry> &geometry,
                                         std::vector<PickSlot> &slots) {
  geometry.reset();slots.assign(draws.size(),{});
  if(vertices.empty()!=indices.empty() || vertices.size()%renderer::MapVertexStride) return false;
  if(vertices.empty()) return true;
  const usize vertexCount=vertices.size()/renderer::MapVertexStride;
  auto prepared=std::make_shared<PickGeometry>();
  prepared->positions.resize(vertexCount*3);
  for(usize v=0;v<vertexCount;++v) {
    std::memcpy(prepared->positions.data()+v*3,vertices.data()+v*renderer::MapVertexStride,3*sizeof(float));
    for(u32 k=0;k<3;++k) if(!std::isfinite(prepared->positions[v*3+k])) return false;
  }
  prepared->indices.assign(indices.begin(),indices.end());
  usize triangleCount=0;
  std::map<std::tuple<u32,u32,i32>,u32> shared;
  for(u32 index=0;index<draws.size();++index) {
    const auto &draw=draws[index];
    auto &slot=slots[index];
    slot.firstIndex=draw.firstIndex;slot.indexCount=draw.indexCount;slot.vertexOffset=draw.vertexOffset;slot.canonical=index;
    const auto key=std::make_tuple(draw.firstIndex,draw.indexCount,static_cast<i32>(draw.vertexOffset));
    if(const auto found=shared.find(key);found!=shared.end()) {slot.canonical=found->second;continue;}
    triangleCount+=draw.indexCount/3;
    if(!draw.indexCount || draw.indexCount%3 || triangleCount>EditorPickMesh::MaximumTriangles ||
       u64(draw.firstIndex)+draw.indexCount>indices.size()) return false;
    for(u32 i=0;i<draw.indexCount;++i) {
      const auto vertex=static_cast<long long>(draw.vertexOffset)+indices[draw.firstIndex+i];
      if(vertex<0 || static_cast<u64>(vertex)>=vertexCount) return false;
    }
    // Cache the bounded audit once per shared draw at resource publication.
    // Large or pathological layouts are explicitly unverified, never silently
    // called usable. UV chart padding still belongs to the external baker.
    if(draw.indexCount/3>65536) slot.lightmapUvStatus=scene::LightmapUvStatus::AnalysisLimit;
    else {
      std::vector<scene::LightmapUvTriangle> uv(draw.indexCount/3);
      for(u32 i=0;i<draw.indexCount;++i) {
        const auto vertex=static_cast<usize>(draw.vertexOffset)+indices[draw.firstIndex+i];
        std::memcpy(uv[i/3].data()+(i%3)*2,vertices.data()+vertex*renderer::MapVertexStride+36,2*sizeof(float));
      }
      slot.lightmapUvStatus=scene::auditLightmapUv(uv);
    }
    shared.emplace(key,index);
  }
  geometry=std::move(prepared);
  return true;
}


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
  std::shared_ptr<const PickGeometry> geometry;std::vector<PickSlot> slots;
  if(!preparePickGeometry(draws,vertices,indices,geometry,slots)) return false;
  std::vector<resources::AssetGuid> assets(draws.size());
  for(u32 index=0;index<draws.size();++index)
    assets[index]=index<identities.size() && identities[index].valid()
        ? identities[index] : libraryAssetGuid(packageFingerprint,index,draws,materials);
  // Duas identidades iguais no mesmo pacote fariam `assetSlot` escolher pela
  // ordem da lista — exatamente o que a identidade existe para eliminar.
  for(u32 a=0;a<assets.size();++a) for(u32 b=0;b<a;++b) if(assets[a]==assets[b]) return false;
  if(!pivots.empty() && pivots.size()!=draws.size()*3) return false;
  if(!names.empty() && names.size()!=draws.size()) return false;
  pickGeometry_=std::move(geometry);pickSlots_=std::move(slots);pickMeshes_.assign(draws.size(),nullptr);
  source_.assign(draws.begin(),draws.end());
  materials_.assign(materials.begin(),materials.end());
  assets_=std::move(assets);
  assetNames_.assign(names.begin(),names.end());
  assetNames_.resize(draws.size());
  for(u32 i=0;i<draws.size();++i) {
    const auto type=renderer::primitiveFromFlags(materialFlagsForAsset(i));
    if(assetNames_[i].empty() && scene::validPrimitive(type)) assetNames_[i]=scene::primitiveNames[static_cast<u32>(type)];
  }
  pivots_.assign(pivots.begin(),pivots.end());
  collisionHullCache_.clear();
  reconcileAssets(document);
  return true;
}

bool EditorMapScene::import(EditorDocument &document, std::span<const renderer::MapDrawRecord> draws, std::span<const renderer::MapMaterialRecord> materials, bool instantiate, std::span<const u8> vertices, std::span<const u32> indices, u64 packageFingerprint) {
  if(draws.size()+1>EditorDocument::kMaximumEntities) return false;
  std::shared_ptr<const PickGeometry> geometry;std::vector<PickSlot> slots;
  if(!preparePickGeometry(draws,vertices,indices,geometry,slots)) return false;
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
    render->mesh=index+1;render->asset=libraryAssetGuid(packageFingerprint,index,draws,materials);entity.visible=draw.lodLevel==0;
    if(!setWaterBodyFlags(entity,true,(flags & renderer::MapMaterialWaterCameraGrid)!=0)) return false;
    if(draw.materialIndex<materials.size()) render->material=renderer::materialOverrideFrom(materials[draw.materialIndex]);
    std::copy(draw.boundsCenter,draw.boundsCenter+3,entity.transform.position);
    if(!prepared.applyEntityValues(id,entity)) return false;
  }
  pickGeometry_=std::move(geometry);pickSlots_=std::move(slots);pickMeshes_.assign(draws.size(),nullptr);
  source_.assign(draws.begin(),draws.end());
  materials_.assign(materials.begin(),materials.end());
  assets_.resize(draws.size());
  for(u32 index=0;index<draws.size();++index) assets_[index]=libraryAssetGuid(packageFingerprint,index,draws,materials);
  pivots_.clear();
  assetNames_.assign(draws.size(),{});
  for(u32 i=0;i<draws.size();++i) {
    const auto type=renderer::primitiveFromFlags(materialFlagsForAsset(i));
    if(scene::validPrimitive(type)) assetNames_[i]=scene::primitiveNames[static_cast<u32>(type)];
  }
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
  // Malha deformável: enquadrar e selecionar pela pose deformada, a que se vê.
  if(const auto *entity=document.find(id); entity && deformation(index))
    if(const auto *mesh=static_cast<const scene::SkinnedMesh *>(entity->components.find(scene::SkinnedMesh::descriptor))) {
      float model[16];drawModelOf(world,record,pivot,model);
      DeformedPose pose;
      if(deformedPose(document,*mesh,index,model,pose)) {
        std::copy(pose.center,pose.center+3,center);radius=pose.radius;return true;
      }
    }
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
std::shared_ptr<const EditorPickMesh> EditorMapScene::pickMesh(u32 index) const {
  if(index>=pickSlots_.size() || !pickGeometry_) return nullptr;
  const u32 canonical=pickSlots_[index].canonical;
  if(canonical>=pickMeshes_.size()) return nullptr;
  if(!pickMeshes_[canonical]) {
    const auto &slot=pickSlots_[canonical];const auto &geometry=*pickGeometry_;
    std::vector<EditorPickMesh::Triangle> triangles(slot.indexCount/3);
    for(u32 t=0;t<triangles.size();++t) for(u32 point=0;point<3;++point) {
      const auto vertex=static_cast<usize>(static_cast<long long>(slot.vertexOffset)+geometry.indices[slot.firstIndex+t*3+point]);
      std::memcpy(triangles[t].data()+point*3,geometry.positions.data()+vertex*3,3*sizeof(float));
    }
    auto mesh=std::make_shared<EditorPickMesh>();
    // Já validado na adoção; uma falha aqui seria dado corrompido depois dela.
    if(!mesh->build(std::move(triangles))) return nullptr;
    pickMeshes_[canonical]=std::move(mesh);
  }
  return pickMeshes_[canonical];
}

bool EditorMapScene::localGeometry(u32 assetId,std::span<const EditorPickMesh::Triangle> &triangles,float relative[16]) const {
  const auto mesh=assetId?pickMesh(assetId-1):nullptr;
  if(!mesh) return false;
  const auto &source=source_[assetId-1];std::copy(source.model,source.model+16,relative);
  float pivot[3];pivotOf(assetId-1,pivot);
  for(u32 axis=0;axis<3;++axis) relative[12+axis]-=pivot[axis];
  triangles=mesh->triangles();return !triangles.empty();
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
bool EditorMapScene::intersectColliderMesh(std::span<const u32> slots,bool convex,float tolerance,
    const EditorRay &ray,const float pose[16],float &distance) const {
  if(!ray.valid || slots.empty())return false;
  if(convex) {
    CollisionHullPreview preview;if(!collisionHullPreview(slots,tolerance,preview))return false;
    auto entry=std::find_if(collisionHullCache_.begin(),collisionHullCache_.end(),[&](const auto &item) {
      return item.tolerance==tolerance && item.slots.size()==slots.size() &&
          std::equal(item.slots.begin(),item.slots.end(),slots.begin());
    });
    if(entry==collisionHullCache_.end())return false;
    if(!entry->pickMesh) {
      auto mesh=std::make_shared<EditorPickMesh>();if(!mesh->build(entry->triangles))return false;
      entry->pickMesh=std::move(mesh);
    }
    return entry->pickMesh->intersect(ray.origin,ray.direction,pose,distance,ray.minimumDistance,ray.maximumDistance);
  }
  bool hit=false;float nearest=ray.maximumDistance;
  for(const auto slot:slots) {
    std::span<const EditorPickMesh::Triangle> triangles;float relative[16],model[16],depth;
    if(!localGeometry(slot,triangles,relative))return false;
    runtime::multiplyMatrix(pose,relative,model);
    const auto mesh=pickMesh(slot-1);
    if(mesh && mesh->intersect(ray.origin,ray.direction,model,depth,ray.minimumDistance,nearest)) {nearest=depth;hit=true;}
  }
  if(hit)distance=nearest;
  return hit;
}
bool EditorMapScene::pickGeometry(const runtime::SceneGraph &document,EditorEntityId id,EditorPickCandidate &out) const {
  return pickSlotGeometry(document,id,0,out);
}
bool EditorMapScene::pickSlotGeometry(const runtime::SceneGraph &document,EditorEntityId id,u32 slot,EditorPickCandidate &out) const {
  const auto *entity=document.find(id);
  const auto *render=entity?meshRenderer(*entity):nullptr;
  if(!render || !render->slotMesh(slot) || render->slotMesh(slot)>pickSlots_.size()) return false;
  const auto index=render->slotMesh(slot)-1;
  if(index>=pickSlots_.size() || !pickGeometry_) return false;
  float world[16],relative[16],pivot[3];
  if(!editorWorldMatrix(document,id,world)) return false;
  const auto &source=source_[index];std::copy(source.model,source.model+16,relative);
  // O MESMO pivo que `extract` usa para desenhar. Com `boundsCenter` aqui, a
  // malha de selecao ficava deslocada da malha desenhada em tudo que viesse de
  // um GLB com hierarquia, onde o pivo do no nao coincide com o centro dos
  // limites: tocar o objeto nao selecionava nada e tocar ao lado selecionava.
  pivotOf(index,pivot);
  for(u32 axis=0;axis<3;++axis) relative[12+axis]-=pivot[axis];
  multiply(world,relative,out.model);out.mesh={};
  // A BVH do desenho só nasce se o raio chegar até ele (`pickNearest`).
  out.resolve=[this,index]{return pickMesh(index);};
  // Malha deformável: o toque acerta a pose que está na tela, não a de bind.
  std::shared_ptr<const EditorPickMesh> deformed;
  if(deformedPickMesh(document,id,index,out.model,deformed)) {out.mesh=std::move(deformed);out.resolve={};}
  return true;
}
void EditorMapScene::setDeformation(std::vector<std::shared_ptr<const DrawDeformation>> draws,
                                    std::vector<std::shared_ptr<const runtime::SourceAnimations>> animations) {
  deformations_=std::move(draws);animationSources_=std::move(animations);
  clipIndex_.clear();
  for(u32 s=0;s<animationSources_.size();++s)
    for(u32 c=0;c<animationSources_[s]->clipIds.size();++c) clipIndex_[animationSources_[s]->clipIds[c]]={s,c};
}

bool EditorMapScene::findClip(const resources::AssetGuid &clip,runtime::AnimationClipView &out) const {
  const auto found=clipIndex_.find(clip);
  if(found==clipIndex_.end()) return false;
  const auto &source=*animationSources_[found->second.source];
  out.source=&source;out.clip=&source.clips[found->second.clip];
  out.name=resources::animationClipDisplayName(*out.clip,found->second.clip);
  return true;
}

std::vector<EditorMapScene::ClipEntry> EditorMapScene::clipCatalog() const {
  std::vector<ClipEntry> out;
  for(const auto &source:animationSources_)
    for(u32 c=0;c<source->clips.size();++c)
      out.push_back({source->clipIds[c],source->source,resources::animationClipDisplayName(source->clips[c],c),source->clips[c].duration});
  return out;
}

bool EditorMapScene::deformedPose(const runtime::SceneGraph &document,const scene::SkinnedMesh &mesh,u32 assetIndex,
                                  const float drawModel[16],DeformedPose &out) const {
  out={};
  const auto *deform=deformation(assetIndex);
  if(!deform || assetIndex>=source_.size()) return false;
  float localCenter[3]{source_[assetIndex].boundsCenter[0],source_[assetIndex].boundsCenter[1],source_[assetIndex].boundsCenter[2]};
  float localRadius=source_[assetIndex].boundsRadius;
  if(const auto *skin=deform->skin.get()) {
    const usize joints=skin->joints.size();
    std::vector<float> worlds(joints*16);
    std::vector<u8> missing(joints,0);
    for(usize j=0;j<joints;++j) {
      const u64 bone=j<mesh.bones.size()?mesh.bones[j]:0;
      if(!bone || !editorWorldMatrix(document,static_cast<EditorEntityId>(bone),worlds.data()+j*16)) {
        // Placeholder finito; a entrada da paleta vira identidade logo abaixo.
        std::copy(drawModel,drawModel+16,worlds.data()+j*16);
        missing[j]=1;++out.missingBones;
      }
    }
    auto palette=std::make_shared<std::vector<float>>();
    if(!resources::computeSkinPalette(*skin,drawModel,worlds,*palette)) return false;
    // Osso ausente = pose de bind: o vértice fica onde o arquivo o pôs.
    for(usize j=0;j<joints;++j) if(missing[j]) {
      float *m=palette->data()+j*16;
      std::fill(m,m+16,0.0f);m[0]=m[5]=m[10]=m[15]=1;
    }
    if(!resources::skinnedLocalBounds(*skin,*palette,localCenter,localRadius)) return false;
    out.palette=std::move(palette);
  }
  if(const auto *morph=deform->morph.get()) {
    auto weights=std::make_shared<std::vector<float>>(morph->targetCount,0.0f);
    for(u32 t=0;t<morph->targetCount && t<mesh.blendShapeWeights.size();++t) (*weights)[t]=mesh.blendShapeWeights[t]/100.0f;
    localRadius+=resources::morphBoundsExpansion(*morph,*weights);
    out.weights=std::move(weights);
  }
  for(u32 axis=0;axis<3;++axis)
    out.center[axis]=drawModel[12+axis]+drawModel[axis]*localCenter[0]+drawModel[4+axis]*localCenter[1]+drawModel[8+axis]*localCenter[2];
  out.radius=localRadius*matrixNormBound(drawModel);
  return std::isfinite(out.radius) && std::isfinite(out.center[0]) && std::isfinite(out.center[1]) && std::isfinite(out.center[2]);
}

bool EditorMapScene::deformedPickMesh(const runtime::SceneGraph &document,EditorEntityId id,u32 assetIndex,
                                      const float drawModel[16],std::shared_ptr<const EditorPickMesh> &out) const {
  const auto *entity=document.find(id);
  const auto *mesh=entity?static_cast<const scene::SkinnedMesh *>(entity->components.find(scene::SkinnedMesh::descriptor)):nullptr;
  const auto *deform=deformation(assetIndex);
  if(!mesh || !deform || deform->restPositions.empty()) return false;
  DeformedPose pose;
  if(!deformedPose(document,*mesh,assetIndex,drawModel,pose)) return false;
  // Mesmos passos do compute: blend shapes e depois skin, na forma base local.
  std::vector<float> positions=deform->restPositions;
  static const std::vector<float> none;
  if(!resources::deformPositions(positions,deform->influences,pose.palette?*pose.palette:none,mesh->influences(),
                                 deform->morph.get(),pose.weights?*pose.weights:none)) return false;
  std::vector<EditorPickMesh::Triangle> triangles(deform->localIndices.size()/3);
  for(usize t=0;t<triangles.size();++t)
    for(u32 point=0;point<3;++point) {
      const u32 vertex=deform->localIndices[t*3+point];
      if(usize(vertex)*3+2>=positions.size()) return false;
      std::copy(positions.begin()+vertex*3,positions.begin()+vertex*3+3,triangles[t].begin()+point*3);
    }
  auto built=std::make_shared<EditorPickMesh>();
  if(!built->build(std::move(triangles))) return false;
  out=std::move(built);
  return true;
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
      if(const auto *skinned=static_cast<const scene::SkinnedMesh *>(entity->components.find(scene::SkinnedMesh::descriptor));
         skinned && deformation(index)) {
        DeformedPose pose;
        if(deformedPose(document,*skinned,index,update.pose.draw.model,pose)) {
          std::copy(pose.center,pose.center+3,update.pose.draw.boundsCenter);update.pose.draw.boundsRadius=pose.radius;
          update.skinPalette=std::move(pose.palette);
          update.morphWeights=std::move(pose.weights);
        }
        update.skinInfluences=static_cast<u8>(skinned->influences());
        update.skinnedMotion=skinned->skinnedMotionVectors;
      }
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
