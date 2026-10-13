#include "editor/editor_session.h"
#include "editor/editor_animation_pose_tools.h"
#include "runtime/transform_math.h"
#include "resources/animation_binding_path.h"
#include "scene/skinned_mesh.h"
#include "core/rotation_math.h"
#include <algorithm>
#include <cmath>

namespace ae::editor {
bool sampleAnimationPoseChannel(const resources::AnimationClipTrack &track,float time,std::vector<float> &values) {
  if(track.path==resources::AnimationPath::Rotation&&track.rotationMode==resources::AnimationRotationMode::Euler) {
    values.resize(3);
    for(u32 i=0;i<3;++i) {
      resources::AnimationCurveSample sampled;
      if(!resources::sampleValidatedAnimationCurve(track.curves[i],time,sampled))return false;
      values[i]=static_cast<float>(sampled.value);
    }
    return true;
  }
  resources::AnimationChannel channel;channel.path=track.path;channel.rotationMode=track.rotationMode;channel.euler=track.euler;
  channel.weightCount=track.weightCount;channel.curves=track.curves;
  values.resize(track.path==resources::AnimationPath::Rotation&&track.rotationMode!=resources::AnimationRotationMode::Euler?4:track.components());
  return resources::sampleValidatedAnimationChannel(channel,time,values);
}
bool animationEulerPoseValues(const resources::AnimationClipTrack &track,float time,const runtime::Transform &pose,std::vector<float> &values) {
  // Trilha no padrao da cena: os graus da pose sao os proprios valores.
  if(track.euler.identity()) {values.assign(pose.rotationDegrees,pose.rotationDegrees+3);return true;}
  std::vector<float> current;float q[4],angles[3];
  if(!sampleAnimationPoseChannel(track,time,current)||current.size()<3)return false;
  runtime::transformRotationQuaternion(pose,q);
  if(!resources::eulerFrameAnglesNear(track.euler,q,current.data(),angles))return false;
  values.assign(angles,angles+3);return true;
}
bool applyAnimationPoseLocks(const resources::AnimationClipTrack &track,float time,u64 mask,
                             std::span<const float> requested,std::vector<float> &out,std::string &error) {
  error.clear();std::vector<float> baseline;
  if(!sampleAnimationPoseChannel(track,time,baseline)||requested.size()!=baseline.size()) {error="Componentes da pose não correspondem ao canal";return false;}
  for(const auto value:requested)if(!std::isfinite(value)) {error="Pose contém valor não finito";return false;}
  auto candidate=std::vector<float>(requested.begin(),requested.end());
  const bool quaternion=track.path==resources::AnimationPath::Rotation&&track.rotationMode!=resources::AnimationRotationMode::Euler;
  const usize count=quaternion?3:candidate.size();
  if(count<64&&(mask>>count)) {error="Trava referencia um eixo ausente";return false;}
  if(mask&&(count==64?mask==~u64(0):mask==((u64(1)<<count)-1))) {error="Todos os eixos deste canal estão travados";return false;}
  if(quaternion&&mask) {
    float zero[]{0,0,0},reference[3],angles[3];
    if(!rotationEulerXYZNear(baseline.data(),zero,reference)||!rotationEulerXYZNear(candidate.data(),reference,angles)) {error="Rotação não pode ser interpretada em XYZ local";return false;}
    for(u32 i=0;i<3;++i)if(mask&(u64(1)<<i))angles[i]=reference[i];
    runtime::Transform pose;std::copy_n(angles,3,pose.rotationDegrees);
    runtime::transformRotationQuaternion(pose,candidate.data());
  } else for(usize i=0;i<candidate.size();++i)if(mask&(u64(1)<<i))candidate[i]=baseline[i];
  out=std::move(candidate);return true;
}
bool EditorSession::configureAnimationPoseRecording(AnimationPoseRecordingPolicy policy,std::string &error,std::optional<bool> autoKey) {
  error.clear();const auto *asset=animationClipAsset(state_.clipGuid);
  if(!state_.clipOpen||!asset||clipPoseGizmo_.active||clipDrag_||!std::isfinite(policy.start)||!std::isfinite(policy.end)||
     policy.start<0||policy.end<policy.start||policy.end>asset->duration) {error="Política de gravação exige clipe aberto e intervalo válido, sem gesto ativo";return false;}
  state_.clipPoseRecording=policy;if(autoKey)state_.clipPoseAutoKey=*autoKey;return true;
}
bool EditorSession::configureAnimationPoseLocks(resources::AnimationPath path,u64 mask,std::string &error) {
  error.clear();const auto *asset=state_.clipAsset;const auto *track=asset?asset->track(state_.clipTrack):nullptr;
  if(!state_.clipOpen||!track||track->path!=path||clipPoseGizmo_.active||clipDrag_) {error="Selecione o canal e conclua o gesto antes de alterar travas";return false;}
  const auto count=path==resources::AnimationPath::Weights?track->weightCount:3;
  if(count<64&&(mask>>count)) {error="Trava referencia um eixo ausente";return false;}
  cancelAnimationClipPose();state_.clipPoseLocks[static_cast<u32>(path)]=mask;refreshAnimationClip();return true;
}
bool EditorSession::autoRecordAnimationClipPose(std::string &error) {
  const auto &policy=state_.clipPoseRecording;
  if(policy.rangeArmed&&(state_.clipTime<policy.start||state_.clipTime>policy.end)) {
    error="Fora do intervalo armado: rascunho mantido; Gravar publica explicitamente";return false;
  }
  return recordAnimationClipPose(error);
}
bool EditorSession::resetAnimationClipPose(bool identity,std::string &error) {
  error.clear();const auto *asset=state_.clipAsset;const auto *track=asset?asset->track(state_.clipTrack):nullptr;
  const auto *object=document_.find(state_.clipTarget);
  if(!track||!object) {error="Selecione um canal com alvo válido";return false;}
  std::vector<float> values;
  if(track->path==resources::AnimationPath::Translation)values=identity?std::vector<float>(3,0):std::vector<float>(object->transform.position,object->transform.position+3);
  else if(track->path==resources::AnimationPath::Scale)values=identity?std::vector<float>(3,1):std::vector<float>(object->transform.scale,object->transform.scale+3);
  else if(track->path==resources::AnimationPath::Rotation) {
    runtime::Transform pose;if(!identity)pose=object->transform;
    if(track->rotationMode==resources::AnimationRotationMode::Euler) {
      if(!animationEulerPoseValues(*track,state_.clipTime,pose,values)) {error="Rotação Euler fora do ramo";return false;}
    }
    else {float q[4];runtime::transformRotationQuaternion(pose,q);values.assign(q,q+4);}
  } else {
    const auto *skin=static_cast<const scene::SkinnedMesh*>(object->components.find(scene::SkinnedMesh::descriptor));
    if(!identity&&(!skin||skin->blendShapeWeights.size()!=track->weightCount)) {error="Pesos de referência não correspondem ao canal";return false;}
    values.resize(track->weightCount);if(!identity)for(u32 i=0;i<track->weightCount;++i)values[i]=skin->blendShapeWeights[i]*.01f;
  }
  const auto id=track->id;auto candidate=*asset;std::vector<float> locked;
  if(!applyAnimationPoseLocks(*candidate.track(id),state_.clipTime,state_.clipPoseLocks[static_cast<u32>(track->path)],values,locked,error)||
     !expandAnimationPoseDraft(candidate,id,locked,error,identity)||!candidate.putPose(id,state_.clipTime,locked,error)||
     !stageAnimationClipPoseCandidate(std::move(candidate),id,error))return false;
  return !state_.clipPoseAutoKey||autoRecordAnimationClipPose(error);
}
bool EditorSession::copyAnimationClipPose(bool world,std::string &error) {
  error.clear();const auto *asset=state_.clipAsset;const auto *track=asset?asset->track(state_.clipTrack):nullptr;
  if(!track||!state_.clipTarget||!clipPreview_.active()) {error="Selecione um canal com alvo válido";return false;}
  AnimationPoseClipboard clipboard;clipboard.world=world;clipboard.path=track->path;clipboard.rotation=track->rotationMode;
  if(world) {
    if(state_.clipLayer||track->path==resources::AnimationPath::Weights) {error="Pose mundial exige a Base e um canal TRS; morph usa cópia local";return false;}
    if(!runtime::worldMatrix(evaluatedEditorScene(),state_.clipTarget,clipboard.worldMatrix.data())) {error="Transformação mundial indisponível";return false;}
  } else if(!sampleAnimationPoseChannel(*track,state_.clipTime,clipboard.values)) {error="Canal não produz uma pose local";return false;}
  clipPoseClipboard_=std::move(clipboard);state_.clipPoseClipboardAvailable=true;state_.clipPoseClipboardWorld=world;state_.clipDiagnostic.clear();return true;
}
bool EditorSession::pasteAnimationClipPose(std::string &error) {
  error.clear();const auto *live=animationClipAsset(state_.clipGuid);
  const auto *track=state_.clipAsset?state_.clipAsset->track(state_.clipTrack):nullptr;
  if(!clipPoseClipboard_||!live||!track||!clipPreview_.active()||isPlaying()||clipPoseGizmo_.active||clipDrag_||history_.isOpen()) {error="Clipboard de pose indisponível ou contexto ocupado";return false;}
  const auto &clipboard=*clipPoseClipboard_;
  if(!clipboard.world) {
    if(track->path!=clipboard.path||(track->path==resources::AnimationPath::Rotation&&track->rotationMode!=clipboard.rotation)) {error="Canal/representação não corresponde à cópia local; conversão exige ação explícita";return false;}
    if(!previewAnimationClipPose(track->id,clipboard.values,error))return false;
  } else {
    if(animationPoseSelectionRoots(document_,state_.clipPoseSelection).size()>1) {error="Colar TRS mundial exige um único alvo; cópia local aplica delta ao grupo";return false;}
    if(state_.clipLayer||track->path==resources::AnimationPath::Weights) {error="Colar pose mundial exige a Base e alvo TRS";return false;}
    float parent[16];runtime::Transform local;
    if(!runtime::parentWorldMatrix(evaluatedEditorScene(),state_.clipTarget,parent)||
       !runtime::localTransformForWorld(clipboard.worldMatrix.data(),parent,local)) {error="Pose mundial exige pai inversível e TRS local sem shear";return false;}
    auto candidate=clipPoseDraft_?*clipPoseDraft_:*live;
    const auto binding=std::find_if(candidate.bindings.begin(),candidate.bindings.end(),[&](const auto &b){return b.id==track->binding;});
    if(binding==candidate.bindings.end()) {error="Binding da pose ausente";return false;}
    const auto bindingValue=*binding;
    for(u32 p=0;p<3;++p) {
      const auto path=static_cast<resources::AnimationPath>(p);u64 id=0;
      for(const auto &item:candidate.tracks)if(!item.layer&&item.binding==track->binding&&item.path==path)id=item.id;
      const auto mode=id?candidate.track(id)->rotationMode:resources::AnimationRotationMode::Quaternion;
      std::vector<float> values;
      if(path==resources::AnimationPath::Translation)values.assign(local.position,local.position+3);
      else if(path==resources::AnimationPath::Scale)values.assign(local.scale,local.scale+3);
      else if(mode==resources::AnimationRotationMode::Euler) {
        if(!animationEulerPoseValues(*candidate.track(id),state_.clipTime,local,values)) {error="Rotação Euler fora do ramo";return false;}
      }
      else {float q[4];runtime::transformRotationQuaternion(local,q);values.assign(q,q+4);}
      if(!id) {
        if(!state_.clipPoseRecording.createChannels) {error="Pose mundial requer os três canais TRS; política proíbe criá-los";return false;}
        const auto *object=evaluatedEditorScene().find(state_.clipTarget);if(!object) {error="Alvo da pose ausente";return false;}
        std::vector<float> initial;
        if(path==resources::AnimationPath::Translation)initial.assign(object->transform.position,object->transform.position+3);
        else if(path==resources::AnimationPath::Scale)initial.assign(object->transform.scale,object->transform.scale+3);
        else {float q[4];runtime::transformRotationQuaternion(object->transform,q);initial.assign(q,q+4);}
        if(!candidate.addTrack(bindingValue,path,mode,initial,id,error))return false;
      }
      std::vector<float> locked;
      if(!applyAnimationPoseLocks(*candidate.track(id),state_.clipTime,state_.clipPoseLocks[p],values,locked,error)||!candidate.putPose(id,state_.clipTime,locked,error))return false;
    }
    if(!stageAnimationClipPoseCandidate(std::move(candidate),state_.clipTrack,error))return false;
  }
  return !state_.clipPoseAutoKey||autoRecordAnimationClipPose(error);
}
}
