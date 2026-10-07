#include "editor/editor_session.h"
#include "editor/editor_component_catalog.h"
#include "scene/prefab_link.h"
#include "scene/import_link.h"
#include "scene/script_behavior.h"
#include "editor/editor_import_transaction.h"
#include "scene/mesh_renderer.h"
#include "scene/collision_recipe.h"
#include <sstream>
#include <unordered_set>

namespace ae::editor {
namespace {
std::string payload(const scene::ComponentValue &value) {
  std::ostringstream out;out.imbue(std::locale::classic());
  out<<std::setprecision(std::numeric_limits<float>::max_digits10);value.write(out);return out.str();
}
std::string componentLabel(const scene::ComponentValue &value) {
  if(const auto *script=scene::scriptBehavior(&value)) return script->scriptType;
  if(const auto *entry=findEditorComponent(value.type().id)) return entry->name;
  return std::string(value.type().id);
}
bool metadata(const scene::ComponentValue &value) {
  return &value.type()==&scene::PrefabLink::descriptor || &value.type()==&scene::ImportLink::descriptor;
}
std::string collectionSummary(const scene::ComponentValue &value) {
  std::string text;
  for(const auto &collection:value.type().collections) {
    if(!collection.size || !collection.elementId || !collection.nextId)continue;
    if(!text.empty())text+='\n';
    const auto count=collection.size(value);
    text+=std::to_string(count)+(collection.id=="points"?" pontos":collection.id=="clips"?" clipes":" elementos");
    text+='\n';text+=count?"IDs: ":"Sem IDs";
    for(u32 i=0;i<std::min(count,4u);++i) {if(i)text+=" → ";text+=std::to_string(collection.elementId(value,i));}
    if(count>4)text+=" …";
    text+="\nPróximo ID livre: "+std::to_string(collection.nextId(value));
  }
  return text;
}
const scene::ScriptPropertyValue *property(const scene::ScriptBehavior &script,std::string_view id) {
  for(const auto &p:script.properties) if(p.id==id) return &p;
  return nullptr;
}
// One coherent source/base graph per instance, in source IDs until the last
// remap. A single object's baseline cannot resolve references between siblings.
struct PrefabComparison {
  runtime::Prefab source,baseline;
  runtime::ObjectCloneMap mapping;
  resources::AssetGuid asset;
  std::string hash;
  EditorEntityId instanceRoot=0;
};
bool comparison(EditorSession &session,EditorEntityId selected,PrefabComparison &result,std::string &error) {
  const auto &document=session.document();const auto *current=document.find(selected);
  const auto *link=current?scene::prefabLink(current->components):nullptr;
  if(!link) {error="Selecione um objeto vinculado a um prefab";return false;}
  PrefabComparison next;next.asset=link->asset;next.instanceRoot=link->instanceRoot;
  runtime::Prefab authored;std::string bytes;
  if(!session.loadPrefab(next.asset,authored,error,&bytes)) return false;
  next.hash=Sha256::hex({reinterpret_cast<const u8*>(bytes.data()),bytes.size()});
  std::vector<EditorEntityId> sceneIds;document.collectSubtree(document.root(),sceneIds);
  std::unordered_map<EditorEntityId,EditorEntity> bases;
  for(const auto id:sceneIds) if(const auto *member=scene::prefabLink(document.find(id)->components);
      member && member->asset==next.asset && member->instanceRoot==next.instanceRoot) {
    if(member->sourceObject>runtime::SceneGraph::kMaximumObjects ||
       !next.mapping.emplace(member->sourceObject,id).second) {
      error="Identidade de origem inválida ou duplicada na instância";return false;
    }
    EditorEntity base;std::istringstream input(member->base);input.imbue(std::locale::classic());
    if(!runtime::deserializePrefabObject(input,defaultEditorComponentRegistry(),base) || base.id!=member->sourceObject) {
      error="Base da instância inválida; nenhum valor foi alterado";return false;
    }
    input>>std::ws;if(!input.eof()) {error="Dados adicionais na base da instância";return false;}
    bases.emplace(base.id,std::move(base));
  }
  const auto root=next.mapping.find(authored.root());
  if(root==next.mapping.end() || root->second!=next.instanceRoot) {
    error="Raiz de origem ausente ou incompatível na instância";return false;
  }
  std::vector<EditorEntityId> sourceIds;authored.graph().collectSubtree(authored.root(),sourceIds);
  if(sourceIds.size()!=bases.size()) {error="A hierarquia da fonte mudou; reconciliação estrutural pendente";return false;}
  EditorDocument baselineGraph;
  for(const auto id:sourceIds) {
    const auto found=bases.find(id);const auto &source=*authored.graph().find(id);
    if(found==bases.end() || found->second.parent!=source.parent || found->second.kind!=source.kind) {
      error="A identidade ou hierarquia da fonte mudou; reconciliação estrutural pendente";return false;
    }
    const auto &base=found->second;
    for(usize i=0;i<base.components.size();++i) {
      const auto *a=base.components.at(i),*b=source.components.findInstance(a->instanceId());
      if(b && &a->type()!=&b->type()) {error="A identidade dos componentes da fonte mudou";return false;}
    }
    if(!baselineGraph.restoreEntity(base,std::numeric_limits<u32>::max())) {
      error="Não foi possível reconstruir a base da instância";return false;
    }
  }
  if(!baselineGraph.reserveObjectIdsUntil(authored.graph().nextObjectId())) {error="Identidades inválidas na base";return false;}
  runtime::Prefab baseline;
  if(!baseline.capture(baselineGraph,authored.root(),next.asset,error) ||
     !session.preparePrefab(baseline,next.baseline,error) || !session.preparePrefab(authored,next.source,error)) return false;
  result=std::move(next);return true;
}
bool comparisonObjects(EditorSession &session,const PrefabComparison &comparison,EditorEntityId selected,
                       EditorEntity &current,EditorEntity &baseline,EditorEntity &source,std::string &error) {
  const auto *live=session.document().find(selected);
  const auto *link=live?scene::prefabLink(live->components):nullptr;
  if(!link || link->asset!=comparison.asset || link->instanceRoot!=comparison.instanceRoot) {
    error="O vínculo da instância mudou";return false;
  }
  const auto *b=comparison.baseline.graph().find(link->sourceObject),*s=comparison.source.graph().find(link->sourceObject);
  if(!b || !s) {error="Objeto ausente na base ou fonte";return false;}
  baseline=*b;source=*s;
  // capture() validated that every source/base reference belongs to these
  // complete graphs, and mapping is bijective over exactly their objects.
  if(!runtime::remapObjectReferences(baseline,comparison.mapping) || !runtime::remapObjectReferences(source,comparison.mapping)) {
    error="Referências da instância indisponíveis";return false;
  }
  baseline.id=source.id=selected;baseline.parent=source.parent=live->parent;
  // Normalize effective renderer caches on a copy. Explicit material overrides
  // and authoring GUIDs remain untouched by the existing hydration path.
  EditorDocument normalized;auto value=*live;value.parent=normalized.root();
  if(!normalized.restoreEntity(value,std::numeric_limits<u32>::max())) {error="Objeto inválido na comparação";return false;}
  session.mapScene().reconcileAssets(normalized);session.mapScene().hydrateMaterials(normalized);
  current=*normalized.find(selected);current.parent=live->parent;
  return true;
}

void differences(const EditorEntity &current,const EditorEntity &source,bool root,std::vector<PrefabOverride> &rows,
                 const std::unordered_set<u64> *atomic=nullptr) {
  const auto object=[&](std::string field,std::string label,std::string a,std::string b,u32 slot=0) {
    if(a!=b) rows.push_back({PrefabOverrideKind::Object,0,std::move(field),std::move(label),std::move(a),std::move(b),slot});
  };
  object("name","Nome",current.name,source.name);
  const auto flag=[&](const char *field,const char *label,bool a,bool b) {object(field,label,a?"Sim":"Não",b?"Sim":"Não");};
  flag("active","Ativo",current.active,source.active);flag("visible","Visível",current.visible,source.visible);
  flag("castShadow","Projeta sombras",current.castShadow,source.castShadow);
  flag("receiveShadow","Recebe sombras",current.receiveShadow,source.receiveShadow);
  flag("isStatic","Estático (marcação de autoria)",current.isStatic,source.isStatic);
  flag("rigidBodyEnabled","Física legada / Habilitada",current.rigidBodyEnabled,source.rigidBodyEnabled);
  const char *bodyFields[]{"Massa","Arrasto","Meia extensão X","Meia extensão Y","Meia extensão Z"};
  for(u32 i=0;i<5;++i)
    object("rigidBody",std::string("Física legada / ")+bodyFields[i],scene::detail::formatNumber(current.rigidBody[i]),scene::detail::formatNumber(source.rigidBody[i]),i);
  const char *environmentFields[]{"Sol","Ambiente","Exposição","Rotação do céu"};
  for(u32 i=0;i<4;++i)
    object("environment",std::string("Ambiente legado / ")+environmentFields[i],scene::detail::formatNumber(current.environment[i]),scene::detail::formatNumber(source.environment[i]),i);
  object("tag","Tag",current.tag,source.tag);object("layer","Camada",std::to_string(current.layer),std::to_string(source.layer));
  object("groups","Grupos",current.groups.description(),source.groups.description());
  const char *groups[]{"Posição","Rotação","Escala"};const char *axes[]{"X","Y","Z"};
  const float *a[]{current.transform.position,current.transform.rotationDegrees,current.transform.scale};
  const float *b[]{source.transform.position,source.transform.rotationDegrees,source.transform.scale};
  for(u32 group=root?2:0;group<3;++group) for(u32 axis=0;axis<3;++axis) if(a[group][axis]!=b[group][axis])
    rows.push_back({PrefabOverrideKind::Object,0,"transform",std::string(groups[group])+" "+axes[axis],scene::detail::formatNumber(a[group][axis]),scene::detail::formatNumber(b[group][axis]),group*3+axis});
  for(usize i=0;i<current.components.size();++i) {
    const auto *local=current.components.at(i);if(metadata(*local)) continue;
    const auto *original=source.components.findInstance(local->instanceId());
    const auto label=componentLabel(*local);
    if(!original) {rows.push_back({PrefabOverrideKind::AddedComponent,local->instanceId(),{},label,"Componente adicionado","Ausente na fonte",static_cast<u32>(i)});continue;}
    if(&local->type()!=&original->type()) {
      rows.push_back({PrefabOverrideKind::Component,local->instanceId(),{},label,"Identidade incompatível","Requer reconciliação",0,scene::FieldKind::Number,false});continue;
    }
    if(!scene::sameComponentCollectionStructure(*local,*original)) {
      rows.push_back({PrefabOverrideKind::Component,local->instanceId(),{},label+" (coleção estrutural)",
        collectionSummary(*local),collectionSummary(*original)});
      continue;
    }
    if(payload(*local)==payload(*original)) continue;
    if(atomic && atomic->contains(local->instanceId())) {
      rows.push_back({PrefabOverrideKind::Component,local->instanceId(),{},label+" (completo)",
        local->type().collections.empty()?"Conteúdo local diferente":collectionSummary(*local),
        original->type().collections.empty()?"Restaurar todos os dados do componente":collectionSummary(*original)});continue;
    }
    const auto *script=scene::scriptBehavior(local),*sourceScript=scene::scriptBehavior(original);
    if(script && sourceScript && script->scriptType==sourceScript->scriptType && script->source==sourceScript->source) {
      if(script->enabled!=sourceScript->enabled) rows.push_back({PrefabOverrideKind::Field,local->instanceId(),"enabled",label+" / Habilitado",script->enabled?"Sim":"Não",sourceScript->enabled?"Sim":"Não",0,scene::FieldKind::Boolean});
      for(const auto &p:script->properties) {
        const auto *q=property(*sourceScript,p.id);
        if(!q || p.valueType!=q->valueType || p.value!=q->value)
          rows.push_back({PrefabOverrideKind::ScriptField,local->instanceId(),p.id,label+" / "+p.id,p.value,q?q->value:"Padrão do script"});
      }
      for(const auto &p:sourceScript->properties) if(!property(*script,p.id))
        rows.push_back({PrefabOverrideKind::ScriptField,local->instanceId(),p.id,label+" / "+p.id,"Padrão do script",p.value});
      continue;
    }
    const auto delta=scene::componentDelta(*local,*original);auto candidate=current.components;
    const auto fields=scene::changedFields(delta);
    bool covered=false;
    if(!fields.empty()) {
      const auto applied=scene::applyComponentFields(candidate,*original,fields,local->instanceId());
      covered=applied.ok() && !applied.rejected && payload(*candidate.findInstance(local->instanceId()))==payload(*original);
    }
    if(!covered) {
      // Collections and other serialized data have no per-field reflection.
      // Show the actual atomic scope instead of silently dropping those values.
      rows.push_back({PrefabOverrideKind::Component,local->instanceId(),{},label+" (completo)","Conteúdo local diferente","Restaurar todos os dados do componente"});continue;
    }
    for(const auto &d:delta) if(d.differs)
      rows.push_back({PrefabOverrideKind::Field,local->instanceId(),std::string(d.address.id),label+" / "+d.label,d.current,d.candidate,d.address.slot,d.address.kind,d.applicable});
  }
  for(usize i=0;i<source.components.size();++i) {
    const auto *component=source.components.at(i);
    const auto *present=current.components.findInstance(component->instanceId());
    if(!metadata(*component) && (!present || metadata(*present)))
      rows.push_back({PrefabOverrideKind::RemovedComponent,component->instanceId(),{},componentLabel(*component),"Componente removido","Restaurar componente",static_cast<u32>(i)});
  }
  std::vector<u64> localOrder,sourceOrder;
  for(usize i=0;i<current.components.size();++i) {
    const auto *c=current.components.at(i);
    if(!metadata(*c) && source.components.findInstance(c->instanceId()))localOrder.push_back(c->instanceId());
  }
  for(usize i=0;i<source.components.size();++i) {
    const auto *c=source.components.at(i);
    const auto *present=current.components.findInstance(c->instanceId());
    if(!metadata(*c) && present && !metadata(*present))sourceOrder.push_back(c->instanceId());
  }
  if(localOrder!=sourceOrder) rows.push_back({PrefabOverrideKind::ComponentOrder,0,{},"Ordem dos componentes","Ordem local","Ordem na fonte"});
}

bool inheritCollectionFloors(EditorEntity &candidate,const EditorEntity &previous) {
  for(usize i=0;i<candidate.components.size();++i) {
    const auto *value=candidate.components.at(i);if(value->type().collections.empty())continue;
    const auto *prior=previous.components.findInstance(value->instanceId());
    if(!prior || &prior->type()!=&value->type())continue;
    auto copy=value->clone();
    if(!scene::preserveComponentCollectionIdentityFloor(*copy,*prior) || !candidate.components.replaceInstance(value->instanceId(),*copy))return false;
  }
  return true;
}
// All three comparisons use the same atomic scope for unreflected data. A
// collection change must not be mistaken for independent editable fields.
void threeWayDifferences(const EditorEntity &currentInput,const EditorEntity &baselineInput,const EditorEntity &sourceInput,
                         bool root,std::vector<PrefabOverride> &rows) {
  auto current=currentInput,baseline=baselineInput,source=sourceInput;
  // Allocator floors are retirement bookkeeping, not an author-selected value.
  // Compare coherent copies, then retain the same floor in published candidates.
  if(!inheritCollectionFloors(current,baseline) || !inheritCollectionFloors(current,source) ||
     !inheritCollectionFloors(baseline,current) || !inheritCollectionFloors(source,current)) {
    rows.push_back({PrefabOverrideKind::Object,0,"collections","Identidades da coleção",{},"Contrato de reserva indisponível",0,scene::FieldKind::Number,false});return;
  }
  std::vector<PrefabOverride> local,incoming;std::unordered_set<u64> atomic;
  differences(current,baseline,root,local);differences(baseline,source,root,incoming);differences(current,source,root,rows);
  for(const auto *set:{&local,&incoming,&rows}) for(const auto &row:*set)
    if(row.kind==PrefabOverrideKind::Component || row.kind==PrefabOverrideKind::AddedComponent || row.kind==PrefabOverrideKind::RemovedComponent) atomic.insert(row.component);
  if(!atomic.empty()) {
    local.clear();incoming.clear();rows.clear();
    differences(current,baseline,root,local,&atomic);differences(baseline,source,root,incoming,&atomic);differences(current,source,root,rows,&atomic);
  }
  // Removal is itself the local change: incoming fields of a removed component
  // are not independent inherited changes. Restore brings back the latest source.
  for(auto &row:rows) {
    const auto membership=[&](const PrefabOverride &r) {return r.kind==PrefabOverrideKind::AddedComponent || r.kind==PrefabOverrideKind::RemovedComponent;};
    const auto own=std::find_if(local.begin(),local.end(),[&](const auto &other){
      return other.sameAddress(row) || (row.component && other.component==row.component && (membership(row)||membership(other)));
    });
    const auto external=std::find_if(incoming.begin(),incoming.end(),[&](const auto &other){
      return other.sameAddress(row) || (row.component && other.component==row.component && (membership(row)||membership(other)));
    });
    row.origin=own==local.end()?PrefabOverrideOrigin::Inherited:
        external==incoming.end()?PrefabOverrideOrigin::Local:PrefabOverrideOrigin::Conflict;
    row.baseline=own!=local.end()?own->source:external!=incoming.end()?external->current:std::string();
  }
}

bool componentReferenced(const EditorDocument &document,EditorEntityId object,u64 component,const EditorEntity *replacement=nullptr) {
  std::vector<EditorEntityId> ids;document.collectSubtree(document.root(),ids);
  for(const auto id:ids) {
    const auto *entity=replacement && replacement->id==id?replacement:document.find(id);
    for(usize i=0;i<entity->components.size();++i) {
      const auto *value=entity->components.at(i);
      if(id==object && value->instanceId()==component) continue;
      if(const auto *script=scene::scriptBehavior(value)) for(const auto &p:script->properties) {
        const auto matches=[&](std::string_view type,std::string_view text) {
          u64 owner=0,instance=0;
          return !scene::scriptComponentTypeId(type).empty() && scene::parseScriptComponentValue(text,owner,instance) && owner==object && instance==component;
        };
        if(matches(p.valueType,p.value)) return true;
        const auto element=scene::scriptArrayElementType(p.valueType);std::vector<std::string> items;
        if(!element.empty() && scene::parseScriptArray(p.value,items)) for(const auto &item:items) if(matches(element,item)) return true;
      }
    }
  }
  return false;
}
bool restoredReferencesValid(const runtime::SceneGraph &document,const EditorEntity &before,const EditorEntity &candidate) {
  const auto lookup=[&](u64 id)->const EditorEntity* {
    if(id==candidate.id) return &candidate;
    return id<=std::numeric_limits<EditorEntityId>::max()?document.find(static_cast<EditorEntityId>(id)):nullptr;
  };
  for(usize i=0;i<candidate.components.size();++i) {
    const auto *value=candidate.components.at(i),*old=before.components.findInstance(value->instanceId());
    (void)old; // Unchanged references must also survive removal of their target.
    for(const auto &ref:value->type().references) if(ref.read) {
      const auto target=ref.read(*value);if(target && !lookup(target)) return false;
    }
    if(const auto *script=scene::scriptBehavior(value)) for(const auto &p:script->properties) {
      bool valid=true;scene::forEachScriptPropertyObject(p,[&](u64 owner) {if(owner && !lookup(owner)) valid=false;});
      if(!valid) return false;
      const auto check=[&](std::string_view type,std::string_view text) {
        const auto typeId=scene::scriptComponentTypeId(type);if(typeId.empty()) return true;
        u64 owner=0,instance=0;if(!scene::parseScriptComponentValue(text,owner,instance)) return false;
        if(!owner) return true;
        const auto *object=lookup(owner);const auto *target=object?object->components.findInstance(instance):nullptr;
        return target && target->type().id==typeId;
      };
      if(!check(p.valueType,p.value)) return false;
      const auto element=scene::scriptArrayElementType(p.valueType);std::vector<std::string> items;
      if(!element.empty() && scene::parseScriptArray(p.value,items)) for(const auto &item:items) if(!check(element,item)) return false;
    }
  }
  return true;
}
bool structuralCompositionValid(const EditorEntity &before,const EditorEntity &candidate,std::string &error) {
  bool changed=before.components.size()!=candidate.components.size();
  for(usize i=0;i<before.components.size();++i) if(!candidate.components.findInstance(before.components.at(i)->instanceId()))changed=true;
  if(!changed)return true;
  for(usize i=0;i<candidate.components.size();++i) {
    const auto *value=candidate.components.at(i);const auto *schema=scene::findComponentSchema(value->type());
    if(!schema)continue;
    for(const auto &rule:schema->requirements)if(!candidate.components.find(rule.typeId)){error=rule.message;return false;}
    for(const auto &rule:schema->conflicts)if(candidate.components.find(rule.typeId)){error=rule.message;return false;}
  }
  return true;
}
// Copy membership from a complete object, retaining IDs and existing order.
bool adoptComponent(EditorEntity &candidate,const EditorEntity &source,u64 id) {
  const auto *value=source.components.findInstance(id);
  if(!value)return !candidate.components.findInstance(id) || candidate.components.removeInstance(id);
  // Editor ownership metadata is not a source component. Source additions can
  // reuse its numerical ID; move metadata to a fresh ID instead of losing it.
  if(const auto *existing=candidate.components.findInstance(id);existing && metadata(*existing)) {
    auto copy=existing->clone();usize position=0;u64 fresh=1;
    for(usize i=0;i<candidate.components.size();++i){const auto cid=candidate.components.at(i)->instanceId();fresh=std::max(fresh,cid);if(cid==id)position=i;}
    for(usize i=0;i<source.components.size();++i)fresh=std::max(fresh,source.components.at(i)->instanceId());
    if(fresh>=std::numeric_limits<u64>::max()-1)return false;
    if(!candidate.components.removeInstance(id) || !candidate.components.restoreInstance(*copy,fresh+1,position))return false;
  }
  if(const auto *existing=candidate.components.findInstance(id)) {
    auto copy=value->clone();
    return scene::preserveComponentCollectionIdentityFloor(*copy,*existing) && candidate.components.replaceInstance(id,*copy);
  }
  usize position=0;while(position<source.components.size() && source.components.at(position)->instanceId()!=id)++position;
  return candidate.components.restoreInstance(*value,id,position);
}
bool adoptComponentOrder(EditorEntity &candidate,const EditorEntity &source) {
  // Reorder shared IDs in their current slots. Local additions and metadata
  // keep their slots; membership rows reconcile birth/death separately.
  std::vector<usize> slots;std::vector<u64> order;
  for(usize i=0;i<candidate.components.size();++i) {
    const auto *v=candidate.components.at(i);
    if(!metadata(*v) && source.components.findInstance(v->instanceId()))slots.push_back(i);
  }
  for(usize i=0;i<source.components.size();++i) {
    const auto *v=source.components.at(i);
    const auto *present=candidate.components.findInstance(v->instanceId());
    if(!metadata(*v) && present && !metadata(*present))order.push_back(v->instanceId());
  }
  if(slots.size()!=order.size())return false;
  std::vector<u64> desired;for(usize i=0;i<candidate.components.size();++i)desired.push_back(candidate.components.at(i)->instanceId());
  for(usize i=0;i<slots.size();++i)desired[slots[i]]=order[i];
  for(usize i=0;i<desired.size();++i)if(!candidate.components.moveInstance(desired[i],i))return false;
  return true;
}
bool applyDifference(EditorEntity &candidate,const EditorEntity &source,const PrefabOverride &row,std::string &error) {
  const auto *component=source.components.findInstance(row.component);
  switch(row.kind) {
  case PrefabOverrideKind::Object:
    if(row.field=="name") runtime::assignObjectName(candidate,source.name);
    else if(row.field=="tag") candidate.tag=source.tag;
    else if(row.field=="groups") candidate.groups=source.groups;
    else if(row.field=="layer") candidate.layer=source.layer;
    else if(row.field=="active") candidate.active=source.active;
    else if(row.field=="visible") candidate.visible=source.visible;
    else if(row.field=="castShadow") candidate.castShadow=source.castShadow;
    else if(row.field=="receiveShadow") candidate.receiveShadow=source.receiveShadow;
    else if(row.field=="isStatic") candidate.isStatic=source.isStatic;
    else if(row.field=="rigidBodyEnabled") candidate.rigidBodyEnabled=source.rigidBodyEnabled;
    else if(row.field=="rigidBody" && row.slot<5) candidate.rigidBody[row.slot]=source.rigidBody[row.slot];
    else if(row.field=="environment" && row.slot<4) candidate.environment[row.slot]=source.environment[row.slot];
    else if(row.field=="transform" && row.slot<9) {
      float *a[]{candidate.transform.position,candidate.transform.rotationDegrees,candidate.transform.scale};
      const float *b[]{source.transform.position,source.transform.rotationDegrees,source.transform.scale};
      a[row.slot/3][row.slot%3]=b[row.slot/3][row.slot%3];
    } else return false;
    return true;
  case PrefabOverrideKind::Field: {
    const auto *prior=candidate.components.findInstance(row.component);if(!component || !prior)return false;
    auto portable=component->clone(),previous=prior->clone();
    if(!scene::preserveComponentCollectionIdentityFloor(*portable,*previous) ||
       !scene::preserveComponentCollectionIdentityFloor(*previous,*portable) ||
       !candidate.components.replaceInstance(row.component,*previous))return false;
    const scene::FieldAddress address{row.field,row.slot,row.fieldKind};
    const auto outcome=scene::applyComponentFields(candidate.components,*portable,{&address,1},row.component);
    if(!outcome.ok() || outcome.rejected) {error=outcome.error?outcome.error:"Campo indisponível neste estado";return false;}
    return true;
  }
  case PrefabOverrideKind::ScriptField: {
    auto *value=candidate.components.editInstance(row.component);
    if(!scene::scriptBehavior(value)) return false;
    auto *script=static_cast<scene::ScriptBehavior*>(value);
    const auto *original=scene::scriptBehavior(component);if(!original) return false;
    if(const auto *p=property(*original,row.field)) return script->setProperty(p->id,p->valueType,p->value);
    std::erase_if(script->properties,[&](const auto &p) {return p.id==row.field;});return true;
  }
  case PrefabOverrideKind::Component: {
    const auto *previous=candidate.components.findInstance(row.component);if(!component || !previous)return false;
    auto copy=component->clone();
    return scene::preserveComponentCollectionIdentityFloor(*copy,*previous) && candidate.components.replaceInstance(row.component,*copy);
  }
  case PrefabOverrideKind::AddedComponent: return candidate.components.removeInstance(row.component);
  case PrefabOverrideKind::RemovedComponent: return component && adoptComponent(candidate,source,row.component);
  case PrefabOverrideKind::ComponentOrder: return adoptComponentOrder(candidate,source);
  }
  return false;
}

// Ownership and object topology must remain coherent before file changes.
// Component membership/order are reconciled by the three-way comparison.
bool structurallyApplyable(EditorSession &session,const PrefabComparison &data,std::string &error) {
  const auto &document=session.document();std::vector<EditorEntityId> ids;
  document.collectSubtree(data.instanceRoot,ids);
  if(ids.size()!=data.mapping.size()) {error="Objeto adicionado, nested prefab ou hierarquia local exige reconciliação estrutural";return false;}
  for(const auto id:ids) {
    const auto *live=document.find(id);const auto *link=live?scene::prefabLink(live->components):nullptr;
    if(!link || link->asset!=data.asset || link->instanceRoot!=data.instanceRoot) {
      error="Composição nested ou vínculo local incompatível; Apply foi recusado";return false;
    }
    const auto *source=data.source.graph().find(link->sourceObject);
    if(!source || live->kind!=source->kind || (id!=data.instanceRoot &&
       (!data.mapping.contains(source->parent)||live->parent!=data.mapping.at(source->parent)))) {
      error="A hierarquia da instância mudou; Apply estrutural ainda não é suportado";return false;
    }
    std::vector<const scene::ComponentValue*> local,original;
    for(usize i=0;i<live->components.size();++i) if(!metadata(*live->components.at(i))) local.push_back(live->components.at(i));
    for(usize i=0;i<source->components.size();++i) if(!metadata(*source->components.at(i))) original.push_back(source->components.at(i));
    for(const auto *component:local) {
      const auto *other=source->components.findInstance(component->instanceId());
      if(component->unresolved() || (other && &component->type()!=&other->type())) {
        error="Identidade ou implementação de componente incompatível";return false;
      }
    }
  }
  return true;
}
bool loadedInstances(EditorSession &session,resources::AssetGuid asset,const std::string &hash,
                     std::vector<EditorEntityId> &roots,std::string &error) {
  std::vector<EditorEntityId> ids;session.document().collectSubtree(session.document().root(),ids);
  std::unordered_set<EditorEntityId> unique;
  for(const auto id:ids) if(const auto *link=scene::prefabLink(session.document().find(id)->components);link && link->asset==asset) {
    if(!link->instanceRoot || link->instanceRoot>std::numeric_limits<EditorEntityId>::max()) {error="Raiz de instância inválida";return false;}
    unique.insert(static_cast<EditorEntityId>(link->instanceRoot));
  }
  roots.assign(unique.begin(),unique.end());std::sort(roots.begin(),roots.end());
  if(roots.empty()) {error="Nenhuma instância carregada do recurso";return false;}
  for(const auto root:roots) {
    PrefabComparison data;
    if(!comparison(session,root,data,error) || data.asset!=asset || data.hash!=hash ||
       !structurallyApplyable(session,data,error)) {
      if(error.empty()) error="A fonte mudou durante a preparação";
      return false;
    }
  }
  return true;
}
std::string entitySnapshot(const EditorEntity &value) {
  auto text=runtime::serializePrefabObject(value);
  if(const auto *link=scene::prefabLink(value.components)) {std::ostringstream out;link->write(out);text+='\n';text+=out.str();}
  return text;
}
bool sameRecord(const resources::AssetRecord &a,const resources::AssetRecord &b) {
  return a.guid==b.guid && a.type==b.type && a.path==b.path && a.source==b.source && a.contentHash==b.contentHash &&
    a.importerVersion==b.importerVersion && a.importerParameters==b.importerParameters && a.dependencies==b.dependencies && a.derived==b.derived;
}
bool portableAddress(const EditorEntity &donor,const PrefabOverride &row,const runtime::ObjectCloneMap &reverse) {
  if(row.kind==PrefabOverrideKind::RemovedComponent || row.kind==PrefabOverrideKind::ComponentOrder)return true;
  const auto *component=donor.components.findInstance(row.component);if(!component) return row.kind==PrefabOverrideKind::Object;
  const bool whole=row.kind==PrefabOverrideKind::Component || row.kind==PrefabOverrideKind::AddedComponent;
  bool valid=true;
  const auto check=[&](u64 target){if(target && (target>std::numeric_limits<EditorEntityId>::max() || !reverse.contains(static_cast<EditorEntityId>(target)))) valid=false;};
  if(whole || row.fieldKind==scene::FieldKind::Reference) {
    for(const auto &reference:component->type().references)
      if(reference.read && (whole || reference.id==row.field)) check(reference.read(*component));
  }
  if(const auto *script=scene::scriptBehavior(component)) for(const auto &p:script->properties)
    if(whole || (row.kind==PrefabOverrideKind::ScriptField && p.id==row.field))
      scene::forEachScriptPropertyObject(p,check);
  return valid;
}
// A regeneration baseline and its physical parts form one authoring revision.
// Publishing only the baseline would reinterpret inherited old meshes as local
// edits on every other instance. Include existing, added and removed members;
// never pull unrelated colliders, Body, motor, visual or child transforms in.
std::vector<u64> collisionRevisionMembers(const EditorEntity &donor,const EditorEntity &source,u64 component) {
  const auto *value=donor.components.findInstance(component);
  if(!value||&value->type()!=&scene::CollisionRecipe::descriptor)return {};
  std::vector<u64> ids;
  const auto append=[&](const scene::CollisionRecipe &recipe){for(const auto &part:recipe.parts)if(std::find(ids.begin(),ids.end(),part.collider)==ids.end())ids.push_back(part.collider);};
  append(*static_cast<const scene::CollisionRecipe*>(value));
  if(!source.components.find(scene::CollisionRecipe::descriptor)) {
    // First-time authoring on a linked instance also creates required physical
    // authority. Include only new dependencies; existing Body/motor edits stay
    // separate addresses and are never silently published with regeneration.
    for(const auto *type:{&scene::PhysicsBody::descriptor,&scene::DynamicBodyMotor::descriptor})
      if(const auto *required=donor.components.find(*type);required&&!source.components.find(*type))
        ids.push_back(required->instanceId());
  }
  if(const auto *old=source.components.findInstance(component);old&&&old->type()==&scene::CollisionRecipe::descriptor)append(*static_cast<const scene::CollisionRecipe*>(old));
  return ids;
}
bool prepareCollisionRevision(EditorEntity &candidate,const EditorEntity &donor,u64 component,std::string &error) {
  const auto members=collisionRevisionMembers(donor,candidate,component);
  // Free removed slots first, so replacing a full 64-component object is an
  // atomic operation rather than an artificial transient-capacity failure.
  for(auto id:members)if(!donor.components.findInstance(id))candidate.components.removeInstance(id);
  for(auto id:members)if(donor.components.findInstance(id)&&!adoptComponent(candidate,donor,id)){error="A revisão física não pôde transferir o Colisor #"+std::to_string(id);return false;}
  return true;
}
}

bool EditorSession::inspectPrefabOverrides(EditorEntityId selected,PrefabOverrideView &view,std::string &error) {
  error.clear();view={};view.object=selected;view.revision=document_.revision();
  PrefabComparison comparisonData;
  if(!comparison(*this,selected,comparisonData,error)) {view.error=error;return false;}
  view.asset=comparisonData.asset;view.sourceHash=comparisonData.hash;
  EditorEntity current,base,source;
  if(!comparisonObjects(*this,comparisonData,selected,current,base,source,error)) {view.error=error;return false;}
  threeWayDifferences(current,base,source,comparisonData.instanceRoot==selected,view.rows);
  std::vector<EditorEntityId> roots;std::string applyError;
  const bool structural=loadedInstances(*this,view.asset,view.sourceHash,roots,applyError);
  runtime::ObjectCloneMap reverse;for(const auto &[from,to]:comparisonData.mapping) reverse.emplace(to,from);
  for(auto &row:view.rows) {
    const auto revisionMembers=collisionRevisionMembers(current,source,row.component);
    if(!revisionMembers.empty()) {
      row.label+=" + partes";
      if(std::any_of(view.rows.begin(),view.rows.end(),[&](const auto &part){return std::find(revisionMembers.begin(),revisionMembers.end(),part.component)!=revisionMembers.end()&&part.origin==PrefabOverrideOrigin::Conflict;}))row.origin=PrefabOverrideOrigin::Conflict;
    }
    row.applyable=structural && row.applicable && row.origin!=PrefabOverrideOrigin::Inherited &&
      portableAddress(current,row,reverse);
    row.applyReason=!structural?applyError:row.origin==PrefabOverrideOrigin::Inherited?"Campo herdado: receba a fonte":
      !row.applicable?"Campo indisponível neste estado":
      !row.applyable?"Este endereço aponta para fora da instância portátil":
      row.origin==PrefabOverrideOrigin::Conflict?"Substituir a fonte exige escolha explícita":
      !revisionMembers.empty()?"Revisão física: receita + "+std::to_string(revisionMembers.size())+" Colisores; os outros componentes ficam separados":row.kind==PrefabOverrideKind::Component?"Aplicar componente completo, com estrutura e identidades":"Aplicar somente este endereço à fonte";
    if(row.applyable) {
      auto donor=current;donor.components.remove(scene::PrefabLink::descriptor);
      auto candidate=*comparisonData.source.graph().find(scene::prefabLink(document_.find(selected)->components)->sourceObject);
      std::string diagnostic;
      const bool membership=row.kind==PrefabOverrideKind::AddedComponent || row.kind==PrefabOverrideKind::RemovedComponent ||
        (row.kind==PrefabOverrideKind::Component && !candidate.components.findInstance(row.component));
      if(!runtime::remapObjectReferences(donor,reverse) ||
         !prepareCollisionRevision(candidate,donor,row.component,diagnostic) ||
         !(membership?adoptComponent(candidate,donor,row.component):applyDifference(candidate,donor,row,diagnostic)) ||
         !structuralCompositionValid(*comparisonData.source.graph().find(candidate.id),candidate,diagnostic) ||
         !restoredReferencesValid(comparisonData.source.graph(),*comparisonData.source.graph().find(candidate.id),candidate)) {
        row.applyable=false;row.applyReason=diagnostic.empty()?"O endereço isolado criaria uma referência ausente":diagnostic+"; aplique primeiro a dependência ou use um lote completo";
      }
    }
  }
  std::vector<EditorEntityId> ids;comparisonData.source.graph().collectSubtree(comparisonData.source.root(),ids);
  for(const auto id:ids) {
    std::vector<PrefabOverride> incoming;
    auto base=*comparisonData.baseline.graph().find(id),source=*comparisonData.source.graph().find(id);
    if(!inheritCollectionFloors(base,source) || !inheritCollectionFloors(source,base)) {view.error="Contrato de identidades da coleção indisponível";return false;}
    differences(base,source,id==comparisonData.source.root(),incoming);
    if(!incoming.empty()) {view.hasSourceChanges=true;break;}
  }
  return true;
}

bool EditorSession::revertPrefabOverride(const PrefabOverrideView &view,usize rowIndex,std::string &error) {
  return revertPrefabOverrides(view,{&rowIndex,1},error);
}

bool EditorSession::revertPrefabOverrides(const PrefabOverrideView &view,std::span<const usize> requested,std::string &error) {
  error.clear();
  if(isPlaying() || history_.isOpen()) {error="Finalize a edição antes de reverter";return false;}
  if(view.revision!=document_.revision() || requested.empty()) {error="Seleção vazia ou cena alterada; atualize a comparação antes de reverter";return false;}
  PrefabComparison comparisonData;
  if(!comparison(*this,view.object,comparisonData,error)) return false;
  if(comparisonData.asset!=view.asset || comparisonData.hash!=view.sourceHash) {error="A fonte mudou; atualize a comparação antes de reverter";return false;}
  EditorEntity current,base,source;
  if(!comparisonObjects(*this,comparisonData,view.object,current,base,source,error)) return false;
  auto candidate=*document_.find(view.object);std::vector<PrefabOverride> actual;
  threeWayDifferences(current,base,source,comparisonData.instanceRoot==view.object,actual);
  // Rebase only the reverted address, in SOURCE IDs. Rebasing the whole object
  // here would turn its other still-inherited old values into local overrides.
  const auto *link=scene::prefabLink(candidate.components);
  auto baseline=*comparisonData.baseline.graph().find(link->sourceObject);
  const auto &authoredSource=*comparisonData.source.graph().find(link->sourceObject);
  std::unordered_set<usize> unique;
  // Use comparison order, independent of the order in which rows were picked.
  // Local additions are removed before source components are restored by index.
  std::vector<usize> ordered(requested.begin(),requested.end());
  std::sort(ordered.begin(),ordered.end());
  for(const auto index:ordered) {
    if(index>=view.rows.size()) {error="Endereço de alteração inválido";return false;}
    if(!unique.insert(index).second) continue;
    const auto found=std::find_if(actual.begin(),actual.end(),[&](const auto &row){return row.sameAddress(view.rows[index]);});
    if(found==actual.end() || !found->applicable) {error="Alteração indisponível; atualize a comparação";return false;}
    if(!applyDifference(candidate,source,*found,error)) {if(error.empty()) error="Não foi possível restaurar esta alteração";return false;}
    if(found->kind==PrefabOverrideKind::RemovedComponent || found->kind==PrefabOverrideKind::AddedComponent ||
       (found->kind==PrefabOverrideKind::Component && !baseline.components.findInstance(found->component))) {
      if(!adoptComponent(baseline,authoredSource,found->component)) {error="Não foi possível atualizar a base do componente";return false;}
    } else if(!applyDifference(baseline,authoredSource,*found,error)) return false;
  }
  for(const auto index:unique) {
    const auto &row=view.rows[index];
    if(row.kind==PrefabOverrideKind::AddedComponent && componentReferenced(document_,view.object,row.component,&candidate)) {
      error="Um componente removido ainda é referenciado; reverta também a referência";return false;
    }
  }
  if(!structuralCompositionValid(*document_.find(view.object),candidate,error))return false;
  if(!restoredReferencesValid(document_,*document_.find(view.object),candidate)) {
    error="A restauração criaria uma referência ausente; restaure também o componente referido";return false;
  }
  const auto text=runtime::serializePrefabObject(baseline);
  if(text.empty()) {error="Base restaurada inválida";return false;}
  static_cast<scene::PrefabLink*>(candidate.components.edit(scene::PrefabLink::descriptor))->base=text;
  if(!history_.begin("Reverter alteração de prefab")) {error="Histórico indisponível";return false;}
  if(!history_.applyValues(document_,view.object,candidate)) {
    history_.cancel(document_);error="Valores recusados; a instância foi preservada";return false;
  }
  history_.end();state_.status=std::to_string(unique.size())+" alterações de prefab revertidas";return true;
}

bool EditorSession::refreshPrefabInstance(const PrefabOverrideView &view,std::string &error) {
  error.clear();
  if(isPlaying() || history_.isOpen()) {error="Finalize a edição antes de receber a fonte";return false;}
  if(view.revision!=document_.revision()) {error="A cena mudou; atualize a comparação antes de receber a fonte";return false;}
  PrefabComparison comparisonData;
  if(!comparison(*this,view.object,comparisonData,error)) return false;
  if(comparisonData.asset!=view.asset || comparisonData.hash!=view.sourceHash) {error="A fonte mudou; atualize a comparação antes de receber a fonte";return false;}
  auto prepared=document_;auto nextHistory=history_;
  if(!nextHistory.begin("Receber fonte de prefab")) {error="Histórico indisponível";return false;}
  std::vector<EditorEntityId> ids;comparisonData.source.graph().collectSubtree(comparisonData.source.root(),ids);
  for(const auto sourceId:ids) {
    const auto id=comparisonData.mapping.at(sourceId);
    EditorEntity current,base,source;
    if(!comparisonObjects(*this,comparisonData,id,current,base,source,error)) return false;
    std::vector<PrefabOverride> rows;threeWayDifferences(current,base,source,id==comparisonData.instanceRoot,rows);
    auto candidate=*prepared.find(id);
    for(const auto &row:rows)if(!row.applicable) {error="Identidade incompatível: "+row.label;return false;}
    for(const auto &row:rows) if(row.origin==PrefabOverrideOrigin::Inherited) {
      if(!row.applicable || !applyDifference(candidate,source,row,error)) {
        if(error.empty()) error="Campo herdado indisponível: "+row.label;
        return false;
      }
    }
    if(!inheritCollectionFloors(candidate,source)) {error="Não foi possível conservar identidades da coleção";return false;}
    // A conflict keeps its local value, but its base advances with the source.
    // Each sibling retains this instance's mapping and its own stable identity.
    const auto baseline=runtime::serializePrefabObject(*comparisonData.source.graph().find(sourceId));
    if(baseline.empty()) {error="Base da fonte inválida";return false;}
    static_cast<scene::PrefabLink*>(candidate.components.edit(scene::PrefabLink::descriptor))->base=baseline;
    if(!structuralCompositionValid(*prepared.find(id),candidate,error))return false;
    if(!nextHistory.applyValues(prepared,id,candidate)) {
      error="A atualização criaria valores ou referências inválidas; a instância foi preservada";return false;
    }
  }
  std::vector<EditorEntityId> allIds;prepared.collectSubtree(prepared.root(),allIds);
  for(const auto id:allIds)if(!restoredReferencesValid(prepared,*document_.find(id),*prepared.find(id))) {
    error="A atualização invalidaria uma referência; a instância foi preservada";return false;
  }
  nextHistory.end();document_=std::move(prepared);history_=std::move(nextHistory);
  state_.status="Fonte recebida na instância; alterações locais preservadas";return true;
}

bool EditorSession::applyPrefabOverride(const PrefabOverrideView &view,usize row,bool overwriteConflict,
                                      std::string &error,PrefabApplyReport *report) {
  return applyPrefabOverrides(view,{&row,1},overwriteConflict,error,report);
}

bool EditorSession::applyPrefabOverrides(const PrefabOverrideView &view,std::span<const usize> requested,
                                       bool overwriteConflicts,std::string &error,PrefabApplyReport *report) {
  error.clear();if(report) *report={};
  if(isPlaying() || history_.isOpen()) {error="Finalize a edição e saia do Play antes de aplicar";return false;}
  if(view.revision!=document_.revision() || requested.empty()) {error="Seleção vazia ou cena alterada; atualize a comparação";return false;}
  PrefabComparison selected;
  if(!comparison(*this,view.object,selected,error)) return false;
  if(selected.asset!=view.asset || selected.hash!=view.sourceHash) {error="A fonte mudou; atualize a comparação antes de aplicar";return false;}
  std::vector<EditorEntityId> roots;
  if(!loadedInstances(*this,selected.asset,selected.hash,roots,error)) return false;
  EditorEntity local,base,source;
  if(!comparisonObjects(*this,selected,view.object,local,base,source,error)) return false;
  std::vector<PrefabOverride> actual,chosen;threeWayDifferences(local,base,source,view.object==selected.instanceRoot,actual);
  for(const auto index:requested) {
    if(index>=view.rows.size()) {error="Endereço de override ausente";return false;}
    const auto &address=view.rows[index];
    const auto found=std::find_if(actual.begin(),actual.end(),[&](const auto &row){return row.sameAddress(address);});
    if(found==actual.end() || !found->applicable || found->origin==PrefabOverrideOrigin::Inherited) {
      error="Apply exige uma alteração local válida; alterações herdadas devem ser recebidas da fonte";return false;
    }
    if(found->origin==PrefabOverrideOrigin::Conflict && !overwriteConflicts) {
      error="Conflito: escolha explicitamente substituir a fonte antes de aplicar";return false;
    }
    if(std::any_of(chosen.begin(),chosen.end(),[&](const auto &row){return row.sameAddress(*found);})) {
      error="Endereço duplicado na seleção de Apply";return false;
    }
    chosen.push_back(*found);
  }
  runtime::Prefab authored;std::string beforeText;
  if(!loadPrefab(view.asset,authored,error,&beforeText) || Sha256::hex({reinterpret_cast<const u8*>(beforeText.data()),beforeText.size()})!=view.sourceHash) {
    if(error.empty()) error="A fonte mudou durante a preparação";
    return false;
  }
  auto authoredGraph=authored.graph();const auto *ownership=scene::prefabLink(document_.find(view.object)->components);
  // Expand a selected recipe to the concrete delta addresses of its own parts.
  // Each dependency still passes the existing portability/conflict gates.
  const auto primary=chosen;
  for(const auto &row:primary) {
    const auto members=collisionRevisionMembers(local,*authoredGraph.find(ownership->sourceObject),row.component);
    for(const auto &part:actual)if(std::find(members.begin(),members.end(),part.component)!=members.end()&&part.origin!=PrefabOverrideOrigin::Inherited&&!std::any_of(chosen.begin(),chosen.end(),[&](const auto &address){return address.sameAddress(part);})) {
      if(!part.applicable||(part.origin==PrefabOverrideOrigin::Conflict&&!overwriteConflicts)){error="A revisão física possui uma parte indisponível ou conflitante; confirme o lote completo antes de aplicar";return false;}
      chosen.push_back(part);
    }
  }
  runtime::ObjectCloneMap reverse;for(const auto &[from,to]:selected.mapping) reverse.emplace(to,from);
  for(const auto &row:chosen) if(!portableAddress(local,row,reverse)) {
    error="O endereço selecionado aponta para fora da instância; a fonte portátil foi preservada";return false;
  }
  auto donor=local;donor.components.remove(scene::PrefabLink::descriptor);
  if(!runtime::remapObjectReferences(donor,reverse)) {error="Não foi possível normalizar referências da seleção";return false;}
  auto sourceCandidate=*authoredGraph.find(ownership->sourceObject);
  donor.id=sourceCandidate.id;donor.parent=sourceCandidate.parent;
  std::stable_sort(chosen.begin(),chosen.end(),[](const auto &a,const auto &b){const auto order=[](const auto &row){return row.kind==PrefabOverrideKind::RemovedComponent?0:row.kind==PrefabOverrideKind::ComponentOrder?2:1;};return order(a)<order(b);});
  for(const auto &row:chosen) {
    const bool membership=row.kind==PrefabOverrideKind::AddedComponent || row.kind==PrefabOverrideKind::RemovedComponent ||
      (row.kind==PrefabOverrideKind::Component && !sourceCandidate.components.findInstance(row.component));
    if(!(membership?adoptComponent(sourceCandidate,donor,row.component):applyDifference(sourceCandidate,donor,row,error))) {
      if(error.empty()) error="Endereço selecionado não pôde ser aplicado";
      return false;
    }
  }
  if(!structuralCompositionValid(*authoredGraph.find(sourceCandidate.id),sourceCandidate,error))return false;
  if(!authoredGraph.applyEntityValues(sourceCandidate.id,sourceCandidate)) {error="Valores autorais da fonte recusados";return false;}
  runtime::Prefab nextAuthored,nextResolved;
  if(!nextAuthored.capture(authoredGraph,authored.root(),view.asset,error) || !preparePrefab(nextAuthored,nextResolved,error)) return false;
  const auto afterText=nextAuthored.write();
  if(afterText.empty()) {error="Fonte nova não pôde ser serializada";return false;}
  const auto *registered=assets_.find(view.asset);
  if(!registered || registered->type!=resources::AssetType::Prefab) {error="Registro do prefab indisponível";return false;}
  const auto beforeRecord=*registered;auto afterRecord=beforeRecord;
  afterRecord.contentHash=Sha256::hex({reinterpret_cast<const u8*>(afterText.data()),afterText.size()});
  afterRecord.dependencies.clear();std::vector<EditorEntityId> sourceIds;
  nextAuthored.graph().collectSubtree(nextAuthored.root(),sourceIds);
  const auto dependency=[&](resources::AssetGuid guid)->bool {
    if(!guid.valid()) return true;
    if(!assets_.find(guid)) {error="Dependência não registrada na fonte: "+guid.text();return false;}
    if(std::find(afterRecord.dependencies.begin(),afterRecord.dependencies.end(),guid)==afterRecord.dependencies.end()) afterRecord.dependencies.push_back(guid);
    return true;
  };
  for(const auto id:sourceIds) {
    const auto &value=*nextAuthored.graph().find(id);
    for(usize i=0;i<value.components.size();++i) {
      const auto *component=value.components.at(i);
      for(const auto &binding:component->type().resourceBindings) for(u32 slot=0;slot<binding.slotCount(*component);++slot) {
        auto guid=binding.at(*component,slot);if(!guid.valid() || binding.declaresNone(guid)) continue;
        if(!assets_.find(guid) && binding.kind==resources::AssetType::Mesh) {
          if(!mapScene_.assetSlot(guid)) {error="Malha da fonte indisponível";return false;}
          auto owner=resources::AssetGuid{};
          for(const auto &imported:importedSources_)
            if(std::find(imported.identities.begin(),imported.identities.end(),guid)!=imported.identities.end()) {owner=imported.guid;break;}
          guid=owner; // engine primitives have no project resource lifetime.
        } else if(!assets_.find(guid) && binding.kind==resources::AssetType::AnimationClip) {
          runtime::AnimationClipView clip;if(!mapScene_.findClip(guid,clip) || !clip.source) {error="Clipe da fonte indisponível";return false;}
          guid=clip.source->source;
        }
        if(!dependency(guid)) return false;
      }
      if(const auto *script=scene::scriptBehavior(component)) {
        const auto *asset=assets_.findByPath(script->source);
        if(asset && asset->type==resources::AssetType::Script && !dependency(asset->guid)) return false;
      }
    }
  }
  auto nextAssets=assets_;
  if(!nextAssets.publishImport(view.asset,afterRecord.contentHash,afterRecord.importerVersion,afterRecord.importerParameters,
                              afterRecord.derived,afterRecord.dependencies)) {error="Registro recusou dependências da fonte nova";return false;}
  auto prepared=document_;std::vector<EditorEntity> beforeValues,afterValues;PrefabApplyReport result;result.instances=roots.size();result.sourceHash=afterRecord.contentHash;
  for(const auto root:roots) {
    PrefabComparison instance;if(!comparison(*this,root,instance,error) || instance.hash!=view.sourceHash) {
      if(error.empty()) error="A fonte mudou durante a propagação";
      return false;
    }
    for(const auto sourceId:sourceIds) {
      const auto id=instance.mapping.at(sourceId);
      EditorEntity current,previousBaseline,oldSource;
      if(!comparisonObjects(*this,instance,id,current,previousBaseline,oldSource,error)) return false;
      auto incoming=*nextResolved.graph().find(sourceId);
      if(!runtime::remapObjectReferences(incoming,instance.mapping)) {error="Referência indisponível na propagação";return false;}
      incoming.id=id;incoming.parent=current.parent;
      std::vector<PrefabOverride> rows;threeWayDifferences(current,previousBaseline,incoming,id==root,rows);
      auto candidate=*prepared.find(id);auto rebased=*nextResolved.graph().find(sourceId);
      for(const auto &row:rows) {
        if(!row.applicable) {error="Identidades incompatíveis na propagação: "+row.label;return false;}
        if(row.origin==PrefabOverrideOrigin::Inherited) {
          if(!row.applicable || !applyDifference(candidate,incoming,row,error)) {
            if(error.empty()) error="Campo herdado indisponível: "+row.label;
            return false;
          }
          ++result.propagatedFields;
        } else if(row.origin==PrefabOverrideOrigin::Conflict) {
          // Keep the old B only at conflicting addresses: conflict remains
          // inspectable and survives save/reopen instead of becoming silently Local.
          const auto &oldBase=*instance.baseline.graph().find(sourceId);
          if(row.component && (!oldBase.components.findInstance(row.component) || !rebased.components.findInstance(row.component) ||
             row.kind==PrefabOverrideKind::AddedComponent || row.kind==PrefabOverrideKind::RemovedComponent)) {
            if(!adoptComponent(rebased,oldBase,row.component)) {error="Base estrutural conflitante indisponível";return false;}
          } else if(!applyDifference(rebased,oldBase,row,error)) return false;
          ++result.conflicts;
        }
      }
      if(!inheritCollectionFloors(candidate,incoming) || !inheritCollectionFloors(rebased,*instance.baseline.graph().find(sourceId))) {
        error="Não foi possível conservar a fronteira de identidades da coleção";return false;
      }
      const auto baseline=runtime::serializePrefabObject(rebased);
      auto *link=static_cast<scene::PrefabLink*>(candidate.components.edit(scene::PrefabLink::descriptor));
      if(!link || baseline.empty()) {error="Base da propagação inválida";return false;}
      link->base=baseline;
      if(!structuralCompositionValid(*document_.find(id),candidate,error))return false;
      beforeValues.push_back(*document_.find(id));afterValues.push_back(candidate);
      if(!prepared.applyEntityValues(id,candidate)) {error="A propagação criaria valores inválidos";return false;}
      ++result.objects;
    }
  }
  for(usize i=0;i<afterValues.size();++i) if(!restoredReferencesValid(prepared,beforeValues[i],afterValues[i])) {
    error="A propagação criaria referências inválidas; nada foi publicado";return false;
  }
  // External typed references into an instance must not become dangling when
  // the source removes a component, even if their owners are not prefab members.
  std::vector<EditorEntityId> allIds;prepared.collectSubtree(prepared.root(),allIds);
  for(const auto id:allIds)if(!restoredReferencesValid(prepared,*document_.find(id),*prepared.find(id))) {
    error="Uma referência externa seria invalidada pela propagação";return false;
  }
  const auto project=files_.rootPath();auto nextHistory=history_;
  if(!nextHistory.recordResource("Aplicar e propagar prefab",[this,project,beforeText,afterText,beforeRecord,afterRecord,beforeValues,afterValues](bool forward) {
    if(isPlaying() || files_.rootPath()!=project) {state_.status="Histórico de prefab indisponível neste projeto ou no Play";return false;}
    const auto &expected=forward?beforeValues:afterValues;const auto &desired=forward?afterValues:beforeValues;
    const auto &expectedRecord=forward?beforeRecord:afterRecord;const auto &desiredRecord=forward?afterRecord:beforeRecord;
    const auto *record=assets_.find(expectedRecord.guid);
    if(!record || !sameRecord(*record,expectedRecord)) {state_.status="O registro do prefab mudou; histórico preservado";return false;}
    auto nextDocument=document_;
    for(usize i=0;i<expected.size();++i) {
      const auto *value=document_.find(expected[i].id);
      if(!value || entitySnapshot(*value)!=entitySnapshot(expected[i]) || !nextDocument.applyEntityValues(desired[i].id,desired[i])) {
        state_.status="A instância mudou; histórico não sobrescreveu a cena";return false;
      }
    }
    for(usize i=0;i<desired.size();++i) if(!restoredReferencesValid(nextDocument,expected[i],desired[i])) {state_.status="Referências do histórico indisponíveis";return false;}
    std::vector<EditorEntityId> ids;nextDocument.collectSubtree(nextDocument.root(),ids);
    for(const auto id:ids)if(!restoredReferencesValid(nextDocument,*document_.find(id),*nextDocument.find(id))) {
      state_.status="Uma referência externa impede restaurar a composição do prefab";return false;
    }
    auto restoredAssets=assets_;
    if(!restoredAssets.publishImport(desiredRecord.guid,desiredRecord.contentHash,desiredRecord.importerVersion,
       desiredRecord.importerParameters,desiredRecord.derived,desiredRecord.dependencies)) {state_.status="Dependência do histórico indisponível";return false;}
    const auto &oldText=forward?beforeText:afterText;const auto &text=forward?afterText:beforeText;
    std::string diagnostic;EditorImportTransaction transaction(project);
    if(!transaction.begin(desiredRecord.path,Sha256::hex({reinterpret_cast<const u8*>(oldText.data()),oldText.size()}),diagnostic)) {state_.status=diagnostic;return false;}
    if(!transaction.commit({reinterpret_cast<const u8*>(text.data()),text.size()},restoredAssets.serialize())) {
      state_.status=transaction.rollback()?"Histórico recusado; fonte e registro restaurados":"Recuperação pendente no journal";return false;
    }
    document_=std::move(nextDocument);assets_=std::move(restoredAssets);assetRegistryDirty_=true;files_.rebuildTree();
    state_.status=forward?"Apply de prefab refeito com propagação":"Apply de prefab desfeito com fonte e instâncias";return true;
  })) {error="Não foi possível preparar o histórico reversível";return false;}
  EditorImportTransaction transaction(project);
  if(!transaction.begin(afterRecord.path,view.sourceHash,error)) return false;
  if(!transaction.commit({reinterpret_cast<const u8*>(afterText.data()),afterText.size()},nextAssets.serialize())) {
    error=transaction.rollback()?"Publicação recusada; fonte e registro restaurados":"Recuperação pendente no journal";return false;
  }
  document_=std::move(prepared);assets_=std::move(nextAssets);history_=std::move(nextHistory);assetRegistryDirty_=true;files_.rebuildTree();
  if(report) *report=result;
  state_.status="Apply: "+std::to_string(result.instances)+" instâncias; "+std::to_string(result.conflicts)+" conflitos locais preservados";
  return true;
}
} // namespace ae::editor
