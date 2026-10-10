#include "editor/editor_session.h"

#include "core/sha256.h"
#include "editor/editor_import_transaction.h"
#include "editor/editor_texture_history.h"

#include <chrono>
#include <filesystem>

namespace ae::editor {
namespace {
resources::TextureImportLimits textureLimits(const resources::GltfImportLimits &source) {
  resources::TextureImportLimits limits;
  limits.image=source.image;
  limits.projectMaximumDimension=source.maximumTextureDimension;
  return limits;
}
}

void EditorSession::showTextureImportPreview(std::string path,
                                              const resources::PreparedTextureImport &prepared,
                                              const resources::TextureProfile &settings) {
  const bool sameTiff=state_.importTexture&&state_.textureImportTiff&&state_.importPath==path;
  state_.importPanel=true;state_.importTexture=true;state_.importEnvironment=false;
  state_.importPath=std::move(path);state_.importPage=sameTiff?state_.importPage:prepared.sourceTiff&&!prepared.valid()?5:0;
  state_.importError=!prepared.valid();
  state_.importReady=prepared.valid();state_.textureImportSettings=settings;
  state_.textureImportPreparedSettings=settings;state_.textureImportReprepare=false;
  state_.textureImportSourceWidth=prepared.sourceWidth;state_.textureImportSourceHeight=prepared.sourceHeight;
  state_.textureImportDroppedMips=prepared.droppedMipLevels;state_.textureImportSourceHasAlpha=prepared.sourceHasAlpha;
  state_.textureImportTiff=prepared.sourceTiff;state_.textureImportTiffBits=prepared.sourceTiffBits;
  state_.textureImportTiffUndefinedExtra=prepared.sourceTiffUndefinedExtra;state_.textureImportTiffHasIcc=prepared.sourceTiffHasIcc;
  state_.textureImportTiffMatrixSrgb=prepared.sourceTiffMatrixSrgb;
  state_.textureImportImage={};
  if(prepared.valid()) {
    const usize baseBytes=static_cast<usize>(prepared.texture->width)*prepared.texture->height*4;
    state_.textureImportImage=preview_.writeViewer(std::span<const u8>(prepared.texture->mipChain).first(baseBytes),
        prepared.texture->width,prepared.texture->height,TexturePreviewChannel::Rgba,0,TexturePreviewBackground::Checker);
    state_.importSummary=prepared.diagnostic;
    for(const auto &loss:prepared.knownLosses) {if(!state_.importSummary.empty())state_.importSummary+=' ';state_.importSummary+=loss;}
    state_.importStatus="Textura pronta para importar";
  } else {
    state_.importSummary=prepared.diagnostic.empty()?"A prévia de textura é inválida.":prepared.diagnostic;
    state_.importStatus="Importação de textura não concluída";
  }
}

bool EditorSession::commitTextureImport(const std::string &path,std::span<const u8> bytes,
                                        const std::string &expectedHash,
                                        const resources::PreparedTextureImport &prepared,
                                        const resources::TextureProfile &settings,
                                        std::string &diagnostic) {
  diagnostic.clear();
  if(isPlaying()||history_.isOpen()) {diagnostic="Finalize a edição e pare a execução antes de publicar.";return false;}
  const auto limits=textureLimits(importLimits_);
  const auto hash=Sha256::hex(bytes);
  const auto expectedKey=resources::textureAssetCacheKey(hash,settings,true,
      EditorMapScene::DefaultTextureSampler,limits);
  if(path.empty()||bytes.empty()||!resources::validTextureProfile(settings)||!prepared.valid()||
     prepared.sourceHash!=hash||prepared.cacheKey!=expectedKey) {
    diagnostic="Fonte, receita e derivados da textura não correspondem à prévia preparada.";return false;
  }
  const auto *known=assets_.findByPath(path);
  if(known&&known->type!=resources::AssetType::Texture) {
    diagnostic="O destino já pertence a outro tipo de recurso.";return false;
  }
  const auto guid=known?known->guid:resources::assetGuidFromSeed("texture:"+path+":"+
      std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+":"+
      std::to_string(++importInstanceCounter_));

  std::vector<u8> cacheBytes;std::filesystem::path cachePath;
  const auto cacheRelative=resources::textureAssetCachePath(guid);
  const auto root=EditorImportTransaction::fromUtf8(files_.rootPath());
  const std::optional<resources::AssetRecord> before=known?std::optional(*known):std::nullopt;
  if(!texture_history::persisted(root,known,path)) {
    diagnostic="Metadados da textura mudaram no disco; reabra o projeto antes de publicar.";return false;
  }
  if(before) {
    std::vector<u8> previous;
    if(!texture_history::readSnapshot(root,before->contentHash,limits.image.maximumEncodedBytes,previous)) {
      std::filesystem::path previousPath;
      if(!EditorImportTransaction::safePath(root,path,previousPath)||
          !EditorImportTransaction::read(previousPath,previous,limits.image.maximumEncodedBytes)||Sha256::hex(previous)!=before->contentHash) {
        diagnostic="A versão anterior não possui cópia autoral íntegra. Restaure a fonte registrada antes de reimportar; nada publicado.";return false;
      }
      if(!texture_history::snapshot(root,before->contentHash,previous,diagnostic))return false;
    }
  }
  if(!texture_history::snapshot(root,hash,bytes,diagnostic))return false;
  if(!resources::writeTextureAssetCache(prepared,cacheBytes)||
     !EditorImportTransaction::safePath(root,cacheRelative,cachePath)) {
    diagnostic="Derivado de textura inválido ou destino de cache recusado.";return false;
  }
  std::error_code error;std::filesystem::create_directories(cachePath.parent_path(),error);
  if(error||!EditorImportTransaction::write(cachePath,cacheBytes)) {
    diagnostic="Não foi possível gravar o cache preparado da textura.";return false;
  }

  resources::AssetRecord record=known?*known:resources::AssetRecord{};
  record.guid=guid;record.type=resources::AssetType::Texture;record.path=path;record.source=path;
  record.contentHash=hash;record.importerVersion=resources::TextureAssetImporterRevision;
  record.importerParameters=resources::serializeTextureProfile(settings);
  record.derived.clear();record.dependencies.clear();
  if(!publishTextureImportCandidate(before,record,bytes,expectedHash,diagnostic))return false;
  state_.selectedFile=path;
  const auto project=files_.rootPath();
  history_.recordResource(before?"Reimportar textura":"Importar textura",[this,project,before,after=record](bool forward) {
    if(files_.rootPath()!=project||isPlaying()||history_.isOpen())return false;
    const auto root=EditorImportTransaction::fromUtf8(project);
    const std::optional<resources::AssetRecord> expected=forward?before:std::optional(after);
    const std::optional<resources::AssetRecord> target=forward?std::optional(after):before;
    std::vector<u8> source;
    // Undo of a first import detaches the resource while preserving its source.
    const auto &sourceHash=target?target->contentHash:after.contentHash;
    if(!texture_history::readSnapshot(root,sourceHash,importLimits_.image.maximumEncodedBytes,source)) {
      state_.status="Cópia autoral da textura indisponível; nada alterado.";return false;
    }
    std::string error;
    const auto diskHash=expected?expected->contentHash:after.contentHash;
    if(!publishTextureImportCandidate(expected,target,source,diskHash,error)) {state_.status=error;return false;}
    state_.status=forward?"Importação de textura refeita":"Importação de textura desfeita; fonte preservada";
    return true;
  });
  state_.status=before?"Textura reimportada; vínculos preservados":"Textura importada: "+path;
  appearanceChanged_=true;
  return true;
}
} // namespace ae::editor
