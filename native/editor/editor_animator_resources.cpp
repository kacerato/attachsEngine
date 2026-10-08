#include "editor/editor_session.h"
#include "editor/editor_import_transaction.h"
#include "core/sha256.h"
#include <chrono>
#include <filesystem>
#include <limits>

namespace ae::editor {
namespace {
std::vector<resources::AssetGuid> controllerDependencies(const resources::AnimatorControllerAsset &asset,
                                                       const EditorMapScene &map,const resources::AssetRegistry &registry) {
  std::vector<resources::AssetGuid> result;
  for(const auto &clip:resources::animatorControllerClips(asset.graph)) {
    runtime::AnimationClipView view;
    if(map.findClip(clip,view)&&view.source&&registry.find(view.source->source)&&
       std::find(result.begin(),result.end(),view.source->source)==result.end()) result.push_back(view.source->source);
  }
  return result;
}
}
void EditorSession::loadAnimatorControllers() {
  animatorControllers_.clear();animatorResourceDrag_.reset();
  if(files_.rootPath().empty()) return;
  for(const auto &record:assets_.records()) if(record.type==resources::AssetType::AnimatorController) {
    std::filesystem::path path;std::vector<u8> bytes;resources::AnimatorControllerAsset asset;
    if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(files_.rootPath()),record.path,path)||
       !EditorImportTransaction::read(path,bytes,resources::AnimatorControllerAsset::MaximumBytes)||
       !resources::AnimatorControllerAsset::deserialize({reinterpret_cast<const char*>(bytes.data()),bytes.size()},asset)||asset.guid!=record.guid) {
      reportProblem(EditorConsoleSeverity::Warning,"Controller ausente ou inválido: "+record.path);continue;
    }
    animatorControllers_.push_back(std::move(asset));
  }
}
bool EditorSession::createAnimatorController() {
  if(animatorResourceDrag_) {state_.status="Conclua o arrasto antes de criar o recurso";return false;}
  const auto *source=openAnimator();const auto *entity=document_.find(state_.animatorEntity);
  if(!source||!entity||isPlaying()||playMirrorOpen_||history_.isOpen()||files_.rootPath().empty()) {
    state_.status="Abra um projeto e pare o Play para criar o recurso";return false;
  }
  resources::AnimatorControllerAsset asset;
  asset.name=entity->name;asset.name.resize(std::min(asset.name.size(),scene::Animator::MaximumName));
  if(!scene::Animator::validName(asset.name)) asset.name="Controller";
  asset.graph=resources::AnimatorControllerAsset::portableGraph(*source);
  std::string stem=asset.name;
  for(auto &c:stem) if(c=='/'||c=='\\'||c==':'||c=='"'||c=='.') c='_';
  std::string path="Animação/"+stem+".aeanimator";
  for(u32 n=2;assets_.findByPath(path)||files_.exists(path);++n) path="Animação/"+stem+" "+std::to_string(n)+".aeanimator";
  asset.guid=resources::assetGuidFromSeed("animator:"+path+":"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+
      ":"+std::to_string(++importInstanceCounter_));
  if(!asset.valid()) {state_.status="Grafo inválido para compartilhamento";return false;}
  resources::AssetRecord record;record.guid=asset.guid;record.type=resources::AssetType::AnimatorController;record.path=path;
  const auto text=asset.serialize();record.contentHash=Sha256::hex({reinterpret_cast<const u8*>(text.data()),text.size()});
  record.dependencies=controllerDependencies(asset,mapScene_,assets_);
  auto next=assets_;if(!next.add(record)) {state_.status="Registro recusou o controller";return false;}
  auto values=*entity;auto *component=values.components.editInstance(state_.animatorInstance);
  if(!component||&component->type()!=&scene::Animator::descriptor) return false;
  auto &instance=scene::animator(*component);scene::replaceAnimatorData(instance,*source);instance.controller=asset.guid;instance.clipOverrides.clear();
  // Stage the scene/history before publishing the file. Creating the resource
  // keeps an unused asset after Undo; Undo reverses the instance assignment.
  auto staged=document_;auto stagedHistory=history_;
  if(!stagedHistory.applyValues(staged,entity->id,values)) return false;
  std::filesystem::path absolute;
  if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(files_.rootPath()),path,absolute)) return false;
  std::error_code error;std::filesystem::create_directories(absolute.parent_path(),error);
  if(error) {state_.status="Não foi possível preparar a pasta do controller";return false;}
  // The existing journal owns the new file and registry publication together.
  std::string diagnostic;
  EditorImportTransaction transaction(files_.rootPath());
  if(!transaction.begin(path,"",diagnostic)) {state_.status=diagnostic;return false;}
  if(!transaction.commit({reinterpret_cast<const u8*>(text.data()),text.size()},next.serialize())) {
    state_.status=transaction.rollback()?"Criação recusada; projeto preservado":"Recuperação pendente no journal";return false;
  }
  assets_=std::move(next);assetRegistryDirty_=true;document_=std::move(staged);history_=std::move(stagedHistory);
  animatorControllers_.push_back(std::move(asset));files_.rebuildTree();state_.animatorEditShared=false;
  state_.status="Controller criado; instância vinculada";return true;
}
bool EditorSession::assignAnimatorController(resources::AssetGuid guid) {
  const auto *asset=resources::findAnimatorController(animatorControllers_,guid);
  if(!asset||!asset->valid()) {state_.status="Controller ausente ou inválido";return false;}
  const bool ok=editAnimatorInstance([&](scene::Animator &instance){
    // Keep only compatible, stable layer IDs when reassigning the same source.
    auto masks=instance.controller==guid?instance.layers:std::vector<scene::AnimatorLayer>{};
    if(instance.controller!=guid) instance.clipOverrides.clear();
    instance.parameters=asset->graph.parameters;instance.layers=asset->graph.layers;instance.nextId=asset->graph.nextId;instance.controller=guid;
    for(auto &layer:instance.layers) for(const auto &local:masks) if(layer.id==local.id) layer.mask=local.mask;
    return true;
  });
  if(ok) {state_.animatorEditShared=false;state_.animatorState=state_.animatorTransition=state_.animatorParameter=0;frameAnimator();}
  return ok;
}
bool EditorSession::detachAnimatorController() {
  const auto *source=openAnimator();if(!source||!source->controller.valid()) return false;
  scene::Animator resolved;std::string diagnostic;
  const auto *owner=document_.find(state_.animatorEntity);
  const auto *component=owner?owner->components.findInstance(state_.animatorInstance):nullptr;
  if(!component||!resources::resolveAnimatorController(scene::animator(*component),animatorControllers_,resolved,diagnostic)) {
    state_.status="Recarregue ou substitua o recurso antes de desvincular";return false;
  }
  resolved.controller={};resolved.clipOverrides.clear();
  const bool ok=editAnimatorInstance([&](scene::Animator &instance){scene::replaceAnimatorData(instance,resolved);return true;});
  if(ok) {state_.animatorEditShared=false;state_.status="Grafo local conserva os clipes resolvidos";}
  return ok;
}
bool EditorSession::overrideAnimatorClip(resources::AssetGuid original,resources::AssetGuid replacement) {
  return editAnimatorInstance([&](scene::Animator &instance){
    const auto *asset=resources::findAnimatorController(animatorControllers_,instance.controller);if(!asset) return false;
    const auto clips=resources::animatorControllerClips(asset->graph);
    if(std::find(clips.begin(),clips.end(),original)==clips.end()&&replacement.valid()) return false;
    runtime::AnimationClipView view;
    if(replacement.valid()) {
      if(!mapScene_.findClip(replacement,view)||!view.clip||!view.source||view.clip->duration<=0) {
        state_.status="Clipe ausente ou sem duração";return false;
      }
      std::vector<runtime::ObjectId> targets;
      runtime::resolveAnimationTargets(document_,instance.target?static_cast<runtime::ObjectId>(instance.target):state_.animatorEntity,*view.source,targets);
      usize matched=0,missing=0;
      for(const auto &channel:view.clip->channels)
        if(channel.node<targets.size()&&targets[channel.node]) ++matched;else ++missing;
      if(!matched) {state_.status="Clipe sem canais compatíveis com esta hierarquia";return false;}
      if(missing) state_.status="Override parcial: "+std::to_string(missing)+" canais sem objeto";
    }
    std::erase_if(instance.clipOverrides,[&](const auto &entry){return entry.original==original;});
    if(replacement.valid()&&replacement!=original) instance.clipOverrides.push_back({original,replacement});
    return true;
  });
}
bool EditorSession::commitAnimatorController(const resources::AnimatorControllerAsset &candidate,std::string &diagnostic,bool recordHistory) {
  diagnostic.clear();
  if(isPlaying()||playMirrorOpen_||history_.isOpen()) {diagnostic="Pare o Play e finalize a edição";return false;}
  const auto *current=resources::findAnimatorController(animatorControllers_,candidate.guid);const auto *record=assets_.find(candidate.guid);
  if(!current||!record||record->type!=resources::AssetType::AnimatorController||!candidate.valid()||
     current->revision==std::numeric_limits<u32>::max()||candidate.revision!=current->revision+1||candidate.graph.nextId<current->graph.nextId) {
    diagnostic="Controller, identidade ou revisão inválidos";return false;
  }
  const auto before=*current;const auto path=record->path;
  std::filesystem::path absolute;std::vector<u8> bytes;resources::AnimatorControllerAsset disk;
  if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(files_.rootPath()),path,absolute)||
     !EditorImportTransaction::read(absolute,bytes,resources::AnimatorControllerAsset::MaximumBytes)||
     !resources::AnimatorControllerAsset::deserialize({reinterpret_cast<const char*>(bytes.data()),bytes.size()},disk)||disk.serialize()!=before.serialize()) {
    diagnostic="Controller mudou no disco; recarregue antes de editar";return false;
  }
  auto compare=candidate;compare.revision=before.revision;if(compare.serialize()==before.serialize()) return true;
  const auto text=candidate.serialize();auto next=assets_;
  if(!next.publishImport(candidate.guid,Sha256::hex({reinterpret_cast<const u8*>(text.data()),text.size()}),record->importerVersion,
       record->importerParameters,record->derived,controllerDependencies(candidate,mapScene_,assets_))) {diagnostic="Registro recusou o controller";return false;}
  if(!EditorImportTransaction::publishTextBatch(files_.rootPath(),{{path,Sha256::hex(bytes),text}},next.serialize(),diagnostic)) return false;
  assets_=std::move(next);assetRegistryDirty_=true;
  for(auto &asset:animatorControllers_) if(asset.guid==candidate.guid) {asset=candidate;break;}
  if(recordHistory) {
    const auto project=files_.rootPath();const auto after=candidate;
    history_.recordResource("Controller compartilhado",[this,project,before,after](bool forward){
      if(project!=files_.rootPath()) return false;
      const auto *live=resources::findAnimatorController(animatorControllers_,before.guid);if(!live) return false;
      auto expected=forward?before:after;expected.revision=live->revision;expected.graph.nextId=live->graph.nextId;
      if(expected.serialize()!=live->serialize()) {state_.status="Recurso mudou; histórico preservado";return false;}
      auto value=forward?after:before;value.revision=live->revision+1;value.graph.nextId=std::max(value.graph.nextId,live->graph.nextId);
      std::string error;if(!commitAnimatorController(value,error,false)) {state_.status=error;return false;}
      state_.status=forward?"Controller refeito":"Controller desfeito";return true;
    });
  }
  return true;
}
} // namespace ae::editor
