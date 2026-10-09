#include "editor/editor_animation_clip_preview.h"
#include "scene/animation.h"
#include "scene/skinned_mesh.h"
#include "core/rotation_math.h"
#include <algorithm>
#include <cmath>

namespace ae::editor {
bool AnimationClipPreview::findClip(const resources::AssetGuid &guid,runtime::AnimationClipView &out) const {
  if(source_.clipIds.empty()||guid!=source_.clipIds.front())return false;
  out={&source_.clips.front(),&source_,source_.clips.front().name};return true;
}
bool AnimationClipPreview::begin(const runtime::SceneGraph &scene,runtime::ObjectId owner,
                                const resources::AnimationClipAsset &clip,std::string &error) {
  error.clear();resources::AnimationClip evaluated;
  if(!scene.exists(owner)) {error="Objeto de preview ausente";return false;}
  if(!clip.compile(evaluated,&error))return false;
  runtime::SourceAnimations source;source.source=clip.source.valid()?clip.source:clip.guid;
  source.channelsValidated=true;
  source.clips.push_back(std::move(evaluated));source.clipIds.push_back(clip.guid);
  for(const auto &b:clip.bindings) {source.nodes.push_back(b.sourceNode);source.nodeNames.push_back(b.name);source.nodePaths.push_back(b.path);}
  std::vector<runtime::ObjectId> targets;runtime::resolveAnimationTargets(scene,owner,source,targets);
  for(const auto &channel:source.clips.front().channels) {
    if(channel.node>=targets.size()||!targets[channel.node]) {
      error="Binding ausente ou ambíguo no preview: "+(channel.node<source.nodePaths.size()?source.nodePaths[channel.node]:std::to_string(channel.node));return false;
    }
    if(channel.path==resources::AnimationPath::Weights&&
       !scene.find(targets[channel.node])->components.find(scene::SkinnedMesh::descriptor)) {
      error="Canal de morph exige Malha com skin no objeto: "+source.nodePaths[channel.node];return false;
    }
  }
  // A new failed begin preserves the current preview. No source state is ever
  // touched, including Animation.playAutomatically and morph weight vectors.
  auto posed=scene;std::vector<runtime::ObjectId> ids;posed.collectSubtree(posed.root(),ids);
  for(const auto id:ids)if(auto *components=posed.editComponents(id))
    for(usize i=0;i<components->size();++i)if(const auto *value=components->at(i);&value->type()==&scene::Animation::descriptor)
      static_cast<scene::Animation*>(components->editInstance(value->instanceId()))->playAutomatically=false;
  sampler_.reset();scene_=std::move(posed);source_=std::move(source);owner_=owner;
  sceneRevision_=scene.revision();clipRevision_=clip.revision;time_=0;sampler_.begin(scene_,*this);
  return true;
}
bool AnimationClipPreview::seek(float seconds,std::string &error) {
  error.clear();
  if(!active()||!std::isfinite(seconds)||seconds<0||seconds>source_.clips.front().duration) {error="Tempo de preview fora do clipe";return false;}
  // Validate evaluated values before the compositor can modify any object.
  // Singular authored quaternion curves fail visibly and keep the last pose.
  float values[resources::MaximumMorphTargets];
  for(const auto &channel:source_.clips.front().channels)
    if(!resources::sampleValidatedAnimationChannel(channel,seconds,std::span<float>(values,resources::MaximumMorphTargets))) {
      error="A curva não produz uma pose válida neste tempo";return false;
    }
  runtime::SceneAnimator::ExternalSample sample;sample.owner=owner_;sample.clip=source_.clipIds.front();sample.time=seconds;sample.weight=1;
  sampler_.setExternalSamples({sample});
  if(!sampler_.advance(0,{})||!sampler_.compositionDiagnostic().empty()) {
    error="Composição de preview recusada: "+std::string(sampler_.compositionDiagnostic());return false;
  }
  time_=seconds;return true;
}
bool AnimationClipPreview::overridePose(runtime::ObjectId target,resources::AnimationPath path,std::span<const float> values,std::string &error) {
  error.clear();const auto *object=scene_.find(target);
  if(!active()||!object) {error="Alvo da pose isolada ausente";return false;}
  for(float v:values)if(!std::isfinite(v)) {error="Pose isolada não finita";return false;}
  if(path==resources::AnimationPath::Weights) {
    const auto *skin=static_cast<const scene::SkinnedMesh*>(object->components.find(scene::SkinnedMesh::descriptor));
    if(!skin||values.size()!=skin->blendShapeWeights.size()) {error="Morph incompatível com a malha";return false;}
    const auto instance=skin->instanceId();auto *components=scene_.editComponents(target);
    auto *edited=static_cast<scene::SkinnedMesh*>(components->editInstance(instance));
    for(usize i=0;i<values.size();++i)edited->blendShapeWeights[i]=values[i]*100;
    return true;
  }
  auto pose=object->transform;
  if(path==resources::AnimationPath::Rotation) {
    if(values.size()!=4||!rotationEulerXYZNear(values.data(),pose.rotationDegrees,pose.rotationDegrees)) {error="Quaternion isolado inválido";return false;}
  } else if(values.size()==3&&path==resources::AnimationPath::Translation)std::copy_n(values.begin(),3,pose.position);
  else if(values.size()==3&&path==resources::AnimationPath::Scale)std::copy_n(values.begin(),3,pose.scale);
  else {error="Propriedade da pose isolada inválida";return false;}
  if(!runtime::isTransformValid(pose)||!scene_.setTransform(target,pose)) {error="Pose isolada singular ou inválida";return false;}
  return true;
}
void AnimationClipPreview::cancel() {
  sampler_.reset();scene_.reset();source_={};owner_=runtime::kInvalidObject;time_=0;sceneRevision_=0;clipRevision_=0;
}
} // namespace ae::editor
