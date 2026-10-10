#include "editor/editor_session.h"
#include "editor/editor_fbx_rig_binding.h"
#include "editor/editor_animation_pose_selection.h"
#include "scene/import_link.h"
#include <chrono>
#include <algorithm>
#include <limits>

namespace ae::editor {
bool EditorSession::openModelTakes(resources::AssetGuid source,std::string &diagnostic) {
  diagnostic.clear();const auto *record=assets_.find(source);
  if(!record||isPlaying()||history_.isOpen()) {diagnostic="Pare o Play e selecione um modelo registrado.";return false;}
  const auto found=std::find_if(importedSources_.begin(),importedSources_.end(),[&](const auto &value){return value.guid==source;});
  if(found==importedSources_.end()||found->animations.empty()) {diagnostic="Modelo sem takes carregados; reimporte a fonte com deformação habilitada.";return false;}
  // Only view rows are copied. Opening this destination must not duplicate all
  // morph deltas, samplers and geometry of a mobile character in a fake preview.
  beginImportPreparation(record->path);
  std::vector<u32> depths(found->nodes.size());state_.importNodes.reserve(found->nodes.size());
  for(usize node=0;node<found->nodes.size();++node) {
    const auto &entry=found->nodes[node];const auto parent=entry.parent;
    depths[node]=parent>=0&&static_cast<usize>(parent)<node?depths[static_cast<usize>(parent)]+1:0;
    state_.importNodes.push_back({entry.name,depths[node],0});
  }
  state_.importRigJoints=state_.importRigMorphs=0;
  for(const auto &skin:found->skins)state_.importRigJoints+=static_cast<u32>(skin.joints.size());
  for(const auto &morph:found->morphs)state_.importRigMorphs+=morph.targetCount;
  for(const auto &clip:found->animations) {
    u32 weights=0;for(const auto &channel:clip.channels)weights+=channel.path==resources::AnimationPath::Weights;
    state_.importTakes.push_back({clip.name,clip.duration,static_cast<u32>(clip.channels.size()),weights});
  }
  state_.importTake=state_.importTakeRoot=0;state_.importRigDiagnostic.clear();
  state_.importPublishedView=true;state_.importReady=false;state_.importTab=EditorScreenState::ImportTab::Rig;
  state_.importTakeSource=source;state_.importStatus="Takes do recurso publicado";
  diagnostic="Selecione o take e a raiz correspondente; Extrair/Editar cria um clipe independente.";return true;
}

bool EditorSession::extractModelTake(resources::AssetGuid source,u32 take,u32 sourceRoot,EditorEntityId owner,
    resources::AssetGuid &guid,std::string &diagnostic) {
  guid={};diagnostic.clear();
  if(isPlaying()||playMirrorOpen_||history_.isOpen()||clipDrag_) {diagnostic="Pare o Play e finalize a edição antes de extrair.";return false;}
  const auto found=std::find_if(importedSources_.begin(),importedSources_.end(),[&](const auto &value){return value.guid==source;});
  const auto *record=assets_.find(source);
  if(found==importedSources_.end()||!record||take>=found->animations.size()||record->contentHash.size()!=64) {
    diagnostic="Take ou revisão do recurso indisponível.";return false;}
  std::vector<const resources::MorphTargetSet*> morphs(found->nodes.size());
  for(usize draw=0;draw<found->drawNodes.size()&&draw<found->drawMorphs.size();++draw) {
    const auto node=found->drawNodes[draw];const auto morph=found->drawMorphs[draw];
    if(node<morphs.size()&&morph>=0&&static_cast<usize>(morph)<found->morphs.size())morphs[node]=&found->morphs[static_cast<usize>(morph)];
  }
  std::vector<EditorEntityId> targets;
  if(!validateFbxClipRig(document_,mapScene_,owner,source,found->map,found->animations[take],sourceRoot,morphs,targets,diagnostic))return false;
  auto clip=found->animations[take];std::vector<resources::AnimationClipBinding> bindings;
  std::vector<u32> remap(targets.size(),std::numeric_limits<u32>::max());
  for(auto &channel:clip.channels) {
    const auto original=channel.node;
    if(original>=targets.size()||!targets[original]) {diagnostic="Vínculo do canal ausente; extração cancelada.";return false;}
    if(remap[original]==std::numeric_limits<u32>::max()) {
      resources::AnimationClipBinding binding;
      if(!animationPoseTargetPath(document_,owner,targets[original],binding.path,diagnostic))return false;
      const auto *object=document_.find(targets[original]);binding.name=object->name;
      const auto *link=scene::importLink(object->components);
      if(link&&link->source==source&&!link->orphan&&!link->unlinked&&link->primitive<0)binding.sourceNode=link->node;
      remap[original]=static_cast<u32>(bindings.size());bindings.push_back(std::move(binding));
    }
    channel.node=remap[original];
  }
  std::string path="Clipes/Take importado.aeclip";
  for(u32 index=2;files_.exists(path)||assets_.findByPath(path);++index)path="Clipes/Take importado "+std::to_string(index)+".aeclip";
  const auto identity=resources::assetGuidFromSeed("clip:"+files_.rootPath()+":"+path+":"+
      std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+":"+std::to_string(++importInstanceCounter_));
  resources::AnimationClipAsset value;const auto ids=resources::animationClipGuids(source,found->animations);
  if(!resources::authorAnimationClip(clip,identity,source,ids[take],record->contentHash,bindings,value,diagnostic))return false;
  AnimationClipPreview candidate;
  if(!candidate.begin(document_,owner,value,diagnostic)||!candidate.seek(0,diagnostic)||!candidate.seek(value.duration,diagnostic))return false;
  if(!createAnimationClipAsset(value,path,diagnostic))return false;
  guid=identity;return true;
}
}
