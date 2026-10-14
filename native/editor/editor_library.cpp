#include "editor/editor_session.h"
#include "editor/editor_umotion_import.h"
#include "scene/skinned_mesh.h"
#include <fstream>
#include "resources/unity_material_import.h"
#include "resources/unity_import_recipe.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <map>
#include <set>

namespace ae::editor {
namespace {
using namespace resources;
std::string lower(std::string value) {for(auto &c:value)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return value;}
bool fileHash(const std::filesystem::path &path,std::string &hash,u64 &bytes,const std::function<bool()> &cancelled={}) {
  std::ifstream input(path,std::ios::binary);if(!input)return false;
  Sha256 digest;std::array<u8,65536> chunk{};bytes=0;
  while(input) {if(cancelled&&cancelled())return false;input.read(reinterpret_cast<char*>(chunk.data()),chunk.size());
    const auto count=input.gcount();if(count>0){bytes+=static_cast<u64>(count);digest.update({chunk.data(),static_cast<usize>(count)});}}
  if(!input.eof())return false;
  hash=Sha256::hex(digest.digest());return true;
}
// Projeto UMotion Pro: .asset com a lista de objetos animados e de clipes.
bool umotionProject(const std::filesystem::path &path) {
  std::ifstream input(path,std::ios::binary);if(!input)return false;
  std::string head(262144,'\0');input.read(head.data(),static_cast<std::streamsize>(head.size()));head.resize(static_cast<usize>(input.gcount()));
  return head.find("\n  AnimatedObjectList:")!=head.npos&&head.find("\n  EditorState:")!=head.npos;
}
LibraryItemKind kind(const std::string &path) {
  const auto ext=lower(std::filesystem::path(path).extension().string());
  if(ext==".glb"||ext==".gltf"||ext==".fbx"||ext==".obj"||ext==".dae")return LibraryItemKind::Model;
  if(ext==".anim"||ext==".aeclip")return LibraryItemKind::Clip;
  if(ext==".controller"||ext==".aeanimator")return LibraryItemKind::Controller;
  if(ext==".mat"||ext==".material")return LibraryItemKind::Material;
  if(ext==".png"||ext==".jpg"||ext==".jpeg"||ext==".ktx2"||ext==".tga"||ext==".tif"||ext==".tiff"||ext==".hdr")return LibraryItemKind::Texture;
  if(ext==".prefab")return LibraryItemKind::Prefab;
  if(ext==".unity"||ext==".aescene")return LibraryItemKind::Scene;
  if(ext==".cs"||ext==".shader"||ext==".shadergraph")return LibraryItemKind::Code;
  if(ext==".txt"||ext==".md"||ext==".html"||ext==".pdf")return LibraryItemKind::Documentation;
  return LibraryItemKind::Other;
}
const char *kindName(LibraryItemKind value) {
  constexpr const char *names[]{"Modelos","Clipes","Materiais","Texturas","Prefabs","Cenas","Código","Documentos","Outros","Controllers"};
  return names[static_cast<u32>(value)];
}
bool compatible(LibraryItemKind kind,AssetType type) {
  return (kind==LibraryItemKind::Material&&type==AssetType::Material)||(kind==LibraryItemKind::Texture&&type==AssetType::Texture)||
    (kind==LibraryItemKind::Model&&type==AssetType::Mesh)||(kind==LibraryItemKind::Clip&&type==AssetType::AnimationClip)||
    (kind==LibraryItemKind::Prefab&&type==AssetType::Prefab)||(kind==LibraryItemKind::Scene&&type==AssetType::Scene)||
    (kind==LibraryItemKind::Controller&&type==AssetType::AnimatorController);
}
}
PreparedLibraryPackage prepareLibraryPackage(const std::string &project,const std::string &staging,
    std::string name,std::vector<LibrarySourceFile> files,const std::function<bool()> &cancelled) {
  PreparedLibraryPackage result;result.project=project;result.staging=staging;
  const auto fail=[&](const std::string &message){result.diagnostic=message;return result;};
  if(files.empty()||files.size()>EditorImportTransaction::MaximumCompanions||!staging.starts_with(".astra/import-staging/"))
    return fail("Escolha um pacote com 1 a 4096 arquivos numa pasta de preparo do projeto.");
  if(name.empty()||name.size()>128||name.find_first_of("\r\n\t")!=name.npos)return fail("Nome de pacote inválido.");
  std::sort(files.begin(),files.end(),[](const auto &a,const auto &b){return a.relative<b.relative;});
  const auto root=EditorImportTransaction::fromUtf8(project);std::ostringstream manifest;manifest<<"ASTRA_SOURCE_PACKAGE_1\n";
  std::map<std::string,std::string> identities;std::map<std::string,std::string> metadata;std::set<std::string> foldedPaths;
  for(usize i=0;i<files.size();++i) {
    auto &file=files[i];std::filesystem::path absolute;
    if(file.relative.empty()||file.relative.starts_with(".astra/")||file.relative.find_first_of("\\:\r\n\t")!=file.relative.npos||
       lower(file.relative)=="package.astrapackage"||!foldedPaths.insert(lower(file.relative)).second||!EditorImportTransaction::safePath(root,staging+"/"+file.relative,absolute))
      return fail("Caminho duplicado ou fora do pacote: "+file.relative);
    std::error_code stampError;const auto before=std::filesystem::last_write_time(absolute,stampError);
    if(stampError)return fail("Fonte copiada inacessível.");
    std::string actual;u64 size=0;
    if(!fileHash(absolute,actual,size,cancelled))return fail(cancelled&&cancelled()?"Catalogação cancelada.":"Fonte inacessível: "+file.relative);
    if(actual!=file.sha256||size!=file.bytes)return fail("Fonte mudou após a cópia: "+file.relative);
    std::error_code error;file.modified=std::filesystem::last_write_time(absolute,error);if(error||file.modified!=before)return fail("Fonte mudou durante a catalogação.");
    manifest<<std::quoted(file.relative)<<' '<<size<<' '<<actual<<'\n';
    if(file.relative.ends_with(".meta")) {
      std::vector<u8> bytes;if(!EditorImportTransaction::read(absolute,bytes,65536))return fail("Metadados Unity ilegíveis: "+file.relative);
      const std::string body(bytes.begin(),bytes.end());auto at=body.find("\nguid: ");
      if(at!=body.npos)++at;else if(body.starts_with("guid: "))at=0;
      AssetGuid guid;if(at!=body.npos&&AssetGuid::parse(std::string_view(body).substr(at+6,32),guid)) {
        const auto source=file.relative.substr(0,file.relative.size()-5);
        if(!identities.emplace(guid.text(),source).second)return fail("GUID Unity duplicado: "+guid.text());
        metadata.emplace(source,guid.text());
      }
    }
  }
  result.manifest=manifest.str();if(result.manifest.size()>LibraryIndex::MaximumBytes)return fail("Manifesto do pacote excede 16 MiB.");
  auto &package=result.package;package.sourceHash=Sha256::hex({reinterpret_cast<const u8*>(result.manifest.data()),result.manifest.size()});
  package.id=assetGuidFromSeed("library-package:"+name+":"+package.sourceHash);package.name=std::move(name);package.version="fonte-1";
  package.origin="pasta selecionada pelo autor";package.licenseNotice="Fontes e avisos originais preservados; uso e redistribuição dependem da licença do pacote.";
  const std::string destination="Biblioteca/"+package.id.text()+"/";result.manifestPath=destination+"package.astrapackage";
  std::map<std::string,AssetGuid> sourceIds;
  for(const auto &file:files) {
    result.companions.push_back({staging+"/"+file.relative,destination+file.relative});
    if(file.relative.ends_with(".meta"))continue;
    LibraryItem item;item.path=destination+file.relative;item.sourceHash=file.sha256;item.kind=kind(file.relative);
    // O preparo e relativo ao projeto: sem a raiz, o arquivo so era achado
    // quando o diretorio atual do processo era o projeto (no PC, nao no Android).
    std::filesystem::path sniffed;
    const bool umotion=lower(file.relative).ends_with(".asset")&&EditorImportTransaction::safePath(root,staging+"/"+file.relative,sniffed)&&umotionProject(sniffed);
    if(umotion)item.kind=LibraryItemKind::Clip;
    const auto meta=metadata.find(file.relative);item.sourceIdentity=meta==metadata.end()?"path:"+file.relative:"unity:"+meta->second;
    item.id=assetGuidFromSeed(package.id.text()+":"+item.sourceIdentity);sourceIds.emplace(file.relative,item.id);
    item.diagnostic=umotion?"Projeto UMotion Pro: selecione o personagem e abra para importar clipes, alvos de IK e Filho de.":"Fonte preservada; conversão ainda não executada.";
    if(item.kind==LibraryItemKind::Code||item.kind==LibraryItemKind::Scene||
       (item.kind==LibraryItemKind::Model&&!lower(file.relative).ends_with(".glb")&&!lower(file.relative).ends_with(".gltf")&&!lower(file.relative).ends_with(".fbx"))) {
      item.conversion=LibraryConversionState::Unsupported;item.diagnostic="Fonte preservada. Este formato de origem ainda não possui conversor neste fluxo.";
    }
    package.items.push_back(std::move(item));
  }
  for(auto &item:package.items) {
    const auto relative=item.path.substr(destination.size());const auto ext=lower(std::filesystem::path(relative).extension().string());
    if(ext!=".mat"&&ext!=".prefab"&&ext!=".unity"&&ext!=".anim"&&ext!=".asset"&&ext!=".controller")continue;
    std::filesystem::path absolute;std::vector<u8> bytes;
    if(!EditorImportTransaction::safePath(root,staging+"/"+relative,absolute)||!EditorImportTransaction::read(absolute,bytes,256u*1024u)) {
      item.knownLosses.push_back("Dependências do texto não inspecionadas: fonte maior que 256 KiB ou ilegível.");continue;
    }
    const std::string body(bytes.begin(),bytes.end());std::set<AssetGuid> dependencies;
    for(usize at=0;(at=body.find("guid: ",at))!=body.npos;at+=6) {
      AssetGuid guid;if(!AssetGuid::parse(std::string_view(body).substr(at+6,32),guid))continue;
      if(guid.text()=="0000000000000000f000000000000000"||guid.text()=="0000000000000000e000000000000000")continue;
      const auto identity=identities.find(guid.text());AssetGuid dependency;
      if(identity!=identities.end()) {const auto source=sourceIds.find(identity->second);if(source!=sourceIds.end())dependency=source->second;}
      if(!dependency.valid())dependency=assetGuidFromSeed(package.id.text()+":missing-unity:"+guid.text());
      if(dependency!=item.id)dependencies.insert(dependency);
      if(dependencies.size()>LibraryIndex::MaximumDependencies)return fail("Item excede 128 dependências: "+relative);
    }
    item.dependencies.assign(dependencies.begin(),dependencies.end());
    if(ext==".mat") {UnityMaterialImport converted;if(!readUnityMaterial(body,converted)||!converted.supported()) {
      item.conversion=LibraryConversionState::Unsupported;item.diagnostic=converted.diagnostic.empty()?"Shader ou propriedades não suportadas pelo conversor Standard.":converted.diagnostic;
      for(const auto &issue:converted.unsupported)if(item.knownLosses.size()<LibraryIndex::MaximumLosses)item.knownLosses.push_back(issue);
    }}
  }
  LibraryIndex check;if(!check.add(package))return fail("Metadados do pacote violam o contrato da biblioteca.");
  result.files=std::move(files);result.accepted=true;result.diagnostic="Pacote preservado e catalogado; conversões são ações explícitas por item.";return result;
}
bool EditorSession::loadLibraryIndex(std::string &diagnostic) {
  diagnostic.clear();
  resources::LibraryIndex candidate;std::filesystem::path path;std::vector<u8> bytes;std::error_code error;
  if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(files_.rootPath()),LibraryIndexPath,path))return false;
  if(std::filesystem::exists(path,error)) {
    if(error||!EditorImportTransaction::read(path,bytes,resources::LibraryIndex::MaximumBytes)||
       !resources::LibraryIndex::deserialize({reinterpret_cast<const char*>(bytes.data()),bytes.size()},candidate,diagnostic))return false;
  } else if(error)return false;
  library_=std::move(candidate);refreshLibraryRows();return true;
}
bool EditorSession::prepareLibraryIndex(const resources::AssetRegistry &registry,std::string &encoded,std::string &diagnostic) const {
  const auto root=EditorImportTransaction::fromUtf8(files_.rootPath());std::filesystem::path path;std::vector<u8> bytes;std::error_code error;
  if(!EditorImportTransaction::safePath(root,LibraryIndexPath,path))return false;
  if(std::filesystem::exists(path,error)) {
    if(error||!EditorImportTransaction::read(path,bytes,resources::LibraryIndex::MaximumBytes)||
       std::string(bytes.begin(),bytes.end())!=library_.serialize()) {diagnostic="Índice da biblioteca mudou no disco; reabra o projeto.";return false;}
  } else if(error||!library_.packages().empty()){diagnostic="Índice da biblioteca ausente.";return false;}
  auto candidate=library_;
  for(const auto &current:library_.packages()) {
    auto package=current;
    for(auto &item:package.items) {
      const resources::AssetRecord *output=nullptr;
      for(const auto &record:registry.records())if((record.source==item.path||record.path==item.path)&&compatible(item.kind,record.type)) {
        if(output){diagnostic="Mais de uma saída nativa para o item: "+item.path;return false;}output=&record;
      }
      if(output&&output->importerVersion&&!output->importerParameters.empty()) {
        item.nativeGuid=output->guid;item.conversion=resources::LibraryConversionState::Converted;item.converter="astra-import";
        item.converterVersion=output->importerVersion;item.recipe=output->importerParameters;item.diagnostic="Saída nativa registrada; prévia gráfica depende da publicação dos recursos.";
        if(output->importerParameters.starts_with("ASTRA_UNITY_IMPORT")) {
          UnityImportRecipe recipe;if(!UnityImportRecipe::deserialize(output->importerParameters,recipe,diagnostic))return false;
          item.converter="astra-unity";item.knownLosses=recipe.transformations;
          item.diagnostic="Recurso nativo convertido por GUID/fileID; fonte e baseline preservados.";
        }
      } else if(item.conversion==resources::LibraryConversionState::Converted) {
        item.nativeGuid={};item.conversion=resources::LibraryConversionState::Pending;item.converter.clear();item.converterVersion=0;item.recipe.clear();
        item.diagnostic="Conversão desfeita ou saída removida; fonte preservada.";
      }
    }
    if(!candidate.replace(std::move(package))){diagnostic="Receita ou metadados da saída nativa incompatíveis com o índice.";return false;}
  }
  encoded=candidate.serialize();return !encoded.empty();
}
bool EditorSession::importLibraryPackage(const PreparedLibraryPackage &prepared,std::string &diagnostic) {
  std::error_code identityError;
  const bool sameProject=std::filesystem::equivalent(EditorImportTransaction::fromUtf8(prepared.project),
      EditorImportTransaction::fromUtf8(files_.rootPath()),identityError);
  if(!prepared.accepted||!sameProject||identityError||isPlaying()||history_.isOpen()) {
    diagnostic="Preparo de pacote indisponível neste projeto.";return false;
  }
  std::string current;if(!prepareLibraryIndex(assets_,current,diagnostic))return false;
  if(library_.package(prepared.package.id)) {diagnostic="Esta revisão do pacote já pertence à biblioteca.";return false;}
  resources::LibraryIndex candidate;
  if(!resources::LibraryIndex::deserialize(current,candidate,diagnostic)||!candidate.add(prepared.package)){diagnostic="Biblioteca recusou o pacote.";return false;}
  const auto root=EditorImportTransaction::fromUtf8(files_.rootPath());
  // The worker owns the private staging until publication. Hashing stays off
  // the UI thread; reject any ordinary edit between preparation and adoption.
  for(const auto &file:prepared.files) {std::filesystem::path path;std::error_code error;
    if(!EditorImportTransaction::safePath(root,prepared.staging+"/"+file.relative,path)||
       std::filesystem::file_size(path,error)!=file.bytes||error||std::filesystem::last_write_time(path,error)!=file.modified||error) {
      diagnostic="Fonte mudou no preparo: "+file.relative;return false;}}
  std::filesystem::path manifest;std::error_code error;
  if(!EditorImportTransaction::safePath(root,prepared.manifestPath,manifest)||std::filesystem::exists(manifest,error)||error) {
    diagnostic="Destino do pacote ocupado; fontes existentes preservadas.";return false;}
  for(const auto &file:prepared.companions) {std::filesystem::path path;
    if(!EditorImportTransaction::safePath(root,file.relative,path)||std::filesystem::exists(path,error)||error) {
      diagnostic="Destino de fonte ocupado: "+file.relative;return false;}}
  EditorImportTransaction transaction(files_.rootPath());
  if(!transaction.begin(prepared.manifestPath,{},diagnostic,LibraryIndexPath,prepared.companions))return false;
  if(!transaction.commit({reinterpret_cast<const u8*>(prepared.manifest.data()),prepared.manifest.size()},assets_.serialize(),candidate.serialize(),&diagnostic)) {
    transaction.rollback();return false;}
  library_=std::move(candidate);const auto project=files_.rootPath();const auto package=prepared.package;
  const auto manifestPath=prepared.manifestPath,manifestText=prepared.manifest;
  history_.recordResource("Adicionar pacote à biblioteca",[this,project,package,manifestPath,manifestText](bool forward) {
    if(project!=files_.rootPath()||isPlaying())return false;
    std::string encoded,error;if(!prepareLibraryIndex(assets_,encoded,error)){state_.status=error;return false;}
    resources::LibraryIndex candidate;if(!resources::LibraryIndex::deserialize(encoded,candidate,error))return false;
    if(!(forward?candidate.add(package):candidate.remove(package.id)))return false;
    if(!EditorImportTransaction::publishTextBatch(project,{{manifestPath,Sha256::hex({reinterpret_cast<const u8*>(manifestText.data()),manifestText.size()}),manifestText}},
        assets_.serialize(),error,LibraryIndexPath,candidate.serialize())){state_.status=error;return false;}
    library_=std::move(candidate);refreshLibraryRows();state_.status=forward?"Pacote recolocado na biblioteca":"Pacote retirado da biblioteca; fontes e recursos preservados";return true;
  });
  cameraPreview_.close();files_.rebuildTree();state_.libraryPackage=prepared.package.id;state_.libraryPage=0;state_.workspace=EditorWorkspace::Library;
  refreshLibraryRows();diagnostic=prepared.diagnostic;state_.status=diagnostic;return true;
}
void EditorSession::refreshLibraryRows() {
  state_.libraryRows.clear();state_.libraryPackages.clear();
  state_.libraryPackages.push_back({{},"Todos os pacotes"});
  const auto query=lower(state_.libraryQuery);
  for(const auto &package:library_.packages()) {
    state_.libraryPackages.push_back({package.id,package.name});
    if(state_.libraryPackage.valid()&&state_.libraryPackage!=package.id)continue;
    for(const auto &item:package.items) {
      if(state_.libraryKind&&state_.libraryKind!=static_cast<u32>(item.kind)+1)continue;
      if(!query.empty()&&lower(item.path+" "+package.name).find(query)==std::string::npos)continue;
      auto resolved=library_.resolve(item.id,assets_);const auto slash=item.path.rfind('/');
      std::filesystem::path sourcePath;std::error_code ec;
      if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(files_.rootPath()),item.path,sourcePath)||!std::filesystem::exists(sourcePath,ec)||ec) {
        resolved.state=LibraryResolutionState::Incomplete;resolved.diagnostic="Fonte do pacote ausente: "+item.path;
      } else if(resolved.output) {
        std::filesystem::path output;
        if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(files_.rootPath()),resolved.output->path,output)||!std::filesystem::exists(output,ec)||ec) {
          resolved.state=LibraryResolutionState::MissingOutput;resolved.diagnostic="Saída nativa registrada está ausente no disco: "+resolved.output->path;
        }
      }
      state_.libraryRows.push_back({item.id,item.path.substr(slash==item.path.npos?0:slash+1),package.name+" · "+kindName(item.kind),resolved.diagnostic,resolved.state,item.kind});
    }
  }
  if(state_.libraryPackage.valid()&&!library_.package(state_.libraryPackage)){state_.libraryPackage={};refreshLibraryRows();}
}
bool EditorSession::openLibraryItem(resources::AssetGuid id,std::string &diagnostic) {
  const auto *item=library_.item(id);if(!item){diagnostic="Item indisponível.";return false;}
  const auto sourcePath=item->path,sourceIdentity=item->sourceIdentity;
  state_.libraryDetailName=std::filesystem::path(sourcePath).filename().string();
  state_.libraryDetailItem=id;state_.libraryConflict=false;
  state_.libraryCanInstantiate=state_.libraryCanAssignController=state_.libraryCanReimport=false;
  const auto resolution=library_.resolve(id,assets_);
  const auto extension=lower(std::filesystem::path(item->path).extension().string());
  if(extension==".asset") {
    std::filesystem::path absolute;const auto root=EditorImportTransaction::fromUtf8(files_.rootPath());
    if(EditorImportTransaction::safePath(root,item->path,absolute)&&umotionProject(absolute)) {
      // Personagem: o selecionado; sem selecao, o unico modelo com malha
      // deformavel na cena (o caso comum de um personagem so).
      EditorEntityId character=document_.find(state_.selection)&&state_.selection!=document_.root()?state_.selection:kInvalidEntity;
      if(!character) {
        std::vector<EditorEntityId> roots;
        std::vector<EditorEntityId> all;document_.collectSubtree(document_.root(),all);
        for(const auto top:all) {
          if(top==document_.root()||document_.find(top)->parent!=document_.root())continue;
          std::vector<EditorEntityId> subtree;document_.collectSubtree(top,subtree);
          if(std::any_of(subtree.begin(),subtree.end(),[&](EditorEntityId id){const auto *e=document_.find(id);return e&&e->components.find(scene::SkinnedMesh::descriptor);}))roots.push_back(top);
        }
        if(roots.size()!=1) {diagnostic=roots.empty()?"Coloque o personagem na cena antes de aplicar o projeto UMotion.":"Há mais de um personagem na cena: selecione o que recebe o projeto UMotion.";return false;}
        character=roots.front();
      }
      UMotionImportReport report;if(!importUMotionProject(sourcePath,character,report,diagnostic))return false;
      setSelection(character);
      state_.workspace=EditorWorkspace::Scene;
      diagnostic="Projeto UMotion importado: "+std::to_string(report.clips.size())+" clipe(s), "+std::to_string(report.bones)+" ossos, "+
        std::to_string(report.helpers)+" auxiliares, "+std::to_string(report.ikChains)+" IK, "+std::to_string(report.childOf)+" Filho de"+
        (report.silencedAnimations.empty()?std::string("."):"; animação do modelo pausada.");
      state_.status=diagnostic;return true;
    }
  }
  const bool unity=extension==".anim"||extension==".prefab"||extension==".controller";
  if(unity) {
    std::filesystem::path absolute;std::error_code ec;
    const auto root=EditorImportTransaction::fromUtf8(files_.rootPath());
    if(!EditorImportTransaction::safePath(root,item->path,absolute)||!std::filesystem::exists(absolute,ec)||ec) {
      diagnostic="Fonte Unity ausente: "+item->path;return false;
    }
    AssetGuid guid=resolution.output?resolution.output->guid:AssetGuid{};
    if(!guid.valid())guid=importUnityAsset(sourcePath,state_.selection,UnityReimportPolicy::RefuseConflict,diagnostic);
    if(!guid.valid())return false;
    if(extension==".anim") {
      state_.workspace=EditorWorkspace::Scene;return openAnimationClip(guid,state_.selection,diagnostic);
    }
    closeAnimationClip();
    state_.workspace=EditorWorkspace::Library;state_.libraryDetailItem=id;
    state_.libraryCanInstantiate=extension==".prefab";state_.libraryCanAssignController=extension==".controller";state_.libraryCanReimport=true;
    state_.libraryDiagnostic="Recurso nativo pronto: "+assets_.find(guid)->path+"\nFonte: "+sourcePath+"\nIdentidade: "+sourceIdentity;
    if(const auto *current=library_.item(id))for(const auto &loss:current->knownLosses)state_.libraryDiagnostic+="\nConversão: "+loss;
    state_.libraryDiagnosticPage=0;return true;
  }
  if(resolution.state==resources::LibraryResolutionState::Registered&&resolution.output) {
    EditorFileEntry entry;entry.relativePath=resolution.output->path;entry.name=std::filesystem::path(entry.relativePath).filename().string();
    state_.workspace=EditorWorkspace::Scene;openProjectFile(entry);diagnostic=resolution.diagnostic;return true;
  }
  if(item->kind==resources::LibraryItemKind::Material&&item->path.ends_with(".mat")&&item->conversion!=resources::LibraryConversionState::Unsupported) {
    const auto path=item->path;const auto guid=importUnityMaterial(path,diagnostic);if(!guid.valid())return false;
    state_.workspace=EditorWorkspace::Scene;return true;
  }
  if(item->kind==resources::LibraryItemKind::Model) {
    const auto extension=lower(std::filesystem::path(item->path).extension().string());
    if(extension==".fbx"||extension==".glb"||extension==".gltf") {
      reimportPath_=item->path;state_.workspace=EditorWorkspace::Library;
      diagnostic="Preparando modelo original; revise hierarquia, receita e perdas antes de publicar.";return true;
    }
  }
  if(item->kind==resources::LibraryItemKind::Texture) {
    const auto extension=lower(std::filesystem::path(item->path).extension().string());
    if(extension==".png"||extension==".jpg"||extension==".jpeg"||extension==".ktx2"||extension==".tga"||extension==".tif"||extension==".tiff") {
      textureReimportPath_=item->path;state_.workspace=EditorWorkspace::Scene;
      diagnostic="Preparando textura original; revise a receita antes de publicar.";return true;
    }
  }
  diagnostic=resolution.diagnostic.empty()?"Fonte preservada; conversão deste formato ainda não integrada à biblioteca.":resolution.diagnostic;return false;
}
}
