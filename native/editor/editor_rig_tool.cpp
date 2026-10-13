#include "editor/editor_screen.h"
// Rig articulado no Inspector: captura da hierarquia real, controles com
// limite, espelho e poses reutilizáveis. A folha substitui Propriedades; o
// viewport continua com a cena real. Escritas na cena passam pelo History;
// recursos .aerig/.aerpose vão ao registro do projeto como os materiais.
#include "editor/editor_session.h"
#include "editor/editor_studio_state.h"
#include "editor/editor_import_transaction.h"
#include "editor/editor_rig_authoring_candidate.h"
#include "resources/rig_evaluation.h"
#include "core/sha256.h"
#include "core/rotation_math.h"
#include "editor/editor_animation_pose_selection.h"
#include "editor/editor_animation_pose_tools.h"
#include "runtime/transform_math.h"
#include <chrono>
#include <cstdio>
#include <cctype>

namespace ae::editor {
EditorStudioState *createStudioState() {return new EditorStudioState();}
void destroyStudioState(EditorStudioState *state) {delete state;}
namespace {
using resources::RigAsset;
using resources::RigControl;
using resources::RigLimitKind;
std::string safeStem(std::string_view name,std::string_view fallback) {
  std::string stem;
  for(unsigned char c:name) stem.push_back(c<32||c==127||c=='/'||c=='\\'||c==':'||c=='"'||c=='*'||c=='?'||c=='<'||c=='>'||c=='|'?'_':static_cast<char>(c));
  if(stem.empty()||stem=="."||stem=="..") stem=std::string(fallback);
  return stem;
}
std::string channelsText(u8 c) {
  std::string out;
  if(c&resources::RigPosition) out+="Posição ";
  if(c&resources::RigRotation) out+="Rotação ";
  if(c&resources::RigScale) out+="Escala ";
  if(!out.empty()) out.pop_back();
  return out.empty()?"Nenhum":out;
}
std::string degrees(float v) {char b[24];std::snprintf(b,sizeof(b),"%.0f°",v);return b;}
const char *limitName(RigLimitKind k) {return k==RigLimitKind::Hinge?"Dobradiça":k==RigLimitKind::ConeTwist?"Cone e torção":"Livre";}
const char *axisName(const std::array<float,3> &a) {return a[1]>.5f?"Y":a[2]>.5f?"Z":"X";}
template<class T,usize N>T nextIn(const T (&values)[N],T current) {
  for(usize i=0;i<N;++i) if(values[i]==current) return values[(i+1)%N];
  return values[0];
}
} // namespace

bool EditorSession::openRigTool(EditorEntityId root) {
  if(!document_.exists(root)||document_.childrenOf(root).empty()) {state_.status="Selecione a raiz de uma hierarquia articulada.";return false;}
  loadRigForRoot(root);
  state_.rigToolOpen=true;state_.rigTool={};state_.inspectorMenu=false;state_.prefabOverridesOpen=false;
  refreshRigTool();return true;
}

bool EditorSession::loadRigForRoot(EditorEntityId root) {
  if(!document_.exists(root))return false;
  studio_->rigRoot=root;studio_->rigDraft={};studio_->rigPoses.clear();studio_->rigControl=studio_->rigJoint=studio_->rigPose=studio_->rigMirrorLeft=studio_->rigMirrorRight=0;studio_->rigPoseWeight=100;
  studio_->rigCaptured=studio_->rigDirty=false;studio_->rigSyncedTarget=0;
  const auto *entity=document_.find(root);
  const std::string path="Rigs/"+safeStem(entity?entity->name:"","Rig")+".aerig";
  const auto absoluteRoot=EditorImportTransaction::fromUtf8(files_.rootPath());
  // Recurso já publicado para esta raiz: reabre a revisão gravada.
  if(const auto *record=assets_.findByPath(path); record&&record->type==resources::AssetType::Rig) {
    std::filesystem::path absolute;std::vector<u8> bytes;std::string error;
    if(EditorImportTransaction::safePath(absoluteRoot,path,absolute)&&EditorImportTransaction::read(absolute,bytes,RigAsset::MaximumBytes)&&
       RigAsset::deserialize(std::string(bytes.begin(),bytes.end()),studio_->rigDraft,&error)) studio_->rigCaptured=true;
    else studio_->rigDraft={};
  }
  if(studio_->rigCaptured) for(const auto &record:assets_.records()) {
    if(record.type!=resources::AssetType::RigPose) continue;
    std::filesystem::path absolute;std::vector<u8> bytes;resources::RigPose pose;
    if(EditorImportTransaction::safePath(absoluteRoot,record.path,absolute)&&EditorImportTransaction::read(absolute,bytes,RigAsset::MaximumBytes)&&
       resources::RigPose::deserialize(std::string(bytes.begin(),bytes.end()),pose)&&pose.rig==studio_->rigDraft.guid) studio_->rigPoses.push_back(std::move(pose));
  }
  return studio_->rigCaptured;
}

void EditorSession::publishClipRigState() {
  state_.clipRigLoaded=studio_->rigCaptured&&studio_->rigRoot==state_.clipOwner;
  state_.clipRigMirrorPairs=state_.clipRigLoaded?static_cast<u32>(studio_->rigDraft.mirror.size()):0;
  state_.clipPoseLibrary.clear();
  if(state_.clipRigLoaded)for(const auto &pose:studio_->rigPoses)state_.clipPoseLibrary.push_back(pose.name);
}

// Escreve a pose local do rig como rotacoes no rascunho de pose do clipe, so
// nas juntas que mudaram. Gravar/Cancelar/auto-key/Desfazer seguem o gizmo.
bool EditorSession::stageRigLocalPose(std::span<const resources::RigTransform> local,std::string &error) {
  error.clear();
  const auto *live=animationClipAsset(state_.clipGuid);
  if(!live||!clipPreview_.active()||isPlaying()||history_.isOpen()) {error="Abra o clipe em modo pose";return false;}
  if(state_.clipLayer) {error="Pose do rig grava na camada Base";return false;}
  resources::CompiledRig compiled;if(!resources::compileRig(studio_->rigDraft,compiled,error))return false;
  const auto &graph=evaluatedEditorScene();runtime::RigBindings bindings;
  runtime::bindRigToScene(studio_->rigDraft,compiled,graph,studio_->rigRoot,bindings);
  if(local.size()!=compiled.joints.size()||bindings.objects.size()!=compiled.joints.size()) {error="Rig não corresponde à hierarquia do clipe";return false;}
  auto candidate=clipPoseDraft_?*clipPoseDraft_:*live;u64 primary=0;u32 written=0;
  for(usize j=0;j<compiled.joints.size();++j) {
    const auto *object=graph.find(bindings.objects[j]);if(!object||object->id==state_.clipOwner)continue;
    float current[4];runtime::transformRotationQuaternion(object->transform,current);
    const auto &q=local[j].rotation;
    const float similarity=std::abs(current[0]*q[0]+current[1]*q[1]+current[2]*q[2]+current[3]*q[3]);
    if(similarity>.999999f)continue;
    resources::AnimationClipBinding binding;
    if(!animationPoseTargetPath(document_,state_.clipOwner,object->id,binding.path,error))return false;
    binding.name=object->name;u64 bindingId=0,id=0;
    for(const auto &item:candidate.bindings)if(item.path==binding.path) {binding=item;bindingId=item.id;break;}
    for(const auto &track:candidate.tracks)if(!track.layer&&bindingId&&track.binding==bindingId&&track.path==resources::AnimationPath::Rotation)id=track.id;
    const auto mode=id?candidate.track(id)->rotationMode:resources::AnimationRotationMode::Quaternion;
    std::vector<float> values;float value[4]{q[0],q[1],q[2],q[3]};
    if(mode==resources::AnimationRotationMode::Euler) {
      std::vector<float> sampled;float euler[3];
      if(!sampleAnimationPoseChannel(*candidate.track(id),state_.clipTime,sampled)||sampled.size()<3||!resources::eulerFrameAnglesNear(candidate.track(id)->euler,value,sampled.data(),euler)) {error="Rotação Euler fora do ramo";return false;}
      values.assign(euler,euler+3);
    } else values.assign(value,value+4);
    if(!id) {
      if(!state_.clipPoseRecording.createChannels) {error="Política de gravação proíbe criar canais";return false;}
      const std::vector<float> initial(current,current+4);
      if(!candidate.addTrack(binding,resources::AnimationPath::Rotation,mode,initial,id,error))return false;
    }
    if(!candidate.putPose(id,state_.clipTime,values,error))return false;
    if(!primary)primary=id;
    ++written;
  }
  if(!written) {error="A pose já está aplicada neste tempo";return false;}
  if(!stageAnimationClipPoseCandidate(std::move(candidate),primary,error))return false;
  if(state_.clipPoseAutoKey&&!autoRecordAnimationClipPose(error))return false;
  refreshAnimationClip();
  return true;
}

bool EditorSession::mirrorAnimationPoseWithRig(std::string &error) {
  if(!studio_->rigCaptured||studio_->rigRoot!=state_.clipOwner) {error="Publique um rig para este objeto (aba Rig)";return false;}
  if(studio_->rigDraft.mirror.empty()) {error="Defina pares de espelho na aba Rig · Espelho";return false;}
  resources::CompiledRig compiled;if(!resources::compileRig(studio_->rigDraft,compiled,error))return false;
  runtime::RigBindings bindings;runtime::bindRigToScene(studio_->rigDraft,compiled,evaluatedEditorScene(),studio_->rigRoot,bindings);
  std::vector<resources::RigTransform> local(compiled.joints.size()),mirrored(compiled.joints.size());
  if(!runtime::readRigScenePose(compiled,bindings,evaluatedEditorScene(),local,error))return false;
  if(!resources::mirrorRigPose(studio_->rigDraft,compiled,local,mirrored,resources::RigRotation,true,error))return false;
  // Copia o membro selecionado para o lado oposto: so o par correspondente e
  // seus filhos recebem a pose espelhada; o lado de origem fica como esta.
  const auto selected=state_.clipPreviewTarget?state_.clipPreviewTarget:state_.clipTarget;
  i32 source=-1;
  for(usize j=0;j<bindings.objects.size();++j)if(bindings.objects[j]==selected)source=static_cast<i32>(j);
  u64 destinationJoint=0;
  // Sobe ate o ancestral pareado mais alto: o membro inteiro (ombro→pata).
  for(i32 at=source;at>=0;at=compiled.joints[at].parent) {
    for(const auto &pair:studio_->rigDraft.mirror) {
      const auto *a=studio_->rigDraft.control(pair.left),*b=studio_->rigDraft.control(pair.right);if(!a||!b||pair.left==pair.right)continue;
      if(a->joint==compiled.joints[at].id)destinationJoint=b->joint;
      else if(b->joint==compiled.joints[at].id)destinationJoint=a->joint;
    }
  }
  if(!destinationJoint) {error="Selecione um osso de um membro com par de espelho";return false;}
  const auto found=compiled.index.find(destinationJoint);if(found==compiled.index.end()) {error="Par de espelho fora do rig";return false;}
  std::vector<u8> copy(compiled.joints.size(),0);copy[found->second]=1;
  for(usize j=0;j<compiled.joints.size();++j)if(compiled.joints[j].parent>=0&&copy[compiled.joints[j].parent])copy[j]=1;
  for(usize j=0;j<compiled.joints.size();++j)if(!copy[j])mirrored[j]=local[j];
  if(!stageRigLocalPose(mirrored,error))return false;
  std::string ignored;selectAnimationClipPreviewTarget(selected,resources::AnimationPath::Rotation,false,ignored);
  return true;
}

bool EditorSession::applyRigPoseToClip(u32 index,std::string &error) {
  if(!studio_->rigCaptured||studio_->rigRoot!=state_.clipOwner||index>=studio_->rigPoses.size()) {error="Pose indisponível";return false;}
  resources::CompiledRig compiled;if(!resources::compileRig(studio_->rigDraft,compiled,error))return false;
  if(studio_->rigPoses[index].rigRevision!=studio_->rigDraft.revision) {error="Pose de outra revisão do rig; salve de novo";return false;}
  runtime::RigBindings bindings;runtime::bindRigToScene(studio_->rigDraft,compiled,evaluatedEditorScene(),studio_->rigRoot,bindings);
  std::vector<resources::RigTransform> local(compiled.joints.size()),blended(compiled.joints.size());
  if(!runtime::readRigScenePose(compiled,bindings,evaluatedEditorScene(),local,error))return false;
  if(!resources::blendRigPose(compiled,studio_->rigPoses[index],1,local,blended,error))return false;
  return stageRigLocalPose(blended,error);
}

bool EditorSession::saveRigPoseFromScene(std::string &error) {
  error.clear();
  if(!studio_->rigCaptured) {error="Publique o rig antes de salvar poses.";return false;}
  if(studio_->rigDirty) {error="Publique o rig antes de salvar poses.";return false;}
  // No estudio a pose vem do clipe avaliado no tempo atual; fora dele, da cena.
  const runtime::SceneGraph &scene=state_.clipOpen?evaluatedEditorScene():static_cast<const runtime::SceneGraph&>(document_);
  resources::CompiledRig compiled;if(!resources::compileRig(studio_->rigDraft,compiled,error))return false;
  runtime::RigBindings bindings;runtime::bindRigToScene(studio_->rigDraft,compiled,scene,studio_->rigRoot,bindings);
  std::vector<resources::RigTransform> local(compiled.joints.size());
  if(!runtime::readRigScenePose(compiled,bindings,scene,local,error))return false;
  std::vector<u8> channels(local.size(),resources::RigAll);
  std::string name="Pose "+std::to_string(studio_->rigPoses.size()+1);
  std::string path="Rigs/Poses/"+safeStem(studio_->rigDraft.name,"Rig")+" - "+name+".aerpose";
  for(u32 n=2;assets_.findByPath(path);++n) {name="Pose "+std::to_string(studio_->rigPoses.size()+n);path="Rigs/Poses/"+safeStem(studio_->rigDraft.name,"Rig")+" - "+name+".aerpose";}
  const auto guid=resources::assetGuidFromSeed("rig-pose:"+path+":"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
  resources::RigPose pose;
  if(!prepareRigPose(compiled,guid,name,local,channels,pose,error))return false;
  const auto root=EditorImportTransaction::fromUtf8(files_.rootPath());std::filesystem::path absolute;
  if(files_.rootPath().empty()||!EditorImportTransaction::safePath(root,path,absolute)) {error="Caminho da pose indisponível.";return false;}
  const auto text=pose.serialize();resources::AssetRecord record;record.guid=pose.guid;record.type=resources::AssetType::RigPose;record.path=path;
  record.contentHash=Sha256::hex(std::span<const u8>(reinterpret_cast<const u8*>(text.data()),text.size()));
  auto next=assets_;if(!next.add(record)) {error="O registro recusou a pose.";return false;}
  std::error_code fs;std::filesystem::create_directories(absolute.parent_path(),fs);
  if(fs||!EditorImportTransaction::writeText(absolute,text)) {error="Não foi possível gravar "+path+".";return false;}
  assets_=std::move(next);assetRegistryDirty_=true;files_.rebuildTree();
  studio_->rigPoses.push_back(std::move(pose));studio_->rigPose=static_cast<u32>(studio_->rigPoses.size()-1);
  return true;
}

std::vector<EditorEntityId> EditorSession::retargetCandidates() const {
  // Hierarquias articuladas de topo na cena, exceto a do clipe aberto.
  std::vector<EditorEntityId> out;
  std::vector<std::pair<EditorEntityId,u32>> sub;
  for(const auto id:document_.childrenOf(document_.root())) {
    if(id==studio_->rigRoot)continue;
    animationStudioBones(document_,id,sub);
    if(sub.size()>=3)out.push_back(id);
  }
  return out;
}

bool EditorSession::readRigForRoot(EditorEntityId root,resources::RigAsset &out,bool &published,std::string &error) const {
  published=false;const auto *entity=document_.find(root);if(!entity) {error="Destino ausente";return false;}
  const std::string path="Rigs/"+safeStem(entity->name,"Rig")+".aerig";
  if(const auto *record=assets_.findByPath(path); record&&record->type==resources::AssetType::Rig) {
    std::filesystem::path absolute;std::vector<u8> bytes;
    if(EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(files_.rootPath()),path,absolute)&&
       EditorImportTransaction::read(absolute,bytes,RigAsset::MaximumBytes)&&RigAsset::deserialize(std::string(bytes.begin(),bytes.end()),out,&error)) {published=true;return true;}
  }
  // Sem rig publicado: captura transitoria da hierarquia real (referencia = pose atual).
  return prepareRigCapture(document_,root,resources::assetGuidFromSeed(std::string("rig-transitorio:")+entity->name),entity->name,out,error);
}

bool EditorSession::transferAnimationClipTo(EditorEntityId target,std::string &summary,std::string &error) {
  summary.clear();error.clear();
  const auto *live=animationClipAsset(state_.clipGuid);
  if(!live||!clipPreview_.active()||clipPoseDraft_||isPlaying()||history_.isOpen()) {error="Abra o clipe de origem e grave a pose pendente";return false;}
  if(!studio_->rigCaptured||studio_->rigRoot!=state_.clipOwner) {error="Publique o rig da origem na aba Rig";return false;}
  RigAsset destination;bool published=false;
  if(!readRigForRoot(target,destination,published,error))return false;
  const auto stem=[](std::string name) {
    while(!name.empty()&&std::isdigit(static_cast<unsigned char>(name.back())))name.pop_back();
    while(!name.empty()&&(name.back()=='_'||name.back()=='.'))name.pop_back();
    return name;
  };
  // Correspondencia por nome exato, depois por nome sem sufixo numerico.
  resources::RetargetProfile profile;profile.guid=resources::assetGuidFromSeed("retarget:"+studio_->rigDraft.name+":"+destination.name);
  profile.name=studio_->rigDraft.name+" → "+destination.name;profile.source=studio_->rigDraft.guid;profile.target=destination.guid;
  profile.sourceRevision=studio_->rigDraft.revision;profile.targetRevision=destination.revision;profile.allowUnmappedSource=true;
  std::vector<u8> used(studio_->rigDraft.joints.size(),0);
  for(const auto &t:destination.joints) {
    i32 match=-1;
    for(usize s=0;s<studio_->rigDraft.joints.size()&&match<0;++s)if(!used[s]&&studio_->rigDraft.joints[s].name==t.name)match=static_cast<i32>(s);
    for(usize s=0;s<studio_->rigDraft.joints.size()&&match<0;++s)if(!used[s]&&stem(studio_->rigDraft.joints[s].name)==stem(t.name))match=static_cast<i32>(s);
    if(match<0)continue;
    used[match]=1;
    resources::RetargetJoint joint;joint.source=studio_->rigDraft.joints[match].id;joint.target=t.id;profile.mapping.push_back(joint);
  }
  if(profile.mapping.size()<2) {error="Menos de duas juntas correspondem pelo nome entre os rigs";return false;}
  resources::CompiledRetarget program;
  if(!resources::compileRetarget(studio_->rigDraft,destination,profile,program,error))return false;
  resources::RetargetWorkspace scratch;scratch.prepare(program);
  runtime::RigBindings sourceBindings,targetBindings;
  runtime::bindRigToScene(studio_->rigDraft,program.source,clipPreview_.scene(),studio_->rigRoot,sourceBindings);
  runtime::bindRigToScene(destination,program.target,document_,target,targetBindings);
  if(targetBindings.objects.size()!=program.target.joints.size()) {error="Hierarquia do destino não corresponde ao rig";return false;}
  // Amostra o clipe da origem pelo mesmo avaliador de transferencia, a 30 Hz.
  const float duration=live->duration;const u32 samples=std::max(2u,static_cast<u32>(std::ceil(duration*30))+1);
  std::vector<std::vector<resources::RigTransform>> frames(samples,std::vector<resources::RigTransform>(program.target.joints.size()));
  std::vector<resources::RigTransform> sourceLocal(program.source.joints.size());
  const float now=state_.clipTime;std::string seekError;
  for(u32 i=0;i<samples;++i) {
    const float time=std::min(duration,duration*i/float(samples-1));
    if(!clipPreview_.seek(time,seekError)||!runtime::readRigScenePose(program.source,sourceBindings,clipPreview_.scene(),sourceLocal,error)||
       !resources::evaluateRetarget(program,sourceLocal,frames[i],scratch)) {clipPreview_.seek(now,seekError);if(error.empty())error="Falha ao avaliar a transferência";return false;}
  }
  clipPreview_.seek(now,seekError);
  // createAnimationClip pode realocar o armazenamento: nada de "live" daqui em diante.
  const std::string sourceName=live->name;live=nullptr;
  resources::AssetGuid created;
  const bool made=createAnimationClip(target,duration,created,error);
  refreshAnimationClip(); // reaponta state_.clipAsset apos a realocacao
  if(!made)return false;
  const auto *fresh=animationClipAsset(created);if(!fresh) {error="Clipe novo ausente";return false;}
  u32 channels=0;
  const bool ok=editAnimationClipAsset(created,fresh->revision,[&](auto &clip) {
    clip.name=sourceName+" → "+(document_.find(target)?document_.find(target)->name:std::string("destino"));
    for(usize j=0;j<program.target.joints.size();++j) {
      const auto object=targetBindings.objects[j];if(!object||object==target)continue;
      const bool mapped=std::any_of(program.mapping.begin(),program.mapping.end(),[&](const auto &m){return m.target==j;});
      if(!mapped)continue;
      resources::AnimationClipBinding binding;std::string ignored;
      if(!animationPoseTargetPath(document_,target,object,binding.path,ignored))continue;
      binding.name=document_.find(object)->name;
      u64 track=0;const auto &q0=frames[0][j].rotation;const float first[4]{q0[0],q0[1],q0[2],q0[3]};
      if(!clip.addTrack(binding,resources::AnimationPath::Rotation,resources::AnimationRotationMode::Quaternion,first,track,ignored))continue;
      for(u32 i=0;i<samples;++i) {
        const auto &q=frames[i][j].rotation;const float value[4]{q[0],q[1],q[2],q[3]};
        if(!clip.putPose(track,std::min(duration,duration*i/float(samples-1)),value,ignored))return false;
      }
      ++channels;
    }
    return channels>0;
  },error);
  refreshAnimationClip();
  if(!ok) {if(error.empty())error="Nenhuma junta do destino recebeu movimento";return false;}
  summary=std::to_string(profile.mapping.size())+" juntas correspondidas · "+std::to_string(channels)+" canais · "+std::to_string(samples)+" amostras"+(published?"":" · rig do destino capturado na hora");
  return true;
}

bool EditorSession::publishRigDraft(std::string &error) {
  error.clear();
  if(!studio_->rigCaptured) {error="Capture a hierarquia antes de publicar.";return false;}
  if(!studio_->rigDraft.valid(&error)) return false;
  const auto *entity=document_.find(studio_->rigRoot);
  const std::string path="Rigs/"+safeStem(entity?entity->name:studio_->rigDraft.name,"Rig")+".aerig";
  const auto root=EditorImportTransaction::fromUtf8(files_.rootPath());std::filesystem::path absolute;
  if(files_.rootPath().empty()||!EditorImportTransaction::safePath(root,path,absolute)) {error="Rigs precisam de um projeto aberto.";return false;}
  const auto text=studio_->rigDraft.serialize();
  resources::AssetRecord record;record.guid=studio_->rigDraft.guid;record.type=resources::AssetType::Rig;record.path=path;
  record.contentHash=Sha256::hex(std::span<const u8>(reinterpret_cast<const u8*>(text.data()),text.size()));
  auto next=assets_;
  if(const auto *old=next.find(record.guid)) {if(old->path!=path||old->type!=record.type) {error="Outro recurso já usa esta identidade.";return false;} next.remove(record.guid);}
  else if(next.findByPath(path)) {error="Outro recurso ocupa "+path+".";return false;}
  if(!next.add(record)) {error="O registro recusou o rig.";return false;}
  std::error_code fs;std::filesystem::create_directories(absolute.parent_path(),fs);
  if(fs||!EditorImportTransaction::writeText(absolute,text)) {error="Não foi possível gravar "+path+".";return false;}
  assets_=std::move(next);assetRegistryDirty_=true;studio_->rigDirty=false;files_.rebuildTree();
  return true;
}

bool EditorSession::applyRigPoseToScene(std::span<const resources::RigTransform> local,std::span<const u8> channels,
                                        std::string_view label,std::string &error) {
  resources::CompiledRig compiled;if(!resources::compileRig(studio_->rigDraft,compiled,error)) return false;
  runtime::RigBindings bindings;
  if(!runtime::bindRigToScene(studio_->rigDraft,compiled,document_,studio_->rigRoot,bindings)&&bindings.objects.empty()) {error="Hierarquia do rig não encontrada na cena.";return false;}
  std::vector<runtime::RigSceneChange> changes;
  if(!runtime::planRigScenePose(compiled,bindings,document_,local,channels,changes,error)) return false;
  if(changes.empty()) {error="A pose já está aplicada.";return false;}
  if(!history_.begin(label)) {error="Finalize a edição em andamento.";return false;}
  bool ok=true;
  for(const auto &change:changes) {
    const auto *entity=document_.find(change.object);if(!entity) {ok=false;break;}
    auto values=*entity;values.transform=change.after;
    if(!history_.applyValues(document_,change.object,values)) {ok=false;break;}
  }
  history_.end();
  if(!ok) {history_.undo(document_);error="Uma junta recusou a pose; nada foi alterado.";return false;}
  return true;
}

void EditorSession::refreshRigTool() {
  // No estudio de animacao, a junta do rig segue a selecao da arvore/viewport.
  if(state_.clipOpen&&studio_->rigCaptured) {
    const auto target=state_.clipPreviewTarget?state_.clipPreviewTarget:state_.clipTarget;
    if(target&&target!=studio_->rigSyncedTarget) {
      resources::CompiledRig compiled;std::string ignored;runtime::RigBindings bindings;
      if(resources::compileRig(studio_->rigDraft,compiled,ignored)) {
        runtime::bindRigToScene(studio_->rigDraft,compiled,document_,studio_->rigRoot,bindings);
        for(usize i=0;i<bindings.objects.size()&&i<compiled.joints.size();++i) if(bindings.objects[i]==target) {
          for(usize j=0;j<studio_->rigDraft.joints.size();++j) if(studio_->rigDraft.joints[j].id==compiled.joints[i].id) studio_->rigJoint=static_cast<u32>(j);
        }
      }
      studio_->rigSyncedTarget=target;
    }
  }
  auto &ui=state_.rigTool;const auto page=ui.page;const auto route=ui.route;const bool routes=ui.routesOpen;
  ui.fields.clear();ui.error=false;ui.hasPreview=false;ui.canConfirm=false;ui.confirmLabel.clear();
  ui.route=route;ui.page=page;ui.routesOpen=routes;
  const auto *root=document_.find(studio_->rigRoot);
  ui.title=(root?root->name:std::string("Rig"))+(studio_->rigCaptured?" · rev. "+std::to_string(studio_->rigDraft.revision)+" · "+std::to_string(studio_->rigDraft.joints.size())+" juntas":"");
  const auto add=[&](RigToolAction action,std::string name,std::string value,bool enabled=true,ui::UiIcon icon=ui::UiIcon::None) {ui.fields.push_back({action,std::move(name),std::move(value),enabled,icon});};
  if(!studio_->rigCaptured) {
    add(RigToolAction::CaptureRig,"Capturar hierarquia","Juntas reais da seleção, referência = pose atual",true,ui::UiIcon::AnimationSkeleton);
    ui.status="Nenhum rig publicado para esta raiz.";return;
  }
  if(studio_->rigDirty) {ui.confirmLabel="Publicar rig";ui.canConfirm=true;}
  switch(route) {
  case RigToolRoute::Controls: {
    const auto &joint=studio_->rigDraft.joints[std::min<usize>(studio_->rigJoint,studio_->rigDraft.joints.size()-1)];
    add(RigToolAction::NextControl,"Junta "+std::to_string(studio_->rigJoint+1)+" de "+std::to_string(studio_->rigDraft.joints.size()),joint.name);
    const RigControl *control=nullptr;
    for(const auto &c:studio_->rigDraft.controls) if(c.joint==joint.id) control=&c;
    if(!control) {add(RigToolAction::CreateControl,"Criar controle","Rotação local, sem limite",true,ui::UiIcon::AnimationKey);break;}
    add(RigToolAction::ControlChannels,"Canais",channelsText(control->channels));
    add(RigToolAction::ControlSpace,"Espaço",control->space==resources::RigControlSpace::LocalReference?"Referência local":"Espaço do rig");
    add(RigToolAction::LimitKind,"Limite",limitName(control->limit.kind),true,ui::UiIcon::AnimationRotationBranch);
    if(control->limit.kind!=RigLimitKind::Free) {
      add(RigToolAction::LimitAxis,"Eixo do limite",axisName(control->limit.axis));
      add(RigToolAction::LimitMinimum,"Mínimo",degrees(control->limit.minimum));
      add(RigToolAction::LimitMaximum,"Máximo",degrees(control->limit.maximum));
      if(control->limit.kind==RigLimitKind::ConeTwist) add(RigToolAction::LimitCone,"Cone",degrees(control->limit.cone));
    }
    add(RigToolAction::ReferenceCapture,"Referência","Usar pose atual da cena",true,ui::UiIcon::AnimationReferencePose);
    add(RigToolAction::RemoveControl,"Remover controle","Remove também pares de espelho");
    break;
  }
  case RigToolRoute::Mirror: {
    if(studio_->rigDraft.controls.size()<1) {ui.status="Crie controles para formar pares.";break;}
    const auto name=[&](u32 i){const auto &c=studio_->rigDraft.controls[i%studio_->rigDraft.controls.size()];const auto *j=studio_->rigDraft.joint(c.joint);return j?j->name:c.name;};
    add(RigToolAction::MirrorLeft,"Lado A",name(studio_->rigMirrorLeft));
    add(RigToolAction::MirrorRight,"Lado B",name(studio_->rigMirrorRight));
    add(RigToolAction::MirrorPair,"Formar par",std::to_string(studio_->rigDraft.mirror.size())+(studio_->rigDraft.mirror.size()==1?" par no rig":" pares no rig"),true,ui::UiIcon::AnimationRigMirror);
    add(RigToolAction::MirrorSuggest,"Sugerir pares por nome","Left/Right, _L/_R, .L/.R",true,ui::UiIcon::AnimationRigMirror);
    add(RigToolAction::MirrorPlane,"Plano",std::string("Normal ")+axisName(studio_->rigDraft.mirrorNormal));
    add(RigToolAction::MirrorPreview,"Espelhar pose atual",studio_->rigDraft.mirror.empty()?"Forme um par primeiro":"A → B e B → A, com Desfazer",!studio_->rigDraft.mirror.empty(),ui::UiIcon::AnimationRigMirror);
    break;
  }
  case RigToolRoute::Poses: {
    add(RigToolAction::PoseCapture,"Salvar pose atual","Recurso .aerpose reutilizável",!studio_->rigDirty,ui::UiIcon::AnimationRecordPose);
    if(studio_->rigDirty) ui.status="Publique o rig antes de salvar poses.";
    if(!studio_->rigPoses.empty()) {
      const auto &pose=studio_->rigPoses[studio_->rigPose%studio_->rigPoses.size()];
      add(RigToolAction::PoseNext,"Pose "+std::to_string(studio_->rigPose%studio_->rigPoses.size()+1)+" de "+std::to_string(studio_->rigPoses.size()),pose.name);
      add(RigToolAction::PoseWeight,"Peso",std::to_string(studio_->rigPoseWeight)+"%");
      const bool compatible=pose.rigRevision==studio_->rigDraft.revision;
      add(RigToolAction::PoseApply,"Aplicar na cena",compatible?"Com Desfazer":"Revisão "+std::to_string(pose.rigRevision)+" ≠ rig "+std::to_string(studio_->rigDraft.revision),compatible,ui::UiIcon::AnimationReferencePose);
    } else if(!studio_->rigDirty) ui.status="Nenhuma pose salva para este rig.";
    break;
  }
  case RigToolRoute::Retarget: {
    const auto candidates=retargetCandidates();
    if(candidates.empty()) {ui.status="Coloque outro personagem articulado na cena para receber o movimento.";break;}
    const auto target=candidates[studio_->rigRetargetIndex%candidates.size()];
    const auto *entity=document_.find(target);
    add(RigToolAction::RetargetTarget,"Destino ("+std::to_string(studio_->rigRetargetIndex%candidates.size()+1)+" de "+std::to_string(candidates.size())+")",entity?entity->name:"?",true,ui::UiIcon::AnimationRetarget);
    add(RigToolAction::RetargetSource,"Origem",state_.clipOpen?std::string("Clipe aberto: ")+(state_.clipAsset?state_.clipAsset->name:""):"Abra um clipe no estúdio",false,ui::UiIcon::AnimationBinding);
    add(RigToolAction::RetargetBake,"Transferir clipe","Cria um clipe novo no destino, 30 Hz",state_.clipOpen&&!studio_->rigDirty,ui::UiIcon::AnimationBake);
    if(studio_->rigDirty)ui.status="Publique o rig da origem antes de transferir.";
    break;
  }
  case RigToolRoute::Diagnostics: {
    resources::CompiledRig compiled;std::string error;
    if(!resources::compileRig(studio_->rigDraft,compiled,error)) {ui.status=error;ui.error=true;break;}
    runtime::RigBindings bindings;runtime::bindRigToScene(studio_->rigDraft,compiled,document_,studio_->rigRoot,bindings);
    add(RigToolAction::Routes,"Juntas vinculadas",std::to_string(studio_->rigDraft.joints.size()-bindings.issues.size())+" de "+std::to_string(studio_->rigDraft.joints.size()),false);
    for(const auto &issue:bindings.issues) {
      const auto *joint=studio_->rigDraft.joint(issue.joint);
      add(RigToolAction::Routes,joint?joint->name:"Junta "+std::to_string(issue.joint),issue.message,false);
    }
    if(bindings.issues.empty()) ui.status="Todas as juntas resolvidas na cena.";
    break;
  }
  }
}

bool EditorSession::handleRigTool(const ui::UiPointerRouting &routing) {
  if(!routing.tapped||routing.target!=ui::UiPointerTarget::Widget) return false;
  const u32 id=routing.widgetId;
  if(id<static_cast<u32>(RigToolAction::Close)||id>static_cast<u32>(RigToolAction::PoseNext)) return false;
  const auto action=static_cast<RigToolAction>(id);
  if(action==RigToolAction::Open) {openRigTool(state_.inspectorTarget?state_.inspectorTarget:state_.selection);return true;}
  if(!state_.rigToolOpen) return false;
  auto &ui=state_.rigTool;std::string error;
  const auto routeTo=[&](RigToolRoute r){ui.route=r;ui.routesOpen=false;ui.page=0;};
  const auto editControl=[&](const std::function<void(RigControl &)> &change) {
    const auto &joint=studio_->rigDraft.joints[std::min<usize>(studio_->rigJoint,studio_->rigDraft.joints.size()-1)];
    for(const auto &c:studio_->rigDraft.controls) if(c.joint==joint.id) {
      auto value=c;change(value);RigAsset next;u64 assigned=0;
      if(prepareRigControl(studio_->rigDraft,studio_->rigDraft.revision,value,next,assigned,error)) {studio_->rigDraft=std::move(next);studio_->rigDirty=true;}
      return;
    }
  };
  switch(action) {
  case RigToolAction::Close:
    if(studio_->rigDirty&&!ui.error&&ui.status!="Alterações não publicadas · toque × de novo para descartar") {
      ui.status="Alterações não publicadas · toque × de novo para descartar";return true;
    }
    state_.rigToolOpen=false;studio_->rigDraft={};studio_->rigPoses.clear();studio_->rigCaptured=studio_->rigDirty=false;return true;
  case RigToolAction::Routes: if(studio_->rigCaptured) ui.routesOpen=!ui.routesOpen;return true;
  case RigToolAction::RouteControls: routeTo(RigToolRoute::Controls);break;
  case RigToolAction::RouteMirror: routeTo(RigToolRoute::Mirror);break;
  case RigToolAction::RoutePoses: routeTo(RigToolRoute::Poses);break;
  case RigToolAction::RouteRetarget: routeTo(RigToolRoute::Retarget);break;
  case RigToolAction::RouteDiagnostics: routeTo(RigToolRoute::Diagnostics);break;
  case RigToolAction::PreviousPage: if(ui.page) --ui.page;break;
  case RigToolAction::NextPage: ++ui.page;break;
  case RigToolAction::CaptureRig: {
    const auto *root=document_.find(studio_->rigRoot);
    const auto guid=resources::assetGuidFromSeed("rig:"+std::to_string(studio_->rigRoot)+":"+
      std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
    RigAsset captured;
    if(prepareRigCapture(document_,studio_->rigRoot,guid,root?root->name:"Rig",captured,error)) {studio_->rigDraft=std::move(captured);studio_->rigCaptured=studio_->rigDirty=true;}
    break;
  }
  case RigToolAction::NextControl: if(!studio_->rigDraft.joints.empty()) studio_->rigJoint=(studio_->rigJoint+1)%studio_->rigDraft.joints.size();break;
  case RigToolAction::CreateControl: {
    const auto &joint=studio_->rigDraft.joints[std::min<usize>(studio_->rigJoint,studio_->rigDraft.joints.size()-1)];
    RigControl value;value.joint=joint.id;value.name=joint.name;RigAsset next;u64 assigned=0;
    if(prepareRigControl(studio_->rigDraft,studio_->rigDraft.revision,value,next,assigned,error)) {studio_->rigDraft=std::move(next);studio_->rigDirty=true;}
    break;
  }
  case RigToolAction::RemoveControl: {
    const auto &joint=studio_->rigDraft.joints[std::min<usize>(studio_->rigJoint,studio_->rigDraft.joints.size()-1)];
    for(const auto &c:studio_->rigDraft.controls) if(c.joint==joint.id) {
      RigAsset next;if(prepareRigRemoveControl(studio_->rigDraft,studio_->rigDraft.revision,c.id,next,error)) {studio_->rigDraft=std::move(next);studio_->rigDirty=true;}
      break;
    }
    break;
  }
  case RigToolAction::ControlChannels: {
    const u8 options[]{resources::RigRotation,resources::RigPosition|resources::RigRotation,resources::RigAll,resources::RigPosition};
    editControl([&](RigControl &c){c.channels=nextIn(options,c.channels);});break;
  }
  case RigToolAction::ControlSpace:
    editControl([](RigControl &c){c.space=c.space==resources::RigControlSpace::LocalReference?resources::RigControlSpace::RigWorld:resources::RigControlSpace::LocalReference;});break;
  case RigToolAction::LimitKind: {
    const RigLimitKind kinds[]{RigLimitKind::Free,RigLimitKind::Hinge,RigLimitKind::ConeTwist};
    editControl([&](RigControl &c){c.limit.kind=nextIn(kinds,c.limit.kind);if(c.limit.kind==RigLimitKind::Hinge){c.limit.minimum=-90;c.limit.maximum=90;}if(c.limit.kind==RigLimitKind::ConeTwist)c.limit.cone=45;});break;
  }
  case RigToolAction::LimitAxis:
    editControl([](RigControl &c){c.limit.axis=c.limit.axis[0]>.5f?std::array<float,3>{0,1,0}:c.limit.axis[1]>.5f?std::array<float,3>{0,0,1}:std::array<float,3>{1,0,0};});break;
  case RigToolAction::LimitMinimum: {
    const float steps[]{-180,-135,-90,-45,-25,-10,0};
    editControl([&](RigControl &c){c.limit.minimum=std::min(nextIn(steps,c.limit.minimum),c.limit.maximum);});break;
  }
  case RigToolAction::LimitMaximum: {
    const float steps[]{0,10,25,45,90,135,180};
    editControl([&](RigControl &c){c.limit.maximum=std::max(nextIn(steps,c.limit.maximum),c.limit.minimum);});break;
  }
  case RigToolAction::LimitCone: {
    const float steps[]{15,30,45,60,90,120,180};
    editControl([&](RigControl &c){c.limit.cone=nextIn(steps,c.limit.cone);});break;
  }
  case RigToolAction::ReferenceCapture: {
    resources::CompiledRig compiled;
    if(!resources::compileRig(studio_->rigDraft,compiled,error)) break;
    runtime::RigBindings bindings;runtime::bindRigToScene(studio_->rigDraft,compiled,document_,studio_->rigRoot,bindings);
    std::vector<resources::RigTransform> local(compiled.joints.size());
    if(!runtime::readRigScenePose(compiled,bindings,document_,local,error)) break;
    std::vector<u8> channels(local.size(),resources::RigAll);RigAsset next;
    if(prepareRigReference(studio_->rigDraft,studio_->rigDraft.revision,local,channels,next,error)) {studio_->rigDraft=std::move(next);studio_->rigDirty=true;}
    break;
  }
  case RigToolAction::MirrorLeft: if(!studio_->rigDraft.controls.empty()) studio_->rigMirrorLeft=(studio_->rigMirrorLeft+1)%studio_->rigDraft.controls.size();break;
  case RigToolAction::MirrorRight: if(!studio_->rigDraft.controls.empty()) studio_->rigMirrorRight=(studio_->rigMirrorRight+1)%studio_->rigDraft.controls.size();break;
  case RigToolAction::MirrorPair: {
    if(studio_->rigDraft.controls.empty()) break;
    RigAsset next;const auto left=studio_->rigDraft.controls[studio_->rigMirrorLeft%studio_->rigDraft.controls.size()].id,right=studio_->rigDraft.controls[studio_->rigMirrorRight%studio_->rigDraft.controls.size()].id;
    if(prepareRigMirrorPair(studio_->rigDraft,studio_->rigDraft.revision,left,right,next,error)) {studio_->rigDraft=std::move(next);studio_->rigDirty=true;}
    break;
  }
  case RigToolAction::RetargetTarget: ++studio_->rigRetargetIndex;break;
  case RigToolAction::RetargetBake: {
    const auto candidates=retargetCandidates();if(candidates.empty())break;
    std::string summary;
    if(transferAnimationClipTo(candidates[studio_->rigRetargetIndex%candidates.size()],summary,error))ui.status="Transferido · "+summary;
    break;
  }
  case RigToolAction::MirrorSuggest: {
    // Sugestao por nome; pares existentes e centrais ficam como estao.
    const auto swap=[](std::string name) {
      const std::pair<const char*,const char*> pairs[]{{"Left","Right"},{"left","right"},{"_L","_R"},{".L",".R"},{"_l","_r"},{".l",".r"}};
      for(const auto &[a,b]:pairs) {
        if(const auto at=name.find(a);at!=std::string::npos) return name.replace(at,std::string_view(a).size(),b);
        if(const auto at=name.find(b);at!=std::string::npos) return name.replace(at,std::string_view(b).size(),a);
      }
      return std::string();
    };
    u32 added=0;
    for(const auto &left:studio_->rigDraft.controls) {
      const auto *joint=studio_->rigDraft.joint(left.joint);if(!joint)continue;
      // Sufixo numerico de exportadores (b_LeftArm_09 / b_RightArm_06) nao entra na comparacao.
      const auto stem=[](std::string name) {
        while(!name.empty()&&std::isdigit(static_cast<unsigned char>(name.back())))name.pop_back();
        while(!name.empty()&&(name.back()=='_'||name.back()=='.'))name.pop_back();
        return name;
      };
      const auto other=stem(swap(joint->name));if(other.empty())continue;
      for(const auto &right:studio_->rigDraft.controls) {
        const auto *rj=studio_->rigDraft.joint(right.joint);if(!rj||stem(rj->name)!=other||left.id>=right.id)continue;
        bool exists=false;for(const auto &p:studio_->rigDraft.mirror)exists=exists||(p.left==left.id&&p.right==right.id)||(p.left==right.id&&p.right==left.id);
        RigAsset next;std::string ignored;
        if(!exists&&prepareRigMirrorPair(studio_->rigDraft,studio_->rigDraft.revision,left.id,right.id,next,ignored)) {studio_->rigDraft=std::move(next);studio_->rigDirty=true;++added;}
      }
    }
    ui.status=added?std::to_string(added)+" pares sugeridos · publique o rig":"Nenhum par novo encontrado pelos nomes";
    break;
  }
  case RigToolAction::MirrorPlane: {
    const auto n=studio_->rigDraft.mirrorNormal;const std::array<float,3> next=n[0]>.5f?std::array<float,3>{0,1,0}:n[1]>.5f?std::array<float,3>{0,0,1}:std::array<float,3>{1,0,0};
    RigAsset candidate;if(prepareRigMirrorPlane(studio_->rigDraft,studio_->rigDraft.revision,next,studio_->rigDraft.mirrorOffset,candidate,error)) {studio_->rigDraft=std::move(candidate);studio_->rigDirty=true;}
    break;
  }
  case RigToolAction::MirrorPreview: {
    resources::CompiledRig compiled;if(!resources::compileRig(studio_->rigDraft,compiled,error)) break;
    runtime::RigBindings bindings;runtime::bindRigToScene(studio_->rigDraft,compiled,document_,studio_->rigRoot,bindings);
    std::vector<resources::RigTransform> local(compiled.joints.size()),mirrored(compiled.joints.size());
    if(!runtime::readRigScenePose(compiled,bindings,document_,local,error)) break;
    if(!resources::mirrorRigPose(studio_->rigDraft,compiled,local,mirrored,resources::RigAll,true,error)) break;
    std::vector<u8> channels(local.size(),resources::RigAll);
    if(applyRigPoseToScene(mirrored,channels,"Espelhar pose do rig",error)) ui.status="Pose espelhada · Desfazer restaura";
    break;
  }
  case RigToolAction::PoseCapture:
    if(saveRigPoseFromScene(error))ui.status="Pose salva na biblioteca";
    break;
  case RigToolAction::PoseNext: if(!studio_->rigPoses.empty()) studio_->rigPose=(studio_->rigPose+1)%studio_->rigPoses.size();break;
  case RigToolAction::PoseWeight: studio_->rigPoseWeight=studio_->rigPoseWeight>=100?25:studio_->rigPoseWeight+25;break;
  case RigToolAction::PoseApply: {
    if(studio_->rigPoses.empty()) break;
    resources::CompiledRig compiled;if(!resources::compileRig(studio_->rigDraft,compiled,error)) break;
    runtime::RigBindings bindings;runtime::bindRigToScene(studio_->rigDraft,compiled,document_,studio_->rigRoot,bindings);
    std::vector<resources::RigTransform> local(compiled.joints.size()),blended(compiled.joints.size());
    if(!runtime::readRigScenePose(compiled,bindings,document_,local,error)) break;
    if(!resources::blendRigPose(compiled,studio_->rigPoses[studio_->rigPose%studio_->rigPoses.size()],studio_->rigPoseWeight/100.f,local,blended,error)) break;
    std::vector<u8> channels(local.size(),resources::RigAll);
    if(applyRigPoseToScene(blended,channels,"Aplicar pose do rig",error)) ui.status="Pose aplicada a "+std::to_string(studio_->rigPoseWeight)+"% · Desfazer restaura";
    break;
  }
  case RigToolAction::Confirm:
    if(publishRigDraft(error)) ui.status="Rig publicado · "+std::to_string(studio_->rigDraft.joints.size())+" juntas";
    break;
  default: return true;
  }
  refreshRigTool();if(state_.clipOpen)publishClipRigState();
  if(!error.empty()) {ui.status=error;ui.error=true;}
  return true;
}
} // namespace ae::editor
