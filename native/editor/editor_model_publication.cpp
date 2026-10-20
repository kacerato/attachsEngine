#include "editor/editor_session.h"
#include "renderer/authoring_texture.h"
#include "editor/editor_texture_history.h"
#include "resources/model_import.h"
#include <limits>

namespace ae::editor {
namespace {
using OptionalText=std::optional<std::string>;
bool readMetadata(const std::filesystem::path &root,const std::string &relative,usize maximum,OptionalText &out) {
  std::filesystem::path path;std::error_code error;out.reset();
  if(!EditorImportTransaction::safePath(root,relative,path))return false;
  if(!std::filesystem::exists(path,error))return !error;
  std::vector<u8> bytes;
  if(error||!EditorImportTransaction::read(path,bytes,maximum))return false;
  out=std::string(bytes.begin(),bytes.end());return true;
}
std::string textHash(const OptionalText &text) {
  return text?Sha256::hex({reinterpret_cast<const u8*>(text->data()),text->size()}):std::string();
}
bool modelSnapshot(const std::filesystem::path &root,const std::string &hash,std::span<const u8> bytes) {
  std::filesystem::path path;std::error_code error;std::vector<u8> current;
  if(hash.size()!=64||Sha256::hex(bytes)!=hash||
     !EditorImportTransaction::safePath(root,".astra/imports/model-sources/"+hash+".source",path))return false;
  if(std::filesystem::exists(path,error))return !error&&EditorImportTransaction::read(path,current,bytes.size())&&Sha256::hex(current)==hash;
  std::filesystem::create_directories(path.parent_path(),error);
  return !error&&EditorImportTransaction::write(path,bytes);
}
bool readModelSnapshot(const std::filesystem::path &root,const std::string &hash,usize maximum,std::vector<u8> &out) {
  std::filesystem::path path;
  return hash.size()==64&&EditorImportTransaction::safePath(root,".astra/imports/model-sources/"+hash+".source",path)&&
      EditorImportTransaction::read(path,out,maximum)&&Sha256::hex(out)==hash;
}
bool applyLibraryItem(std::string &encoded,const std::string &path,const std::optional<resources::LibraryItem> &item,std::string &diagnostic) {
  if(!item)return true;
  resources::LibraryIndex index;if(!resources::LibraryIndex::deserialize(encoded,index,diagnostic))return false;
  bool found=false;
  for(const auto &current:index.packages()) {
    auto package=current;
    for(auto &entry:package.items)if(entry.path==path) {
      if(entry.id!=item->id)return false;
      entry=*item;found=true;
    }
    if(found) {if(!index.replace(std::move(package)))return false;break;}
  }
  if(!found)return false;
  encoded=index.serialize();return !encoded.empty();
}
// Registros pendentes (textura extraida, material criado) ainda nao foram
// gravados: o disco pode estar atras da memoria, mas nunca divergir dela.
bool persistedRegistry(const std::filesystem::path &root,const resources::AssetRegistry &expected,bool pending=false) {
  std::error_code error;const auto path=root/".astra/assets.astra";std::vector<u8> bytes;resources::AssetRegistry actual;
  if(!std::filesystem::exists(path,error))return !error&&(pending||expected.size()==0);
  if(error||!EditorImportTransaction::read(path,bytes,32u<<20)||
     !resources::AssetRegistry::deserialize(std::string(bytes.begin(),bytes.end()),actual))return false;
  if(actual.serialize()==expected.serialize())return true;
  if(!pending)return false;
  for(const auto &record:actual.records()) {
    const auto *mine=expected.find(record.guid);
    if(!mine||mine->type!=record.type||mine->path!=record.path)return false;
  }
  return true;
}
template<class T>u64 storage(const std::vector<T> &values) {return static_cast<u64>(values.capacity())*sizeof(T);}
}

u64 EditorSession::retainedModelBytes(const ImportedSource &source) {
  // Imported samplers own flat arrays. Authored curves/layer composition belong
  // to editable clip resources and cannot enter this source replay snapshot.
  // Texturas compartilhadas e imutaveis: cada versao retida conta os mips em
  // memoria de cada uma (conservador; versoes que dividem o mesmo bloco contam
  // de novo). Antes, qualquer textura recusava o FBX inteiro.
  u64 textureBytes=0;
  for(const auto &texture:source.textures)if(texture) {
    textureBytes+=sizeof(*texture)+texture->mipChain.capacity();
    if(texture->file)textureBytes+=sizeof(*texture->file)+texture->file->path.capacity();
  }
  u64 bytes=textureBytes+sizeof(source)+storage(source.vertices)+storage(source.indices)+storage(source.draws)+storage(source.materials)+
      storage(source.terrainControls)+storage(source.identities)+storage(source.names)+storage(source.nodes)+storage(source.drawNodes)+storage(source.map.nodes)+
      storage(source.materialNames)+storage(source.textures)+storage(source.skins)+storage(source.drawSkins)+storage(source.skinInfluences)+
      storage(source.animations)+storage(source.morphs)+storage(source.drawMorphs)+storage(source.textureImages)+storage(source.meshLods)+source.map.sourceHash.capacity();
  for(const auto &name:source.names)bytes+=name.capacity();
  for(const auto &name:source.materialNames)bytes+=name.capacity();
  for(const auto &name:source.textureImages)bytes+=name.capacity();
  for(const auto &node:source.nodes)bytes+=node.name.capacity()+node.authoredId.capacity()+storage(node.morphWeights);
  for(const auto &node:source.map.nodes)bytes+=node.name.capacity()+node.authoredId.capacity()+storage(node.draws);
  for(const auto &skin:source.skins)bytes+=skin.name.capacity()+storage(skin.joints)+storage(skin.inverseBind)+storage(skin.jointSpheres);
  for(const auto &clip:source.animations) {
    if(!clip.properties.empty()||clip.drivers)return std::numeric_limits<u64>::max();
    bytes+=clip.name.capacity()+storage(clip.channels)+storage(clip.cues)+storage(clip.regions);
    for(const auto &channel:clip.channels) {
      if(!channel.curves.empty()||!channel.layerSources.empty())return std::numeric_limits<u64>::max();
      bytes+=storage(channel.times)+storage(channel.values);
    }
    for(const auto &cue:clip.cues)bytes+=cue.name.capacity();
    for(const auto &region:clip.regions)bytes+=region.name.capacity();
  }
  for(const auto &morph:source.morphs) {
    bytes+=storage(morph.deltas)+storage(morph.defaultWeights)+storage(morph.names)+storage(morph.maximumDisplacement);
    for(const auto &name:morph.names)bytes+=name.capacity();
  }
  return bytes;
}

void EditorSession::settleModelPublication() {
  // Called after History has either adopted the commands or restored its old
  // document. Never retain entity pointers across that boundary.
  mapScene_.reconcileAssets(document_);mapScene_.hydrateMaterials(document_);
  std::erase_if(state_.selectionSet,[&](const auto id){return !document_.find(id);});
  if(state_.selection&&!document_.find(state_.selection))state_.selection=state_.selectionSet.empty()?kInvalidEntity:state_.selectionSet.front();
  refreshImportLinkView();cameraPreview_.close();appearanceChanged_=true;
  sourceThumbCells_={};sourceThumbContent_={};preview_.markDirty();
}

bool EditorSession::replayModelResource(const ModelResourceState &expected,const ModelResourceState &target,
                                      const std::string &path,std::string &diagnostic) {
  const auto &identity=target.record?*target.record:*expected.record;
  const auto root=EditorImportTransaction::fromUtf8(files_.rootPath());
  const auto *current=assets_.findByPath(path);
  OptionalText map,profile,index;
  if((expected.record?(!current||!texture_history::sameRecord(*current,*expected.record)):current!=nullptr)||
     !texture_history::persisted(root,expected.record?&*expected.record:nullptr,path)||!persistedRegistry(root,assets_)||
     !readMetadata(root,resources::importNodeMapPath(identity.guid),64u<<20,map)||map!=expected.nodeMap||
     !readMetadata(root,resources::importProfilePath(identity.guid),65536,profile)||profile!=expected.profile||
     !readMetadata(root,LibraryIndexPath,resources::LibraryIndex::MaximumBytes,index)) {
    diagnostic="Fonte ou metadados do modelo mudaram; histórico preservado.";return false;
  }
  std::vector<u8> bytes;
  if(!readModelSnapshot(root,target.encodedHash,importLimits_.maximumBytes,bytes)) {
    diagnostic="Cópia autoral do modelo ausente ou alterada; nada publicado.";return false;
  }
  auto next=assets_;
  if(target.record) {
    const auto &record=*target.record;
    if(current?!next.publishImport(record.guid,record.contentHash,record.importerVersion,record.importerParameters,record.derived,record.dependencies):!next.add(record))return false;
  } else if(!next.remove(identity.guid)) {diagnostic="Modelo ainda possui dependentes; desfaça seus vínculos primeiro.";return false;}
  std::string nextIndex;
  if(!prepareLibraryIndex(next,nextIndex,diagnostic)||!applyLibraryItem(nextIndex,path,target.libraryItem,diagnostic))return false;
  resources::ImportProfile replayProfile;
  if(target.profile&&!resources::parseImportProfile(*target.profile,replayProfile)) {
    diagnostic="Receita capturada do modelo inválida; nada publicado.";return false;
  }
  EditorImportTransaction transaction(files_.rootPath());
  const EditorImportTransaction::Metadata metadata[]{
    {EditorImportTransaction::MetadataKind::NodeMap,resources::importNodeMapPath(identity.guid),textHash(map)},
    {EditorImportTransaction::MetadataKind::SourceProfile,resources::importProfilePath(identity.guid),textHash(profile)},
    {EditorImportTransaction::MetadataKind::LibraryIndex,LibraryIndexPath,textHash(index)}};
  if(!transaction.begin(path,expected.encodedHash,diagnostic,{}, {},metadata))return false;
  const auto previousSources=importedSources_;const auto previousAssets=assets_;const auto previousMap=mapScene_;
  const bool previousDirty=assetRegistryDirty_;
  const auto restore=[&]() {
    const auto reason=diagnostic;const bool disk=transaction.rollback();
    importedSources_=previousSources;assets_=previousAssets;assetRegistryDirty_=previousDirty;
    std::string gpu;if(!publishAndAdopt(flattenSources(importedSources_),gpu))diagnostic=reason+" GPU anterior pendente: "+gpu;
    else diagnostic=reason;
    mapScene_=previousMap;
    if(!disk)diagnostic+=" Recuperação de disco pendente; backups preservados.";
  };
  auto sources=importedSources_;
  const auto found=std::find_if(sources.begin(),sources.end(),[&](const auto &entry){return entry.guid==identity.guid;});
  if(target.source) {if(found==sources.end())sources.push_back(*target.source);else *found=*target.source;}
  else if(found!=sources.end())sources.erase(found);
  publishingImportProfile_=std::pair(identity.guid,replayProfile);
  const bool accepted=publishAndAdopt(flattenSources(sources),diagnostic);publishingImportProfile_.reset();
  if(!accepted) {restore();return false;}
  importedSources_=std::move(sources);assets_=std::move(next);
  const OptionalText text[]{target.nodeMap,target.profile,nextIndex};
  if(!transaction.commit(bytes,assets_.serialize(),{},&diagnostic,text)) {restore();return false;}
  assetRegistryDirty_=false;loadLibraryIndex(diagnostic);files_.rebuildTree();return true;
}

bool EditorSession::commitModelImport(std::span<const u8> bytes,const resources::GltfImport &model,
    const std::string &path,const std::string &expectedHash,ModelImportReport &report,
    resources::ImportAmbiguityPolicy policy,std::span<const resources::AssetGuid> excludedNodes,
    std::span<const FolderCompanion> companions,std::string_view contentHash,
    std::span<const resources::AssetGuid> sourceDependencies,const resources::ImportProfile *preparedProfile) {
  report={};
  if(isPlaying()||history_.isOpen()) {report.diagnostic="Finalize a edição antes de publicar.";return false;}
  const auto root=EditorImportTransaction::fromUtf8(files_.rootPath());
  const auto *known=assets_.findByPath(path);
  const auto source=known?known->guid:resources::assetGuidFromSeed("fonte:"+path);
  if(known&&known->type!=resources::AssetType::Mesh) {report.diagnostic="Destino pertence a outro tipo de recurso.";return false;}
  if(!texture_history::persisted(root,known,path)||!persistedRegistry(root,assets_,assetRegistryDirty_)) {report.diagnostic="Registro do modelo mudou no disco; reabra o projeto.";return false;}
  auto profile=preparedProfile?*preparedProfile:importProfileFor(source);
  profile.excludedNodes.assign(excludedNodes.begin(),excludedNodes.end());
  if(!resources::validImportProfile(profile)) {report.diagnostic="Receita do modelo inválida.";return false;}
  ModelResourceState before,after;
  if(known)before.record=*known;
  OptionalText currentIndex;
  if(!readMetadata(root,resources::importNodeMapPath(source),64u<<20,before.nodeMap)||
     !readMetadata(root,resources::importProfilePath(source),65536,before.profile)||
     !readMetadata(root,LibraryIndexPath,resources::LibraryIndex::MaximumBytes,currentIndex)) {
    report.diagnostic="Metadados anteriores indisponíveis.";return false;
  }
  const bool compound=resources::isFbxSource(bytes);
  if(compound&&(!companions.empty()||(!contentHash.empty()&&contentHash!=Sha256::hex(bytes)))) {
    report.diagnostic="FBX deste perfil requer uma fonte original única e seu hash de bytes; dependências em pasta ainda não integradas.";return false;
  }
  for(const auto &package:library_.packages())for(const auto &item:package.items)if(item.path==path)before.libraryItem=item;
  if(compound) {
    const auto old=std::find_if(importedSources_.begin(),importedSources_.end(),[&](const auto &entry){return entry.guid==source;});
    if(known&&old==importedSources_.end()) {report.diagnostic="Reabra o recurso antes de reimportar para preservar o histórico.";return false;}
    if(old!=importedSources_.end())before.source=std::make_shared<const ImportedSource>(*old);
    before.encodedHash=known?known->contentHash:expectedHash.empty()?Sha256::hex(bytes):expectedHash;
    std::vector<u8> previous;
    if((known||before.encodedHash!=Sha256::hex(bytes))&&!readModelSnapshot(root,before.encodedHash,importLimits_.maximumBytes,previous)) {
      std::filesystem::path absolute;
      if(!EditorImportTransaction::safePath(root,path,absolute)||!EditorImportTransaction::read(absolute,previous,importLimits_.maximumBytes)||Sha256::hex(previous)!=before.encodedHash||!modelSnapshot(root,before.encodedHash,previous)) {
        report.diagnostic="Versão anterior sem cópia autoral íntegra; restaure a fonte registrada antes de reimportar.";return false;
      }
    }
    after.encodedHash=Sha256::hex(bytes);
    if(!modelSnapshot(root,after.encodedHash,bytes)) {report.diagnostic="Não foi possível preservar a fonte autoral do modelo.";return false;}
  }
  auto candidateSources=importedSources_;auto next=assets_;StagedSource staged;
  if(!stageSource(model,contentHash.empty()?Sha256::hex(bytes):std::string(contentHash),path,policy,candidateSources,next,report,staged,excludedNodes,&profile))return false;
  if(!sourceDependencies.empty()) {
    const auto record=*next.find(source);
    if(!next.publishImport(source,record.contentHash,record.importerVersion,record.importerParameters,record.derived,{sourceDependencies.begin(),sourceDependencies.end()}))return false;
  }
  const auto stagedSource=std::find_if(candidateSources.begin(),candidateSources.end(),[&](const auto &entry){return entry.guid==source;});
  after.record=*next.find(source);after.nodeMap=stagedSource->map.serialize();after.profile=resources::serializeImportProfile(profile);
  std::string nextIndex;
  if(!prepareLibraryIndex(next,nextIndex,report.diagnostic))return false;
  if(compound&&before.libraryItem) {
    resources::LibraryIndex candidateIndex;
    if(!resources::LibraryIndex::deserialize(nextIndex,candidateIndex,report.diagnostic))return false;
    const auto *item=candidateIndex.item(before.libraryItem->id);
    if(!item)return false;
    after.libraryItem=*item;after.libraryItem->converter=profile.fbxDeformation?"ufbx-scene/2":"ufbx-static/2";
    after.libraryItem->knownLosses=model.appearanceExtensions;
    for(const auto &note:model.notes)if(note.starts_with("Skin FBX:")||note.starts_with("Descontinuidades FBX")||note.starts_with("Takes FBX:"))
      after.libraryItem->knownLosses.push_back(note);
    if(model.skippedAnimations)after.libraryItem->knownLosses.push_back(std::to_string(model.skippedAnimations)+" takes vazios sem canal TRS/morph não publicados.");
    if(model.skippedTextures)after.libraryItem->knownLosses.push_back("Texturas FBX externas não vinculadas; fontes e referências preservadas, aparência incompleta.");
    if(model.skippedCameras||model.skippedLights)after.libraryItem->knownLosses.push_back("Câmeras/luzes FBX excluídas explicitamente neste perfil.");
    if(!applyLibraryItem(nextIndex,path,after.libraryItem,report.diagnostic))return false;
  }
  u64 retained=0;
  if(compound) {
    after.source=std::make_shared<const ImportedSource>(*stagedSource);
    const auto first=before.source?retainedModelBytes(*before.source):0,last=retainedModelBytes(*after.source);
    retained=first>EditorHistory::kMaximumRetainedResourceBytes||last>EditorHistory::kMaximumRetainedResourceBytes?std::numeric_limits<u64>::max():first+last;
    if(retained<=EditorHistory::kMaximumRetainedResourceBytes)for(const auto *state:{&before,&after}) {
      retained+=sizeof(*state)+state->encodedHash.capacity();
      if(state->nodeMap)retained+=state->nodeMap->capacity();
      if(state->profile)retained+=state->profile->capacity();
      if(state->libraryItem) {
        const auto &item=*state->libraryItem;
        retained+=sizeof(item)+item.path.capacity()+item.sourceIdentity.capacity()+item.sourceHash.capacity()+item.converter.capacity()+item.recipe.capacity()+item.diagnostic.capacity()+storage(item.dependencies)+storage(item.knownLosses);
        for(const auto &loss:item.knownLosses)retained+=loss.capacity();
      }
      if(state->record) {const auto &record=*state->record;retained+=sizeof(record)+record.path.capacity()+record.source.capacity()+record.contentHash.capacity()+record.importerParameters.capacity()+storage(record.dependencies)+storage(record.derived);for(const auto &derived:record.derived)retained+=derived.capacity();}
    }
    if(retained>EditorHistory::kMaximumRetainedResourceBytes) {report.diagnostic="Modelo excede os 64 MiB de snapshots autorais deste histórico; nada publicado.";return false;}
  }
  EditorImportTransaction transaction(files_.rootPath());std::vector<EditorImportTransaction::Companion> moved;
  for(const auto &companion:companions)moved.push_back({companion.staged,companion.relative});
  const EditorImportTransaction::Metadata metadata[]{
    {EditorImportTransaction::MetadataKind::NodeMap,resources::importNodeMapPath(source),textHash(before.nodeMap)},
    {EditorImportTransaction::MetadataKind::SourceProfile,resources::importProfilePath(source),textHash(before.profile)},
    {EditorImportTransaction::MetadataKind::LibraryIndex,LibraryIndexPath,textHash(currentIndex)}};
  if(!transaction.begin(path,expectedHash,report.diagnostic,{},moved,metadata))return false;
  const auto previousSources=importedSources_;const auto previousAssets=assets_;const auto previousDocument=document_;
  const auto previousMap=mapScene_;const auto previousHistory=history_;const bool previousDirty=assetRegistryDirty_;
  const auto restore=[&]() {
    const auto reason=report.diagnostic;const bool disk=transaction.rollback();
    importedSources_=previousSources;assets_=previousAssets;document_=previousDocument;history_=previousHistory;assetRegistryDirty_=previousDirty;
    std::string gpu;if(!publishAndAdopt(flattenSources(importedSources_),gpu))report.diagnostic=reason+" GPU anterior pendente: "+gpu;
    else report.diagnostic=reason;
    document_=previousDocument;mapScene_=previousMap;settleModelPublication();
    if(!disk)report.diagnostic+=" Recuperação de disco pendente; backups preservados.";
  };
  publishingImportProfile_=std::pair(source,profile);
  const bool accepted=publishAndAdopt(flattenSources(candidateSources),report.diagnostic);publishingImportProfile_.reset();
  if(!accepted) {restore();return false;}
  importedSources_=std::move(candidateSources);assets_=std::move(next);assetRegistryDirty_=true;
  history_.begin(report.reimported?"Reimportar recurso":"Importar recurso");
  reconcileStagedSource(staged,report);
  if(!report.diagnostic.empty()) {restore();return false;}
  if(compound) {
    const auto project=files_.rootPath();
    if(!history_.setPendingResourceReplay([this,before,after,path,project](bool forward) {
      if(files_.rootPath()!=project||isPlaying())return false;
      std::string error;const bool ok=replayModelResource(forward?before:after,forward?after:before,path,error);
      if(!ok)state_.status=error;
      return ok;
    },retained,[this]{settleModelPublication();})) {report.diagnostic="Histórico autoral recusou o modelo.";restore();return false;}
  }
  const OptionalText text[]{after.nodeMap,after.profile,nextIndex};
  if(!transaction.commit(bytes,assets_.serialize(),{},&report.diagnostic,text)) {restore();return false;}
  history_.end();assetRegistryDirty_=false;loadLibraryIndex(report.diagnostic);files_.rebuildTree();state_.selectedFile=path;
  settleModelPublication();state_.status=report.reimported?"Recurso reimportado; instâncias preservadas":"Recurso registrado; pronto para instanciar";return true;
}
}
