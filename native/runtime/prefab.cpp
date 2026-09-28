#include "runtime/prefab.h"
#include "scene/prefab_link.h"
#include "scene/script_behavior.h"
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <unordered_set>

namespace ae::runtime {
namespace {
bool portableReferences(const SceneGraph &graph,ObjectId root,std::string &error) {
  std::vector<ObjectId> ids;graph.collectSubtree(root,ids);
  const std::unordered_set<ObjectId> members(ids.begin(),ids.end());
  for(const auto id:ids) {
    const auto &object=*graph.find(id);
    for(usize i=0;i<object.components.size();++i) {
      const auto *component=object.components.at(i);
      if(component->unresolved()) {error="Componente indisponível em "+std::string(object.name);return false;}
      bool valid=true;
      const auto check=[&](u64 target) {if(target && (target>std::numeric_limits<ObjectId>::max() || !members.contains(static_cast<ObjectId>(target)))) valid=false;};
      for(const auto &property:component->type().references) if(property.read) check(property.read(*component));
      if(const auto *script=scene::scriptBehavior(component))
        for(const auto &property:script->properties) scene::forEachScriptPropertyObject(property,check);
      if(!valid) {error="Referência externa ao prefab em "+std::string(object.name)+" ("+std::string(component->type().id)+")";return false;}
    }
  }
  return true;
}
bool validObject(const SceneObject &object) {
  if(object.id<2 || object.id>SceneGraph::kMaximumObjects ||
     static_cast<u32>(object.kind)>static_cast<u32>(ObjectKind::Effect) || !object.name[0] ||
     !isTransformValid(object.transform) || object.layer>=32 || !ObjectTags::validName(object.tag) ||
     !object.components.valid()) return false;
  for(float value:object.rigidBody) if(!std::isfinite(value)) return false;
  for(float value:object.environment) if(!std::isfinite(value)) return false;
  return true;
}
}

std::string serializePrefabObject(const SceneObject &object) {
  if(!validObject(object)) return {};
  std::ostringstream out;out.imbue(std::locale::classic());
  out<<std::setprecision(std::numeric_limits<float>::max_digits10);
  out<<object.id<<' '<<object.parent<<' '<<static_cast<u32>(object.kind)<<' '<<std::quoted(object.name);
  for(float value:object.transform.position) out<<' '<<value;
  for(float value:object.transform.rotationDegrees) out<<' '<<value;
  for(float value:object.transform.scale) out<<' '<<value;
  out<<' '<<object.active<<' '<<object.visible<<' '<<object.castShadow<<' '<<object.receiveShadow
     <<' '<<object.isStatic<<' '<<object.layer<<' '<<std::quoted(object.tag)<<' '<<object.rigidBodyEnabled;
  for(float value:object.rigidBody) out<<' '<<value;
  for(float value:object.environment) out<<' '<<value;
  auto components=object.components;components.remove(scene::PrefabLink::descriptor);
  if(!components.write(out,true)) return {};
  return out.str();
}

bool deserializePrefabObject(std::istream &in,Prefab::Registry registry,SceneObject &object) {
  SceneObject value;std::string name;u32 kind=0;
  if(!(in>>value.id>>value.parent>>kind>>std::quoted(name)) || name.empty() || name.size()>=kNameCapacity ||
     name.find('\0')!=std::string::npos || kind>static_cast<u32>(ObjectKind::Effect)) return false;
  value.kind=static_cast<ObjectKind>(kind);assignObjectName(value,name);
  for(float &v:value.transform.position) if(!(in>>v)) return false;
  for(float &v:value.transform.rotationDegrees) if(!(in>>v)) return false;
  for(float &v:value.transform.scale) if(!(in>>v)) return false;
  if(!(in>>value.active>>value.visible>>value.castShadow>>value.receiveShadow>>value.isStatic>>value.layer
       >>std::quoted(value.tag)>>value.rigidBodyEnabled)) return false;
  for(float &v:value.rigidBody) if(!(in>>v)) return false;
  for(float &v:value.environment) if(!(in>>v)) return false;
  if(!value.components.read(in,registry,scene::UnknownComponentPolicy::Reject,true) ||
     value.components.find(scene::PrefabLink::descriptor) || !validObject(value)) return false;
  object=std::move(value);return true;
}

bool Prefab::capture(const SceneGraph &source,ObjectId root,resources::AssetGuid asset,std::string &error) {
  error.clear();
  if(!asset.valid() || root==source.root() || !source.exists(root)) {error="Selecione uma subárvore válida para o prefab";return false;}
  if(!portableReferences(source,root,error)) return false;
  Prefab prepared;prepared.asset_=asset;prepared.root_=root;
  std::vector<ObjectId> ids;source.collectSubtree(root,ids);
  for(const auto id:ids) {
    auto object=*source.find(id);
    // Nested ownership must not be flattened silently before composition exists.
    if(scene::prefabLink(object.components)) {error="Esta operação exige composição de prefabs; o vínculo existente foi preservado";return false;}
    if(id==root) object.parent=prepared.graph_.root();
    if(!validObject(object) || serializePrefabObject(object).size()>256*1024 ||
       !prepared.graph_.restoreEntity(object,std::numeric_limits<u32>::max())) {error="Objeto inválido ou grande demais: "+std::string(object.name);return false;}
  }
  if(prepared.write().empty()) {error="O prefab excede o limite de armazenamento";return false;}
  *this=std::move(prepared);return true;
}

std::string Prefab::write() const {
  if(!asset_.valid() || !root_ || !graph_.exists(root_)) return {};
  std::vector<ObjectId> ids;graph_.collectSubtree(root_,ids);
  std::ostringstream out;out.imbue(std::locale::classic());
  out<<"ASTRA_PREFAB 1 "<<asset_.text()<<' '<<root_<<' '<<ids.size()<<'\n';
  for(const auto id:ids) {
    const auto text=serializePrefabObject(*graph_.find(id));
    if(text.empty() || text.size()>256*1024) return {};
    out<<text<<'\n';
    if(out.tellp()>static_cast<std::streamoff>(MaximumBytes)) return {};
  }
  return out.str();
}

bool Prefab::read(std::string_view text,Registry registry,std::string &error) {
  error.clear();
  if(text.size()>MaximumBytes) {error="Prefab excede 32 MiB";return false;}
  std::istringstream in{std::string(text)};in.imbue(std::locale::classic());
  std::string magic,guid;u32 version=0,count=0;Prefab prepared;
  if(!(in>>magic>>version>>guid>>prepared.root_>>count) || magic!="ASTRA_PREFAB" || version!=1 ||
     !resources::AssetGuid::parse(guid,prepared.asset_) || !prepared.asset_.valid() || !count || count>=SceneGraph::kMaximumObjects) {
    error="Cabeçalho de prefab inválido ou versão não suportada";return false;
  }
  for(u32 i=0;i<count;++i) {
    SceneObject object;
    if(!deserializePrefabObject(in,registry,object) || (i==0 && (object.id!=prepared.root_ || object.parent!=prepared.graph_.root())) ||
       (i && (object.parent==prepared.graph_.root() || !prepared.graph_.exists(object.parent))) ||
       serializePrefabObject(object).size()>256*1024 || !prepared.graph_.restoreEntity(object,std::numeric_limits<u32>::max())) {
      error="Objeto, componente ou hierarquia inválida no prefab";return false;
    }
  }
  in>>std::ws;
  if(!in.eof()) {error="Dados adicionais após o prefab";return false;}
  if(!portableReferences(prepared.graph_,prepared.root_,error)) return false;
  *this=std::move(prepared);return true;
}

ObjectId Prefab::instantiate(SceneGraph &destination,ObjectId parent,ObjectCloneMap &mapping,std::string &error) const {
  error.clear();mapping.clear();
  if(!asset_.valid() || !root_ || !destination.exists(parent)) {error="Prefab ou destino inválido";return 0;}
  const auto root=destination.cloneSubtree(graph_,root_,parent,mapping);
  if(!root) {error="Não foi possível criar a hierarquia do prefab";return 0;}
  for(const auto &[source,target]:mapping) {
    auto object=*destination.find(target);
    auto *link=static_cast<scene::PrefabLink*>(object.components.add(scene::PrefabLink::descriptor));
    if(link) {link->asset=asset_;link->sourceObject=source;link->instanceRoot=root;link->base=serializePrefabObject(*graph_.find(source));}
    if(!link || !destination.applyEntityValues(target,object)) {
      destination.destroyEntity(root);mapping.clear();error="Não foi possível registrar o vínculo do prefab";return 0;
    }
  }
  return root;
}
} // namespace ae::runtime
