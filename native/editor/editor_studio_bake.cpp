// N06: bake da pose final. Reproduz o pipeline do Play (animacao -> root
// motion -> constraints -> IK) quadro a quadro sobre o clipe aberto e grava o
// resultado como um clipe FK novo, verificado reabrindo o clipe com os
// solvers fora de jogo (o preview so toca animacao). Job incremental: avanca
// alguns quadros por update, mostra progresso, cancela sem publicar nada.
#include "editor/editor_studio_tools.h"
#include "editor/editor_session.h"
#include "editor/editor_studio_state.h"
#include "editor/editor_animation_pose_selection.h"
#include "runtime/game_world.h"
#include "runtime/scene_constraints.h"
#include "runtime/scene_ik.h"
#include "runtime/scene_root_motion.h"
#include "runtime/transform_math.h"
#include "core/rotation_math.h"
#include "resources/asset_registry.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace ae::editor {
struct StudioBakeJob {
  runtime::GameWorld world;runtime::SceneConstraints constraints;runtime::SceneIk ik;runtime::SceneRootMotion rootMotion;
  resources::AssetGuid source{};u32 sourceRevision=0;EditorEntityId owner=0;
  std::vector<EditorEntityId> bones;            // subarvore do personagem, sem o dono
  std::vector<float> times;                     // tempos absolutos a 2x a taxa (pares = chaves, impares = verificacao)
  std::vector<std::vector<runtime::Transform>> locals; // por quadro: dono + ossos
  std::vector<std::vector<std::array<float,7>>> ownerSpace; // por quadro: osso no espaco do dono (pos + quat)
  float start=0,end=0,rate=30;u32 next=0;bool rootToOwner=false,ownerAnimated=false;
  float worldStep=0;
};

namespace {
std::array<float,4> quat(const runtime::Transform &t) {float q[4];runtime::transformRotationQuaternion(t,q);return {q[0],q[1],q[2],q[3]};}
bool same(const runtime::Transform &a,const runtime::Transform &b) {
  for(u32 i=0;i<3;++i)if(std::abs(a.position[i]-b.position[i])>1e-5f||std::abs(a.rotationDegrees[i]-b.rotationDegrees[i])>1e-3f)return false;
  return true;
}
// Pose do osso no espaco do dono (posicao + quaternion).
bool ownerSpacePose(const runtime::SceneGraph &g,EditorEntityId owner,EditorEntityId bone,std::array<float,7> &out) {
  float o[16],b[16];runtime::Transform rel;
  if(!runtime::worldMatrix(g,owner,o)||!runtime::worldMatrix(g,bone,b)||!runtime::localTransformForWorld(b,o,rel))return false;
  const auto q=quat(rel);
  out={rel.position[0],rel.position[1],rel.position[2],q[0],q[1],q[2],q[3]};return true;
}
} // namespace

bool EditorStudio::bakeStart(EditorSession &s,bool rootToOwner,std::string &summary,std::string &error) {
  summary.clear();error.clear();
  if(s.studio_->bake) {error="Bake em andamento · toque para cancelar";return false;}
  s.refreshAnimationClip();
  const auto *live=s.animationClipAsset(s.state_.clipGuid);
  if(!live||!s.clipPreview_.active()||s.clipPoseDraft_||s.isPlaying()||s.history_.isOpen()) {error="Grave a pose pendente e mantenha o clipe aberto";return false;}
  if(s.state_.clipLayer) {error="O bake lê a camada Base composta; selecione a Base";return false;}
  auto job=std::make_shared<StudioBakeJob>();
  job->source=s.state_.clipGuid;job->sourceRevision=live->revision;job->owner=s.state_.clipOwner;job->rootToOwner=rootToOwner;
  job->start=s.state_.clipRangeShown?std::min(s.state_.clipRangeStart,s.state_.clipRangeEnd):0.f;
  job->end=s.state_.clipRangeShown?std::max(s.state_.clipRangeStart,s.state_.clipRangeEnd):live->duration;
  job->start=std::clamp(job->start,0.f,live->duration);job->end=std::clamp(job->end,0.f,live->duration);
  if(job->end-job->start<1.f/job->rate) {error="Intervalo curto demais para o bake";return false;}
  const u32 keys=static_cast<u32>(std::ceil((job->end-job->start)*job->rate))+1;
  if(keys>4096) {error="Intervalo longo demais (máximo 4096 quadros)";return false;}
  for(u32 i=0;i<2*keys-1;++i)job->times.push_back(job->start+(job->end-job->start)*float(i)/float(2*keys-2));
  job->worldStep=(job->end-job->start)/float(2*keys-2);
  const auto &preview=s.clipPreview_.scene();
  if(!preview.find(job->owner)) {error="Personagem ausente no preview";return false;}
  std::vector<EditorEntityId> subtree;preview.collectSubtree(job->owner,subtree);
  for(const auto id:subtree)if(id!=job->owner)job->bones.push_back(id);
  for(const auto &binding:live->bindings)if(binding.path.empty()) {
    for(const auto &track:live->tracks)if(track.binding==binding.id)job->ownerAnimated=true;
  }
  if(!job->world.load(preview)) {error="Não foi possível montar o mundo do bake";return false;}
  s.studio_->bake=job;s.studio_->bakeProgress=0;s.studio_->bakeReport.clear();
  summary="Bake iniciado · "+std::to_string(keys)+" quadros";
  return true;
}

void EditorStudio::bakeCancel(EditorSession &s) {
  if(!s.studio_->bake)return;
  s.studio_->bake.reset();s.studio_->bakeProgress=0;s.studio_->bakeReport="Bake cancelado; nada publicado";
  std::string seek;s.clipPreview_.seek(s.state_.clipTime,seek);
}

bool EditorStudio::bakeStep(EditorSession &s,u32 frames,std::string &summary,std::string &error) {
  summary.clear();error.clear();
  auto job=s.studio_->bake;if(!job)return false;
  const auto *live=s.animationClipAsset(job->source);
  if(!live||live->revision!=job->sourceRevision||s.state_.clipGuid!=job->source||!s.clipPreview_.active()) {
    bakeCancel(s);s.studio_->bakeReport=error="Fonte do bake mudou; nada publicado";return false;
  }
  std::string seekError;
  for(u32 n=0;n<frames&&job->next<job->times.size();++n,++job->next) {
    const float t=job->times[job->next];
    if(!s.clipPreview_.seek(t,seekError)) {bakeCancel(s);error="Quadro "+std::to_string(job->next)+": "+seekError;return false;}
    const auto &preview=s.clipPreview_.scene();
    // Pose animada do quadro; o dono so recebe a pose do clipe quando o clipe o anima.
    if(job->next==0||job->ownerAnimated)job->world.setLocalTransform(job->world.handle(job->owner),preview.find(job->owner)->transform);
    for(const auto id:job->bones)if(const auto *o=preview.find(id))job->world.setLocalTransform(job->world.handle(id),o->transform);
    job->world.beginFrame(job->next==0?0.0:job->worldStep);
    if(job->rootToOwner)job->rootMotion.advance(job->world,job->next==0?1.f/60:job->worldStep,nullptr);
    job->constraints.advance(job->world,job->next==0?0.0:job->worldStep);
    job->ik.advance(job->world);
    std::vector<runtime::Transform> frame{job->world.graph().find(job->owner)->transform};
    std::vector<std::array<float,7>> space;
    for(const auto id:job->bones) {
      frame.push_back(job->world.graph().find(id)->transform);
      std::array<float,7> pose{};ownerSpacePose(job->world.graph(),job->owner,id,pose);space.push_back(pose);
    }
    job->locals.push_back(std::move(frame));job->ownerSpace.push_back(std::move(space));
  }
  s.studio_->bakeProgress=float(job->next)/float(job->times.size());
  if(job->next<job->times.size())return true;
  // Publica: chaves nos quadros pares; impares ficam para a verificacao.
  resources::AnimationClipAsset value;
  value.guid=resources::assetGuidFromSeed("bake:"+s.files_.rootPath()+":"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+":"+std::to_string(++s.importInstanceCounter_));
  value.name=live->name+" · bake";value.duration=job->end-job->start;value.displayRate=live->displayRate;
  std::vector<float> keyTimes;for(usize i=0;i<job->times.size();i+=2)keyTimes.push_back(job->times[i]-job->start);
  u32 channels=0;
  const auto keep=[&](usize slot,bool translation) {
    // Canal entra se variou ao longo do bake ou se o clipe original o anima.
    for(usize i=2;i<job->locals.size();i+=2) {
      const auto &a=job->locals[0][slot],&b=job->locals[i][slot];
      if(translation) {for(u32 k=0;k<3;++k)if(std::abs(a.position[k]-b.position[k])>1e-5f)return true;}
      else if(!same(a,b))return true;
    }
    return false;
  };
  for(usize slot=0;slot<=job->bones.size();++slot) {
    const EditorEntityId object=slot==0?job->owner:job->bones[slot-1];
    if(slot==0&&!job->rootToOwner&&!job->ownerAnimated)continue;
    for(const bool translation:{false,true}) {
      if(!keep(slot,translation))continue;
      std::vector<std::array<float,4>> values;
      for(usize i=0;i<job->locals.size();i+=2) {
        const auto &t=job->locals[i][slot];
        if(translation)values.push_back({t.position[0],t.position[1],t.position[2],0});
        else {
          auto q=quat(t);
          // Mesmo hemisferio do quadro anterior: a interpolacao segue o arco curto.
          if(!values.empty()&&q[0]*values.back()[0]+q[1]*values.back()[1]+q[2]*values.back()[2]+q[3]*values.back()[3]<0)for(auto &x:q)x=-x;
          values.push_back(q);
        }
      }
      std::string bindingPath,pathError;
      if(!animationPoseTargetPath(s.document_,job->owner,object,bindingPath,pathError)) {bakeCancel(s);error=pathError;return false;}
      u64 bindingId=0;for(const auto &b:value.bindings)if(b.path==bindingPath)bindingId=b.id;
      if(!bindingId) {
        bindingId=value.nextId++;
        value.bindings.push_back({bindingId,{},bindingPath,s.document_.find(object)?s.document_.find(object)->name:std::string("Objeto")});
      }
      // Trilha montada direto (um clipe vazio ainda nao e valido para addTrack).
      resources::AnimationClipTrack track;track.id=value.nextId++;track.binding=bindingId;
      track.path=translation?resources::AnimationPath::Translation:resources::AnimationPath::Rotation;
      track.rotationMode=resources::AnimationRotationMode::Quaternion;track.curves.resize(track.components());
      for(usize k=0;k<keyTimes.size();++k)for(u32 c=0;c<track.components();++c) {
        resources::AnimationCurveKey key;key.id=value.nextId++;key.time=keyTimes[k];key.value=values[k][c];
        key.incoming=key.outgoing=resources::AnimationTangentMode::Linear;track.curves[c].keys.push_back(key);
      }
      value.tracks.push_back(std::move(track));
      ++channels;
    }
  }
  if(!channels) {
    const auto bones=job->bones.size(),frames=job->locals.size();bakeCancel(s);
    error="Nada a gravar: nenhum osso mudou no intervalo ("+std::to_string(bones)+" ossos, "+std::to_string(frames)+" quadros)";return false;
  }
  std::string stem=value.name;for(auto &c:stem)if(c=='/'||c=='\\'||c==':'||c=='"'||c=='*'||c=='?'||c=='<'||c=='>'||c=='|'||static_cast<unsigned char>(c)<32)c='_';
  std::string path="Clipes/"+stem+".aeclip";
  for(u32 n=2;s.files_.exists(path)||s.assets_.findByPath(path);++n)path="Clipes/"+stem+" "+std::to_string(n)+".aeclip";
  const auto original=job->source;const auto owner=job->owner;
  if(!s.createAnimationClipAsset(value,path,error)) {bakeCancel(s);return false;}
  // Verificacao independente: o clipe novo, tocado sem solvers, nos quadros impares.
  float maxPosition=0,maxAngle=0;
  if(s.openAnimationClip(value.guid,owner,error)) {
    for(usize i=1;i<job->times.size();i+=2) {
      if(!s.clipPreview_.seek(job->times[i]-job->start,seekError))break;
      const auto &solved=job->ownerSpace[i];
      for(usize b=0;b<job->bones.size();++b) {
        std::array<float,7> baked{};if(!ownerSpacePose(s.clipPreview_.scene(),owner,job->bones[b],baked))continue;
        maxPosition=std::max(maxPosition,std::hypot(baked[0]-solved[b][0],baked[1]-solved[b][1],baked[2]-solved[b][2]));
        const float dot=std::abs(baked[3]*solved[b][3]+baked[4]*solved[b][4]+baked[5]*solved[b][5]+baked[6]*solved[b][6]);
        maxAngle=std::max(maxAngle,2*std::acos(std::clamp(dot,0.f,1.f))*57.2957795f);
      }
    }
  }
  std::string reopen;s.openAnimationClip(original,owner,reopen);
  s.studio_->bake.reset();s.studio_->bakeProgress=1;
  char report[220];std::snprintf(report,sizeof report,"Bake publicado · %s · %zu quadros · %u canais · verificação sem solvers: %.1f mm / %.2f°",
                                 path.c_str(),keyTimes.size(),channels,maxPosition*1000,maxAngle);
  s.studio_->bakeReport=summary=report;
  s.studio_->bakeGuid=value.guid;
  return false;
}
} // namespace ae::editor
