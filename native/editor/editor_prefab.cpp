#include "editor/editor_session.h"
#include "editor/editor_archive.h"
#include "editor/editor_import_transaction.h"
#include "runtime/prefab.h"
#include "runtime/scene_components.h"
#include "scene/prefab_link.h"
#include "scene/script_behavior.h"
#include <algorithm>
#include <chrono>

namespace ae::editor {
bool EditorSession::unpackPrefab(EditorEntityId selected,std::string &error) {
  error.clear();
  if(isPlaying() || history_.isOpen()) {error="Finalize a edição antes de desvincular";return false;}
  const auto *object=document_.find(selected);
  const auto *link=object?scene::prefabLink(object->components):nullptr;
  if(!link) {error="Selecione uma instância de prefab";return false;}
  const auto owner=link->instanceRoot;
  const auto asset=link->asset;
  // Inspect the graph, not the source file: unpack is also the recovery path
  // for a missing source. Preserve every object/component identity and value.
  std::vector<EditorEntityId> ids;document_.collectSubtree(document_.root(),ids);
  if(!history_.begin("Desvincular prefab")) {error="Histórico indisponível";return false;}
  for(const auto id:ids) {
    auto value=*document_.find(id);
    const auto *member=scene::prefabLink(value.components);
    if(!member || member->instanceRoot!=owner || member->asset!=asset) continue;
    value.components.remove(scene::PrefabLink::descriptor);
    if(!history_.applyValues(document_,id,value)) {
      history_.cancel(document_);error="Não foi possível desvincular; a cena foi preservada";return false;
    }
  }
  history_.end();state_.status="Instância desvinculada; objetos e referências preservados";return true;
}

resources::AssetGuid EditorSession::createPrefab(EditorEntityId root,std::string &error) {
  error.clear();
  if(isPlaying() || history_.isOpen() || !files_.ready()) {error="Finalize a edição e abra um projeto para criar o prefab";return {};}
  const auto *object=document_.find(root);
  if(!object || root==document_.root()) {error="Selecione um objeto da cena";return {};}
  std::string stem=object->name;
  for(char &c:stem) if(static_cast<unsigned char>(c)<32 || c=='/' || c=='\\' || c==':' || c=='"' || c=='*' || c=='?' || c=='<' || c=='>' || c=='|') c='_';
  if(stem.empty() || stem=="." || stem=="..") stem="Prefab";
  std::string path="Prefabs/"+stem+".prefab";
  for(u32 suffix=2;assets_.findByPath(path) || files_.exists(path);++suffix) path="Prefabs/"+stem+" "+std::to_string(suffix)+".prefab";
  const auto guid=resources::assetGuidFromSeed("prefab:"+path+":"+
    std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+":"+std::to_string(++importInstanceCounter_));
  runtime::Prefab prefab;
  if(!prefab.capture(document_,root,guid,error)) return {};
  const auto text=prefab.write();
  resources::AssetRecord record;record.guid=guid;record.type=resources::AssetType::Prefab;record.path=path;
  record.contentHash=Sha256::hex({reinterpret_cast<const u8*>(text.data()),text.size()});
  std::vector<EditorEntityId> ids;prefab.graph().collectSubtree(prefab.root(),ids);
  for(const auto id:ids) {
    const auto &source=*prefab.graph().find(id);
    for(usize i=0;i<source.components.size();++i) {
      const auto *component=source.components.at(i);
      for(const auto &binding:component->type().resourceBindings) for(u32 slot=0;slot<binding.slotCount(*component);++slot) {
        const auto dependency=binding.at(*component,slot);
        if(!dependency.valid() || binding.declaresNone(dependency)) continue;
        auto owner=dependency;
        if(!assets_.find(owner) && binding.kind==resources::AssetType::Mesh) {
          owner={};
          if(!mapScene_.assetSlot(dependency)) {error="Malha ausente no objeto "+std::string(source.name);return {};}
          for(const auto &imported:importedSources_)
            if(std::find(imported.identities.begin(),imported.identities.end(),dependency)!=imported.identities.end()) {owner=imported.guid;break;}
          // A primitive's lifetime belongs to the engine library, not a deletable project file.
        } else if(!assets_.find(owner) && binding.kind==resources::AssetType::AnimationClip) {
          runtime::AnimationClipView view;
          if(!mapScene_.findClip(dependency,view) || !view.source) {error="Clipe ausente no prefab";return {};}
          owner=view.source->source;
        }
        if(owner.valid()) {
          if(!assets_.find(owner)) {error="Recurso ausente no objeto "+std::string(source.name)+": "+owner.text();return {};}
          if(std::find(record.dependencies.begin(),record.dependencies.end(),owner)==record.dependencies.end()) record.dependencies.push_back(owner);
        }
      }
      if(const auto *script=scene::scriptBehavior(component)) {
        if(const auto *dependency=assets_.findByPath(script->source);dependency && dependency->type==resources::AssetType::Script &&
           std::find(record.dependencies.begin(),record.dependencies.end(),dependency->guid)==record.dependencies.end()) record.dependencies.push_back(dependency->guid);
      }
    }
  }
  auto nextAssets=assets_;
  if(!nextAssets.add(record)) {error="O registro recusou as dependências do prefab";return {};}
  // Prepare the scene and its history before publishing any file. The source
  // asset remains reusable when the user undoes the conversion of this instance.
  auto prepared=document_;auto nextHistory=history_;
  if(!nextHistory.begin("Criar vínculo de prefab")) {error="Histórico indisponível";return {};}
  for(const auto id:ids) {
    auto value=*prepared.find(id);
    auto *link=static_cast<scene::PrefabLink*>(value.components.add(scene::PrefabLink::descriptor));
    if(link) {link->asset=guid;link->sourceObject=id;link->instanceRoot=root;link->base=runtime::serializePrefabObject(*prefab.graph().find(id));}
    if(!link || !nextHistory.applyValues(prepared,id,value)) {error="Não foi possível vincular o objeto "+std::string(value.name);return {};}
  }
  nextHistory.end();
  EditorImportTransaction transaction(files_.rootPath());
  if(!transaction.begin(path,{},error)) return {};
  if(!transaction.commit({reinterpret_cast<const u8*>(text.data()),text.size()},nextAssets.serialize())) {
    error=transaction.rollback()?"Falha na gravação; recurso e registro restaurados":"Recuperação pendente no journal do projeto";return {};
  }
  document_=std::move(prepared);history_=std::move(nextHistory);assets_=std::move(nextAssets);assetRegistryDirty_=true;
  files_.rebuildTree();state_.status="Prefab criado: "+path;
  return guid;
}

bool EditorSession::loadPrefab(resources::AssetGuid asset,runtime::Prefab &prefab,std::string &error,std::string *sourceText) const {
  error.clear();
  const auto *record=assets_.find(asset);
  if(!record || record->type!=resources::AssetType::Prefab) {error="Prefab não registrado no projeto";return false;}
  std::filesystem::path absolute;
  if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(files_.rootPath()),record->path,absolute)) {error="Caminho do prefab inválido";return false;}
  std::vector<u8> bytes;
  if(!EditorImportTransaction::read(absolute,bytes,runtime::Prefab::MaximumBytes)) {error="Fonte de prefab ausente ou grande demais: "+record->path;return false;}
  runtime::Prefab prepared;
  if(!prepared.read(std::string_view(reinterpret_cast<const char*>(bytes.data()),bytes.size()),defaultEditorComponentRegistry(),error)) return false;
  if(prepared.asset()!=asset) {error="A identidade do arquivo não corresponde ao prefab registrado";return false;}
  if(sourceText) sourceText->assign(reinterpret_cast<const char*>(bytes.data()),bytes.size());
  prefab=std::move(prepared);return true;
}

bool EditorSession::preparePrefab(resources::AssetGuid asset,runtime::Prefab &prefab,std::string &error) {
  runtime::Prefab authored;if(!loadPrefab(asset,authored,error)) return false;
  return preparePrefab(authored,prefab,error);
}

bool EditorSession::preparePrefab(const runtime::Prefab &authored,runtime::Prefab &prefab,std::string &error) {
  error.clear();
  EditorDocument resolved;
  std::vector<EditorEntityId> ids;authored.graph().collectSubtree(authored.root(),ids);
  for(const auto id:ids) if(!resolved.restoreEntity(*authored.graph().find(id),std::numeric_limits<u32>::max())) {
    error="Valores do prefab recusados pelo editor";return false;
  }
  if(!resolved.reserveObjectIdsUntil(authored.graph().nextObjectId())) {error="Identidades inválidas no prefab";return false;}
  mapScene_.reconcileAssets(resolved);
  const auto available=runtimeResourceResolver();
  for(const auto id:ids) {
    auto value=*resolved.find(id);
    if(!document_.tags().contains(value.tag)) {error="Tag do prefab ausente no projeto: "+value.tag;return false;}
    for(usize i=0;i<value.components.size();++i) {
      const auto *component=value.components.at(i);auto resolvedComponent=component->clone();
      if(!resolveComponentResources(*resolvedComponent,nullptr,error) ||
         !value.components.replaceInstance(component->instanceId(),*resolvedComponent)) return false;
    }
    if(auto *render=meshRenderer(value)?editMeshRenderer(value):nullptr) for(u32 slot=0;slot<render->slotCount();++slot) {
      if(render->slotAsset(slot).valid() && !render->slotMesh(slot)) {error="Malha do prefab indisponível: "+render->slotAsset(slot).text();return false;}
      if(!available(render->slotMaterialAsset(slot),resources::AssetType::Material,"material",slot,*render)) {
        error="Material ou textura do prefab indisponível no objeto "+std::string(value.name);return false;
      }
    }
    if(!resolved.applyEntityValues(id,value)) {error="Não foi possível resolver os recursos do prefab";return false;}
  }
  mapScene_.hydrateMaterials(resolved);
  return prefab.capture(resolved,authored.root(),authored.asset(),error);
}

EditorEntityId EditorSession::instantiatePrefab(resources::AssetGuid asset,EditorEntityId parent,std::string &error) {
  error.clear();
  if(isPlaying() || history_.isOpen() || !document_.exists(parent)) {error="Destino indisponível para instanciar o prefab";return 0;}
  runtime::Prefab prefab;if(!preparePrefab(asset,prefab,error)) return 0;
  EditorDocument prepared;runtime::ObjectCloneMap sourceMap;
  const auto source=prefab.instantiate(prepared,prepared.root(),sourceMap,error);
  if(!source) return 0;
  mapScene_.reconcileAssets(prepared);
  std::vector<EditorEntityId> ids;prepared.collectSubtree(source,ids);
  for(const auto id:ids) if(const auto *render=meshRenderer(*prepared.find(id)))
    for(u32 slot=0;slot<render->slotCount();++slot) if(render->slotAsset(slot).valid() && !render->slotMesh(slot)) {
      error="Malha do prefab indisponível: "+render->slotAsset(slot).text();return 0;
    }
  if(!history_.begin("Instanciar prefab")) {error="Histórico indisponível";return 0;}
  runtime::ObjectCloneMap mapping;EditorEntityId root=0;
  const auto rollback=[&]() {history_.cancel(document_);error="A instância foi recusada; a cena foi preservada";return EditorEntityId{0};};
  for(const auto id:ids) {
    const auto &value=*prepared.find(id);
    const auto target=history_.createEntity(document_,id==source?parent:mapping.at(value.parent),value.kind,value.name);
    if(!target) return rollback();
    mapping.emplace(id,target);if(id==source) root=target;
  }
  std::vector<runtime::SceneObject> values;
  for(const auto id:ids) values.push_back(*prepared.find(id));
  if(!runtime::remapSubtreeReferences(values,mapping)) return rollback();
  for(const auto &value:values) if(!history_.applyValues(document_,mapping.at(value.id),value)) return rollback();
  history_.end();state_.selection=root;state_.status="Instância de prefab criada";return root;
}
} // namespace ae::editor
