#include "editor/editor_screen.h"
#include "editor/editor_session.h"
#include "scene/ik_constraints.h"
#include "scene/animation.h"
#include <cctype>
#include "editor/editor_studio_state.h"
#include "editor/editor_studio_tools.h"
#include "runtime/transform_math.h"
#include "resources/animation_binding_path.h"
#include "resources/animation_clip_arrange.h"
#include "editor/editor_animation_curve_view.h"
#include "scene/skinned_mesh.h"
#include "scene/import_link.h"
#include "renderer/normal_matrix.h"
#include "core/rotation_math.h"
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
// Filtro opcional por binding: a timeline do estudio pode mostrar so o osso selecionado.
std::vector<Row> rows(const Asset &asset,u64 layer,const std::vector<u64> *rowFilter=nullptr) {
  std::vector<Row> result;
  for(const auto &t:asset.tracks)if(t.layer==layer) {
    if(rowFilter&&!rowFilter->empty()&&std::find(rowFilter->begin(),rowFilter->end(),t.binding)==rowFilter->end())continue;
    for(u32 c=0;c<t.components();++c)result.push_back({t.id,c});
  }
  return result;
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
  if(!animationPoseTargetPath(document,owner,target,binding.path,error))return false;
  binding.name=document.find(target)->name;return true;
}
}
void EditorSession::refreshClipRowFilter() {
  state_.clipFilterBindings.clear();
  if(!state_.clipFilterSelected||!state_.clipAsset)return;
  const auto target=state_.clipPreviewTarget?state_.clipPreviewTarget:state_.clipTarget;
  for(const auto &binding:state_.clipAsset->bindings) {
    runtime::ObjectId resolved=0;std::string ignored;
    if(resolveAnimationPoseTargetPath(document_,state_.clipOwner,binding.path,resolved,ignored)&&resolved==target)state_.clipFilterBindings.push_back(binding.id);
  }
  // Osso sem canal ainda: um binding impossivel deixa a lista vazia, nao "todos".
  if(state_.clipFilterBindings.empty())state_.clipFilterBindings.push_back(~u64(0));
  state_.clipRow=0;
}
void EditorSession::refreshAnimationGhost() {
  state_.clipGhostBefore.clear();state_.clipGhostAfter.clear();
  const auto *live=animationClipAsset(state_.clipGuid);
  if(!state_.clipGhost||!live||!clipPreview_.active()||clipPoseDraft_||state_.clipPlaying)return;
  // Um quadro de referencia de cada lado (0,1 s ou o limite do clipe).
  const float now=state_.clipTime,step=std::max(.1f,1.f/std::max(1.f,static_cast<float>(live->displayRate)));
  const auto sample=[&](float time,std::vector<float> &out) {
    std::string ignored;if(!clipPreview_.seek(std::clamp(time,0.f,live->duration),ignored))return;
    for(const auto bone:state_.clipGhostIds) {
      float world[16];if(!runtime::worldMatrix(clipPreview_.scene(),bone,world)) {out.insert(out.end(),{0,0,0});continue;}
      out.insert(out.end(),{world[12],world[13],world[14]});
    }
  };
  // Hierarquia inteira do objeto animado: personagens e mecanismos.
  std::vector<std::pair<EditorEntityId,u32>> bones;animationStudioBones(clipPreview_.scene(),state_.clipOwner,bones);
  state_.clipGhostIds.clear();for(const auto &[id,depth]:bones)state_.clipGhostIds.push_back(id);
  if(now>0)sample(now-step,state_.clipGhostBefore);
  if(now<live->duration)sample(now+step,state_.clipGhostAfter);
  std::string ignored;clipPreview_.seek(now,ignored);
}
void EditorSession::revealAnimationClipTrack() {
  // Rola o dopesheet ate o canal ativo, para a chave gravada ficar visivel.
  if(!state_.clipAsset)return;
  const auto all=rows(*state_.clipAsset,state_.clipLayer,&state_.clipFilterBindings);
  for(usize i=0;i<all.size();++i)if(all[i].track==state_.clipTrack) {
    const u32 visible=std::max(1u,layout_.clipVisibleRows);
    if(i<state_.clipRow||i>=state_.clipRow+visible)state_.clipRow=static_cast<u32>(i);
    break;
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
// Clipe novo do estudio: entra no Animation do dono como clipe padrao, tocando
// ao iniciar (a janela Animation da Unity faz o mesmo ao criar um clipe). Os
// clipes que ja estavam ali continuam na lista; so deixam de ser o padrao.
bool EditorSession::playNewClipOnOwner(EditorEntityId owner,resources::AssetGuid guid,std::string &error) {
  const auto *entity=document_.find(owner);
  if(!entity||owner==document_.root()) {error="Escolha um objeto para receber o clipe";return false;}
  auto values=*entity;
  auto *animation=static_cast<scene::Animation*>(values.components.find(scene::Animation::descriptor)?values.components.edit(scene::Animation::descriptor):values.components.add(scene::Animation::descriptor));
  if(!animation||(!animation->contains(guid)&&!animation->appendClip(guid))) {error="Animation do objeto recusou o clipe";return false;}
  animation->clip=guid;animation->playAutomatically=true;
  if(!history_.begin("Clipe novo no objeto")) {error="Conclua a edição em andamento";return false;}
  const bool ok=history_.applyValues(document_,owner,values);history_.end();
  if(!ok)error="Animation do objeto recusou o clipe";
  return ok;
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
  clipPoseDraft_.reset();state_.clipPosePending=false;state_.clipPoseIsolated=false;clipPoseGizmo_={};state_.clipPoseAxis=EditorGizmoHandle::None;
  std::string previewDiagnostic;
  if(!clipPreview_.begin(document_,owner,*asset,previewDiagnostic)||!clipPreview_.seek(0,previewDiagnostic))clipPreview_.cancel();
  else if(const auto &missing=clipPreview_.missingBindings();!missing.empty()) {
    previewDiagnostic="Fora do personagem, ignorado como na Unity: "+missing.front();
    if(missing.size()>1)previewDiagnostic+=" e mais "+std::to_string(missing.size()-1);
  }
  state_.clipLayer=0;state_.clipLayersShown=state_.clipLayerPicker=false;
  state_.clipOpen=true;state_.clipGuid=guid;state_.clipOwner=owner;state_.clipTime=0;
  // Rig publicado desta raiz entra junto: limites, espelho e poses ficam ativos.
  state_.clipIkMode=false;state_.clipIkPoleSet=false;loadRigForRoot(owner);publishClipRigState();
  state_.clipGhost=false;state_.clipFilterSelected=false;state_.clipFilterBindings.clear();state_.clipGhostBefore.clear();state_.clipGhostAfter.clear();
  state_.clipCurveView=state_.clipCurveNormalized=state_.clipCurveIsolated=false;
  state_.clipRegionsShown=state_.clipRegionDetails=false;state_.clipRegion=state_.clipPreviewRegion=0;state_.clipRegionPage=0;
  state_.clipCurveViewTask=state_.clipCurveReferencePage=0;
  state_.clipCurvePins.clear();state_.clipCurveRanges.clear();
  viewportPointers_.clear();pinchDistance_=0;
  state_.clipPlaying=false;state_.clipPicker=false;state_.clipNewPicker=false;state_.clipTangentPicker=false;state_.clipRow=0;state_.clipPage=0;
  state_.clipImportedPicker=false;state_.clipSourceEntries.clear();
  state_.clipModeSide=0;
  state_.clipAuthoringPicker=0;state_.clipTargetPage=0;state_.clipTargetBranch=owner;state_.clipTargetNode=owner;state_.clipPoseShown=false;
  state_.clipPreviewTarget=0;state_.clipPoseNumbers=false;state_.clipPoseJoints=true;state_.clipPreviewExpanded=false;
  state_.clipPoseToolsShown=false;state_.clipPoseToolsPage=0;state_.clipPoseWorldSpace=false;
  state_.clipPoseSelection={owner};state_.clipPoseSelectionAdd=state_.clipPoseSelectionEmpty=state_.clipPoseSelectionIsolated=false;state_.clipPoseSetPage=0;
  state_.clipPoseRecording={true,false,0,asset->duration};std::fill_n(state_.clipPoseLocks,4,0);
  clipPreviewTopologyRevision_=~u64(0);
  state_.clipCues=state_.clipCueOptions=false;state_.clipCue=0;
  state_.clipCueSelection.clear();state_.clipRangeShown=false;
  state_.clipRangeStart=0;state_.clipRangeEnd=asset->duration;state_.clipRangeDestination=0;
  state_.clipTrack=asset->tracks.empty()?0:asset->tracks.front().id;state_.clipComponent=0;state_.clipKey=0;
  state_.clipSelection.clear();state_.clipSelectionMode=state_.clipSelectionAdd=state_.clipSelecting=false;state_.clipEditPicker=false;
  state_.clipBakeShown=state_.clipBakeHasReport=false;
  state_.clipBakeConsolidate=state_.clipBakeReferenceShown=state_.clipBakeReportConsolidated=false;
  // A previously inspected game camera must not override the authoring view:
  // otherwise orbit changes the editor camera while the image stays frozen.
  state_.cameraViewEntity=0;state_.cameraPiloting=false;
  state_.workspace=EditorWorkspace::Scene;state_.animatorOpen=false;state_.tool=EditorGizmoMode::Select;
  clipEpoch_=sceneEpoch_;state_.clipDiagnostic=previewDiagnostic;appearanceChanged_=true;refreshAnimationClip();frameAnimationClip();return true;
}
void EditorSession::closeAnimationClip() {
  clipDriverDraft_.reset();state_.clipDriverDraft=nullptr;state_.clipDriversShown=false;
  state_.clipPropertiesShown=state_.clipPropertyRemap=state_.clipPropertyCurves=false;
  state_.clipPropertyTrack=state_.clipPropertyKey=0;state_.clipPropertyQuery.clear();
  state_.clipAuthorReturn=0;
  clipPoseDraft_.reset();state_.clipPosePending=false;
  clipPoseGizmo_={};clipPoseBeforeDrag_.reset();state_.clipPoseGraph=nullptr;state_.clipPoseAxis=EditorGizmoHandle::None;
  clipPreview_.cancel();clipDrag_.reset();clipPointer_=0;
  clipHandle_=0;state_.clipTangentPicker=false;state_.clipModeSide=0;
  viewportPointers_.clear();pinchDistance_=0;
  state_.clipLayersShown=state_.clipLayerPicker=false;
  state_.clipOpen=false;state_.clipPlaying=false;state_.clipAsset=nullptr;state_.clipPicker=false;state_.clipNewPicker=false;
  state_.clipCurveView=false;state_.clipCurvePins.clear();state_.clipCurveRanges.clear();
  state_.clipRegionsShown=state_.clipRegionDetails=false;state_.clipRegion=state_.clipPreviewRegion=0;
  state_.clipImportedPicker=false;state_.clipSourceEntries.clear();
  state_.clipAuthoringPicker=0;state_.clipPoseShown=false;
  state_.clipPoseToolsShown=false;
  state_.clipPoseSelection.clear();state_.clipPoseSelectionIsolated=state_.clipPoseSelectionAdd=state_.clipPoseSelectionEmpty=false;
  state_.clipSelection.clear();state_.clipSelecting=state_.clipEditPicker=false;clipPressSelection_.clear();clipNumberSelection_.clear();
  state_.clipCueSelection.clear();clipPressCues_.clear();clipNumberCues_.clear();state_.clipRangeShown=false;
  state_.clipBakeShown=state_.clipBakeHasReport=false;
  state_.clipBakeConsolidate=state_.clipBakeReferenceShown=state_.clipBakeReportConsolidated=false;
  appearanceChanged_=true;
}
bool EditorSession::seekAnimationClip(float seconds,std::string &error) {
  if(!state_.clipOpen||!clipPreview_.active()) {error="Abra um clipe";return false;}
  // Invalid seeks must not discard a useful unrecorded pose.
  const auto *live=animationClipAsset(state_.clipGuid);
  if(!live||!std::isfinite(seconds)||seconds<0||seconds>live->duration) {error="Tempo de preview fora do clipe";return false;}
  cancelAnimationClipPose();
  if(!clipPreview_.seek(seconds,error))return false;
  state_.clipTime=seconds;appearanceChanged_=true;refreshAnimationGhost();return true;
}
bool EditorSession::previewAnimationClipRegion(u64 region,std::string &error) {
  error.clear();const auto *asset=animationClipAsset(state_.clipGuid);
  if(!state_.clipOpen||!asset||!clipPreview_.active()||clipDrag_||history_.isOpen()) {error="Abra um clipe e conclua o gesto atual";return false;}
  const auto *section=region?asset->region(region):nullptr;
  if(region&&!section) {error="Região ausente";return false;}
  if(section&&!seekAnimationClip(section->start,error))return false;
  state_.clipPreviewRegion=region;state_.clipPlaying=false;appearanceChanged_=true;return true;
}
bool EditorSession::previewAnimationClipPose(u64 trackId,std::span<const float> values,std::string &error) {
  error.clear();const auto *live=animationClipAsset(state_.clipGuid);
  if(!state_.clipOpen||!live||isPlaying()||clipDrag_||history_.isOpen()||!clipPreview_.active()) {error="Abra um clipe e conclua o gesto atual";return false;}
  if(clipPoseDraft_&&(clipPoseDraft_->revision!=live->revision||clipPoseSceneRevision_!=document_.revision()||
      clipPoseTime_!=state_.clipTime)) {cancelAnimationClipPose();error="Contexto da pose mudou; tente novamente";return false;}
  const auto *track=live->track(trackId);
  if(!track) {error="Trilha da pose ausente";return false;}
  auto candidate=clipPoseDraft_?*clipPoseDraft_:*live;
  std::vector<float> locked;
  if(!applyAnimationPoseLocks(*candidate.track(trackId),state_.clipTime,state_.clipPoseLocks[static_cast<u32>(track->path)],values,locked,error)||
     !expandAnimationPoseDraft(candidate,trackId,locked,error)||
     !candidate.putPose(trackId,state_.clipTime,locked,error))return false;
  return stageAnimationClipPoseCandidate(std::move(candidate),trackId,error);
}
bool EditorSession::stageAnimationClipPoseCandidate(resources::AnimationClipAsset candidate,u64 trackId,std::string &error) {
  const auto *live=animationClipAsset(state_.clipGuid);const auto *track=candidate.track(trackId);
  if(!state_.clipOpen||!live||!track||isPlaying()||history_.isOpen()||clipDrag_||candidate.guid!=live->guid||candidate.revision!=live->revision||
     (clipPoseDraft_&&(clipPoseSceneRevision_!=document_.revision()||clipPoseTime_!=state_.clipTime))) {
    error="Contexto da pose mudou; tente novamente";return false;
  }
  if(!validateAnimationPropertyClip(candidate,state_.clipOwner,state_.clipTime,error))return false;
  if(!clipPreview_.begin(document_,state_.clipOwner,candidate,error)||!clipPreview_.seek(state_.clipTime,error))return false;
  state_.clipTrack=trackId;state_.clipLayer=track->layer;state_.clipPreviewTarget=0;
  clipPoseDraft_=std::move(candidate);clipPoseSceneRevision_=document_.revision();clipPoseTrack_=trackId;clipPoseTime_=state_.clipTime;
  state_.clipPosePending=true;state_.clipPlaying=false;appearanceChanged_=true;refreshAnimationClip();return true;
}
bool EditorSession::recordAnimationClipPose(std::string &error) {
  error.clear();const auto *live=animationClipAsset(state_.clipGuid);
  if(!clipPoseDraft_||!live||isPlaying()||clipDrag_||clipPoseDraft_->guid!=state_.clipGuid||
      clipPoseDraft_->revision!=live->revision||clipPoseSceneRevision_!=document_.revision()||
      clipPoseTime_!=state_.clipTime||clipPoseTrack_!=state_.clipTrack) {error="Pose pendente ausente ou contexto modificado";return false;}
  const auto draft=*clipPoseDraft_;
  if(!editAnimationClipAsset(live->guid,live->revision,[&](auto &candidate){candidate=draft;return true;},error))return false;
  clipPoseDraft_.reset();state_.clipPosePending=false;refreshAnimationClip();frameAnimationClip();appearanceChanged_=true;return true;
}
void EditorSession::cancelAnimationClipPose() {
  if(!clipPoseDraft_)return;
  clipPoseDraft_.reset();state_.clipPosePending=false;state_.clipAsset=animationClipAsset(state_.clipGuid);
  if(state_.clipOpen&&state_.clipAsset) {
    std::string error;
    if(!clipPreview_.begin(document_,state_.clipOwner,*state_.clipAsset,error)||!clipPreview_.seek(std::min(state_.clipTime,state_.clipAsset->duration),error)) {
      clipPreview_.cancel();state_.clipDiagnostic=error;
    }
  }
  appearanceChanged_=true;
}
void EditorSession::refreshAnimationClip() {
  state_.clipAssets=&animationClipAssets_;state_.clipAsset=nullptr;
  state_.clipPoseGraph=nullptr;
  if(!state_.clipOpen)return;
  if(clipPreviewTopologyRevision_!=document_.revision()) {
    state_.clipPreviewJoints=animationPreviewJoints(document_,state_.clipOwner);
    clipPreviewTopologyRevision_=document_.revision();
  }
  if(state_.clipPreviewTarget&&!document_.exists(state_.clipPreviewTarget))state_.clipPreviewTarget=0;
  const auto *live=animationClipAsset(state_.clipGuid);
  if(clipPoseDraft_&&(!live||clipPoseDraft_->revision!=live->revision||clipPoseSceneRevision_!=document_.revision()||
      clipPoseTime_!=state_.clipTime||clipPoseTrack_!=state_.clipTrack))cancelAnimationClipPose();
  const auto *asset=clipDrag_?&*clipDrag_:clipPoseDraft_?&*clipPoseDraft_:live;
  if(!asset||clipEpoch_!=sceneEpoch_||!document_.exists(state_.clipOwner)||isPlaying()) {closeAnimationClip();return;}
  state_.clipAsset=asset;
  if(state_.clipRegion&&!asset->region(state_.clipRegion)) {state_.clipRegion=0;state_.clipRegionDetails=false;}
  if(state_.clipPreviewRegion&&!asset->region(state_.clipPreviewRegion)) {state_.clipPreviewRegion=0;state_.clipPlaying=false;}
  state_.clipRegionPage=std::min(state_.clipRegionPage,static_cast<u32>(asset->regions.empty()?0:(asset->regions.size()-1)/3));
  std::erase_if(state_.clipCurvePins,[&](const auto &pin){const auto *track=asset->track(pin.track);return !track||pin.component>=track->curves.size();});
  state_.clipCurveReferencePage=std::min(state_.clipCurveReferencePage,static_cast<u32>(state_.clipCurvePins.empty()?0:(state_.clipCurvePins.size()-1)/3));
  std::erase_if(state_.clipCueSelection,[&](u64 id){return !asset->cue(id);});
  state_.clipRangeStart=std::min(state_.clipRangeStart,asset->duration);
  state_.clipRangeEnd=std::clamp(state_.clipRangeEnd,state_.clipRangeStart,asset->duration);
  state_.clipRangeDestination=std::min(state_.clipRangeDestination,asset->duration);
  state_.clipPoseRecording.start=std::min(state_.clipPoseRecording.start,asset->duration);
  state_.clipPoseRecording.end=std::clamp(state_.clipPoseRecording.end,state_.clipPoseRecording.start,asset->duration);
  if(state_.clipCue&&!asset->cue(state_.clipCue)) {state_.clipCue=0;state_.clipCueOptions=false;}
  if(clipPreview_.active())state_.clipPoseGraph=&clipPreview_.scene();
  refreshAnimationPropertySurface();
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
    } else if(!(state_.numericError&&w::owns(state_.numericField))) {
      // A successful preview refresh cannot accept or erase a refused numeric
      // transaction. Its actionable reason belongs to the still-open field.
      state_.clipDiagnostic.clear();
    }
  }
  state_.clipTarget=0;state_.clipPoseCount=0;
  if(!document_.exists(state_.clipTargetBranch)||(state_.clipTargetBranch!=state_.clipOwner&&!document_.isDescendantOf(state_.clipTargetBranch,state_.clipOwner))) {state_.clipTargetBranch=state_.clipOwner;state_.clipTargetPage=0;state_.clipAuthoringPicker=state_.clipAuthoringPicker?1:0;}
  refreshAnimationDrivers();
  if(!track)return;
  if(track->path==resources::AnimationPath::Weights&&track->weightCount<64)
    state_.clipPoseLocks[static_cast<u32>(track->path)]&=(u64(1)<<track->weightCount)-1;
  runtime::SourceAnimations bindings;bindings.source=asset->source.valid()?asset->source:asset->guid;
  for(const auto &binding:asset->bindings) {bindings.nodes.push_back(binding.sourceNode);bindings.nodeNames.push_back(binding.name);bindings.nodePaths.push_back(binding.path);}
  std::vector<runtime::ObjectId> targets;runtime::resolveAnimationTargets(document_,state_.clipOwner,bindings,targets);
  for(usize i=0;i<asset->bindings.size();++i)if(asset->bindings[i].id==track->binding&&i<targets.size())state_.clipTarget=targets[i];
  if(state_.clipPoseSelectionEmpty)state_.clipTarget=0;
  syncAnimationPoseSelection();
  // Fields edit the selected layer, never the already-composed viewport pose.
  if(state_.clipTarget) {
    state_.clipPoseCount=track->path==resources::AnimationPath::Weights?track->weightCount:3;
    if(track->path!=resources::AnimationPath::Rotation||track->rotationMode==resources::AnimationRotationMode::Euler) {
      for(u32 c=0;c<state_.clipPoseCount;++c) {
        resources::AnimationCurveSample v;if(resources::sampleValidatedAnimationCurve(track->curves[c],state_.clipTime,v))
          state_.clipPoseValues[c]=static_cast<float>(v.value)*(track->path==resources::AnimationPath::Weights?100.f:1.f);
      }
    } else {
      resources::AnimationChannel channel;channel.path=track->path;channel.rotationMode=track->rotationMode;channel.euler=track->euler;channel.curves=track->curves;
      float q[4],matrix[16];const float zero[]{0,0,0},one[]{1,1,1},identity[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};runtime::Transform pose;
      if(resources::sampleValidatedAnimationChannel(channel,state_.clipTime,q)) {
        resources::composeTransform(zero,q,one,matrix);
        if(runtime::localTransformForWorld(matrix,identity,pose))std::copy_n(pose.rotationDegrees,3,state_.clipPoseValues);
      }
    }
    if(state_.clipPoseShown&&state_.clipPoseIsolated&&clipPreview_.active()&&
        (!state_.clipPreviewTarget||state_.clipPreviewTarget==state_.clipTarget)) {
      resources::AnimationChannel channel;channel.path=track->path;channel.rotationMode=track->rotationMode;channel.euler=track->euler;channel.weightCount=track->weightCount;channel.curves=track->curves;
      float values[resources::MaximumMorphTargets];std::string error;
      // The raw override belongs to one property. Remove it before isolating
      // another property; a static preview needs no extra evaluation per frame.
      if((clipPoseTrack_!=track->id||clipPoseTime_!=state_.clipTime)&&!clipPreview_.seek(state_.clipTime,error)) {
        state_.clipDiagnostic=error;return;
      }
      if(!resources::sampleValidatedAnimationChannel(channel,state_.clipTime,values)||
         !clipPreview_.overridePose(state_.clipTarget,track->path,std::span(values,channel.components()),error)) {
        state_.clipDiagnostic=error.empty()?"Camada não produz uma pose isolável":error;
      } else {clipPoseTrack_=track->id;clipPoseTime_=state_.clipTime;}
    }
  }
}
bool EditorSession::selectAnimationClipPreviewTarget(EditorEntityId target,resources::AnimationPath path,bool create,std::string &error) {
  error.clear();const auto *live=animationClipAsset(state_.clipGuid);
  if(static_cast<u32>(path)>static_cast<u32>(resources::AnimationPath::Weights)) {error="Propriedade de preview não suportada";return false;}
  if(!state_.clipOpen||clipEpoch_!=sceneEpoch_||!live||!document_.exists(target)||
      (target!=state_.clipOwner&&!document_.isDescendantOf(target,state_.clipOwner))) {
    error="Alvo deve pertencer à raiz do clipe";return false;
  }
  runtime::SourceAnimations bindings;bindings.source=live->source.valid()?live->source:live->guid;
  for(const auto &binding:live->bindings) {bindings.nodes.push_back(binding.sourceNode);bindings.nodePaths.push_back(binding.path);bindings.nodeNames.push_back(binding.name);}
  std::vector<runtime::ObjectId> targets;runtime::resolveAnimationTargets(document_,state_.clipOwner,bindings,targets);
  u64 exact=0,fallback=0;
  for(const auto &track:live->tracks)if(track.layer==state_.clipLayer)
    for(usize i=0;i<live->bindings.size();++i)if(live->bindings[i].id==track.binding&&i<targets.size()&&targets[i]==target) {
      if(!fallback)fallback=track.id;
      if(track.path==path)exact=track.id;
    }
  // Selecting, orbiting and focusing cannot create channels or history entries.
  // Only the explicit TRS action may allocate a missing channel.
  u64 id=exact?exact:!create?fallback:0;
  if(!id&&create&&!state_.clipPoseRecording.createChannels) {error="Auto-key: só canais existentes; adicione o canal explicitamente ou permita criação";return false;}
  cancelAnimationClipPose();state_.clipPlaying=false;
  auto mode=resources::AnimationRotationMode::Quaternion;
  if(path==resources::AnimationPath::Rotation) {
    // New clips already have a root rotation channel carrying the user's
    // representation choice. Inherit it for new joints; existing tracks above
    // keep their own representation and are never silently converted.
    for(const auto &track:live->tracks)if(track.layer==state_.clipLayer&&track.path==path) {mode=track.rotationMode;break;}
  }
  if(!id&&create&&!addAnimationClipTrack(live->guid,live->revision,state_.clipOwner,target,path,
      mode,id,error))return false;
  state_.clipPreviewTarget=target;state_.clipTargetNode=target;state_.clipAuthoringPicker=0;
  state_.clipPoseSelectionEmpty=false;
  if(!selectingAnimationPose_)state_.clipPoseSelection={target};
  if(id) {state_.clipTrack=id;state_.clipComponent=0;state_.clipKey=0;state_.clipSelection.clear();}
  if(clipPreview_.active()&&!clipPreview_.seek(state_.clipTime,error))return false;
  refreshAnimationClip();
  if(id) {
    const auto all=rows(*state_.clipAsset,state_.clipLayer,&state_.clipFilterBindings);
    for(usize i=0;i<all.size();++i)if(all[i].track==id) {state_.clipRow=static_cast<u32>(i);break;}
  }
  appearanceChanged_=true;return true;
}
bool EditorSession::pickAnimationClipPreviewTarget(ui::UiPoint position) {
  if(!state_.clipPoseShown||!clipPreview_.active())return true;
  const auto &graph=clipPreview_.scene();EditorEntityId target=0;
  float best=22.f*22.f,depth=std::numeric_limits<float>::max();
  if(state_.clipPoseJoints)for(const auto bone:state_.clipPreviewJoints) {
    const auto *object=graph.find(bone);if(!object||!graph.activeInHierarchy(bone)||!object->visible)continue;
    bool visible=true;
    for(auto node=object;node;node=graph.find(node->parent))
      if(!node->visible||state_.sceneHiddenHas(node->id)||(node->layer<32&&(state_.hiddenLayers&(1u<<node->layer)))) {visible=false;break;}
    if(!visible||!animationPoseSelectionVisible(bone)||state_.scenePickOffHas(bone)||(object->layer<32&&(state_.unpickableLayers&(1u<<object->layer))))continue;
    float world[16];if(!runtime::worldMatrix(graph,bone,world))continue;
    const auto point=projectWorldToScreen(view_,world+12);
    if(!point.valid||!layout_.viewport.contains(point.screen))continue;
    const float dx=position.x-point.screen.x,dy=position.y-point.screen.y,d=dx*dx+dy*dy;
    if(d<best-.01f||(std::abs(d-best)<.01f&&point.viewDepth<depth)) {target=bone;best=d;depth=point.viewDepth;}
  }
  if(!target) {
    // Bound the expensive skin/BVH fallback before constructing candidates;
    // an empty tap on this rig must not deform every other model in the scene.
    buildPickCandidates(&graph,false,state_.clipOwner);
    if(state_.clipPoseSelectionIsolated)std::erase_if(candidates_,[&](const auto &value){return !animationPoseSelectionVisible(value.id);});
    const auto hit=pickNearest(candidates_,screenPointToRay(view_,position));if(hit.hit)target=hit.id;
  }
  // IK: um toque num dedo pega a mao (o efetor), como os efetores do HumanIK e
  // o Pinning do UMotion. Efetor = osso com IK de dois ossos ou chamado de
  // mao/punho/pe/tornozelo, ate quatro niveis acima do osso tocado.
  if(target&&state_.clipIkMode) {
    const auto effector=[&](const runtime::SceneObject &o) {
      if(o.components.find(scene::TwoBoneIk::descriptor))return true;
      std::string name(o.name);for(auto &c:name)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      for(const char *part:{"wrist","hand","ankle","foot","punho","mao_","mão"})if(name.find(part)!=std::string::npos)return true;
      return false;
    };
    const runtime::SceneObject *at=graph.find(target);
    for(u32 up=0;at&&up<=4&&at->id!=state_.clipOwner;++up,at=graph.find(at->parent))
      if(effector(*at)) {target=at->id;break;}
  }
  if(target) {
    std::string error;const bool bone=std::find(state_.clipPreviewJoints.begin(),state_.clipPreviewJoints.end(),target)!=state_.clipPreviewJoints.end();
    if(state_.clipPoseSelectionAdd)toggleAnimationPoseTarget(target,error);
    else selectAnimationClipPreviewTarget(target,bone?resources::AnimationPath::Rotation:resources::AnimationPath::Translation,false,error);
    state_.clipDiagnostic=error;refreshClipRowFilter();revealAnimationClipTrack();
  }
  return true;
}
bool EditorSession::handleAnimationPoseGizmo(const ui::UiPointerEvent &event,const ui::UiPointerRouting &routing) {
  using ui::UiPointerPhase;
  const auto code=animator_widget::code(routing.widgetId);
  const bool handle=clip_widget::owns(routing.widgetId)&&code>=w::PoseGizmo&&code<w::PoseGizmo+3;
  if(!clipPoseGizmo_.active&&!handle)return false;
  if(clipPoseGizmo_.active&&event.pointerId!=clipPosePointer_)return true;
  const auto *asset=state_.clipAsset;const auto *track=asset?asset->track(state_.clipTrack):nullptr;
  if(!track||!state_.clipPoseShown||!state_.clipPoseIsolated||track->path==resources::AnimationPath::Weights||
      (state_.clipPreviewTarget&&state_.clipPreviewTarget!=state_.clipTarget))return true;
  const auto mode=track->path==resources::AnimationPath::Translation?EditorGizmoMode::Translate:
      track->path==resources::AnimationPath::Rotation?EditorGizmoMode::Rotate:EditorGizmoMode::Scale;
  if(!clipPoseGizmo_.active) {
    if(event.phase!=UiPointerPhase::Down||!clipPreview_.active())return true;
    if(mode==EditorGizmoMode::Scale&&(state_.clipPoseLocks[static_cast<u32>(track->path)]&(u64(1)<<(code-w::PoseGizmo)))) {state_.clipDiagnostic="Eixo local travado";return true;}
    const auto &graph=clipPreview_.scene();const auto *object=graph.find(state_.clipTarget);if(!object)return true;
    if(!runtime::worldMatrix(graph,object->id,clipPoseWorld_)||!runtime::parentWorldMatrix(graph,object->id,clipPoseParent_)||
       !renderer::buildNormalMatrix(clipPoseParent_,clipPoseInverse_)) {state_.clipDiagnostic="Gizmo exige pai com transformação inversível";return true;}
    EditorGizmoSettings settings;settings.screenLengthPixels=72;clipPoseView_=view_;clipPoseAxis_=code-w::PoseGizmo;
    auto frame=mode==EditorGizmoMode::Scale?buildLocalScaleGizmoFrame(view_,clipPoseWorld_,settings):buildGizmoFrame(view_,clipPoseWorld_+12,settings);
    if(mode==EditorGizmoMode::Rotate) {
      if(!gizmoRingAngle(view_,frame.origin,clipPoseAxis_,routing.start,clipPoseAngle_))return true;
      frame.axisUsable[clipPoseAxis_]=true;clipPoseTotalAngle_=0;
    }
    if(!beginGizmoDrag(frame,static_cast<EditorGizmoHandle>(clipPoseAxis_+1),object->transform,settings,clipPoseGizmo_))return true;
    clipPoseBeforeDrag_=clipPoseDraft_;clipPosePointer_=event.pointerId;clipPoseMoved_=false;state_.clipPlaying=false;
    state_.clipPoseAxis=clipPoseGizmo_.handle;return true;
  }
  if(event.phase==UiPointerPhase::Cancel) {
    clipPoseGizmo_={};state_.clipPoseAxis=EditorGizmoHandle::None;
    cancelAnimationClipPose();clipPoseDraft_=std::move(clipPoseBeforeDrag_);state_.clipPosePending=clipPoseDraft_.has_value();
    if(clipPoseDraft_) {std::string error;clipPreview_.begin(document_,state_.clipOwner,*clipPoseDraft_,error);clipPreview_.seek(state_.clipTime,error);}
    refreshAnimationClip();return true;
  }
  if(event.phase==UiPointerPhase::Move&&routing.dragging) {
    runtime::Transform moved;bool resolved=false;
    if(mode==EditorGizmoMode::Rotate) {
      float angle;
      if(gizmoRingAngle(clipPoseView_,clipPoseGizmo_.frame.origin,clipPoseAxis_,routing.position,angle)) {
        clipPoseTotalAngle_+=std::remainder(angle-clipPoseAngle_,6.28318530718f);clipPoseAngle_=angle;
        float world[16];std::copy_n(clipPoseWorld_,16,world);const u32 u=(clipPoseAxis_+1)%3,v=(clipPoseAxis_+2)%3;
        const float c=std::cos(clipPoseTotalAngle_),s=std::sin(clipPoseTotalAngle_);
        for(u32 column=0;column<3;++column) {world[column*4+u]=c*clipPoseWorld_[column*4+u]-s*clipPoseWorld_[column*4+v];world[column*4+v]=s*clipPoseWorld_[column*4+u]+c*clipPoseWorld_[column*4+v];}
        resolved=runtime::localTransformForWorld(world,clipPoseParent_,moved);
        if(!resolved)state_.clipDiagnostic="Rotação exige transformação local sem shear";
      }
    } else {
      resolved=resolveGizmoTransform(clipPoseGizmo_,mode,routing.totalDelta,moved);
      if(resolved&&mode==EditorGizmoMode::Translate) {
        float delta[3];for(u32 c=0;c<3;++c)delta[c]=moved.position[c]-clipPoseGizmo_.initial.position[c];
        for(u32 a=0;a<3;++a) {moved.position[a]=clipPoseGizmo_.initial.position[a];for(u32 b=0;b<3;++b)moved.position[a]+=clipPoseInverse_[a*4+b]*delta[b];}
      }
    }
    if(resolved) {
      std::vector<float> values;
      if(mode==EditorGizmoMode::Rotate) {
        float q[4];runtime::transformRotationQuaternion(moved,q);
        if(track->rotationMode==resources::AnimationRotationMode::Euler) {
          float euler[3];if(!resources::eulerFrameAnglesNear(track->euler,q,state_.clipPoseValues,euler))return true;values.assign(euler,euler+3);
        } else values.assign(q,q+4);
      } else if(mode==EditorGizmoMode::Translate)values.assign(moved.position,moved.position+3);
      else values.assign(moved.scale,moved.scale+3);
      std::string error;if(previewAnimationClipPose(track->id,values,error))clipPoseMoved_=true;else state_.clipDiagnostic=error;
    }
  }
  if(event.phase==UiPointerPhase::Up) {
    clipPoseGizmo_={};state_.clipPoseAxis=EditorGizmoHandle::None;clipPoseBeforeDrag_.reset();
    if(clipPoseMoved_&&state_.clipPoseAutoKey) {std::string error;if(!autoRecordAnimationClipPose(error))state_.clipDiagnostic=error;}
    clipPoseMoved_=false;
  }
  return true;
}
bool EditorSession::applyAnimationClipName(std::string_view value) {
  const auto *asset=animationClipAsset(clipNumberGuid_);std::string error;
  if(!state_.clipOpen||!asset||asset->guid!=state_.clipGuid||asset->revision!=clipNumberRevision_) {state_.clipDiagnostic="Clipe mudou; reabra o nome";return false;}
  const bool ok=editAnimationClipAsset(asset->guid,asset->revision,[&](auto &candidate){candidate.name=std::string(value);return true;},error);
  state_.clipDiagnostic=error;if(ok)refreshAnimationClip();return ok;
}
bool EditorSession::applyAnimationClipCueName(std::string_view value) {
  const auto *asset=animationClipAsset(clipNumberGuid_);std::string error;
  if(!state_.clipOpen||!asset||asset->guid!=state_.clipGuid||asset->revision!=clipNumberRevision_||state_.clipCue!=clipNumberKey_) {state_.clipDiagnostic="Evento mudou; reabra o nome";return false;}
  const bool ok=editAnimationClipAsset(asset->guid,asset->revision,[&](auto &candidate){
    const auto *cue=candidate.cue(clipNumberKey_);if(!cue)return false;auto valueCue=*cue;valueCue.name=value;u64 id=0;return candidate.putCue(valueCue,id,error);
  },error);
  state_.clipDiagnostic=error;if(ok)refreshAnimationClip();return ok;
}
bool EditorSession::applyAnimationClipRegionName(std::string_view value) {
  const auto *asset=animationClipAsset(clipNumberGuid_);std::string error;
  if(!state_.clipOpen||!asset||asset->guid!=state_.clipGuid||asset->revision!=clipNumberRevision_||state_.clipRegion!=clipNumberKey_) {state_.clipDiagnostic="Região mudou; reabra o nome";return false;}
  const bool ok=editAnimationClipAsset(asset->guid,asset->revision,[&](auto &candidate){
    const auto *region=candidate.region(clipNumberKey_);if(!region)return false;auto edited=*region;edited.name=value;u64 id=0;return candidate.putRegion(edited,id,error);
  },error);
  state_.clipDiagnostic=error;if(ok)refreshAnimationClip();return ok;
}
bool EditorSession::applyAnimationClipLayerName(std::string_view value) {
  const auto *asset=animationClipAsset(clipNumberGuid_);std::string error;
  if(!state_.clipOpen||!asset||asset->guid!=state_.clipGuid||asset->revision!=clipNumberRevision_||state_.clipLayer!=clipNumberLayer_) {state_.clipDiagnostic="Camada mudou; reabra o nome";return false;}
  const bool ok=editAnimationClipAsset(asset->guid,asset->revision,[&](auto &candidate){auto *l=candidate.layer(clipNumberLayer_);if(!l)return false;l->name=value;return true;},error);
  state_.clipDiagnostic=error;if(ok)refreshAnimationClip();return ok;
}
void EditorSession::frameAnimationClip(bool selectionOnly) {
  const auto *asset=state_.clipAsset;if(!asset)return;
  state_.clipStart=0;state_.clipEnd=std::max(asset->duration,1.f/asset->displayRate);
  if(selectionOnly) {
    double first=asset->duration,last=0;bool found=false;
    for(const auto &address:state_.clipSelection)if(const auto *key=selectedKey(*asset,address.track,address.component,address.key)) {first=std::min(first,double(key->time));last=std::max(last,double(key->time));found=true;}
    if(!found)if(const auto *key=selectedKey(*asset,state_.clipTrack,state_.clipComponent,state_.clipKey)) {first=last=key->time;found=true;}
    if(found) {const double margin=std::max(1.0/asset->displayRate,(last-first)*.12);state_.clipStart=static_cast<float>(std::max(0.0,first-margin));state_.clipEnd=static_cast<float>(std::min(double(asset->duration),last+margin));}
  }
  const auto *track=asset->track(state_.clipTrack);if(!track||state_.clipComponent>=track->curves.size())return;
  const auto &curve=track->curves[state_.clipComponent];
  state_.clipCurveRanges.clear();
  state_.clipCurveRanges.push_back(animationCurveViewRange(*track,state_.clipComponent,state_.clipStart,state_.clipEnd));
  double minimum=state_.clipCurveRanges.front().minimum,maximum=state_.clipCurveRanges.front().maximum;
  for(const auto &pin:state_.clipCurvePins)if(pin.track!=state_.clipTrack||pin.component!=state_.clipComponent)if(const auto *pinned=asset->track(pin.track);pinned&&pin.component<pinned->curves.size()) {
    const auto range=animationCurveViewRange(*pinned,pin.component,state_.clipStart,state_.clipEnd);state_.clipCurveRanges.push_back(range);
    if(!state_.clipCurveIsolated) {minimum=std::min(minimum,range.minimum);maximum=std::max(maximum,range.maximum);}
  }
  if(state_.clipCurves)if(const auto *key=selectedKey(*asset,state_.clipTrack,state_.clipComponent,state_.clipKey)) {
    const usize index=static_cast<usize>(key-curve.keys.data());
    for(bool incoming:{true,false}) {
      if((incoming&&!index)||(!incoming&&index+1==curve.keys.size()))continue;
      const double span=incoming?key->time-curve.keys[index-1].time:curve.keys[index+1].time-key->time;
      const double weight=(incoming?key->weightedIn:key->weightedOut)?(incoming?key->inWeight:key->outWeight):1.0/3;
      const double handle=key->value+(incoming?-1:1)*span*weight*resources::animationCurveSlope(curve,index,incoming);
      if(std::isfinite(handle)) {minimum=std::min(minimum,handle);maximum=std::max(maximum,handle);auto &range=state_.clipCurveRanges.front();range.minimum=std::min(range.minimum,handle);range.maximum=std::max(range.maximum,handle);}
    }
  }
  if(state_.clipCurveNormalized) {minimum=0;maximum=1;}
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
  if(code==w::CueTime||code==w::CueTag||code==w::CueValue)clipNumberKey_=state_.clipCue;
  if(code==w::RegionStart||code==w::RegionEnd)clipNumberKey_=state_.clipRegion;
  if(code==w::PropertyKeyTime||code==w::PropertyTime||(code>=w::PropertyCurveNumber&&code<w::PropertyCurveNumber+6)||(code>=w::PropertyValue&&code<w::PropertyValue+4)) {
    clipNumberTrack_=state_.clipPropertyTrack;clipNumberKey_=state_.clipPropertyKey;clipNumberComponent_=state_.clipPropertyComponent;
  }
  clipNumberSceneRevision_=document_.revision();clipNumberTime_=state_.clipTime;
  clipNumberSelection_=state_.clipSelection;
  clipNumberCues_=state_.clipCueSelection;
  beginAnimatorNumber(code,value);
  if(code==w::CueTime||code==w::CueTag||code==w::CueValue)
    std::snprintf(state_.numericText,sizeof state_.numericText,"%.17g",value);
  state_.numericEntity=state_.clipOwner;
}
bool EditorSession::applyAnimationClipNumber(u32 code,double value) {
  if(!std::isfinite(value)||(code!=w::CueValue&&std::abs(value)>std::numeric_limits<float>::max()))return false;
  const auto *asset=animationClipAsset(clipNumberGuid_);std::string error;
  if(!state_.clipOpen||state_.clipGuid!=clipNumberGuid_||!asset||asset->revision!=clipNumberRevision_) {state_.status="Clipe mudou; reabra o campo";return false;}
  if((code>=w::DriverConstant&&code<w::DriverConstant+4)||(code>=w::DriverCurveValue&&code<w::DriverCurveValue+6))return applyAnimationDriverNumber(code,value);
  if(code>=w::PropertyCurveNumber&&code<w::PropertyCurveNumber+6)return applyAnimationPropertyCurveNumber(code,value);
  if(code==w::PropertyTime){const bool ok=seekAnimationClip(float(value),error);state_.clipDiagnostic=error;return ok;}
  if(code==w::PropertyKeyTime||(code>=w::PropertyValue&&code<w::PropertyValue+4))return applyAnimationPropertyNumber(code,value);
  if(code==w::Time) {const bool ok=seekAnimationClip(static_cast<float>(value),error);state_.clipDiagnostic=error;return ok;}
  if(code==w::RangeStart||code==w::RangeEnd||code==w::RangeDestination) {
    if(value<0||value>asset->duration) {state_.clipDiagnostic="Tempo fora do clipe";return false;}
    if((code==w::RangeStart&&value>=state_.clipRangeEnd)||(code==w::RangeEnd&&value<=state_.clipRangeStart)) {
      state_.clipDiagnostic="O intervalo exige início menor que fim";return false;
    }
    (code==w::RangeStart?state_.clipRangeStart:code==w::RangeEnd?state_.clipRangeEnd:state_.clipRangeDestination)=static_cast<float>(value);
    state_.clipDiagnostic.clear();return true;
  }
  if(code==w::RangeCopies||code==w::RangeTolerance) {
    if(code==w::RangeCopies) {
      if(value<1||value>64||std::floor(value)!=value) {state_.clipDiagnostic="Use 1 a 64 cópias inteiras";return false;}
      state_.clipRangeCopies=static_cast<u32>(value);
    } else {
      if(value<0||value>1) {state_.clipDiagnostic="Tolerância exige valor de 0 a 1";return false;}
      state_.clipRangeTolerance=static_cast<float>(value);
    }
    state_.clipDiagnostic.clear();return true;
  }
  if(code==w::PoseArmStart||code==w::PoseArmEnd) {
    auto policy=state_.clipPoseRecording;
    (code==w::PoseArmStart?policy.start:policy.end)=static_cast<float>(value);
    const bool ok=configureAnimationPoseRecording(policy,error);state_.clipDiagnostic=error;return ok;
  }
  if(code>=w::PoseValue&&code<w::PoseValue+resources::MaximumMorphTargets) {
    if(document_.revision()!=clipNumberSceneRevision_||state_.clipTime!=clipNumberTime_||state_.numericEntity!=state_.clipOwner||state_.clipTrack!=clipNumberTrack_) {state_.clipDiagnostic="Pose mudou; reabra o campo";return false;}
    const auto *track=asset->track(clipNumberTrack_);const u32 component=code-w::PoseValue;
    if(!track||component>=state_.clipPoseCount||!state_.clipTarget)return false;
    std::vector<float> values(state_.clipPoseValues,state_.clipPoseValues+state_.clipPoseCount);values[component]=static_cast<float>(value);
    if(track->path==resources::AnimationPath::Weights)for(auto &v:values)v*=.01f;
    else if(track->path==resources::AnimationPath::Rotation&&track->rotationMode!=resources::AnimationRotationMode::Euler) {
      runtime::Transform pose;std::copy_n(values.begin(),3,pose.rotationDegrees);float q[4];runtime::transformRotationQuaternion(pose,q);values.assign(q,q+4);
    }
    bool ok=previewAnimationClipPose(clipNumberTrack_,values,error);
    if(ok&&state_.clipPoseAutoKey)ok=autoRecordAnimationClipPose(error);
    state_.clipDiagnostic=error;return ok;
  }
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
    if(code==w::RegionStart||code==w::RegionEnd) {
      const auto *region=candidate.region(clipNumberKey_);
      if(!region||state_.clipRegion!=clipNumberKey_) {error="Região mudou; reabra o campo";return false;}
      auto edited=*region;if(code==w::RegionStart)edited.start=static_cast<float>(value);else edited.end=static_cast<float>(value);
      u64 id=0;return candidate.putRegion(edited,id,error);
    }
    if(code==w::CueTime||code==w::CueTag||code==w::CueValue) {
      const auto *cue=candidate.cue(clipNumberKey_);
      if(!cue||state_.clipCue!=clipNumberKey_) {error="Evento mudou; reabra o campo";return false;}
      auto edited=*cue;
      if(code==w::CueTime)edited.time=static_cast<float>(value);
      else if(code==w::CueTag) {if(value<0||value>16777215||std::floor(value)!=value) {error="Codigo deve ser inteiro entre 0 e 16777215";return false;}edited.tag=static_cast<u32>(value);}
      else edited.value=value;
      u64 id=0;return candidate.putCue(edited,id,error);
    }
    if(code==w::LayerWeight||code==w::LayerReferenceTime) {
      auto *l=candidate.layer(clipNumberLayer_);
      if(!l||state_.clipLayer!=clipNumberLayer_) {error="Camada mudou; reabra o campo";return false;}
      if(code==w::LayerWeight)l->weight=static_cast<float>(value);else l->referenceTime=static_cast<float>(value);
      return true;
    }
    if(code==w::SelectionOffset||code==w::SelectionScale) {
      if(state_.clipCues) {
        if(state_.clipCueSelection!=clipNumberCues_||state_.clipTime!=clipNumberTime_) {error="Seleção de cues ou pivot mudou; reabra o campo";return false;}
        return resources::transformAnimationCueTimes(candidate,clipNumberCues_,clipNumberTime_,code==w::SelectionScale?value:1,code==w::SelectionOffset?value:0,error);
      }
      if(state_.clipSelection!=clipNumberSelection_||state_.clipTime!=clipNumberTime_) {error="Seleção ou pivot mudou; reabra o campo";return false;}
      return resources::transformAnimationKeyTimes(candidate,clipNumberSelection_,clipNumberTime_,code==w::SelectionScale?value:1,code==w::SelectionOffset?value:0,error);
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
  using ui::UiPointerPhase;
  if(w::owns(routing.widgetId)&&routing.target==ui::UiPointerTarget::Widget) {
    const auto code=animator_widget::code(routing.widgetId);
    // Arvore de ossos: arrastar na vertical rola de linha em linha (40 px),
    // sem escolher osso; o toque curto continua escolhendo.
    if((code==w::BoneTree||(code>=w::BoneRow&&code<w::BoneRow+0x60))&&routing.dragging) {
      if(event.phase==UiPointerPhase::Move) {
        const u32 rows=static_cast<u32>(std::max(0.f,layout_.clipBoneRows));
        const u32 limit=layout_.clipBoneCount>rows?layout_.clipBoneCount-rows:0u;
        state_.clipBoneScroll=std::min(state_.clipBoneScroll,limit);clipBoneDrag_+=routing.stepDelta.y;
        while(clipBoneDrag_<=-40.f) {if(state_.clipBoneScroll<limit)++state_.clipBoneScroll;clipBoneDrag_+=40.f;}
        while(clipBoneDrag_>=40.f) {if(state_.clipBoneScroll)--state_.clipBoneScroll;clipBoneDrag_-=40.f;}
      }
      if(routing.released)clipBoneDrag_=0;
      return true;
    }
    if(code==w::BoneTree)return true;
    // Regua do tempo (Unity Animation window: clicar ou arrastar na regua move
    // o playhead): tocar busca o tempo, arrastar faz o scrub.
    if(code==w::TimeRuler) {
      const auto *asset=state_.clipAsset;const auto &canvas=layout_.clipCanvas;
      if(asset&&!canvas.isEmpty()&&(event.phase==UiPointerPhase::Down||(event.phase==UiPointerPhase::Move&&routing.dragging))) {
        state_.clipPlaying=false;
        const float t=state_.clipStart+(event.position.x-canvas.x)/std::max(1.f,canvas.width)*(state_.clipEnd-state_.clipStart);
        const float time=std::clamp(std::round(t*asset->displayRate)/asset->displayRate,0.f,asset->duration);
        std::string error;
        if(time!=state_.clipTime&&!seekAnimationClip(time,error))state_.clipDiagnostic=error;
      }
      return true;
    }
  }
  // Estudio: abas do inspector, arvore de ossos e folha de rig vivem ao lado
  // do viewport; tratadas antes do fluxo da timeline, sem cancelar a pose.
  if(routing.tapped&&routing.target==ui::UiPointerTarget::Widget) {
    if(state_.clipStudioTab==1&&handleRigTool(routing))return true;
    if(w::owns(routing.widgetId)) {
      const auto code=animator_widget::code(routing.widgetId);
      if(code>=w::StudioTabKey&&code<=w::StudioTabIk) {
        state_.clipStudioTab=static_cast<u8>(code-w::StudioTabKey);
        if(state_.clipStudioTab==1&&(!state_.rigToolOpen||studio_->rigRoot!=state_.clipOwner))openRigTool(state_.clipOwner);
        return true;
      }
      if(code>=w::PoseIkWizard&&code<=w::PoseFollow) {
        std::string summary,error;const auto selected=state_.clipPreviewTarget?state_.clipPreviewTarget:state_.clipTarget;
        const float start=state_.clipRangeShown?std::min(state_.clipRangeStart,state_.clipRangeEnd):0.f;
        const float end=state_.clipRangeShown?std::max(state_.clipRangeStart,state_.clipRangeEnd):(state_.clipAsset?state_.clipAsset->duration:0.f);
        bool ok=false;
        if(code==w::PoseIkWizard)ok=EditorStudio::setupIk(*this,state_.clipOwner,summary,error);
        else if(code==w::PosePin)ok=EditorStudio::pinTip(*this,selected,start,end,summary,error);
        else if(code==w::PoseFollowParent) {studio_->followParent=selected;ok=selected!=0;summary="Pai definido · selecione o objeto que vai seguir";error="Selecione um osso ou objeto";}
        // Sem intervalo marcado: segue do tempo atual (o quadro da pegada) ao fim.
        else ok=EditorStudio::followObject(*this,selected,studio_->followParent,state_.clipRangeShown?start:state_.clipTime,end,summary,error);
        state_.clipDiagnostic=ok?summary:error;return true;
      }
      if(code==w::PoseBake) {std::string summary,error;
        if(studio_->bake) {EditorStudio::bakeCancel(*this);state_.clipDiagnostic=studio_->bakeReport;return true;}
        state_.clipDiagnostic=EditorStudio::bakeStart(*this,studio_->bakeRootToOwner,summary,error)?summary:error;return true;}
      if(code==w::PoseExportGlb) {std::string summary,error;
        state_.clipDiagnostic=EditorStudio::exportGlb(*this,summary,error)?summary:error;return true;}
      if(code==w::PoseBakeRoot) {studio_->bakeRootToOwner=!studio_->bakeRootToOwner;
        state_.clipDiagnostic=studio_->bakeRootToOwner?"Bake: root motion vai para o personagem":"Bake: root motion fica no osso";return true;}
      if(code==w::PoseRootMotion) {std::string summary,error;
        state_.clipDiagnostic=EditorStudio::toggleRootMotion(*this,state_.clipOwner,summary,error)?summary:error;return true;}
      if(code==w::PoseIkCreate) {std::string error;const auto tip=state_.clipPreviewTarget?state_.clipPreviewTarget:state_.clipTarget;
        if(createTwoBoneIkForTip(tip,error))state_.clipDiagnostic="IK permanente criado · alvo e direção na cena, ativo em Play";else state_.clipDiagnostic=error;return true;}
      if(code==w::PoseGhost) {state_.clipGhost=!state_.clipGhost;refreshAnimationGhost();return true;}
      if(code==w::TrackFilter) {state_.clipFilterSelected=!state_.clipFilterSelected;refreshClipRowFilter();return true;}
      if(code==w::PoseLibrarySave) {std::string error;if(saveRigPoseFromScene(error))state_.clipDiagnostic="Pose salva na biblioteca";else state_.clipDiagnostic=error;publishClipRigState();return true;}
      if(code==w::PoseMirrorRig) {std::string error;if(mirrorAnimationPoseWithRig(error))state_.clipDiagnostic="Pose espelhada · chave no tempo atual";else state_.clipDiagnostic=error;return true;}
      if(code==w::PoseIkPoleReset) {state_.clipIkPoleSet=false;state_.clipDiagnostic="Direção da dobra volta a seguir a pose";return true;}
      if(code>=w::PoseLibrary&&code<w::PoseLibrary+16) {std::string error;if(applyRigPoseToClip(code-w::PoseLibrary,error))state_.clipDiagnostic="Pose aplicada como chave";else state_.clipDiagnostic=error;return true;}
      if(code==w::PoseIk) {state_.clipIkMode=!state_.clipIkMode;state_.clipDiagnostic=state_.clipIkMode?"IK: arraste o alvo na ponta selecionada":"";return true;}
      if(code==w::BoneTreeUp) {state_.clipBoneScroll=state_.clipBoneScroll>6?state_.clipBoneScroll-6:0;return true;}
      if(code==w::BoneTreeDown) {
        const u32 rows=static_cast<u32>(std::max(0.f,layout_.clipBoneRows));
        state_.clipBoneScroll=std::min(state_.clipBoneScroll+6,layout_.clipBoneCount>rows?layout_.clipBoneCount-rows:0u);return true;
      }
      if(code>=w::BoneRow&&code<w::BoneRow+0x60) {
        const runtime::SceneGraph &graph=clipPreview_.active()?clipPreview_.scene():document_;
        std::vector<std::pair<EditorEntityId,u32>> bones;animationStudioBones(graph,state_.clipOwner,bones);
        const u32 rows=static_cast<u32>(std::max(0.f,layout_.clipBoneRows));
        const u32 first=std::min<u32>(state_.clipBoneScroll,bones.size()>rows?static_cast<u32>(bones.size())-rows:0);
        const u32 index=first+(code-w::BoneRow);
        if(index<bones.size()) {
          std::string error;const auto target=bones[index].first;
          const bool bone=std::find(state_.clipPreviewJoints.begin(),state_.clipPreviewJoints.end(),target)!=state_.clipPreviewJoints.end();
          if(state_.clipPoseShown&&state_.clipPoseSelectionAdd)toggleAnimationPoseTarget(target,error);
          else selectAnimationClipPreviewTarget(target,bone?resources::AnimationPath::Rotation:resources::AnimationPath::Translation,false,error);
          state_.clipDiagnostic=error;refreshClipRowFilter();refreshAnimationClip();revealAnimationClipTrack();if(state_.rigToolOpen)refreshRigTool();
        }
        return true;
      }
    }
  }
  if(clipPoseDraft_&&event.phase==UiPointerPhase::Down&&routing.target!=ui::UiPointerTarget::Viewport) {
    const auto code=animator_widget::code(routing.widgetId);
    const bool poseAction=w::owns(routing.widgetId)&&((code>=w::PoseAutoKey&&code<=w::PoseViewport)||
      code==w::PosePrevious||code==w::PoseNext||code==w::PoseMirrorRig||code==w::PoseIk||code==w::PoseIkPoleReset||code==w::PoseLibrarySave||code==w::TrackFilter||
      (code>=w::PoseLibrary&&code<w::PoseLibrary+16)||(code>=w::StudioTabKey&&code<=w::StudioTabIk)||
      code==w::PoseToolsSelection||code==w::PoseSetSave||code==w::PoseSelectAdd||
      (code>=w::PoseTools&&code<=w::PoseUnlock)||(code>=w::PoseLock&&code<w::PoseLock+resources::MaximumMorphTargets)||
        (code>=w::PoseValue&&code<w::PoseValue+resources::MaximumMorphTargets)||(code>=w::PoseGizmo&&code<w::PoseGizmo+3));
    if(!poseAction) {cancelAnimationClipPose();refreshAnimationClip();}
  }
  const auto *asset=state_.clipAsset;
  if(!asset)return true;
  if(handleAnimationPoseIk(event,routing))return true;
  if(handleAnimationPoseGizmo(event,routing))return true;
  const auto &canvas=layout_.clipCanvas;
  const auto timeAt=[&](float x){const float t=state_.clipStart+(x-canvas.x)/std::max(1.f,canvas.width)*(state_.clipEnd-state_.clipStart);
    return std::clamp(std::round(t*asset->displayRate)/asset->displayRate,0.f,asset->duration);};
  const auto xAt=[&](float t){return canvas.x+(t-state_.clipStart)/(state_.clipEnd-state_.clipStart)*canvas.width;};
  const auto yAt=[&](float v){return canvas.bottom()-static_cast<float>((animationCurveViewValue(state_,state_.clipTrack,state_.clipComponent,v)-state_.clipMinimum)/(double(state_.clipMaximum)-state_.clipMinimum)*canvas.height);};
  if(state_.clipCues&&(routing.widgetId==w::id(w::Canvas)||clipPointer_)) {
    if(clipPointer_&&event.pointerId+1!=clipPointer_)return true;
    if(event.phase==UiPointerPhase::Down) {
      clipPointer_=event.pointerId+1;clipPress_=event.position;state_.clipPlaying=false;state_.clipCueOptions=false;
      clipPressCue_=state_.clipCue;
      float nearest=22;state_.clipCue=0;
      for(const auto &cue:asset->cues) {
        const float row=cue.kind==resources::AnimationCueKind::Event?canvas.y+canvas.height*.3f:canvas.y+canvas.height*.7f;
        const float distance=std::hypot(xAt(cue.time)-event.position.x,row-event.position.y);
        if(distance<nearest) {nearest=distance;state_.clipCue=cue.id;}
      }
      clipPressCues_=state_.clipCueSelection;
      state_.clipSelecting=state_.clipSelectionMode&&!state_.clipCue;
      if(state_.clipCue) {
        if(!state_.clipSelectionMode)state_.clipCueSelection={state_.clipCue};
        else if(std::find(state_.clipCueSelection.begin(),state_.clipCueSelection.end(),state_.clipCue)==state_.clipCueSelection.end()) {
          if(!state_.clipSelectionAdd)state_.clipCueSelection.clear();
          state_.clipCueSelection.push_back(state_.clipCue);
        }
        clipPressCueTime_=asset->cue(state_.clipCue)->time;
      } else if(!state_.clipSelectionAdd)state_.clipCueSelection.clear();
      state_.clipSelectionBox={event.position.x,event.position.y,0,0};
      std::string error;
      seekAnimationClip(state_.clipCue?asset->cue(state_.clipCue)->time:timeAt(event.position.x),error);state_.clipDiagnostic=error;
    } else if(event.phase==UiPointerPhase::Move&&routing.dragging&&state_.clipSelecting) {
      const float x=std::min(clipPress_.x,event.position.x),y=std::min(clipPress_.y,event.position.y);
      state_.clipSelectionBox={x,y,std::abs(event.position.x-clipPress_.x),std::abs(event.position.y-clipPress_.y)};
      auto selected=state_.clipSelectionAdd?clipPressCues_:std::vector<u64>{};
      for(const auto &cue:asset->cues) {
        const ui::UiPoint point{xAt(cue.time),canvas.y+canvas.height*(cue.kind==resources::AnimationCueKind::Event?.3f:.7f)};
        if(state_.clipSelectionBox.contains(point)&&std::find(selected.begin(),selected.end(),cue.id)==selected.end())selected.push_back(cue.id);
      }
      state_.clipCueSelection=std::move(selected);
    } else if(event.phase==UiPointerPhase::Move&&routing.dragging&&state_.clipCue) {
      const auto *live=animationClipAsset(state_.clipGuid);const auto *cue=live?live->cue(state_.clipCue):nullptr;
      if(cue) {auto candidate=*live;std::string error;
        if(resources::transformAnimationCueTimes(candidate,state_.clipCueSelection,0,1,double(timeAt(event.position.x))-clipPressCueTime_,error)) {
          clipDrag_=std::move(candidate);state_.clipAsset=&*clipDrag_;
        } else {clipDrag_.reset();state_.clipAsset=live;}
        state_.clipDiagnostic=error;}
    } else if(event.phase==UiPointerPhase::Up||event.phase==UiPointerPhase::Cancel) {
      clipPointer_=0;state_.clipSelecting=false;
      if(event.phase==UiPointerPhase::Cancel) {state_.clipCueSelection=clipPressCues_;state_.clipCue=clipPressCue_;}
      else if(!state_.clipCueSelection.empty()&&std::find(state_.clipCueSelection.begin(),state_.clipCueSelection.end(),state_.clipCue)==state_.clipCueSelection.end())state_.clipCue=state_.clipCueSelection.front();
      if(clipDrag_) {auto candidate=std::move(*clipDrag_);clipDrag_.reset();std::string error;
        if(event.phase==UiPointerPhase::Up)editAnimationClipAsset(candidate.guid,candidate.revision,[&](auto &value){value=candidate;return true;},error);
        state_.clipDiagnostic=error;refreshAnimationClip();}
    }
    return true;
  }
  if(clipPointer_&&event.pointerId+1!=clipPointer_)return true;
  const u32 widgetCode=animator_widget::code(routing.widgetId);
  if(routing.widgetId==w::id(w::Canvas)||widgetCode==w::HandleIn||widgetCode==w::HandleOut||clipPointer_) {
    if(event.phase==UiPointerPhase::Down) {
      clipPointer_=event.pointerId+1;state_.clipPlaying=false;clipPress_=event.position;
      clipPressSelection_=state_.clipSelection;state_.clipSelecting=false;
      clipHandle_=widgetCode==w::HandleIn?1:widgetCode==w::HandleOut?2:0;
      if(clipHandle_)return true;
      if(!state_.clipCurves) {
        const auto all=rows(*asset,state_.clipLayer,&state_.clipFilterBindings);const usize row=state_.clipRow+static_cast<usize>(std::max(0.f,event.position.y-canvas.y)/30);
        if(row<all.size()&&row<state_.clipRow+layout_.clipVisibleRows) {state_.clipPreviewTarget=0;state_.clipTrack=all[row].track;state_.clipComponent=all[row].component;}
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
        const auto all=rows(*asset,state_.clipLayer,&state_.clipFilterBindings);
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
          const double handleValue=animationCurveAuthorValue(state_,state_.clipTrack,state_.clipComponent,state_.clipMaximum-(event.position.y-canvas.y)/canvas.height*(double(state_.clipMaximum)-state_.clipMinimum));
          if(std::abs(dx)<1e-12&&std::abs(handleValue-key.value)>1e-6) {state_.clipDiagnostic="Handle vertical não tem inclinação finita";return true;}
          const double slope=dx?(handleValue-key.value)/dx:0;
          if(!std::isfinite(slope)||std::abs(slope)>std::numeric_limits<float>::max())return true;
          (incoming?key.inSlope:key.outSlope)=static_cast<float>(slope);(incoming?key.inWeight:key.outWeight)=static_cast<float>(weight);
          (incoming?key.weightedIn:key.weightedOut)=true;(incoming?key.incoming:key.outgoing)=resources::AnimationTangentMode::Free;
          if(!key.broken) {key.inSlope=key.outSlope=static_cast<float>(slope);key.incoming=key.outgoing=resources::AnimationTangentMode::Free;}
        } else key.time=timeAt(xAt(clipPressTime_)+event.position.x-clipPress_.x);
        const auto *track=candidate.track(state_.clipTrack);
        const bool derived=track&&track->rotationMode==resources::AnimationRotationMode::ProgressiveQuaternion&&state_.clipComponent==4;
        if(state_.clipCurves&&!derived&&!clipHandle_)key.value=static_cast<float>(clipPressValue_-(event.position.y-clipPress_.y)/canvas.height*animationCurveAuthorSpan(state_));
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
  if(code==w::Targets)state_.clipAuthorReturn=state_.clipPropertiesShown?1:state_.clipDriversShown?2:0;
  if(handleAnimationDriverWidget(code))return true;
  if(handleAnimationPropertyWidget(code))return true;
  if(code==w::Keys||code==w::Curves||code==w::Pose||code==w::Targets||code==w::Layers||code==w::BakeOpen||code==w::Range||code==w::Edits||code==w::Choose||code==w::New||code==w::Cues)state_.clipPropertiesShown=state_.clipDriversShown=false;
  if(code==w::Keys||code==w::Curves)state_.clipRegionsShown=false;
  if(code==w::Pose||code==w::PoseTools||code==w::Targets||code==w::Layers||code==w::BakeOpen||code==w::Range||code==w::Edits||code==w::Choose||code==w::New||code==w::Cues||code==w::Tangent||code==w::CurveView)state_.clipRegionsShown=false;
  if(code==w::Regions) {
    state_.clipRegionsShown=true;state_.clipRegionDetails=false;state_.clipPlaying=false;
    state_.clipCurveView=state_.clipRangeShown=state_.clipEditPicker=state_.clipTangentPicker=false;
    state_.clipLayersShown=state_.clipLayerPicker=state_.clipBakeShown=state_.clipPoseToolsShown=state_.clipPoseShown=false;
    return true;
  }
  if(code==w::RegionClose) {state_.clipRegionsShown=false;return true;}
  if(code==w::RegionList) {state_.clipRegionDetails=false;return true;}
  if(code==w::RegionPrevious) {if(state_.clipRegionPage)--state_.clipRegionPage;return true;}
  if(code==w::RegionNext) {if((state_.clipRegionPage+1)*3<asset->regions.size())++state_.clipRegionPage;return true;}
  if(code>=w::RegionChoice&&code<w::RegionChoice+3) {
    const usize index=state_.clipRegionPage*3+code-w::RegionChoice;
    if(index<asset->regions.size()) {state_.clipRegion=asset->regions[index].id;state_.clipRegionDetails=true;}
    return true;
  }
  if(code==w::RegionPreview) {previewAnimationClipRegion(state_.clipPreviewRegion==state_.clipRegion?0:state_.clipRegion,error);state_.clipDiagnostic=error;return true;}
  if(code==w::RegionName) {
    if(const auto *region=asset->region(state_.clipRegion)) {
      state_.clipPlaying=false;clipNumberGuid_=asset->guid;clipNumberRevision_=asset->revision;clipNumberKey_=region->id;
      beginAnimatorName(code,region->name);
    }
    return true;
  }
  if(code==w::RegionStart||code==w::RegionEnd) {if(const auto *region=asset->region(state_.clipRegion))beginAnimationClipNumber(code,code==w::RegionStart?region->start:region->end);return true;}
  if(code==w::Pose||code==w::PoseTools||code==w::Targets||code==w::Layers||code==w::BakeOpen||code==w::Range||code==w::Edits||code==w::Choose||code==w::New||code==w::Cues||code==w::Tangent)state_.clipCurveView=false;
  if(code==w::CurveView||code==w::CurveViewClose) {
    state_.clipCurveView=code==w::CurveView&&!state_.clipCurveView;state_.clipRangeShown=state_.clipEditPicker=state_.clipTangentPicker=false;
    if(state_.clipCurveView)state_.clipLayersShown=state_.clipLayerPicker=state_.clipBakeShown=state_.clipPoseToolsShown=state_.clipPoseShown=false;
    return true;
  }
  if(code==w::CurveViewSettings||code==w::CurveViewReferences) {state_.clipCurveViewTask=code==w::CurveViewReferences?1:0;return true;}
  if(code==w::CurveRefPrevious) {if(state_.clipCurveReferencePage)--state_.clipCurveReferencePage;return true;}
  if(code==w::CurveRefNext) {if((state_.clipCurveReferencePage+1)*3<state_.clipCurvePins.size())++state_.clipCurveReferencePage;return true;}
  if(code>=w::CurveRefChoice&&code<w::CurveRefChoice+3) {
    const usize index=state_.clipCurveReferencePage*3+code-w::CurveRefChoice;
    if(index<state_.clipCurvePins.size()) {const auto &pin=state_.clipCurvePins[index];if(const auto *track=asset->track(pin.track)) {state_.clipLayer=track->layer;state_.clipTrack=pin.track;state_.clipComponent=pin.component;state_.clipKey=0;frameAnimationClip();}}
    return true;
  }
  if(code==w::CurvePin) {
    const auto pin=std::find_if(state_.clipCurvePins.begin(),state_.clipCurvePins.end(),[&](const auto &entry){return entry.track==state_.clipTrack&&entry.component==state_.clipComponent;});
    if(pin!=state_.clipCurvePins.end())state_.clipCurvePins.erase(pin);
    else if(state_.clipCurvePins.size()<8)state_.clipCurvePins.push_back({state_.clipTrack,state_.clipComponent,0,1});
    else {state_.clipDiagnostic="Comparação permite até oito canais fixados";return true;}
    frameAnimationClip();return true;
  }
  if(code==w::CurveClearPins) {state_.clipCurvePins.clear();frameAnimationClip();return true;}
  if(code==w::CurveNormalize||code==w::CurveIsolate) {auto &flag=code==w::CurveNormalize?state_.clipCurveNormalized:state_.clipCurveIsolated;flag=!flag;frameAnimationClip();return true;}
  if(code==w::CurveFrameSelection) {frameAnimationClip(true);return true;}
  if(code==w::CurveValueZoomIn||code==w::CurveValueZoomOut||code==w::CurveValueUp||code==w::CurveValueDown) {
    const double center=(double(state_.clipMinimum)+state_.clipMaximum)*.5,span=double(state_.clipMaximum)-state_.clipMinimum;
    const double scale=code==w::CurveValueZoomIn?.5:code==w::CurveValueZoomOut?2:1;
    const double offset=code==w::CurveValueUp?span*.25:code==w::CurveValueDown?-span*.25:0;
    const double low=center+offset-span*scale*.5,high=center+offset+span*scale*.5;
    if(std::isfinite(low)&&std::isfinite(high)&&std::abs(low)<std::numeric_limits<float>::max()&&std::abs(high)<std::numeric_limits<float>::max()&&float(high)>float(low)) {state_.clipMinimum=static_cast<float>(low);state_.clipMaximum=static_cast<float>(high);}return true;
  }
  if(code==w::CurveTimeLeft||code==w::CurveTimeRight) {const float span=state_.clipEnd-state_.clipStart;state_.clipStart=std::clamp(state_.clipStart+(code==w::CurveTimeLeft?-.25f:.25f)*span,0.f,std::max(0.f,asset->duration-span));state_.clipEnd=state_.clipStart+span;return true;}
  if(code==w::PoseTools||code==w::PoseToolsClose) {state_.clipPoseToolsShown=code==w::PoseTools&&!state_.clipPoseToolsShown;state_.clipRangeShown=state_.clipEditPicker=false;return true;}
  if(code==w::PoseToolsRecord||code==w::PoseToolsReuse||code==w::PoseToolsSelection) {state_.clipPoseToolsPage=code==w::PoseToolsSelection?2:code==w::PoseToolsReuse?1:0;return true;}
  if(code==w::PoseSelectAdd) {state_.clipPoseSelectionAdd=!state_.clipPoseSelectionAdd;return true;}
  if(code==w::PoseSelectClear) {selectAnimationPoseTargets({},0,error);state_.clipDiagnostic=error;return true;}
  if(code==w::PoseSelectAll||code==w::PoseSelectParent||code==w::PoseSelectChildren||code==w::PoseSelectSiblings) {
    selectAnimationPoseRelation(code==w::PoseSelectAll?3:code==w::PoseSelectParent?0:code==w::PoseSelectChildren?1:2,error);state_.clipDiagnostic=error;return true;
  }
  if(code==w::PoseSetSave) {
    clipNumberGuid_=state_.clipGuid;clipNumberSceneRevision_=document_.revision();clipPoseNameSelection_=state_.clipPoseSelection;
    beginAnimatorName(code,"Conjunto "+std::to_string(state_.clipPoseSets.size()+1));return true;
  }
  if(code==w::PoseSetPrevious||code==w::PoseSetNext||code==w::PoseSetLoad||code==w::PoseSetDelete) {
    std::vector<std::string> sets;for(const auto &set:state_.clipPoseSets)if(set.clip==state_.clipGuid)sets.push_back(set.name);
    if(sets.empty()) {state_.clipDiagnostic="Este clipe ainda não possui conjuntos salvos";return true;}
    state_.clipPoseSetPage=std::min(state_.clipPoseSetPage,static_cast<u32>(sets.size()-1));
    if(code==w::PoseSetPrevious) {if(state_.clipPoseSetPage)--state_.clipPoseSetPage;}
    else if(code==w::PoseSetNext) {if(state_.clipPoseSetPage+1<sets.size())++state_.clipPoseSetPage;}
    else if(code==w::PoseSetLoad)recallAnimationPoseSelection(sets[state_.clipPoseSetPage],error);
    else removeAnimationPoseSelection(sets[state_.clipPoseSetPage],error);
    state_.clipDiagnostic=error;return true;
  }
  if(code==w::PosePolicy||code==w::PoseArm) {
    auto policy=state_.clipPoseRecording;if(code==w::PosePolicy)policy.createChannels=!policy.createChannels;else policy.rangeArmed=!policy.rangeArmed;
    configureAnimationPoseRecording(policy,error);state_.clipDiagnostic=error;return true;
  }
  if(code==w::PoseArmStart||code==w::PoseArmEnd) {beginAnimationClipNumber(code,code==w::PoseArmStart?state_.clipPoseRecording.start:state_.clipPoseRecording.end);return true;}
  if(code==w::PoseSpace) {state_.clipPoseWorldSpace=!state_.clipPoseWorldSpace;return true;}
  if(code==w::PoseCopy||code==w::PosePaste||code==w::PoseResetIdentity||code==w::PoseResetReference) {
    if(code==w::PoseCopy)copyAnimationClipPose(state_.clipPoseWorldSpace,error);
    else if(code==w::PosePaste)pasteAnimationClipPose(error);
    else resetAnimationClipPose(code==w::PoseResetIdentity,error);
    state_.clipDiagnostic=error;return true;
  }
  if(code==w::PoseUnlock||(code>=w::PoseLock&&code<w::PoseLock+resources::MaximumMorphTargets)) {
    const auto *track=asset->track(state_.clipTrack);if(track) {
      const auto mask=code==w::PoseUnlock?0:state_.clipPoseLocks[static_cast<u32>(track->path)]^(u64(1)<<(code-w::PoseLock));
      configureAnimationPoseLocks(track->path,mask,error);state_.clipDiagnostic=error;
    }
    return true;
  }
  if(code==w::PoseNumbers) {state_.clipPoseNumbers=!state_.clipPoseNumbers;return true;}
  if(code==w::PoseViewport) {state_.clipPreviewExpanded=!state_.clipPreviewExpanded;return true;}
  if(code==w::PoseJoints) {state_.clipPoseJoints=!state_.clipPoseJoints;return true;}
  if(code==w::PoseTranslate||code==w::PoseRotate||code==w::PoseScale) {
    const auto target=state_.clipPreviewTarget?state_.clipPreviewTarget:state_.clipTarget;
    const auto path=code==w::PoseTranslate?resources::AnimationPath::Translation:code==w::PoseRotate?resources::AnimationPath::Rotation:resources::AnimationPath::Scale;
    selectingAnimationPose_=true;
    if(selectAnimationClipPreviewTarget(target,path,true,error))state_.clipPoseIsolated=true;
    selectingAnimationPose_=false;
    state_.clipDiagnostic=error;refreshAnimationClip();return true;
  }
  if(code==w::PoseOrbit||code==w::PosePan||code==w::PoseZoom) {
    state_.navigation=code==w::PoseOrbit?EditorNavigationMode::Orbit:code==w::PosePan?EditorNavigationMode::Pan:EditorNavigationMode::Zoom;return true;
  }
  if(code==w::Restart) {state_.clipPlaying=false;const auto *region=asset->region(state_.clipPreviewRegion);seekAnimationClip(region?region->start:0,error);state_.clipDiagnostic=error;return true;}
  if(code==w::PoseFocus) {
    focusAnimationPoseSelection(error);state_.clipDiagnostic=error;
    return true;
  }
  if(code==w::PoseSelectionIsolate) {isolateAnimationPoseSelection(!state_.clipPoseSelectionIsolated,error);state_.clipDiagnostic=error;return true;}
  if(code==w::PoseAutoKey) {state_.clipPoseAutoKey=!state_.clipPoseAutoKey;return true;}
  if(code==w::PoseRecord) {recordAnimationClipPose(error);state_.clipDiagnostic=error;return true;}
  if(code==w::PoseCancel) {cancelAnimationClipPose();refreshAnimationClip();return true;}
  if(code==w::PoseIsolate) {
    state_.clipPoseIsolated=!state_.clipPoseIsolated;state_.clipPlaying=false;
    if(clipPreview_.active())clipPreview_.seek(state_.clipTime,error);
    state_.clipDiagnostic=error;refreshAnimationClip();return true;
  }
  const auto change=[&](auto operation) {
    state_.clipPlaying=false;const auto *live=animationClipAsset(state_.clipGuid);
    const bool ok=live&&editAnimationClipAsset(live->guid,live->revision,operation,error);
    state_.clipDiagnostic=error;if(ok)refreshAnimationClip();return ok;
  };
  if(code==w::RegionAdd) {
    const float start=state_.clipRangeStart,end=state_.clipRangeEnd;u64 id=0;
    if(change([&](auto &candidate){return candidate.putRegion({0,"Região "+std::to_string(candidate.regions.size()+1),start,end},id,error);})) {
      state_.clipRegion=id;state_.clipRegionDetails=true;
    }
    return true;
  }
  if(code==w::RegionDelete) {const u64 id=state_.clipRegion;change([&](auto &candidate){return candidate.removeRegion(id,error);});return true;}
  if(code==w::Range||code==w::RangeClose) {
    state_.clipRangeShown=code==w::Range&&!state_.clipRangeShown;
    state_.clipEditPicker=state_.clipCueOptions=false;state_.clipPlaying=false;return true;
  }
  if(code==w::RangeStart||code==w::RangeEnd||code==w::RangeDestination) {
    beginAnimationClipNumber(code,code==w::RangeStart?state_.clipRangeStart:code==w::RangeEnd?state_.clipRangeEnd:state_.clipRangeDestination);return true;
  }
  if(code>=w::RangeTaskEdit&&code<=w::RangeTaskArrange) {state_.clipRangeTask=code-w::RangeTaskEdit;return true;}
  if(code==w::RangeCopies||code==w::RangeTolerance) {beginAnimationClipNumber(code,code==w::RangeCopies?state_.clipRangeCopies:state_.clipRangeTolerance);return true;}
  if(code==w::RangeCloseLoop||code==w::RangeRepeat) {
    if(change([&](auto &candidate){return code==w::RangeCloseLoop?resources::closeAnimationRange(candidate,state_.clipRangeStart,state_.clipRangeEnd,error):
        resources::repeatAnimationRange(candidate,state_.clipRangeStart,state_.clipRangeEnd,state_.clipRangeCopies,state_.clipRangeTolerance,error);}))frameAnimationClip();
    return true;
  }
  if(code>=w::RangeAlignStart&&code<=w::RangeDistribute) {
    auto keys=state_.clipCues?std::vector<resources::AnimationKeyAddress>{}:state_.clipSelection;
    auto cues=state_.clipCues?state_.clipCueSelection:std::vector<u64>{};
    if(state_.clipCues&&cues.empty()&&state_.clipCue)cues.push_back(state_.clipCue);
    if(!state_.clipCues&&keys.empty()&&state_.clipKey)keys.push_back({state_.clipTrack,state_.clipComponent,state_.clipKey});
    const auto mode=static_cast<resources::AnimationArrange>(code-w::RangeAlignStart);
    if(change([&](auto &candidate){return resources::arrangeAnimationSelection(candidate,keys,cues,mode,
        mode==resources::AnimationArrange::Distribute?state_.clipRangeStart:state_.clipRangeDestination,state_.clipRangeEnd,error);}))frameAnimationClip();
    return true;
  }
  if(code==w::RangeInsert||code==w::RangeRemove||code==w::RangeDuplicate||code==w::RangeMove) {
    const auto start=state_.clipRangeStart,end=state_.clipRangeEnd,at=state_.clipRangeDestination;
    if(change([&](auto &candidate){return code==w::RangeInsert?resources::insertAnimationTime(candidate,at,end-start,error):
      code==w::RangeRemove?resources::removeAnimationTime(candidate,start,end,error):code==w::RangeMove?resources::moveAnimationRange(candidate,start,end,at,error):resources::duplicateAnimationRange(candidate,start,end,at,error);})) {
      state_.clipRangeShown=false;frameAnimationClip();
    }
    return true;
  }
  if(code==w::CueSelectAll) {
    state_.clipCueSelection.clear();for(const auto &cue:asset->cues)state_.clipCueSelection.push_back(cue.id);
    if(!state_.clipCueSelection.empty())state_.clipCue=state_.clipCueSelection.front();
    state_.clipSelectionMode=true;state_.clipEditPicker=false;return true;
  }
  if(code==w::Cues) {
    state_.clipCues=!state_.clipCues;state_.clipCueOptions=false;state_.clipPlaying=false;
    state_.clipBakeShown=state_.clipLayersShown=state_.clipTangentPicker=state_.clipEditPicker=state_.clipPicker=state_.clipNewPicker=false;
    state_.clipAuthoringPicker=0;return true;
  }
  if(code>=w::CueAddEvent&&code<=w::CueOptionsClose) {
    const auto *cue=asset->cue(state_.clipCue);
    if(code==w::CuePrevious||code==w::CueNext) {
      if(!asset->cues.empty()) {
        auto i=cue?static_cast<usize>(cue-asset->cues.data()):(code==w::CuePrevious?asset->cues.size()-1:0);
        if(cue)i=code==w::CuePrevious?(i?i-1:0):std::min(i+1,asset->cues.size()-1);
        state_.clipCue=asset->cues[i].id;seekAnimationClip(asset->cues[i].time,error);
        state_.clipCueSelection={state_.clipCue};
      }
      return true;
    }
    if(code==w::CueOptions||code==w::CueOptionsClose) {state_.clipCueOptions=code==w::CueOptions&&!state_.clipCueOptions;return true;}
    if(code==w::CueAddEvent||code==w::CueAddMarker||code==w::CueDuplicate) {
      if(code==w::CueDuplicate&&!cue)return true;
      if(code==w::CueDuplicate&&state_.clipCueSelection.size()>1) {
        resources::AnimationCueClipboard clipboard;std::vector<u64> pasted;
        if(clipboard.copy(*asset,state_.clipCueSelection,error)) {
          float at=asset->duration;for(const auto id:state_.clipCueSelection)at=std::min(at,asset->cue(id)->time);
          if(change([&](auto &candidate){return clipboard.paste(candidate,at,pasted,error);})) {
            state_.clipCueSelection=std::move(pasted);state_.clipCue=state_.clipCueSelection.front();
          }
        }
        state_.clipDiagnostic=error;state_.clipCueOptions=false;return true;
      }
      resources::AnimationCue value=cue&&code==w::CueDuplicate?*cue:resources::AnimationCue{};value.id=0;
      if(code!=w::CueDuplicate)value.time=state_.clipTime;
      if(code!=w::CueDuplicate) {value.kind=code==w::CueAddEvent?resources::AnimationCueKind::Event:resources::AnimationCueKind::Marker;value.name=value.kind==resources::AnimationCueKind::Event?"Evento":"Marcador";}
      u64 id=0;if(change([&](auto &candidate){return candidate.putCue(value,id,error);})) {state_.clipCue=id;state_.clipCueSelection={id};}
      state_.clipCueOptions=false;return true;
    }
    if(!cue)return true;
    if(code==w::CueName) {
      state_.clipPlaying=false;clipNumberGuid_=asset->guid;clipNumberRevision_=asset->revision;clipNumberKey_=cue->id;beginAnimatorName(code,cue->name);return true;
    }
    if(code==w::CueTime||code==w::CueTag||code==w::CueValue) {beginAnimationClipNumber(code,code==w::CueTime?cue->time:code==w::CueTag?cue->tag:cue->value);return true;}
    const auto id=cue->id;auto edited=*cue;
    if(code==w::CueDelete) {const auto ids=state_.clipCueSelection.empty()?std::vector<u64>{id}:state_.clipCueSelection;
      change([&](auto &candidate){return resources::eraseAnimationCues(candidate,ids,error);});state_.clipCueOptions=false;return true;}
    if(code==w::CueForward)edited.forward=!edited.forward;else if(code==w::CueReverse)edited.reverse=!edited.reverse;else if(code==w::CueEnabled)edited.enabled=!edited.enabled;else return true;
    const auto ids=state_.clipCueSelection.empty()?std::vector<u64>{id}:state_.clipCueSelection;
    change([&](auto &candidate){
      for(auto &item:candidate.cues)if(std::find(ids.begin(),ids.end(),item.id)!=ids.end()) {
        if(code==w::CueForward)item.forward=edited.forward;else if(code==w::CueReverse)item.reverse=edited.reverse;else item.enabled=edited.enabled;
      }
      return candidate.valid(&error);
    });return true;
  }
  if(code==w::Close) {closeAnimationClip();return true;}
  if(code==w::Layers) {
    state_.clipCues=state_.clipCueOptions=false;
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
  if(code==w::BakeOrder) {
    // Ordem do resultado: mantem a da trilha, depois as seis ordens.
    auto &order=state_.clipBakeSettings.eulerOrder;order=order>=RotationEulerOrderCount?0:order+1;
    state_.clipBakeHasReport=false;return true;
  }
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
    if(state_.clipCues) {
      auto clipboard=std::make_shared<resources::AnimationCueClipboard>();
      const auto ids=state_.clipCueSelection.empty()&&state_.clipCue?std::vector<u64>{state_.clipCue}:state_.clipCueSelection;
      if(clipboard->copy(*asset,ids,error)&&(code==w::CopyKeys||change([&](auto &candidate){return resources::eraseAnimationCues(candidate,ids,error);}))) {
        state_.clipCueClipboard=std::move(clipboard);state_.clipEditPicker=false;
      }
      state_.clipDiagnostic=error;return true;
    }
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
    if(state_.clipCues) {
      const auto clipboard=state_.clipCueClipboard;std::vector<u64> selected;
      if(clipboard&&change([&](auto &candidate){return clipboard->paste(candidate,state_.clipTime,selected,error);})) {
        state_.clipCueSelection=std::move(selected);state_.clipCue=state_.clipCueSelection.front();state_.clipSelectionMode=true;state_.clipEditPicker=false;
      }
      return true;
    }
    const auto clipboard=state_.clipClipboard;std::vector<resources::AnimationKeyAddress> selection;
    const auto mode=code==w::PasteReplace?resources::AnimationPasteMode::Replace:code==w::PasteInsertTracks?resources::AnimationPasteMode::InsertTracks:resources::AnimationPasteMode::InsertAllTracks;
    if(clipboard&&change([&](auto &candidate){return clipboard->paste(candidate,state_.clipTime,mode,selection,error);})) {
      selectKeys(state_,*state_.clipAsset,selection);state_.clipSelectionMode=true;state_.clipEditPicker=false;
      state_.clipLayer=state_.clipAsset->track(selection.front().track)->layer;state_.clipTrack=selection.front().track;state_.clipComponent=selection.front().component;state_.clipKey=selection.front().key;
      const auto all=rows(*state_.clipAsset,state_.clipLayer,&state_.clipFilterBindings);for(usize i=0;i<all.size();++i)if(all[i].track==state_.clipTrack&&all[i].component==state_.clipComponent) {state_.clipRow=static_cast<u32>(i);break;}
      frameAnimationClip();
    }
    return true;
  }
  if(code==w::Selection) {state_.clipSelectionMode=!state_.clipSelectionMode;state_.clipTangentPicker=false;state_.clipModeSide=0;return true;}
  if(code==w::SelectionAdd) {state_.clipSelectionAdd=!state_.clipSelectionAdd;return true;}
  if(code==w::SelectionClear) {state_.clipSelection.clear();state_.clipKey=0;state_.clipCueSelection.clear();state_.clipCue=0;return true;}
  if(code==w::SelectionOffset||code==w::SelectionScale) {beginAnimationClipNumber(code,code==w::SelectionScale?1:0);return true;}
  if(code==w::Name) {
    state_.clipPlaying=false;clipNumberGuid_=asset->guid;clipNumberRevision_=asset->revision;beginAnimatorName(code,asset->name);return true;
  }
  if(code==w::Play) {
    if(!state_.clipPlaying)if(const auto *region=asset->region(state_.clipPreviewRegion);region&&(state_.clipTime<region->start||state_.clipTime>=region->end))seekAnimationClip(region->start,error);
    state_.clipPlaying=!state_.clipPlaying&&clipPreview_.active();state_.clipDiagnostic=error;
  }
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
    state_.clipPoseToolsShown=false;
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
  } else if(code==w::TargetUse) {
    if(state_.clipPoseSelectionAdd) {toggleAnimationPoseTarget(state_.clipTargetBranch,error);state_.clipAuthoringPicker=1;state_.clipDiagnostic=error;}
    else {state_.clipTargetNode=state_.clipTargetBranch;state_.clipAuthoringPicker=2;}
  }
  else if(code>=w::TargetChoice&&code<w::TargetChoice+64) {
    const auto children=document_.childrenOf(state_.clipTargetBranch);const usize index=state_.clipTargetPage*layout_.clipTargetRows+code-w::TargetChoice;
    if(index<children.size()) {
      if(state_.clipPoseSelectionAdd) {toggleAnimationPoseTarget(children[index],error);state_.clipAuthoringPicker=1;state_.clipDiagnostic=error;}
      else {state_.clipTargetNode=children[index];state_.clipAuthoringPicker=2;}
    }
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
      state_.clipPreviewTarget=state_.clipTargetNode;state_.clipTrack=id;state_.clipComponent=0;state_.clipKey=0;state_.clipAuthoringPicker=0;refreshAnimationClip();
      const auto all=rows(*state_.clipAsset,state_.clipLayer,&state_.clipFilterBindings);for(usize i=0;i<all.size();++i)if(all[i].track==id) {state_.clipRow=static_cast<u32>(i);break;}frameAnimationClip();
    }
    state_.clipDiagnostic=error;
  } else if(code==w::Pose) {
    state_.clipPoseShown=!state_.clipPoseShown;state_.clipAuthoringPicker=0;
    if(!state_.clipPoseShown) {
      cancelAnimationClipPose();
      if(clipPreview_.active()&&!clipPreview_.seek(state_.clipTime,error))state_.clipDiagnostic=error;
    }
  }
  else if(code==w::PosePrevious) {if(state_.clipPosePage)--state_.clipPosePage;}
  else if(code==w::PoseNext) {if((state_.clipPosePage+1)*3<state_.clipPoseCount)++state_.clipPosePage;}
  else if(code>=w::PoseValue&&code<w::PoseValue+resources::MaximumMorphTargets) {
    const u32 component=code-w::PoseValue;if(component<state_.clipPoseCount)beginAnimationClipNumber(code,state_.clipPoseValues[component]);
  } else if(code==w::RemoveTrack) {
    if(change([&](auto &candidate){return candidate.removeTrack(state_.clipTrack,error);})) {state_.clipKey=0;frameAnimationClip();}
  }
  else if(code==w::TangentClose) {state_.clipTangentPicker=false;state_.clipModeSide=0;}
  else if(code==w::ModeIn||code==w::ModeOut) {const u32 side=code==w::ModeIn?1:2;state_.clipModeSide=state_.clipModeSide==side?0:side;}
  else if(code==w::Curves||code==w::Keys) {state_.clipCurves=code==w::Curves;state_.clipCurveView=false;state_.clipCues=state_.clipCueOptions=false;frameAnimationClip();}
  else if(code==w::Expand)state_.clipExpanded=!state_.clipExpanded;
  else if(code==w::Frame)frameAnimationClip();
  else if(code==w::ZoomIn||code==w::ZoomOut) {
    const float span=std::clamp((state_.clipEnd-state_.clipStart)*(code==w::ZoomIn?.5f:2.f),1.f/asset->displayRate,std::max(asset->duration,1.f/asset->displayRate));
    state_.clipStart=std::clamp(state_.clipTime-span*.5f,0.f,std::max(0.f,asset->duration-span));state_.clipEnd=state_.clipStart+span;
  } else if(code==w::RowsPrevious)state_.clipRow=state_.clipRow>=layout_.clipVisibleRows?state_.clipRow-layout_.clipVisibleRows:0;
  else if(code==w::RowsNext) {const auto count=rows(*asset,state_.clipLayer,&state_.clipFilterBindings).size();if(state_.clipRow+layout_.clipVisibleRows<count)state_.clipRow+=layout_.clipVisibleRows;}
  else if(code==w::PickerPrevious) {if(state_.clipPage) --state_.clipPage;}
  else if(code==w::PickerNext) {if((state_.clipPage+1)*layout_.clipPickerRows<(state_.clipImportedPicker?state_.clipSourceEntries.size():animationClipAssets_.size())) ++state_.clipPage;}
  else if(code>=w::Row&&code<w::Row+64) {
    const auto all=rows(*asset,state_.clipLayer,&state_.clipFilterBindings);const usize i=state_.clipRow+code-w::Row;
    if(i<all.size()) {state_.clipPreviewTarget=0;state_.clipTrack=all[i].track;state_.clipComponent=all[i].component;state_.clipKey=0;state_.clipPosePage=state_.clipComponent/3;frameAnimationClip();}
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
    resources::AssetGuid guid;if(createAnimationClip(state_.clipOwner,2,guid,error,mode)&&playNewClipOnOwner(state_.clipOwner,guid,error))openAnimationClip(guid,state_.clipOwner,error);state_.clipDiagnostic=error;
  } else if(code==w::Time||code==w::Duration)beginAnimationClipNumber(code,code==w::Time?state_.clipTime:asset->duration);
  else if(code==w::PreviousFrame||code==w::NextFrame) {
    state_.clipPlaying=false;seekAnimationClip(std::clamp(state_.clipTime+(code==w::PreviousFrame?-1.f:1.f)/asset->displayRate,0.f,asset->duration),error);state_.clipDiagnostic=error;
  } else if(code==w::Undo||code==w::Redo) {
    state_.clipPlaying=false;state_.status="Histórico indisponível";
    const bool ok=code==w::Undo?history_.undo(document_):history_.redo(document_);
    // Resource replay supplies the actionable refusal (disk conflict, journal,
    // revision); keep it instead of replacing every failure with a generic label.
    if(!ok)state_.clipDiagnostic=state_.status;else {state_.clipDiagnostic.clear();refreshAnimationClip();}
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
  if(state_.clipAuthorReturn&&(code==w::TargetClose||((code==w::TargetUse||(code>=w::TargetChoice&&code<w::TargetChoice+64))&&state_.clipAuthoringPicker==2))) {
    state_.clipAuthoringPicker=0;
    if(state_.clipAuthorReturn==1){state_.clipPropertiesShown=true;state_.clipPropertyCatalog=true;clipPropertyResourceRevision_=0;refreshAnimationPropertySurface();}
    else {state_.clipDriversShown=true;refreshAnimationDrivers();}
    state_.clipAuthorReturn=0;appearanceChanged_=true;
  }
  return true;
}
} // namespace ae::editor
