#include "resources/animation_property.h"
#include "resources/skeletal_animation.h"
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace ae::resources {
namespace {
bool identifier(std::string_view text) {
  if(text.empty()||text.size()>256)return false;
  for(const unsigned char c:text)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
      (c>='0'&&c<='9')||c=='.'||c=='_'))return false;
  return true;
}
bool discreteValue(AnimationPropertyKind kind,const AnimationPropertyValue &value) {
  switch(kind) {
  case AnimationPropertyKind::Boolean:return std::holds_alternative<bool>(value);
  case AnimationPropertyKind::Enumeration:return std::holds_alternative<u32>(value);
  case AnimationPropertyKind::Object:return std::holds_alternative<AnimationPropertyObject>(value);
  case AnimationPropertyKind::Resource:return std::holds_alternative<AssetGuid>(value);
  default:return false;
  }
}
bool fail(std::string &error,const char *message) {error=message;return false;}
}
u32 animationPropertyComponents(AnimationPropertyKind kind) {
  switch(kind) {
  case AnimationPropertyKind::Number:return 1;
  case AnimationPropertyKind::Vector2:return 2;
  case AnimationPropertyKind::Vector3:case AnimationPropertyKind::LinearColor:return 3;
  case AnimationPropertyKind::Quaternion:return 4;
  default:return 0;
  }
}
bool validAnimationPropertyAddress(const AnimationPropertyAddress &a) {
  if(static_cast<u8>(a.kind)>8||static_cast<u8>(a.domain)>2||!a.component||
     a.component==std::numeric_limits<u64>::max()||!a.version||!identifier(a.type)||!identifier(a.property))return false;
  switch(a.domain) {
  case AnimationPropertyDomain::Component:return !a.slot&&!a.guiNode&&!a.resource.valid();
  case AnimationPropertyDomain::MaterialSlot:
    return a.type=="astra.render.mesh"&&a.slot<4096&&!a.guiNode&&a.resource.valid()&&
      (a.kind==AnimationPropertyKind::Number||a.kind==AnimationPropertyKind::LinearColor);
  case AnimationPropertyDomain::GuiNode:
    return a.type=="astra.ui.canvas"&&a.kind==AnimationPropertyKind::Number&&a.property=="opacity"&&
      a.guiNode&&!a.slot&&a.resource.valid();
  }
  return false;
}
bool validAnimationPropertyData(const AnimationPropertyData &data,float duration,std::string &error) {
  error.clear();
  if(static_cast<u8>(data.kind)>8||!std::isfinite(duration)||duration<0||duration>86400)
    return fail(error,"Tipo ou duração da propriedade inválidos");
  constexpr usize maximumKeys=262144;
  std::unordered_set<u64> ids;usize total=0;
  const u32 dimensions=animationPropertyComponents(data.kind);
  if(dimensions) {
    if(data.curves.size()!=dimensions||!data.keys.empty()||data.leftContinuous)return fail(error,"Dimensão ou representação contínua inválida");
    for(const auto &curve:data.curves) {
      if(curve.keys.size()>maximumKeys-total||!validAnimationCurve(curve)||curve.keys.front().time<0||curve.keys.back().time>duration)
        return fail(error,"Curva de propriedade inválida ou fora do orçamento");
      total+=curve.keys.size();
      for(const auto &key:curve.keys)if(!key.id||!ids.insert(key.id).second)return fail(error,"Identidade de chave duplicada ou ausente");
    }
    if(data.kind==AnimationPropertyKind::Quaternion) {
      AnimationChannel quaternion;quaternion.path=AnimationPath::Rotation;quaternion.curves=data.curves;
      if(!validAnimationChannel(quaternion))return fail(error,"Quaternion exige poses finitas, não nulas e sincronizadas");
    }
  } else {
    if(!data.curves.empty()||data.keys.empty()||data.keys.size()>maximumKeys)return fail(error,"Representação discreta inválida ou fora do orçamento");
    float previous=-1;
    for(const auto &key:data.keys) {
      if(!key.id||!ids.insert(key.id).second||!std::isfinite(key.time)||key.time<0||key.time>duration||
         key.time<=previous||!discreteValue(data.kind,key.value))return fail(error,"Chave discreta inválida, duplicada ou com tipo incompatível");
      previous=key.time;
    }
  }
  return true;
}
bool compileAnimationProperty(const AnimationPropertyData &data,float duration,CompiledAnimationProperty &out,std::string &error) {
  if(!validAnimationPropertyData(data,duration,error))return false;
  CompiledAnimationProperty value;value.kind_=data.kind;value.leftContinuous_=data.leftContinuous;
  if(data.kind==AnimationPropertyKind::Quaternion) {
    auto channel=std::make_shared<AnimationChannel>();channel->path=AnimationPath::Rotation;channel->curves=data.curves;
    value.quaternion_=std::move(channel);
  } else {value.curves_=data.curves;value.keys_=data.keys;}
  out=std::move(value);return true;
}
bool CompiledAnimationProperty::sample(float time,AnimationPropertyValue &out) const {
  if(!std::isfinite(time))return false;
  const u32 dimensions=animationPropertyComponents(kind_);
  if(!dimensions) {
    if(keys_.empty())return false;
    const auto end=leftContinuous_?std::lower_bound(keys_.begin(),keys_.end(),time,[](const auto &key,float t){return key.time<t;}):
      std::upper_bound(keys_.begin(),keys_.end(),time,[](float t,const auto &key){return t<key.time;});
    out=(leftContinuous_?(end==keys_.end()?keys_.back():*end):(end==keys_.begin()?*end:*(end-1))).value;return true;
  }
  float values[4]{};
  if(kind_==AnimationPropertyKind::Quaternion) {
    if(!quaternion_||!sampleValidatedAnimationChannel(*quaternion_,time,values))return false;
  } else {
    if(curves_.size()!=dimensions)return false;
    for(u32 i=0;i<dimensions;++i) {
      AnimationCurveSample value;if(!sampleValidatedAnimationCurve(curves_[i],time,value))return false;
      values[i]=static_cast<float>(value.value);if(!std::isfinite(values[i]))return false;
    }
  }
  switch(kind_) {
  case AnimationPropertyKind::Number:out=values[0];break;
  case AnimationPropertyKind::Vector2:out=AnimationPropertyVector2{values[0],values[1]};break;
  case AnimationPropertyKind::Vector3:out=AnimationPropertyVector3{values[0],values[1],values[2]};break;
  case AnimationPropertyKind::LinearColor:out=AnimationPropertyColor{values[0],values[1],values[2]};break;
  case AnimationPropertyKind::Quaternion:out=AnimationPropertyQuaternion{values[0],values[1],values[2],values[3]};break;
  default:return false;
  }
  return true;
}
std::string animationPropertyIdentity(const AnimationPropertyAddress &a) {
  std::ostringstream out;out<<static_cast<u32>(a.domain)<<':'<<a.component<<':'<<a.version<<':'<<a.type<<':'<<a.property<<':'
    <<static_cast<u32>(a.kind)<<':'<<a.slot<<':'<<a.guiNode<<':'<<a.resource.text();return out.str();
}
namespace {
void writeCurve(std::ostream &out,const AnimationCurve &curve) {
  out<<curve.keys.size()<<'\n';
  for(const auto &k:curve.keys)out<<k.id<<' '<<k.time<<' '<<k.value<<' '<<k.inSlope<<' '<<k.outSlope<<' '<<k.inWeight<<' '<<k.outWeight<<' '
    <<static_cast<u32>(k.incoming)<<' '<<static_cast<u32>(k.outgoing)<<' '<<k.broken<<' '<<k.weightedIn<<' '<<k.weightedOut<<'\n';
}
bool readCurve(std::istream &in,AnimationCurve &curve,usize &budget) {
  usize count=0;if(!(in>>count)||!count||count>budget)return false;budget-=count;curve.keys.resize(count);
  for(auto &k:curve.keys) {
    u32 a=0,b=0,broken=0,wi=0,wo=0;
    if(!(in>>k.id>>k.time>>k.value>>k.inSlope>>k.outSlope>>k.inWeight>>k.outWeight>>a>>b>>broken>>wi>>wo)||a>6||b>6||broken>1||wi>1||wo>1)return false;
    k.incoming=static_cast<AnimationTangentMode>(a);k.outgoing=static_cast<AnimationTangentMode>(b);k.broken=broken;k.weightedIn=wi;k.weightedOut=wo;
  }
  return true;
}
}
void writeAnimationPropertyTrack(std::ostream &out,const AnimationPropertyTrack &track) {
  const auto &a=track.address;const auto &d=track.data;
  out<<track.id<<' '<<track.binding<<' '<<static_cast<u32>(a.domain)<<' '<<static_cast<u32>(a.kind)<<' '<<a.component<<' '<<a.version<<' '
    <<std::quoted(a.type)<<' '<<std::quoted(a.property)<<' '<<a.slot<<' '<<a.guiNode<<' '<<(a.resource.valid()?a.resource.text():"-")<<' '
    <<d.leftContinuous<<' '<<d.curves.size()<<' '<<d.keys.size()<<'\n';
  for(const auto &curve:d.curves)writeCurve(out,curve);
  for(const auto &key:d.keys) {
    out<<key.id<<' '<<key.time<<' ';
    switch(d.kind) {
    case AnimationPropertyKind::Boolean:out<<std::get<bool>(key.value);break;
    case AnimationPropertyKind::Enumeration:out<<std::get<u32>(key.value);break;
    case AnimationPropertyKind::Object:out<<std::get<AnimationPropertyObject>(key.value).id;break;
    case AnimationPropertyKind::Resource:{const auto guid=std::get<AssetGuid>(key.value);out<<(guid.valid()?guid.text():"-");break;}
    default:out.setstate(std::ios::failbit);return;
    }
    out<<'\n';
  }
}
bool readAnimationPropertyTrack(std::istream &in,AnimationPropertyTrack &out,usize &keyBudget,std::string &error) {
  AnimationPropertyTrack track;auto &a=track.address;auto &d=track.data;u32 domain=0,kind=0,left=0;usize curves=0,keys=0;std::string guid;
  if(!(in>>track.id>>track.binding>>domain>>kind>>a.component>>a.version>>std::quoted(a.type)>>std::quoted(a.property)>>a.slot>>a.guiNode>>guid>>left>>curves>>keys)||
      domain>2||kind>8||left>1||curves>4||keys>keyBudget||(guid!="-"&&!AssetGuid::parse(guid,a.resource)))return fail(error,"Canal tipado incompleto ou acima do orçamento");
  a.domain=static_cast<AnimationPropertyDomain>(domain);a.kind=d.kind=static_cast<AnimationPropertyKind>(kind);d.leftContinuous=left;
  if(!validAnimationPropertyAddress(a))return fail(error,"Endereço tipado inválido");
  const auto dimensions=animationPropertyComponents(d.kind);
  if(curves!=dimensions||(dimensions?keys!=0:keys==0)||(!dimensions&&left>1))return fail(error,"Representação tipada incompatível");
  d.curves.resize(curves);for(auto &curve:d.curves)if(!readCurve(in,curve,keyBudget))return fail(error,"Curva tipada incompleta");
  keyBudget-=keys;d.keys.resize(keys);
  for(auto &key:d.keys) {
    if(!(in>>key.id>>key.time))return fail(error,"Chave tipada incompleta");
    switch(d.kind) {
    case AnimationPropertyKind::Boolean:{u32 value=0;if(!(in>>value)||value>1)return fail(error,"Booleano inválido");key.value=bool(value);break;}
    case AnimationPropertyKind::Enumeration:{u32 value=0;if(!(in>>value))return fail(error,"Enum inválido");key.value=value;break;}
    case AnimationPropertyKind::Object:{u64 value=0;if(!(in>>value))return fail(error,"Referência inválida");key.value=AnimationPropertyObject{value};break;}
    case AnimationPropertyKind::Resource:{AssetGuid value;if(!(in>>guid)||(guid!="-"&&!AssetGuid::parse(guid,value)))return fail(error,"Recurso inválido");key.value=value;break;}
    default:return fail(error,"Chave discreta incompatível");
    }
  }
  out=std::move(track);return true;
}
bool retimeAnimationProperty(AnimationPropertyData &data,double scale,double offset,std::string &error) {
  if(!std::isfinite(scale)||scale<=0||!std::isfinite(offset))return fail(error,"Transformação de tempo tipada inválida");
  auto value=data;
  for(auto &curve:value.curves)if(!retimeAnimationCurve(curve,scale,offset))return fail(error,"Tempo tipado excede a precisão");
  float previous=-1;
  for(auto &key:value.keys) {
    const double t=double(key.time)*scale+offset;key.time=static_cast<float>(t);
    if(!std::isfinite(t)||key.time<0||key.time>86400||key.time<=previous)return fail(error,"Tempo discreto excede a precisão");
    previous=key.time;
  }
  data=std::move(value);return true;
}
bool reverseAnimationProperty(AnimationPropertyData &data,float duration,std::string &error) {
  auto value=data;
  for(auto &curve:value.curves)if(!reverseAnimationCurve(curve,0,duration))return fail(error,"Inversão tipada excede a precisão");
  if(!value.keys.empty()) {
    for(auto &key:value.keys)key.time=duration-key.time;
    std::reverse(value.keys.begin(),value.keys.end());value.leftContinuous=!value.leftContinuous;
    float previous=-1;for(const auto &key:value.keys) {if(key.time<=previous)return fail(error,"Inversão discreta colide chaves");previous=key.time;}
  }
  data=std::move(value);return true;
}
bool cropAnimationProperty(AnimationPropertyData &data,float start,float end,u64 &nextId,std::string &error) {
  if(!std::isfinite(start)||!std::isfinite(end)||start<0||end<start)return fail(error,"Corte tipado inválido");
  auto value=data;u64 frontier=nextId;
  if(!value.keys.empty()) {
    CompiledAnimationProperty source;const float duration=std::max(end,value.keys.back().time);
    if(!compileAnimationProperty(data,duration,source,error))return false;
    AnimationPropertyValue first,last;if(!source.sample(start,first)||!source.sample(end,last))return fail(error,"Limite discreto inválido");
    std::erase_if(value.keys,[&](const auto &key){return key.time<start||key.time>end;});
    const auto boundary=[&](float time,const AnimationPropertyValue &v) {
      if(std::none_of(value.keys.begin(),value.keys.end(),[&](const auto &k){return k.time==time;}))value.keys.push_back({frontier++,time,v});
    };
    boundary(start,first);if(end>start)boundary(end,last);
    std::sort(value.keys.begin(),value.keys.end(),[](const auto &a,const auto &b){return a.time<b.time;});
    for(auto &key:value.keys)key.time-=start;
  } else {
    for(auto &curve:value.curves)if(!cropAnimationCurve(curve,start,end,frontier))return fail(error,"Corte tipado não preserva handles");
    if(data.kind==AnimationPropertyKind::Quaternion) {
      AnimationChannel source;source.path=AnimationPath::Rotation;source.curves=data.curves;
      for(const float boundary:{start,end}) {
        const auto &original=data.curves.front().keys;
        if(boundary<=original.front().time||boundary>=original.back().time)continue;
        const auto upper=std::upper_bound(original.begin(),original.end(),boundary,[](float t,const auto &k){return t<k.time;});
        const usize index=static_cast<usize>(upper-original.begin())-1;
        bool linear=true;for(const auto &curve:data.curves)linear&=curve.keys[index].outgoing==AnimationTangentMode::Linear&&curve.keys[index+1].incoming==AnimationTangentMode::Linear;
        if(!linear)continue;
        float q[4];if(!sampleValidatedAnimationChannel(source,boundary,q))return fail(error,"Limite quaternion inválido");
        for(u32 c=0;c<4;++c)for(auto &key:value.curves[c].keys)if(key.time==boundary-start)key.value=q[c];
      }
      for(usize i=0;i+1<value.curves.front().keys.size();++i) {
        const float middle=start+(value.curves[0].keys[i].time+value.curves[0].keys[i+1].time)*.5f;
        const auto &original=data.curves.front().keys;const auto upper=std::upper_bound(original.begin(),original.end(),middle,[](float t,const auto &k){return t<k.time;});
        if(upper==original.begin()||upper==original.end())continue;
        const usize index=static_cast<usize>(upper-original.begin())-1;bool linear=true;
        for(const auto &curve:data.curves)linear&=curve.keys[index].outgoing==AnimationTangentMode::Linear&&curve.keys[index+1].incoming==AnimationTangentMode::Linear;
        if(linear)for(auto &curve:value.curves) {curve.keys[i].outgoing=curve.keys[i+1].incoming=AnimationTangentMode::Linear;curve.keys[i].broken=curve.keys[i+1].broken=true;}
      }
    }
  }
  if(!validAnimationPropertyData(value,end-start,error)||frontier<nextId||!frontier)return false;
  data=std::move(value);nextId=frontier;return true;
}
}
