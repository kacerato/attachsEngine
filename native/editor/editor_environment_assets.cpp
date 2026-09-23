#include "editor/editor_session.h"

#include "core/sha256.h"
#include "editor/editor_import_transaction.h"

#include <algorithm>
#include <chrono>
#include <filesystem>

namespace ae::editor {

bool EditorSession::adoptEnvironmentMap(const resources::AssetGuid &guid,
                                        renderer::SharedEnvironmentMap map,
                                        std::string &diagnostic) {
  diagnostic.clear();
  const auto *record=assets_.find(guid);
  if(!guid.valid()||!map||!map->valid()||!record||record->type!=resources::AssetType::EnvironmentMap) {
    diagnostic="Mapa HDRI não pertence ao registro carregado ou seus derivados são inválidos.";
    return false;
  }
  const auto found=std::find_if(environmentMaps_.begin(),environmentMaps_.end(),
      [&](const auto &entry){return entry.first==guid;});
  if(found==environmentMaps_.end()) environmentMaps_.push_back({guid,std::move(map)});
  else found->second=std::move(map);
  return true;
}

bool EditorSession::commitEnvironmentMap(const std::string &path,std::span<const u8> bytes,
                                         const std::string &expectedHash,
                                         renderer::SharedEnvironmentMap prepared,
                                         std::string &diagnostic) {
  return commitEnvironmentMap(path,bytes,expectedHash,std::move(prepared),
                              resources::EnvironmentMapImportSettings{},diagnostic);
}

bool EditorSession::commitEnvironmentMap(const std::string &path,std::span<const u8> bytes,
                                         const std::string &expectedHash,
                                         renderer::SharedEnvironmentMap prepared,
                                         const resources::EnvironmentMapImportSettings &settings,
                                         std::string &diagnostic) {
  diagnostic.clear();
  if(isPlaying()) {diagnostic="Pare a execução antes de publicar.";return false;}
  const auto hash=Sha256::hex(bytes);
  const resources::EnvironmentMapImportLimits limits{};
  const auto expectedCacheKey=resources::environmentMapCacheKey(hash,settings,limits);
  const auto mipLevels=[](u32 size) {u32 levels=1;while(size>1) {size/=2;++levels;}return levels;};
  if(path.empty()||bytes.empty()||!settings.valid()||!prepared||!prepared->valid()||prepared->sourceHash!=hash||
     prepared->cacheKey!=expectedCacheKey||prepared->panorama.width!=settings.panoramaWidth||
     prepared->panorama.levels!=mipLevels(settings.panoramaWidth)||
     prepared->specular.width!=settings.specularSize||prepared->specular.height!=settings.specularSize||
     prepared->specular.levels!=mipLevels(settings.specularSize)||
     prepared->brdf.width!=settings.brdfSize||prepared->brdf.height!=settings.brdfSize||prepared->brdf.levels!=1) {
    diagnostic="Fonte e derivados HDRI não correspondem à prévia preparada.";return false;
  }
  const auto *known=assets_.findByPath(path);
  if(known&&known->type!=resources::AssetType::EnvironmentMap) {
    diagnostic="O destino já pertence a outro tipo de recurso.";return false;
  }
  const auto guid=known?known->guid:resources::assetGuidFromSeed("environment-map:"+path+":"+
      std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+":"+
      std::to_string(++importInstanceCounter_));
  const std::string cacheRelative=resources::environmentMapCacheRelativePath(prepared->cacheKey);
  std::vector<u8> cacheBytes;
  std::filesystem::path cachePath;
  const auto root=EditorImportTransaction::fromUtf8(files_.rootPath());
  if(!resources::writeEnvironmentMapCache(*prepared,cacheBytes)||
     !EditorImportTransaction::safePath(root,cacheRelative,cachePath)) {
    diagnostic="Derivado HDRI inválido ou destino de cache recusado.";return false;
  }
  std::error_code error;
  std::filesystem::create_directories(cachePath.parent_path(),error);
  if(error||!EditorImportTransaction::write(cachePath,cacheBytes)) {
    diagnostic="Não foi possível gravar o cache HDRI preparado.";return false;
  }

  auto next=assets_;
  resources::AssetRecord record=known?*known:resources::AssetRecord{};
  record.guid=guid;record.type=resources::AssetType::EnvironmentMap;record.path=path;record.source=path;
  record.contentHash=hash;record.importerVersion=resources::EnvironmentMapImporterRevision;
  record.importerParameters=resources::writeEnvironmentMapImportSettings(settings);
  // `.astra/cache` é regenerável e deliberadamente não entra em `derived`:
  // caminhos internos iniciados por ponto não são recursos autorais do registro.
  record.derived.clear();record.dependencies.clear();
  const bool registered=known
      ?next.publishImport(guid,hash,resources::EnvironmentMapImporterRevision,record.importerParameters,
                          {},{})
      :next.add(record);
  if(!registered) {diagnostic="Registro recusou o mapa HDRI preparado.";return false;}

  EditorImportTransaction transaction(files_.rootPath());
  if(!transaction.begin(path,expectedHash,diagnostic)) return false;
  if(!transaction.commit(bytes,next.serialize())) {
    diagnostic="Não foi possível gravar fonte HDRI e registro; a versão anterior foi preservada.";
    if(!transaction.rollback()) diagnostic+=" Recuperação de disco pendente; backups preservados.";
    return false;
  }
  assets_=std::move(next);assetRegistryDirty_=false;
  if(!adoptEnvironmentMap(guid,std::move(prepared),diagnostic)) return false;
  files_.rebuildTree();state_.selectedFile=path;state_.status="Mapa HDRI importado: "+path;
  return true;
}

void EditorSession::showEnvironmentImportPreview(std::string path,
                                                  const renderer::EnvironmentMapResource &map,
                                                  const resources::EnvironmentMapImportSettings &settings) {
  state_.importPanel=true;state_.importReady=map.valid();state_.importError=!map.valid();
  state_.importEnvironment=true;state_.importPath=std::move(path);state_.importPage=0;
  state_.environmentImportSettings=settings;state_.environmentImportReprepare=false;
  state_.importTab=EditorScreenState::ImportTab::Summary;
  state_.importNodes.clear();state_.importMeshes.clear();state_.importTextures.clear();
  state_.importSummary=map.valid()?"HDRI linear preparado para céu, luz difusa e reflexão especular.":
                                  "Os derivados HDRI preparados são inválidos.";
  if(map.valid()) {
    const auto mib=[](u64 bytes){return (bytes+(u64{1}<<19))>>20;};
    state_.importSummary+="\nPanorama: "+std::to_string(map.panorama.width)+"×"+
        std::to_string(map.panorama.height)+" · "+std::to_string(map.panorama.levels)+" níveis.";
    state_.importSummary+="\nReflexão GGX: "+std::to_string(map.specular.width)+"×"+
        std::to_string(map.specular.height)+" · "+std::to_string(map.specular.levels)+" níveis.";
    state_.importSummary+="\nBRDF split-sum: "+std::to_string(map.brdf.width)+"×"+
        std::to_string(map.brdf.height)+" · "+std::to_string(mib(
          (map.panorama.texels.size()+map.specular.texels.size()+map.brdf.texels.size())*sizeof(u16)))+" MB derivados.";
    state_.importSummary+="\nA fonte permanece no projeto; o cache pode ser regenerado.";
    state_.importStatus="HDRI pronto para importar";
  } else state_.importStatus="Importação HDRI não concluída";
}

} // namespace ae::editor
