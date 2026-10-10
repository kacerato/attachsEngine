#include "runtime/rig_instance.h"
#include "resources/animation_binding_path.h"
#include "scene/import_link.h"
#include "core/rotation_math.h"
#include <algorithm>
#include <set>

namespace ae::runtime {
namespace {
bool fail(std::string &error,std::string message){error=std::move(message);return false;}
std::string relativePath(const SceneGraph &scene,ObjectId root,ObjectId id) {
  std::vector<std::string> names;
  for(auto at=id;at!=root;) {
    const auto *object=scene.find(at);if(!object||!object->parent)return {};
    names.push_back(resources::animationBindingSegment(object->name));at=object->parent;
  }
  std::string path;for(auto it=names.rbegin();it!=names.rend();++it){if(!path.empty())path+='/';path+=*it;}return path;
}
ObjectId resolvePath(const SceneGraph &scene,ObjectId root,std::string_view path) {
  auto at=root;
  for(usize begin=0;begin<path.size();) {
    const auto end=path.find('/',begin);const auto segment=path.substr(begin,end==path.npos?path.size()-begin:end-begin);
    std::string name;if(!resources::animationBindingName(segment,name))return 0;
    ObjectId found=0;
    for(auto child:scene.childrenOf(at))if(const auto *object=scene.find(child);object&&name==object->name){if(found)return 0;found=child;}
    if(!found)return 0;
    at=found;if(end==path.npos)break;begin=end+1;
  }
  return at;
}
bool bindingsMatch(const resources::CompiledRig &rig,const RigBindings &bindings) {
  return bindings.rig==rig.guid&&bindings.revision==rig.revision&&bindings.objects.size()==rig.joints.size();
}
bool bindingLive(const resources::CompiledRig &rig,const RigBindings &bindings,const SceneGraph &scene,usize index) {
  if(index>=bindings.objects.size())return false;
  const auto *object=scene.find(bindings.objects[index]);if(!object)return false;
  const auto parent=rig.joints[index].parent;
  return parent<0?object->id==bindings.root:object->parent==bindings.objects[parent];
}
}
bool rigTransformFromScene(const Transform &scene,resources::RigTransform &out) {
  resources::RigTransform value;std::copy_n(scene.position,3,value.position.begin());std::copy_n(scene.scale,3,value.scale.begin());
  rotationQuaternionXYZ(scene.rotationDegrees,value.rotation.data());if(!resources::validRigTransform(value))return false;out=value;return true;
}
bool rigTransformToScene(const resources::RigTransform &rig,const Transform &near,Transform &out) {
  if(!resources::validRigTransform(rig))return false;
  Transform value=near;std::copy_n(rig.position.begin(),3,value.position);std::copy_n(rig.scale.begin(),3,value.scale);
  if(!rotationEulerXYZNear(rig.rotation.data(),near.rotationDegrees,value.rotationDegrees)||!isTransformValid(value))return false;
  out=value;return true;
}
bool captureRig(const SceneGraph &scene,ObjectId root,resources::AssetGuid guid,std::string_view name,resources::RigAsset &out,std::string &error) {
  error.clear();if(!scene.exists(root)||root==scene.root()||!guid.valid())return fail(error,"Selecione a raiz explícita do mecanismo ou personagem.");
  std::vector<ObjectId> objects;scene.collectSubtree(root,objects);
  if(objects.empty()||objects.size()>resources::RigAsset::MaximumJoints)return fail(error,"Hierarquia excede orçamento do rig.");
  resources::RigAsset value;value.guid=guid;value.name=name;std::unordered_map<ObjectId,u64> ids;
  for(auto id:objects) {
    const auto *object=scene.find(id);resources::RigJoint joint;joint.id=value.nextId++;joint.parent=id==root?0:ids.at(object->parent);
    joint.name=object->name;joint.path=relativePath(scene,root,id);
    if(!rigTransformFromScene(object->transform,joint.bind))return fail(error,"Junta exige escala uniforme positiva: "+joint.name);
    joint.reference=joint.bind;
    if(const auto *link=scene::importLink(object->components);link&&!link->orphan&&!link->unlinked&&link->primitive<0)joint.sourceNode=link->node;
    ids.emplace(id,joint.id);value.joints.push_back(joint);
    resources::RigControl control;control.id=value.nextId++;control.joint=joint.id;control.name=joint.name;control.channels=resources::RigRotation;
    value.controls.push_back(control);
  }
  if(!value.valid(&error))return false;
  out=std::move(value);return true;
}
bool bindRigToScene(const resources::RigAsset &asset,const resources::CompiledRig &rig,const SceneGraph &scene,ObjectId root,RigBindings &out) {
  RigBindings value;value.rig=rig.guid;value.revision=rig.revision;value.root=root;value.objects.resize(rig.joints.size());
  if(asset.guid!=rig.guid||asset.revision!=rig.revision||!scene.exists(root))return false;
  std::vector<ObjectId> objects;scene.collectSubtree(root,objects);std::set<ObjectId> assigned;
  for(usize i=0;i<rig.joints.size();++i) {
    const auto *joint=asset.joint(rig.joints[i].id);ObjectId found=0;bool ambiguous=false;
    if(joint->sourceNode.valid())for(auto objectId:objects) {
      const auto *object=scene.find(objectId);const auto *link=scene::importLink(object->components);
      if(link&&!link->orphan&&!link->unlinked&&link->primitive<0&&link->node==joint->sourceNode){if(found)ambiguous=true;found=objectId;}
    }
    if(ambiguous)found=0;else if(!found)found=resolvePath(scene,root,joint->path);
    const auto parent=rig.joints[i].parent;const auto *object=scene.find(found);
    if(!object||ambiguous||assigned.contains(found)||(parent<0?found!=root:object->parent!=value.objects[parent])) {
      value.issues.push_back({joint->id,"Junta ausente, ambígua ou reparentada: "+joint->name});continue;
    }
    value.objects[i]=found;assigned.insert(found);
  }
  const bool complete=value.issues.empty();out=std::move(value);return complete;
}
bool readRigScenePose(const resources::CompiledRig &rig,const RigBindings &bindings,const SceneGraph &scene,std::span<resources::RigTransform> local,std::string &error) {
  error.clear();if(!bindingsMatch(rig,bindings)||local.size()!=rig.joints.size())return fail(error,"Binding do rig exige revalidação.");
  for(usize i=0;i<local.size();++i)if(!bindingLive(rig,bindings,scene,i)||!rigTransformFromScene(scene.find(bindings.objects[i])->transform,local[i]))return fail(error,"Junta ausente ou escala incompatível: "+std::to_string(rig.joints[i].id));
  return true;
}
bool planRigScenePose(const resources::CompiledRig &rig,const RigBindings &bindings,const SceneGraph &scene,std::span<const resources::RigTransform> local,
    std::span<const u8> channels,std::vector<RigSceneChange> &out,std::string &error) {
  error.clear();if(!bindingsMatch(rig,bindings)||local.size()!=rig.joints.size()||channels.size()!=local.size())return fail(error,"Binding ou máscara de pose inválidos.");
  std::vector<RigSceneChange> candidate;
  for(usize i=0;i<local.size();++i) {
    if(channels[i]>resources::RigAll)return fail(error,"Máscara de canais inválida.");
    if(!channels[i])continue;
    if(!bindingLive(rig,bindings,scene,i))return fail(error,"Junta afetada ausente ou reparentada: "+std::to_string(rig.joints[i].id));
    const auto *object=scene.find(bindings.objects[i]);resources::RigTransform before;
    if(!rigTransformFromScene(object->transform,before))return fail(error,"Escala da junta não representável.");
    auto after=before;if(channels[i]&resources::RigPosition)after.position=local[i].position;
    if(channels[i]&resources::RigRotation)after.rotation=local[i].rotation;
    if(channels[i]&resources::RigScale)after.scale=local[i].scale;
    RigSceneChange change{object->id,object->transform,{}};
    if(!rigTransformToScene(after,change.before,change.after))return fail(error,"Pose não representável na cena.");
    candidate.push_back(change);
  }
  out=std::move(candidate);return true;
}
bool applyRigSceneChanges(SceneGraph &scene,std::span<const RigSceneChange> changes,std::string &error) {
  error.clear();std::set<ObjectId> affected;
  for(const auto &change:changes) {
    const auto *object=scene.find(change.object);
    if(!object||!affected.insert(change.object).second||!isTransformValid(change.after))return fail(error,"Pose recusada antes de alterar a cena.");
    resources::RigTransform current,before;
    if(!rigTransformFromScene(object->transform,current)||!rigTransformFromScene(change.before,before)||current.position!=before.position||current.scale!=before.scale||rotationDistanceDegrees(current.rotation.data(),before.rotation.data())>.001)
      return fail(error,"Junta foi alterada depois do preparo da pose.");
  }
  for(const auto &change:changes)scene.setTransform(change.object,change.after);
  return true;
}
bool RetargetInstance::configure(resources::CompiledRetarget program,resources::CompiledRigAnimation clip,RigBindings target,std::string &error) {
  error.clear();if(!resources::validateRetargetAnimation(program,clip,error)||!bindingsMatch(program.target,target))return fail(error,"Retargeting exige clipe e bindings das revisões compiladas.");
  for(auto id:target.objects)if(!id)return fail(error,"Destino tem junta ausente; nenhuma transferência foi ativada.");
  program_=std::move(program);clip_=std::move(clip);targetBindings_=std::move(target);
  scratch_.prepare(program_);sourceLocal_.resize(program_.source.joints.size());targetLocal_.resize(program_.target.joints.size());targetValues_.resize(targetLocal_.size());ready_=true;return true;
}
bool RetargetInstance::evaluate(SceneGraph &target,float time,std::string &error) {
  error.clear();if(!ready_)return fail(error,"Instância de retargeting não está configurada.");
  if(!resources::sampleRigAnimation(program_.source,clip_,time,sourceLocal_)||!resources::evaluateRetarget(program_,sourceLocal_,targetLocal_,scratch_))return fail(error,"Pose da transferência não representável.");
  for(usize i=0;i<targetLocal_.size();++i) {
    if(!bindingLive(program_.target,targetBindings_,target,i))return fail(error,"Junta destino removida ou reparentada; revalide o rig.");
    if(!rigTransformToScene(targetLocal_[i],target.find(targetBindings_.objects[i])->transform,targetValues_[i]))return fail(error,"Pose não pode ser representada pela cena.");
  }
  for(usize i=0;i<targetValues_.size();++i)target.setTransform(targetBindings_.objects[i],targetValues_[i]);
  return true;
}
void RetargetInstance::reset() {*this=RetargetInstance{};}
} // namespace ae::runtime
