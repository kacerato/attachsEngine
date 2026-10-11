#include "editor/editor_rig_preview.h"
#include "core/rotation_math.h"
#include "runtime/rig_placement.h"
#include <algorithm>

namespace ae::editor {
namespace {
bool fail(std::string &error,std::string text){error=std::move(text);return false;}
bool same(const runtime::Transform &a,const runtime::Transform &b) {
  resources::RigTransform x,y;
  return runtime::rigTransformFromScene(a,x)&&runtime::rigTransformFromScene(b,y)&&x.position==y.position&&x.scale==y.scale&&rotationDistanceDegrees(x.rotation.data(),y.rotation.data())<.001;
}
}
bool EditorRigPreview::begin(const runtime::SceneGraph &scene,runtime::ObjectId root,const resources::RigAsset &asset,u64 epoch,std::string &error) {
  error.clear();resources::CompiledRig compiled;
  if(!epoch||!resources::compileRig(asset,compiled,error))return false;
  runtime::RigBindings bindings;
  runtime::bindRigToScene(asset,compiled,scene,root,bindings);
  if(!scene.exists(root)||bindings.objects.size()!=compiled.joints.size()||bindings.objects.empty()||bindings.objects.front()!=root)return fail(error,"Raiz da prévia exige vínculo/revalidação do rig.");
  std::vector<resources::RigTransform> local(compiled.joints.size());std::vector<runtime::Transform> original(local.size());
  for(usize i=0;i<local.size();++i) {
    const auto *object=scene.find(bindings.objects[i]);
    if(!object){local[i]=compiled.joints[i].reference;continue;}
    original[i]=object->transform;
    if(!runtime::rigTransformFromScene(object->transform,local[i]))return fail(error,"Escala da junta não pode ser avaliada no rig.");
    if(compiled.joints[i].parent<0&&!runtime::rigRootToResource(local[i],bindings.placement,local[i]))return fail(error,"Placement da raiz não é representável.");
  }
  // Placement normalization is performed at the binding/read boundary. This
  // read succeeds for a complete hierarchy; missing unrelated joints retain
  // references above and are refused only when an operation addresses them.
  if(bindings.issues.empty()&&!runtime::readRigScenePose(compiled,bindings,scene,local,error))return false;
  scene_=scene;asset_=asset;compiled_=std::move(compiled);bindings_=std::move(bindings);
  local_=std::move(local);original_=std::move(original);mask_.assign(local_.size(),0);owner_=root;epoch_=epoch;return true;
}
bool EditorRigPreview::publish(std::vector<resources::RigTransform> candidate,std::span<const u8> channels,std::string &error) {
  std::vector<runtime::RigSceneChange> changes;
  if(!runtime::planRigScenePose(compiled_,bindings_,scene_,candidate,channels,changes,error)||!runtime::applyRigSceneChanges(scene_,changes,error))return false;
  local_=std::move(candidate);for(usize i=0;i<mask_.size();++i)mask_[i]|=channels[i];return true;
}
bool EditorRigPreview::control(u64 id,const resources::RigTransform &value,std::string &error) {
  if(!active())return fail(error,"Prévia do rig está fechada.");
  const auto *control=asset_.control(id);if(!control)return fail(error,"Controle ausente.");
  std::vector<resources::RigTransform> candidate(local_.size());
  if(!resources::applyRigControl(asset_,compiled_,id,value,local_,candidate,error))return false;
  std::vector<u8> channels(local_.size());channels[compiled_.index.at(control->joint)]=control->channels;
  return publish(std::move(candidate),channels,error);
}
bool EditorRigPreview::mirror(u8 channels,bool referenceMode,std::string &error) {
  if(!active())return fail(error,"Prévia do rig está fechada.");
  std::vector<resources::RigTransform> candidate(local_.size());
  if(!resources::mirrorRigPose(asset_,compiled_,local_,candidate,channels,referenceMode,error))return false;
  std::vector<u8> mask(local_.size());
  for(const auto &pair:asset_.mirror) {
    const auto *left=asset_.control(pair.left),*right=asset_.control(pair.right);const u8 effective=channels&left->channels&right->channels;
    mask[compiled_.index.at(left->joint)]=effective;mask[compiled_.index.at(right->joint)]=effective;
  }
  return publish(std::move(candidate),mask,error);
}
bool EditorRigPreview::pose(const resources::RigPose &pose,float weight,std::string &error,bool acceptRevisedRig) {
  if(!active())return fail(error,"Prévia do rig está fechada.");
  std::vector<resources::RigTransform> candidate(local_.size());
  if(!resources::blendRigPose(compiled_,pose,weight,local_,candidate,error,acceptRevisedRig))return false;
  std::vector<u8> mask(local_.size());for(const auto &entry:pose.entries)mask[compiled_.index.at(entry.joint)]=entry.channels;
  return publish(std::move(candidate),mask,error);
}
bool EditorRigPreview::changes(const runtime::SceneGraph &live,u64 epoch,std::vector<runtime::RigSceneChange> &out,std::string &error) const {
  error.clear();if(!active()||epoch!=epoch_)return fail(error,"Cena da prévia foi substituída; reabra o rig.");
  std::vector<runtime::RigSceneChange> candidate;
  if(!runtime::planRigScenePose(compiled_,bindings_,live,local_,mask_,candidate,error))return false;
  for(auto &change:candidate) {
    const auto at=std::find(bindings_.objects.begin(),bindings_.objects.end(),change.object);
    if(at==bindings_.objects.end()||!same(change.before,original_[at-bindings_.objects.begin()]))return fail(error,"Junta mudou depois da abertura da prévia; cancele ou reabra.");
  }
  out=std::move(candidate);return true;
}
bool EditorRigPreview::pending() const {return std::any_of(mask_.begin(),mask_.end(),[](u8 value){return value!=0;});}
void EditorRigPreview::cancel() {scene_.reset();asset_={};compiled_={};bindings_={};owner_=0;epoch_=0;local_.clear();original_.clear();mask_.clear();}
} // namespace ae::editor
