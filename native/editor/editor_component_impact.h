#pragma once
#include "editor/editor_document.h"
#include "editor/editor_map_scene.h"
#include "scene/component_schema.h"
#include "runtime/joint_requirements.h"
#include "runtime/physics_requirements.h"
namespace ae::editor {
struct ComponentImpactEntry {
  EditorEntityId object=0;u64 instance=0;
  std::string relation,detail;
  bool blocksRemoval=false;
  bool invalid=false;
  resources::AssetGuid asset{};
};
inline std::vector<ComponentImpactEntry> physicsComponentImpact(const EditorDocument &document,
    EditorEntityId id,const scene::ComponentValue &value) {
  std::vector<ComponentImpactEntry> rows;
  if(!document.activeInHierarchy(id)) return rows;
  const auto issue=[&](EditorEntityId target,u64 instance,const char *message) {
    rows.push_back({target,instance,"Condição de execução",message,false,true});
  };
  const auto inspectShape=[&](EditorEntityId source,const scene::Collider &collider,EditorEntityId owner) {
    float world[16],frame[16];runtime::Transform bodyPose,shapePose;
    if(const auto *error=runtime::bodyFrameForPhysics(document,owner,world,bodyPose,frame))
      issue(owner,0,error);
    else if(const auto *error=runtime::colliderPoseForPhysics(document,source,collider,frame,shapePose))
      issue(source,collider.instanceId(),error);
  };
  if(&value.type()==&scene::Collider::descriptor) {
    const auto &collider=static_cast<const scene::Collider&>(value);if(!collider.enabled) return rows;
    runtime::ObjectId owner=0;
    if(const auto *error=runtime::colliderOwnerForPhysics(document,id,collider,owner)) issue(id,value.instanceId(),error);
    else {
      const auto *body=runtime::physicsBody(*document.find(owner));
      rows.push_back({owner,body->instanceId(),"Proprietário físico",document.find(owner)->name});
      inspectShape(id,collider,owner);
    }
  } else if(&value.type()==&scene::PhysicsBody::descriptor) {
    if(const auto *error=runtime::bodyHierarchyForPhysics(document,id)) issue(id,value.instanceId(),error);
    float world[16],frame[16];runtime::Transform pose;
    if(const auto *error=runtime::bodyFrameForPhysics(document,id,world,pose,frame)) issue(id,value.instanceId(),error);
    std::vector<EditorEntityId> ids;document.collectSubtree(id,ids);u32 count=0;
    for(const auto source:ids) {
      if(!document.activeInHierarchy(source)) continue;
      const auto *object=document.find(source);
      for(usize i=0;i<object->components.size();++i) {
        const auto *component=object->components.at(i);if(&component->type()!=&scene::Collider::descriptor) continue;
        const auto &collider=static_cast<const scene::Collider&>(*component);
        if(!collider.enabled || (collider.owner?collider.owner:source)!=id) continue;
        runtime::ObjectId owner=0;
        if(const auto *error=runtime::colliderOwnerForPhysics(document,source,collider,owner)) {issue(source,collider.instanceId(),error);continue;}
        ++count;rows.push_back({source,collider.instanceId(),"Forma vinculada",object->name});
        inspectShape(source,collider,id);
      }
    }
    if(!count) issue(id,value.instanceId(),"Corpo sem colisores ativos vinculados");
    if(count>256) issue(id,value.instanceId(),"Limite de 256 colisores por corpo excedido");
  }
  return rows;
}
// Authored bindings plus shared-material inheritance when a library is supplied.
// Package-local numeric indices are never treated as GUIDs; source-package
// textures without persistent bindings remain outside this query.
enum class ComponentResourceKind { Mesh, Material, Texture };
struct ComponentResourceUse {
  resources::AssetGuid asset;ComponentResourceKind kind;u32 slot;std::string binding;
};
inline std::vector<ComponentResourceUse> componentResources(const scene::ComponentValue &value, const EditorMapScene *library=nullptr) {
  std::vector<ComponentResourceUse> result;
  if(&value.type()!=&scene::MeshRenderer::descriptor) return result;
  const auto &mesh=static_cast<const scene::MeshRenderer&>(value);
  const auto add=[&](resources::AssetGuid asset,ComponentResourceKind kind,u32 slot,std::string binding) {
    if(asset.valid() && asset!=scene::MaterialTextureNone) result.push_back({asset,kind,slot,std::move(binding)});
  };
  for(u32 slot=0;slot<mesh.slotCount();++slot) {
    add(mesh.slotAsset(slot),ComponentResourceKind::Mesh,slot,"Malha");
    add(mesh.slotMaterialAsset(slot),ComponentResourceKind::Material,slot,"Material");
    constexpr const char *names[]={"Cor base","Normal","Metal / rugosidade","Emissão"};
    for(u32 binding=0;binding<scene::MaterialTextureCount;++binding) {
      const auto local=mesh.slotTextures(slot)[binding];
      const auto effective=library?library->slotTexture(mesh,slot,binding):local;
      add(effective,ComponentResourceKind::Texture,slot,std::string(names[binding])+(!local.valid()&&effective.valid()?" · herdada":""));
    }
    const auto local=mesh.slotOcclusionTexture(slot);
    const auto effective=library?library->slotOcclusionTexture(mesh,slot):local;
    add(effective,ComponentResourceKind::Texture,slot,std::string("Oclusão")+(!local.valid()&&effective.valid()?" · herdada":""));
  }
  return result;
}
inline std::vector<ComponentImpactEntry> componentImpact(const EditorDocument &document,EditorEntityId id,u64 instance,
    const resources::AssetRegistry *registry=nullptr,const EditorMapScene *library=nullptr) {
  std::vector<ComponentImpactEntry> result;
  const auto typeName=[](std::string_view type) {
    const auto *registered=scene::findComponentSchema(type);
    return registered?std::string(registered->name):std::string(type);
  };
  const auto *object=document.find(id);const auto *value=object?object->components.findInstance(instance):nullptr;
  if(!value) return result;
  bool lastOfType=true;
  for(usize i=0;i<object->components.size();++i) {
    const auto *other=object->components.at(i);
    if(other->instanceId()!=instance&&other->type().id==value->type().id) lastOfType=false;
  }
  const auto *schema=scene::findComponentSchema(value->type());
  if(schema) for(const auto &rule:schema->requirements) {
    const auto *target=object->components.find(rule.typeId);
    result.push_back({id,target?target->instanceId():0,target?"Requer":"Requisito ausente",typeName(rule.typeId),false,!target});
  }
  for(const auto &property:value->type().references) if(property.read) {
    const auto target=property.read(*value);const auto *destination=target<=std::numeric_limits<EditorEntityId>::max()?document.find(static_cast<EditorEntityId>(target)):nullptr;
    const bool valid=destination && runtime::referenceAccepts(document,id,property,static_cast<EditorEntityId>(target));
    u64 targetInstance=0;u32 matches=0;
    if(valid && !property.requiredType.empty()) for(usize i=0;i<destination->components.size();++i) {
      const auto *candidate=destination->components.at(i);
      if(candidate->type().id==property.requiredType) {targetInstance=candidate->instanceId();++matches;}
    }
    if(matches!=1) targetInstance=0;
    auto detail=std::string(property.name);
    const auto readiness=runtime::referenceReadiness(document,id,*value,property);
    const bool required=property.isRequiredForExecution(*value);
    if(required) detail+=" · obrigatória para executar";
    if(!property.requiredType.empty()) detail+=" · "+typeName(property.requiredType);
    if(matches>1) detail+=" · várias instâncias";
    const char *relation=target?(valid?"Referencia":"Referência inválida"):"Referência vazia";
    if(readiness==runtime::ReferenceReadiness::RequiredEmpty) relation="Requisito de execução ausente";
    if(readiness==runtime::ReferenceReadiness::Inactive) relation="Destino inativo";
    result.push_back({destination?destination->id:0,targetInstance,relation,detail,false,
      readiness!=runtime::ReferenceReadiness::Ready && readiness!=runtime::ReferenceReadiness::OptionalEmpty});
  }
  const auto resources=componentResources(*value,library);
  const auto physics=physicsComponentImpact(document,id,*value);
  result.insert(result.end(),physics.begin(),physics.end());
  if(&value->type()==&scene::Joint::descriptor)
    for(const auto &issue:runtime::jointRequirementIssues(document,id,static_cast<const scene::Joint&>(*value))) {
      // Reference and composition rows already carry these two diagnostics.
      if(issue.code=="joint.target" || issue.code=="joint.source_body") continue;
      result.push_back({issue.object,issue.instance,"Condição de execução",issue.message,false,true});
    }
  for(const auto &use:resources) {
    const auto *record=registry?registry->find(use.asset):nullptr;
    auto relation=use.binding+" · slot "+std::to_string(use.slot+1);
    bool invalid=false;
    const auto expected=use.kind==ComponentResourceKind::Mesh?resources::AssetType::Mesh:
        use.kind==ComponentResourceKind::Material?resources::AssetType::Material:resources::AssetType::Texture;
    if(record && record->type!=expected) {relation+=" · tipo incompatível";invalid=true;}
    else if(registry && !record) {
      if(use.kind==ComponentResourceKind::Mesh && library && library->assetSlot(use.asset)) relation+=" · pacote";
      else {relation+=" · não registrado";invalid=true;}
    } else if(!registry) relation+=" · registro não consultado";
    if(use.kind==ComponentResourceKind::Material && library && !library->sharedMaterial(use.asset)) {
      relation+=" · não carregado";invalid=true;
    }
    result.push_back({0,0,relation,record?record->path:use.asset.text(),false,invalid,use.asset});
  }
  std::vector<EditorEntityId> ids;document.collectSubtree(document.root(),ids);
  for(auto sourceId:ids) {
    const auto *source=document.find(sourceId);
    for(usize i=0;i<source->components.size();++i) {
      const auto *other=source->components.at(i);
      if(sourceId==id&&other->instanceId()==instance) continue;
      if(!resources.empty()) for(const auto &use:componentResources(*other,library)) {
        bool shared=false;
        for(const auto &own:resources) if(own.kind==use.kind&&own.asset==use.asset) {shared=true;break;}
        if(shared) result.push_back({sourceId,other->instanceId(),"Compartilha recurso",use.binding+" · slot "+std::to_string(use.slot+1)+" · "+use.asset.text()});
      }
      if(sourceId==id) if(const auto *otherSchema=scene::findComponentSchema(other->type()))
        for(const auto &rule:otherSchema->requirements) if(rule.typeId==value->type().id)
          result.push_back({sourceId,other->instanceId(),"Requer este tipo",otherSchema->name,lastOfType,false});
      for(const auto &property:other->type().references) if(property.read && property.read(*other)==id && property.requiredType==value->type().id) {
        const bool valid=runtime::referenceAccepts(document,sourceId,property,id);
        result.push_back({sourceId,other->instanceId(),valid?"Referenciado por":"Referência recebida inválida",typeName(other->type().id)+" · "+property.name,lastOfType&&valid,!valid});
      }
    }
  }
  return result;
}
// Registry edges and scene consumers are deliberately separate: neither proves
// file existence or GPU residency. Querying never changes the authored scene.
inline std::vector<ComponentImpactEntry> resourceImpact(const EditorDocument &document,resources::AssetGuid guid,
    const resources::AssetRegistry *registry,const EditorMapScene *library) {
  std::vector<ComponentImpactEntry> rows;
  const auto *record=registry?registry->find(guid):nullptr;
  rows.push_back({0,0,"Identidade",guid.text()});
  rows.push_back({0,0,"Registro",record?resources::assetTypeName(record->type):registry?"Não registrado":"Não consultado",false,registry&&!record});
  if(record) {
    rows.push_back({0,0,"Caminho",record->path});
    if(!record->source.empty()) rows.push_back({0,0,"Fonte",record->source});
    rows.push_back({0,0,"Importador",std::to_string(record->importerVersion)});
    const auto edge=[&](resources::AssetGuid target,const char *relation) {
      const auto *resolved=registry->find(target);
      rows.push_back({0,0,relation,resolved?resolved->path:target.text(),false,!resolved,target});
    };
    for(const auto &dependency:record->dependencies) edge(dependency,"Depende de · registro");
    for(const auto &dependent:registry->dependents(guid)) edge(dependent,"Usado por · registro");
  }
  if(library && record && record->type==resources::AssetType::Material)
    rows.push_back({0,0,"Biblioteca de materiais",library->sharedMaterial(guid)?"Carregado":"Não carregado",false,!library->sharedMaterial(guid)});
  if(const auto *shared=library?library->sharedMaterial(guid):nullptr) {
    const auto textureRow=[&](resources::AssetGuid texture,const std::string &binding) {
      if(!texture.valid()||texture==scene::MaterialTextureNone) return;
      const auto *entry=registry?registry->find(texture):nullptr;
      rows.push_back({0,0,binding,entry?entry->path:texture.text(),false,!entry,texture});
    };
    for(u32 binding=0;binding<scene::MaterialTextureCount;++binding) textureRow(shared->textures[binding],"Textura do material · "+std::to_string(binding+1));
    textureRow(shared->occlusionTexture,"Oclusão do material");
  }
  if(library && ((record&&record->type==resources::AssetType::Mesh)||library->assetSlot(guid)))
    rows.push_back({0,0,"Biblioteca de malhas",library->assetSlot(guid)?"Presente no pacote":"Não carregado",false,!library->assetSlot(guid)});
  std::vector<EditorEntityId> ids;document.collectSubtree(document.root(),ids);
  for(auto id:ids) {
    const auto *object=document.find(id);
    for(usize index=0;index<object->components.size();++index) {
      const auto *component=object->components.at(index);
      for(const auto &use:componentResources(*component,library)) if(use.asset==guid)
        rows.push_back({id,component->instanceId(),"Usado por · cena",use.binding+" · slot "+std::to_string(use.slot+1)});
    }
  }
  return rows;
}
inline std::vector<ComponentResourceUse> localResourceUses(const scene::ComponentValue *component,resources::AssetGuid asset) {
  std::vector<ComponentResourceUse> result;
  if(component) for(const auto &use:componentResources(*component)) if(use.asset==asset) result.push_back(use);
  return result;
}
inline std::vector<ComponentImpactEntry> resourceRepairChoices(const scene::ComponentValue *component,resources::AssetGuid from,
    const resources::AssetRegistry *registry,const EditorMapScene *library) {
  std::vector<ComponentImpactEntry> rows;
  const auto uses=localResourceUses(component,from);
  if(uses.empty()||!registry||!library) return rows;
  const auto kind=uses.front().kind;
  for(const auto &use:uses) if(use.kind!=kind) return rows; // ambiguous/corrupt binding: never guess a type
  const auto type=kind==ComponentResourceKind::Mesh?resources::AssetType::Mesh:
      kind==ComponentResourceKind::Material?resources::AssetType::Material:resources::AssetType::Texture;
  for(const auto &record:registry->records()) {
    if(record.type!=type||record.guid==from) continue;
    if(type==resources::AssetType::Mesh&&!library->assetSlot(record.guid)) continue;
    if(type==resources::AssetType::Material&&!library->sharedMaterial(record.guid)) continue;
    rows.push_back({0,0,"Substituir por",record.path,false,false,record.guid});
  }
  if(type==resources::AssetType::Mesh) for(u32 index=0;index<library->assetCount();++index) {
    const auto guid=library->assetGuid(index);
    if(!guid.valid()||guid==from||registry->find(guid)) continue;
    rows.push_back({0,0,"Malha do pacote", "Malha "+std::to_string(index+1),false,false,guid});
  }
  return rows;
}
// Caller validates target availability/type and scene revision before committing.
// Preserve every other authored field, including local material/sampler overrides.
inline u32 replaceLocalResource(scene::MeshRenderer &mesh,resources::AssetGuid from,resources::AssetGuid to,
    ComponentResourceKind kind,const EditorMapScene &library) {
  u32 count=0;
  const auto replace=[&](resources::AssetGuid *value) {if(*value!=from)return false;*value=to;++count;return true;};
  for(u32 slot=0;slot<mesh.slotCount();++slot) {
    if(kind==ComponentResourceKind::Mesh) {
      if(replace(mesh.editSlotAsset(slot))) *mesh.editSlotMesh(slot)=library.assetSlot(to);
    } else if(kind==ComponentResourceKind::Material) replace(mesh.editSlotMaterialAsset(slot));
    else {
      for(auto &texture:*mesh.editSlotTextures(slot)) replace(&texture);
      replace(mesh.editSlotOcclusionTexture(slot));
    }
  }
  return count;
}
inline scene::MeshRenderer sharedTextureBindings(resources::AssetGuid material,const EditorMapScene *library) {
  scene::MeshRenderer result;
  if(const auto *shared=library?library->sharedMaterial(material):nullptr) {
    result.textures=shared->textures;result.occlusionTexture=shared->occlusionTexture;
  }
  return result;
}
inline resources::AssetGuid repairMaterialContext(const std::vector<std::pair<resources::AssetGuid,u32>> &trail,
    resources::AssetGuid texture,const EditorMapScene *library) {
  if(trail.empty()) return {};
  const auto material=trail.back().first;
  const auto bindings=sharedTextureBindings(material,library);
  return localResourceUses(&bindings,texture).empty()?resources::AssetGuid{}:material;
}
inline std::vector<ComponentImpactEntry> sharedTextureImpact(const EditorDocument &document,resources::AssetGuid material,
    resources::AssetGuid texture,const EditorMapScene *library) {
  std::vector<ComponentImpactEntry> rows;
  const auto bindings=sharedTextureBindings(material,library);
  std::vector<EditorEntityId> ids;document.collectSubtree(document.root(),ids);
  for(auto id:ids) {
    const auto *object=document.find(id);
    for(usize i=0;i<object->components.size();++i) {
      const auto *component=object->components.at(i);
      if(&component->type()!=&scene::MeshRenderer::descriptor) continue;
      const auto &mesh=static_cast<const scene::MeshRenderer&>(*component);
      for(u32 slot=0;slot<mesh.slotCount();++slot) if(mesh.slotMaterialAsset(slot)==material)
        for(u32 binding=0;binding<=scene::MaterialOcclusionTextureBinding;++binding) {
          const bool occlusion=binding==scene::MaterialOcclusionTextureBinding;
          if((occlusion?bindings.occlusionTexture:bindings.textures[binding])!=texture) continue;
          const auto local=occlusion?mesh.slotOcclusionTexture(slot):mesh.slotTextures(slot)[binding];
          rows.push_back({id,component->instanceId(),local.valid()?"Override preservado":"Consumidor afetado",
              "Slot "+std::to_string(slot+1)+" · "+(occlusion?std::string("Oclusão"):"Textura "+std::to_string(binding+1))});
        }
    }
  }
  return rows;
}
inline std::vector<ComponentImpactEntry> sceneResourceRepairImpact(const EditorDocument &document,resources::AssetGuid resource) {
  std::vector<ComponentImpactEntry> rows;std::vector<EditorEntityId> ids;document.collectSubtree(document.root(),ids);
  for(auto id:ids) {
    const auto *object=document.find(id);
    for(usize index=0;index<object->components.size();++index) {
      const auto *component=object->components.at(index);
      for(const auto &use:localResourceUses(component,resource))
        rows.push_back({id,component->instanceId(),"Uso local afetado",use.binding+" · slot "+std::to_string(use.slot+1)});
    }
  }
  return rows;
}
}
