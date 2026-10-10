#include "editor/editor_session.h"
#include "runtime/animation_property_binding.h"
#include "scene/animator.h"
#include <algorithm>
#include <map>
#include <set>

namespace ae::editor {
namespace {
using namespace resources;
bool fail(std::string &error,const char *text){error=text;return false;}
bool bindingUsed(const AnimationClipAsset &asset,u64 binding) {
  for(const auto &t:asset.tracks)if(t.binding==binding)return true;
  for(const auto &t:asset.properties)if(t.binding==binding)return true;
  for(const auto &t:asset.drivers.inputs)if(t.binding==binding)return true;
  for(const auto &t:asset.drivers.targets)if(t.binding==binding)return true;
  return false;
}
}
std::vector<EditorScreenState::ClipDriverEntry> EditorSession::animationDriverParameters(EditorEntityId target,std::string &error) const {
  error.clear();std::vector<EditorScreenState::ClipDriverEntry> entries;const auto *object=document_.find(target);
  if(!object){error="Objeto de parâmetro removido";return entries;}
  for(usize i=0;i<object->components.size();++i) {
    const auto &component=*object->components.at(i);if(&component.type()!=&scene::Animator::descriptor)continue;
    const auto &animator=static_cast<const scene::Animator&>(component);scene::Animator resolved;
    if(!resolveAnimatorController(animator,animatorControllers_,resolved,error))return {};
    for(const auto &parameter:resolved.parameters)if(parameter.type==scene::AnimatorParameterType::Float) {
      EditorScreenState::ClipDriverEntry entry;entry.target=target;entry.label="Animator · "+parameter.name;
      entry.input.kind=AnimationDriverInputKind::AnimatorParameter;entry.input.parameter=parameter.id;entry.input.controller=animator.controller;
      auto &a=entry.input.address;a.component=component.instanceId();a.version=component.type().version;a.type=component.type().id;a.property="parameter";
      entries.push_back(std::move(entry));
    }
  }
  return entries;
}
bool EditorSession::animationDriverParameter(runtime::ObjectId object,u64 component,u64 parameter,
    resources::AssetGuid controller,double &value,std::string &error) const {
  const auto *entity=document_.find(object);const auto *source=entity?entity->components.findInstance(component):nullptr;
  if(!source||&source->type()!=&scene::Animator::descriptor)return fail(error,"Animator de driver ausente");
  const auto &animator=static_cast<const scene::Animator&>(*source);scene::Animator resolved;
  if(animator.controller!=controller||!resources::resolveAnimatorController(animator,animatorControllers_,resolved,error)) {
    if(error.empty())error="Controlador de driver substituído";
    return false;
  }
  const auto *p=resolved.parameter(parameter);
  if(!p||p->type!=scene::AnimatorParameterType::Float)return fail(error,"Parâmetro de driver removido ou sem tipo Float");
  value=p->value;return true;
}
bool EditorSession::validateAnimationPropertyClip(const AnimationClipAsset &asset,EditorEntityId owner,float time,std::string &error) const {
  AnimationClipPreview preview;
  preview.configureGuiLoader([this](AssetGuid asset,ui::GuiDocument &doc,std::string &diagnostic){return loadGuiDocument(asset,doc,diagnostic);});
  preview.configureDriverParameterResolver([this](auto object,auto component,auto parameter,auto controller,double &value,std::string &diagnostic){
    return animationDriverParameter(object,component,parameter,controller,value,diagnostic);
  });
  // Validate both ends and each authoring frontier. Runtime refuses any later
  // data-dependent failure atomically, including a driver overflow or limit.
  if(!preview.begin(document_,owner,asset,error)||!preview.seek(time,error))return false;
  return true;
}
bool EditorSession::animationPropertyBinding(AnimationClipAsset &asset,EditorEntityId owner,EditorEntityId target,u64 &id,std::string &error) const {
  const auto *object=document_.find(target);if(!object)return fail(error,"Objeto de propriedade removido");
  AnimationClipBinding binding;if(!animationPoseTargetPath(document_,owner,target,binding.path,error))return false;
  for(const auto &existing:asset.bindings)if(existing.path==binding.path){id=existing.id;return true;}
  if(asset.bindings.size()>=AnimationClipAsset::MaximumBindings||!asset.nextId||asset.nextId==~u64{0})return fail(error,"Orçamento de bindings esgotado");
  binding.id=asset.nextId++;binding.name=object->name;id=binding.id;asset.bindings.push_back(std::move(binding));return true;
}
bool EditorSession::prepareAnimationProperty(AnimationClipAsset &asset,EditorEntityId owner,EditorEntityId target,
    const AnimationPropertyAddress &address,u64 &created,std::string &error) const {
  error.clear();const auto catalog=animationPropertyCatalog(target,error);
  if(std::none_of(catalog.begin(),catalog.end(),[&](const auto &entry){return entry.address==address;}))
    return fail(error,"Propriedade não pertence ao catálogo efetivo do objeto");
  const auto *object=document_.find(target);const auto *component=object?object->components.findInstance(address.component):nullptr;
  if(!component)return fail(error,"Componente de propriedade removido");
  AnimationPropertyValue value;
  if(address.domain==AnimationPropertyDomain::GuiNode) {
    ui::GuiDocument doc;if(!loadGuiDocument(address.resource,doc,error))return false;
    const auto *node=doc.find(address.guiNode);if(!node)return fail(error,"Nó UI removido");value=node->opacity;
  } else if(!runtime::readAnimationProperty(*component,address,value,error))return false;
  AnimationClipBinding binding;if(!animationPoseTargetPath(document_,owner,target,binding.path,error))return false;
  binding.name=object->name;
  for(const auto &existing:asset.bindings)if(existing.path==binding.path)binding.sourceNode=existing.sourceNode;
  AnimationPropertyData data;if(!seedAnimationProperty(address.kind,value,0,data,error))return false;
  auto candidate=asset;u64 id=0;
  if(!candidate.addPropertyTrack(binding,address,data,id,error)||!validateAnimationPropertyClip(candidate,owner,0,error))return false;
  asset=std::move(candidate);created=id;return true;
}
bool EditorSession::remapAnimationProperty(AnimationClipAsset &asset,EditorEntityId owner,EditorEntityId target,
    u64 trackId,const AnimationPropertyAddress &address,std::string &error) const {
  auto candidate=asset;auto *track=candidate.propertyTrack(trackId);
  if(!track||address.kind!=track->data.kind)return fail(error,"Remapeamento exige um canal do mesmo tipo");
  const auto catalog=animationPropertyCatalog(target,error);
  if(std::none_of(catalog.begin(),catalog.end(),[&](const auto &entry){return entry.address==address;}))return fail(error,"Destino não está no catálogo efetivo");
  const u64 before=track->binding;u64 binding=0;
  if(!animationPropertyBinding(candidate,owner,target,binding,error))return false;
  track=candidate.propertyTrack(trackId);track->address=address;track->binding=binding;
  if(!bindingUsed(candidate,before))std::erase_if(candidate.bindings,[&](const auto &b){return b.id==before;});
  if(!candidate.valid(&error))return false;
  // Repair one broken channel even when its peers still need remapping. The
  // repaired channel must execute; unrelated broken consumers remain visible.
  auto isolated=candidate;isolated.tracks.clear();isolated.properties={*track};isolated.drivers={};
  isolated.layers={AnimationClipLayer{}};isolated.cues.clear();isolated.regions.clear();
  if(!validateAnimationPropertyClip(isolated,owner,0,error)||!validateAnimationPropertyClip(isolated,owner,isolated.duration,error))return false;
  asset=std::move(candidate);return true;
}
bool EditorSession::configureAnimationDrivers(AnimationClipAsset &asset,EditorEntityId owner,
    const AnimationClipDrivers &drivers,std::string &error) const {
  auto candidate=asset;auto value=drivers;
  // Public graph IDs are symbolic for new nodes. Allocate through the clip's
  // real ledger; editing existing drivers retains their persistent identities.
  std::set<u64> retainedNodes,retainedKeys;
  for(const auto &n:asset.drivers.graph.nodes){retainedNodes.insert(n.id);for(const auto &k:n.curve.keys)retainedKeys.insert(k.id);}
  std::map<u64,u64> ids;std::set<u64> keys;
  const auto allocate=[&](u64 &id){if(!candidate.nextId||candidate.nextId==~u64{0})return false;id=candidate.nextId++;return true;};
  for(auto &n:value.graph.nodes) {
    const u64 symbolic=n.id;
    if(!symbolic||ids.contains(symbolic))return fail(error,"Identidade simbólica de driver ausente ou duplicada");
    if(!retainedNodes.contains(symbolic)&&!allocate(n.id))return fail(error,"Identidades de driver esgotadas");
    ids.emplace(symbolic,n.id);
    for(auto &key:n.curve.keys) {
      if(key.id&&retainedKeys.contains(key.id)) {if(!keys.insert(key.id).second)return fail(error,"Chave de driver duplicada");}
      else if(!allocate(key.id))return fail(error,"Identidades de curva esgotadas");
    }
  }
  for(auto &n:value.graph.nodes)for(auto *edge:{&n.first,&n.second})if(*edge) {
    const auto found=ids.find(*edge);if(found==ids.end())return fail(error,"Operação de origem removida do driver");*edge=found->second;
  }
  for(auto &output:value.graph.outputs) {
    const auto found=ids.find(output);if(found==ids.end())return fail(error,"Saída aponta para operação removida");output=found->second;
  }
  candidate.drivers=std::move(value);
  if(!candidate.valid(&error)||!validateAnimationPropertyClip(candidate,owner,0,error)||
     !validateAnimationPropertyClip(candidate,owner,candidate.duration,error))return false;
  asset=std::move(candidate);return true;
}
}
