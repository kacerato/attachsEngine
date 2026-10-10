#include "editor/editor_session.h"
#include "editor/editor_import_transaction.h"
#include "runtime/transform_math.h"
#include "scene/skinned_mesh.h"
#include "core/rotation_math.h"
#include <filesystem>
#include <set>
#include <algorithm>

namespace ae::editor {
bool EditorSession::selectAnimationPoseTargets(std::span<const EditorEntityId> targets,EditorEntityId primary,std::string &error) {
  error.clear();if(!state_.clipOpen||isPlaying()||clipPoseGizmo_.active||clipDrag_||targets.size()>MaximumAnimationPoseSelection) {error="Seleção exige preview aberto, gesto concluído e até 256 alvos";return false;}
  std::vector<EditorEntityId> selected;std::set<EditorEntityId> unique;
  for(const auto id:targets) {
    std::string path;if(!animationPoseTargetPath(document_,state_.clipOwner,id,path,error)||!unique.insert(id).second)return false;
    selected.push_back(id);
  }
  if((selected.empty()&&primary)||(!selected.empty()&&!unique.contains(primary))) {error="Alvo principal precisa pertencer à seleção";return false;}
  const auto roots=animationPoseSelectionRoots(document_,selected);
  if(!roots.empty()&&std::find(roots.begin(),roots.end(),primary)==roots.end())primary=roots.front();
  if(primary) {
    const auto *track=state_.clipAsset?state_.clipAsset->track(state_.clipTrack):nullptr;
    const auto path=track?track->path:resources::AnimationPath::Translation;
    selectingAnimationPose_=true;
    const bool ok=selectAnimationClipPreviewTarget(primary,path,false,error);selectingAnimationPose_=false;
    if(!ok)return false;
  } else cancelAnimationClipPose();
  state_.clipPoseSelection=std::move(selected);state_.clipPreviewTarget=primary;state_.clipPoseSelectionEmpty=state_.clipPoseSelection.empty();
  if(!primary) {state_.clipTarget=0;state_.clipPoseSelectionIsolated=false;}
  appearanceChanged_=true;return true;
}
bool EditorSession::focusAnimationPoseSelection(std::string &error) {
  error.clear();if(!state_.clipOpen||state_.clipPoseSelection.empty()||isPlaying()) {error="Selecione alvos no preview antes de focar";return false;}
  float low[3]{},high[3]{};bool first=true;
  for(const auto id:state_.clipPoseSelection) {
    float world[16];if(!runtime::worldMatrix(evaluatedEditorScene(),id,world)) {error="Alvo do foco indisponível";return false;}
    for(usize axis=0;axis<3;++axis) {low[axis]=first?world[12+axis]:std::min(low[axis],world[12+axis]);high[axis]=first?world[12+axis]:std::max(high[axis],world[12+axis]);}first=false;
  }
  float extent=0;for(usize axis=0;axis<3;++axis) {camera_.target[axis]=(low[axis]+high[axis])*.5f;extent+=(high[axis]-low[axis])*(high[axis]-low[axis]);}
  // Single-joint focus retains the current distance; a group can only enlarge it.
  if(state_.clipPoseSelection.size()>1)camera_.distance=std::max(camera_.distance,std::sqrt(extent)*1.5f);
  state_.cameraViewEntity=0;state_.cameraPiloting=false;appearanceChanged_=true;return true;
}
bool EditorSession::isolateAnimationPoseSelection(bool enabled,std::string &error) {
  error.clear();if(!state_.clipOpen||isPlaying()||(enabled&&state_.clipPoseSelection.empty())) {error="Isolamento exige seleção no preview";return false;}
  state_.clipPoseSelectionIsolated=enabled;appearanceChanged_=true;return true;
}
bool EditorSession::animationPoseSelectionVisible(EditorEntityId id) const {
  if(!state_.clipOpen||!state_.clipPoseSelectionIsolated||isPlaying())return true;
  const auto &graph=evaluatedEditorScene();
  const auto selected=[&](EditorEntityId target){return std::any_of(state_.clipPoseSelection.begin(),state_.clipPoseSelection.end(),[&](auto root){return target==root||graph.isDescendantOf(target,root);});};
  if(selected(id))return true;
  const auto *object=graph.find(id);
  const auto *skin=object?static_cast<const scene::SkinnedMesh*>(object->components.find(scene::SkinnedMesh::descriptor)):nullptr;
  return skin&&std::any_of(skin->bones.begin(),skin->bones.end(),selected);
}
bool EditorSession::toggleAnimationPoseTarget(EditorEntityId target,std::string &error) {
  auto selected=state_.clipPoseSelection;const auto found=std::find(selected.begin(),selected.end(),target);
  if(found==selected.end())selected.push_back(target);else selected.erase(found);
  const auto primary=std::find(selected.begin(),selected.end(),target)!=selected.end()?target:selected.empty()?0:selected.front();
  return selectAnimationPoseTargets(selected,primary,error);
}
void EditorSession::syncAnimationPoseSelection() {
  if(selectingAnimationPose_||!state_.clipOpen||state_.clipPoseSelectionEmpty)return;
  std::erase_if(state_.clipPoseSelection,[&](auto id){return !document_.exists(id)||(id!=state_.clipOwner&&!document_.isDescendantOf(id,state_.clipOwner));});
  const auto primary=state_.clipPreviewTarget?state_.clipPreviewTarget:state_.clipTarget;
  if(!primary||!document_.exists(primary)) {
    if(state_.clipPoseSelection.empty()) {state_.clipPoseSelectionEmpty=true;state_.clipPoseSelectionIsolated=false;}
    return;
  }
  if(std::find(state_.clipPoseSelection.begin(),state_.clipPoseSelection.end(),primary)==state_.clipPoseSelection.end())
    state_.clipPoseSelection={primary};
}
bool EditorSession::selectAnimationPoseRelation(u32 relation,std::string &error) {
  error.clear();if(relation>3) {error="Relação de seleção inválida";return false;}
  std::vector<EditorEntityId> targets;const auto add=[&](EditorEntityId id){if(id&&(id==state_.clipOwner||document_.isDescendantOf(id,state_.clipOwner))&&std::find(targets.begin(),targets.end(),id)==targets.end())targets.push_back(id);};
  if(relation==3)document_.collectSubtree(state_.clipOwner,targets);
  else for(const auto id:state_.clipPoseSelection)if(const auto *node=document_.find(id)) {
    if(!relation)add(node->parent);
    else for(const auto child:document_.childrenOf(relation==1?id:node->parent))if(relation!=2||child!=id)add(child);
  }
  return selectAnimationPoseTargets(targets,targets.empty()?0:targets.front(),error);
}
void EditorSession::loadAnimationPoseSelections() {
  state_.clipPoseSets.clear();state_.clipPoseSelection.clear();state_.clipPoseSetPage=0;
  std::filesystem::path file;
  if(files_.rootPath().empty()||!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(files_.rootPath()),".astra/animation-selection.astra",file))return;
  std::error_code ec;if(!std::filesystem::exists(file,ec))return;
  std::vector<u8> bytes;std::string error;
  if(!EditorImportTransaction::read(file,bytes,1024*1024)||!deserializeAnimationPoseSets({reinterpret_cast<const char*>(bytes.data()),bytes.size()},state_.clipPoseSets,error))
    reportProblem(EditorConsoleSeverity::Warning,"Seleções de animação não carregadas · "+error);
}
bool EditorSession::publishAnimationPoseSelections(std::vector<AnimationPoseSelectionSet> sets,std::string &error,bool recordHistory) {
  error.clear();const auto next=serializeAnimationPoseSets(sets),previous=serializeAnimationPoseSets(state_.clipPoseSets);
  if(next.empty()||files_.rootPath().empty()||isPlaying()||history_.isOpen()) {error="Conjuntos inválidos ou projeto ocupado";return false;}
  std::filesystem::path file;
  if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(files_.rootPath()),".astra/animation-selection.astra",file)) {error="Caminho das seleções indisponível";return false;}
  std::error_code ec;
  if(std::filesystem::exists(file,ec)) {
    std::vector<u8> disk;if(!EditorImportTransaction::read(file,disk,1024*1024)||std::string(disk.begin(),disk.end())!=previous) {error="Seleções mudaram no disco; reabra o projeto antes de gravar";return false;}
  } else if(!state_.clipPoseSets.empty()) {error="Arquivo de seleções removido externamente";return false;}
  std::filesystem::create_directories(file.parent_path(),ec);
  if(ec||!EditorImportTransaction::writeText(file,next,&error))return false;
  auto before=state_.clipPoseSets;state_.clipPoseSets=sets;appearanceChanged_=true;
  if(recordHistory&&next!=previous) {
    const auto project=files_.rootPath();history_.recordResource("Conjunto de seleção",[this,project,before=std::move(before),after=std::move(sets)](bool forward) {
      if(project!=files_.rootPath())return false;
      if(serializeAnimationPoseSets(state_.clipPoseSets)!=serializeAnimationPoseSets(forward?before:after))return false;
      std::string diagnostic;if(!publishAnimationPoseSelections(forward?after:before,diagnostic,false)) {state_.status=diagnostic;return false;}return true;
    });
  }
  return true;
}
bool EditorSession::saveAnimationPoseSelection(std::string_view name,std::string &error) {
  error.clear();if(!state_.clipOpen||state_.clipPoseSelection.empty()) {error="Selecione alvos antes de salvar o conjunto";return false;}
  AnimationPoseSelectionSet set;set.clip=state_.clipGuid;set.name=name;
  const auto primary=state_.clipPreviewTarget?state_.clipPreviewTarget:state_.clipTarget;
  auto selected=state_.clipPoseSelection;
  if(const auto found=std::find(selected.begin(),selected.end(),primary);found!=selected.end())std::rotate(selected.begin(),found,found+1);
  for(const auto id:selected) {std::string path;if(!animationPoseTargetPath(document_,state_.clipOwner,id,path,error))return false;set.paths.push_back(std::move(path));}
  auto sets=state_.clipPoseSets;const auto found=std::find_if(sets.begin(),sets.end(),[&](const auto &value){return value.clip==set.clip&&value.name==name;});
  if(found==sets.end())sets.push_back(std::move(set));else *found=std::move(set);
  return publishAnimationPoseSelections(std::move(sets),error);
}
bool EditorSession::recallAnimationPoseSelection(std::string_view name,std::string &error) {
  error.clear();const auto found=std::find_if(state_.clipPoseSets.begin(),state_.clipPoseSets.end(),[&](const auto &set){return set.clip==state_.clipGuid&&set.name==name;});
  if(found==state_.clipPoseSets.end()) {error="Conjunto de seleção ausente neste clipe";return false;}
  std::vector<EditorEntityId> targets;
  for(const auto &path:found->paths) {EditorEntityId id=0;if(!resolveAnimationPoseTargetPath(document_,state_.clipOwner,path,id,error))return false;targets.push_back(id);}
  return selectAnimationPoseTargets(targets,targets.front(),error);
}
bool EditorSession::removeAnimationPoseSelection(std::string_view name,std::string &error) {
  auto sets=state_.clipPoseSets;
  if(!std::erase_if(sets,[&](const auto &set){return set.clip==state_.clipGuid&&set.name==name;})) {error="Conjunto de seleção ausente";return false;}
  return publishAnimationPoseSelections(std::move(sets),error);
}
bool EditorSession::applyAnimationPoseSelectionName(std::string_view name) {
  std::string error;
  if(!state_.clipOpen||state_.clipGuid!=clipNumberGuid_||document_.revision()!=clipNumberSceneRevision_||state_.clipPoseSelection!=clipPoseNameSelection_) {state_.clipDiagnostic="Seleção mudou; reabra o nome";return false;}
  const bool ok=saveAnimationPoseSelection(name,error);state_.clipDiagnostic=error;return ok;
}
namespace {
void multiplyPoseRotation(const float *a,const float *b,float *out) {
  const float q[]{a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1],a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0],
    a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3],a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2]};std::copy_n(q,4,out);
}
}
bool EditorSession::expandAnimationPoseDraft(resources::AnimationClipAsset &candidate,u64 primaryId,std::span<const float> requested,std::string &error,std::optional<bool> resetIdentity) {
  const auto primary=*candidate.track(primaryId);std::vector<float> before;
  if(!sampleAnimationPoseChannel(primary,state_.clipTime,before)) {error="Canal principal não produz pose";return false;}
  const auto roots=animationPoseSelectionRoots(document_,state_.clipPoseSelection);
  if(roots.size()<2)return true;
  const auto active=state_.clipPreviewTarget?state_.clipPreviewTarget:state_.clipTarget;
  if(std::find(roots.begin(),roots.end(),active)==roots.end()) {error="Principal deve ser uma raiz da seleção; selecione novamente";return false;}
  const auto binding=std::find_if(candidate.bindings.begin(),candidate.bindings.end(),[&](const auto &value){return value.id==primary.binding;});
  runtime::ObjectId resolved=0;
  if(binding==candidate.bindings.end()||!resolveAnimationPoseTargetPath(document_,state_.clipOwner,binding->path,resolved,error)||resolved!=active) {
    error="Canal principal não pertence ao alvo ativo da seleção";return false;
  }
  float delta[4]{};
  const bool quaternion=primary.path==resources::AnimationPath::Rotation&&primary.rotationMode!=resources::AnimationRotationMode::Euler;
  if(quaternion) {const float inverse[]{-before[0],-before[1],-before[2],before[3]};multiplyPoseRotation(inverse,requested.data(),delta);}
  for(const auto target:roots)if(target!=active) {
    const auto *object=evaluatedEditorScene().find(target);resources::AnimationClipBinding binding;
    if(!object||!animationPoseTargetPath(document_,state_.clipOwner,target,binding.path,error))return false;
    binding.name=object->name;u64 bindingId=0,id=0,base=0;
    for(const auto &item:candidate.bindings)if(item.path==binding.path) {binding=item;bindingId=item.id;break;}
    for(const auto &track:candidate.tracks)if(track.binding==bindingId&&track.path==primary.path) {if(!track.layer)base=track.id;if(track.layer==primary.layer)id=track.id;}
    if(!id) {
      if(!state_.clipPoseRecording.createChannels) {error="Seleção contém alvo sem canal; política permite só canais existentes";return false;}
      std::vector<float> initial;
      if(primary.path==resources::AnimationPath::Translation)initial.assign(object->transform.position,object->transform.position+3);
      else if(primary.path==resources::AnimationPath::Scale)initial.assign(object->transform.scale,object->transform.scale+3);
      else if(primary.path==resources::AnimationPath::Rotation) {
        if(!quaternion)initial.assign(object->transform.rotationDegrees,object->transform.rotationDegrees+3);
        else {float q[4];runtime::transformRotationQuaternion(object->transform,q);initial.assign(q,q+4);}
      } else {
        const auto *skin=static_cast<const scene::SkinnedMesh*>(object->components.find(scene::SkinnedMesh::descriptor));
        if(!skin||skin->blendShapeWeights.size()!=before.size()) {error="Morphs da seleção não possuem a mesma dimensão";return false;}
        initial=skin->blendShapeWeights;for(auto &value:initial)value*=.01f;
      }
      if(!base&&!candidate.addTrack(binding,primary.path,primary.rotationMode,initial,base,error))return false;
      id=base;if(primary.layer&&!candidate.addLayerTrack(primary.layer,base,false,id,error,primary.path==resources::AnimationPath::Rotation?std::optional{primary.rotationMode}:std::nullopt))return false;
    }
    const auto *track=candidate.track(id);
    if(track->rotationMode!=primary.rotationMode||track->weightCount!=primary.weightCount) {error="Canais da seleção exigem a mesma representação e dimensão";return false;}
    std::vector<float> values;
    if(!sampleAnimationPoseChannel(*track,state_.clipTime,values)||values.size()!=requested.size()) {error="Canal da seleção não produz pose compatível";return false;}
    if(resetIdentity) {
      const auto *reference=document_.find(target);if(!reference) {error="Referência da seleção ausente";return false;}
      const bool identity=*resetIdentity;
      if(primary.path==resources::AnimationPath::Translation)values=identity?std::vector<float>(3,0):std::vector<float>(reference->transform.position,reference->transform.position+3);
      else if(primary.path==resources::AnimationPath::Scale)values=identity?std::vector<float>(3,1):std::vector<float>(reference->transform.scale,reference->transform.scale+3);
      else if(primary.path==resources::AnimationPath::Rotation) {
        runtime::Transform pose;if(!identity)pose=reference->transform;
        if(quaternion) {float q[4];runtime::transformRotationQuaternion(pose,q);values.assign(q,q+4);}
        else values.assign(pose.rotationDegrees,pose.rotationDegrees+3);
      } else {
        const auto *skin=static_cast<const scene::SkinnedMesh*>(reference->components.find(scene::SkinnedMesh::descriptor));
        if(!identity&&(!skin||skin->blendShapeWeights.size()!=values.size())) {error="Morphs da referência não correspondem ao canal";return false;}
        for(usize i=0;i<values.size();++i)values[i]=identity?0:skin->blendShapeWeights[i]*.01f;
      }
    } else if(quaternion) {float q[4];multiplyPoseRotation(values.data(),delta,q);values.assign(q,q+4);}
    else for(usize i=0;i<values.size();++i) {
      if(primary.path==resources::AnimationPath::Scale) {
        if(before[i]==0&&requested[i]!=0) {error="Escala relativa de grupo exige eixo inicial diferente de zero";return false;}
        values[i]*=before[i]==0?1:requested[i]/before[i];
      } else values[i]+=requested[i]-before[i];
    }
    std::vector<float> locked;
    if(!applyAnimationPoseLocks(*track,state_.clipTime,state_.clipPoseLocks[static_cast<u32>(primary.path)],values,locked,error)||!candidate.putPose(id,state_.clipTime,locked,error))return false;
  }
  return true;
}
}
