// Pacote B: autoria UMotion no jeito Astra (assistente de IK, pinning, Child-Of).
// Tudo grava recursos reais (componentes na cena ou chaves no clipe) por uma
// unica operacao de historico; o que e resolvido usa os solvers do runtime.
#include "editor/editor_studio_tools.h"
#include "editor/editor_session.h"
#include "editor/editor_studio_state.h"
#include "editor/editor_animation_pose_selection.h"
#include "editor/editor_animation_pose_tools.h"
#include "runtime/ik_solvers.h"
#include "runtime/transform_math.h"
#include "core/rotation_math.h"
#include "scene/ik_constraints.h"
#include "scene/root_motion.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace ae::editor {
namespace {
std::string lower(std::string_view text) {
  std::string out(text);for(auto &c:out)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return out;
}
bool contains(const std::string &name,std::initializer_list<const char*> words) {
  for(const char *w:words)if(name.find(w)!=std::string::npos)return true;
  return false;
}
struct Sampled {std::vector<float> values;};
// Chaves de um canal (rotacao quaternion ou translacao) num clipe candidato.
bool writeTrack(resources::AnimationClipAsset &clip,const runtime::SceneGraph &document,EditorEntityId owner,EditorEntityId object,
                resources::AnimationPath path,const std::vector<float> &times,const std::vector<std::array<float,4>> &values,std::string &error) {
  resources::AnimationClipBinding binding;
  if(!animationPoseTargetPath(document,owner,object,binding.path,error))return false;
  binding.name=document.find(object)->name;u64 bindingId=0,id=0;
  for(const auto &item:clip.bindings)if(item.path==binding.path) {binding=item;bindingId=item.id;break;}
  for(const auto &track:clip.tracks)if(!track.layer&&bindingId&&track.binding==bindingId&&track.path==path)id=track.id;
  const bool rotation=path==resources::AnimationPath::Rotation;
  auto mode=id?clip.track(id)->rotationMode:resources::AnimationRotationMode::Quaternion;
  if(!id) {
    std::vector<float> first(values.front().begin(),values.front().begin()+(rotation?4:3));
    if(!clip.addTrack(binding,path,mode,first,id,error))return false;
  }
  float previous[3]{};bool hasPrevious=false;
  for(usize i=0;i<times.size();++i) {
    std::vector<float> key;
    if(rotation&&mode==resources::AnimationRotationMode::Euler) {
      const float q[4]{values[i][0],values[i][1],values[i][2],values[i][3]};float euler[3];
      const auto &frame=clip.track(id)->euler;
      if(hasPrevious?!resources::eulerFrameAnglesNear(frame,q,previous,euler):frame.identity()?!rotationEulerXYZ(q,euler):!resources::eulerFrameAnglesNear(frame,q,previous,euler)) {error="Rotação Euler fora do ramo";return false;}
      std::copy(euler,euler+3,previous);hasPrevious=true;key.assign(euler,euler+3);
    } else key.assign(values[i].begin(),values[i].begin()+(rotation?4:3));
    if(!clip.putPose(id,times[i],key,error))return false;
  }
  return true;
}
std::array<float,4> quaternionOf(const runtime::Transform &t) {float q[4];runtime::transformRotationQuaternion(t,q);return {q[0],q[1],q[2],q[3]};}
} // namespace

bool EditorStudio::setupIk(EditorSession &s,EditorEntityId owner,std::string &summary,std::string &error) {
  summary.clear();error.clear();
  if(s.isPlaying()||s.history_.isOpen()) {error="Pare o Play e conclua a edição";return false;}
  auto &doc=s.document_;
  if(!doc.exists(owner)) {error="Objeto animado ausente";return false;}
  std::vector<EditorEntityId> bones;doc.collectSubtree(owner,bones);
  EditorEntityId pelvis=0,head=0;std::vector<EditorEntityId> hands,feet;
  for(const auto id:bones) {
    const auto *o=doc.find(id);if(!o||id==owner)continue;
    const auto name=lower(o->name);const auto *parent=doc.find(o->parent);const auto parentName=parent?lower(parent->name):std::string();
    if(!pelvis&&contains(name,{"hip","pelvis","quadril","pelve"}))pelvis=id;
    if(!head&&contains(name,{"head","cabeca","cabeça"}))head=id;
    if(contains(name,{"hand","paw","mao","mão","wrist","pulso"})&&!contains(parentName,{"hand","paw","mao","mão"}))hands.push_back(id);
    if(contains(name,{"foot","feet","pe_","_pe","ankle","tornozelo","pata"})&&!contains(parentName,{"foot","feet","ankle","pata"}))feet.push_back(id);
  }
  // Quadrupede: maos na altura dos pes tambem pisam no chao.
  if(!feet.empty()) {
    float lowest=1e30f;for(const auto f:feet){float m[16];if(runtime::worldMatrix(doc,f,m))lowest=std::min(lowest,m[13]);}
    float top=lowest;if(pelvis){float m[16];if(runtime::worldMatrix(doc,pelvis,m))top=m[13];}
    for(auto it=hands.begin();it!=hands.end();) {
      float m[16];
      if(runtime::worldMatrix(doc,*it,m)&&m[13]<=lowest+(top-lowest)*.25f) {feet.push_back(*it);it=hands.erase(it);}
      else ++it;
    }
  }
  // Sem nomes reconheciveis: le a estrutura. Pelve = primeira bifurcacao;
  // folhas mais baixas = pes; folha mais alta acima da pelve = cabeca; as
  // demais folhas com tres ossos ou mais = maos. Vale para robos e criaturas.
  bool structural=false;
  if(hands.empty()&&feet.empty()&&!head) {
    structural=true;
    std::vector<EditorEntityId> leaves;std::vector<EditorEntityId> queue{owner};
    for(usize at=0;at<queue.size();++at) {
      const auto children=doc.childrenOf(queue[at]);
      if(!pelvis&&queue[at]!=owner&&children.size()>=2)pelvis=queue[at];
      if(children.empty()&&queue[at]!=owner)leaves.push_back(queue[at]);
      for(const auto c:children)queue.push_back(c);
    }
    const auto depth=[&](EditorEntityId id){u32 d=0;for(const auto *o=doc.find(id);o&&o->id!=owner;o=doc.find(o->parent))++d;return d;};
    float lowest=1e30f,highest=-1e30f;
    for(const auto l:leaves){float m[16];if(runtime::worldMatrix(doc,l,m)){lowest=std::min(lowest,m[13]);highest=std::max(highest,m[13]);}}
    float pelvisY=lowest;if(pelvis){float m[16];if(runtime::worldMatrix(doc,pelvis,m))pelvisY=m[13];}
    EditorEntityId top=0;float topY=-1e30f;
    for(const auto l:leaves) {
      float m[16];if(!runtime::worldMatrix(doc,l,m)||depth(l)<3)continue;
      if(m[13]<=lowest+(pelvisY-lowest)*.25f)feet.push_back(l);
      else if(m[13]>pelvisY&&m[13]>topY){topY=m[13];top=l;}
    }
    for(const auto l:leaves) {
      float m[16];if(l==top||depth(l)<3||!runtime::worldMatrix(doc,l,m))continue;
      if(std::find(feet.begin(),feet.end(),l)==feet.end())hands.push_back(l);
    }
    head=top;(void)highest;
  }
  if(hands.empty()&&feet.empty()&&!head) {error="Nenhuma mão, pé ou cabeça encontrada pelos nomes nem pela estrutura do esqueleto";return false;}
  if(!s.history_.begin("Assistente de IK")) {error="Conclua a edição em andamento";return false;}
  bool ok=true;u32 limbs=0,grounded=0,looks=0;
  const auto place=[&](EditorEntityId id,const float world[16]) {
    float parent[16];runtime::Transform local;
    return runtime::parentWorldMatrix(doc,id,parent)&&runtime::localTransformForWorld(world,parent,local)&&s.history_.setTransform(doc,id,local);
  };
  const auto chainOk=[&](EditorEntityId tip) {
    const auto *e=doc.find(tip);const auto *m=e?doc.find(e->parent):nullptr;const auto *r=m?doc.find(m->parent):nullptr;
    return r&&m->id!=owner&&r->id!=doc.root();
  };
  for(const auto tip:hands) {
    if(!ok||!chainOk(tip)||doc.find(tip)->components.find(scene::TwoBoneIk::descriptor))continue;
    float tipWorld[16];runtime::worldMatrix(doc,tip,tipWorld);
    const auto target=s.history_.createEntity(doc,owner,EditorEntityKind::Folder,"Alvo IK · "+std::string(doc.find(tip)->name));
    float targetWorld[16]{1,0,0,0,0,1,0,0,0,0,1,0,tipWorld[12],tipWorld[13],tipWorld[14],1};
    ok=target&&place(target,targetWorld);
    if(ok) {auto values=*doc.find(tip);auto *ik=static_cast<scene::TwoBoneIk*>(values.components.add(scene::TwoBoneIk::descriptor));
      ok=ik;if(ok){ik->target=target;ok=s.history_.applyValues(doc,tip,values);}}
    if(ok)++limbs;
  }
  for(const auto foot:feet) {
    if(!ok||!chainOk(foot)||doc.find(foot)->components.find(scene::FootGrounding::descriptor))continue;
    auto values=*doc.find(foot);ok=values.components.add(scene::FootGrounding::descriptor)&&s.history_.applyValues(doc,foot,values);
    if(ok)++grounded;
  }
  if(ok&&grounded&&pelvis&&!doc.find(owner)->components.find(scene::GrounderBody::descriptor)) {
    auto values=*doc.find(owner);auto *g=static_cast<scene::GrounderBody*>(values.components.add(scene::GrounderBody::descriptor));
    ok=g;if(ok){g->pelvis=pelvis;ok=s.history_.applyValues(doc,owner,values);}
  }
  if(ok&&head&&!doc.find(head)->components.find(scene::LookAtIk::descriptor)) {
    float headWorld[16];runtime::worldMatrix(doc,head,headWorld);
    const auto target=s.history_.createEntity(doc,owner,EditorEntityKind::Folder,"Olhar · "+std::string(doc.find(head)->name));
    float forward[3];runtime::ikAxisWorld(headWorld,2,forward);
    float targetWorld[16]{1,0,0,0,0,1,0,0,0,0,1,0,headWorld[12]+forward[0]*2,headWorld[13]+forward[1]*2,headWorld[14]+forward[2]*2,1};
    ok=target&&place(target,targetWorld);
    if(ok) {auto values=*doc.find(head);auto *look=static_cast<scene::LookAtIk*>(values.components.add(scene::LookAtIk::descriptor));
      ok=look;if(ok){look->target=target;look->chainLength=3;ok=s.history_.applyValues(doc,head,values);}}
    if(ok)++looks;
  }
  s.history_.end();
  if(!ok) {s.history_.undo(doc);error="O assistente não conseguiu criar todos os componentes; nada foi alterado";return false;}
  summary=std::string(structural?"Pela estrutura · ":"")+std::to_string(limbs)+" membros com IK · "+std::to_string(grounded)+" pés no chão"+(grounded&&pelvis?" + pelve":"")+(looks?" · cabeça olha um alvo":"");
  return true;
}

EditorEntityId EditorStudio::rootMotionBone(const runtime::SceneGraph &doc,EditorEntityId owner) {
  std::vector<EditorEntityId> bones;doc.collectSubtree(owner,bones);
  for(const auto id:bones) {
    const auto *o=doc.find(id);if(!o||id==owner)continue;
    if(contains(lower(o->name),{"hip","pelvis","quadril","pelve"}))return id;
  }
  // Sem nomes: a primeira bifurcacao abaixo do personagem (onde pernas e
  // coluna se separam); sem bifurcacao, o primeiro filho.
  std::vector<EditorEntityId> queue{owner};
  for(usize at=0;at<queue.size();++at) {
    const auto children=doc.childrenOf(queue[at]);
    if(queue[at]!=owner&&children.size()>=2)return queue[at];
    for(const auto c:children)queue.push_back(c);
  }
  const auto children=doc.childrenOf(owner);
  return children.empty()?0:children.front();
}

bool EditorStudio::toggleRootMotion(EditorSession &s,EditorEntityId owner,std::string &summary,std::string &error) {
  summary.clear();error.clear();
  if(s.isPlaying()||s.history_.isOpen()) {error="Pare o Play e conclua a edição";return false;}
  auto &doc=s.document_;
  if(!doc.exists(owner)) {error="Objeto animado ausente";return false;}
  auto values=*doc.find(owner);
  if(values.components.find(scene::RootMotion::descriptor)) {
    auto *existing=static_cast<scene::RootMotion*>(values.components.edit(scene::RootMotion::descriptor));
    existing->enabled=!existing->enabled;
    if(!s.history_.begin("Root motion"))return false;
    const bool ok=s.history_.applyValues(doc,owner,values);s.history_.end();
    if(!ok) {s.history_.undo(doc);error="Não foi possível alterar o root motion";return false;}
    summary=existing->enabled?"Root motion ligado":"Root motion desligado · a animação fica no lugar";
    return true;
  }
  const auto bone=rootMotionBone(doc,owner);
  if(!bone) {error="Nenhum osso abaixo do personagem";return false;}
  auto *c=static_cast<scene::RootMotion*>(values.components.add(scene::RootMotion::descriptor));
  if(!c) {error="Root motion não cabe neste objeto";return false;}
  c->bone=bone;
  if(!s.history_.begin("Root motion"))return false;
  const bool ok=s.history_.applyValues(doc,owner,values);s.history_.end();
  if(!ok) {s.history_.undo(doc);error="Não foi possível criar o root motion";return false;}
  summary="Root motion no personagem · osso "+std::string(doc.find(bone)->name)+" · ativo em Play";
  return true;
}

bool EditorStudio::pinTip(EditorSession &s,EditorEntityId tip,float start,float end,std::string &summary,std::string &error) {
  summary.clear();error.clear();
  // O preview precisa refletir a revisao atual do clipe antes de amostrar.
  s.refreshAnimationClip();
  const auto *live=s.animationClipAsset(s.state_.clipGuid);
  if(!live||!s.clipPreview_.active()||s.clipPoseDraft_||s.isPlaying()||s.history_.isOpen()) {error="Grave a pose pendente e mantenha o clipe aberto";return false;}
  if(s.state_.clipLayer) {error="Fixar grava na camada Base";return false;}
  start=std::clamp(start,0.f,live->duration);end=std::clamp(end,0.f,live->duration);
  if(end<=start) {error="Intervalo vazio: defina início e fim";return false;}
  const auto &graph=s.clipPreview_.scene();
  const auto *e=graph.find(tip);const auto *m=e?graph.find(e->parent):nullptr;const auto *r=m?graph.find(m->parent):nullptr;
  if(!r||m->id==s.state_.clipOwner||r->id==graph.root()) {error="Selecione a ponta de uma cadeia com dois ossos acima";return false;}
  const EditorEntityId mid=m->id,root=r->id;
  float pinned[16];if(!runtime::worldMatrix(graph,tip,pinned)) {error="Ponta indisponível";return false;}
  const u32 frames=std::max(2u,static_cast<u32>(std::ceil((end-start)*30))+1);
  std::vector<float> times;std::vector<std::array<float,4>> rootValues,midValues;
  const float now=s.state_.clipTime;std::string seekError;
  for(u32 i=0;i<frames;++i) {
    const float t=start+(end-start)*i/float(frames-1);times.push_back(t);
    float rw[16],mw[16],tw[16],parent[16];runtime::TwoBoneIkResult solved;runtime::Transform rl,ml;
    if(!s.clipPreview_.seek(t,seekError)||!runtime::worldMatrix(s.clipPreview_.scene(),root,rw)||!runtime::worldMatrix(s.clipPreview_.scene(),mid,mw)||
       !runtime::worldMatrix(s.clipPreview_.scene(),tip,tw)||!runtime::solveTwoBoneIk(rw,mw,tw,pinned+12,solved)||
       !runtime::parentWorldMatrix(s.clipPreview_.scene(),root,parent)||!runtime::localTransformForWorld(solved.root,parent,rl)||
       !runtime::localTransformForWorld(solved.mid,solved.root,ml)) {s.clipPreview_.seek(now,seekError);error="Quadro "+std::to_string(i)+": cadeia sem solução";return false;}
    rootValues.push_back(quaternionOf(rl));midValues.push_back(quaternionOf(ml));
  }
  s.clipPreview_.seek(now,seekError);
  const auto owner=s.state_.clipOwner;const auto guid=s.state_.clipGuid;
  // Passes gravados sem historico; no fim, um unico registro original->final.
  const auto original=*live;
  const auto write=[&]() {
    const auto *current=s.animationClipAsset(guid);if(!current) {error="Clipe ausente";return false;}
    auto candidate=*current;
    if(!candidate.edit(current->revision,[&](resources::AnimationClipAsset &clip) {
      return writeTrack(clip,s.document_,owner,root,resources::AnimationPath::Rotation,times,rootValues,error)&&
             writeTrack(clip,s.document_,owner,mid,resources::AnimationPath::Rotation,times,midValues,error);
    },error))return false;
    const bool committed=candidate.revision==current->revision||s.commitAnimationClipAsset(candidate,error,false);
    s.refreshAnimationClip();return committed;
  };
  bool ok=write();
  if(!ok)return false;
  // Verificacao no clipe gravado: cada quadro e reavaliado; o que ficou fora
  // da ponta fixa e resolvido de novo a partir da pose gravada (ate 3 passes).
  float worst=0;
  for(u32 pass=0;pass<3;++pass) {
    worst=0;bool changed=false;
    for(u32 i=0;i<frames;++i) {
      float rw[16],mw[16],tw[16],parent[16];runtime::TwoBoneIkResult solved;runtime::Transform rl,ml;
      if(!s.clipPreview_.seek(times[i],seekError)||!runtime::worldMatrix(s.clipPreview_.scene(),root,rw)||!runtime::worldMatrix(s.clipPreview_.scene(),mid,mw)||
         !runtime::worldMatrix(s.clipPreview_.scene(),tip,tw))break;
      const float miss=std::sqrt((tw[12]-pinned[12])*(tw[12]-pinned[12])+(tw[13]-pinned[13])*(tw[13]-pinned[13])+(tw[14]-pinned[14])*(tw[14]-pinned[14]));
      worst=std::max(worst,miss);
      if(miss<5e-4f)continue;
      if(!runtime::solveTwoBoneIk(rw,mw,tw,pinned+12,solved)||!runtime::parentWorldMatrix(s.clipPreview_.scene(),root,parent)||
         !runtime::localTransformForWorld(solved.root,parent,rl)||!runtime::localTransformForWorld(solved.mid,solved.root,ml))continue;
      rootValues[i]=quaternionOf(rl);midValues[i]=quaternionOf(ml);changed=true;
    }
    s.clipPreview_.seek(now,seekError);
    if(!changed)break;
    if(!write())break;
  }
  // Volta ao original sem historico e publica o final com um unico Desfazer.
  {
    const auto *current=s.animationClipAsset(guid);
    if(!current) {error="Clipe ausente";return false;}
    auto result=*current;auto back=original;back.revision=current->revision+1;back.nextId=std::max(back.nextId,current->nextId);
    if(!s.commitAnimationClipAsset(back,error,false))return false;
    result.revision=back.revision+1;result.nextId=std::max(result.nextId,back.nextId);
    if(!s.commitAnimationClipAsset(result,error,true))return false;
    s.refreshAnimationClip();
  }
  char residual[48];std::snprintf(residual,sizeof residual," · desvio máx. %.1f mm",worst*1000);
  summary="Ponta fixa de "+std::to_string(start).substr(0,4)+" s a "+std::to_string(end).substr(0,4)+" s · "+std::to_string(frames)+" quadros em 2 ossos"+residual;
  return true;
}

bool EditorStudio::followObject(EditorSession &s,EditorEntityId object,EditorEntityId parentObject,float start,float end,std::string &summary,std::string &error) {
  summary.clear();error.clear();
  // O preview precisa refletir a revisao atual do clipe antes de amostrar.
  s.refreshAnimationClip();
  const auto *live=s.animationClipAsset(s.state_.clipGuid);
  if(!live||!s.clipPreview_.active()||s.clipPoseDraft_||s.isPlaying()||s.history_.isOpen()) {error="Grave a pose pendente e mantenha o clipe aberto";return false;}
  if(s.state_.clipLayer) {error="Seguir grava na camada Base";return false;}
  if(object==parentObject||!s.clipPreview_.scene().find(object)||!s.clipPreview_.scene().find(parentObject)) {error="Escolha outro objeto como pai";return false;}
  if(s.clipPreview_.scene().isDescendantOf(parentObject,object)) {error="O pai não pode estar dentro do objeto";return false;}
  start=std::clamp(start,0.f,live->duration);end=std::clamp(end,0.f,live->duration);
  if(end<=start) {error="Intervalo vazio: defina início e fim";return false;}
  float objectWorld[16],parentWorld[16],offset[16];runtime::Transform offsetPose;
  if(!runtime::worldMatrix(s.clipPreview_.scene(),object,objectWorld)||!runtime::worldMatrix(s.clipPreview_.scene(),parentObject,parentWorld)||
     !runtime::localTransformForWorld(objectWorld,parentWorld,offsetPose)) {error="Pose do pai sem inversa";return false;}
  // Deslocamento atual do objeto no espaco do pai (mantido durante o intervalo).
  runtime::transformMatrix(offsetPose,offset);
  const u32 frames=std::max(2u,static_cast<u32>(std::ceil((end-start)*30))+1);
  std::vector<float> times;std::vector<std::array<float,4>> positions,rotations;
  const float now=s.state_.clipTime;std::string seekError;
  for(u32 i=0;i<frames;++i) {
    const float t=start+(end-start)*i/float(frames-1);times.push_back(t);
    float pw[16],desired[16],holder[16];runtime::Transform local;
    if(!s.clipPreview_.seek(t,seekError)||!runtime::worldMatrix(s.clipPreview_.scene(),parentObject,pw)) {s.clipPreview_.seek(now,seekError);error="Pai indisponível no quadro";return false;}
    runtime::multiplyMatrix(pw,offset,desired);
    if(!runtime::parentWorldMatrix(s.clipPreview_.scene(),object,holder)||!runtime::localTransformForWorld(desired,holder,local)) {s.clipPreview_.seek(now,seekError);error="Pose local sem solução";return false;}
    positions.push_back({local.position[0],local.position[1],local.position[2],0});rotations.push_back(quaternionOf(local));
  }
  s.clipPreview_.seek(now,seekError);
  const auto owner=s.state_.clipOwner;
  const bool ok=s.editAnimationClipAsset(s.state_.clipGuid,live->revision,[&](resources::AnimationClipAsset &clip) {
    return writeTrack(clip,s.document_,owner,object,resources::AnimationPath::Translation,times,positions,error)&&
           writeTrack(clip,s.document_,owner,object,resources::AnimationPath::Rotation,times,rotations,error);
  },error);
  s.refreshAnimationClip();
  if(!ok)return false;
  summary=std::string(s.clipPreview_.scene().find(object)->name)+" segue "+s.clipPreview_.scene().find(parentObject)->name+" · "+std::to_string(frames)+" quadros";
  return true;
}
} // namespace ae::editor
