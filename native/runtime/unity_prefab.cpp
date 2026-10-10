#include "runtime/unity_prefab.h"
#include "core/rotation_math.h"
#include "scene/mesh_renderer.h"
#include <algorithm>
#include <limits>
#include <map>
#include <set>

namespace ae::runtime {
bool buildUnityPrefab(const resources::UnityPrefabImport &source,resources::AssetGuid guid,const UnityPrefabResolver &resolve,
    const UnityPrefabIdentityMap *previous,Prefab &out,UnityPrefabIdentityMap &identities,std::string &diagnostic) {
  const auto fail=[&](const std::string &message){diagnostic=message;return false;};
  if(!guid.valid()||source.objects.empty()||source.objects.size()>SceneGraph::kMaximumObjects-1||!resolve)return fail("Prefab convertido ou resolvedor inválido.");
  UnityPrefabIdentityMap candidate=previous?*previous:UnityPrefabIdentityMap{};
  std::map<i64,ObjectId> mapping;std::set<ObjectId> used;
  for(const auto &entry:candidate.entries)if(!entry.fileId||entry.object<2||entry.object>=candidate.nextObject||
      !mapping.emplace(entry.fileId,entry.object).second||!used.insert(entry.object).second)return fail("Mapa persistente de fileIDs Unity inválido.");
  for(const auto &object:source.objects)if(!mapping.contains(object.fileId)) {
    if(candidate.nextObject>SceneGraph::kMaximumObjects)return fail("Identidades de prefab esgotadas; IDs removidos não são reciclados.");
    const auto id=candidate.nextObject++;candidate.entries.push_back({object.fileId,id});mapping.emplace(object.fileId,id);
  }
  SceneGraph graph;std::map<i64,const resources::UnityPrefabObject*> transforms;std::set<i64> pending;
  for(const auto &object:source.objects){transforms.emplace(object.transformId,&object);pending.insert(object.fileId);}
  while(!pending.empty()) {
    bool progressed=false;
    for(const auto &object:source.objects) {
      if(!pending.contains(object.fileId))continue;
      ObjectId parent=graph.root();
      if(object.parentTransform) {
        const auto found=transforms.find(object.parentTransform);if(found==transforms.end())return fail("Pai Unity ausente.");
        parent=mapping.at(found->second->fileId);if(!graph.exists(parent))continue;
      }
      if(object.tag!="Untagged"||object.layer)return fail("Tags/camadas Unity exigem mapa explícito do projeto: "+object.name);
      SceneObject value;value.id=mapping.at(object.fileId);value.parent=parent;value.kind=object.renderer?ObjectKind::Mesh:ObjectKind::Folder;
      assignObjectName(value,object.name);value.active=object.active;value.castShadow=object.castShadow;value.receiveShadow=object.receiveShadow;
      std::copy(object.position.begin(),object.position.end(),value.transform.position);std::copy(object.scale.begin(),object.scale.end(),value.transform.scale);
      if(!rotationEulerXYZ(object.rotation.data(),value.transform.rotationDegrees))return fail("Rotação do prefab não representável.");
      if(object.renderer) {
        std::vector<resources::AssetGuid> meshes;
        if(!resolve(object.mesh,resources::AssetType::Mesh,meshes,diagnostic))return false;
        if(meshes.empty()||meshes.size()!=object.materials.size())return fail("Quantidade de submeshes/materiais Unity difere da malha convertida: "+object.name);
        auto *renderer=static_cast<scene::MeshRenderer*>(value.components.add(scene::MeshRenderer::descriptor));
        if(!renderer)return fail("Componente MeshRenderer nativo indisponível.");
        renderer->enabled=object.rendererEnabled;renderer->asset=meshes.front();
        for(usize slot=1;slot<meshes.size();++slot){scene::MeshSubmesh submesh;submesh.asset=meshes[slot];renderer->submeshes.push_back(submesh);}
        for(usize slot=0;slot<object.materials.size();++slot) {
          std::vector<resources::AssetGuid> materials;
          if(!resolve(object.materials[slot],resources::AssetType::Material,materials,diagnostic))return false;
          if(materials.size()!=1||!materials.front().valid())return fail("Material convertido ausente ou ambíguo.");
          *renderer->editSlotMaterialAsset(static_cast<u32>(slot))=materials.front();
        }
      }
      if(!graph.restoreEntity(value,std::numeric_limits<u32>::max()))return fail("Prefab recusado pelo grafo nativo: "+object.name);
      pending.erase(object.fileId);progressed=true;
    }
    if(!progressed)return fail("Ciclo ou pai ausente no prefab convertido.");
  }
  if(!mapping.contains(source.rootFileId)||!graph.reserveObjectIdsUntil(candidate.nextObject))return fail("Raiz ou fronteira de IDs inválida.");
  Prefab prefab;if(!prefab.capture(graph,mapping.at(source.rootFileId),guid,diagnostic))return false;
  out=std::move(prefab);identities=std::move(candidate);diagnostic.clear();return true;
}
} // namespace ae::runtime
