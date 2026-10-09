#include "editor/editor_session.h"
#include "runtime/transform_math.h"
#include "resources/animation_binding_path.h"
#include "scene/skinned_mesh.h"
#include "scene/import_link.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>

namespace ae::editor {
namespace {
namespace w=clip_widget;
using Asset=resources::AnimationClipAsset;
struct Row {u64 track;u32 component;};
std::vector<Row> rows(const Asset &asset,u64 layer) {
  std::vector<Row> result;for(const auto &t:asset.tracks)if(t.layer==layer)for(u32 c=0;c<t.components();++c)result.push_back({t.id,c});return result;
}
const resources::AnimationCurveKey *selectedKey(const Asset &asset,u64 track,u32 component,u64 key) {
  const auto *t=asset.track(track);if(!t||component>=t->curves.size())return nullptr;
  for(const auto &k:t->curves[component].keys)if(k.id==key)return &k;
  return nullptr;
}
void selectKeys(EditorScreenState &state,const Asset &asset,std::span<const resources::AnimationKeyAddress> keys) {
  std::vector<resources::AnimationKeyAddress> expanded;std::string error;
  if(keys.empty())state.clipSelection.clear();
  else if(resources::expandAnimationKeySelection(asset,keys,expanded,error))state.clipSelection=std::move(expanded);
}
bool relativeBinding(const EditorDocument &document,EditorEntityId owner,EditorEntityId target,
                     resources::AnimationClipBinding &binding,std::string &error) {
  const auto *object=document.find(target);
  if(!object||!document.exists(owner)||(target!=owner&&!document.isDescendantOf(target,owner))) {error="Objeto deve pertencer à raiz do clipe";return false;}
  binding.name=object->name;binding.path.clear();
  for(auto current=target;current!=owner;) {
    const auto *node=document.find(current);if(!node||!node->parent) {error="Caminho do objeto indisponível";return false;}
    u32 matches=0;for(const auto sibling:document.childrenOf(node->parent))if(const auto *other=document.find(sibling);other&&std::string_view(other->name)==node->name)++matches;
    if(matches!=1) {error="Nomes iguais entre irmãos tornam o binding ambíguo; renomeie o objeto";return false;}
    const auto segment=resources::animationBindingSegment(node->name);binding.path=segment+(binding.path.empty()?"":"/"+binding.path);current=node->parent;
  }
  return true;
}
}
bool EditorSession::createAnimationClip(EditorEntityId owner,float duration,resources::AssetGuid &guid,std::string &error,resources::AnimationRotationMode mode) {
  const auto *object=document_.find(owner);
  if(!object||!std::isfinite(duration)||duration<0||duration>86400||static_cast<u8>(mode)>2) {error="Objeto, modo ou duração inválidos";return false;}
  // Creating animation does not add an Animator or a character controller.
  // A reusable root binding animates any object using its real local TRS.
  std::string path="Clipes/Clipe.aeclip";
  for(u32 n=2;files_.exists(path)||assets_.findByPath(path);++n)path="Clipes/Clipe "+std::to_string(n)+".aeclip";
  Asset value;value.guid=resources::assetGuidFromSeed("clip:"+files_.rootPath()+":"+path+":"+
      std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+":"+std::to_string(++importInstanceCounter_));
  value.name=path.substr(7,path.size()-7-7);value.duration=duration;
  value.bindings.push_back({value.nextId++,{},"",object->name});
  float rotation[4];runtime::transformRotationQuaternion(object->transform,rotation);
  for(u32 p=0;p<3;++p) {
    resources::AnimationClipTrack track;track.id=value.nextId++;track.binding=value.bindings[0].id;
    track.path=static_cast<resources::AnimationPath>(p);if(p==1)track.rotationMode=mode;
    track.curves.resize(track.components());
    for(u32 c=0;c<track.components();++c) {
      resources::AnimationCurveKey key;key.id=value.nextId++;key.incoming=key.outgoing=resources::AnimationTangentMode::Linear;
      key.value=p==0?object->transform.position[c]:p==1?(mode==resources::AnimationRotationMode::Euler?object->transform.rotationDegrees[c]:c==4?0:rotation[c]):object->transform.scale[c];
      track.curves[c].keys.push_back(key);
      if(duration>0) {key.id=value.nextId++;key.time=duration;track.curves[c].keys.push_back(key);}
    }
    value.tracks.push_back(std::move(track));
  }
  if(!createAnimationClipAsset(value,path,error))return false;
  guid=value.guid;return true;
}
bool EditorSession::extractAnimationClip(resources::AssetGuid sourceClip,EditorEntityId owner,resources::AssetGuid &guid,std::string &error) {
  error.clear();runtime::AnimationClipView view;
  if(isPlaying()||playMirrorOpen_||history_.isOpen()||clipDrag_) {error="Pare o Play e conclua a edição";return false;}
  if(!document_.exists(owner)||!mapScene_.findClip(sourceClip,view)||!view.clip||!view.source||animationClipAsset(sourceClip)) {
    error="Selecione um clipe importado e uma raiz válida";return false;
  }
  const auto *record=assets_.find(view.source->source);
  if(!record||record->contentHash.size()!=64) {error="Origem do clipe não possui revisão registrada";return false;}
  std::vector<runtime::ObjectId> targets;runtime::resolveAnimationTargets(document_,owner,*view.source,targets);
  // Preserve native sampler values/tangents. Compact only unused source nodes;
  // a missing used node refuses the whole extraction before any file is made.
  auto clip=*view.clip;std::vector<resources::AnimationClipBinding> bindings;
  std::vector<u32> remap(targets.size(),std::numeric_limits<u32>::max());
  for(auto &channel:clip.channels) {
    if(channel.node>=targets.size()||!targets[channel.node]) {error="Nó animado ausente ou ambíguo na raiz selecionada";return false;}
    const auto original=channel.node;
    if(remap[original]==std::numeric_limits<u32>::max()) {
      resources::AnimationClipBinding binding;
      if(!relativeBinding(document_,owner,targets[original],binding,error))return false;
      const auto *object=document_.find(targets[original]);const auto *link=scene::importLink(object->components);
      if(link&&link->source==view.source->source&&link->primitive<0&&!link->unlinked&&!link->orphan)binding.sourceNode=link->node;
      remap[original]=static_cast<u32>(bindings.size());bindings.push_back(std::move(binding));
    }
    channel.node=remap[original];
  }
  std::string path="Clipes/Extraído.aeclip";
  for(u32 n=2;files_.exists(path)||assets_.findByPath(path);++n)path="Clipes/Extraído "+std::to_string(n)+".aeclip";
  const auto identity=resources::assetGuidFromSeed("clip:"+files_.rootPath()+":"+path+":"+
      std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+":"+std::to_string(++importInstanceCounter_));Asset value;
  if(!resources::authorAnimationClip(clip,identity,view.source->source,sourceClip,record->contentHash,bindings,value,error))return false;
  // Catch incompatible morph bindings and singular evaluated endpoints before
  // creating the project resource; preview itself remains isolated.
  AnimationClipPreview candidate;
  if(!candidate.begin(document_,owner,value,error)||!candidate.seek(0,error)||!candidate.seek(value.duration,error))return false;
  if(!createAnimationClipAsset(value,path,error))return false;
  guid=identity;return true;
}
bool EditorSession::addAnimationClipTrack(resources::AssetGuid guid,u32 expectedRevision,EditorEntityId owner,EditorEntityId target,
                                         resources::AnimationPath path,resources::AnimationRotationMode mode,u64 &trackId,std::string &error) {
  error.clear();const auto *object=document_.find(target);resources::AnimationClipBinding binding;
  if(!relativeBinding(document_,owner,target,binding,error))return false;
  // Existing imported bindings retain source-node identity when adding another
  // property. A path-only new binding must not collide with that identity.
  if(const auto *live=animationClipAsset(guid))for(const auto &existing:live->bindings)if(existing.path==binding.path) {binding.sourceNode=existing.sourceNode;break;}
  const auto *posed=state_.clipOpen&&state_.clipGuid==guid&&state_.clipOwner==owner?evaluatedEditorScene().find(target):object;if(!posed)posed=object;
  std::vector<float> values;
  if(path==resources::AnimationPath::Translation)values.assign(posed->transform.position,posed->transform.position+3);
  else if(path==resources::AnimationPath::Scale)values.assign(posed->transform.scale,posed->transform.scale+3);
  else if(path==resources::AnimationPath::Rotation) {
    if(mode==resources::AnimationRotationMode::Euler)values.assign(posed->transform.rotationDegrees,posed->transform.rotationDegrees+3);
    else {float q[4];runtime::transformRotationQuaternion(posed->transform,q);values.assign(q,q+4);}
  } else if(path==resources::AnimationPath::Weights) {
    const auto *skin=static_cast<const scene::SkinnedMesh*>(posed->components.find(scene::SkinnedMesh::descriptor));
    if(!skin||skin->blendShapeWeights.empty()) {error="Objeto não possui pesos de morph disponíveis";return false;}
    values=skin->blendShapeWeights;for(auto &value:values)value*=.01f;
  } else {error="Propriedade não suportada";return false;}
  u64 id=0;const u64 layer=state_.clipOpen&&state_.clipGuid==guid?state_.clipLayer:0;
  const bool ok=editAnimationClipAsset(guid,expectedRevision,[&](auto &candidate){
    u64 base=0;
    for(const auto &b:candidate.bindings)if(b.path==binding.path)for(const auto &t:candidate.tracks)if(!t.layer&&t.binding==b.id&&t.path==path)base=t.id;
    if(!base&&!candidate.addTrack(binding,path,mode,values,base,error))return false;
    if(!layer) {id=base;return true;}
    return candidate.addLayerTrack(layer,base,false,id,error,path==resources::AnimationPath::Rotation?std::optional{mode}:std::nullopt);
  },error);
  if(ok)trackId=id;
  return ok;
}
bool EditorSession::openAnimationClip(resources::AssetGuid guid,EditorEntityId owner,std::string &error) {
  error.clear();
  if(isPlaying()||playMirrorOpen_||history_.isOpen()||clipDrag_) {error="Pare o Play e conclua a edição";return false;}
  const auto *asset=animationClipAsset(guid);
  if(!asset) {error="Clipe editável ausente";return false;}
  if(!document_.exists(owner)) {error="Raiz de preview ausente";return false;}
  std::string previewDiagnostic;
  if(!clipPreview_.begin(document_,owner,*asset,previewDiagnostic)||!clipPreview_.seek(0,previewDiagnostic))clipPreview_.cancel();
  state_.clipLayer=0;state_.clipLayersShown=state_.clipLayerPicker=false;
  state_.clipOpen=true;state_.clipGuid=guid;state_.clipOwner=owner;state_.clipTime=0;
  viewportPointers_.clear();pinchDistance_=0;
  state_.clipPlaying=false;state_.clipPicker=false;state_.clipNewPicker=false;state_.clipTangentPicker=false;state_.clipRow=0;state_.clipPage=0;
  state_.clipImportedPicker=false;state_.clipSourceEntries.clear();
  state_.clipModeSide=0;
  state_.clipAuthoringPicker=0;state_.clipTargetPage=0;state_.clipTargetBranch=owner;state_.clipTargetNode=owner;state_.clipPoseShown=false;
  state_.clipTrack=asset->tracks.front().id;state_.clipComponent=0;state_.clipKey=0;
  state_.clipSelection.clear();state_.clipSelectionMode=state_.clipSelectionAdd=state_.clipSelecting=false;state_.clipEditPicker=false;
  state_.clipBakeShown=state_.clipBakeHasReport=false;
  state_.clipBakeConsolidate=state_.clipBakeReferenceShown=state_.clipBakeReportConsolidated=false;
  state_.workspace=EditorWorkspace::Scene;state_.animatorOpen=false;state_.tool=EditorGizmoMode::Select;
  clipEpoch_=sceneEpoch_;state_.clipDiagnostic=previewDiagnostic;appearanceChanged_=true;refreshAnimationClip();frameAnimationClip();return true;
}
void EditorSession::closeAnimationClip() {
  clipPreview_.cancel();clipDrag_.reset();clipPointer_=0;
  clipHandle_=0;state_.clipTangentPicker=false;state_.clipModeSide=0;
  viewportPointers_.clear();pinchDistance_=0;
  state_.clipLayersShown=state_.clipLayerPicker=false;
  state_.clipOpen=false;state_.clipPlaying=false;state_.clipAsset=nullptr;state_.clipPicker=false;state_.clipNewPicker=false;
  state_.clipImportedPicker=false;state_.clipSourceEntries.clear();
  state_.clipAuthoringPicker=0;state_.clipPoseShown=false;
  state_.clipSelection.clear();state_.clipSelecting=state_.clipEditPicker=false;clipPressSelection_.clear();clipNumberSelection_.clear();
  state_.clipBakeShown=state_.clipBakeHasReport=false;
  state_.clipBakeConsolidate=state_.clipBakeReferenceShown=state_.clipBakeReportConsolidated=false;
  appearanceChanged_=true;
}
bool EditorSession::seekAnimationClip(float seconds,std::string &error) {
  if(!state_.clipOpen||!clipPreview_.active()) {error="Abra um clipe";return false;}
  if(!clipPreview_.seek(seconds,error))return false;
  state_.clipTime=seconds;appearanceChanged_=true;return true;
}
void EditorSession::refreshAnimationClip() {
  state_.clipAssets=&animationClipAssets_;state_.clipAsset=nullptr;
  if(!state_.clipOpen)return;
  const auto *asset=clipDrag_?&*clipDrag_:animationClipAsset(state_.clipGuid);
  if(!asset||clipEpoch_!=sceneEpoch_||!document_.exists(state_.clipOwner)||isPlaying()) {closeAnimationClip();return;}
  state_.clipAsset=asset;
  if(state_.clipBakeHasReport&&(state_.clipBakeRevision!=asset->revision||state_.clipBakeTrack!=state_.clipTrack))state_.clipBakeHasReport=false;
  // Published clip revisions are immutable. Reconcile retired IDs only when
  // the evaluated revision changes, not on every editor frame or drag sample.
  if(!clipDrag_&&!state_.clipSelection.empty()&&clipPreview_.clipRevision()!=asset->revision) {
    std::vector<resources::AnimationKeyAddress> retained;
    for(const auto &address:state_.clipSelection)if(selectedKey(*asset,address.track,address.component,address.key))retained.push_back(address);
    selectKeys(state_,*asset,retained);
  }
  if(!asset->layer(state_.clipLayer))state_.clipLayer=0;
  const auto *track=asset->track(state_.clipTrack);
  if(!track||track->layer!=state_.clipLayer) {
    const auto found=std::find_if(asset->tracks.begin(),asset->tracks.end(),[&](const auto &t){return t.layer==state_.clipLayer;});
    track=found==asset->tracks.end()?nullptr:&*found;state_.clipTrack=track?track->id:0;
    state_.clipComponent=0;state_.clipKey=0;state_.clipSelection.clear();
  }
  if(track&&state_.clipComponent>=track->curves.size()) {state_.clipComponent=0;state_.clipKey=0;}
  if(state_.clipKey&&!selectedKey(*asset,state_.clipTrack,state_.clipComponent,state_.clipKey))state_.clipKey=0;
  if(!state_.clipKey) {state_.clipTangentPicker=false;state_.clipModeSide=0;}
  if(!clipDrag_&&(clipPreview_.sceneRevision()!=document_.revision()||clipPreview_.clipRevision()!=asset->revision)) {
    std::string error;
    if(!clipPreview_.begin(document_,state_.clipOwner,*asset,error)||
       !seekAnimationClip(std::min(state_.clipTime,asset->duration),error)) {
      clipPreview_.cancel();state_.clipPlaying=false;state_.clipDiagnostic=error;appearanceChanged_=true;
    } else state_.clipDiagnostic.clear();
  }
  state_.clipTarget=0;state_.clipPoseCount=0;
  if(!document_.exists(state_.clipTargetBranch)||(state_.clipTargetBranch!=state_.clipOwner&&!document_.isDescendantOf(state_.clipTargetBranch,state_.clipOwner))) {state_.clipTargetBranch=state_.clipOwner;state_.clipTargetPage=0;state_.clipAuthoringPicker=state_.clipAuthoringPicker?1:0;}
  if(!track)return;
  runtime::SourceAnimations bindings;bindings.source=asset->source.valid()?asset->source:asset->guid;
  for(const auto &binding:asset->bindings) {bindings.nodes.push_back(binding.sourceNode);bindings.nodeNames.push_back(binding.name);bindings.nodePaths.push_back(binding.path);}
  std::vector<runtime::ObjectId> targets;runtime::resolveAnimationTargets(document_,state_.clipOwner,bindings,targets);
  for(usize i=0;i<asset->bindings.size();++i)if(asset->bindings[i].id==track->binding&&i<targets.size())state_.clipTarget=targets[i];
  // Fields edit the selected layer, never the already-composed viewport pose.
  if(state_.clipTarget) {
    state_.clipPoseCount=track->path==resources::AnimationPath::Weights?track->weightCount:3;
    if(track->path!=resources::AnimationPath::Rotation||track->rotationMode==resources::AnimationRotationMode::Euler) {
      for(u32 c=0;c<state_.clipPoseCount;++c) {
        resources::AnimationCurveSample v;if(resources::sampleValidatedAnimationCurve(track->curves[c],state_.clipTime,v))
          state_.clipPoseValues[c]=static_cast<float>(v.value)*(track->path==resources::AnimationPath::Weights?100.f:1.f);
      }
    } else {
      resources::AnimationChannel channel;channel.path=track->path;channel.rotationMode=track->rotationMode;channel.curves=track->curves;
      float q[4],matrix[16];const float zero[]{0,0,0},one[]{1,1,1},identity[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};runtime::Transform pose;
      if(resources::sampleValidatedAnimationChannel(channel,state_.clipTime,q)) {
        resources::composeTransform(zero,q,one,matrix);
        if(runtime::localTransformForWorld(matrix,identity,pose))std::copy_n(pose.rotationDegrees,3,state_.clipPoseValues);
      }
    }
  }
}
bool EditorSession::applyAnimationClipName(std::string_view value) {
  const auto *asset=animationClipAsset(clipNumberGuid_);std::string error;
  if(!state_.clipOpen||!asset||asset->guid!=state_.clipGuid||asset->revision!=clipNumberRevision_) {state_.clipDiagnostic="Clipe mudou; reabra o nome";return false;}
  const bool ok=editAnimationClipAsset(asset->guid,asset->revision,[&](auto &candidate){candidate.name=std::string(value);return true;},error);
  state_.clipDiagnostic=error;if(ok)refreshAnimationClip();return ok;
}
bool EditorSession::applyAnimationClipLayerName(std::string_view value) {
  const auto *asset=animationClipAsset(clipNumberGuid_);std::string error;
  if(!state_.clipOpen||!asset||asset->guid!=state_.clipGuid||asset->revision!=clipNumberRevision_||state_.clipLayer!=clipNumberLayer_) {state_.clipDiagnostic="Camada mudou; reabra o nome";return false;}
  const bool ok=editAnimationClipAsset(asset->guid,asset->revision,[&](auto &candidate){auto *l=candidate.layer(clipNumberLayer_);if(!l)return false;l->name=value;return true;},error);
  state_.clipDiagnostic=error;if(ok)refreshAnimationClip();return ok;
}
void EditorSession::frameAnimationClip() {
  const auto *asset=state_.clipAsset;if(!asset)return;
  state_.clipStart=0;state_.clipEnd=std::max(asset->duration,1.f/asset->displayRate);
  const auto *track=asset->track(state_.clipTrack);if(!track||state_.clipComponent>=track->curves.size())return;
  const auto &curve=track->curves[state_.clipComponent];double minimum=curve.keys.front().value,maximum=minimum;
  // Include the evaluated overshoot; fitting just key values hides free handles.
  for(u32 i=0;i<=512;++i) {resources::AnimationCurveSample sample;
    if(resources::sampleValidatedAnimationCurve(curve,state_.clipEnd*i/512,sample)) {minimum=std::min(minimum,sample.value);maximum=std::max(maximum,sample.value);}
  }
  if(state_.clipCurves)if(const auto *key=selectedKey(*asset,state_.clipTrack,state_.clipComponent,state_.clipKey)) {
    const usize index=static_cast<usize>(key-curve.keys.data());
    for(bool incoming:{true,false}) {
      if((incoming&&!index)||(!incoming&&index+1==curve.keys.size()))continue;
      const double span=incoming?key->time-curve.keys[index-1].time:curve.keys[index+1].time-key->time;
      const double weight=(incoming?key->weightedIn:key->weightedOut)?(incoming?key->inWeight:key->outWeight):1.0/3;
      const double handle=key->value+(incoming?-1:1)*span*weight*resources::animationCurveSlope(curve,index,incoming);
      if(std::isfinite(handle)) {minimum=std::min(minimum,handle);maximum=std::max(maximum,handle);}
    }
  }
  const double margin=std::max(.1,(maximum-minimum)*.12);
  state_.clipMinimum=static_cast<float>(minimum-margin);state_.clipMaximum=static_cast<float>(maximum+margin);
  if(!std::isfinite(state_.clipMinimum)||!std::isfinite(state_.clipMaximum)||state_.clipMaximum<=state_.clipMinimum) {
    state_.clipMinimum=-1;state_.clipMaximum=1;
  }
}
void EditorSession::beginAnimationClipNumber(u32 code,double value) {
  const auto *asset=animationClipAsset(state_.clipGuid);if(!asset||clipDrag_)return;
  state_.clipPlaying=false;clipNumberGuid_=asset->guid;clipNumberRevision_=asset->revision;
  clipNumberLayer_=state_.clipLayer;clipNumberTrack_=state_.clipTrack;clipNumberComponent_=state_.clipComponent;clipNumberKey_=state_.clipKey;
  clipNumberSceneRevision_=document_.revision();clipNumberTime_=state_.clipTime;
  clipNumberSelection_=state_.clipSelection;
  beginAnimatorNumber(code,value);
  state_.numericEntity=state_.clipOwner;
}
bool EditorSession::applyAnimationClipNumber(u32 code,double value) {
  if(!std::isfinite(value)||std::abs(value)>std::numeric_limits<float>::max())return false;
  const auto *asset=animationClipAsset(clipNumberGuid_);std::string error;
  if(!state_.clipOpen||state_.clipGuid!=clipNumberGuid_||!asset||asset->revision!=clipNumberRevision_) {state_.status="Clipe mudou; reabra o campo";return false;}
  if(code==w::Time) {const bool ok=seekAnimationClip(static_cast<float>(value),error);state_.clipDiagnostic=error;return ok;}
  if(code==w::BakeRate||code==w::BakeTolerance||(code>=w::BakeSeedX&&code<=w::BakeSeedZ)) {
    if(!state_.clipBakeShown||state_.clipTrack!=clipNumberTrack_) {state_.status="Trilha mudou; reabra o campo";return false;}
    if(code>=w::BakeSeedX&&code<=w::BakeSeedZ) {
      if(std::abs(value)>1e7) {state_.status="Referência XYZ fora do limite";return false;}
      state_.clipBakeSettings.eulerReference[code-w::BakeSeedX]=static_cast<float>(value);
    } else if(code==w::BakeRate) {
      if(value<1||value>240||std::floor(value)!=value) {state_.status="Taxa deve ser inteira, entre 1 e 240";return false;}
      state_.clipBakeSettings.sampleRate=static_cast<u32>(value);
    } else {
      if(value<=0||value>1000000) {state_.status="Tolerância deve ser positiva e finita";return false;}
      state_.clipBakeSettings.tolerance=value;
    }
    state_.clipBakeHasReport=false;state_.clipDiagnostic.clear();return true;
  }
  const bool ok=editAnimationClipAsset(asset->guid,asset->revision,[&](auto &candidate) {
    if(code==w::LayerWeight||code==w::LayerReferenceTime) {
      auto *l=candidate.layer(clipNumberLayer_);
      if(!l||state_.clipLayer!=clipNumberLayer_) {error="Camada mudou; reabra o campo";return false;}
      if(code==w::LayerWeight)l->weight=static_cast<float>(value);else l->referenceTime=static_cast<float>(value);
      return true;
    }
    if(code==w::SelectionOffset||code==w::SelectionScale) {
      if(state_.clipSelection!=clipNumberSelection_||state_.clipTime!=clipNumberTime_) {error="Seleção ou pivot mudou; reabra o campo";return false;}
      return resources::transformAnimationKeyTimes(candidate,clipNumberSelection_,clipNumberTime_,code==w::SelectionScale?value:1,code==w::SelectionOffset?value:0,error);
    }
    if(code>=w::PoseValue&&code<w::PoseValue+resources::MaximumMorphTargets) {
      if(document_.revision()!=clipNumberSceneRevision_||state_.clipTime!=clipNumberTime_||state_.numericEntity!=state_.clipOwner||state_.clipTrack!=clipNumberTrack_) {error="Pose mudou; reabra o campo";return false;}
      const auto *track=candidate.track(clipNumberTrack_);const u32 component=code-w::PoseValue;
      if(!track||component>=state_.clipPoseCount||state_.clipTarget==0)return false;
      std::vector<float> values(state_.clipPoseValues,state_.clipPoseValues+state_.clipPoseCount);values[component]=static_cast<float>(value);
      if(track->path==resources::AnimationPath::Weights)for(auto &v:values)v*=.01f;
      else if(track->path==resources::AnimationPath::Rotation&&track->rotationMode!=resources::AnimationRotationMode::Euler) {
        runtime::Transform pose;std::copy_n(values.begin(),3,pose.rotationDegrees);float q[4];runtime::transformRotationQuaternion(pose,q);values.assign(q,q+4);
      }
      return candidate.putPose(clipNumberTrack_,clipNumberTime_,values,error);
    }
    if(code==w::Duration) {candidate.duration=static_cast<float>(value);return true;}
    const auto *current=selectedKey(candidate,clipNumberTrack_,clipNumberComponent_,clipNumberKey_);if(!current)return false;
    auto key=*current;
    if(code==w::KeyTime)key.time=static_cast<float>(value);else if(code==w::KeyValue)key.value=static_cast<float>(value);
    else if(code==w::SlopeIn||code==w::SlopeOut) {
      const bool incoming=code==w::SlopeIn;
      (incoming?key.incoming:key.outgoing)=resources::AnimationTangentMode::Free;
      (incoming?key.inSlope:key.outSlope)=static_cast<float>(value);
      if(!key.broken) {key.incoming=key.outgoing=resources::AnimationTangentMode::Free;key.inSlope=key.outSlope=static_cast<float>(value);}
    } else if(code==w::WeightIn||code==w::WeightOut) {
      if(value<0||value>1) {error="Peso do handle deve ficar entre 0 e 1";return false;}
      (code==w::WeightIn?key.inWeight:key.outWeight)=static_cast<float>(value);
    } else return false;
    u64 result=0;return candidate.putKey(clipNumberTrack_,clipNumberComponent_,key,result,error);
  },error);
  state_.clipDiagnostic=error;if(ok) {refreshAnimationClip();frameAnimationClip();}return ok;
}
bool EditorSession::handleAnimationClip(const ui::UiPointerEvent &event,const ui::UiPointerRouting &routing) {
  using ui::UiPointerPhase;const auto *asset=state_.clipAsset;
  if(!asset)return true;
  const auto &canvas=layout_.clipCanvas;
  const auto timeAt=[&](float x){const float t=state_.clipStart+(x-canvas.x)/std::max(1.f,canvas.width)*(state_.clipEnd-state_.clipStart);
    return std::clamp(std::round(t*asset->displayRate)/asset->displayRate,0.f,asset->duration);};
  const auto xAt=[&](float t){return canvas.x+(t-state_.clipStart)/(state_.clipEnd-state_.clipStart)*canvas.width;};
  const auto yAt=[&](float v){return canvas.bottom()-(v-state_.clipMinimum)/(state_.clipMaximum-state_.clipMinimum)*canvas.height;};
  if(clipPointer_&&event.pointerId+1!=clipPointer_)return true;
  const u32 widgetCode=animator_widget::code(routing.widgetId);
  if(routing.widgetId==w::id(w::Canvas)||widgetCode==w::HandleIn||widgetCode==w::HandleOut||clipPointer_) {
    if(event.phase==UiPointerPhase::Down) {
      clipPointer_=event.pointerId+1;state_.clipPlaying=false;clipPress_=event.position;
      clipPressSelection_=state_.clipSelection;state_.clipSelecting=false;
      clipHandle_=widgetCode==w::HandleIn?1:widgetCode==w::HandleOut?2:0;
      if(clipHandle_)return true;
      if(!state_.clipCurves) {
        const auto all=rows(*asset,state_.clipLayer);const usize row=state_.clipRow+static_cast<usize>(std::max(0.f,event.position.y-canvas.y)/30);
        if(row<all.size()&&row<state_.clipRow+layout_.clipVisibleRows) {state_.clipTrack=all[row].track;state_.clipComponent=all[row].component;}
      }
      const auto *track=asset->track(state_.clipTrack);float nearest=20;state_.clipKey=0;
      if(track)for(const auto &key:track->curves[state_.clipComponent].keys) {
        const float dx=xAt(key.time)-event.position.x;
        const float dy=state_.clipCurves?yAt(key.value)-event.position.y:0;
        const float distance=std::hypot(dx,dy);
        if(distance<nearest) {nearest=distance;state_.clipKey=key.id;clipPressTime_=key.time;clipPressValue_=key.value;}
      }
      if(state_.clipKey) {
        const resources::AnimationKeyAddress hit{state_.clipTrack,state_.clipComponent,state_.clipKey};
        const bool already=std::find(state_.clipSelection.begin(),state_.clipSelection.end(),hit)!=state_.clipSelection.end();
        if(!state_.clipSelectionMode||(!already&&!state_.clipSelectionAdd))state_.clipSelection.clear();
        if(!already||state_.clipSelection.empty()) {auto keys=state_.clipSelection;keys.push_back(hit);selectKeys(state_,*asset,keys);}
        clipPressSelection_=state_.clipSelection;
      } else if(state_.clipSelectionMode) {
        state_.clipSelecting=true;state_.clipSelectionBox={event.position.x,event.position.y,0,0};
      } else state_.clipSelection.clear();
      std::string error;seekAnimationClip(state_.clipKey?clipPressTime_:timeAt(event.position.x),error);state_.clipDiagnostic=error;
    } else if(event.phase==UiPointerPhase::Move&&routing.dragging) {
      std::string error;
      if(state_.clipSelecting) {
        state_.clipSelectionBox={std::min(clipPress_.x,event.position.x),std::min(clipPress_.y,event.position.y),std::abs(event.position.x-clipPress_.x),std::abs(event.position.y-clipPress_.y)};
        auto keys=state_.clipSelectionAdd?clipPressSelection_:std::vector<resources::AnimationKeyAddress>{};
        const auto all=rows(*asset,state_.clipLayer);
        for(usize row=0;row<all.size();++row) {
          const auto &entry=all[row];
          if(state_.clipCurves&&(entry.track!=state_.clipTrack||entry.component!=state_.clipComponent))continue;
          if(!state_.clipCurves&&(row<state_.clipRow||row>=state_.clipRow+layout_.clipVisibleRows))continue;
          const auto *track=asset->track(entry.track);
          for(const auto &key:track->curves[entry.component].keys) {
            const ui::UiPoint point{xAt(key.time),state_.clipCurves?yAt(key.value):canvas.y+(row-state_.clipRow)*30+13};
            if(canvas.contains(point)&&state_.clipSelectionBox.contains(point))keys.push_back({entry.track,entry.component,key.id});
          }
        }
        selectKeys(state_,*asset,keys);appearanceChanged_=true;return true;
      }
      if(state_.clipKey) {
        const auto *live=animationClipAsset(state_.clipGuid);if(!live)return true;
        const bool multi=state_.clipSelectionMode&&state_.clipSelection.size()>1&&!clipHandle_;
        auto candidate=multi?*live:clipDrag_?*clipDrag_:*live;
        const auto *found=selectedKey(candidate,state_.clipTrack,state_.clipComponent,state_.clipKey);if(!found)return true;
        auto key=*found;
        if(multi) {
          const float moved=timeAt(xAt(clipPressTime_)+event.position.x-clipPress_.x);
          if(resources::transformAnimationKeyTimes(candidate,clipPressSelection_,0,1,moved-clipPressTime_,error)&&
             clipPreview_.begin(document_,state_.clipOwner,candidate,error)&&clipPreview_.seek(moved,error)) {
            clipDrag_=std::move(candidate);state_.clipAsset=&*clipDrag_;state_.clipTime=moved;appearanceChanged_=true;
          }
          state_.clipDiagnostic=error;return true;
        }
        if(clipHandle_) {
          const auto *track=candidate.track(state_.clipTrack);const auto &curve=track->curves[state_.clipComponent];
          const auto index=static_cast<usize>(found-curve.keys.data());const bool incoming=clipHandle_==1;
          if((incoming&&!index)||(!incoming&&index+1==curve.keys.size()))return true;
          const double span=incoming?key.time-curve.keys[index-1].time:curve.keys[index+1].time-key.time;
          const double handleTime=state_.clipStart+(event.position.x-canvas.x)/canvas.width*(state_.clipEnd-state_.clipStart);
          const double weight=std::clamp((incoming?key.time-handleTime:handleTime-key.time)/span,0.0,1.0);
          const double dx=(incoming?-1:1)*weight*span;
          const double handleValue=state_.clipMaximum-(event.position.y-canvas.y)/canvas.height*(state_.clipMaximum-state_.clipMinimum);
          if(std::abs(dx)<1e-12&&std::abs(handleValue-key.value)>1e-6) {state_.clipDiagnostic="Handle vertical não tem inclinação finita";return true;}
          const double slope=dx?(handleValue-key.value)/dx:0;
          if(!std::isfinite(slope)||std::abs(slope)>std::numeric_limits<float>::max())return true;
          (incoming?key.inSlope:key.outSlope)=static_cast<float>(slope);(incoming?key.inWeight:key.outWeight)=static_cast<float>(weight);
          (incoming?key.weightedIn:key.weightedOut)=true;(incoming?key.incoming:key.outgoing)=resources::AnimationTangentMode::Free;
          if(!key.broken) {key.inSlope=key.outSlope=static_cast<float>(slope);key.incoming=key.outgoing=resources::AnimationTangentMode::Free;}
        } else key.time=timeAt(xAt(clipPressTime_)+event.position.x-clipPress_.x);
        const auto *track=candidate.track(state_.clipTrack);
        const bool derived=track&&track->rotationMode==resources::AnimationRotationMode::ProgressiveQuaternion&&state_.clipComponent==4;
        if(state_.clipCurves&&!derived&&!clipHandle_)key.value=clipPressValue_-(event.position.y-clipPress_.y)/canvas.height*(state_.clipMaximum-state_.clipMinimum);
        u64 result=0;
        if(candidate.putKey(state_.clipTrack,state_.clipComponent,key,result,error)&&
           clipPreview_.begin(document_,state_.clipOwner,candidate,error)&&clipPreview_.seek(clipHandle_?state_.clipTime:key.time,error)) {
          clipDrag_=std::move(candidate);state_.clipAsset=&*clipDrag_;if(!clipHandle_)state_.clipTime=key.time;appearanceChanged_=true;
        }
      } else seekAnimationClip(timeAt(event.position.x),error);
      state_.clipDiagnostic=error;
    } else if(event.phase==UiPointerPhase::Up||event.phase==UiPointerPhase::Cancel) {
      if(state_.clipSelecting) {
        if(event.phase==UiPointerPhase::Cancel)selectKeys(state_,*asset,clipPressSelection_);
        state_.clipSelecting=false;appearanceChanged_=true;
      }
      if(clipDrag_) {
        const auto draft=*clipDrag_;clipDrag_.reset();std::string error;
        if(event.phase==UiPointerPhase::Up) {
          const auto *live=animationClipAsset(state_.clipGuid);
          if(!live||!editAnimationClipAsset(draft.guid,draft.revision,[&](auto &candidate){candidate=draft;return true;},error))state_.clipDiagnostic=error;
        }
        clipPreview_.cancel();refreshAnimationClip();appearanceChanged_=true;
      }
      clipPointer_=0;clipHandle_=0;
    }
    return true;
  }
  // Scene gestures may orbit the preview camera, but never select/move source
  // objects or activate the ordinary document gizmo underneath this surface.
  if(routing.target==ui::UiPointerTarget::Viewport) {
    return handleViewportPointer(event,routing);
  }
  if(!routing.tapped||!w::owns(routing.widgetId))return true;
  const u32 code=animator_widget::code(routing.widgetId);std::string error;
  const auto change=[&](auto operation) {
    state_.clipPlaying=false;const auto *live=animationClipAsset(state_.clipGuid);
    const bool ok=live&&editAnimationClipAsset(live->guid,live->revision,operation,error);
    state_.clipDiagnostic=error;if(ok)refreshAnimationClip();return ok;
  };
  if(code==w::Close) {closeAnimationClip();return true;}
  if(code==w::Layers) {
    state_.clipLayerPicker=false;state_.clipLayersShown=!state_.clipLayersShown;state_.clipBakeShown=false;state_.clipPlaying=false;
    state_.clipTangentPicker=state_.clipEditPicker=state_.clipPicker=state_.clipNewPicker=false;state_.clipAuthoringPicker=state_.clipModeSide=0;return true;
  }
  if(code==w::LayerClose) {state_.clipLayerPicker=false;state_.clipLayersShown=false;return true;}
  const auto *layer=asset->layer(state_.clipLayer);
  if(code==w::LayerChoose) {state_.clipLayerPicker=!state_.clipLayerPicker;state_.clipLayerPage=static_cast<u32>(layer-asset->layers.data())/4;return true;}
  if(code==w::LayerPickerClose) {state_.clipLayerPicker=false;return true;}
  if(code==w::LayerPickerPrevious) {if(state_.clipLayerPage)--state_.clipLayerPage;return true;}
  if(code==w::LayerPickerNext) {if((state_.clipLayerPage+1)*4<asset->layers.size())++state_.clipLayerPage;return true;}
  if(code>=w::LayerChoice&&code<w::LayerChoice+4) {
    const auto index=state_.clipLayerPage*4+code-w::LayerChoice;
    if(index<asset->layers.size()) {state_.clipLayer=asset->layers[index].id;state_.clipTrack=0;state_.clipSelection.clear();state_.clipKey=0;state_.clipLayerPicker=false;refreshAnimationClip();frameAnimationClip();}
    return true;
  }
  if(code==w::LayerPrevious||code==w::LayerNext) {
    const auto i=static_cast<usize>(layer-asset->layers.data());
    if(code==w::LayerPrevious&&i)state_.clipLayer=asset->layers[i-1].id;
    if(code==w::LayerNext&&i+1<asset->layers.size())state_.clipLayer=asset->layers[i+1].id;
    state_.clipTrack=0;state_.clipSelection.clear();state_.clipKey=0;refreshAnimationClip();frameAnimationClip();return true;
  }
  if(code==w::LayerName) {
    state_.clipPlaying=false;clipNumberGuid_=asset->guid;clipNumberRevision_=asset->revision;clipNumberLayer_=state_.clipLayer;
    beginAnimatorName(code,layer->name);return true;
  }
  if(code==w::LayerWeight||code==w::LayerReferenceTime) {beginAnimationClipNumber(code,code==w::LayerWeight?layer->weight:std::max(0.f,layer->referenceTime));return true;}
  if(code>=w::LayerAdd&&code<=w::LayerCopy) {
    const auto layerId=state_.clipLayer,index=static_cast<usize>(layer-asset->layers.data());u64 created=0;
    if(change([&](auto &candidate){
      auto *l=candidate.layer(layerId);if(!l)return false;
      if(code==w::LayerAdd)return candidate.addLayer("Camada "+std::to_string(candidate.layers.size()),resources::AnimationAuthorBlend::Additive,created,error);
      if(code==w::LayerDuplicate)return candidate.duplicateLayer(layerId,created,error);
      if(code==w::LayerRemove)return candidate.removeLayer(layerId,error);
      if(code==w::LayerUp||code==w::LayerDown)return candidate.moveLayer(layerId,static_cast<u32>(code==w::LayerUp?index-1:index+1),error);
      if(code==w::LayerCopy)return candidate.copyBaseToTrack(state_.clipTrack,error);
      if(code==w::LayerBlend) {l->blend=l->blend==resources::AnimationAuthorBlend::Override?resources::AnimationAuthorBlend::Additive:resources::AnimationAuthorBlend::Override;l->referenceTime=-1;}
      else if(code==w::LayerMute)l->muted=!l->muted;
      else if(code==w::LayerSolo)l->solo=!l->solo;
      else if(code==w::LayerReference)l->referenceTime=l->referenceTime<0?state_.clipTime:-1;
      else return false;
      return true;
    })) {
      if(created)state_.clipLayer=created;
      if(created||code==w::LayerRemove)state_.clipTrack=0;
      state_.clipSelection.clear();state_.clipKey=0;refreshAnimationClip();frameAnimationClip();
    }
    return true;
  }
  if(code==w::BakeOpen) {
    state_.clipPlaying=false;state_.clipLayerPicker=state_.clipLayersShown=false;state_.clipBakeShown=true;state_.clipBakeHasReport=false;
    state_.clipEditPicker=state_.clipPicker=state_.clipNewPicker=state_.clipTangentPicker=false;
    state_.clipAuthoringPicker=state_.clipModeSide=0;return true;
  }
  if(code==w::BakeClose) {state_.clipBakeShown=false;return true;}
  if(code==w::BakeRate||code==w::BakeTolerance) {
    beginAnimationClipNumber(code,code==w::BakeRate?state_.clipBakeSettings.sampleRate:state_.clipBakeSettings.tolerance);return true;
  }
  if(code==w::BakeReduction) {state_.clipBakeSettings.reduce=!state_.clipBakeSettings.reduce;state_.clipBakeHasReport=false;return true;}
  if(code==w::BakeTarget) {state_.clipBakeConsolidate=!state_.clipBakeConsolidate;state_.clipBakeHasReport=false;return true;}
  if(code==w::BakeReference) {state_.clipBakeReferenceShown=!state_.clipBakeReferenceShown;return true;}
  if(code==w::BakeSeedUse) {state_.clipBakeSettings.eulerReferenceExplicit=!state_.clipBakeSettings.eulerReferenceExplicit;state_.clipBakeHasReport=false;return true;}
  if(code>=w::BakeSeedX&&code<=w::BakeSeedZ) {beginAnimationClipNumber(code,state_.clipBakeSettings.eulerReference[code-w::BakeSeedX]);return true;}
  if(code==w::BakeMode) {
    auto &mode=state_.clipBakeSettings.rotation;mode=mode==3?0:mode==0?2:mode==2?1:3;
    state_.clipBakeReferenceShown=mode==1;state_.clipBakeHasReport=false;return true;
  }
  if(code==w::BakeApply) {
    auto settings=state_.clipBakeSettings;const auto *track=asset->track(state_.clipTrack);
    if(!track)return true;
    if(track->path!=resources::AnimationPath::Rotation&&!state_.clipBakeConsolidate)settings.rotation=3;
    resources::AnimationBakeReport report;
    state_.clipBakeHasReport=false;
    if(state_.clipBakeConsolidate) {
      resources::AssetGuid created;std::string name=asset->name;
      if(name.size()>220) {usize n=220;while(n&&(static_cast<unsigned char>(name[n])&0xc0)==0x80)--n;name.resize(n);}
      name+=" · consolidado";const auto owner=state_.clipOwner;
      if(createConsolidatedAnimationClip(*asset,name,settings,created,report,error)&&openAnimationClip(created,owner,error)) {
        state_.clipBakeShown=state_.clipBakeHasReport=true;state_.clipBakeConsolidate=state_.clipBakeReferenceShown=false;
        state_.clipBakeReportConsolidated=true;state_.clipBakeReport=report;state_.clipBakeSettings=settings;
        state_.clipBakeRevision=state_.clipAsset->revision;state_.clipBakeTrack=state_.clipTrack;frameAnimationClip();
      }
      state_.clipDiagnostic=error;return true;
    }
    if(change([&](auto &candidate){return resources::bakeAnimationClipTrack(candidate,state_.clipTrack,settings,report,error);})) {
      state_.clipBakeReport=report;state_.clipBakeHasReport=true;state_.clipBakeReportConsolidated=false;state_.clipSelection.clear();state_.clipKey=0;frameAnimationClip();
      state_.clipBakeRevision=state_.clipAsset->revision;state_.clipBakeTrack=state_.clipTrack;
    }
    return true;
  }
  if(code==w::Edits) {
    state_.clipBakeShown=false;state_.clipLayerPicker=state_.clipLayersShown=false;
    state_.clipEditPicker=!state_.clipEditPicker;state_.clipPicker=state_.clipNewPicker=state_.clipTangentPicker=false;
    state_.clipAuthoringPicker=state_.clipModeSide=0;return true;
  }
  if(code==w::EditsClose) {state_.clipEditPicker=false;return true;}
  if(code==w::CopyKeys||code==w::CutKeys) {
    auto copied=std::make_shared<resources::AnimationKeyClipboard>();
    auto selection=state_.clipSelection;
    if(selection.empty()&&state_.clipKey)selection.push_back({state_.clipTrack,state_.clipComponent,state_.clipKey});
    if(copied->copy(*asset,selection,error)) {
      if(code==w::CopyKeys||change([&](auto &candidate){return resources::eraseAnimationKeySelection(candidate,selection,error);})) {
        state_.clipClipboard=std::move(copied);state_.clipEditPicker=false;
        if(code==w::CutKeys) {state_.clipSelection.clear();state_.clipKey=0;}
      }
    }
    state_.clipDiagnostic=error;return true;
  }
  if(code==w::PasteReplace||code==w::PasteInsertTracks||code==w::PasteInsertAll) {
    const auto clipboard=state_.clipClipboard;std::vector<resources::AnimationKeyAddress> selection;
    const auto mode=code==w::PasteReplace?resources::AnimationPasteMode::Replace:code==w::PasteInsertTracks?resources::AnimationPasteMode::InsertTracks:resources::AnimationPasteMode::InsertAllTracks;
    if(clipboard&&change([&](auto &candidate){return clipboard->paste(candidate,state_.clipTime,mode,selection,error);})) {
      selectKeys(state_,*state_.clipAsset,selection);state_.clipSelectionMode=true;state_.clipEditPicker=false;
      state_.clipLayer=state_.clipAsset->track(selection.front().track)->layer;state_.clipTrack=selection.front().track;state_.clipComponent=selection.front().component;state_.clipKey=selection.front().key;
      const auto all=rows(*state_.clipAsset,state_.clipLayer);for(usize i=0;i<all.size();++i)if(all[i].track==state_.clipTrack&&all[i].component==state_.clipComponent) {state_.clipRow=static_cast<u32>(i);break;}
      frameAnimationClip();
    }
    return true;
  }
  if(code==w::Selection) {state_.clipSelectionMode=!state_.clipSelectionMode;state_.clipTangentPicker=false;state_.clipModeSide=0;return true;}
  if(code==w::SelectionAdd) {state_.clipSelectionAdd=!state_.clipSelectionAdd;return true;}
  if(code==w::SelectionClear) {state_.clipSelection.clear();state_.clipKey=0;return true;}
  if(code==w::SelectionOffset||code==w::SelectionScale) {beginAnimationClipNumber(code,code==w::SelectionScale?1:0);return true;}
  if(code==w::Name) {
    state_.clipPlaying=false;clipNumberGuid_=asset->guid;clipNumberRevision_=asset->revision;beginAnimatorName(code,asset->name);return true;
  }
  if(code==w::Play)state_.clipPlaying=!state_.clipPlaying&&clipPreview_.active();
  else if(code==w::Loop)state_.clipLoop=!state_.clipLoop;
  else if(code==w::Choose) {
    state_.clipBakeShown=false;state_.clipLayerPicker=state_.clipLayersShown=false;
    state_.clipEditPicker=false;
    state_.clipPicker=!state_.clipPicker;state_.clipNewPicker=false;state_.clipTangentPicker=false;state_.clipModeSide=0;state_.clipAuthoringPicker=0;
    state_.clipImportedPicker=false;state_.clipPage=0;state_.clipSourceEntries.clear();
    for(const auto &source:mapScene_.animationSources())if(source)for(usize i=0;i<source->clips.size()&&i<source->clipIds.size();++i) {
      auto name=resources::animationClipDisplayName(source->clips[i],static_cast<u32>(i));
      if(const auto *record=assets_.find(source->source))name+=" · "+record->path;
      state_.clipSourceEntries.push_back({source->clipIds[i],std::move(name),source->clips[i].duration});
    }
  } else if(code==w::OwnedCatalog||code==w::ImportedCatalog) {state_.clipImportedPicker=code==w::ImportedCatalog;state_.clipPage=0;}
  else if(code==w::Targets) {
    state_.clipBakeShown=false;state_.clipLayerPicker=state_.clipLayersShown=false;
    state_.clipEditPicker=false;
    state_.clipAuthoringPicker=state_.clipAuthoringPicker?0:1;state_.clipTargetBranch=state_.clipOwner;state_.clipTargetPage=0;
    state_.clipPicker=state_.clipNewPicker=state_.clipTangentPicker=false;state_.clipModeSide=0;
  } else if(code==w::TargetClose)state_.clipAuthoringPicker=0;
  else if(code==w::TargetUp) {
    if(state_.clipAuthoringPicker==2)state_.clipAuthoringPicker=1;
    else if(const auto *branch=document_.find(state_.clipTargetBranch);branch&&branch->id!=state_.clipOwner)state_.clipTargetBranch=branch->parent;
    state_.clipTargetPage=0;
  } else if(code==w::TargetPrevious) {if(state_.clipTargetPage)--state_.clipTargetPage;}
  else if(code==w::TargetNext) {
    if((state_.clipTargetPage+1)*layout_.clipTargetRows<document_.childrenOf(state_.clipTargetBranch).size())++state_.clipTargetPage;
  } else if(code==w::TargetUse) {state_.clipTargetNode=state_.clipTargetBranch;state_.clipAuthoringPicker=2;}
  else if(code>=w::TargetChoice&&code<w::TargetChoice+64) {
    const auto children=document_.childrenOf(state_.clipTargetBranch);const usize index=state_.clipTargetPage*layout_.clipTargetRows+code-w::TargetChoice;
    if(index<children.size()) {state_.clipTargetNode=children[index];state_.clipAuthoringPicker=2;}
  } else if(code>=w::TargetEnter&&code<w::TargetEnter+64) {
    const auto children=document_.childrenOf(state_.clipTargetBranch);const usize index=state_.clipTargetPage*layout_.clipTargetRows+code-w::TargetEnter;
    if(index<children.size()&&!document_.childrenOf(children[index]).empty()) {state_.clipTargetBranch=children[index];state_.clipTargetPage=0;}
  } else if(code>=w::PropertyChoice&&code<w::PropertyChoice+6) {
    const u32 property=code-w::PropertyChoice;
    const auto path=property==0?resources::AnimationPath::Translation:property==1?resources::AnimationPath::Scale:property==5?resources::AnimationPath::Weights:resources::AnimationPath::Rotation;
    const auto mode=property==3?resources::AnimationRotationMode::Euler:property==4?resources::AnimationRotationMode::ProgressiveQuaternion:resources::AnimationRotationMode::Quaternion;
    const auto *live=animationClipAsset(state_.clipGuid);u64 id=0;
    runtime::SourceAnimations bindings;bindings.source=live&&live->source.valid()?live->source:state_.clipGuid;
    if(live)for(const auto &binding:live->bindings) {bindings.nodes.push_back(binding.sourceNode);bindings.nodePaths.push_back(binding.path);bindings.nodeNames.push_back(binding.name);}
    std::vector<runtime::ObjectId> targets;runtime::resolveAnimationTargets(document_,state_.clipOwner,bindings,targets);
    if(live)for(const auto &track:live->tracks)if(track.layer==state_.clipLayer&&track.path==path)for(usize i=0;i<live->bindings.size();++i)
      if(live->bindings[i].id==track.binding&&i<targets.size()&&targets[i]==state_.clipTargetNode) {
        if(track.rotationMode!=mode) {state_.clipDiagnostic="Rotação já usa outro modo; a conversão exige uma operação explícita";return true;}
        id=track.id;
      }
    if(id||(live&&addAnimationClipTrack(live->guid,live->revision,state_.clipOwner,state_.clipTargetNode,path,mode,id,error))) {
      state_.clipTrack=id;state_.clipComponent=0;state_.clipKey=0;state_.clipAuthoringPicker=0;refreshAnimationClip();
      const auto all=rows(*state_.clipAsset,state_.clipLayer);for(usize i=0;i<all.size();++i)if(all[i].track==id) {state_.clipRow=static_cast<u32>(i);break;}frameAnimationClip();
    }
    state_.clipDiagnostic=error;
  } else if(code==w::Pose) {state_.clipPoseShown=!state_.clipPoseShown;state_.clipAuthoringPicker=0;}
  else if(code==w::PosePrevious) {if(state_.clipPosePage)--state_.clipPosePage;}
  else if(code==w::PoseNext) {if((state_.clipPosePage+1)*3<state_.clipPoseCount)++state_.clipPosePage;}
  else if(code>=w::PoseValue&&code<w::PoseValue+resources::MaximumMorphTargets) {
    const u32 component=code-w::PoseValue;if(component<state_.clipPoseCount)beginAnimationClipNumber(code,state_.clipPoseValues[component]);
  } else if(code==w::RemoveTrack) {
    if(change([&](auto &candidate){return candidate.removeTrack(state_.clipTrack,error);})) {state_.clipKey=0;frameAnimationClip();}
  }
  else if(code==w::TangentClose) {state_.clipTangentPicker=false;state_.clipModeSide=0;}
  else if(code==w::ModeIn||code==w::ModeOut) {const u32 side=code==w::ModeIn?1:2;state_.clipModeSide=state_.clipModeSide==side?0:side;}
  else if(code==w::Curves||code==w::Keys)state_.clipCurves=code==w::Curves;
  else if(code==w::Expand)state_.clipExpanded=!state_.clipExpanded;
  else if(code==w::Frame)frameAnimationClip();
  else if(code==w::ZoomIn||code==w::ZoomOut) {
    const float span=std::clamp((state_.clipEnd-state_.clipStart)*(code==w::ZoomIn?.5f:2.f),1.f/asset->displayRate,std::max(asset->duration,1.f/asset->displayRate));
    state_.clipStart=std::clamp(state_.clipTime-span*.5f,0.f,std::max(0.f,asset->duration-span));state_.clipEnd=state_.clipStart+span;
  } else if(code==w::RowsPrevious)state_.clipRow=state_.clipRow>=layout_.clipVisibleRows?state_.clipRow-layout_.clipVisibleRows:0;
  else if(code==w::RowsNext) {const auto count=rows(*asset,state_.clipLayer).size();if(state_.clipRow+layout_.clipVisibleRows<count)state_.clipRow+=layout_.clipVisibleRows;}
  else if(code==w::PickerPrevious) {if(state_.clipPage) --state_.clipPage;}
  else if(code==w::PickerNext) {if((state_.clipPage+1)*layout_.clipPickerRows<(state_.clipImportedPicker?state_.clipSourceEntries.size():animationClipAssets_.size())) ++state_.clipPage;}
  else if(code>=w::Row&&code<w::Row+64) {
    const auto all=rows(*asset,state_.clipLayer);const usize i=state_.clipRow+code-w::Row;
    if(i<all.size()) {state_.clipTrack=all[i].track;state_.clipComponent=all[i].component;state_.clipKey=0;state_.clipPosePage=state_.clipComponent/3;frameAnimationClip();}
  } else if(code>=w::Choice&&code<w::Choice+64) {
    const usize i=state_.clipPage*layout_.clipPickerRows+code-w::Choice;
    if(state_.clipImportedPicker) {
      if(i<state_.clipSourceEntries.size()) {
        const auto sourceGuid=state_.clipSourceEntries[i].guid;resources::AssetGuid extracted;
        if(extractAnimationClip(sourceGuid,state_.clipOwner,extracted,error))openAnimationClip(extracted,state_.clipOwner,error);
        state_.clipDiagnostic=error;
      }
    } else if(i<animationClipAssets_.size()) {const auto guid=animationClipAssets_[i].guid;openAnimationClip(guid,state_.clipOwner,error);state_.clipDiagnostic=error;}
  } else if(code==w::New) {state_.clipBakeShown=false;state_.clipNewPicker=!state_.clipNewPicker;state_.clipPicker=false;state_.clipTangentPicker=false;state_.clipModeSide=0;state_.clipAuthoringPicker=0;
  } else if(code==w::NewQuaternion||code==w::NewEuler||code==w::NewProgressive) {
    const auto mode=code==w::NewEuler?resources::AnimationRotationMode::Euler:code==w::NewProgressive?resources::AnimationRotationMode::ProgressiveQuaternion:resources::AnimationRotationMode::Quaternion;
    resources::AssetGuid guid;if(createAnimationClip(state_.clipOwner,2,guid,error,mode))openAnimationClip(guid,state_.clipOwner,error);state_.clipDiagnostic=error;
  } else if(code==w::Time||code==w::Duration)beginAnimationClipNumber(code,code==w::Time?state_.clipTime:asset->duration);
  else if(code==w::PreviousFrame||code==w::NextFrame) {
    state_.clipPlaying=false;seekAnimationClip(std::clamp(state_.clipTime+(code==w::PreviousFrame?-1.f:1.f)/asset->displayRate,0.f,asset->duration),error);state_.clipDiagnostic=error;
  } else if(code==w::Undo||code==w::Redo) {
    state_.clipPlaying=false;const bool ok=code==w::Undo?history_.undo(document_):history_.redo(document_);
    if(!ok)state_.clipDiagnostic="Historico indisponível ou recurso modificado";else {state_.clipDiagnostic.clear();refreshAnimationClip();}
  } else if(code==w::AddKey) {
    u64 id=0;change([&](auto &candidate){
      return candidate.splitKey(state_.clipTrack,state_.clipComponent,state_.clipTime,id,error);
    });state_.clipKey=id;if(id) {const resources::AnimationKeyAddress address{state_.clipTrack,state_.clipComponent,id};selectKeys(state_,*state_.clipAsset,std::span(&address,1));}
  } else if(code==w::DeleteKey) {
    const auto selection=state_.clipSelection;
    if(change([&](auto &candidate){return state_.clipSelectionMode&&!selection.empty()?resources::eraseAnimationKeySelection(candidate,selection,error):candidate.eraseKey(state_.clipTrack,state_.clipComponent,state_.clipKey,error);})) {state_.clipKey=0;state_.clipSelection.clear();}
  } else if(const auto *key=selectedKey(*asset,state_.clipTrack,state_.clipComponent,state_.clipKey)) {
    if(code==w::KeyTime||code==w::KeyValue)beginAnimationClipNumber(code,code==w::KeyTime?key->time:key->value);
    else if(code==w::Tangent) {state_.clipBakeShown=false;state_.clipTangentPicker=!state_.clipTangentPicker;state_.clipPicker=false;state_.clipNewPicker=false;state_.clipModeSide=0;}
    else if(code==w::SlopeIn||code==w::SlopeOut||code==w::WeightIn||code==w::WeightOut)
      beginAnimationClipNumber(code,code==w::SlopeIn?key->inSlope:code==w::SlopeOut?key->outSlope:code==w::WeightIn?key->inWeight:key->outWeight);
    else if(code==w::TangentLink||(code>=w::SelectMode&&code<w::SelectMode+7)||code==w::WeightedIn||code==w::WeightedOut)change([&](auto &candidate){
      auto next=*key;
      if(code==w::TangentLink) {next.broken=!next.broken;if(!next.broken) {next.incoming=next.outgoing;next.inSlope=next.outSlope;}}
      else if(code==w::WeightedIn)next.weightedIn=!next.weightedIn;
      else if(code==w::WeightedOut)next.weightedOut=!next.weightedOut;
      else {
        if(!state_.clipModeSide)return false;
        const bool incoming=state_.clipModeSide==1;
        auto &mode=incoming?next.incoming:next.outgoing;mode=static_cast<resources::AnimationTangentMode>(code-w::SelectMode);
        if(mode==resources::AnimationTangentMode::Free) {
          const auto *track=asset->track(state_.clipTrack);const auto &curve=track->curves[state_.clipComponent];
          const usize index=static_cast<usize>(key-curve.keys.data());
          (incoming?next.inSlope:next.outSlope)=static_cast<float>(resources::animationCurveSlope(curve,index,incoming));
          if(!next.broken)next.inSlope=next.outSlope=incoming?next.inSlope:next.outSlope;
        }
        if(!next.broken)next.incoming=next.outgoing=mode;
        state_.clipModeSide=0;
      }
      u64 id=0;return candidate.putKey(state_.clipTrack,state_.clipComponent,next,id,error);
    });
  }
  return true;
}
} // namespace ae::editor
