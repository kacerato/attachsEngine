// Material físico do projeto (bloco F, F039). Mesmo modelo transacional do
// Perfil de ambiente: arquivo e registro publicados juntos, cópia nos corpos
// sincronizada e Desfazer/Refazer do recurso pelo histórico.
#include "editor/editor_session.h"

#include "core/sha256.h"
#include "editor/editor_import_transaction.h"
#include "scene/physics_body.h"

#include <chrono>
#include <filesystem>
#include <limits>

namespace ae::editor {
namespace {
const scene::PhysicsBody *physicsBodyOf(const EditorEntity *entity,u64 instance) {
  const auto *component=entity?entity->components.findInstance(instance):nullptr;
  return component&&&component->type()==&scene::PhysicsBody::descriptor?static_cast<const scene::PhysicsBody*>(component):nullptr;
}
// Os valores do recurso são os mesmos campos do corpo; o resto do corpo é dele.
void applyMaterial(scene::PhysicsBody &body,const resources::PhysicsMaterialAsset &material) {
  body.friction=material.friction;body.restitution=material.restitution;
  body.frictionCombine=material.frictionCombine;body.restitutionCombine=material.restitutionCombine;
}
} // namespace

void EditorSession::loadPhysicsMaterials() {
  physicsMaterials_.clear();
  const auto root=files_.rootPath();
  if(root.empty()) return;
  for(const auto &record:assets_.records()) {
    if(record.type!=resources::AssetType::PhysicsMaterial) continue;
    std::filesystem::path absolute;std::vector<u8> bytes;resources::PhysicsMaterialAsset material;
    if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(root),record.path,absolute) ||
       !EditorImportTransaction::read(absolute,bytes,4096) ||
       !resources::PhysicsMaterialAsset::deserialize(std::string_view(reinterpret_cast<const char*>(bytes.data()),bytes.size()),material) ||
       material.guid!=record.guid) {
      reportProblem(EditorConsoleSeverity::Warning,"Material físico ilegível ou ausente: "+record.path+"; os corpos conservam sua cópia local.");
      continue;
    }
    physicsMaterials_.push_back(std::move(material));
  }
}

void EditorSession::synchronizePhysicsMaterial(const resources::PhysicsMaterialAsset &material) {
  std::vector<EditorEntityId> ids;document_.collectSubtree(document_.root(),ids);
  for(const auto id:ids) {
    const auto *entity=document_.find(id);if(!entity) continue;
    auto values=*entity;bool changed=false;
    for(u32 index=0;index<values.components.size();++index) {
      auto *component=values.components.editInstance(values.components.at(index)->instanceId());
      if(!component||&component->type()!=&scene::PhysicsBody::descriptor) continue;
      auto &body=static_cast<scene::PhysicsBody&>(*component);
      if(body.material!=material.guid) continue;
      applyMaterial(body,material);changed=true;
    }
    // A cópia faz parte da transação do recurso; desfazer o recurso repete-a.
    if(changed) document_.applyEntityValues(id,values);
  }
}

resources::AssetGuid EditorSession::createPhysicsMaterial(EditorEntityId id,u64 instance,std::string &diagnostic) {
  diagnostic.clear();const auto *entity=document_.find(id);const auto *body=physicsBodyOf(entity,instance);
  if(isPlaying()||history_.isOpen()||!body) {diagnostic="Corpo físico indisponível.";return {};}
  if(files_.rootPath().empty()) {diagnostic="Materiais físicos precisam de um projeto aberto.";return {};}
  resources::PhysicsMaterialAsset material;material.name=entity->name[0]?entity->name:"Material físico";
  material.friction=body->friction;material.restitution=body->restitution;
  material.frictionCombine=body->frictionCombine;material.restitutionCombine=body->restitutionCombine;
  std::string stem;for(unsigned char c:material.name)
    stem.push_back(c<32||c==127||c=='/'||c=='\\'||c==':'||c=='"'?'_':static_cast<char>(c));
  if(stem.empty()||stem=="."||stem=="..") stem="Material físico";
  std::string path="Física/"+stem+".physmat";
  for(u32 n=2;assets_.findByPath(path)||files_.exists(path);++n) path="Física/"+stem+" "+std::to_string(n)+".physmat";
  material.guid=resources::assetGuidFromSeed("physics_material:"+path+":"+
      std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+":"+std::to_string(++importInstanceCounter_));
  if(!material.valid()) {diagnostic="Valores do material inválidos.";return {};}
  const auto text=material.serialize();resources::AssetRecord record;
  record.guid=material.guid;record.type=resources::AssetType::PhysicsMaterial;record.path=path;
  record.contentHash=Sha256::hex(std::span<const u8>(reinterpret_cast<const u8*>(text.data()),text.size()));
  auto nextAssets=assets_;if(!nextAssets.add(record)) {diagnostic="O registro recusou o material.";return {};}
  auto values=*entity;auto *editable=static_cast<scene::PhysicsBody*>(values.components.editInstance(instance));
  if(!editable) {diagnostic="O corpo mudou durante a criação.";return {};}
  editable->material=material.guid;
  auto stagedDocument=document_;auto stagedHistory=history_;
  if(!stagedHistory.applyValues(stagedDocument,id,values)) {diagnostic="O histórico recusou o vínculo do material.";return {};}
  // Toda a mutação autoral é validada antes de o arquivo existir.
  std::filesystem::path absolute;
  if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(files_.rootPath()),path,absolute)) {diagnostic="Caminho inválido: "+path;return {};}
  std::error_code error;std::filesystem::create_directories(absolute.parent_path(),error);
  if(error||!EditorImportTransaction::writeText(absolute,text)) {diagnostic="Não foi possível gravar "+path+".";return {};}
  document_=std::move(stagedDocument);history_=std::move(stagedHistory);
  assets_=std::move(nextAssets);assetRegistryDirty_=true;physicsMaterials_.push_back(material);files_.rebuildTree();
  diagnostic="Material físico criado: "+path;return material.guid;
}

bool EditorSession::updatePhysicsMaterial(EditorEntityId id,u64 instance,std::string &diagnostic) {
  const auto *body=physicsBodyOf(document_.find(id),instance);
  const auto *current=body?findPhysicsMaterial(body->material):nullptr;
  if(!body||!current) {diagnostic="Escolha ou crie um material antes de atualizá-lo.";return false;}
  auto candidate=*current;candidate.friction=body->friction;candidate.restitution=body->restitution;
  candidate.frictionCombine=body->frictionCombine;candidate.restitutionCombine=body->restitutionCombine;++candidate.revision;
  if(!commitPhysicsMaterial(candidate,diagnostic,true)) return false;
  diagnostic="Material físico compartilhado atualizado: "+candidate.name;return true;
}

bool EditorSession::commitPhysicsMaterial(const resources::PhysicsMaterialAsset &candidate,std::string &diagnostic,bool recordHistory) {
  diagnostic.clear();
  if(isPlaying()||history_.isOpen()) {diagnostic="Finalize a edição antes de alterar o material.";return false;}
  const auto *current=findPhysicsMaterial(candidate.guid);const auto *record=assets_.find(candidate.guid);
  if(!candidate.valid()||!current||!record||record->type!=resources::AssetType::PhysicsMaterial||
     current->revision==std::numeric_limits<u32>::max()||candidate.revision!=current->revision+1) {
    diagnostic="Material ou revisão indisponível.";return false;
  }
  const auto root=EditorImportTransaction::fromUtf8(files_.rootPath());
  std::filesystem::path path;std::vector<u8> bytes;resources::PhysicsMaterialAsset disk;
  if(!EditorImportTransaction::safePath(root,record->path,path)||!EditorImportTransaction::read(path,bytes)||
     !resources::PhysicsMaterialAsset::deserialize(std::string(bytes.begin(),bytes.end()),disk)||disk.serialize()!=current->serialize()) {
    diagnostic="Material mudou no disco: "+record->path;return false;
  }
  auto same=candidate;same.revision=current->revision;if(same.serialize()==current->serialize()) return true;
  const auto text=candidate.serialize();auto next=assets_;
  if(!next.publishImport(candidate.guid,Sha256::hex({reinterpret_cast<const u8*>(text.data()),text.size()}),
      record->importerVersion,record->importerParameters,record->derived,{})) {diagnostic="Registro recusou o material.";return false;}
  std::vector<EditorImportTransaction::TextEdit> edits{{record->path,Sha256::hex(bytes),text}};
  if(!EditorImportTransaction::publishTextBatch(files_.rootPath(),edits,next.serialize(),diagnostic)) return false;
  const auto before=*current;
  assets_=std::move(next);assetRegistryDirty_=true;
  for(auto &material:physicsMaterials_) if(material.guid==candidate.guid) {material=candidate;break;}
  synchronizePhysicsMaterial(candidate);
  if(recordHistory) {
    const auto project=files_.rootPath();const auto after=candidate;
    history_.recordResource("Material físico",[this,before,after,project](bool forward) {
      if(files_.rootPath()!=project) {state_.status="Projeto do histórico indisponível.";return false;}
      const auto *live=findPhysicsMaterial(before.guid);
      if(!live) {state_.status="Material do histórico indisponível.";return false;}
      auto expected=forward?before:after;expected.revision=live->revision;
      if(expected.serialize()!=live->serialize()) {state_.status="Material mudou; histórico preservado.";return false;}
      auto value=forward?after:before;value.revision=live->revision+1;
      std::string error;if(!commitPhysicsMaterial(value,error,false)) {state_.status=error;return false;}
      state_.status=forward?"Material físico refeito":"Material físico desfeito";return true;
    });
  }
  return true;
}

} // namespace ae::editor
