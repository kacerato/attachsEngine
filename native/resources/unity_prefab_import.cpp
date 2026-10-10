#include "resources/unity_prefab_import.h"
#include "core/rotation_math.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>

namespace ae::resources {
namespace {
using Node=UnityYamlNode;
bool fail(std::string &error,const std::string &message){error=message;return false;}
bool ref(const Node &node,std::string_view key,UnityObjectReference &out) {const auto *value=node.get(key);return value&&unityObjectReference(*value,out);}
bool flag(const Node &node,std::string_view key,bool &out,bool optional=false) {const auto *value=node.get(key);return value?value->boolean(out):optional;}
template<usize N> bool vector(const Node &node,std::string_view key,std::array<float,N> &out) {
  const auto *value=node.get(key);if(!value)return false;
  constexpr const char *names[]{"x","y","z","w"};
  for(usize c=0;c<N;++c){double n=0;const auto *part=value->get(names[c]);if(!part||!part->number(n)||std::abs(n)>1e7)return false;out[c]=static_cast<float>(n);}
  return true;
}
}
bool convertUnityPrefab(const UnityYamlDocument &document,UnityPrefabImport &out,std::string &diagnostic) {
  UnityPrefabImport candidate;std::map<i64,usize> gameObjects;std::map<i64,i64> transforms;
  std::set<i64> ownedComponents;std::map<i64,std::set<i64>> declaredComponents;
  for(const auto &object:document.objects) {
    if(object.stripped)return fail(diagnostic,"Prefab Unity contém objeto stripped/instância; patches precisam de conversão específica.");
    if(object.classId==1001) {
      const auto *mod=object.value.get("m_Modification");UnityObjectReference parent;
      if(object.type!="Prefab"||!ref(object.value,"m_ParentPrefab",parent)||parent.fileId||parent.external()||!mod)
        return fail(diagnostic,"PrefabVariant/PrefabInstance Unity exige aplicação de patches por fileID; fonte preservada.");
      for(const char *field:{"m_Modifications","m_RemovedComponents"})if(const auto *value=mod->get(field);value&&!value->empty())
        return fail(diagnostic,"Prefab Unity contém modificações não materializadas.");
      UnityObjectReference root;if(!ref(object.value,"m_RootGameObject",root)||root.external()||!root.fileId||candidate.rootFileId)
        return fail(diagnostic,"Raiz de prefab Unity inválida ou duplicada.");
      candidate.rootFileId=root.fileId;continue;
    }
    if(object.classId!=1&&object.classId!=4&&object.classId!=23&&object.classId!=33)
      return fail(diagnostic,"Componente Unity sem consumidor neste conversor de prefab: classID "+std::to_string(object.classId)+" · "+object.type+" · fileID "+std::to_string(object.fileId));
    if(object.classId!=1)continue;
    if(object.type!="GameObject")return fail(diagnostic,"ClassID Unity não corresponde ao tipo GameObject.");
    UnityPrefabObject value;value.fileId=object.fileId;
    const auto *name=object.value.get("m_Name"),*tag=object.value.get("m_TagString"),*layer=object.value.get("m_Layer"),*components=object.value.get("m_Component");i64 number=0;
    if(!name||name->kind!=Node::Kind::Scalar||name->scalar.empty()||name->scalar.size()>=64||
        !tag||tag->kind!=Node::Kind::Scalar||!layer||!layer->integer(number)||number<0||number>31||
        !components||components->kind!=Node::Kind::Sequence||!flag(object.value,"m_IsActive",value.active))
      return fail(diagnostic,"Nome/tag/layer/componentes de GameObject Unity inválidos ou fora do contrato nativo.");
    value.name=name->scalar;value.tag=tag->scalar;value.layer=static_cast<u32>(number);
    auto &declared=declaredComponents[value.fileId];
    for(const auto &component:components->elements) {
      UnityObjectReference reference;const auto *node=component.get("component");
      if(!node||!unityObjectReference(*node,reference)||reference.external()||!reference.fileId||!declared.insert(reference.fileId).second)
        return fail(diagnostic,"Referência de componente Unity inválida ou duplicada.");
    }
    gameObjects.emplace(value.fileId,candidate.objects.size());candidate.objects.push_back(std::move(value));
  }
  if(candidate.objects.empty())return fail(diagnostic,"Prefab Unity sem GameObjects completos.");
  for(const auto &object:document.objects) {
    if(object.classId==1||object.classId==1001)continue;
    UnityObjectReference owner;
    if(!ref(object.value,"m_GameObject",owner)||owner.external()||!gameObjects.contains(owner.fileId)||
       !declaredComponents[owner.fileId].contains(object.fileId)||!ownedComponents.insert(object.fileId).second)
      return fail(diagnostic,"Componente Unity aponta para GameObject ausente ou não o declara.");
    auto &value=candidate.objects[gameObjects.at(owner.fileId)];
    if(object.classId==4) {
      UnityObjectReference parent;
      if(object.type!="Transform"||value.transformId||!ref(object.value,"m_Father",parent)||parent.external()||
          !vector(object.value,"m_LocalPosition",value.position)||!vector(object.value,"m_LocalRotation",value.rotation)||
          !vector(object.value,"m_LocalScale",value.scale)||!normalizeRotationQuaternion(value.rotation.data()))
        return fail(diagnostic,"Transform Unity inválido, duplicado ou externo.");
      for(float scale:value.scale)if(std::abs(scale)<1e-5f||std::abs(scale)>1e5f)return fail(diagnostic,"Escala de prefab Unity singular ou fora do orçamento.");
      value.position[2]*=-1;value.rotation[0]*=-1;value.rotation[1]*=-1;
      value.transformId=object.fileId;value.parentTransform=parent.fileId;transforms.emplace(object.fileId,owner.fileId);
    } else if(object.classId==33) {
      if(object.type!="MeshFilter"||value.mesh.fileId||!ref(object.value,"m_Mesh",value.mesh)||!value.mesh.fileId||!value.mesh.external())
        return fail(diagnostic,"MeshFilter Unity exige referência externa GUID/fileID de malha completa.");
    } else {
      if(object.type!="MeshRenderer"||value.renderer||!flag(object.value,"m_Enabled",value.rendererEnabled)||!flag(object.value,"m_ReceiveShadows",value.receiveShadow))
        return fail(diagnostic,"MeshRenderer Unity inválido ou duplicado.");
      i64 shadows=0;const auto *cast=object.value.get("m_CastShadows");
      if(!cast||!cast->integer(shadows)||shadows<0||shadows>1)return fail(diagnostic,"ShadowCastingMode Unity especial exige conversão de render; não equivale ao bool nativo.");
      value.castShadow=shadows!=0;value.renderer=true;
      const auto *materials=object.value.get("m_Materials");
      if(!materials||materials->kind!=Node::Kind::Sequence||materials->elements.empty()||materials->elements.size()>256)
        return fail(diagnostic,"Materiais de MeshRenderer Unity ausentes ou acima do limite.");
      for(const auto &material:materials->elements) {
        UnityObjectReference reference;
        if(!unityObjectReference(material,reference)||!reference.external()||!reference.fileId)
          return fail(diagnostic,"Material Unity ausente ou embutido sem conversão.");
        value.materials.push_back(reference);
      }
      for(const char *field:{"m_LightProbeUsage","m_ReflectionProbeUsage"}) {
        i64 mode=0;const auto *node=object.value.get(field);
        if(node&&(!node->integer(mode)||mode<0||mode>1))return fail(diagnostic,"Uso especial de probes Unity exige configuração de render nativa explícita.");
      }
      candidate.transformations.push_back("Probes e GI Unity não transportados; iluminação e sombras são recalculadas pelo renderer Astra.");
    }
  }
  usize roots=0;
  for(const auto &value:candidate.objects) {
    if(!value.transformId||declaredComponents[value.fileId].size()!=1u+(value.renderer?1u:0u)+(value.mesh.fileId?1u:0u)||value.renderer!=bool(value.mesh.fileId))
      return fail(diagnostic,"Prefab Unity contém componente ausente ou MeshFilter/MeshRenderer incompleto.");
    if(!value.parentTransform) {++roots;if(candidate.rootFileId&&candidate.rootFileId!=value.fileId)return fail(diagnostic,"Raiz Unity não coincide com a hierarquia.");candidate.rootFileId=value.fileId;}
    else if(!transforms.contains(value.parentTransform))return fail(diagnostic,"Pai Transform Unity ausente.");
    std::set<i64> path;const UnityPrefabObject *cursor=&value;
    while(cursor->parentTransform) {
      if(!path.insert(cursor->transformId).second)return fail(diagnostic,"Ciclo na hierarquia Unity.");
      cursor=&candidate.objects[gameObjects.at(transforms.at(cursor->parentTransform))];
    }
    const auto *transform=document.find(value.transformId);const auto *children=transform->value.get("m_Children");std::set<i64> declared;
    if(!children||children->kind!=Node::Kind::Sequence)return fail(diagnostic,"Lista de filhos Transform Unity inválida.");
    for(const auto &child:children->elements) {UnityObjectReference reference;
      if(!unityObjectReference(child,reference)||reference.external()||!transforms.contains(reference.fileId)||!declared.insert(reference.fileId).second||
          candidate.objects[gameObjects.at(transforms.at(reference.fileId))].parentTransform!=value.transformId)return fail(diagnostic,"Lista de filhos e m_Father Unity divergem.");}
    usize count=0;for(const auto &child:candidate.objects)count+=child.parentTransform==value.transformId;
    if(count!=declared.size())return fail(diagnostic,"Transform Unity omitiu um filho declarado por m_Father.");
  }
  if(roots!=1)return fail(diagnostic,"Prefab Unity precisa de exatamente uma raiz.");
  candidate.transformations.push_back("Hierarquia e referências locais por fileID exato; coordenadas LH para RH por reflexão Z; escala preservada.");
  candidate.transformations.push_back("Flags Static, ícones e metadados exclusivos do editor Unity não transportados; o grafo nativo conserva somente o contrato de cena executável declarado.");
  std::sort(candidate.transformations.begin(),candidate.transformations.end());candidate.transformations.erase(std::unique(candidate.transformations.begin(),candidate.transformations.end()),candidate.transformations.end());
  out=std::move(candidate);diagnostic.clear();return true;
}
} // namespace ae::resources
