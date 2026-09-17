#pragma once
#include "scene/components.h"
#include <variant>

namespace ae::scene {
struct ObjectReference {u64 id=0;};
using ComponentPropertyValue = std::variant<float,bool,u32,ObjectReference>;
enum class ComponentPropertyStatus {
  Applied, MissingComponent, UnknownProperty, AmbiguousProperty, TypeMismatch, InvalidValue
};

// Caller owns the collection and serializes mutations with its scene thread.
// This API changes an existing component only. Editor history wraps the same
// operation; runtime consumers can use it without an editor or UI dependency.
inline ComponentPropertyStatus setComponentProperty(Components &components,
    std::string_view typeId,std::string_view propertyId,const ComponentPropertyValue &value,u64 instanceId=0) {
  const auto *source=instanceId?components.findInstance(instanceId):components.find(typeId);
  if(source&&source->type().id!=typeId) return ComponentPropertyStatus::MissingComponent;
  if(!instanceId&&source&&source->type().allowMultiple) {
    u32 count=0;for(usize i=0;i<components.size();++i) if(components.at(i)->type().id==typeId) ++count;
    if(count>1) return ComponentPropertyStatus::AmbiguousProperty;
  }
  if(!source) return ComponentPropertyStatus::MissingComponent;
  if(propertyId.empty()) return ComponentPropertyStatus::UnknownProperty;
  const ComponentNumber *number=nullptr;
  const ComponentBoolean *boolean=nullptr;
  const ComponentEnum *enumeration=nullptr;
  const ComponentObjectReference *reference=nullptr;
  u32 matches=0;
  for(const auto &p:source->type().numbers) if(p.id==propertyId) {number=&p;++matches;}
  for(const auto &p:source->type().booleans) if(p.id==propertyId) {boolean=&p;++matches;}
  for(const auto &p:source->type().enums) if(p.id==propertyId) {enumeration=&p;++matches;}
  for(const auto &p:source->type().references) if(p.id==propertyId) {reference=&p;++matches;}
  if(!matches) return ComponentPropertyStatus::UnknownProperty;
  if(matches!=1) return ComponentPropertyStatus::AmbiguousProperty;
  const auto &presentation=number?number->presentation:boolean?boolean->presentation:
      enumeration?enumeration->presentation:reference->presentation;
  if(!presentation.isEditable(*source)) return ComponentPropertyStatus::InvalidValue;
  if((number && !std::holds_alternative<float>(value)) ||
     (boolean && !std::holds_alternative<bool>(value)) ||
     (enumeration && !std::holds_alternative<u32>(value)) ||
     (reference && !std::holds_alternative<ObjectReference>(value))) return ComponentPropertyStatus::TypeMismatch;
  if(number) {
    const float v=std::get<float>(value);
    if(!number->write || !std::isfinite(v) || v<number->minimum || v>number->maximum)
      return ComponentPropertyStatus::InvalidValue;
  } else if(enumeration) {
    bool found=false;
    for(const auto &option:enumeration->options) if(option.value==std::get<u32>(value)) found=true;
    if(!found || !enumeration->write) return ComponentPropertyStatus::InvalidValue;
  } else if(reference ? !reference->write : !boolean->write) return ComponentPropertyStatus::InvalidValue;
  auto candidate=source->clone();
  if(!candidate || &candidate->type()!=&source->type()) return ComponentPropertyStatus::InvalidValue;
  if(number) {
    auto *destination=number->write(*candidate);
    if(!destination) return ComponentPropertyStatus::InvalidValue;
    *destination=std::get<float>(value);
  } else if(enumeration) enumeration->write(*candidate,std::get<u32>(value));
  else if(reference) reference->write(*candidate,std::get<ObjectReference>(value).id);
  else boolean->write(*candidate,std::get<bool>(value));
  // Validate cross-field invariants before replacing any live state.
  if(!candidate->valid() || !components.replaceInstance(source->instanceId(),*candidate)) return ComponentPropertyStatus::InvalidValue;
  return ComponentPropertyStatus::Applied;
}
// Resolves persistent channel IDs and validates the completed tuple once.
// Useful for direction vectors whose intermediate scalar edits can be invalid.
inline ComponentPropertyStatus setComponentTriple(Components &components,std::string_view typeId,
    std::string_view propertyId,const float (&values)[3],u64 instanceId=0) {
  const auto *source=instanceId?components.findInstance(instanceId):components.find(typeId);
  if(!source||source->type().id!=typeId) return ComponentPropertyStatus::MissingComponent;
  if(!instanceId) {
    u32 count=0;for(usize i=0;i<components.size();++i) if(components.at(i)->type().id==typeId) ++count;
    if(count>1) return ComponentPropertyStatus::AmbiguousProperty;
  }
  const ComponentTriple *triple=nullptr;
  for(const auto &p:source->type().triples) if(p.id==propertyId) {
    if(triple) return ComponentPropertyStatus::AmbiguousProperty;
    triple=&p;
  }
  if(!triple) return ComponentPropertyStatus::UnknownProperty;
  auto candidate=source->clone();
  if(!candidate || &candidate->type()!=&source->type()) return ComponentPropertyStatus::InvalidValue;
  for(u32 axis=0;axis<3;++axis) {
    const ComponentNumber *channel=nullptr;
    for(const auto &p:source->type().numbers) if(p.id==triple->channels[axis]) {
      if(channel) return ComponentPropertyStatus::AmbiguousProperty;
      channel=&p;
    }
    if(!channel) return ComponentPropertyStatus::UnknownProperty;
    if(!channel->write || !channel->presentation.isEditable(*source) || !std::isfinite(values[axis]) ||
       values[axis]<channel->minimum || values[axis]>channel->maximum) return ComponentPropertyStatus::InvalidValue;
    auto *destination=channel->write(*candidate);
    if(!destination) return ComponentPropertyStatus::InvalidValue;
    *destination=values[axis];
  }
  if(!candidate->valid() || !components.replaceInstance(source->instanceId(),*candidate)) return ComponentPropertyStatus::InvalidValue;
  return ComponentPropertyStatus::Applied;
}

}
