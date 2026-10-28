#include "editor/editor_session.h"
#include "editor/editor_session_internal.h"
#include "editor/editor_animation_pose_selection.h"
#include "resources/unity_animation_import.h"
#include "resources/unity_animation_merge.h"
#include "resources/unity_controller_import.h"
#include "resources/unity_import_recipe.h"
#include "resources/unity_subassets.h"
#include "runtime/transform_math.h"
#include "runtime/unity_prefab.h"
#include "resources/unity_material_import.h"
#include "resources/unistorm_import.h"
#include "resources/model_import.h"
#include "scene/import_link.h"
#include "scene/prefab_link.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <memory>

namespace ae::editor {
namespace {
using namespace resources;
std::string hash(std::string_view text){return Sha256::hex({reinterpret_cast<const u8*>(text.data()),text.size()});}
struct Snapshot {std::optional<AssetRecord> record;std::string text;};
struct Dependency {
  std::vector<AssetGuid> identities;
  AssetGuid owner{};
  bool loop=false;
};
bool equal(const AssetRecord &a,const AssetRecord &b) {
  return a.guid==b.guid&&a.type==b.type&&a.path==b.path&&a.source==b.source&&a.contentHash==b.contentHash&&
    a.importerVersion==b.importerVersion&&a.importerParameters==b.importerParameters&&a.dependencies==b.dependencies&&a.derived==b.derived;
}
u64 retainedBytes(const Snapshot &value) {
  u64 bytes=sizeof(Snapshot)+value.text.capacity();
  if(value.record) {
    const auto &r=*value.record;
    bytes+=r.path.capacity()+r.source.capacity()+r.contentHash.capacity()+r.importerParameters.capacity()+
      r.dependencies.capacity()*sizeof(AssetGuid)+r.derived.capacity()*sizeof(std::string);
    for(const auto &path:r.derived)bytes+=path.capacity();
  }
  return bytes;
}
std::string controllerContent(AnimatorControllerAsset value) {
  value.revision=1;value.graph.nextId=std::numeric_limits<u64>::max();return value.serialize();
}
std::string prefabContent(std::string_view text,std::string &error) {
  runtime::Prefab value;
  if(!value.read(text,defaultEditorComponentRegistry(),error))return {};
  auto graph=value.graph();
  if(!graph.reserveObjectIdsUntil(65537)||!value.capture(graph,value.root(),value.asset(),error))return {};
  return value.write();
}
bool sameNative(std::string_view expected,std::string_view observed,AssetType type,std::string &error) {
  if(expected==observed)return true;
  if(type==AssetType::AnimationClip) {
    AnimationClipAsset before,current;
    if(!AnimationClipAsset::deserialize(expected,before,&error)||!AnimationClipAsset::deserialize(observed,current,&error)||
       current.revision<before.revision||current.nextId<before.nextId)return false;
    before.revision=current.revision;before.nextId=current.nextId;return before.serialize()==current.serialize();
  }
  if(type==AssetType::AnimatorController) {
    AnimatorControllerAsset before,current;
    if(!AnimatorControllerAsset::deserialize(expected,before)||!AnimatorControllerAsset::deserialize(observed,current)||
       current.revision<before.revision||current.graph.nextId<before.graph.nextId)return false;
    before.revision=current.revision;before.graph.nextId=current.graph.nextId;return before.serialize()==current.serialize();
  }
  if(type==AssetType::Prefab) {
    runtime::Prefab before,current;
    if(!before.read(expected,defaultEditorComponentRegistry(),error)||!current.read(observed,defaultEditorComponentRegistry(),error)||
       current.graph().nextObjectId()<before.graph().nextObjectId())return false;
    return prefabContent(expected,error)==prefabContent(observed,error);
  }
  return false;
}
bool read(const std::filesystem::path &root,const std::string &relative,std::string &out,usize maximum=UnityYamlDocument::MaximumBytes) {
  std::filesystem::path path;std::vector<u8> bytes;
  if(!EditorImportTransaction::safePath(root,relative,path)||!EditorImportTransaction::read(path,bytes,maximum))return false;
  out.assign(bytes.begin(),bytes.end());return true;
}
std::string baselinePath(const std::string &digest){return ".astra/imports/unity-baselines/"+digest+".source";}
bool baseline(const std::filesystem::path &root,const std::string &text,std::string &digest,std::string &error) {
  digest=hash(text);const auto relative=baselinePath(digest);std::filesystem::path path;std::error_code ec;
  if(!EditorImportTransaction::safePath(root,relative,path))return false;
  if(std::filesystem::exists(path,ec)) {std::string existing;return !ec&&read(root,relative,existing)&&hash(existing)==digest;}
  std::filesystem::create_directories(path.parent_path(),ec);
  if(ec)return false;
  auto prepared=path;prepared+=".prepared";
  if(!EditorImportTransaction::write(prepared,{reinterpret_cast<const u8*>(text.data()),text.size()},&error))return false;
  std::vector<u8> verification;
  if(!EditorImportTransaction::read(prepared,verification,UnityYamlDocument::MaximumBytes)||Sha256::hex(verification)!=digest)return false;
  std::filesystem::rename(prepared,path,ec);return !ec;
}
}

// One editor writer, shared by Library conversion, reimport and History. This
// class is a Session friend to keep the substantial importer in its own TU;
// it does not create a second registry, scene or resource ownership system.
class UnityPackageImporter {
public:
  static AssetGuid import(EditorSession &session,const std::string &path,EditorEntityId owner,UnityReimportPolicy policy,std::string &error);
private:
  static bool resolve(EditorSession &,const std::string &,const UnityObjectReference &,AssetType,Dependency &,std::string &,bool cascade=true);
  // G6a: o material de partícula do pacote (dica para o componente) e a malha
  // de partícula, que converte a fonte FBX/GLB do pacote quando preciso.
  static bool particleLook(EditorSession &,const std::string &,const UnityObjectReference &,UnityParticleLook &);
  static bool particleMesh(EditorSession &,const std::string &,const UnityObjectReference &,Dependency &,std::string &);
  static const LibraryItem *sourceItem(EditorSession &,const std::string &origin,const AssetGuid &guid);
  // G6b: o prefab é o efeito de um Weather Type do UniStorm do mesmo pacote?
  // Então o sistema o prende ao jogador nessa distância (eixos da Astra).
  static bool uniStormEffectOffset(EditorSession &,const std::string &origin,std::array<float,3> &offset,float &amount);
  static bool publish(EditorSession &,const Snapshot &,Snapshot &,std::string &);
};

const LibraryItem *UnityPackageImporter::sourceItem(EditorSession &session,const std::string &origin,const AssetGuid &guid) {
  const LibraryPackage *scope=nullptr;
  for(const auto &package:session.library_.packages())for(const auto &item:package.items)if(item.path==origin)scope=&package;
  if(!scope)return nullptr;
  const LibraryItem *found=nullptr;
  for(const auto &item:scope->items)if(item.sourceIdentity=="unity:"+guid.text()){if(found)return nullptr;found=&item;}
  return found;
}

bool UnityPackageImporter::uniStormEffectOffset(EditorSession &session,const std::string &origin,std::array<float,3> &offset,float &amount) {
  const LibraryPackage *scope=nullptr;const LibraryItem *self=nullptr;
  for(const auto &package:session.library_.packages())for(const auto &item:package.items)if(item.path==origin){scope=&package;self=&item;}
  if(!scope||!self||!self->sourceIdentity.starts_with("unity:"))return false;
  AssetGuid guid;if(!AssetGuid::parse(std::string_view(self->sourceIdentity).substr(6),guid))return false;
  const auto root=EditorImportTransaction::fromUtf8(session.files_.rootPath());
  for(const auto &item:scope->items) {
    if(!item.path.ends_with(".asset"))continue;
    std::string encoded,error,script;UnityYamlDocument document;UniStormWeather weather;
    if(!read(root,item.path,encoded,256u*1024u)||!readUnityYaml(encoded,document,error)||!uniStormScript(document,script)||
       script!=UniStormWeatherTypeScript||!readUniStormWeather(document,weather,error))continue;
    const float *vector=weather.effect.guid==guid?weather.effectOffset:weather.additionalEffect.guid==guid?weather.additionalOffset:nullptr;
    if(!vector)continue;
    // Unity → Astra espelha o Z, como o resto do prefab.
    offset={vector[0],vector[1],-vector[2]};
    amount=vector==weather.effectOffset?weather.effectAmount:weather.additionalAmount;
    return true;
  }
  return false;
}

bool UnityPackageImporter::particleLook(EditorSession &session,const std::string &origin,const UnityObjectReference &reference,UnityParticleLook &out) {
  const auto *source=sourceItem(session,origin,reference.guid);if(!source||!source->path.ends_with(".mat"))return false;
  std::string text;const auto root=EditorImportTransaction::fromUtf8(session.files_.rootPath());
  if(!read(root,source->path,text,256u*1024u))return false;
  UnityMaterialImport converted;if(!readUnityMaterial(text,converted)||converted.particle.family==UnityParticleFamily::None)return false;
  out=converted.particle;return true;
}

bool UnityPackageImporter::particleMesh(EditorSession &session,const std::string &origin,const UnityObjectReference &reference,Dependency &out,std::string &error) {
  if(resolve(session,origin,reference,AssetType::Mesh,out,error,false))return true;
  // A fonte do modelo ainda não foi convertida: o perfil padrão dela, sem
  // instanciar nada na cena (só o recurso), e de novo a resolução por nome.
  const auto *source=sourceItem(session,origin,reference.guid);
  if(!source)return false;
  const auto path=source->path;
  const auto extension=std::filesystem::path(path).extension().string();
  if(extension!=".fbx"&&extension!=".FBX"&&extension!=".glb"&&extension!=".gltf"){error="Malha de partícula fora de FBX/glTF: "+path;return false;}
  for(const auto &record:session.assets_.records())if(record.source==path||record.path==path)return false;
  std::vector<u8> bytes;std::filesystem::path absolute;
  if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(session.files_.rootPath()),path,absolute)||
     !EditorImportTransaction::read(absolute,bytes,256u*1024u*1024u)){error="Fonte do modelo ilegível: "+path;return false;}
  const auto profile=session.importProfileForPath(path);
  GltfImport model;
  if(!importModelSource(bytes,session.importLimitsFor(profile),{},model)){error=path+": "+model.diagnostic;return false;}
  EditorSession::ModelImportReport report;
  if(!session.publishModel(model,Sha256::hex(bytes),path,report,ImportAmbiguityPolicy::Refuse,profile.excludedNodes)){error=path+": "+report.diagnostic;return false;}
  session.persistImportMap(report.source);
  return resolve(session,origin,reference,AssetType::Mesh,out,error,false);
}

bool UnityPackageImporter::resolve(EditorSession &session,const std::string &origin,const UnityObjectReference &reference,
    AssetType type,Dependency &out,std::string &error,bool cascade) {
  const auto fail=[&](const std::string &message){error=message;return false;};
  if(!reference.external()||!reference.fileId)return fail("Dependência Unity exige GUID/fileID externo completo.");
  const LibraryPackage *scope=nullptr;
  for(const auto &package:session.library_.packages())for(const auto &item:package.items)if(item.path==origin)scope=&package;
  if(!scope)return fail("Converta fontes Unity a partir de um pacote preservado na Biblioteca.");
  const LibraryItem *source=nullptr;
  for(const auto &item:scope->items)if(item.sourceIdentity=="unity:"+reference.guid.text()) {
    if(source)return fail("GUID Unity duplicado nesta revisão de pacote.");
    source=&item;
  }
  if(!source)return fail("Dependência de origem ausente: GUID "+reference.guid.text()+" · fileID "+std::to_string(reference.fileId));
  const auto root=EditorImportTransaction::fromUtf8(session.files_.rootPath());std::string meta;UnitySourceMetadata metadata;
  if(!read(root,source->path+".meta",meta,1u<<20)||!readUnitySourceMetadata(meta,metadata,error)||metadata.guid!=reference.guid)
    return fail("Metadados da dependência ausentes ou GUID alterado: "+source->path);
  const AssetRecord *record=nullptr;
  for(const auto &entry:session.assets_.records())if((entry.source==source->path||entry.path==source->path)&&
      (entry.type==type||(type==AssetType::AnimationClip&&entry.type==AssetType::Mesh))) {
    if(record)return fail("Mais de uma saída corresponde à dependência Unity: "+source->path);
    record=&entry;
  }
  // G6a: material, áudio e prefab do pacote convertem na hora, pela mesma porta
  // da Biblioteca (com receita e histórico próprios). A malha de MeshRenderer
  // continua pedindo a conversão revisada do modelo.
  if(!record&&cascade) {
    static thread_local u32 depth=0;
    const auto path=source->path;std::string nested;bool converted=false;
    if(depth<8) {
      ++depth;
      if(type==AssetType::Material&&path.ends_with(".mat"))converted=session.importUnityMaterial(path,nested).valid();
      else if(type==AssetType::AudioClip&&(path.ends_with(".wav")||path.ends_with(".WAV"))){AssetGuid clip;converted=session.importWaveClip(path,clip,nested);}
      else if(type==AssetType::Prefab&&path.ends_with(".prefab"))converted=import(session,path,0,UnityReimportPolicy::RefuseConflict,nested).valid();
      --depth;
    }
    if(converted)return resolve(session,origin,reference,type,out,error,false);
    if(!nested.empty())return fail("Dependência não convertida ("+path+"): "+nested);
  }
  if(!record)return fail("Converta primeiro a dependência pela Biblioteca: "+source->path+" · fileID "+std::to_string(reference.fileId));
  Dependency result;result.owner=record->guid;
  if(record->type==AssetType::Mesh) {
    const auto *subasset=metadata.find(reference.fileId);
    if(!subasset)return fail("fileID não declarado no .meta do modelo: "+std::to_string(reference.fileId)+" · "+source->path);
    const auto found=std::find_if(session.importedSources_.begin(),session.importedSources_.end(),[&](const auto &value){return value.guid==record->guid;});
    if(found==session.importedSources_.end())return fail("Modelo convertido não está carregado: "+source->path);
    if(type==AssetType::Mesh) {
      if(subasset->classId&&subasset->classId!=43)return fail("fileID Unity não representa Mesh.");
      u32 node=std::numeric_limits<u32>::max();
      for(usize draw=0;draw<found->sourceDrawCount&&draw<found->names.size()&&draw<found->identities.size()&&draw<found->drawNodes.size();++draw)
        if(found->names[draw]==subasset->name) {
          if(node!=std::numeric_limits<u32>::max()&&node!=found->drawNodes[draw])return fail("Nome de Mesh .meta é ambíguo no modelo convertido: "+subasset->name);
          node=found->drawNodes[draw];result.identities.push_back(found->identities[draw]);
        }
      if(result.identities.empty())return fail("Mesh declarada no .meta não corresponde ao resultado convertido: "+subasset->name);
      for(const auto id:result.identities)if(!session.mapScene_.assetSlot(id))return fail("Mesh convertida ainda não foi publicada no renderer.");
    } else {
      if(subasset->classId&&subasset->classId!=74)return fail("fileID Unity não representa AnimationClip.");
      const auto ids=animationClipGuids(found->guid,found->animations);
      for(usize clip=0;clip<found->animations.size();++clip)if(found->animations[clip].name==subasset->name)result.identities.push_back(ids[clip]);
      if(result.identities.size()!=1)return fail("Take Unity exige nome/intervalo explícito correspondente à fonte: "+subasset->name);
      UnityYamlNode parsed;if(!readUnityMeta(meta,parsed,error))return false;
      const auto *importer=parsed.get("ModelImporter"),*animations=importer?importer->get("animations"):nullptr;
      const auto *clips=animations?animations->get("clipAnimations"):nullptr;bool specified=false;
      if(clips&&clips->kind==UnityYamlNode::Kind::Sequence)for(const auto &clip:clips->elements) {
        const auto *name=clip.get("name"),*loop=clip.get("loopTime");
        if(name&&name->scalar==subasset->name&&loop&&loop->boolean(result.loop))specified=true;
      }
      if(!specified)result.loop=false;
    }
  } else if(type==AssetType::AudioClip) {
    // WAV: um recurso por arquivo (o fileID 8300000 da Unity é o próprio clipe).
    result.identities.push_back(record->guid);
  } else {
    std::string encoded;UnityYamlDocument document;
    if(!read(root,source->path,encoded)||!readUnityYaml(encoded,document,error))return false;
    const auto *object=document.find(reference.fileId);
    // Prefab: a referência aponta para um GameObject (ou o Prefab raiz) dele.
    const bool matches=object&&(type==AssetType::Prefab?object->classId==1||object->classId==1001:
                                object->classId==(type==AssetType::Material?21u:type==AssetType::AnimationClip?74u:0u));
    if(!matches)return fail("GUID existe, mas fileID/tipo da dependência não corresponde: "+source->path);
    result.identities.push_back(record->guid);
    if(type==AssetType::AnimationClip) {
      UnityImportRecipe recipe;if(!UnityImportRecipe::deserialize(record->importerParameters,recipe,error))return false;
      result.loop=recipe.loop;
    }
  }
  out=std::move(result);error.clear();return true;
}

bool UnityPackageImporter::publish(EditorSession &session,const Snapshot &expected,Snapshot &target,std::string &error) {
  const auto fail=[&](const char *message){error=message;return false;};
  if(session.isPlaying()||session.playMirrorOpen_||session.history_.isOpen()||(!expected.record&&!target.record))return fail("Publicação Unity indisponível neste estado do editor.");
  const auto &identity=target.record?*target.record:*expected.record;
  const auto root=EditorImportTransaction::fromUtf8(session.files_.rootPath());const auto *current=session.assets_.find(identity.guid);
  if(bool(current)!=bool(expected.record))return fail("Recurso Unity mudou; histórico preservado.");
  if(current) {
    auto logical=*expected.record;logical.contentHash=current->contentHash;
    if(!equal(*current,logical))return fail("Receita ou dependências Unity mudaram; histórico preservado.");
  }
  std::string disk;std::filesystem::path absolute;std::error_code ec;
  if(!EditorImportTransaction::safePath(root,identity.path,absolute))return false;
  if(expected.record) {
    if(!read(root,identity.path,disk)||hash(disk)!=current->contentHash||!sameNative(expected.text,disk,identity.type,error))return fail("Saída Unity mudou no disco; nada publicado.");
  } else if(std::filesystem::exists(absolute,ec)||ec)return fail("Destino Unity ocupado.");
  // Reabrir o projeto atualiza o registro na memória e só grava no próximo
  // salvamento: disco atrás da memória é esperado; divergente, recusa.
  if(session.assets_.size()&&!session_detail::persistedRegistry(root,session.assets_,session.assetRegistryDirty_))return fail("Registro mudou no disco; reabra antes de converter.");
  auto nextAssets=session.assets_;auto clips=session.animationClipAssets_;auto controllers=session.animatorControllers_;
  const auto guid=identity.guid;
  std::erase_if(clips,[&](const auto &value){return value.guid==guid;});
  std::erase_if(controllers,[&](const auto &value){return value.guid==guid;});
  if(target.record) {
    UnityImportRecipe recipe;if(!UnityImportRecipe::deserialize(target.record->importerParameters,recipe,error))return false;
    if(target.record->type==AssetType::AnimationClip) {
      AnimationClipAsset value;if(!AnimationClipAsset::deserialize(target.text,value,&error)||value.guid!=guid)return false;
      if(const auto *old=session.animationClipAsset(guid)) {
        if(old->revision==std::numeric_limits<u32>::max())return fail("Revisão de clipe esgotada.");
        value.revision=std::max(value.revision,old->revision+1);value.nextId=std::max(value.nextId,old->nextId);
      }
      target.text=value.serialize();clips.push_back(std::move(value));
    } else if(target.record->type==AssetType::AnimatorController) {
      AnimatorControllerAsset value;if(!AnimatorControllerAsset::deserialize(target.text,value)||value.guid!=guid)return fail("Controller nativo inválido.");
      if(const auto *old=findAnimatorController(session.animatorControllers_,guid)) {
        if(old->revision==std::numeric_limits<u32>::max())return fail("Revisão de controller esgotada.");
        value.revision=std::max(value.revision,old->revision+1);value.graph.nextId=std::max(value.graph.nextId,old->graph.nextId);
      }
      target.text=value.serialize();controllers.push_back(std::move(value));
    } else if(target.record->type==AssetType::Prefab) {
      runtime::Prefab value,resolved;
      if(!value.read(target.text,defaultEditorComponentRegistry(),error)||value.asset()!=guid||!session.preparePrefab(value,resolved,error))return false;
      if(expected.record) {
        runtime::Prefab old;if(!old.read(disk,defaultEditorComponentRegistry(),error))return false;
        auto graph=value.graph();const auto frontier=std::max(graph.nextObjectId(),old.graph().nextObjectId());
        if(!graph.reserveObjectIdsUntil(frontier)||!value.capture(graph,value.root(),guid,error))return false;
        recipe.nextIdentity=std::max(recipe.nextIdentity,static_cast<u64>(frontier));
        target.record->importerParameters=recipe.serialize();if(target.record->importerParameters.empty())return fail("Mapa Unity excedeu orçamento da receita.");
      }
      target.text=value.write();
    } else return fail("Tipo Unity não pertence a este publicador.");
    target.record->contentHash=hash(target.text);
    if(current) {
      if(!nextAssets.publishImport(guid,target.record->contentHash,target.record->importerVersion,target.record->importerParameters,
          target.record->derived,target.record->dependencies))return fail("Registro recusou a reimportação Unity.");
    } else if(!nextAssets.add(*target.record))return fail("Registro recusou a conversão Unity.");
  } else {
    if(session.sceneUsersOf(guid)||!nextAssets.remove(guid))return fail("Recurso Unity ainda está em uso; desfaça seus vínculos primeiro.");
  }
  std::vector<std::shared_ptr<const runtime::SourceAnimations>> library;library.reserve(clips.size());
  for(const auto &value:clips) {
    auto source=std::make_shared<runtime::SourceAnimations>();AnimationClip clip;
    if(!value.compile(clip,&error))return false;
    source->channelsValidated=true;source->source=value.source.valid()?value.source:value.guid;
    source->clips.push_back(std::move(clip));source->clipIds.push_back(value.guid);
    for(const auto &binding:value.bindings){source->nodes.push_back(binding.sourceNode);source->nodeNames.push_back(binding.name);source->nodePaths.push_back(binding.path);}
    library.push_back(std::move(source));
  }
  if(!session.mapScene_.validateAuthoredAnimations(library))return fail("Biblioteca recusou clipe Unity antes da publicação.");
  std::string index;if(!session.prepareLibraryIndex(nextAssets,index,error))return false;
  LibraryIndex nextLibrary;if(!LibraryIndex::deserialize(index,nextLibrary,error))return false;
  EditorImportTransaction transaction(session.files_.rootPath());
  if(!transaction.begin(identity.path,expected.record?hash(disk):std::string(),error,LibraryIndexPath))return false;
  const bool ok=target.record?transaction.commit({reinterpret_cast<const u8*>(target.text.data()),target.text.size()},nextAssets.serialize(),index,&error):transaction.commitRemoval(nextAssets.serialize(),index);
  if(!ok){transaction.rollback();return false;}
  session.clipPreview_.cancel();session.clipDrag_.reset();session.clipPoseDraft_.reset();
  session.assets_=std::move(nextAssets);session.animationClipAssets_=std::move(clips);session.animatorControllers_=std::move(controllers);
  session.mapScene_.setAuthoredAnimations(std::move(library));session.files_.rebuildTree();session.assetRegistryDirty_=false;
  session.library_=std::move(nextLibrary);session.refreshLibraryRows();session.refreshAnimationClip();session.appearanceChanged_=true;
  if(target.record)target.record=*session.assets_.find(guid);
  error.clear();return true;
}

AssetGuid UnityPackageImporter::import(EditorSession &session,const std::string &path,EditorEntityId owner,UnityReimportPolicy policy,std::string &error) {
  const auto fail=[&](const std::string &message)->AssetGuid{error=message;return {};};
  if(session.isPlaying()||session.playMirrorOpen_||session.history_.isOpen()||session.files_.rootPath().empty()||static_cast<u8>(policy)>2)
    return fail("Abra um projeto em Edit e finalize a edição antes de converter Unity.");
  const auto extension=std::filesystem::path(path).extension().string();
  const UnityImportKind kind=extension==".anim"?UnityImportKind::Clip:extension==".prefab"?UnityImportKind::Prefab:UnityImportKind::Controller;
  if(extension!=".anim"&&extension!=".prefab"&&extension!=".controller")return fail("Selecione .anim, .prefab ou .controller preservado na Biblioteca.");
  const auto root=EditorImportTransaction::fromUtf8(session.files_.rootPath());std::string source,meta;
  UnitySourceMetadata metadata;UnityYamlDocument document;
  if(!read(root,path,source)||!read(root,path+".meta",meta,1u<<20)||!readUnitySourceMetadata(meta,metadata,error)||!readUnityYaml(source,document,error))
    return fail(error.empty()?"Fonte Unity ou .meta ausente/inválido.":error);
  const LibraryItem *catalog=nullptr;
  for(const auto &package:session.library_.packages())for(const auto &item:package.items)if(item.path==path)catalog=&item;
  if(!catalog||catalog->sourceIdentity!="unity:"+metadata.guid.text())return fail("Fonte fora da Biblioteca ou GUID .meta mudou; importe uma revisão explícita do pacote.");
  Snapshot before,after;const AssetRecord *existing=nullptr;
  for(const auto &record:session.assets_.records())if(record.source==path){if(existing)return fail("Mais de uma saída para esta fonte Unity.");existing=&record;}
  UnityImportRecipe oldRecipe,recipe;std::string oldBaseline;
  if(existing) {
    before.record=*existing;
    if(!read(root,existing->path,before.text)||hash(before.text)!=existing->contentHash||
       !UnityImportRecipe::deserialize(existing->importerParameters,oldRecipe,error)||oldRecipe.kind!=kind||oldRecipe.unityGuid!=metadata.guid||
       !read(root,baselinePath(oldRecipe.baselineHash),oldBaseline)||hash(oldBaseline)!=oldRecipe.baselineHash)return fail("Baseline/receita/saída Unity ausente ou modificada; nada publicado.");
  }
  AssetRecord record;record.guid=existing?existing->guid:assetGuidFromSeed("unity-native:"+catalog->id.text());
  record.source=path;record.path=existing?existing->path:path+(kind==UnityImportKind::Clip?".aeclip":kind==UnityImportKind::Prefab?".astra.prefab":".aeanimator");
  record.type=kind==UnityImportKind::Clip?AssetType::AnimationClip:kind==UnityImportKind::Prefab?AssetType::Prefab:AssetType::AnimatorController;
  if(existing&&existing->type!=record.type)return fail("Tipo de saída Unity mudou; publique como recurso distinto.");
  record.importerVersion=1;recipe.kind=kind;recipe.unityGuid=metadata.guid;recipe.sourceHash=hash(source);recipe.metaHash=hash(meta);
  std::string native,sourceBaseline;
  const auto dependency=[&](AssetGuid guid){if(guid.valid()&&guid!=record.guid&&std::find(record.dependencies.begin(),record.dependencies.end(),guid)==record.dependencies.end())record.dependencies.push_back(guid);};
  if(kind==UnityImportKind::Clip) {
    std::vector<UnityAnimationMorphBinding> morphs;
    if(owner&&session.document_.exists(owner)) {
      std::vector<EditorEntityId> objects;session.document_.collectSubtree(owner,objects);
      for(const auto id:objects)if(const auto *object=session.document_.find(id))if(const auto *render=meshRenderer(*object)) {
        UnityAnimationMorphBinding binding;if(!animationPoseTargetPath(session.document_,owner,id,binding.path,error))return {};
        bool found=false;
        for(u32 slot=0;slot<render->slotCount();++slot)for(const auto &model:session.importedSources_)
          for(usize draw=0;draw<model.identities.size()&&draw<model.drawMorphs.size();++draw)if(model.identities[draw]==render->slotAsset(slot)&&model.drawMorphs[draw]>=0) {
            const auto morph=static_cast<usize>(model.drawMorphs[draw]);
            if(morph>=model.morphs.size())return fail("Índice morph da malha convertida inválido.");
            const auto &set=model.morphs[morph];
            if(found&&(binding.names!=set.names||binding.defaults!=set.defaultWeights))return fail("Submeshes do alvo têm conjuntos morph diferentes; escolha uma malha coerente.");
            binding.names=set.names;binding.defaults=set.defaultWeights;found=true;dependency(model.guid);
          }
        if(found)morphs.push_back(std::move(binding));
      }
    }
    // Modelo importado de FBX sob o dono (sem prefab convertido): a Unity o vê
    // com X espelhado, então as curvas de Transform espelham X; senão Z.
    UnityHandedness handedness=UnityHandedness::MirrorZ;
    if(owner&&session.document_.exists(owner)&&!scene::prefabLink(session.document_.find(owner)->components)) {
      std::vector<EditorEntityId> objects;session.document_.collectSubtree(owner,objects);
      for(const auto id:objects)if(const auto *object=session.document_.find(id);object&&scene::importLink(object->components))
        {handedness=UnityHandedness::MirrorX;break;}
    }
    // Humanoid: o personagem alvo em descanso, o mapeamento do Avatar da Unity
    // e a pose em T dele (humanDescription do .meta do modelo), com reserva
    // pelos nomes padrão.
    UnityHumanoidTarget humanoid;bool humanoidReady=false;
    if(owner&&session.document_.exists(owner)) {
      std::vector<EditorEntityId> objects;session.document_.collectSubtree(owner,objects);
      std::map<std::string,i32> avatar;
      struct Tpose {float position[3]{},rotation[4]{0,0,0,1};};std::map<std::string,Tpose> tpose;
      const auto number=[](std::string_view content,std::string_view key,float &out) {
        const auto at=content.find(key);if(at==std::string_view::npos)return false;
        out=std::strtof(std::string(content.substr(at+key.size(),24)).c_str(),nullptr);return std::isfinite(out);
      };
      const auto parseSkeleton=[&](const std::string &text) {
        const auto description=text.find("humanDescription:");if(description==std::string::npos)return;
        const auto skeleton=text.find("\n    skeleton:",description);if(skeleton==std::string::npos)return;
        std::string bone;usize at=text.find('\n',skeleton+1);
        while(at!=std::string::npos&&at+1<text.size()) {
          const auto end=text.find('\n',at+1);
          std::string_view line(text.data()+at+1,(end==std::string::npos?text.size():end)-at-1);
          while(!line.empty()&&(line.back()=='\r'||line.back()==' '))line.remove_suffix(1);
          const auto first=line.find_first_not_of(' ');
          if(first!=std::string_view::npos&&first<=4&&!line.substr(first).starts_with("- "))break;
          if(first!=std::string_view::npos) {
            auto content=line.substr(first);if(content.starts_with("- "))content.remove_prefix(2);
            if(content.starts_with("name: "))bone=std::string(content.substr(6));
            else if(!bone.empty()&&content.starts_with("position: ")) {
              auto &pose=tpose[bone];number(content,"x: ",pose.position[0]);number(content,"y: ",pose.position[1]);number(content,"z: ",pose.position[2]);
            } else if(!bone.empty()&&content.starts_with("rotation: ")) {
              auto &pose=tpose[bone];
              number(content,"x: ",pose.rotation[0]);number(content,"y: ",pose.rotation[1]);number(content,"z: ",pose.rotation[2]);number(content,"w: ",pose.rotation[3]);
            }
          }
          at=end;
        }
      };
      const auto parseAvatar=[&](const std::string &text) {
        const auto description=text.find("humanDescription:");if(description==std::string::npos)return;
        const auto human=text.find("human:",description);if(human==std::string::npos)return;
        std::string bone;usize at=text.find('\n',human);
        while(at!=std::string::npos&&at+1<text.size()) {
          const auto end=text.find('\n',at+1);
          std::string_view line(text.data()+at+1,(end==std::string::npos?text.size():end)-at-1);
          while(!line.empty()&&(line.back()=='\r'||line.back()==' '))line.remove_suffix(1);
          const auto first=line.find_first_not_of(' ');
          if(first!=std::string_view::npos&&first<=4&&line.substr(first)!="human:"&&!line.substr(first).starts_with("- "))break;
          if(first!=std::string_view::npos) {
            auto content=line.substr(first);if(content.starts_with("- "))content.remove_prefix(2);
            if(content.starts_with("boneName: "))bone=std::string(content.substr(10));
            else if(content.starts_with("humanName: ")&&!bone.empty()) {
              if(const auto index=unityHumanBoneIndex(content.substr(11));index>=0)avatar[bone]=index;
              bone.clear();
            }
          }
          at=end;
        }
      };
      // O .meta da fonte do modelo: ao lado do arquivo de origem, ou na Biblioteca.
      for(const auto id:objects)if(const auto *object=session.document_.find(id))if(const auto *link=scene::importLink(object->components)) {
        if(const auto *source=session.assets_.find(link->source)) {
          const auto fileName=std::filesystem::path(source->source.empty()?source->path:source->source).filename().string();
          std::vector<std::string> candidates{source->source+".meta",source->path+".meta"};
          for(const auto &package:session.library_.packages())for(const auto &item:package.items)
            if(std::filesystem::path(item.path).filename().string()==fileName)candidates.push_back(item.path+".meta");
          for(const auto &candidate:candidates) {
            std::string text;
            if(candidate.size()>5&&read(root,candidate,text,8u<<20)) {parseAvatar(text);if(!avatar.empty()){parseSkeleton(text);break;}}
          }
        }
        break;
      }
      // Reserva: o nome humano da Unity ou o padrão Mixamo no nome do osso.
      const auto key=[](std::string_view name) {
        std::string out;if(const auto colon=name.rfind(':');colon!=std::string_view::npos)name.remove_prefix(colon+1);
        for(const char c:name)if(std::isalnum(static_cast<unsigned char>(c)))out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        return out;
      };
      std::map<std::string,i32> names;
      for(u32 bone=0;bone<UnityHumanBoneCount;++bone)names[key(unityHumanBoneName(bone))]=static_cast<i32>(bone);
      for(const auto &[alias,bone]:std::initializer_list<std::pair<const char*,i32>>{{"leftupleg",1},{"rightupleg",2},{"leftleg",3},{"rightleg",4},
          {"spine1",8},{"spine2",54},{"leftarm",13},{"rightarm",14},{"leftforearm",15},{"rightforearm",16},{"lefttoebase",19},{"righttoebase",20}})
        names[alias]=bone;
      std::map<EditorEntityId,i32> index;
      for(const auto id:objects) {
        const auto *object=session.document_.find(id);if(!object)continue;
        UnityHumanoidTarget::Node node;node.parent=id==owner?-1:(index.count(object->parent)?index[object->parent]:-1);
        if(id!=owner&&node.parent<0)continue;
        if(id!=owner) {
          if(!avatar.empty()) {if(const auto found=avatar.find(object->name);found!=avatar.end())node.human=found->second;}
          else if(const auto found=names.find(key(object->name));found!=names.end())node.human=found->second;
        }
        for(u32 c=0;c<3;++c){node.translation[c]=object->transform.position[c];node.scale[c]=object->transform.scale[c];}
        runtime::transformRotationQuaternion(object->transform,node.rotation);
        // Pose em T do Avatar (coordenadas da Unity) no espelhamento do alvo.
        if(id!=owner)if(const auto found=tpose.find(object->name);found!=tpose.end()) {
          const auto &p=found->second.position;const auto &q=found->second.rotation;node.hasTpose=true;
          const bool mirrorX=handedness==UnityHandedness::MirrorX;
          node.tposeTranslation[0]=mirrorX?-p[0]:p[0];node.tposeTranslation[1]=p[1];node.tposeTranslation[2]=mirrorX?p[2]:-p[2];
          node.tposeRotation[0]=mirrorX?q[0]:-q[0];node.tposeRotation[1]=-q[1];node.tposeRotation[2]=mirrorX?-q[2]:q[2];node.tposeRotation[3]=q[3];
        }
        if(node.human>=0) {
          for(const auto &other:humanoid.nodes)if(other.human==node.human){node.human=-1;break;}
          if(node.human>=0&&!animationPoseTargetPath(session.document_,owner,id,node.path,error))return {};
        }
        index[id]=static_cast<i32>(humanoid.nodes.size());humanoid.nodes.push_back(std::move(node));
      }
      for(const auto &node:humanoid.nodes)if(node.human==0)humanoidReady=true;
    }
    UnityAnimationImport imported;
    if(!convertUnityAnimation(document,record.guid,metadata.guid,recipe.sourceHash,morphs,humanoidReady?&humanoid:nullptr,imported,error,handedness))return {};
    auto value=imported.clip;recipe.fileId=imported.fileId;recipe.loop=imported.loop;recipe.transformations=imported.transformations;
    if(existing) {
      AnimationClipAsset previous,current,merged,next;UnityAnimationMergeReport report;
      if(!AnimationClipAsset::deserialize(oldBaseline,previous,&error)||!AnimationClipAsset::deserialize(before.text,current,&error)||
         !mergeUnityAnimation(previous,current,value,policy,merged,next,report,error)) {
        for(const auto &conflict:report.conflicts)error+="\n"+conflict;
        return {};
      }
      value=std::move(merged);sourceBaseline=next.serialize();
      for(const auto &conflict:report.conflicts)recipe.transformations.push_back((policy==UnityReimportPolicy::KeepLocal?"Override local preservado: ":"Origem recebida: ")+conflict);
    } else sourceBaseline=value.serialize();
    dependency(value.source);
    for(const auto &property:value.properties) {
      if(property.address.resource.valid())dependency(property.address.resource);
      for(const auto &key:property.data.keys)if(const auto *id=std::get_if<AssetGuid>(&key.value);id&&id->valid())dependency(*id);
    }
    for(const auto &input:value.drivers.inputs) {
      if(input.address.resource.valid())dependency(input.address.resource);
      if(input.controller.valid())dependency(input.controller);
    }
    for(const auto &target:value.drivers.targets)if(target.address.resource.valid())dependency(target.address.resource);
    if(owner) {AnimationClipPreview preview;if(!preview.begin(session.document_,owner,value,error)||!preview.seek(0,error)||!preview.seek(value.duration,error))return {};}
    native=value.serialize();recipe.nextIdentity=value.nextId;
  } else if(kind==UnityImportKind::Prefab) {
    // G6a: scripts do pacote pelo GUID do .cs (o nome do arquivo é o da classe).
    UnityPrefabContext context;
    context.scriptName=[&session,path](const AssetGuid &guid)->std::string {
      const auto *item=sourceItem(session,path,guid);
      return item&&item->path.ends_with(".cs")?std::filesystem::path(item->path).stem().string():std::string();
    };
    UnityPrefabImport imported;if(!convertUnityPrefab(document,imported,error,&context))return {};
    std::array<float,3> offset{};float amount=0;
    if(uniStormEffectOffset(session,path,offset,amount))
      for(auto &object:imported.objects)if(!object.parentTransform) {
        object.followCamera=offset;
        // O prefab vem com taxa 0: o UniStorm escreve a quantidade do tipo.
        if(object.particle&&amount>0) {
          auto &rate=object.particle->settings.emissionRate;
          if(!rate.usesCurves()&&rate.constantMax<=0) {rate.mode=scene::ParticleCurveMode::Constant;rate.constantMin=rate.constantMax=amount;}
        }
        imported.transformations.push_back("Efeito de clima do UniStorm: Seguir câmera a "+std::to_string(int(offset[1]))+" m acima, como o sistema prende o efeito ao jogador.");
        break;
      }
    runtime::UnityPrefabIdentityMap previous,identities;
    if(existing){previous.nextObject=static_cast<u32>(oldRecipe.nextIdentity);for(const auto &[file,id]:oldRecipe.identities)previous.entries.push_back({file,static_cast<u32>(id)});}
    const runtime::UnityPrefabResolver resolver=[&](const UnityObjectReference &reference,AssetType type,std::vector<AssetGuid> &ids,std::string &diagnostic) {
      Dependency result;if(!resolve(session,path,reference,type,result,diagnostic))return false;
      ids=std::move(result.identities);dependency(result.owner);return true;
    };
    std::vector<std::string> notes,minor;runtime::UnityPrefabEffectServices effects;effects.notes=&notes;effects.approximations=&minor;
    effects.particleLook=[&session,path](const UnityObjectReference &reference,UnityParticleLook &look){return particleLook(session,path,reference,look);};
    effects.particleMesh=[&](const UnityObjectReference &reference,AssetType,std::vector<AssetGuid> &ids,std::string &diagnostic) {
      Dependency result;if(!particleMesh(session,path,reference,result,diagnostic))return false;
      ids=std::move(result.identities);dependency(result.owner);return true;
    };
    runtime::Prefab value;
    if(!runtime::buildUnityPrefab(imported,record.guid,resolver,existing?&previous:nullptr,value,identities,error,&effects))return {};
    sourceBaseline=value.write();native=sourceBaseline;recipe.fileId=imported.rootFileId;
    recipe.nextIdentity=identities.nextObject;for(const auto &entry:identities.entries)recipe.identities.emplace_back(entry.fileId,entry.object);
    recipe.transformations=imported.transformations;
    for(const auto &text:notes)recipe.transformations.push_back("Perda: "+text);
    for(const auto &text:minor)recipe.transformations.push_back("Aproximação: "+text);
    // A receita guarda até 64 notas; o resto vira uma contagem.
    if(recipe.transformations.size()>64) {
      const auto extra=recipe.transformations.size()-63;recipe.transformations.resize(63);
      recipe.transformations.push_back("E mais "+std::to_string(extra)+" notas de conversão neste prefab.");
    }
    for(auto &text:recipe.transformations)if(text.size()>4000)text.resize(4000);
    const auto previousContent=existing?prefabContent(oldBaseline,error):std::string();
    const auto localContent=existing?prefabContent(before.text,error):std::string();
    const auto incomingContent=existing?prefabContent(native,error):std::string();
    if(existing&&(previousContent.empty()||localContent.empty()||incomingContent.empty()))return {};
    if(existing&&localContent!=previousContent&&incomingContent!=previousContent&&incomingContent!=localContent) {
      if(policy==UnityReimportPolicy::RefuseConflict)return fail("Prefab possui alterações locais e nova origem; escolha Manter local ou Receber origem. Instâncias da cena conservam seus overrides.");
      if(policy==UnityReimportPolicy::KeepLocal){native=before.text;record.dependencies=before.record->dependencies;recipe.transformations.push_back("Prefab autoral local preservado; origem nova registrada como baseline, sem sobrescrever instâncias.");}
    } else if(existing&&localContent!=previousContent)native=before.text;
  } else {
    UnityControllerImport imported;
    const UnityControllerClipResolver resolver=[&](const UnityObjectReference &reference,UnityControllerClip &clip,std::string &diagnostic) {
      Dependency result;if(!resolve(session,path,reference,AssetType::AnimationClip,result,diagnostic))return false;
      if(result.identities.size()!=1)return false;
      clip={result.identities.front(),result.loop};dependency(result.owner);return true;
    };
    if(!convertUnityController(document,record.guid,resolver,imported,error))return {};
    AnimatorControllerAsset previous,current;
    if(existing&&(!AnimatorControllerAsset::deserialize(oldBaseline,previous)||!AnimatorControllerAsset::deserialize(before.text,current)||
        !rebaseUnityController(imported,previous,current,oldRecipe.identities,error)))return {};
    for(const auto &object:document.objects)if(object.classId==91)recipe.fileId=object.fileId;
    recipe.identities=imported.identities;recipe.nextIdentity=imported.controller.graph.nextId;recipe.transformations=imported.transformations;
    sourceBaseline=imported.controller.serialize();native=sourceBaseline;
    const auto previousContent=existing?controllerContent(previous):std::string();
    const auto localContent=existing?controllerContent(current):std::string();
    const auto incomingContent=existing?controllerContent(imported.controller):std::string();
    if(existing&&localContent!=previousContent&&incomingContent!=previousContent&&incomingContent!=localContent) {
      if(policy==UnityReimportPolicy::RefuseConflict)return fail("Controller possui alterações locais e nova origem; escolha Manter local ou Receber origem.");
      if(policy==UnityReimportPolicy::KeepLocal){native=before.text;record.dependencies=existing->dependencies;recipe.transformations.push_back("Grafo autoral local preservado integralmente.");}
    } else if(existing&&localContent!=previousContent)native=before.text;
  }
  if(!baseline(root,sourceBaseline,recipe.baselineHash,error))return fail("Não foi possível preservar o baseline nativo imutável da conversão.");
  record.importerParameters=recipe.serialize();record.contentHash=hash(native);
  if(record.importerParameters.empty()||!record.valid())return fail("Receita de conversão Unity excedeu limites ou contém referências inválidas.");
  std::string currentSource,currentMeta;
  if(!read(root,path,currentSource)||hash(currentSource)!=recipe.sourceHash||!read(root,path+".meta",currentMeta,1u<<20)||hash(currentMeta)!=recipe.metaHash)
    return fail("Fonte ou .meta mudou durante a conversão; nada publicado.");
  after.record=record;after.text=std::move(native);
  const u64 retained=retainedBytes(before)+retainedBytes(after);
  if(retained>EditorHistory::kMaximumRetainedResourceBytes)return fail("Conversão excede orçamento de histórico; reduza o recurso antes de publicar.");
  if(!publish(session,before,after,error))return {};
  auto old=std::make_shared<Snapshot>(std::move(before)),next=std::make_shared<Snapshot>(std::move(after));const auto project=session.files_.rootPath();
  session.history_.recordResource(existing?"Reimportar Unity":"Converter Unity",[&session,project,old,next](bool forward) {
    if(session.files_.rootPath()!=project)return false;
    std::string error;
    const bool ok=publish(session,forward?*old:*next,forward?*next:*old,error);session.state_.status=ok?(forward?"Conversão Unity refeita":"Conversão Unity desfeita; fonte preservada"):error;return ok;
  },retained);
  error="Conversão Unity publicada: "+record.path+" · fonte, receita, baseline e dependências preservados";
  session.state_.status=error;return record.guid;
}

resources::AssetGuid EditorSession::importUnityAsset(const std::string &path,EditorEntityId owner,
    resources::UnityReimportPolicy policy,std::string &diagnostic) {
  return UnityPackageImporter::import(*this,path,owner,policy,diagnostic);
}
} // namespace ae::editor
