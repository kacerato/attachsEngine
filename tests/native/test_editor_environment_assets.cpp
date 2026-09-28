#include "core/sha256.h"
#include "editor/editor_import_transaction.h"
#include "editor/editor_session.h"
#include "harness.h"
#include "scene/environment.h"

#include <chrono>
#include <filesystem>

using namespace ae;
using namespace ae::editor;

namespace {

void allocateChain(renderer::Rgba16fMipChain &chain,u32 width,u32 height,u32 levels) {
  chain.width=width;chain.height=height;chain.levels=levels;
  chain.texels.resize(static_cast<usize>(chain.expectedHalfCount()));
  for(usize texel=0;texel<chain.texels.size()/4;++texel) chain.texels[texel*4+3]=0x3c00;
}

renderer::SharedEnvironmentMap preparedEnvironment(std::span<const u8> source,
                                                    const resources::EnvironmentMapImportSettings &settings) {
  resources::EnvironmentMapImportLimits limits;
  const auto levels=[](u32 size) {u32 result=1;while(size>1) {size/=2;++result;}return result;};
  auto map=std::make_shared<renderer::EnvironmentMapResource>();
  map->sourceHash=Sha256::hex(source);
  map->cacheKey=resources::environmentMapCacheKey(map->sourceHash,settings,limits);
  allocateChain(map->panorama,settings.panoramaWidth,settings.panoramaWidth/2,levels(settings.panoramaWidth));
  allocateChain(map->specular,settings.specularSize,settings.specularSize,levels(settings.specularSize));
  allocateChain(map->brdf,settings.brdfSize,settings.brdfSize,1);
  auto &description=map->description;
  description.specularProjection=renderer::EnvironmentProjection::Octahedral;
  description.specularWidth=map->specular.width;description.specularHeight=map->specular.height;
  description.specularMipLevels=map->specular.levels;
  description.brdfWidth=map->brdf.width;description.brdfHeight=map->brdf.height;
  description.brdfMipLevels=map->brdf.levels;
  description.flags=renderer::EnvironmentMapPrefilteredGgx|
      renderer::EnvironmentMapSplitSumBrdf|renderer::EnvironmentMapDiffuseIrradianceSh9;
  return map;
}

} // namespace

AE_TEST(editor_environment_map_commit_reopen_and_component_binding_use_one_registered_resource) {
  namespace fs=std::filesystem;
  const auto root=fs::temp_directory_path()/("aether-editor-hdri-"+
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  AE_EXPECT_TRUE(fs::create_directories(root),"cria projeto isolado");
  struct Cleanup {fs::path path;~Cleanup(){std::error_code error;fs::remove_all(path,error);}} cleanup{root};

  const std::vector<u8> source{'#','?','R','A','D','I','A','N','C','E'};
  resources::EnvironmentMapImportSettings settings;
  settings.panoramaWidth=256;settings.specularSize=64;settings.brdfSize=64;
  settings.specularSamples=32;settings.brdfSamples=128;
  auto prepared=preparedEnvironment(source,settings);
  AE_EXPECT_TRUE(prepared&&prepared->valid(),"fixture HDRI válida");
  EditorSession session;std::string diagnostic;
  AE_EXPECT_TRUE(session.setProjectDirectory(root.string().c_str()),"abre projeto");
  AE_EXPECT_TRUE(session.commitEnvironmentMap("Ambientes/estudio.hdr",source,{},prepared,settings,diagnostic),
                 diagnostic.c_str());
  AE_EXPECT_EQ(session.environmentMaps().size(),1u,"publica mapa na biblioteca da sessão");
  const auto guid=session.environmentMaps().front().first;
  const auto *record=session.assets().find(guid);
  resources::EnvironmentMapImportSettings restoredSettings;
  AE_EXPECT_TRUE(record&&record->type==resources::AssetType::EnvironmentMap&&record->derived.empty()&&
                 resources::readEnvironmentMapImportSettings(record->importerParameters,restoredSettings),
                 "registro guarda fonte e receita; cache interno continua derivável");
  AE_EXPECT_TRUE(restoredSettings.panoramaWidth==settings.panoramaWidth&&
                 restoredSettings.specularSamples==settings.specularSamples,
                 "qualidade autorada volta do registro");
  const auto cacheRelative=resources::environmentMapCacheRelativePath(prepared->cacheKey);
  AE_EXPECT_TRUE(fs::exists(root/fs::path(cacheRelative)),"cache HDRI preparado foi publicado");

  EditorSession reopened;
  AE_EXPECT_TRUE(reopened.setProjectDirectory(root.string().c_str())&&
                 reopened.loadAssets(session.serializeAssets()),"registro reabre");
  std::vector<u8> cacheBytes;renderer::SharedEnvironmentMap cached;
  AE_EXPECT_TRUE(EditorImportTransaction::read(root/fs::path(cacheRelative),cacheBytes)&&
                 resources::readEnvironmentMapCache(cacheBytes,prepared->cacheKey,{},cached)&&
                 reopened.adoptEnvironmentMap(guid,cached,diagnostic),"worker reidrata cache validado");

  const auto entity=reopened.history().createEntity(reopened.document(),reopened.document().root(),
                                                     EditorEntityKind::Folder,"Ambiente");
  auto values=*reopened.document().find(entity);
  auto *environment=static_cast<scene::Environment*>(values.components.add(scene::Environment::descriptor));
  AE_EXPECT_TRUE(environment!=nullptr,"adiciona volume de ambiente");
  environment->values.sky=renderer::SkyModel::Hdri;
  AE_EXPECT_TRUE(reopened.history().applyValues(reopened.document(),entity,values),"publica componente");
  const auto *stored=reopened.document().find(entity)->components.find(scene::Environment::descriptor);
  EditorActionRequest request;request.version=reopened.sceneVersion();request.entity=entity;
  request.action=EditorAction::ComponentResource;request.componentInstance=stored->instanceId();
  request.componentProperty="environment_map";request.componentResource=guid;
  AE_EXPECT_TRUE(reopened.dispatch(request).status==EditorActionStatus::Applied,
                 "seletor vincula apenas o HDRI carregado");
  stored=reopened.document().find(entity)->components.find(scene::Environment::descriptor);
  AE_EXPECT_TRUE(static_cast<const scene::Environment*>(stored)->values.environmentMap==guid,
                 "GUID persistente chega ao componente");

  // O documento ainda conserva a referência órfã após exclusão forçada, logo
  // sua revisão não serve para detectar que a biblioteca GPU precisa mudar.
  reopened.takeAppearanceChanged();
  const auto revision=reopened.document().revision();
  EditorSession::ResourceChangeReport deletion;
  AE_EXPECT_TRUE(reopened.deleteResource("Ambientes/estudio.hdr",true,deletion),deletion.diagnostic.c_str());
  AE_EXPECT_TRUE(reopened.document().revision()==revision&&reopened.environmentMaps().empty(),
                 "exclusão remove biblioteca sem fingir edição do documento");
  AE_EXPECT_TRUE(reopened.takeAppearanceChanged(),
                 "shell recebe invalidação única para republicar a biblioteca HDRI");
}
AE_TEST(environment_batch_prepares_real_hdris_and_publishes_or_replays_all_recipes_atomically) {
  namespace fs=std::filesystem;
  const auto root=fs::temp_directory_path()/("astra-hdri-batch-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(root);
  struct Cleanup {fs::path path;~Cleanup(){std::error_code error;fs::remove_all(path,error);}} cleanup{root};
  EditorSession session;std::string diagnostic;
  AE_EXPECT_TRUE(session.setProjectDirectory(root.string().c_str()),"projeto isolado");
  const std::string header="#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 2 +X 4\n";
  std::vector<u8> source(header.begin(),header.end());
  for(u32 i=0;i<8;++i) source.insert(source.end(),{128,96,64,129});
  resources::EnvironmentMapImportSettings settings;
  settings.panoramaWidth=64;settings.specularSize=16;settings.brdfSize=16;settings.specularSamples=16;settings.brdfSamples=32;
  renderer::SharedEnvironmentMap map;
  AE_EXPECT_TRUE(resources::importRadianceEnvironmentMap(source,settings,{}, {},map,diagnostic),diagnostic.c_str());
  AE_EXPECT_TRUE(session.commitEnvironmentMap("HDRI/a.hdr",source,{},map,settings,diagnostic),diagnostic.c_str());
  AE_EXPECT_TRUE(session.commitEnvironmentMap("HDRI/b.hdr",source,{},map,settings,diagnostic),diagnostic.c_str());
  session.history().clear();session.takeAppearanceChanged();
  std::vector<EditorSession::EnvironmentBatchEdit> edits;
  auto changed=settings;changed.specularSamples=32;
  for(const auto &path:{"HDRI/a.hdr","HDRI/b.hdr"}) {
    const auto before=*session.assets().findByPath(path);auto after=before;
    after.importerParameters=resources::writeEnvironmentMapImportSettings(changed);edits.push_back({before,after});
  }
  const auto registry=session.serializeAssets();
  std::vector<EditorSession::PreparedEnvironmentEdit> prepared;
  AE_EXPECT_TRUE(!EditorSession::prepareEnvironmentBatch(root.string(),edits,{[](void*){return true;},nullptr},prepared,diagnostic),"cancelar antes de publicar");
  AE_EXPECT_EQ(session.serializeAssets(),registry,"cancelamento não altera registro");
  AE_EXPECT_TRUE(EditorSession::prepareEnvironmentBatch(root.string(),edits,{},prepared,diagnostic),diagnostic.c_str());
  AE_EXPECT_EQ(prepared.size(),2u,"dois derivados reais prontos");
  AE_EXPECT_TRUE(EditorImportTransaction::writeText(root/"HDRI/b.hdr","alterado externamente"),"conflito no segundo alvo");
  AE_EXPECT_TRUE(!session.commitEnvironmentBatch(prepared,diagnostic),"nenhum alvo publicado se uma fonte mudou");
  AE_EXPECT_EQ(session.serializeAssets(),registry,"primeiro alvo não publicado isoladamente");
  AE_EXPECT_EQ(session.history().undoDepth(),0u,"falha não cria histórico");
  AE_EXPECT_TRUE(EditorImportTransaction::write(root/"HDRI/b.hdr",source),"restaura fixture");
  AE_EXPECT_TRUE(session.commitEnvironmentBatch(prepared,diagnostic),diagnostic.c_str());
  AE_EXPECT_TRUE(session.takeAppearanceChanged(),"renderer precisa republicar biblioteca de mapas");
  AE_EXPECT_EQ(session.history().undoDepth(),1u,"uma operação para o lote");
  for(const auto &edit:edits) AE_EXPECT_EQ(session.assets().find(edit.after.guid)->importerParameters,edit.after.importerParameters,"receita publicada");
  for(const auto &entry:session.environmentMaps()) AE_EXPECT_TRUE(entry.second->cacheKey!=map->cacheKey,"biblioteca usa derivados novos");
  const auto oldCache=root/resources::environmentMapCacheRelativePath(map->cacheKey);
  std::vector<u8> cacheBytes;AE_EXPECT_TRUE(EditorImportTransaction::read(oldCache,cacheBytes),"cache anterior preservado");
  AE_EXPECT_TRUE(fs::remove(oldCache),"simula limpeza do cache do histórico");
  const auto published=session.serializeAssets();
  AE_EXPECT_TRUE(!session.history().undo(session.document()),"cache ausente recusa desfazer inteiro");
  AE_EXPECT_EQ(session.serializeAssets(),published,"recusa não altera registro");
  AE_EXPECT_TRUE(EditorImportTransaction::write(oldCache,cacheBytes),"restaura cache");
  AE_EXPECT_TRUE(session.history().undo(session.document()),"desfazer pelos caches");
  AE_EXPECT_EQ(session.serializeAssets(),registry,"receitas anteriores exatas");
  AE_EXPECT_TRUE(session.history().redo(session.document()),"refazer pelos caches");
  AE_EXPECT_EQ(session.serializeAssets(),published,"receitas novas exatas");
  std::vector<u8> disk;AE_EXPECT_TRUE(EditorImportTransaction::read(root/".astra/assets.astra",disk),"registro persistido");
  EditorSession reopened;AE_EXPECT_TRUE(reopened.setProjectDirectory(root.string().c_str()),"reabre projeto");
  AE_EXPECT_TRUE(reopened.loadAssets(std::string(disk.begin(),disk.end())),"reabre registro em disco");
  AE_EXPECT_EQ(reopened.serializeAssets(),published,"reabrir conserva receitas");
}
