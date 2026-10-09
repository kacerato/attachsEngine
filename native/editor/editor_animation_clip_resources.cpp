#include "editor/editor_session.h"
#include "resources/animation_clip_bake.h"
#include <chrono>
#include "editor/editor_import_transaction.h"
#include <filesystem>
#include <limits>

namespace ae::editor {
namespace {
using ClipAsset=resources::AnimationClipAsset;
using Library=std::vector<std::shared_ptr<const runtime::SourceAnimations>>;
bool compileLibrary(std::span<const ClipAsset> assets,Library &out,std::string &error) {
  Library next;next.reserve(assets.size());
  for(const auto &asset:assets) {
    auto source=std::make_shared<runtime::SourceAnimations>();resources::AnimationClip clip;
    if(!asset.compile(clip,&error))return false;
    source->channelsValidated=true;
    source->source=asset.source.valid()?asset.source:asset.guid;
    source->clips.push_back(std::move(clip));source->clipIds.push_back(asset.guid);
    for(const auto &b:asset.bindings) {
      source->nodes.push_back(b.sourceNode);source->nodeNames.push_back(b.name);source->nodePaths.push_back(b.path);
    }
    next.push_back(std::move(source));
  }
  out=std::move(next);return true;
}
std::vector<resources::AssetGuid> dependencies(const ClipAsset &asset) {
  return asset.source.valid()?std::vector<resources::AssetGuid>{asset.source}:std::vector<resources::AssetGuid>{};
}
std::string hash(std::string_view text) {return Sha256::hex({reinterpret_cast<const u8*>(text.data()),text.size()});}
}
const resources::AnimationClipAsset *EditorSession::animationClipAsset(resources::AssetGuid guid) const {
  for(const auto &asset:animationClipAssets_)if(asset.guid==guid)return &asset;
  return nullptr;
}
void EditorSession::loadAnimationClipAssets() {
  if(state_.clipOpen) {clipPreview_.cancel();clipDrag_.reset();clipPointer_=0;appearanceChanged_=true;}
  animationClipAssets_.clear();mapScene_.setAuthoredAnimations({});
  if(files_.rootPath().empty())return;
  for(const auto &record:assets_.records())if(record.type==resources::AssetType::AnimationClip) {
    std::filesystem::path path;std::vector<u8> bytes;ClipAsset value;std::string error;
    if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(files_.rootPath()),record.path,path)||
       !EditorImportTransaction::read(path,bytes,ClipAsset::MaximumBytes)||Sha256::hex(bytes)!=record.contentHash||
       !ClipAsset::deserialize({reinterpret_cast<const char*>(bytes.data()),bytes.size()},value,&error)||value.guid!=record.guid||
       (value.source.valid()&&!assets_.find(value.source))) {
      reportProblem(EditorConsoleSeverity::Warning,"Clipe ausente, modificado ou inválido: "+record.path+(error.empty()?"":" · "+error));continue;
    }
    runtime::AnimationClipView imported;
    if(mapScene_.findClip(value.guid,imported)) {
      reportProblem(EditorConsoleSeverity::Warning,"Identidade de clipe já carregada: "+record.path);continue;
    }
    animationClipAssets_.push_back(std::move(value));
  }
  Library library;std::string error;
  if(!compileLibrary(animationClipAssets_,library,error)||!mapScene_.setAuthoredAnimations(std::move(library))) {
    animationClipAssets_.clear();mapScene_.setAuthoredAnimations({});
    reportProblem(EditorConsoleSeverity::Warning,"Biblioteca de clipes recusada · "+error);
  }
}
bool EditorSession::createAnimationClipAsset(const ClipAsset &value,std::string_view relative,std::string &error) {
  return publishAnimationClipCreation(value,relative,error,true);
}
bool EditorSession::createConsolidatedAnimationClip(const ClipAsset &source,std::string_view name,
    const resources::AnimationBakeSettings &settings,resources::AssetGuid &guid,resources::AnimationBakeReport &report,std::string &error) {
  guid={};report={};
  if(isPlaying()||playMirrorOpen_||history_.isOpen()||files_.rootPath().empty()) {error="Abra um projeto, pare o Play e conclua a edição";return false;}
  const auto *live=animationClipAsset(source.guid);
  if(!live||live->revision!=source.revision) {error="Origem da consolidação mudou; reabra o rascunho";return false;}
  // O arquivo leva o nome pedido (o mesmo do recurso no catálogo), não um
  // genérico: dois resultados consolidados ficam distinguíveis em Arquivos.
  std::string stem(name.empty()?std::string_view{"Consolidado"}:name.substr(0,96));
  for(auto &c:stem) if(c=='/'||c=='\\'||c==':'||c=='"'||c=='*'||c=='?'||c=='<'||c=='>'||c=='|'||static_cast<unsigned char>(c)<32) c='_';
  while(!stem.empty()&&(stem.back()==' '||stem.back()=='.')) stem.pop_back();
  if(stem.empty()) stem="Consolidado";
  std::string path="Clipes/"+stem+".aeclip";
  for(u32 n=2;files_.exists(path)||assets_.findByPath(path);++n)path="Clipes/"+stem+" "+std::to_string(n)+".aeclip";
  const auto identity=resources::assetGuidFromSeed("consolidated:"+files_.rootPath()+":"+path+":"+
      std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+":"+std::to_string(++importInstanceCounter_));
  ClipAsset value;resources::AnimationBakeReport result;
  if(!resources::consolidateAnimationClip(source,identity,name,settings,value,result,error)||!createAnimationClipAsset(value,path,error))return false;
  guid=identity;report=result;return true;
}
bool EditorSession::publishAnimationClipCreation(const ClipAsset &value,std::string_view relative,std::string &error,bool recordHistory) {
  error.clear();runtime::AnimationClipView existing;
  if(isPlaying()||playMirrorOpen_||history_.isOpen()||files_.rootPath().empty()) {error="Abra um projeto, pare o Play e conclua a edição";return false;}
  if(!value.valid(&error))return false;
  if((recordHistory&&value.revision!=1)||assets_.find(value.guid)||mapScene_.findClip(value.guid,existing)||assets_.findByPath(relative)||files_.exists(std::string(relative))) {
    error="Identidade, revisão ou caminho de clipe já utilizados";return false;
  }
  std::filesystem::path absolute;
  if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(files_.rootPath()),std::string(relative),absolute)||absolute.extension()!=".aeclip") {
    error="Use um caminho .aeclip dentro do projeto";return false;
  }
  auto nextClips=animationClipAssets_;nextClips.push_back(value);Library library;
  if(!compileLibrary(nextClips,library,error))return false;
  if(!mapScene_.validateAuthoredAnimations(library)) {error="Biblioteca recusou os clipes antes da publicação";return false;}
  const auto text=value.serialize();resources::AssetRecord record;record.guid=value.guid;
  record.type=resources::AssetType::AnimationClip;record.path=relative;record.contentHash=hash(text);record.dependencies=dependencies(value);
  auto nextAssets=assets_;if(!nextAssets.add(record)) {error="Registro recusou o clipe ou sua origem";return false;}
  std::error_code ec;std::filesystem::create_directories(absolute.parent_path(),ec);
  if(ec) {error="Não foi possível preparar a pasta do clipe";return false;}
  EditorImportTransaction transaction(files_.rootPath());if(!transaction.begin(std::string(relative),"",error))return false;
  if(!transaction.commit({reinterpret_cast<const u8*>(text.data()),text.size()},nextAssets.serialize(),{},&error)) {
    const auto reason=error;
    error=(transaction.rollback()?"Criação recusada; projeto preservado":"Recuperação pendente no journal")+(reason.empty()?std::string():" · "+reason);return false;
  }
  // All library entries were validated and every GUID collision checked before
  // publication. No geometry/renderer upload is needed for clip authoring.
  mapScene_.setAuthoredAnimations(std::move(library));
  assets_=std::move(nextAssets);animationClipAssets_=std::move(nextClips);assetRegistryDirty_=true;files_.rebuildTree();
  if(recordHistory) {
    const auto project=files_.rootPath(),path=std::string(relative);auto replay=std::make_shared<ClipAsset>(value);
    history_.recordResource("Criar clipe",[this,project,path,replay](bool forward) {
      if(project!=files_.rootPath()||isPlaying()||playMirrorOpen_||history_.isOpen())return false;
      std::string diagnostic;
      if(forward) {
        if(replay->revision==std::numeric_limits<u32>::max())return false;
        auto restored=*replay;++restored.revision;
        if(!publishAnimationClipCreation(restored,path,diagnostic,false)) {state_.status=diagnostic;return false;}
        *replay=std::move(restored);return true;
      }
      const auto *live=animationClipAsset(replay->guid);const auto *record=assets_.find(replay->guid);
      if(!live||!record||record->path!=path)return false;
      auto expected=*replay;expected.revision=live->revision;expected.nextId=live->nextId;
      if(expected.serialize()!=live->serialize()||sceneUsersOf(live->guid)||!assets_.dependents(live->guid).empty()) {
        state_.status="Clipe modificado ou em uso; histórico preservado";return false;
      }
      std::filesystem::path absolute;std::vector<u8> bytes;
      if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(project),path,absolute)||
         !EditorImportTransaction::read(absolute,bytes,ClipAsset::MaximumBytes)||Sha256::hex(bytes)!=record->contentHash) {
        state_.status="Clipe mudou no disco; histórico preservado";return false;
      }
      auto nextClips=animationClipAssets_;std::erase_if(nextClips,[&](const auto &entry){return entry.guid==live->guid;});
      Library library;if(!compileLibrary(nextClips,library,diagnostic)||!mapScene_.validateAuthoredAnimations(library))return false;
      auto nextAssets=assets_;if(!nextAssets.remove(live->guid))return false;
      const auto snapshot=*live;EditorImportTransaction transaction(project);
      if(!transaction.begin(path,record->contentHash,diagnostic)||!transaction.commitRemoval(nextAssets.serialize())) {
        transaction.rollback();state_.status="Remoção do clipe recusada; journal preserva o projeto";return false;
      }
      mapScene_.setAuthoredAnimations(std::move(library));assets_=std::move(nextAssets);animationClipAssets_=std::move(nextClips);
      *replay=snapshot;assetRegistryDirty_=true;files_.rebuildTree();refreshAnimationClip();appearanceChanged_=true;return true;
    });
  }
  return true;
}
bool EditorSession::editAnimationClipAsset(resources::AssetGuid guid,u32 expectedRevision,
      const std::function<bool(ClipAsset &)> &change,std::string &error) {
  const auto *live=animationClipAsset(guid);if(!live) {error="Clipe editável ausente";return false;}
  auto candidate=*live;if(!candidate.edit(expectedRevision,change,error))return false;
  if(candidate.revision==live->revision)return true;
  return commitAnimationClipAsset(candidate,error);
}
bool EditorSession::commitAnimationClipAsset(const ClipAsset &candidate,std::string &error,bool recordHistory) {
  error.clear();
  if(isPlaying()||playMirrorOpen_||history_.isOpen()) {error="Pare o Play e finalize a edição";return false;}
  const auto *current=animationClipAsset(candidate.guid);const auto *record=assets_.find(candidate.guid);
  if(!current||!record||record->type!=resources::AssetType::AnimationClip||current->revision==std::numeric_limits<u32>::max()||
     candidate.revision!=current->revision+1||candidate.nextId<current->nextId) {error="Identidade, revisão ou alocador de clipe inválidos";return false;}
  if(!candidate.valid(&error))return false;
  const auto before=*current;const auto path=record->path;
  std::filesystem::path absolute;std::vector<u8> bytes;ClipAsset disk;
  if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(files_.rootPath()),path,absolute)||
     !EditorImportTransaction::read(absolute,bytes,ClipAsset::MaximumBytes)||Sha256::hex(bytes)!=record->contentHash||
     !ClipAsset::deserialize({reinterpret_cast<const char*>(bytes.data()),bytes.size()},disk)||disk.serialize()!=before.serialize()) {
    error="Clipe mudou no disco; recarregue antes de editar";return false;
  }
  auto nextClips=animationClipAssets_;for(auto &value:nextClips)if(value.guid==candidate.guid) {value=candidate;break;}
  Library library;if(!compileLibrary(nextClips,library,error))return false;
  if(!mapScene_.validateAuthoredAnimations(library)) {error="Biblioteca recusou os clipes antes da publicação";return false;}
  const auto text=candidate.serialize();auto nextAssets=assets_;
  if(!nextAssets.publishImport(candidate.guid,hash(text),record->importerVersion,record->importerParameters,record->derived,dependencies(candidate))) {
    error="Registro recusou o clipe ou sua origem";return false;
  }
  if(!EditorImportTransaction::publishTextBatch(files_.rootPath(),{{path,Sha256::hex(bytes),text}},nextAssets.serialize(),error))return false;
  // Single writer: no import/library mutation occurs between the preflight and
  // this swap. Every normal refusal happens before the journal is committed.
  mapScene_.setAuthoredAnimations(std::move(library));
  assets_=std::move(nextAssets);animationClipAssets_=std::move(nextClips);assetRegistryDirty_=true;
  if(recordHistory) {
    const auto project=files_.rootPath();const auto after=candidate;
    history_.recordResource("Editar clipe",[this,project,before,after](bool forward) {
      if(project!=files_.rootPath())return false;
      const auto *live=animationClipAsset(before.guid);if(!live)return false;
      auto expected=forward?before:after;expected.revision=live->revision;expected.nextId=live->nextId;
      if(expected.serialize()!=live->serialize()) {state_.status="Clipe mudou; histórico preservado";return false;}
      auto value=forward?after:before;value.revision=live->revision+1;value.nextId=std::max(value.nextId,live->nextId);
      std::string diagnostic;if(!commitAnimationClipAsset(value,diagnostic,false)) {state_.status=diagnostic;return false;}
      state_.status=forward?"Clipe refeito":"Clipe desfeito";return true;
    });
  }
  return true;
}
} // namespace ae::editor
