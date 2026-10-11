// IK de dois ossos no estudio de animacao. O alvo e arrastado no viewport; o
// resultado vira pose pendente da raiz e do meio pelo mesmo rascunho de pose
// do gizmo, entao Gravar, Cancelar, auto-key e Desfazer funcionam iguais.
#include "editor/editor_session.h"
#include "editor/editor_animation_ik.h"
#include "editor/editor_animation_pose_selection.h"
#include "editor/editor_animation_pose_tools.h"
#include "runtime/transform_math.h"
#include "core/rotation_math.h"
#include <cmath>

namespace ae::editor {
bool EditorSession::applyAnimationPoseIk(EditorEntityId end,const float target[3],std::string &error) {
  error.clear();
  const auto *live=animationClipAsset(state_.clipGuid);
  if(!live||!clipPreview_.active()||isPlaying()||history_.isOpen()) {error="IK exige um clipe aberto em modo pose";return false;}
  if(state_.clipLayer) {error="IK grava na camada Base; selecione a Base";return false;}
  const auto &graph=evaluatedEditorScene();
  const auto *endObject=graph.find(end);const auto *midObject=endObject?graph.find(endObject->parent):nullptr;
  const auto *rootObject=midObject?graph.find(midObject->parent):nullptr;
  if(!rootObject||end==state_.clipOwner||midObject->id==state_.clipOwner) {error="IK precisa de dois ossos acima da ponta";return false;}
  float root[16],mid[16],tip[16];
  if(!runtime::worldMatrix(graph,rootObject->id,root)||!runtime::worldMatrix(graph,midObject->id,mid)||!runtime::worldMatrix(graph,end,tip)) {error="Matrizes da cadeia indisponíveis";return false;}
  TwoBoneIkResult solved;
  if(!solveTwoBoneIk(root,mid,tip,target,solved)) {error="Cadeia degenerada: ossos sem comprimento";return false;}
  float rootParent[16];runtime::Transform rootLocal,midLocal;
  if(!runtime::parentWorldMatrix(graph,rootObject->id,rootParent)||!runtime::localTransformForWorld(solved.root,rootParent,rootLocal)||
     !runtime::localTransformForWorld(solved.mid,solved.root,midLocal)) {error="IK exige TRS local sem shear";return false;}
  auto candidate=clipPoseDraft_?*clipPoseDraft_:*live;
  u64 primary=0;
  const std::pair<const runtime::SceneObject*,const runtime::Transform*> writes[]{{rootObject,&rootLocal},{midObject,&midLocal}};
  for(const auto &[object,local]:writes) {
    resources::AnimationClipBinding binding;
    if(!animationPoseTargetPath(document_,state_.clipOwner,object->id,binding.path,error))return false;
    binding.name=object->name;u64 bindingId=0,id=0;
    for(const auto &item:candidate.bindings)if(item.path==binding.path) {binding=item;bindingId=item.id;break;}
    for(const auto &track:candidate.tracks)if(!track.layer&&bindingId&&track.binding==bindingId&&track.path==resources::AnimationPath::Rotation)id=track.id;
    const auto mode=id?candidate.track(id)->rotationMode:resources::AnimationRotationMode::Quaternion;
    std::vector<float> values;
    if(mode==resources::AnimationRotationMode::Euler) {
      float q[4];runtime::transformRotationQuaternion(*local,q);float euler[3];std::vector<float> current;
      if(!sampleAnimationPoseChannel(*candidate.track(id),state_.clipTime,current)||current.size()<3)return false;
      if(!rotationEulerXYZNear(q,current.data(),euler)) {error="Rotação Euler fora do ramo";return false;}
      values.assign(euler,euler+3);
    } else {float q[4];runtime::transformRotationQuaternion(*local,q);values.assign(q,q+4);}
    if(!id) {
      if(!state_.clipPoseRecording.createChannels) {error="Política de gravação proíbe criar canais de rotação";return false;}
      float q[4];runtime::transformRotationQuaternion(object->transform,q);
      const std::vector<float> initial(q,q+4);
      if(!candidate.addTrack(binding,resources::AnimationPath::Rotation,mode,initial,id,error))return false;
    }
    if(!candidate.putPose(id,state_.clipTime,values,error))return false;
    if(!primary)primary=id;
  }
  if(!stageAnimationClipPoseCandidate(std::move(candidate),primary,error))return false;
  clipPoseMoved_=true;
  if(solved.clamped)state_.clipDiagnostic="Alvo fora do alcance · cadeia esticada";
  return true;
}

bool EditorSession::handleAnimationPoseIk(const ui::UiPointerEvent &event,const ui::UiPointerRouting &routing) {
  using ui::UiPointerPhase;
  if(!state_.clipIkMode||!state_.clipPoseShown||!clipPreview_.active())return false;
  if(clipIkPointer_&&event.pointerId+1!=clipIkPointer_)return true;
  const auto rayPlane=[&](ui::UiPoint point,float out[3]) {
    const auto ray=screenPointToRay(view_,point);if(!ray.valid)return false;
    const float *p=clipIkPlane_,*n=clipIkPlane_+3;
    const float denom=ray.direction[0]*n[0]+ray.direction[1]*n[1]+ray.direction[2]*n[2];
    if(std::abs(denom)<1e-5f)return false;
    const float t=((p[0]-ray.origin[0])*n[0]+(p[1]-ray.origin[1])*n[1]+(p[2]-ray.origin[2])*n[2])/denom;
    if(!(t>0)||!std::isfinite(t))return false;
    for(u32 c=0;c<3;++c)out[c]=ray.origin[c]+ray.direction[c]*t;
    return true;
  };
  if(event.phase==UiPointerPhase::Down) {
    if(routing.target!=ui::UiPointerTarget::Viewport)return false;
    const auto end=state_.clipPreviewTarget?state_.clipPreviewTarget:state_.clipTarget;
    float world[16];if(!end||!runtime::worldMatrix(evaluatedEditorScene(),end,world))return false;
    const auto projected=projectWorldToScreen(view_,world+12);
    // So o toque perto do alvo inicia IK; o resto do viewport continua orbitando.
    if(!projected.valid||std::hypot(projected.screen.x-event.position.x,projected.screen.y-event.position.y)>36)return false;
    const auto ray=screenPointToRay(view_,projected.screen);if(!ray.valid)return false;
    for(u32 c=0;c<3;++c) {clipIkPlane_[c]=world[12+c];clipIkPlane_[3+c]=-ray.direction[c];}
    clipIkPointer_=event.pointerId+1;clipIkEnd_=end;state_.clipIkDragging=true;state_.clipPlaying=false;
    return true;
  }
  if(!clipIkPointer_)return false;
  if(event.phase==UiPointerPhase::Move) {
    float target[3];std::string error;
    if(rayPlane(event.position,target)&&!applyAnimationPoseIk(clipIkEnd_,target,error))state_.clipDiagnostic=error;
    return true;
  }
  if(event.phase==UiPointerPhase::Up||event.phase==UiPointerPhase::Cancel) {
    clipIkPointer_=0;state_.clipIkDragging=false;
    std::string error;
    if(event.phase==UiPointerPhase::Cancel)cancelAnimationClipPose();
    else if(clipPoseMoved_&&state_.clipPoseAutoKey&&!autoRecordAnimationClipPose(error))state_.clipDiagnostic=error;
    clipPoseMoved_=false;
    // O rascunho seleciona o canal da raiz; a ponta continua sendo o alvo do IK.
    if(clipIkEnd_&&!state_.clipPosePending) {std::string ignored;selectAnimationClipPreviewTarget(clipIkEnd_,resources::AnimationPath::Rotation,false,ignored);}
    refreshAnimationClip();revealAnimationClipTrack();
    return true;
  }
  return true;
}
} // namespace ae::editor
