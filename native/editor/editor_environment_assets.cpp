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
  if(same(state_.environmentDraft,state_.environmentSaved)) state_.environmentDraft=saved;
  state_.environmentSaved=saved;
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
}

bool EditorSession::editProfileProperty(const EditorScreenState::ProfileRow &row,const scene::ComponentPropertyValue &value,
                                        const float *triple,const resources::AssetGuid *map,std::string &diagnostic) {
  const auto *profile=findEnvironmentProfile(state_.profileInspector);
  if(!profile) {diagnostic="Perfil indisponível.";return false;}
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
  if(!commitEnvironmentProfile(candidate,diagnostic)) return false;
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
