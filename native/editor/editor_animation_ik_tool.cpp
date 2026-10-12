// IK de dois ossos no estudio de animacao. O alvo e arrastado no viewport; o
// resultado vira pose pendente da raiz e do meio pelo mesmo rascunho de pose
// do gizmo, entao Gravar, Cancelar, auto-key e Desfazer funcionam iguais.
#include "editor/editor_session.h"
#include "editor/editor_studio_state.h"
#include "editor/editor_animation_ik.h"
#include "editor/editor_animation_pose_selection.h"
#include "editor/editor_animation_pose_tools.h"
#include "runtime/transform_math.h"
#include "core/rotation_math.h"
#include "resources/rig_evaluation.h"
#include "runtime/rig_instance.h"
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
  if(!solveTwoBoneIk(root,mid,tip,target,solved,state_.clipIkPoleSet?studio_->clipIkPole:nullptr)) {error="Cadeia degenerada: ossos sem comprimento";return false;}
  float rootParent[16];runtime::Transform rootLocal,midLocal;
  if(!runtime::parentWorldMatrix(graph,rootObject->id,rootParent)||!runtime::localTransformForWorld(solved.root,rootParent,rootLocal)||
     !runtime::localTransformForWorld(solved.mid,solved.root,midLocal)) {error="IK exige TRS local sem shear";return false;}
  auto candidate=clipPoseDraft_?*clipPoseDraft_:*live;
  u64 primary=0;bool limited=false;
  const std::pair<const runtime::SceneObject*,const runtime::Transform*> writes[]{{rootObject,&rootLocal},{midObject,&midLocal}};
  for(const auto &[object,local]:writes) {
    resources::AnimationClipBinding binding;
    if(!animationPoseTargetPath(document_,state_.clipOwner,object->id,binding.path,error))return false;
    binding.name=object->name;u64 bindingId=0,id=0;
    for(const auto &item:candidate.bindings)if(item.path==binding.path) {binding=item;bindingId=item.id;break;}
    for(const auto &track:candidate.tracks)if(!track.layer&&bindingId&&track.binding==bindingId&&track.path==resources::AnimationPath::Rotation)id=track.id;
    const auto mode=id?candidate.track(id)->rotationMode:resources::AnimationRotationMode::Quaternion;
    std::vector<float> values;
    float q[4];runtime::transformRotationQuaternion(*local,q);
    // Limites do rig publicado para esta hierarquia (dobradica/cone) valem no IK.
    if(studio_->rigCaptured) {
      resources::CompiledRig compiled;std::string ignored;runtime::RigBindings bindings;
      if(resources::compileRig(studio_->rigDraft,compiled,ignored)) {
        runtime::bindRigToScene(studio_->rigDraft,compiled,document_,studio_->rigRoot,bindings);
        for(usize j=0;j<bindings.objects.size()&&j<compiled.joints.size();++j)if(bindings.objects[j]==object->id) {
          const auto &joint=compiled.joints[j];
          if(joint.limit.kind!=resources::RigLimitKind::Free) {
            std::array<float,4> rotation{q[0],q[1],q[2],q[3]};
            if(resources::constrainRigRotation(joint.limit,joint.reference.rotation,rotation)) {
              for(u32 c=0;c<4;++c)q[c]=rotation[c];
              limited=true;
            }
          }
        }
      }
    }
    if(mode==resources::AnimationRotationMode::Euler) {
      float euler[3];std::vector<float> current;
      if(!sampleAnimationPoseChannel(*candidate.track(id),state_.clipTime,current)||current.size()<3)return false;
      if(!rotationEulerXYZNear(q,current.data(),euler)) {error="Rotação Euler fora do ramo";return false;}
      values.assign(euler,euler+3);
    } else values.assign(q,q+4);
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
  if(limited)state_.clipDiagnostic="Limite do rig aplicado · a ponta para no ângulo permitido";
  else if(solved.clamped)state_.clipDiagnostic="Alvo fora do alcance · cadeia esticada";
  return true;
}

bool EditorSession::handleAnimationPoseIk(const ui::UiPointerEvent &event,const ui::UiPointerRouting &routing) {
  using ui::UiPointerPhase;
  if(!state_.clipIkMode||!state_.clipPoseShown||!clipPreview_.active())return false;
  if(studio_->clipIkPointer&&event.pointerId+1!=studio_->clipIkPointer)return true;
  const auto rayPlane=[&](ui::UiPoint point,float out[3]) {
    const auto ray=screenPointToRay(view_,point);if(!ray.valid)return false;
    const float *p=studio_->clipIkPlane,*n=studio_->clipIkPlane+3;
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
    // A ponta tem prioridade: so um toque longe dela (e perto do joelho) move a direcao.
    const auto tipPoint=projectWorldToScreen(view_,world+12);
    const bool nearTip=tipPoint.valid&&std::hypot(tipPoint.screen.x-event.position.x,tipPoint.screen.y-event.position.y)<=36;
    // Toque perto do joelho/cotovelo arrasta o alvo de direcao (pole).
    if(const auto *endNode=nearTip?nullptr:evaluatedEditorScene().find(end)) {
      float mid[16];
      if(runtime::worldMatrix(evaluatedEditorScene(),endNode->parent,mid)) {
        const auto knee=projectWorldToScreen(view_,mid+12);
        if(knee.valid&&std::hypot(knee.screen.x-event.position.x,knee.screen.y-event.position.y)<=26) {
          const auto ray=screenPointToRay(view_,knee.screen);if(!ray.valid)return false;
          for(u32 c=0;c<3;++c) {studio_->clipIkPlane[c]=mid[12+c];studio_->clipIkPlane[3+c]=-ray.direction[c];studio_->clipIkPole[c]=mid[12+c];}
          studio_->clipIkPointer=event.pointerId+1;studio_->clipIkEnd=end;studio_->clipIkPoleDrag=true;state_.clipIkDragging=true;state_.clipPlaying=false;
          return true;
        }
      }
    }
    const auto projected=projectWorldToScreen(view_,world+12);
    // So o toque perto do alvo inicia IK; o resto do viewport continua orbitando.
    if(!projected.valid||std::hypot(projected.screen.x-event.position.x,projected.screen.y-event.position.y)>36)return false;
    const auto ray=screenPointToRay(view_,projected.screen);if(!ray.valid)return false;
    for(u32 c=0;c<3;++c) {studio_->clipIkPlane[c]=world[12+c];studio_->clipIkPlane[3+c]=-ray.direction[c];}
    studio_->clipIkPointer=event.pointerId+1;studio_->clipIkEnd=end;state_.clipIkDragging=true;state_.clipPlaying=false;
    return true;
  }
  if(!studio_->clipIkPointer)return false;
  if(event.phase==UiPointerPhase::Move) {
    float target[3];std::string error;
    if(!rayPlane(event.position,target))return true;
    if(studio_->clipIkPoleDrag) {
      // A ponta fica onde esta; so a direcao da dobra muda.
      float tip[16];if(!runtime::worldMatrix(evaluatedEditorScene(),studio_->clipIkEnd,tip))return true;
      for(u32 c=0;c<3;++c)studio_->clipIkPole[c]=target[c];
      state_.clipIkPoleSet=true;
      if(!applyAnimationPoseIk(studio_->clipIkEnd,tip+12,error))state_.clipDiagnostic=error;
      return true;
    }
    if(!applyAnimationPoseIk(studio_->clipIkEnd,target,error))state_.clipDiagnostic=error;
    return true;
  }
  if(event.phase==UiPointerPhase::Up||event.phase==UiPointerPhase::Cancel) {
    studio_->clipIkPointer=0;state_.clipIkDragging=false;studio_->clipIkPoleDrag=false;
    std::string error;
    if(event.phase==UiPointerPhase::Cancel)cancelAnimationClipPose();
    else if(clipPoseMoved_&&state_.clipPoseAutoKey&&!autoRecordAnimationClipPose(error))state_.clipDiagnostic=error;
    clipPoseMoved_=false;
    // O rascunho seleciona o canal da raiz; a ponta continua sendo o alvo do IK.
    if(studio_->clipIkEnd&&!state_.clipPosePending) {std::string ignored;selectAnimationClipPreviewTarget(studio_->clipIkEnd,resources::AnimationPath::Rotation,false,ignored);}
    refreshAnimationClip();revealAnimationClipTrack();
    return true;
  }
  return true;
}
} // namespace ae::editor
