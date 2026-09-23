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
