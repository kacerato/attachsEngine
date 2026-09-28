#include "editor/editor_session.h"

#include "core/sha256.h"
#include "editor/editor_import_transaction.h"

#include "scene/component_properties.h"
#include "scene/environment.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <limits>

namespace ae::editor {

namespace {
bool sameRecipe(const resources::EnvironmentMapImportSettings &a,const resources::EnvironmentMapImportSettings &b) {
  return resources::writeEnvironmentMapImportSettings(a)==resources::writeEnvironmentMapImportSettings(b);
}
u32 &recipeField(resources::EnvironmentMapImportSettings &s,u32 field) {
  u32 *values[]{&s.panoramaWidth,&s.specularSize,&s.specularSamples,&s.brdfSize,&s.brdfSamples};return *values[field];
}
bool sameEnvironmentRecord(const resources::AssetRecord &a,const resources::AssetRecord &b) {
  return a.guid==b.guid && a.type==b.type && a.path==b.path && a.source==b.source &&
      a.contentHash==b.contentHash && a.importerVersion==b.importerVersion && a.importerParameters==b.importerParameters;
}
}

void EditorSession::refreshMultiEnvironmentAssets(bool profiles) {
  auto &view=state_.multiAsset;
  view.kind=profiles?EditorScreenState::MultiAssetView::Kind::Profiles:EditorScreenState::MultiAssetView::Kind::EnvironmentMaps;
  view.title=std::to_string(view.items.size())+(profiles?" perfis de ambiente":" mapas HDRI");view.note.clear();
  resources::AssetGuid active;std::vector<MultiEnvironmentDraft> drafts;
  for(const auto &item:view.items) {
    const auto *record=assets_.findByPath(item.path);
    resources::EnvironmentMapImportSettings saved;
    if(!record || (profiles?!findEnvironmentProfile(record->guid):
        !resources::readEnvironmentMapImportSettings(record->importerParameters,saved))) {
      view.kind=EditorScreenState::MultiAssetView::Kind::Other;view.note="Um recurso está ilegível. Abra-o sozinho para verificar.";
      state_.environmentInspector={};state_.profileInspector={};multiProfiles_.clear();multiEnvironments_.clear();return;
    }
    if(item.active) active=record->guid;
    if(profiles) multiProfiles_.push_back(record->guid);
    else {
      MultiEnvironmentDraft draft{record->guid,saved,saved};
      // Rascunho conserva também a base: uma reimportação externa não pode
      // virar a nova base silenciosamente e depois ser sobrescrita por Aplicar.
      for(const auto &old:multiEnvironments_) if(old.guid==record->guid && !sameRecipe(old.saved,old.draft)) draft=old;
      drafts.push_back(draft);
    }
  }
  if(profiles) {
    multiEnvironments_.clear();if(state_.profileInspector!=active) openProfileInspector(active);
  } else {
    multiEnvironments_=std::move(drafts);
    if(state_.environmentInspector!=active) openEnvironmentInspector(active);
    state_.profileInspector={};
    for(const auto &entry:multiEnvironments_) {
      if(entry.guid==active) {state_.environmentDraft=entry.draft;state_.environmentSaved=entry.saved;}
      if(!sameRecipe(entry.draft,entry.saved)) ++state_.environmentPending;
    }
    for(u32 field=0;field<5;++field) for(auto &entry:multiEnvironments_)
      if(recipeField(entry.draft,field)!=recipeField(state_.environmentDraft,field)) state_.environmentMixed|=static_cast<u8>(1u<<field);
  }
}

void EditorSession::editEnvironmentRecipe(u32 field,bool up) {
  if(field>=5) return;
  const std::span<const u32> steps[]{resources::EnvironmentPanoramaSteps,resources::EnvironmentSpecularSizeSteps,
      resources::EnvironmentSpecularSampleSteps,resources::EnvironmentBrdfSizeSteps,resources::EnvironmentBrdfSampleSteps};
  auto &value=recipeField(state_.environmentDraft,field);value=resources::stepEnvironmentMapChoice(value,steps[field],up);
  if(multiAssetEditing_) for(auto &entry:multiEnvironments_) recipeField(entry.draft,field)=value;
}
void EditorSession::revertEnvironmentRecipes() {
  state_.environmentDraft=state_.environmentSaved;
  if(multiAssetEditing_) for(auto &entry:multiEnvironments_) entry.draft=entry.saved;
}
bool EditorSession::showEnvironmentValueMenu(u32 field) {
  if(field>=5 || !(state_.environmentMixed&(1u<<field))) return false;
  auto &menu=state_.setValueMenu;menu.key="asset.hdri."+std::to_string(field);menu.label="Receita HDRI";menu.rows.clear();
  for(u32 i=0;i<multiEnvironments_.size();++i) {
    const auto *record=assets_.find(multiEnvironments_[i].guid);
    if(record) menu.rows.push_back({i,record->path+" · "+std::to_string(recipeField(multiEnvironments_[i].draft,field))});
  }
  return true;
}
bool EditorSession::applyEnvironmentValue(u32 row) {
  const auto &menu=state_.setValueMenu;const auto field=static_cast<u32>(std::stoul(menu.key.substr(11)));
  if(field>=5 || row>=menu.rows.size() || menu.rows[row].first>=multiEnvironments_.size()) return false;
  const u32 value=recipeField(multiEnvironments_[menu.rows[row].first].draft,field);
  for(auto &entry:multiEnvironments_) recipeField(entry.draft,field)=value;
  recipeField(state_.environmentDraft,field)=value;state_.status="Valor copiado; Aplicar prepara os mapas selecionados";return true;
}
bool EditorSession::requestEnvironmentBatch() {
  if(isPlaying() || history_.isOpen() || !environmentBatchRequest_.empty()) return false;
  std::vector<EnvironmentBatchEdit> edits;bool changed=false;
  for(const auto &draft:multiEnvironments_) {
    changed|=!sameRecipe(draft.saved,draft.draft);
    const auto *record=assets_.find(draft.guid);resources::EnvironmentMapImportSettings saved;
    if(!record || !resources::readEnvironmentMapImportSettings(record->importerParameters,saved) || !sameRecipe(saved,draft.saved)) {
      state_.status="Receita mudou; reabra a seleção antes de aplicar.";return false;
    }
    auto after=*record;after.importerParameters=resources::writeEnvironmentMapImportSettings(draft.draft);
    after.importerVersion=resources::EnvironmentMapImporterRevision;edits.push_back({*record,std::move(after)});
  }
  if(!changed) return true;
  environmentBatchRequest_=std::move(edits);
  state_.status="Preparando receitas HDRI; nenhuma será publicada até todas ficarem prontas";return true;
}

bool EditorSession::prepareEnvironmentBatch(const std::string &project,const std::vector<EnvironmentBatchEdit> &edits,
    resources::EnvironmentMapCancel cancel,std::vector<PreparedEnvironmentEdit> &prepared,std::string &diagnostic,bool allowImport) {
  prepared.clear();diagnostic.clear();const auto root=EditorImportTransaction::fromUtf8(project);
  const resources::EnvironmentMapImportLimits limits;u64 total=0;
  for(const auto &edit:edits) {
    if(cancel.cancelled()) {diagnostic="Preparação cancelada.";return false;}
    resources::EnvironmentMapImportSettings before,after;
    if(edit.before.guid!=edit.after.guid || edit.before.path!=edit.after.path || edit.before.contentHash!=edit.after.contentHash ||
       edit.before.type!=resources::AssetType::EnvironmentMap || edit.after.type!=resources::AssetType::EnvironmentMap ||
       !resources::readEnvironmentMapImportSettings(edit.before.importerParameters,before) ||
       !resources::readEnvironmentMapImportSettings(edit.after.importerParameters,after)) {diagnostic="Receita HDRI inválida.";return false;}
    std::filesystem::path path;std::vector<u8> source;
    if(!EditorImportTransaction::safePath(root,edit.before.path,path)||!EditorImportTransaction::read(path,source,limits.maximumSourceBytes)||
       Sha256::hex(source)!=edit.before.contentHash) {diagnostic="Fonte HDRI mudou: "+edit.before.path;return false;}
    const auto load=[&](const resources::EnvironmentMapImportSettings &settings,renderer::SharedEnvironmentMap &map) {
      const auto key=resources::environmentMapCacheKey(edit.before.contentHash,settings,limits);
      std::filesystem::path cache;std::vector<u8> bytes;
      if(!EditorImportTransaction::safePath(root,resources::environmentMapCacheRelativePath(key),cache)) return false;
      if(EditorImportTransaction::read(cache,bytes) && resources::readEnvironmentMapCache(bytes,key,limits,map)) return true;
      if(!allowImport) {diagnostic="Cache do histórico indisponível; nenhuma receita alterada.";return false;}
      if(!resources::importRadianceEnvironmentMap(source,settings,limits,cancel,map,diagnostic)||!resources::writeEnvironmentMapCache(*map,bytes)) return false;
      std::error_code error;std::filesystem::create_directories(cache.parent_path(),error);
      if(error || !EditorImportTransaction::write(cache,bytes)) {diagnostic="Não foi possível preservar o cache HDRI.";return false;}
      return true;
    };
    // Preserve o derivado anterior no cache antes de preparar o novo. O histórico
    // guarda receitas e hashes, não dezenas de cópias de panoramas na memória.
    renderer::SharedEnvironmentMap map;
    if(allowImport && !load(before,map)) return false;
    map.reset();if(!load(after,map)) return false;
    total+=(map->panorama.texels.size()+map->specular.texels.size()+map->brdf.texels.size())*sizeof(u16);
    if(total>limits.maximumOutputBytes) {diagnostic="O lote HDRI excede 128 MB de derivados; selecione menos mapas.";return false;}
    prepared.push_back({edit,std::move(map)});
  }
  if(cancel.cancelled()) {diagnostic="Preparação cancelada.";return false;}
  return true;
}

bool EditorSession::commitEnvironmentBatch(const std::vector<PreparedEnvironmentEdit> &prepared,std::string &diagnostic,bool recordHistory) {
  diagnostic.clear();if(isPlaying()||history_.isOpen()) {diagnostic="Finalize a edição antes de publicar HDRI.";return false;}
  if(prepared.empty()) return true;
  auto next=assets_;std::vector<EnvironmentBatchEdit> edits;
  std::vector<u8> firstSource;
  const auto root=EditorImportTransaction::fromUtf8(files_.rootPath());const resources::EnvironmentMapImportLimits limits;
  for(const auto &entry:prepared) {
    const auto &edit=entry.edit;const auto *current=assets_.find(edit.before.guid);
    resources::EnvironmentMapImportSettings settings;std::filesystem::path path;std::vector<u8> source;
    if(!current || !sameEnvironmentRecord(*current,edit.before) || edit.before.guid!=edit.after.guid ||
       edit.before.path!=edit.after.path || edit.after.type!=resources::AssetType::EnvironmentMap ||
       edit.before.contentHash!=edit.after.contentHash || !entry.map || !entry.map->valid() ||
       !resources::readEnvironmentMapImportSettings(edit.after.importerParameters,settings) ||
       entry.map->panorama.width!=settings.panoramaWidth || entry.map->specular.width!=settings.specularSize ||
       entry.map->brdf.width!=settings.brdfSize || entry.map->sourceHash!=edit.before.contentHash || entry.map->cacheKey!=resources::environmentMapCacheKey(edit.before.contentHash,settings,limits) ||
       !EditorImportTransaction::safePath(root,edit.before.path,path) || !EditorImportTransaction::read(path,source,limits.maximumSourceBytes) ||
       Sha256::hex(source)!=edit.before.contentHash) {diagnostic="HDRI mudou durante a preparação: "+edit.before.path;return false;}
    for(const auto &prior:edits) if(prior.before.guid==edit.before.guid) {diagnostic="HDRI duplicado no lote.";return false;}
    if(edits.empty()) firstSource=std::move(source);
    if(!next.publishImport(edit.after.guid,edit.after.contentHash,edit.after.importerVersion,edit.after.importerParameters,
        edit.after.derived,edit.after.dependencies)) {diagnostic="Registro recusou a receita HDRI.";return false;}
    edits.push_back(edit);
  }
  // As fontes não mudam: só o registro passa a apontar para receitas cujos
  // derivados já estão completos. A primeira fonte ancora o journal existente.
  EditorImportTransaction transaction(files_.rootPath());
  if(!transaction.begin(edits.front().before.path,edits.front().before.contentHash,diagnostic)) return false;
  if(!transaction.commit(firstSource,next.serialize())) {
    diagnostic=transaction.rollback()?"Registro HDRI restaurado; nenhum mapa publicado.":"Recuperação HDRI pendente no journal.";return false;
  }
  assets_=std::move(next);assetRegistryDirty_=false;
  for(const auto &entry:prepared) if(!adoptEnvironmentMap(entry.edit.after.guid,entry.map,diagnostic)) return false;
  appearanceChanged_=true;
  for(auto &draft:multiEnvironments_) for(const auto &edit:edits) if(draft.guid==edit.after.guid) {
    resources::readEnvironmentMapImportSettings(edit.after.importerParameters,draft.saved);draft.draft=draft.saved;
  }
  if(recordHistory) {
    const auto project=files_.rootPath();
    history_.recordResource("Receitas HDRI",[this,project,edits](bool forward) {
      if(files_.rootPath()!=project) {state_.status="Projeto do histórico indisponível.";return false;}
      auto replay=edits;if(!forward) for(auto &edit:replay) std::swap(edit.before,edit.after);
      std::vector<PreparedEnvironmentEdit> prepared;std::string error;
      if(!prepareEnvironmentBatch(project,replay,{},prepared,error,false)||!commitEnvironmentBatch(prepared,error,false)) {
        state_.status=error;return false;
      }
      state_.status=forward?"Receitas HDRI refeitas":"Receitas HDRI desfeitas";return true;
    });
  }
  state_.status="Receitas HDRI aplicadas em conjunto";return true;
}

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

namespace {
float halfToFloat(u16 half) {
  const u32 sign=static_cast<u32>(half>>15)<<31;u32 exponent=(half>>10)&31u,mantissa=half&1023u,bits=0;
  if(exponent==0) {
    if(mantissa) {
      exponent=113;while(!(mantissa&1024u)) {mantissa<<=1;--exponent;}
      bits=sign|(exponent<<23)|((mantissa&1023u)<<13);
    } else bits=sign;
  } else if(exponent==31) bits=sign|0x7f800000u|(mantissa<<13);
  else bits=sign|((exponent+112u)<<23)|(mantissa<<13);
  float value;std::memcpy(&value,&bits,sizeof value);return value;
}
}

bool EditorSession::openEnvironmentInspector(const resources::AssetGuid &guid) {
  const auto *record=assets_.find(guid);
  resources::EnvironmentMapImportSettings saved;
  if(!record || record->type!=resources::AssetType::EnvironmentMap ||
     !resources::readEnvironmentMapImportSettings(record->importerParameters,saved)) return false;
  state_.environmentInspector=guid;state_.environmentSaved=state_.environmentDraft=saved;
  state_.environmentExposure=0;state_.environmentUse=0;state_.propertyPage=0;
  state_.materialInspector={};state_.textureInspector=false;state_.textureManager=false;closeTextureViewer();
  state_.compactPanel=EditorScreenState::CompactPanel::Inspector;
  environmentPreviewSource_=nullptr;
  refreshEnvironmentInspector();
  return true;
}

// Unity 6000.0 Texture Import Settings (Texture Shape Cube, Mapping Latitude-
// Longitude): o mapa HDRI com o que foi derivado dele, quem usa e a receita.
void EditorSession::refreshEnvironmentInspector() {
  const auto guid=state_.environmentInspector;
  const auto *record=assets_.find(guid);
  resources::EnvironmentMapImportSettings saved;
  if(!record || record->type!=resources::AssetType::EnvironmentMap ||
     !resources::readEnvironmentMapImportSettings(record->importerParameters,saved)) {
    state_.environmentInspector={};state_.status="O mapa HDRI aberto não existe mais no projeto";return;
  }
  state_.environmentInspectorPath=record->path;
  // Uma reimportação concluída troca a receita salva; o rascunho segue a salva
  // enquanto não houver mudança pendente.
  const auto same=[](const auto &a,const auto &b) {
    return a.panoramaWidth==b.panoramaWidth && a.specularSize==b.specularSize && a.brdfSize==b.brdfSize &&
           a.specularSamples==b.specularSamples && a.brdfSamples==b.brdfSamples;
  };
  if(previewSecondary_ || multiEnvironments_.size()<2) {
    if(same(state_.environmentDraft,state_.environmentSaved)) state_.environmentDraft=saved;
    state_.environmentSaved=saved;
  }
  // Usos: componentes Ambiente da cena e perfis de ambiente do projeto.
  state_.environmentObjects.clear();state_.environmentProfiles=0;
  std::vector<EditorEntityId> ids;document_.collectSubtree(document_.root(),ids);
  for(const auto id:ids) if(const auto *entity=document_.find(id))
    if(const auto *environment=static_cast<const scene::Environment *>(entity->components.find(scene::Environment::descriptor));
       environment && environment->values.environmentMap==guid) state_.environmentObjects.push_back(id);
  for(const auto &profile:environmentProfiles_) state_.environmentProfiles+=profile.values.environmentMap==guid;
  // Derivados do mapa carregado.
  state_.environmentDerived.clear();
  const auto map=findEnvironmentMap(guid);
  if(!map || !map->valid()) {
    state_.environmentDerived.push_back("Derivados não carregados: Reimportar gera o cache de novo");
    state_.environmentPreview={};environmentPreviewSource_=nullptr;return;
  }
  const auto mib=[](u64 bytes){return (bytes+(u64{1}<<19))>>20;};
  state_.environmentDerived.push_back("Panorama "+std::to_string(map->panorama.width)+"×"+std::to_string(map->panorama.height)+
                                      " · "+std::to_string(map->panorama.levels)+" níveis");
  state_.environmentDerived.push_back("Reflexão GGX "+std::to_string(map->specular.width)+"×"+std::to_string(map->specular.height)+
                                      " · "+std::to_string(map->specular.levels)+" níveis");
  state_.environmentDerived.push_back("BRDF "+std::to_string(map->brdf.width)+"×"+std::to_string(map->brdf.height)+" · "+
      std::to_string(mib((map->panorama.texels.size()+map->specular.texels.size()+map->brdf.texels.size())*sizeof(u16)))+
      " MB derivados · irradiância SH9");
  if(environmentPreviewSource_!=map.get() || environmentPreviewExposure_!=state_.environmentExposure) writeEnvironmentPreview();
}

void EditorSession::writeEnvironmentPreview() {
  const auto map=findEnvironmentMap(state_.environmentInspector);
  if(!map || !map->panorama.valid()) {state_.environmentPreview={};return;}
  // O nível do panorama que cabe na região do visualizador, com tonemap de
  // Reinhard e a exposição da prévia (não muda o recurso nem a cena).
  const auto &chain=map->panorama;
  u32 level=0,width=chain.width,height=chain.height;usize offset=0;
  while(level+1<chain.levels && width>TextureViewerSize) {
    offset+=static_cast<usize>(width)*height*4;width=std::max(1u,width/2);height=std::max(1u,height/2);++level;
  }
  const float scale=std::exp2(state_.environmentExposure);
  std::vector<u8> rgba(static_cast<usize>(width)*height*4);
  for(usize i=0;i<static_cast<usize>(width)*height;++i) {
    for(u32 c=0;c<3;++c) {
      const float linear=std::max(0.0f,halfToFloat(chain.texels[offset+i*4+c]))*scale;
      const float mapped=std::pow(linear/(1.0f+linear),1.0f/2.2f);
      rgba[i*4+c]=static_cast<u8>(std::clamp(mapped*255.0f+.5f,0.0f,255.0f));
    }
    rgba[i*4+3]=255;
  }
  // A janela focada escreve na região própria do atlas: as duas prévias convivem.
  state_.environmentPreview=previewSecondary_?preview_.writeSecondaryViewer(rgba,width,height):
      preview_.writeViewer(rgba,width,height,TexturePreviewChannel::Rgba,0,TexturePreviewBackground::Black);
  environmentPreviewSource_=map.get();environmentPreviewExposure_=state_.environmentExposure;
}

namespace {
// O que o perfil guarda: o `values` do componente, com ativo e prioridade
// normalizados (são da instância). Uma propriedade é "do perfil" quando
// escrevê-la muda esse conteúdo — descoberto pelo esquema, sem lista paralela.
std::string profileContent(const scene::Environment &environment) {
  resources::EnvironmentProfile probe;probe.values=environment.values;
  probe.values.active=true;probe.values.priority=0;probe.name="p";
  return probe.serialize();
}
struct ProfileSchema {
  std::vector<const scene::ComponentBoolean *> booleans;
  std::vector<const scene::ComponentEnum *> enums;
  std::vector<const scene::ComponentNumber *> numbers;
  std::vector<const scene::ComponentTriple *> triples;
  bool environmentMap=false;
};
const ProfileSchema &profileSchema() {
  static const ProfileSchema schema=[] {
    ProfileSchema out;
    const auto &type=scene::Environment::descriptor;
    scene::Environment base;const auto original=profileContent(base);
    const auto changes=[&](auto &&write) {auto probe=base;write(probe);return profileContent(probe)!=original;};
    for(const auto &p:type.booleans) if(p.write && changes([&](scene::Environment &e){p.write(e,!p.read(base));}))
      out.booleans.push_back(&p);
    for(const auto &p:type.enums) if(p.write) for(const auto &option:p.options)
      if(option.value!=p.read(base) && changes([&](scene::Environment &e){p.write(e,option.value);})) {out.enums.push_back(&p);break;}
    std::vector<std::string_view> channels;
    for(const auto &t:type.triples) for(const auto &c:t.channels) channels.push_back(c);
    for(const auto &p:type.numbers) {
      if(!p.write || p.minimum>=p.maximum) continue;
      const bool profile=changes([&](scene::Environment &e){auto *v=p.write(e);*v=*v==p.minimum?p.maximum:p.minimum;});
      if(!profile) continue;
      if(std::find(channels.begin(),channels.end(),p.id)==channels.end()) out.numbers.push_back(&p);
    }
    for(const auto &t:type.triples) for(const auto &c:t.channels) {
      bool profile=false;
      for(const auto &p:type.numbers) if(p.id==c && p.write && p.minimum<p.maximum &&
          changes([&](scene::Environment &e){auto *v=p.write(e);*v=*v==p.minimum?p.maximum:p.minimum;})) profile=true;
      if(profile) {out.triples.push_back(&t);break;}
    }
    for(const auto &binding:type.resourceBindings) if(binding.kind==resources::AssetType::EnvironmentMap && binding.write &&
        changes([&](scene::Environment &e){binding.write(e,0,resources::assetGuidFromSeed("perfil:sonda"));})) out.environmentMap=true;
    return out;
  }();
  return schema;
}
const scene::ComponentNumber *numberById(std::string_view id) {
  for(const auto &p:scene::Environment::descriptor.numbers) if(p.id==id) return &p;
  return nullptr;
}
}

namespace {
std::string profileRowText(const resources::EnvironmentProfile &profile,const EditorScreenState::ProfileRow &row,bool precise=true) {
  scene::Environment proxy;proxy.values=profile.values;
  using Kind=EditorScreenState::ProfileRow::Kind;
  const auto &type=scene::Environment::descriptor;
  if(row.kind==Kind::Boolean) for(const auto &p:type.booleans) if(p.id==row.id) return p.read(proxy)?"Ligado":"Desligado";
  if(row.kind==Kind::Enum) for(const auto &p:type.enums) if(p.id==row.id)
    for(const auto &option:p.options) if(option.value==p.read(proxy)) return std::string(option.name);
  char text[96];
  if(row.kind==Kind::Number) if(const auto *p=numberById(row.id)) {
    std::snprintf(text,sizeof text,precise?"%.9g":"%.6g",static_cast<double>(p->read(proxy)));return text;
  }
  if(row.kind==Kind::Triple) for(const auto &t:type.triples) if(t.id==row.id) {
    float v[3]{};for(u32 i=0;i<3;++i) if(const auto *p=numberById(t.channels[i])) v[i]=p->read(proxy);
    std::snprintf(text,sizeof text,precise?"%.9g %.9g %.9g":"%.6g %.6g %.6g",static_cast<double>(v[0]),static_cast<double>(v[1]),static_cast<double>(v[2]));return text;
  }
  if(row.kind==Kind::EnvironmentMap) return profile.values.environmentMap.text();
  return {};
}
bool profileRowAvailable(const resources::EnvironmentProfile &profile,const EditorScreenState::ProfileRow &row) {
  scene::Environment proxy;proxy.values=profile.values;
  const scene::PropertyPresentation *presentation=nullptr;
  const auto &type=scene::Environment::descriptor;
  using Kind=EditorScreenState::ProfileRow::Kind;
  if(row.kind==Kind::Boolean) for(const auto &p:type.booleans) if(p.id==row.id) presentation=&p.presentation;
  if(row.kind==Kind::Enum) for(const auto &p:type.enums) if(p.id==row.id) presentation=&p.presentation;
  if(row.kind==Kind::Number) if(const auto *p=numberById(row.id)) presentation=&p->presentation;
  if(row.kind==Kind::Triple) for(const auto &t:type.triples) if(t.id==row.id)
    if(const auto *p=numberById(t.channels[0])) presentation=&p->presentation;
  return !presentation || (presentation->isVisible(proxy) && presentation->isEditable(proxy) && presentation->hasConsumer());
}
}

bool EditorSession::showProfileValueMenu(u32 index) {
  if(index>=state_.profileRows.size() || !state_.profileRows[index].mixed || !state_.profileRows[index].editable) return false;
  const auto &row=state_.profileRows[index];auto &menu=state_.setValueMenu;
  menu.key="asset.profile."+std::to_string(index);menu.label=row.label;menu.rows.clear();
  for(u32 i=0;i<multiProfiles_.size();++i) if(const auto *profile=findEnvironmentProfile(multiProfiles_[i])) {
    auto value=profileRowText(*profile,row,false);
    if(row.kind==EditorScreenState::ProfileRow::Kind::EnvironmentMap) {
      const auto *record=assets_.find(profile->values.environmentMap);
      value=record?record->path:profile->values.environmentMap.valid()?"Mapa ausente":"Ambiente padrão";
    }
    menu.rows.push_back({i,profile->name+" · "+value});
  }
  return true;
}
bool EditorSession::applyProfileValue(u32 index) {
  const auto &menu=state_.setValueMenu;
  if(index>=menu.rows.size() || menu.rows[index].first>=multiProfiles_.size()) return false;
  const auto n=static_cast<u32>(std::stoul(menu.key.substr(14)));
  if(n>=state_.profileRows.size()) return false;
  const auto row=state_.profileRows[n];const auto *source=findEnvironmentProfile(multiProfiles_[menu.rows[index].first]);
  if(!source) return false;
  scene::Environment proxy;proxy.values=source->values;
  scene::ComponentPropertyValue value=false;float triple[3]{};
  using Kind=EditorScreenState::ProfileRow::Kind;
  const auto &type=scene::Environment::descriptor;
  if(row.kind==Kind::Boolean) for(const auto &p:type.booleans) if(p.id==row.id) value=p.read(proxy);
  if(row.kind==Kind::Enum) for(const auto &p:type.enums) if(p.id==row.id) value=p.read(proxy);
  if(row.kind==Kind::Number) if(const auto *p=numberById(row.id)) value=p->read(proxy);
  if(row.kind==Kind::Triple) for(const auto &t:type.triples) if(t.id==row.id)
    for(u32 i=0;i<3;++i) if(const auto *p=numberById(t.channels[i])) triple[i]=p->read(proxy);
  std::string diagnostic;
  const bool applied=editProfileProperty(row,value,triple,&source->values.environmentMap,diagnostic);
  state_.status=diagnostic;return applied;
}

bool EditorSession::commitProfileBatch(const std::vector<resources::EnvironmentProfile> &candidates,
                                      std::string &diagnostic,bool recordHistory) {
  diagnostic.clear();
  if(isPlaying()||history_.isOpen()) {diagnostic="Finalize a edição antes de alterar os perfis.";return false;}
  auto next=assets_;
  std::vector<resources::EnvironmentProfile> before,after;
  std::vector<EditorImportTransaction::TextEdit> edits;
  const auto root=EditorImportTransaction::fromUtf8(files_.rootPath());
  for(const auto &candidate:candidates) {
    const auto *current=findEnvironmentProfile(candidate.guid);const auto *record=assets_.find(candidate.guid);
    if(!candidate.valid() || !current || !record || record->type!=resources::AssetType::EnvironmentProfile ||
       current->revision==std::numeric_limits<u32>::max() || candidate.revision!=current->revision+1) {
      diagnostic="Perfil ou revisão indisponível.";return false;
    }
    std::filesystem::path path;std::vector<u8> bytes;resources::EnvironmentProfile disk;
    if(!EditorImportTransaction::safePath(root,record->path,path)||!EditorImportTransaction::read(path,bytes)||
       !resources::EnvironmentProfile::deserialize(std::string(bytes.begin(),bytes.end()),disk)||disk.serialize()!=current->serialize()) {
      diagnostic="Perfil mudou no disco: "+record->path;return false;
    }
    auto same=candidate;same.revision=current->revision;if(same.serialize()==current->serialize()) continue;
    std::vector<resources::AssetGuid> dependencies;
    if(candidate.values.environmentMap.valid()) {
      const auto *map=assets_.find(candidate.values.environmentMap);
      if(!map || map->type!=resources::AssetType::EnvironmentMap) {diagnostic="Mapa HDRI do perfil indisponível.";return false;}
      dependencies.push_back(map->guid);
    }
    const auto text=candidate.serialize();
    if(!next.publishImport(candidate.guid,Sha256::hex({reinterpret_cast<const u8*>(text.data()),text.size()}),
        record->importerVersion,record->importerParameters,record->derived,std::move(dependencies))) {
      diagnostic="Registro recusou o perfil.";return false;
    }
    edits.push_back({record->path,Sha256::hex(bytes),text});before.push_back(*current);after.push_back(candidate);
  }
  if(after.empty()) return true;
  if(!EditorImportTransaction::publishTextBatch(files_.rootPath(),edits,next.serialize(),diagnostic)) return false;
  assets_=std::move(next);assetRegistryDirty_=true;
  for(const auto &value:after) {
    for(auto &profile:environmentProfiles_) if(profile.guid==value.guid) {profile=value;break;}
    synchronizeEnvironmentProfile(value);
  }
  if(recordHistory) {
    const auto project=files_.rootPath();
    history_.recordResource(after.size()>1?"Perfis de ambiente":"Perfil de ambiente",[this,before,after,project](bool forward) {
      if(files_.rootPath()!=project) {state_.status="Projeto do histórico indisponível.";return false;}
      std::vector<resources::EnvironmentProfile> candidates;
      for(usize i=0;i<before.size();++i) {
        const auto *current=findEnvironmentProfile(before[i].guid);
        if(!current) {state_.status="Perfil do histórico indisponível.";return false;}
        auto expected=forward?before[i]:after[i];expected.revision=current->revision;
        if(expected.serialize()!=current->serialize()) {state_.status="Perfil mudou; histórico preservado.";return false;}
        auto value=forward?after[i]:before[i];value.revision=current->revision+1;candidates.push_back(std::move(value));
      }
      std::string error;if(!commitProfileBatch(candidates,error,false)) {state_.status=error;return false;}
      state_.status=forward?"Perfis refeitos":"Perfis desfeitos";return true;
    });
  }
  return true;
}

bool EditorSession::openProfileInspector(const resources::AssetGuid &guid) {
  if(!findEnvironmentProfile(guid)) return false;
  state_.profileInspector=guid;state_.profileGroup=0;state_.profileUse=0;state_.propertyPage=0;
  state_.environmentInspector={};state_.materialInspector={};state_.textureInspector=false;state_.textureManager=false;
  closeTextureViewer();
  state_.compactPanel=EditorScreenState::CompactPanel::Inspector;
  refreshProfileInspector();
  return true;
}

// Unity 6000.0 Volume Profile: o perfil sozinho, com os mesmos controles do
// componente que o usa, só o que ele guarda.
void EditorSession::refreshProfileInspector() {
  const auto *profile=findEnvironmentProfile(state_.profileInspector);
  const auto *record=assets_.find(state_.profileInspector);
  if(!profile || !record) {state_.profileInspector={};state_.status="O perfil aberto não existe mais no projeto";return;}
  state_.profileInspectorName=profile->name;state_.profileInspectorPath=record->path;state_.profileInspectorRevision=profile->revision;
  state_.profileObjects.clear();
  std::vector<EditorEntityId> ids;document_.collectSubtree(document_.root(),ids);
  for(const auto id:ids) if(const auto *entity=document_.find(id))
    if(const auto *environment=static_cast<const scene::Environment *>(entity->components.find(scene::Environment::descriptor));
       environment && environment->profile==profile->guid) state_.profileObjects.push_back(id);
  scene::Environment proxy;proxy.values=profile->values;
  const auto &schema=profileSchema();
  // Grupos na ordem em que aparecem no esquema.
  auto &groups=state_.profileGroups;groups.clear();
  const auto group=[&](std::string_view name) {
    const std::string value(name.empty()?std::string_view("Geral"):name);
    if(std::find(groups.begin(),groups.end(),value)==groups.end()) groups.push_back(value);
  };
  // Ordem do esquema: os números (com os canais das cores) definem a sequência
  // dos grupos, como nas abas do cartão do componente; depois opções e chaves.
  for(const auto &p:scene::Environment::descriptor.numbers) {
    bool profile=std::find(schema.numbers.begin(),schema.numbers.end(),&p)!=schema.numbers.end();
    for(const auto *t:schema.triples) for(const auto &c:t->channels) profile|=c==p.id;
    if(profile) group(p.presentation.group);
  }
  for(const auto *p:schema.enums) group(p->presentation.group);
  for(const auto *p:schema.booleans) group(p->presentation.group);
  if(schema.environmentMap) group("HDRI");
  state_.profileGroup=std::min<u32>(state_.profileGroup,groups.empty()?0:static_cast<u32>(groups.size()-1));
  const std::string current=groups.empty()?std::string():groups[state_.profileGroup];
  const auto in=[&](std::string_view name){return (name.empty()?std::string_view("Geral"):name)==current;};
  auto &rows=state_.profileRows;rows.clear();
  using Row=EditorScreenState::ProfileRow;
  const auto usable=[&](const scene::PropertyPresentation &p){return p.isEditable(proxy) && p.hasConsumer();};
  for(const auto *p:schema.booleans) if(in(p->presentation.group) && p->presentation.isVisible(proxy)) {
    Row row;row.kind=Row::Kind::Boolean;row.id=p->id;row.label=p->name;row.on=p->read(proxy);row.editable=usable(p->presentation);
    rows.push_back(std::move(row));
  }
  for(const auto *p:schema.enums) if(in(p->presentation.group) && p->presentation.isVisible(proxy)) {
    Row row;row.kind=Row::Kind::Enum;row.id=p->id;row.label=p->name;row.editable=usable(p->presentation);
    const u32 value=p->read(proxy);
    for(const auto &option:p->options) if(option.value==value) row.value=option.name;
    rows.push_back(std::move(row));
  }
  for(const auto *t:schema.triples) {
    const auto *first=numberById(t->channels[0]);
    if(!first || !in(first->presentation.group) || !first->presentation.isVisible(proxy)) continue;
    Row row;row.kind=Row::Kind::Triple;row.id=t->id;row.label=t->name;row.editable=usable(first->presentation);
    row.color=t->kind==scene::ComponentTripleKind::LinearColor;
    char text[64];
    for(u32 axis=0;axis<3;++axis) if(const auto *c=numberById(t->channels[axis])) row.rgb[axis]=c->read(proxy);
    std::snprintf(text,sizeof text,"%.3g  %.3g  %.3g",static_cast<double>(row.rgb[0]),static_cast<double>(row.rgb[1]),static_cast<double>(row.rgb[2]));
    row.value=text;rows.push_back(std::move(row));
  }
  for(const auto *p:schema.numbers) if(in(p->presentation.group) && p->presentation.isVisible(proxy)) {
    Row row;row.kind=Row::Kind::Number;row.id=p->id;row.label=p->name;row.editable=usable(p->presentation);
    char text[48];std::snprintf(text,sizeof text,"%.4g",static_cast<double>(p->read(proxy)));
    row.value=std::string(text)+(p->presentation.unit.empty()?std::string():" "+std::string(p->presentation.unit));
    rows.push_back(std::move(row));
  }
  if(schema.environmentMap && current=="HDRI") {
    Row row;row.kind=Row::Kind::EnvironmentMap;row.label="Mapa HDRI";
    const auto *map=proxy.values.environmentMap.valid()?assets_.find(proxy.values.environmentMap):nullptr;
    row.value=map?map->path.substr(map->path.rfind('/')+1):proxy.values.environmentMap.valid()?std::string("Mapa ausente"):std::string("Ambiente padrão");
    rows.push_back(std::move(row));
  }
  if(!previewSecondary_ && multiProfiles_.size()>1) for(auto &row:rows) {
    const auto value=profileRowText(*profile,row);
    for(const auto &guid:multiProfiles_) if(const auto *other=findEnvironmentProfile(guid)) {
      row.mixed|=profileRowText(*other,row)!=value;
      row.editable&=profileRowAvailable(*other,row);
    }
  }

}

bool EditorSession::editProfileProperty(const EditorScreenState::ProfileRow &row,const scene::ComponentPropertyValue &value,
                                        const float *triple,const resources::AssetGuid *map,std::string &diagnostic) {
  const auto *profile=findEnvironmentProfile(state_.profileInspector);
  if(!profile) {diagnostic="Perfil indisponível.";return false;}
  std::vector<resources::AssetGuid> targets{profile->guid};
  if(multiAssetEditing_ && multiProfiles_.size()>1) targets=multiProfiles_;
  std::vector<resources::EnvironmentProfile> candidates;
  for(const auto &guid:targets) {
  profile=findEnvironmentProfile(guid);
  if(!profile || !profileRowAvailable(*profile,row)) {diagnostic="Propriedade indisponível em um perfil da seleção.";return false;}
  scene::Components components;
  auto *proxy=static_cast<scene::Environment *>(components.add(scene::Environment::descriptor));
  if(!proxy) {diagnostic="Não foi possível preparar o perfil.";return false;}
  proxy->values=profile->values;
  const auto typeId=scene::Environment::descriptor.id;
  scene::ComponentPropertyStatus status=scene::ComponentPropertyStatus::UnknownProperty;
  if(row.kind==EditorScreenState::ProfileRow::Kind::Triple && triple)
    status=scene::setComponentTriple(components,typeId,row.id,*reinterpret_cast<const float (*)[3]>(triple));
  else if(row.kind==EditorScreenState::ProfileRow::Kind::EnvironmentMap && map) {
    if(map->valid()) {
      const auto *record=assets_.find(*map);
      if(!record || record->type!=resources::AssetType::EnvironmentMap) {diagnostic="O mapa HDRI não pertence ao projeto.";return false;}
    }
    for(const auto &binding:scene::Environment::descriptor.resourceBindings)
      if(binding.kind==resources::AssetType::EnvironmentMap && binding.write) {
        auto *edit=components.editInstance(proxy->instanceId());
        status=edit && binding.write(*edit,0,*map)?scene::ComponentPropertyStatus::Applied:scene::ComponentPropertyStatus::InvalidValue;
      }
  } else status=scene::setComponentProperty(components,typeId,row.id,value);
  if(status!=scene::ComponentPropertyStatus::Applied) {diagnostic="Valor recusado pelo componente Ambiente.";return false;}
  const auto *edited=static_cast<const scene::Environment *>(components.find(scene::Environment::descriptor));
  auto candidate=*profile;candidate.values=edited->values;
  candidate.values.active=true;candidate.values.priority=0;++candidate.revision;
  candidates.push_back(std::move(candidate));
  }
  if(!commitProfileBatch(candidates,diagnostic,true)) return false;
  diagnostic="Perfil atualizado em todos os ambientes que o usam";return true;
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
