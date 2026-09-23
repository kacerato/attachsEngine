#include "editor/editor_session.h"

#include "core/sha256.h"
#include "editor/editor_import_transaction.h"

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
  state_.importPanel=true;state_.importTexture=true;state_.importEnvironment=false;
  state_.importPath=std::move(path);state_.importPage=0;state_.importError=!prepared.valid();
  state_.importReady=prepared.valid();state_.textureImportSettings=settings;
  state_.textureImportPreparedSettings=settings;state_.textureImportReprepare=false;
  state_.textureImportSourceWidth=prepared.sourceWidth;state_.textureImportSourceHeight=prepared.sourceHeight;
  state_.textureImportDroppedMips=prepared.droppedMipLevels;state_.textureImportSourceHasAlpha=prepared.sourceHasAlpha;
  state_.textureImportImage={};
  if(prepared.valid()) {
    const usize baseBytes=static_cast<usize>(prepared.texture->width)*prepared.texture->height*4;
    state_.textureImportImage=preview_.writeViewer(std::span<const u8>(prepared.texture->mipChain).first(baseBytes),
        prepared.texture->width,prepared.texture->height,TexturePreviewChannel::Rgba,0,TexturePreviewBackground::Checker);
    state_.importSummary=prepared.diagnostic;
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
  if(isPlaying()) {diagnostic="Pare a execução antes de publicar.";return false;}
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
  if(!resources::writeTextureAssetCache(prepared,cacheBytes)||
     !EditorImportTransaction::safePath(root,cacheRelative,cachePath)) {
    diagnostic="Derivado de textura inválido ou destino de cache recusado.";return false;
  }
  std::error_code error;std::filesystem::create_directories(cachePath.parent_path(),error);
  if(error||!EditorImportTransaction::write(cachePath,cacheBytes)) {
    diagnostic="Não foi possível gravar o cache preparado da textura.";return false;
  }

  auto next=assets_;
  resources::AssetRecord record=known?*known:resources::AssetRecord{};
  record.guid=guid;record.type=resources::AssetType::Texture;record.path=path;record.source=path;
  record.contentHash=hash;record.importerVersion=resources::TextureAssetImporterRevision;
  record.importerParameters=resources::serializeTextureProfile(settings);
  record.derived.clear();record.dependencies.clear();
  const bool registered=known?next.publishImport(guid,hash,record.importerVersion,record.importerParameters,{},{}):next.add(record);
  if(!registered) {diagnostic="Registro recusou a textura preparada.";return false;}

  EditorImportTransaction transaction(files_.rootPath());
  if(!transaction.begin(path,expectedHash,diagnostic)) return false;
  if(!transaction.commit(bytes,next.serialize())) {
    diagnostic="Não foi possível gravar fonte e registro; a versão anterior foi preservada.";
    if(!transaction.rollback()) diagnostic+=" Recuperação de disco pendente; backups preservados.";
    return false;
  }
  assets_=std::move(next);assetRegistryDirty_=false;
  std::erase_if(decodedTextures_,[&](const auto &entry){return entry.guid==guid;});
  loadTextureAssets();files_.rebuildTree();state_.selectedFile=path;
  state_.status=known?"Textura reimportada; vínculos preservados":"Textura importada: "+path;
  appearanceChanged_=true;
  return true;
}
} // namespace ae::editor
