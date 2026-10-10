#pragma once
#include "resources/animation_property.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace ae::editor {
// ABI 12 retains integer/object/resource identities without float conversion.
// Unused storage is canonical zero; strings are bounded, terminated UTF-8.
struct AnimationAuthorProperty {
  u32 domain=0,kind=0,version=0,slot=0;
  u64 component=0;
  resources::AssetGuid resource{};
  u32 guiNode=0,reserved=0;
  char type[128]{},property[128]{};
};
struct AnimationAuthorPropertyValue {
  u32 kind=0,reserved=0;
  float values[4]{};
  u64 object=0;
  resources::AssetGuid resource{};
  u32 option=0,reserved2=0;
};
static_assert(sizeof(AnimationAuthorProperty)==304);
static_assert(sizeof(AnimationAuthorPropertyValue)==56);
struct AnimationAuthorDriverParameter {
  AnimationAuthorProperty property;
  u64 parameter=0;
  resources::AssetGuid controller{};
};
static_assert(sizeof(AnimationAuthorDriverParameter)==328);
inline bool authorProperty(const resources::AnimationPropertyAddress &source,AnimationAuthorProperty &out) {
  if(source.type.size()>=sizeof(out.type)||source.property.size()>=sizeof(out.property))return false;
  AnimationAuthorProperty wire;wire.domain=u32(source.domain);wire.kind=u32(source.kind);wire.version=source.version;
  wire.slot=source.slot;wire.component=source.component;wire.resource=source.resource;wire.guiNode=source.guiNode;
  std::copy(source.type.begin(),source.type.end(),wire.type);std::copy(source.property.begin(),source.property.end(),wire.property);
  out=wire;return true;
}
inline bool authorProperty(const AnimationAuthorProperty &wire,resources::AnimationPropertyAddress &out) {
  if(wire.reserved||wire.domain>2||wire.kind>8)return false;
  const auto length=[](const char (&s)[128]){const auto *end=static_cast<const char*>(std::memchr(s,0,128));return end?usize(end-s):usize(128);};
  const auto type=length(wire.type),property=length(wire.property);if(type==128||property==128)return false;
  resources::AnimationPropertyAddress value;value.domain=static_cast<resources::AnimationPropertyDomain>(wire.domain);
  value.kind=static_cast<resources::AnimationPropertyKind>(wire.kind);value.version=wire.version;value.slot=wire.slot;
  value.component=wire.component;value.resource=wire.resource;value.guiNode=wire.guiNode;
  value.type.assign(wire.type,type);value.property.assign(wire.property,property);
  if(!resources::validAnimationPropertyAddress(value))return false;
  out=std::move(value);return true;
}
inline bool authorPropertyValue(const AnimationAuthorPropertyValue &wire,resources::AnimationPropertyValue &out) {
  using namespace resources;
  if(wire.reserved||wire.reserved2||wire.kind>8)return false;
  for(float v:wire.values)if(!std::isfinite(v))return false;
  const auto kind=static_cast<AnimationPropertyKind>(wire.kind);const auto n=animationPropertyComponents(kind);
  for(u32 i=n;i<4;++i)if(wire.values[i]!=0)return false;
  if(kind!=AnimationPropertyKind::Object&&wire.object)return false;
  if(kind!=AnimationPropertyKind::Resource&&wire.resource.valid())return false;
  if(kind!=AnimationPropertyKind::Boolean&&kind!=AnimationPropertyKind::Enumeration&&wire.option)return false;
  switch(kind) {
  case AnimationPropertyKind::Number:out=wire.values[0];break;
  case AnimationPropertyKind::Vector2:out=AnimationPropertyVector2{wire.values[0],wire.values[1]};break;
  case AnimationPropertyKind::Vector3:out=AnimationPropertyVector3{wire.values[0],wire.values[1],wire.values[2]};break;
  case AnimationPropertyKind::LinearColor:out=AnimationPropertyColor{wire.values[0],wire.values[1],wire.values[2]};break;
  case AnimationPropertyKind::Quaternion:out=AnimationPropertyQuaternion{wire.values[0],wire.values[1],wire.values[2],wire.values[3]};break;
  case AnimationPropertyKind::Boolean:if(wire.option>1)return false;out=wire.option!=0;break;
  case AnimationPropertyKind::Enumeration:out=wire.option;break;
  case AnimationPropertyKind::Object:out=AnimationPropertyObject{wire.object};break;
  case AnimationPropertyKind::Resource:out=wire.resource;break;
  }
  return true;
}
inline AnimationAuthorPropertyValue authorPropertyValue(const resources::AnimationPropertyValue &value) {
  using namespace resources;AnimationAuthorPropertyValue wire;wire.kind=static_cast<u32>(value.index());
  if(const auto *v=std::get_if<float>(&value))wire.values[0]=*v;
  else if(const auto *v=std::get_if<AnimationPropertyVector2>(&value)){wire.values[0]=v->x;wire.values[1]=v->y;}
  else if(const auto *v=std::get_if<AnimationPropertyVector3>(&value)){wire.values[0]=v->x;wire.values[1]=v->y;wire.values[2]=v->z;}
  else if(const auto *v=std::get_if<AnimationPropertyColor>(&value)){wire.values[0]=v->r;wire.values[1]=v->g;wire.values[2]=v->b;}
  else if(const auto *v=std::get_if<AnimationPropertyQuaternion>(&value)){wire.values[0]=v->x;wire.values[1]=v->y;wire.values[2]=v->z;wire.values[3]=v->w;}
  else if(const auto *v=std::get_if<bool>(&value))wire.option=*v?1:0;
  else if(const auto *v=std::get_if<u32>(&value))wire.option=*v;
  else if(const auto *v=std::get_if<AnimationPropertyObject>(&value))wire.object=v->id;
  else if(const auto *v=std::get_if<AssetGuid>(&value))wire.resource=*v;
  return wire;
}
}
