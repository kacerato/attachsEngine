#include "runtime/animation_property_binding.h"
#include "runtime/game_world.h"
#include "resources/animation_clip_asset.h"
#include "scene/mesh_renderer.h"
#include "scene/ui_canvas.h"
#include "ui/gui_document.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>

namespace ae::runtime {
using namespace resources;
namespace {
bool fail(std::string &error,const char *text) {error=text;return false;}
bool matches(const scene::ComponentValue &v,const AnimationPropertyAddress &a,std::string &error) {
  if(!validAnimationPropertyAddress(a)||v.instanceId()!=a.component||v.type().id!=a.type)return fail(error,"Componente de animação ausente ou substituído");
  if(v.type().version!=a.version)return fail(error,"Versão do componente mudou; remapeie o canal");
  const auto *schema=scene::findComponentSchema(v.type().id);
  constexpr u32 heavy=scene::Invalidate::PhysicsBody|scene::Invalidate::PhysicsShape|scene::Invalidate::MeshDerived|
    scene::Invalidate::TextureResidency|scene::Invalidate::Policy|scene::Invalidate::Script;
  if(!schema||schema->propertiesInPlay==scene::PlayMutability::Never||(schema->invalidates&heavy))return fail(error,"Componente não permite publicação animada por quadro");
  if(a.domain==AnimationPropertyDomain::MaterialSlot) {
    if(&v.type()!=&scene::MeshRenderer::descriptor)return fail(error,"Canal material exige MeshRenderer");
    const auto &mesh=static_cast<const scene::MeshRenderer&>(v);
    if(a.slot>=mesh.slotCount()||mesh.slotAsset(a.slot)!=a.resource)return fail(error,"Primitiva de material mudou; remapeie o slot");
    if(!mesh.slotMaterial(a.slot).enabled)return fail(error,"Ative a substituição local do material antes de animar seus fatores");
  }
  return true;
}
const scene::ComponentNumber *number(const scene::ComponentValue &v,std::string_view id) {
  const auto *schema=scene::findComponentSchema(v.type().id);if(!schema)return nullptr;
  const scene::ComponentNumber *found=nullptr;
  for(const auto &p:v.type().numbers)if(p.id==id) {if(found||!numberTweenable(*schema,p,v))return nullptr;found=&p;}
  return found;
}
const scene::ComponentTriple *triple(const scene::ComponentValue &v,const AnimationPropertyAddress &a) {
  for(const auto &p:v.type().triples)if(p.id==a.property) {
    const auto expected=p.kind==scene::ComponentTripleKind::LinearColor?AnimationPropertyKind::LinearColor:
      p.dimensions()==2?AnimationPropertyKind::Vector2:AnimationPropertyKind::Vector3;
    if(a.kind!=expected)return nullptr;
    for(u32 c=0;c<p.dimensions();++c)if(!number(v,p.channels[c]))return nullptr;
    return &p;
  }
  return nullptr;
}
const scene::ComponentSlotNumber *slotNumber(const scene::ComponentValue &v,std::string_view id,u32 slot) {
  for(const auto &p:scene::meshRendererSlotMaterial)if(p.id==id&&p.read&&p.write&&slot<p.slotCount(v)&&
      p.presentation.isEditable(v)&&p.presentation.isVisible(v))return &p;
  return nullptr;
}
bool discreteEligible(const scene::ComponentValue &v,const scene::PropertyPresentation &p) {
  const auto *schema=scene::findComponentSchema(v.type().id);
  constexpr u32 heavy=scene::Invalidate::PhysicsBody|scene::Invalidate::PhysicsShape|scene::Invalidate::MeshDerived|
    scene::Invalidate::TextureResidency|scene::Invalidate::Policy|scene::Invalidate::Script;
  return schema&&p.isVisible(v)&&p.isEditable(v)&&!((schema->invalidates|p.invalidates)&heavy)&&
    core::engineCapabilityAuthorable(p.capability.empty()?schema->capability:p.capability);
}
bool values(const AnimationPropertyValue &v,AnimationPropertyKind kind,float result[3]) {
  if(kind==AnimationPropertyKind::Number) {if(const auto *p=std::get_if<float>(&v)){result[0]=*p;return true;}}
  if(kind==AnimationPropertyKind::Vector2) {if(const auto *p=std::get_if<AnimationPropertyVector2>(&v)){result[0]=p->x;result[1]=p->y;return true;}}
  if(kind==AnimationPropertyKind::Vector3) {if(const auto *p=std::get_if<AnimationPropertyVector3>(&v)){result[0]=p->x;result[1]=p->y;result[2]=p->z;return true;}}
  if(kind==AnimationPropertyKind::LinearColor) {if(const auto *p=std::get_if<AnimationPropertyColor>(&v)){result[0]=p->r;result[1]=p->g;result[2]=p->b;return true;}}
  return false;
}
AnimationPropertyValue fromNumbers(AnimationPropertyKind kind,const float v[3]) {
  if(kind==AnimationPropertyKind::Vector2)return AnimationPropertyVector2{v[0],v[1]};
  if(kind==AnimationPropertyKind::Vector3)return AnimationPropertyVector3{v[0],v[1],v[2]};
  if(kind==AnimationPropertyKind::LinearColor)return AnimationPropertyColor{v[0],v[1],v[2]};
  return v[0];
}
}
bool readAnimationProperty(const scene::ComponentValue &v,const AnimationPropertyAddress &a,AnimationPropertyValue &out,std::string &error) {
  error.clear();if(!matches(v,a,error))return false;
  float components[3]{};
  if(a.domain==AnimationPropertyDomain::MaterialSlot) {
    const auto dimensions=animationPropertyComponents(a.kind);
    for(u32 c=0;c<dimensions;++c) {
      const auto id=dimensions==3?a.property+std::string(c==0?".r":c==1?".g":".b"):a.property;
      const auto *p=slotNumber(v,id,a.slot);if(!p)return fail(error,"Fator de material ausente ou não animável");components[c]=p->read(v,a.slot);
    }
    out=fromNumbers(a.kind,components);return true;
  }
  if(a.domain!=AnimationPropertyDomain::Component)return fail(error,"UI exige resolução do lease de Canvas");
  if(a.kind==AnimationPropertyKind::Number) {const auto *p=number(v,a.property);if(!p)return fail(error,"Propriedade numérica ausente, estrutural ou não animável");out=p->read(v);return true;}
  if(const auto *p=triple(v,a)) {for(u32 c=0;c<p->dimensions();++c)components[c]=number(v,p->channels[c])->read(v);out=fromNumbers(a.kind,components);return true;}
  if(a.kind==AnimationPropertyKind::Boolean)for(const auto &p:v.type().booleans)if(p.id==a.property&&p.animationSafe&&p.read&&p.write&&discreteEligible(v,p.presentation)){out=p.read(v);return true;}
  if(a.kind==AnimationPropertyKind::Enumeration)for(const auto &p:v.type().enums)if(p.id==a.property&&p.animationSafe&&p.read&&p.write&&discreteEligible(v,p.presentation)){out=p.read(v);return true;}
  if(a.kind==AnimationPropertyKind::Object)for(const auto &p:v.type().references)if(p.id==a.property&&p.animationSafe&&p.read&&p.write&&discreteEligible(v,p.presentation)){out=AnimationPropertyObject{p.read(v)};return true;}
  return fail(error,"Tipo ou propriedade sem contrato animável efetivo");
}
bool writeAnimationProperty(scene::ComponentValue &v,const AnimationPropertyAddress &a,const AnimationPropertyValue &input,std::string &error) {
  AnimationPropertyValue previous;if(!readAnimationProperty(v,a,previous,error))return false;
  float components[3]{};
  if(a.kind==AnimationPropertyKind::Boolean) {
    const auto *value=std::get_if<bool>(&input);if(!value)return fail(error,"Booleano incompatível");
    for(const auto &p:v.type().booleans)if(p.id==a.property){p.write(v,*value);return true;}
  } else if(a.kind==AnimationPropertyKind::Object) {
    const auto *value=std::get_if<AnimationPropertyObject>(&input);if(!value)return fail(error,"Referência incompatível");
    if(value->id>std::numeric_limits<u32>::max())return fail(error,"Referência de objeto fora da faixa");
    for(const auto &p:v.type().references)if(p.id==a.property){p.write(v,value->id);return true;}
  } else if(a.kind==AnimationPropertyKind::Enumeration) {
    const auto *value=std::get_if<u32>(&input);if(!value)return fail(error,"Enum incompatível");
    for(const auto &p:v.type().enums)if(p.id==a.property) {
      if(std::none_of(p.options.begin(),p.options.end(),[&](const auto &option){return option.value==*value;}))return fail(error,"Opção de enum inválida");
      p.write(v,*value);return true;
    }
  } else if(values(input,a.kind,components)) {
    const auto dimensions=animationPropertyComponents(a.kind);
    if(a.domain==AnimationPropertyDomain::MaterialSlot) {
      for(u32 c=0;c<dimensions;++c) {
        const auto id=dimensions==3?a.property+std::string(c==0?".r":c==1?".g":".b"):a.property;const auto *p=slotNumber(v,id,a.slot);
        if(!p||!std::isfinite(components[c])||components[c]<p->minimum||components[c]>p->maximum)return fail(error,"Fator material fora da faixa");
      }
      for(u32 c=0;c<dimensions;++c) {
        const auto id=dimensions==3?a.property+std::string(c==0?".r":c==1?".g":".b"):a.property;
        if(!slotNumber(v,id,a.slot)->write(v,a.slot,components[c]))return fail(error,"Setter material recusou o slot");
      }
      return true;
    }
    const auto *group=triple(v,a);
    for(u32 c=0;c<dimensions;++c) {
      const auto *p=number(v,group?group->channels[c]:a.property);
      if(!p||!std::isfinite(components[c])||components[c]<p->minimum||components[c]>p->maximum)return fail(error,"Propriedade fora da faixa");
    }
    for(u32 c=0;c<dimensions;++c) {*number(v,group?group->channels[c]:a.property)->write(v)=components[c];}
    return true;
  }
  return fail(error,"Valor incompatível com o canal");
}
std::vector<AnimationPropertyDescriptor> animationPropertyCatalog(const scene::ComponentValue &v) {
  std::vector<AnimationPropertyDescriptor> result;
  const auto append=[&](std::string_view id,AnimationPropertyKind kind,const char *label,std::string_view unit={},float low=0,float high=0) {
    AnimationPropertyDescriptor d;d.address.component=v.instanceId();d.address.version=v.type().version;d.address.type=v.type().id;d.address.property=id;d.address.kind=kind;
    AnimationPropertyValue value;std::string error;if(!readAnimationProperty(v,d.address,value,error))return;
    d.name=label;d.unit=unit;d.minimum=low;d.maximum=high;result.push_back(std::move(d));
  };
  for(const auto &p:v.type().triples)append(p.id,p.kind==scene::ComponentTripleKind::LinearColor?AnimationPropertyKind::LinearColor:p.dimensions()==2?AnimationPropertyKind::Vector2:AnimationPropertyKind::Vector3,p.name);
  for(const auto &p:v.type().numbers)append(p.id,AnimationPropertyKind::Number,p.name,p.presentation.unit,p.minimum,p.maximum);
  for(const auto &p:v.type().booleans)append(p.id,AnimationPropertyKind::Boolean,p.name);
  for(const auto &p:v.type().enums)append(p.id,AnimationPropertyKind::Enumeration,p.name);
  for(const auto &p:v.type().references)if(p.animationSafe)append(p.id,AnimationPropertyKind::Object,p.name);
  if(&v.type()==&scene::MeshRenderer::descriptor) {
    const auto &mesh=static_cast<const scene::MeshRenderer&>(v);
    for(u32 slot=0;slot<mesh.slotCount();++slot)if(mesh.slotMaterial(slot).enabled&&mesh.slotAsset(slot).valid()) {
      const auto material=[&](std::string_view id,AnimationPropertyKind kind,const char *label,float low,float high) {
        AnimationPropertyDescriptor d;d.address={AnimationPropertyDomain::MaterialSlot,kind,v.instanceId(),v.type().version,
          std::string(v.type().id),std::string(id),slot,0,mesh.slotAsset(slot)};
        AnimationPropertyValue value;std::string error;if(!readAnimationProperty(v,d.address,value,error))return;
        d.name="Material "+std::to_string(slot+1)+" · "+label;d.minimum=low;d.maximum=high;result.push_back(std::move(d));
      };
      material("material.base_color",AnimationPropertyKind::LinearColor,"Cor",0,1);
      material("material.emission",AnimationPropertyKind::LinearColor,"Emissão",0,1);
      for(const auto &p:scene::meshRendererSlotMaterial)
        if(p.id!="material.base_color.r"&&p.id!="material.base_color.g"&&p.id!="material.base_color.b"&&
           p.id!="material.emission.r"&&p.id!="material.emission.g"&&p.id!="material.emission.b")
          material(p.id,AnimationPropertyKind::Number,p.name,p.minimum,p.maximum);
    }
  }
  return result;
}
bool publishAnimationProperties(SceneGraph &graph,std::span<const AnimationPropertyWrite> writes,const AnimationGuiResolver &gui,std::string &error) {
  error.clear();if(writes.size()>AnimationClipAsset::MaximumTracks)return fail(error,"Lote animado excede orçamento");
  struct Candidate {SceneObject before,after;};std::map<ObjectId,Candidate> candidates;std::set<std::string> fields;
  struct GuiWrite {ui::GuiRuntime *runtime;ui::GuiId node;float before,after;};std::vector<GuiWrite> guiWrites;
  for(const auto &write:writes) {
    const auto *object=graph.find(write.object);if(!object)return fail(error,"Objeto de propriedade removido");
    const auto *component=object->components.findInstance(write.address.component);if(!component||!matches(*component,write.address,error))return false;
    auto canonical=write.address;
    const auto insertField=[&](std::string_view property) {
      canonical.kind=AnimationPropertyKind::Number;canonical.property=property;
      return fields.insert(std::to_string(write.object)+":"+animationPropertyIdentity(canonical)).second;
    };
    const auto *group=write.address.domain==AnimationPropertyDomain::Component?triple(*component,write.address):nullptr;
    if(group) {for(u32 c=0;c<group->dimensions();++c)if(!insertField(group->channels[c]))return fail(error,"Canais sobrepostos escrevem a mesma propriedade");}
    else if(write.address.domain==AnimationPropertyDomain::MaterialSlot&&write.address.kind==AnimationPropertyKind::LinearColor) {
      for(const auto suffix:{".r",".g",".b"})if(!insertField(write.address.property+suffix))return fail(error,"Canais de material sobrepostos");
    } else if(!insertField(write.address.property))return fail(error,"Dois escritores para o mesmo endereço animado");
    if(write.address.domain==AnimationPropertyDomain::GuiNode) {
      const auto *value=std::get_if<float>(&write.value);
      if(&component->type()!=&scene::UiCanvas::descriptor||static_cast<const scene::UiCanvas&>(*component).document!=write.address.resource||!gui||!value||!std::isfinite(*value)||*value<0||*value>1)return fail(error,"Canvas, documento ou opacidade incompatíveis");
      auto *runtime=gui(write.object,write.address.component,write.address.resource);const auto *node=runtime?runtime->document().find(write.address.guiNode):nullptr;
      if(!node)return fail(error,"Nó UI removido ou lease substituído");
      guiWrites.push_back({runtime,node->id,node->opacity,*value});continue;
    }
    auto [entry,created]=candidates.try_emplace(write.object);if(created){entry->second.before=*object;entry->second.after=*object;}
    auto *candidate=entry->second.after.components.editInstance(write.address.component);
    if(!candidate||!writeAnimationProperty(*candidate,write.address,write.value,error))return false;
  }
  for(const auto &[object,candidate]:candidates){(void)object;if(!candidate.after.components.valid())return fail(error,"Composição viola invariantes do componente");}
  std::vector<ObjectId> committed;
  for(const auto &[object,candidate]:candidates) {
    if(!graph.applyEntityValues(object,candidate.after)) {
      for(const auto id:committed)graph.applyEntityValues(id,candidates.at(id).before);
      return fail(error,"Publicação animada recusada; lote restaurado");
    }
    committed.push_back(object);
  }
  for(const auto &write:guiWrites)if(!write.runtime->setOpacity(write.node,write.after)) {
    for(const auto &restore:guiWrites)restore.runtime->setOpacity(restore.node,restore.before);
    for(const auto id:committed)graph.applyEntityValues(id,candidates.at(id).before);
    return fail(error,"Publicação UI recusada; lote restaurado");
  }
  return true;
}
bool readAnimationPropertyTarget(const SceneGraph &graph,ObjectId objectId,const AnimationPropertyAddress &address,
    const AnimationGuiResolver &gui,AnimationPropertyValue &out,std::string &error) {
  const auto *object=graph.find(objectId);const auto *component=object?object->components.findInstance(address.component):nullptr;
  if(!component)return fail(error,"Objeto ou componente de propriedade removido");
  if(!matches(*component,address,error))return false;
  if(address.domain!=AnimationPropertyDomain::GuiNode)return readAnimationProperty(*component,address,out,error);
  if(&component->type()!=&scene::UiCanvas::descriptor||static_cast<const scene::UiCanvas&>(*component).document!=address.resource||!gui)
    return fail(error,"Documento UI substituído ou preview indisponível");
  auto *runtime=gui(objectId,address.component,address.resource);const auto *node=runtime?runtime->document().find(address.guiNode):nullptr;
  if(!node)return fail(error,"GuiNode removido ou lease substituído");
  out=node->opacity;return true;
}
bool AnimationPropertyMixer::apply(SceneGraph &graph,std::span<const AnimationPropertyContribution> samples,std::string &error,const BeforePublication &before) {
  error.clear();std::map<std::string,std::vector<const AnimationPropertyContribution*>> groups;
  if(samples.size()>AnimationClipAsset::MaximumTracks)return fail(error,"Contribuições tipadas excedem orçamento");
  auto nextRest=rest_;
  for(const auto &sample:samples) {
    if(!std::isfinite(sample.weight)||sample.weight<0)return fail(error,"Peso de propriedade inválido");
    const auto key=std::to_string(sample.object)+":"+animationPropertyIdentity(sample.address);
    AnimationPropertyValue current;if(!readAnimationPropertyTarget(graph,sample.object,sample.address,gui_,current,error))return false;
    nextRest.try_emplace(key,current);if(sample.weight>0)groups[key].push_back(&sample);
  }
  if(nextRest.size()>AnimationClipAsset::MaximumTracks)return fail(error,"Repousos animados excedem orçamento; reinicie a sessão");
  std::vector<AnimationPropertyWrite> writes;writes.reserve(groups.size());
  for(const auto &[key,group]:groups) {
    const auto &seed=*group.front();const auto &rest=nextRest.at(key);const auto kind=seed.address.kind;
    AnimationPropertyWrite write;write.object=seed.object;write.address=seed.address;write.value=rest;
    std::set<u32,std::greater<>> layers;for(const auto *sample:group)layers.insert(sample->layer);
    const auto dimensions=animationPropertyComponents(kind);
    if(dimensions) {
      if(dimensions>3)return fail(error,"Quaternion de componente sem consumidor registrado");
      float baseline[3]{},result[3]{};if(!values(rest,kind,baseline))return fail(error,"Repouso tipado incompatível");
      float remaining=1;
      for(const auto layer:layers) {
        double sum=0;float mixed[3]{},add[3]{};
        for(const auto *sample:group)if(sample->layer==layer) {
          float v[3]{};if(!values(sample->value,kind,v))return fail(error,"Contribuição tipada incompatível");
          for(u32 c=0;c<dimensions;++c)if(!std::isfinite(v[c]))return fail(error,"Contribuição não finita");
          if(sample->additive) {
            float reference[3]{};std::copy_n(baseline,dimensions,reference);
            if(sample->reference&&!values(*sample->reference,kind,reference))return fail(error,"Referência aditiva tipada incompatível");
            for(u32 c=0;c<dimensions;++c)add[c]+=(v[c]-reference[c])*sample->weight;
          }
          else {sum+=sample->weight;for(u32 c=0;c<dimensions;++c)mixed[c]+=v[c]*sample->weight;}
        }
        const float take=std::min(float(sum),1.f)*remaining;
        for(u32 c=0;c<dimensions;++c)result[c]+=(sum>0?mixed[c]*take/float(sum):0)+add[c]*remaining;
        remaining-=take;
      }
      for(u32 c=0;c<dimensions;++c)result[c]+=baseline[c]*remaining;
      write.value=fromNumbers(kind,result);
    } else {
      // Highest layer with half coverage selects its strongest discrete input;
      // equal weights use stable input order. No IDs are numerically blended.
      for(const auto layer:layers) {
        const AnimationPropertyContribution *selected=nullptr;double sum=0;
        for(const auto *sample:group)if(sample->layer==layer) {
          if(sample->additive)return fail(error,"Propriedade discreta não admite adição");
          sum+=sample->weight;if(!selected||sample->weight>selected->weight)selected=sample;
        }
        if(sum>=.5&&selected){write.value=selected->value;break;}
      }
    }
    writes.push_back(std::move(write));
  }
  if(before&&!before(writes,nextRest,error))return false;
  if(!publishAnimationProperties(graph,writes,gui_,error))return false;
  rest_=std::move(nextRest);return true;
}
}
