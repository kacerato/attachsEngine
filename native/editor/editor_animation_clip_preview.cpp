#include "editor/editor_animation_clip_preview.h"
#include "scene/animation.h"
#include "scene/skinned_mesh.h"
#include "core/rotation_math.h"
#include "runtime/scene_gui.h"
#include "ui/gui_world.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace ae::editor {
bool AnimationClipPreview::readProperty(runtime::ObjectId object,const resources::AnimationPropertyAddress &address,
    resources::AnimationPropertyValue &out,std::string &error) {
  if(!active()){error="Preview de propriedades inativo";return false;}
  if(address.domain==resources::AnimationPropertyDomain::GuiNode&&!gui_.contains({object,address.component})) {
    ui::GuiDocument document;
    if(!guiLoader_||!guiLoader_(address.resource,document,error))return false;
    auto instance=std::make_unique<ui::GuiRuntime>();instance->load(document,false);
    gui_.emplace(std::pair{object,address.component},std::move(instance));
  }
  const runtime::AnimationGuiResolver resolver=[this](auto id,auto component,auto)->ui::GuiRuntime* {
    const auto found=gui_.find({id,component});return found==gui_.end()?nullptr:found->second.get();
  };
  return runtime::readAnimationPropertyTarget(scene_,object,address,resolver,out,error);
}
void AnimationClipPreview::drawGui(ui::UiDrawList &out,const ui::UiFontMetrics &metrics,
    const renderer::PerspectiveFrustum &camera,const ui::UiRect &viewport,const ui::UiRect &surface) {
  for(auto &[key,instance]:gui_) {
    const auto *object=scene_.find(key.first);
    const auto *value=object?object->components.findInstance(key.second):nullptr;
    if(!value||&value->type()!=&scene::UiCanvas::descriptor||!scene_.activeInHierarchy(key.first))continue;
    const auto &component=static_cast<const scene::UiCanvas&>(*value);if(!component.enabled)continue;
    const auto canvas=runtime::uiCanvasPresentation(component);
    if(canvas.mode==ui::GuiCanvasMode::Screen) {
      instance->layout(viewport);out.pushClip(viewport);instance->draw(out);out.popClip();
    } else {
      float host[16];ui::GuiWorldFrame frame;
      if(!runtime::worldMatrix(scene_,key.first,host)||!frame.configure(canvas,camera,viewport,surface,host))continue;
      instance->layout({0,0,canvas.resolution.x,canvas.resolution.y});
      ui::UiDrawList projected;projected.begin({0,0,canvas.resolution.x,canvas.resolution.y},metrics);
      instance->draw(projected);projected.projectRange(0,frame.projection(),canvas.occlusion,viewport);out.append(projected);
    }
  }
}
std::vector<runtime::ObjectId> animationPreviewJoints(const runtime::SceneGraph &scene,runtime::ObjectId owner) {
  std::vector<runtime::ObjectId> nodes,result;scene.collectSubtree(owner,nodes);
  std::unordered_set<runtime::ObjectId> seen;
  for(const auto id:nodes)if(const auto *object=scene.find(id))
    for(usize slot=0;slot<object->components.size();++slot) {
      const auto *component=object->components.at(slot);
      if(&component->type()!=&scene::SkinnedMesh::descriptor)continue;
      const auto *skin=static_cast<const scene::SkinnedMesh*>(component);
      for(const auto bone:skin->bones)if(bone&&scene.exists(bone)&&
          (bone==owner||scene.isDescendantOf(bone,owner))&&seen.insert(bone).second)result.push_back(bone);
    }
  std::sort(result.begin(),result.end());return result;
}
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
  std::map<std::pair<runtime::ObjectId,u64>,std::unique_ptr<ui::GuiRuntime>> preparedGui;
  std::vector<resources::CompiledAnimationDriverTarget> propertyTargets;
  for(const auto &property:source.clips.front().properties)propertyTargets.push_back({property.node,property.address});
  if(source.clips.front().drivers) {
    for(const auto &input:source.clips.front().drivers->inputs)if(input.kind==resources::AnimationDriverInputKind::Property)propertyTargets.push_back({input.node,input.address});
    for(const auto &output:source.clips.front().drivers->targets)propertyTargets.push_back(output);
  }
  for(const auto &channel:propertyTargets) {
    if(channel.node>=targets.size()||!targets[channel.node]) {error="Binding de propriedade ausente no preview: "+channel.address.property;return false;}
    const auto target=targets[channel.node];const auto &address=channel.address;
    if(address.domain==resources::AnimationPropertyDomain::GuiNode) {
      const auto key=std::pair{target,address.component};
      if(!preparedGui.contains(key)) {
        ui::GuiDocument document;
        if(!guiLoader_||!guiLoader_(address.resource,document,error)) {if(error.empty())error="Documento UI indisponível no preview";return false;}
        auto runtime=std::make_unique<ui::GuiRuntime>();runtime->load(document,false);preparedGui.emplace(key,std::move(runtime));
      }
    }
    resources::AnimationPropertyValue value;
    const runtime::AnimationGuiResolver resolver=[&](runtime::ObjectId id,u64 component,resources::AssetGuid)->ui::GuiRuntime* {
      const auto found=preparedGui.find({id,component});return found==preparedGui.end()?nullptr:found->second.get();
    };
    if(!runtime::readAnimationPropertyTarget(scene,target,address,resolver,value,error))return false;
  }
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
  gui_=std::move(preparedGui);
  sampler_.setPropertyGuiResolver([this](runtime::ObjectId object,u64 component,resources::AssetGuid)->ui::GuiRuntime* {
    const auto found=gui_.find({object,component});return found==gui_.end()?nullptr:found->second.get();
  });
  sampler_.setDriverResolvers([this](runtime::ObjectId object,u64 component,resources::AssetGuid)->ui::GuiRuntime* {
    const auto found=gui_.find({object,component});return found==gui_.end()?nullptr:found->second.get();
  },driverParameter_);
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
  sampler_.reset();scene_.reset();source_={};gui_.clear();owner_=runtime::kInvalidObject;time_=0;sceneRevision_=0;clipRevision_=0;
}
const ui::GuiDocument *AnimationClipPreview::guiDocument(runtime::ObjectId object,u64 component) const {
  const auto found=gui_.find({object,component});return found==gui_.end()?nullptr:&found->second->document();
}
} // namespace ae::editor
