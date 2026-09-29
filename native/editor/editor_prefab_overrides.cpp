#include "editor/editor_session.h"
#include "editor/editor_component_catalog.h"
#include "scene/prefab_link.h"
#include "scene/import_link.h"
#include "scene/script_behavior.h"
#include <sstream>

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
const scene::ScriptPropertyValue *property(const scene::ScriptBehavior &script,std::string_view id) {
  for(const auto &p:script.properties) if(p.id==id) return &p;
  return nullptr;
}
// Normalize only the source being compared. Never treat an unmapped source ID
// as a scene ID; the same integer may identify an unrelated scene object.
bool comparison(EditorSession &session,EditorEntityId selected,EditorEntity &source,
                resources::AssetGuid &asset,std::string &hash,std::string &error) {
  const auto &document=session.document();const auto *current=document.find(selected);
  const auto *link=current?scene::prefabLink(current->components):nullptr;
  if(!link) {error="Selecione um objeto vinculado a um prefab";return false;}
  asset=link->asset;
  runtime::Prefab authored;
  if(!session.loadPrefab(asset,authored,error)) return false;
  const auto text=authored.write();hash=Sha256::hex({reinterpret_cast<const u8*>(text.data()),text.size()});
  const auto *authoredObject=authored.graph().find(link->sourceObject);
  if(!authoredObject) {error="Objeto removido da fonte; a reconciliação estrutural ainda está pendente";return false;}
  EditorEntity baseline;std::istringstream base(link->base);
  if(!runtime::deserializePrefabObject(base,defaultEditorComponentRegistry(),baseline) || baseline.id!=link->sourceObject) {
    error="Base da instância inválida; nenhum valor foi alterado";return false;
  }
  base>>std::ws;if(!base.eof()) {error="Dados adicionais na base da instância";return false;}
  // Before structural reconciliation exists, refuse ambiguous source additions
  // instead of matching a new source component to a locally added component.
  if(baseline.components.size()!=authoredObject->components.size()) {
    error="A estrutura da fonte mudou; reconciliação de componentes pendente";return false;
  }
  for(usize i=0;i<baseline.components.size();++i) {
    const auto *a=baseline.components.at(i),*b=authoredObject->components.findInstance(a->instanceId());
    if(!b || &a->type()!=&b->type()) {error="A identidade dos componentes da fonte mudou";return false;}
  }
  runtime::Prefab prepared;if(!session.preparePrefab(authored,prepared,error)) return false;
  source=*prepared.graph().find(link->sourceObject);
  runtime::ObjectCloneMap mapping;std::vector<EditorEntityId> ids;document.collectSubtree(document.root(),ids);
  for(const auto id:ids) if(const auto *member=scene::prefabLink(document.find(id)->components);
      member && member->asset==asset && member->instanceRoot==link->instanceRoot) {
    if(!mapping.emplace(member->sourceObject,id).second) {error="Identidade de origem duplicada na instância";return false;}
  }
  bool references=true;
  const auto check=[&](u64 target) {
    if(target && (target>std::numeric_limits<EditorEntityId>::max() || !mapping.contains(static_cast<EditorEntityId>(target)))) references=false;
  };
  for(usize i=0;i<source.components.size();++i) {
    const auto *component=source.components.at(i);
    for(const auto &ref:component->type().references) if(ref.read) check(ref.read(*component));
    if(const auto *script=scene::scriptBehavior(component)) for(const auto &p:script->properties) scene::forEachScriptPropertyObject(p,check);
  }
  if(!references || !runtime::remapObjectReferences(source,mapping)) {
    error="A fonte referencia um objeto ausente da instância; restaure a estrutura antes de reverter propriedades";return false;
  }
  source.id=selected;source.parent=current->parent;
  return true;
}

void differences(const EditorEntity &current,const EditorEntity &source,bool root,std::vector<PrefabOverride> &rows) {
  const auto object=[&](std::string field,std::string label,std::string a,std::string b,u32 slot=0) {
    if(a!=b) rows.push_back({PrefabOverrideKind::Object,0,std::move(field),std::move(label),std::move(a),std::move(b),slot});
  };
  object("name","Nome",current.name,source.name);
  const auto flag=[&](const char *field,const char *label,bool a,bool b) {object(field,label,a?"Sim":"Não",b?"Sim":"Não");};
  flag("active","Ativo",current.active,source.active);flag("visible","Visível",current.visible,source.visible);
  flag("castShadow","Projeta sombras",current.castShadow,source.castShadow);
  flag("receiveShadow","Recebe sombras",current.receiveShadow,source.receiveShadow);
  object("tag","Tag",current.tag,source.tag);object("layer","Camada",std::to_string(current.layer),std::to_string(source.layer));
  const char *groups[]{"Posição","Rotação","Escala"};const char *axes[]{"X","Y","Z"};
  const float *a[]{current.transform.position,current.transform.rotationDegrees,current.transform.scale};
  const float *b[]{source.transform.position,source.transform.rotationDegrees,source.transform.scale};
  for(u32 group=root?2:0;group<3;++group) for(u32 axis=0;axis<3;++axis) if(a[group][axis]!=b[group][axis])
    rows.push_back({PrefabOverrideKind::Object,0,"transform",std::string(groups[group])+" "+axes[axis],scene::detail::formatNumber(a[group][axis]),scene::detail::formatNumber(b[group][axis]),group*3+axis});
  for(usize i=0;i<current.components.size();++i) {
    const auto *local=current.components.at(i);if(metadata(*local)) continue;
    const auto *original=source.components.findInstance(local->instanceId());
    const auto label=componentLabel(*local);
    if(!original) {rows.push_back({PrefabOverrideKind::AddedComponent,local->instanceId(),{},label,"Componente adicionado","Ausente na fonte"});continue;}
    if(&local->type()!=&original->type()) {
      rows.push_back({PrefabOverrideKind::Component,local->instanceId(),{},label,"Identidade incompatível","Requer reconciliação",0,scene::FieldKind::Number,false});continue;
    }
    if(payload(*local)==payload(*original)) continue;
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
    if(!metadata(*component) && !current.components.findInstance(component->instanceId()))
      rows.push_back({PrefabOverrideKind::RemovedComponent,component->instanceId(),{},componentLabel(*component),"Componente removido","Restaurar componente",static_cast<u32>(i)});
  }
}

bool componentReferenced(const EditorDocument &document,EditorEntityId object,u64 component) {
  std::vector<EditorEntityId> ids;document.collectSubtree(document.root(),ids);
  for(const auto id:ids) for(usize i=0;i<document.find(id)->components.size();++i) {
    const auto *value=document.find(id)->components.at(i);
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
  return false;
}
bool restoredReferencesValid(const EditorDocument &document,const EditorEntity &before,const EditorEntity &candidate) {
  const auto lookup=[&](u64 id)->const EditorEntity* {
    if(id==candidate.id) return &candidate;
    return id<=std::numeric_limits<EditorEntityId>::max()?document.find(static_cast<EditorEntityId>(id)):nullptr;
  };
  for(usize i=0;i<candidate.components.size();++i) {
    const auto *value=candidate.components.at(i),*old=before.components.findInstance(value->instanceId());
    if(old && payload(*old)==payload(*value)) continue;
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
bool applyDifference(EditorEntity &candidate,const EditorEntity &source,const PrefabOverride &row,std::string &error) {
  const auto *component=source.components.findInstance(row.component);
  switch(row.kind) {
  case PrefabOverrideKind::Object:
    if(row.field=="name") runtime::assignObjectName(candidate,source.name);
    else if(row.field=="tag") candidate.tag=source.tag;
    else if(row.field=="layer") candidate.layer=source.layer;
    else if(row.field=="active") candidate.active=source.active;
    else if(row.field=="visible") candidate.visible=source.visible;
    else if(row.field=="castShadow") candidate.castShadow=source.castShadow;
    else if(row.field=="receiveShadow") candidate.receiveShadow=source.receiveShadow;
    else if(row.field=="transform" && row.slot<9) {
      float *a[]{candidate.transform.position,candidate.transform.rotationDegrees,candidate.transform.scale};
      const float *b[]{source.transform.position,source.transform.rotationDegrees,source.transform.scale};
      a[row.slot/3][row.slot%3]=b[row.slot/3][row.slot%3];
    } else return false;
    return true;
  case PrefabOverrideKind::Field: {
    if(!component) return false;
    const scene::FieldAddress address{row.field,row.slot,row.fieldKind};
    const auto outcome=scene::applyComponentFields(candidate.components,*component,{&address,1},row.component);
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
  case PrefabOverrideKind::Component: return component && candidate.components.replaceInstance(row.component,*component);
  case PrefabOverrideKind::AddedComponent: return candidate.components.removeInstance(row.component);
  case PrefabOverrideKind::RemovedComponent: return component && candidate.components.restoreInstance(*component,row.component,row.slot);
  }
  return false;
}
}

bool EditorSession::inspectPrefabOverrides(EditorEntityId selected,PrefabOverrideView &view,std::string &error) {
  error.clear();view={};view.object=selected;view.revision=document_.revision();
  EditorEntity source;
  if(!comparison(*this,selected,source,view.asset,view.sourceHash,error)) {view.error=error;return false;}
  const auto &current=*document_.find(selected);const auto *link=scene::prefabLink(current.components);
  differences(current,source,link->instanceRoot==selected,view.rows);return true;
}

bool EditorSession::revertPrefabOverride(const PrefabOverrideView &view,usize rowIndex,std::string &error) {
  error.clear();
  if(isPlaying() || history_.isOpen()) {error="Finalize a edição antes de reverter";return false;}
  if(view.revision!=document_.revision() || rowIndex>=view.rows.size()) {error="A cena mudou; atualize a comparação antes de reverter";return false;}
  EditorEntity source;resources::AssetGuid asset;std::string hash;
  if(!comparison(*this,view.object,source,asset,hash,error)) return false;
  if(asset!=view.asset || hash!=view.sourceHash) {error="A fonte mudou; atualize a comparação antes de reverter";return false;}
  auto candidate=*document_.find(view.object);std::vector<PrefabOverride> actual;
  differences(candidate,source,scene::prefabLink(candidate.components)->instanceRoot==view.object,actual);
  const auto &requested=view.rows[rowIndex];
  const auto found=std::find_if(actual.begin(),actual.end(),[&](const auto &row) {return row.sameAddress(requested);});
  if(found==actual.end() || !found->applicable) {error="Alteração indisponível; atualize a comparação";return false;}
  if(found->kind==PrefabOverrideKind::AddedComponent && componentReferenced(document_,view.object,found->component)) {
    error="Este componente é referenciado por um script; remova a referência antes de reverter sua adição";return false;
  }
  if(!applyDifference(candidate,source,*found,error)) {if(error.empty()) error="Não foi possível restaurar esta alteração";return false;}
  if(!restoredReferencesValid(document_,*document_.find(view.object),candidate)) {
    error="A restauração criaria uma referência ausente; restaure primeiro o objeto ou componente referido";return false;
  }
  if(!history_.begin("Reverter alteração de prefab")) {error="Histórico indisponível";return false;}
  if(!history_.applyValues(document_,view.object,candidate)) {
    history_.cancel(document_);error="Valores recusados; a instância foi preservada";return false;
  }
  history_.end();state_.status="Alteração revertida: "+found->label;return true;
}
} // namespace ae::editor
