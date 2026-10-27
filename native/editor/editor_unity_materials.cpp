#include "editor/editor_session.h"
#include "editor/editor_import_transaction.h"
#include "resources/unity_material_import.h"
#include "core/sha256.h"
#include <filesystem>
#include <map>

namespace ae::editor {
namespace {
std::string text(const std::vector<u8> &bytes) {return {bytes.begin(),bytes.end()};}
std::string hashText(const std::string &value) {return Sha256::hex({reinterpret_cast<const u8*>(value.data()),value.size()});}
std::string hashMaterial(resources::MaterialAsset value) {value.revision=1;return hashText(value.serialize());}
bool sameRecipe(const resources::AssetRecord &a,const resources::AssetRecord &b) {
  return a.guid==b.guid&&a.type==b.type&&a.path==b.path&&a.source==b.source&&
    a.importerVersion==b.importerVersion&&a.importerParameters==b.importerParameters&&
    a.derived==b.derived&&a.dependencies==b.dependencies;
}
bool persistedRecordMatches(const std::filesystem::path &root,const resources::AssetRecord &expected) {
  std::vector<u8> bytes;resources::AssetRegistry registry;
  if(!EditorImportTransaction::read(root/".astra/assets.astra",bytes,32u*1024u*1024u)||
     !resources::AssetRegistry::deserialize(text(bytes),registry)) return false;
  const auto *record=registry.find(expected.guid);
  return record&&sameRecipe(*record,expected)&&record->contentHash==expected.contentHash;
}
}

resources::AssetGuid EditorSession::importUnityMaterial(const std::string &path,std::string &diagnostic) {
  diagnostic.clear();
  if(isPlaying()||history_.isOpen()||files_.rootPath().empty()||!path.ends_with(".mat")) {
    diagnostic="Abra um projeto em Edit e selecione um material .mat Unity.";return {};
  }
  const auto root=EditorImportTransaction::fromUtf8(files_.rootPath());
  std::filesystem::path source;std::vector<u8> bytes;
  if(!EditorImportTransaction::safePath(root,path,source)||!EditorImportTransaction::read(source,bytes,256u*1024u)) {
    diagnostic="Material de origem ausente, inacessível ou muito grande: "+path;return {};
  }
  resources::UnityMaterialImport converted;
  if(!resources::readUnityMaterial(text(bytes),converted)) {diagnostic=converted.diagnostic;return {};}
  if(!converted.supported()) {
    diagnostic="Conversão recusada; fonte preservada.";
    for(const auto &issue:converted.unsupported) diagnostic+="\n"+issue;
    return {};
  }
  const auto sourceHash=Sha256::hex(bytes);
  const auto *existing=static_cast<const resources::AssetRecord*>(nullptr);
  for(const auto &record:assets_.records()) if(record.type==resources::AssetType::Material && record.source==path) {
    if(existing) {diagnostic="Mais de um material convertido referencia a mesma origem.";return {};}
    existing=&record;
  }
  const auto nativePath=existing?existing->path:path+".material";
  auto &material=converted.material;
  material.guid=existing?existing->guid:resources::assetGuidFromSeed("unity-material:"+path);
  const auto *current=existing?findMaterialAsset(existing->guid):nullptr;
  if(existing) {
    if(!current || existing->importerParameters.find("native="+hashMaterial(*current))==std::string::npos) {
      diagnostic="Conflito de reimportação: material possui alterações locais ou não tem receita reconhecida. Preserve-o e converta uma cópia da fonte.";return {};
    }
    if(!persistedRecordMatches(root,*existing)) {
      diagnostic="Metadados do material mudaram no disco; reabra o registro antes de reimportar.";return {};
    }
    material.revision=current->revision+1;
  } else if(files_.exists(nativePath)||assets_.findByPath(nativePath)||assets_.find(material.guid)) {
    diagnostic="Destino da conversão já ocupado: "+nativePath;return {};
  }
  // GUID/fileID are resolved from adjacent Unity .meta files, never by basename.
  std::map<resources::AssetGuid,std::string> unityTextures;
  // A preserved package revision is its own source namespace. Two revisions
  // may legally contain the same Unity GUID and must never bind each other's
  // texture payload or compete for a single native texture identity.
  std::string packageScope;
  constexpr std::string_view packagePrefix="Biblioteca/";
  if(path.starts_with(packagePrefix)) {
    const auto slash=path.find('/',packagePrefix.size());resources::AssetGuid package;
    if(slash!=path.npos&&resources::AssetGuid::parse(std::string_view(path).substr(packagePrefix.size(),slash-packagePrefix.size()),package)&&library_.package(package))
      packageScope=path.substr(0,slash+1);
  }
  const auto resolutionRoot=packageScope.empty()?root:root/EditorImportTransaction::fromUtf8(packageScope);
  if(!converted.textures.empty()) {
    std::error_code error;usize count=0;
    for(std::filesystem::recursive_directory_iterator it(resolutionRoot,std::filesystem::directory_options::skip_permission_denied,error),end;
        !error&&it!=end;it.increment(error)) {
      if(++count>resources::AssetRegistry::MaximumRecords) {diagnostic="Pacote excede o limite de arquivos para resolver GUIDs.";return {};}
      const auto status=it->symlink_status(error);if(error) break;
      if(std::filesystem::is_symlink(status)) {it.disable_recursion_pending();continue;}
      if(it->is_directory(error)&&it->path().filename()==".astra") {it.disable_recursion_pending();continue;}
      if(!it->is_regular_file(error)||it->path().extension()!=".meta") continue;
      std::vector<u8> meta;if(!EditorImportTransaction::read(it->path(),meta,64u*1024u)) continue;
      const auto body=text(meta);auto at=body.find("\nguid: ");
      if(at==std::string::npos && body.starts_with("guid: ")) at=0;else if(at!=std::string::npos) ++at;
      if(at==std::string::npos) continue;
      resources::AssetGuid guid;if(!resources::AssetGuid::parse(std::string_view(body).substr(at+6,32),guid)) continue;
      bool needed=false;for(const auto &texture:converted.textures) needed|=texture.unityGuid==guid;
      if(!needed) continue;
      auto relative=std::filesystem::relative(it->path(),root,error).generic_u8string();if(error) break;
      std::string name(relative.begin(),relative.end());name.resize(name.size()-5);
      if(!unityTextures.emplace(guid,name).second) {diagnostic="GUID Unity duplicado: "+guid.text();return {};}
    }
    if(error) {diagnostic="Não foi possível resolver todas as dependências Unity: "+error.message();return {};}
  }
  auto nextAssets=assets_;std::vector<resources::AssetGuid> dependencies;
  std::vector<std::pair<std::string,std::string>> textureSources;
  resources::TextureImportLimits limits;limits.image=importLimits_.image;limits.projectMaximumDimension=importLimits_.maximumTextureDimension;
  for(const auto &texture:converted.textures) {
    const auto found=unityTextures.find(texture.unityGuid);
    if(found==unityTextures.end()) {diagnostic="Dependência Unity ausente: "+texture.property+" · GUID "+texture.unityGuid.text();return {};}
    const auto &texturePath=found->second;
    std::filesystem::path absolute;std::vector<u8> encoded;
    if(!EditorImportTransaction::safePath(root,texturePath,absolute)||!EditorImportTransaction::read(absolute,encoded,limits.image.maximumEncodedBytes)) {
      diagnostic="Textura ausente ou inacessível: "+texturePath;return {};
    }
    resources::TextureProfile profile;
    // PK-9: as extras sobem como o binding que seguem (detalhe de cor é cor,
    // normal de detalhe é normal, altura/máscara/verniz são dado).
    const u32 publication=texture.binding>=scene::MaterialExtraTextureBindingBase?
      scene::materialExtraPublicationBinding(texture.binding-scene::MaterialExtraTextureBindingBase):texture.binding;
    profile.interpretation=publication==1?resources::TextureInterpretationNormal:
      (publication==0||publication==3?resources::TextureInterpretationColor:resources::TextureInterpretationData);
    // Fonte do alfa do importador da Unity (`alphaUsage` no .meta da textura):
    // 0 nenhum, 1 do arquivo, 2 tons de cinza (trilhas e brilhos em RGB puro).
    {
      std::filesystem::path metaPath;std::vector<u8> metaBytes;
      if(EditorImportTransaction::safePath(root,texturePath+".meta",metaPath)&&EditorImportTransaction::read(metaPath,metaBytes,64u*1024u)) {
        const auto meta=text(metaBytes);const auto at=meta.find("alphaUsage: ");
        if(at!=std::string::npos&&at+12<meta.size()&&meta[at+12]>='0'&&meta[at+12]<='2')
          profile.alphaSource=static_cast<u8>(meta[at+12]-'0');
      }
    }
    const auto textureHash=Sha256::hex(encoded);
    textureSources.push_back({texturePath,textureHash});
    const auto *known=nextAssets.findByPath(texturePath);
    resources::AssetGuid guid=known?known->guid:resources::assetGuidFromSeed("unity-texture:"+packageScope+texture.unityGuid.text());
    // Prepara a imagem com o perfil e grava o cache da textura.
    const auto cook=[&](const resources::TextureProfile &settings) {
      resources::PreparedTextureImport prepared;
      if(!resources::prepareTextureImport(encoded,settings,true,EditorMapScene::DefaultTextureSampler,limits,nullptr,prepared)) {
        diagnostic=texturePath+": "+prepared.diagnostic;return false;
      }
      std::vector<u8> cache;std::filesystem::path cached;
      if(!resources::writeTextureAssetCache(prepared,cache)||!EditorImportTransaction::safePath(root,resources::textureAssetCachePath(guid),cached)) {
        diagnostic="Cache da textura inválido.";return false;
      }
      std::error_code error;std::filesystem::create_directories(cached.parent_path(),error);
      return !error&&EditorImportTransaction::write(cached,cache,&diagnostic);
    };
    if(known) {
      resources::TextureProfile saved;
      if(known->type!=resources::AssetType::Texture||known->contentHash!=textureHash||
          !resources::parseTextureProfile(known->importerParameters,saved)||saved.interpretation!=profile.interpretation) {
        diagnostic="Conflito na textura já registrada: "+texturePath+". Reimporte-a com interpretação compatível antes do material.";return {};
      }
      // Registrada antes da fonte de alfa (perfil < 6): só o alfa muda, o resto
      // do perfil salvo fica; a imagem é repreparada e o registro atualizado.
      if(saved.alphaSource!=profile.alphaSource) {
        saved.alphaSource=profile.alphaSource;
        if(!cook(saved)) return {};
        if(!nextAssets.publishImport(guid,textureHash,resources::TextureAssetImporterRevision,resources::serializeTextureProfile(saved),
                                     known->derived,known->dependencies)) {
          diagnostic="Registro recusou a fonte de alfa da textura: "+texturePath;return {};
        }
      }
    } else {
      if(!cook(profile)) return {};
      resources::AssetRecord record;record.guid=guid;record.type=resources::AssetType::Texture;
      record.path=record.source=texturePath;record.contentHash=textureHash;
      record.importerVersion=resources::TextureAssetImporterRevision;record.importerParameters=resources::serializeTextureProfile(profile);
      if(!nextAssets.add(record)) {diagnostic="Registro recusou a textura: "+texturePath;return {};}
    }
    if(std::find(dependencies.begin(),dependencies.end(),guid)==dependencies.end()) dependencies.push_back(guid);
    if(texture.binding<scene::MaterialTextureCount) {material.textures[texture.binding]=guid;material.sampling[texture.binding]=texture.sampling;}
    else if(texture.binding==scene::MaterialOcclusionTextureBinding) {material.occlusionTexture=guid;material.sampling[2]=texture.sampling;}
    else material.extraTextures[texture.binding-scene::MaterialExtraTextureBindingBase]=guid;
  }
  if(!material.valid()) {diagnostic="Material convertido inválido.";return {};}
  if(!EditorImportTransaction::read(source,bytes,256u*1024u)||Sha256::hex(bytes)!=sourceHash) {
    diagnostic="O material de origem mudou durante a conversão; nada publicado.";return {};
  }
  for(const auto &[texturePath,expected]:textureSources) {
    std::filesystem::path absolute;
    if(!EditorImportTransaction::safePath(root,texturePath,absolute)||
        !EditorImportTransaction::read(absolute,bytes,limits.image.maximumEncodedBytes)||Sha256::hex(bytes)!=expected) {
      diagnostic="Textura mudou durante a conversão: "+texturePath+"; nada publicado.";return {};
    }
  }
  const auto native=material.serialize();
  const auto recipe="unity-standard-metallic/1 source="+sourceHash+" native="+hashMaterial(material);
  if(existing) {
    // Values, source recipe and dependency edges form one resource command.
    // Conversion must not fan out through the Inspector's multi-edit selection.
    const auto before=*current;const auto beforeRecord=*existing;
    const auto derived=existing->derived;
    if(!nextAssets.publishImport(material.guid,hashText(native),1,recipe,derived,dependencies)) {
      diagnostic="Registro recusou a receita de reimportação.";return {};
    }
    const auto afterRecord=*nextAssets.find(material.guid);
    const auto project=files_.rootPath();
    std::string libraryText;if(!prepareLibraryIndex(nextAssets,libraryText,diagnostic))return {};
    if(!persistedRecordMatches(root,beforeRecord)||!EditorImportTransaction::publishTextBatch(project,
        {{nativePath,hashText(before.serialize()),native}},nextAssets.serialize(),diagnostic,LibraryIndexPath,libraryText)) {
      if(diagnostic.empty()) diagnostic="Metadados mudaram durante a reimportação.";
      return {};
    }
    assets_=std::move(nextAssets);loadMaterialAssets();loadLibraryIndex(diagnostic);
    const auto after=material;
    history_.recordResource("Reimportar material Unity",[this,project,before,after,beforeRecord,afterRecord](bool forward) {
      if(files_.rootPath()!=project||isPlaying()||history_.isOpen()) {state_.status="Projeto do material indisponível.";return false;}
      const auto *current=findMaterialAsset(before.guid);const auto *record=assets_.find(before.guid);
      const auto &expectedValue=forward?before:after;const auto &expectedRecord=forward?beforeRecord:afterRecord;
      if(!current||!record||hashMaterial(*current)!=hashMaterial(expectedValue)||!sameRecipe(*record,expectedRecord)||
         !persistedRecordMatches(EditorImportTransaction::fromUtf8(project),*record)||current->revision==std::numeric_limits<u32>::max()) {
        state_.status="Material ou metadados mudaram; histórico preservado sem sobrescrever.";return false;
      }
      auto value=forward?after:before;value.revision=current->revision+1;
      const auto &target=forward?afterRecord:beforeRecord;auto registry=assets_;const auto native=value.serialize();
      if(!registry.publishImport(value.guid,hashText(native),target.importerVersion,target.importerParameters,target.derived,target.dependencies)) {
        state_.status="Dependência ou receita do histórico indisponível.";return false;
      }
      std::string error;
      std::string libraryText;if(!prepareLibraryIndex(registry,libraryText,error)){state_.status=error;return false;}
      if(!EditorImportTransaction::publishTextBatch(project,{{target.path,hashText(current->serialize()),native}},registry.serialize(),error,LibraryIndexPath,libraryText)) {
        state_.status=error;return false;
      }
      assets_=std::move(registry);loadMaterialAssets();loadTextureAssets();publishMaterialLibrary();loadLibraryIndex(error);assetRegistryDirty_=true;
      state_.status=forward?"Reimportação refeita com receita e dependências":"Reimportação desfeita com receita e dependências";
      return true;
    });
    assetRegistryDirty_=true;
  } else {
    resources::AssetRecord record;record.guid=material.guid;record.type=resources::AssetType::Material;
    record.path=nativePath;record.source=path;record.contentHash=hashText(native);record.importerVersion=1;
    record.importerParameters=recipe;record.dependencies=dependencies;
    if(!nextAssets.add(record)) {diagnostic="Registro recusou o material convertido.";return {};}
    std::string libraryText;if(!prepareLibraryIndex(nextAssets,libraryText,diagnostic))return {};
    EditorImportTransaction transaction(files_.rootPath());
    if(!transaction.begin(nativePath,{},diagnostic,LibraryIndexPath)) return {};
    if(!transaction.commit({reinterpret_cast<const u8*>(native.data()),native.size()},nextAssets.serialize(),libraryText,&diagnostic)) {
      transaction.rollback();return {};
    }
    assets_=std::move(nextAssets);materials_.push_back(material);loadLibraryIndex(diagnostic);
    const auto project=files_.rootPath();
    history_.recordResource("Converter material Unity",[this,project,record,native](bool forward) {
      if(files_.rootPath()!=project||isPlaying()) {state_.status="Projeto do material indisponível.";return false;}
      auto registry=assets_;std::string error;
      if(!forward) {
        const auto *value=findMaterialAsset(record.guid);
        if(!value||value->serialize()!=native) {state_.status="Material foi alterado; conversão não será removida.";return false;}
        std::vector<EditorEntityId> ids;document_.collectSubtree(document_.root(),ids);
        for(const auto id:ids) if(const auto *entity=document_.find(id)) if(const auto *render=meshRenderer(*entity))
          for(u32 slot=0;slot<render->slotCount();++slot) if(render->slotMaterialAsset(slot)==record.guid) {
            state_.status="Material ainda é usado na cena; desfaça seus vínculos primeiro.";return false;
          }
        if(!registry.remove(record.guid)) {state_.status="Outro recurso depende deste material.";return false;}
      } else if(!registry.add(record)) {state_.status="Destino ou dependência do material mudou.";return false;}
      std::string libraryText;if(!prepareLibraryIndex(registry,libraryText,error)){state_.status=error;return false;}
      EditorImportTransaction transaction(project);
      if(!transaction.begin(record.path,forward?std::string():hashText(native),error,LibraryIndexPath)) {state_.status=error;return false;}
      const bool ok=forward?transaction.commit({reinterpret_cast<const u8*>(native.data()),native.size()},registry.serialize(),libraryText,&error):
          transaction.commitRemoval(registry.serialize(),libraryText);
      if(!ok) {transaction.rollback();state_.status=error.empty()?"Falha ao restaurar material.":error;return false;}
      assets_=std::move(registry);loadMaterialAssets();files_.rebuildTree();loadLibraryIndex(error);assetRegistryDirty_=false;
      if(!forward&&state_.materialInspector==record.guid) state_.materialInspector={};
      state_.status=forward?"Conversão refeita":"Conversão desfeita; fontes e texturas preservadas";return true;
    });
  }
  loadTextureAssets();publishMaterialLibrary();files_.rebuildTree();assetRegistryDirty_=true;
  state_.selectedFile=nativePath;openMaterialInspector(material.guid);
  std::string publish;if(!ensureTexturesPublished(publish)) reportProblem(EditorConsoleSeverity::Warning,"Material convertido; publicação gráfica pendente: "+publish);
  diagnostic="Material convertido: "+nativePath+" · "+std::to_string(dependencies.size())+" texturas · fonte Unity preservada";
  for(const auto &issue:converted.approximations) diagnostic+="\n"+std::string(resources::UnityMaterialApproximationPrefix)+issue;
  return material.guid;
}
} // namespace ae::editor
