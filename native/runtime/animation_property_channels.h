#pragma once
#include "runtime/animation_property_binding.h"

namespace ae::runtime {
// Map a numeric channel to a grouped authoring address through the actual
// component schema. A driver can override one channel after the clip without
// publishing overlapping scalar/group writers or observing the previous frame.
inline int animationPropertyChannel(const scene::ComponentValue &component,
    const resources::AnimationPropertyAddress &group,const resources::AnimationPropertyAddress &scalar) {
  using namespace resources;
  if(scalar.kind!=AnimationPropertyKind::Number||group.kind==AnimationPropertyKind::Number||
     group.component!=scalar.component||group.type!=scalar.type||group.version!=scalar.version||
     group.domain!=scalar.domain||group.slot!=scalar.slot||group.resource!=scalar.resource||group.guiNode!=scalar.guiNode)return -1;
  if(group.domain==AnimationPropertyDomain::MaterialSlot&&group.kind==AnimationPropertyKind::LinearColor) {
    for(int c=0;c<3;++c)if(scalar.property==group.property+(c==0?".r":c==1?".g":".b"))return c;
  } else if(group.domain==AnimationPropertyDomain::Component) {
    for(const auto &triple:component.type().triples)if(triple.id==group.property)
      for(u32 c=0;c<triple.dimensions();++c)if(triple.channels[c]==scalar.property)return int(c);
  }
  return -1;
}
inline bool animationPropertyChannelValue(resources::AnimationPropertyValue &value,int channel,float &number,bool write=false) {
  using namespace resources;
  if(channel<0)return false;
  float *field=nullptr;
  if(auto *v=std::get_if<AnimationPropertyVector2>(&value)){if(channel<2)field=channel==0?&v->x:&v->y;}
  else if(auto *v=std::get_if<AnimationPropertyVector3>(&value)){if(channel<3)field=channel==0?&v->x:channel==1?&v->y:&v->z;}
  else if(auto *v=std::get_if<AnimationPropertyColor>(&value)){if(channel<3)field=channel==0?&v->r:channel==1?&v->g:&v->b;}
  if(!field)return false;
  if(write)*field=number;else number=*field;
  return true;
}
}
