// Rig articulado no Inspector: captura da hierarquia real, controles com
// limite, espelho e poses reutilizáveis. A folha substitui Propriedades; o
// viewport continua com a cena real. Escritas na cena passam pelo History;
// recursos .aerig/.aerpose vão ao registro do projeto como os materiais.
#include "editor/editor_session.h"
#include "editor/editor_import_transaction.h"
#include "editor/editor_rig_authoring_candidate.h"
#include "resources/rig_evaluation.h"
#include "core/sha256.h"
#include <chrono>
#include <cstdio>

namespace ae::editor {
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
  rigRoot_=root;rigDraft_={};rigPoses_.clear();rigControl_=rigJoint_=rigPose_=rigMirrorLeft_=rigMirrorRight_=0;rigPoseWeight_=100;
  rigCaptured_=rigDirty_=false;
  const auto *entity=document_.find(root);
  const std::string path="Rigs/"+safeStem(entity?entity->name:"","Rig")+".aerig";
  const auto absoluteRoot=EditorImportTransaction::fromUtf8(files_.rootPath());
  // Recurso já publicado para esta raiz: reabre a revisão gravada.
  if(const auto *record=assets_.findByPath(path); record&&record->type==resources::AssetType::Rig) {
    std::filesystem::path absolute;std::vector<u8> bytes;std::string error;
    if(EditorImportTransaction::safePath(absoluteRoot,path,absolute)&&EditorImportTransaction::read(absolute,bytes,RigAsset::MaximumBytes)&&
       RigAsset::deserialize(std::string(bytes.begin(),bytes.end()),rigDraft_,&error)) rigCaptured_=true;
    else rigDraft_={};
  }
  if(rigCaptured_) for(const auto &record:assets_.records()) {
    if(record.type!=resources::AssetType::RigPose) continue;
    std::filesystem::path absolute;std::vector<u8> bytes;resources::RigPose pose;
    if(EditorImportTransaction::safePath(absoluteRoot,record.path,absolute)&&EditorImportTransaction::read(absolute,bytes,RigAsset::MaximumBytes)&&
       resources::RigPose::deserialize(std::string(bytes.begin(),bytes.end()),pose)&&pose.rig==rigDraft_.guid) rigPoses_.push_back(std::move(pose));
  }
  state_.rigToolOpen=true;state_.rigTool={};state_.inspectorMenu=false;state_.prefabOverridesOpen=false;
  refreshRigTool();return true;
}

bool EditorSession::publishRigDraft(std::string &error) {
  error.clear();
  if(!rigCaptured_) {error="Capture a hierarquia antes de publicar.";return false;}
  if(!rigDraft_.valid(&error)) return false;
  const auto *entity=document_.find(rigRoot_);
  const std::string path="Rigs/"+safeStem(entity?entity->name:rigDraft_.name,"Rig")+".aerig";
  const auto root=EditorImportTransaction::fromUtf8(files_.rootPath());std::filesystem::path absolute;
  if(files_.rootPath().empty()||!EditorImportTransaction::safePath(root,path,absolute)) {error="Rigs precisam de um projeto aberto.";return false;}
  const auto text=rigDraft_.serialize();
  resources::AssetRecord record;record.guid=rigDraft_.guid;record.type=resources::AssetType::Rig;record.path=path;
  record.contentHash=Sha256::hex(std::span<const u8>(reinterpret_cast<const u8*>(text.data()),text.size()));
  auto next=assets_;
  if(const auto *old=next.find(record.guid)) {if(old->path!=path||old->type!=record.type) {error="Outro recurso já usa esta identidade.";return false;} next.remove(record.guid);}
  else if(next.findByPath(path)) {error="Outro recurso ocupa "+path+".";return false;}
  if(!next.add(record)) {error="O registro recusou o rig.";return false;}
  std::error_code fs;std::filesystem::create_directories(absolute.parent_path(),fs);
  if(fs||!EditorImportTransaction::writeText(absolute,text)) {error="Não foi possível gravar "+path+".";return false;}
  assets_=std::move(next);assetRegistryDirty_=true;rigDirty_=false;files_.rebuildTree();
  return true;
}

bool EditorSession::applyRigPoseToScene(std::span<const resources::RigTransform> local,std::span<const u8> channels,
                                        std::string_view label,std::string &error) {
  resources::CompiledRig compiled;if(!resources::compileRig(rigDraft_,compiled,error)) return false;
  runtime::RigBindings bindings;
  if(!runtime::bindRigToScene(rigDraft_,compiled,document_,rigRoot_,bindings)&&bindings.objects.empty()) {error="Hierarquia do rig não encontrada na cena.";return false;}
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
  auto &ui=state_.rigTool;const auto page=ui.page;const auto route=ui.route;const bool routes=ui.routesOpen;
  ui.fields.clear();ui.error=false;ui.hasPreview=false;ui.canConfirm=false;ui.confirmLabel.clear();
  ui.route=route;ui.page=page;ui.routesOpen=routes;
  const auto *root=document_.find(rigRoot_);
  ui.title=(root?root->name:std::string("Rig"))+(rigCaptured_?" · rev. "+std::to_string(rigDraft_.revision)+" · "+std::to_string(rigDraft_.joints.size())+" juntas":"");
  const auto add=[&](RigToolAction action,std::string name,std::string value,bool enabled=true,ui::UiIcon icon=ui::UiIcon::None) {ui.fields.push_back({action,std::move(name),std::move(value),enabled,icon});};
  if(!rigCaptured_) {
    add(RigToolAction::CaptureRig,"Capturar hierarquia","Juntas reais da seleção, referência = pose atual",true,ui::UiIcon::AnimationSkeleton);
    ui.status="Nenhum rig publicado para esta raiz.";return;
  }
  if(rigDirty_) {ui.confirmLabel="Publicar rig";ui.canConfirm=true;}
  switch(route) {
  case RigToolRoute::Controls: {
    const auto &joint=rigDraft_.joints[std::min<usize>(rigJoint_,rigDraft_.joints.size()-1)];
    add(RigToolAction::NextControl,"Junta "+std::to_string(rigJoint_+1)+" de "+std::to_string(rigDraft_.joints.size()),joint.name);
    const RigControl *control=nullptr;
    for(const auto &c:rigDraft_.controls) if(c.joint==joint.id) control=&c;
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
    if(rigDraft_.controls.size()<1) {ui.status="Crie controles para formar pares.";break;}
    const auto name=[&](u32 i){const auto &c=rigDraft_.controls[i%rigDraft_.controls.size()];const auto *j=rigDraft_.joint(c.joint);return j?j->name:c.name;};
    add(RigToolAction::MirrorLeft,"Lado A",name(rigMirrorLeft_));
    add(RigToolAction::MirrorRight,"Lado B",name(rigMirrorRight_));
    add(RigToolAction::MirrorPair,"Formar par",std::to_string(rigDraft_.mirror.size())+(rigDraft_.mirror.size()==1?" par no rig":" pares no rig"),true,ui::UiIcon::AnimationRigMirror);
    add(RigToolAction::MirrorPlane,"Plano",std::string("Normal ")+axisName(rigDraft_.mirrorNormal));
    add(RigToolAction::MirrorPreview,"Espelhar pose atual",rigDraft_.mirror.empty()?"Forme um par primeiro":"A → B e B → A, com Desfazer",!rigDraft_.mirror.empty(),ui::UiIcon::AnimationRigMirror);
    break;
  }
  case RigToolRoute::Poses: {
    add(RigToolAction::PoseCapture,"Salvar pose atual","Recurso .aerpose reutilizável",!rigDirty_,ui::UiIcon::AnimationRecordPose);
    if(rigDirty_) ui.status="Publique o rig antes de salvar poses.";
    if(!rigPoses_.empty()) {
      const auto &pose=rigPoses_[rigPose_%rigPoses_.size()];
      add(RigToolAction::PoseNext,"Pose "+std::to_string(rigPose_%rigPoses_.size()+1)+" de "+std::to_string(rigPoses_.size()),pose.name);
      add(RigToolAction::PoseWeight,"Peso",std::to_string(rigPoseWeight_)+"%");
      const bool compatible=pose.rigRevision==rigDraft_.revision;
      add(RigToolAction::PoseApply,"Aplicar na cena",compatible?"Com Desfazer":"Revisão "+std::to_string(pose.rigRevision)+" ≠ rig "+std::to_string(rigDraft_.revision),compatible,ui::UiIcon::AnimationReferencePose);
    } else if(!rigDirty_) ui.status="Nenhuma pose salva para este rig.";
    break;
  }
  case RigToolRoute::Retarget:
    ui.status="Transferência exige dois rigs publicados; seleção do destino ainda não ligada à interface.";
    break;
  case RigToolRoute::Diagnostics: {
    resources::CompiledRig compiled;std::string error;
    if(!resources::compileRig(rigDraft_,compiled,error)) {ui.status=error;ui.error=true;break;}
    runtime::RigBindings bindings;runtime::bindRigToScene(rigDraft_,compiled,document_,rigRoot_,bindings);
    add(RigToolAction::Routes,"Juntas vinculadas",std::to_string(rigDraft_.joints.size()-bindings.issues.size())+" de "+std::to_string(rigDraft_.joints.size()),false);
    for(const auto &issue:bindings.issues) {
      const auto *joint=rigDraft_.joint(issue.joint);
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
    const auto &joint=rigDraft_.joints[std::min<usize>(rigJoint_,rigDraft_.joints.size()-1)];
    for(const auto &c:rigDraft_.controls) if(c.joint==joint.id) {
      auto value=c;change(value);RigAsset next;u64 assigned=0;
      if(prepareRigControl(rigDraft_,rigDraft_.revision,value,next,assigned,error)) {rigDraft_=std::move(next);rigDirty_=true;}
      return;
    }
  };
  switch(action) {
  case RigToolAction::Close:
    if(rigDirty_&&!ui.error&&ui.status!="Alterações não publicadas · toque × de novo para descartar") {
      ui.status="Alterações não publicadas · toque × de novo para descartar";return true;
    }
    state_.rigToolOpen=false;rigDraft_={};rigPoses_.clear();rigCaptured_=rigDirty_=false;return true;
  case RigToolAction::Routes: if(rigCaptured_) ui.routesOpen=!ui.routesOpen;return true;
  case RigToolAction::RouteControls: routeTo(RigToolRoute::Controls);break;
  case RigToolAction::RouteMirror: routeTo(RigToolRoute::Mirror);break;
  case RigToolAction::RoutePoses: routeTo(RigToolRoute::Poses);break;
  case RigToolAction::RouteRetarget: routeTo(RigToolRoute::Retarget);break;
  case RigToolAction::RouteDiagnostics: routeTo(RigToolRoute::Diagnostics);break;
  case RigToolAction::PreviousPage: if(ui.page) --ui.page;break;
  case RigToolAction::NextPage: ++ui.page;break;
  case RigToolAction::CaptureRig: {
    const auto *root=document_.find(rigRoot_);
    const auto guid=resources::assetGuidFromSeed("rig:"+std::to_string(rigRoot_)+":"+
      std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
    RigAsset captured;
    if(prepareRigCapture(document_,rigRoot_,guid,root?root->name:"Rig",captured,error)) {rigDraft_=std::move(captured);rigCaptured_=rigDirty_=true;}
    break;
  }
  case RigToolAction::NextControl: if(!rigDraft_.joints.empty()) rigJoint_=(rigJoint_+1)%rigDraft_.joints.size();break;
  case RigToolAction::CreateControl: {
    const auto &joint=rigDraft_.joints[std::min<usize>(rigJoint_,rigDraft_.joints.size()-1)];
    RigControl value;value.joint=joint.id;value.name=joint.name;RigAsset next;u64 assigned=0;
    if(prepareRigControl(rigDraft_,rigDraft_.revision,value,next,assigned,error)) {rigDraft_=std::move(next);rigDirty_=true;}
    break;
  }
  case RigToolAction::RemoveControl: {
    const auto &joint=rigDraft_.joints[std::min<usize>(rigJoint_,rigDraft_.joints.size()-1)];
    for(const auto &c:rigDraft_.controls) if(c.joint==joint.id) {
      RigAsset next;if(prepareRigRemoveControl(rigDraft_,rigDraft_.revision,c.id,next,error)) {rigDraft_=std::move(next);rigDirty_=true;}
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
    if(!resources::compileRig(rigDraft_,compiled,error)) break;
    runtime::RigBindings bindings;runtime::bindRigToScene(rigDraft_,compiled,document_,rigRoot_,bindings);
    std::vector<resources::RigTransform> local(compiled.joints.size());
    if(!runtime::readRigScenePose(compiled,bindings,document_,local,error)) break;
    std::vector<u8> channels(local.size(),resources::RigAll);RigAsset next;
    if(prepareRigReference(rigDraft_,rigDraft_.revision,local,channels,next,error)) {rigDraft_=std::move(next);rigDirty_=true;}
    break;
  }
  case RigToolAction::MirrorLeft: if(!rigDraft_.controls.empty()) rigMirrorLeft_=(rigMirrorLeft_+1)%rigDraft_.controls.size();break;
  case RigToolAction::MirrorRight: if(!rigDraft_.controls.empty()) rigMirrorRight_=(rigMirrorRight_+1)%rigDraft_.controls.size();break;
  case RigToolAction::MirrorPair: {
    if(rigDraft_.controls.empty()) break;
    RigAsset next;const auto left=rigDraft_.controls[rigMirrorLeft_%rigDraft_.controls.size()].id,right=rigDraft_.controls[rigMirrorRight_%rigDraft_.controls.size()].id;
    if(prepareRigMirrorPair(rigDraft_,rigDraft_.revision,left,right,next,error)) {rigDraft_=std::move(next);rigDirty_=true;}
    break;
  }
  case RigToolAction::MirrorPlane: {
    const auto n=rigDraft_.mirrorNormal;const std::array<float,3> next=n[0]>.5f?std::array<float,3>{0,1,0}:n[1]>.5f?std::array<float,3>{0,0,1}:std::array<float,3>{1,0,0};
    RigAsset candidate;if(prepareRigMirrorPlane(rigDraft_,rigDraft_.revision,next,rigDraft_.mirrorOffset,candidate,error)) {rigDraft_=std::move(candidate);rigDirty_=true;}
    break;
  }
  case RigToolAction::MirrorPreview: {
    resources::CompiledRig compiled;if(!resources::compileRig(rigDraft_,compiled,error)) break;
    runtime::RigBindings bindings;runtime::bindRigToScene(rigDraft_,compiled,document_,rigRoot_,bindings);
    std::vector<resources::RigTransform> local(compiled.joints.size()),mirrored(compiled.joints.size());
    if(!runtime::readRigScenePose(compiled,bindings,document_,local,error)) break;
    if(!resources::mirrorRigPose(rigDraft_,compiled,local,mirrored,resources::RigAll,true,error)) break;
    std::vector<u8> channels(local.size(),resources::RigAll);
    if(applyRigPoseToScene(mirrored,channels,"Espelhar pose do rig",error)) ui.status="Pose espelhada · Desfazer restaura";
    break;
  }
  case RigToolAction::PoseCapture: {
    if(rigDirty_) {error="Publique o rig antes de salvar poses.";break;}
    resources::CompiledRig compiled;if(!resources::compileRig(rigDraft_,compiled,error)) break;
    runtime::RigBindings bindings;runtime::bindRigToScene(rigDraft_,compiled,document_,rigRoot_,bindings);
    std::vector<resources::RigTransform> local(compiled.joints.size());
    if(!runtime::readRigScenePose(compiled,bindings,document_,local,error)) break;
    std::vector<u8> channels(local.size(),resources::RigAll);
    const std::string name="Pose "+std::to_string(rigPoses_.size()+1);
    const std::string path="Rigs/Poses/"+safeStem(rigDraft_.name,"Rig")+" - "+name+".aerpose";
    const auto guid=resources::assetGuidFromSeed("rig-pose:"+path+":"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
    resources::RigPose pose;
    if(!prepareRigPose(compiled,guid,name,local,channels,pose,error)) break;
    const auto root=EditorImportTransaction::fromUtf8(files_.rootPath());std::filesystem::path absolute;
    if(files_.rootPath().empty()||!EditorImportTransaction::safePath(root,path,absolute)||assets_.findByPath(path)) {error="Caminho da pose indisponível.";break;}
    const auto text=pose.serialize();resources::AssetRecord record;record.guid=pose.guid;record.type=resources::AssetType::RigPose;record.path=path;
    record.contentHash=Sha256::hex(std::span<const u8>(reinterpret_cast<const u8*>(text.data()),text.size()));
    auto next=assets_;if(!next.add(record)) {error="O registro recusou a pose.";break;}
    std::error_code fs;std::filesystem::create_directories(absolute.parent_path(),fs);
    if(fs||!EditorImportTransaction::writeText(absolute,text)) {error="Não foi possível gravar "+path+".";break;}
    assets_=std::move(next);assetRegistryDirty_=true;files_.rebuildTree();
    rigPoses_.push_back(std::move(pose));rigPose_=static_cast<u32>(rigPoses_.size()-1);ui.status=name+" salva em "+path;
    break;
  }
  case RigToolAction::PoseNext: if(!rigPoses_.empty()) rigPose_=(rigPose_+1)%rigPoses_.size();break;
  case RigToolAction::PoseWeight: rigPoseWeight_=rigPoseWeight_>=100?25:rigPoseWeight_+25;break;
  case RigToolAction::PoseApply: {
    if(rigPoses_.empty()) break;
    resources::CompiledRig compiled;if(!resources::compileRig(rigDraft_,compiled,error)) break;
    runtime::RigBindings bindings;runtime::bindRigToScene(rigDraft_,compiled,document_,rigRoot_,bindings);
    std::vector<resources::RigTransform> local(compiled.joints.size()),blended(compiled.joints.size());
    if(!runtime::readRigScenePose(compiled,bindings,document_,local,error)) break;
    if(!resources::blendRigPose(compiled,rigPoses_[rigPose_%rigPoses_.size()],rigPoseWeight_/100.f,local,blended,error)) break;
    std::vector<u8> channels(local.size(),resources::RigAll);
    if(applyRigPoseToScene(blended,channels,"Aplicar pose do rig",error)) ui.status="Pose aplicada a "+std::to_string(rigPoseWeight_)+"% · Desfazer restaura";
    break;
  }
  case RigToolAction::Confirm:
    if(publishRigDraft(error)) ui.status="Rig publicado · "+std::to_string(rigDraft_.joints.size())+" juntas";
    break;
  default: return true;
  }
  refreshRigTool();
  if(!error.empty()) {ui.status=error;ui.error=true;}
  return true;
}
} // namespace ae::editor
