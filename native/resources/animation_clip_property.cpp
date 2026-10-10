#include "resources/animation_clip_asset.h"
#include <algorithm>
#include <cmath>

namespace ae::resources {
namespace {
bool fail(std::string &error,const char *text) {error=text;return false;}
bool continuousValue(AnimationPropertyKind kind,const AnimationPropertyValue &value,float out[4]) {
  switch(kind) {
  case AnimationPropertyKind::Number:if(const auto *v=std::get_if<float>(&value)){out[0]=*v;return true;}break;
  case AnimationPropertyKind::Vector2:if(const auto *v=std::get_if<AnimationPropertyVector2>(&value)){out[0]=v->x;out[1]=v->y;return true;}break;
  case AnimationPropertyKind::Vector3:if(const auto *v=std::get_if<AnimationPropertyVector3>(&value)){out[0]=v->x;out[1]=v->y;out[2]=v->z;return true;}break;
  case AnimationPropertyKind::LinearColor:if(const auto *v=std::get_if<AnimationPropertyColor>(&value)){out[0]=v->r;out[1]=v->g;out[2]=v->b;return true;}break;
  case AnimationPropertyKind::Quaternion:if(const auto *v=std::get_if<AnimationPropertyQuaternion>(&value)){out[0]=v->x;out[1]=v->y;out[2]=v->z;out[3]=v->w;return true;}break;
  default:break;
  }
  return false;
}
}
bool seedAnimationProperty(AnimationPropertyKind kind,const AnimationPropertyValue &value,float time,
    AnimationPropertyData &out,std::string &error) {
  AnimationPropertyData candidate;candidate.kind=kind;
  const auto dimensions=animationPropertyComponents(kind);
  if(dimensions) {
    float numbers[4]{};if(!continuousValue(kind,value,numbers))return fail(error,"Valor inicial incompatível");
    candidate.curves.resize(dimensions);
    for(u32 c=0;c<dimensions;++c) {AnimationCurveKey key;key.id=c+1;key.time=time;key.value=numbers[c];
      key.incoming=key.outgoing=AnimationTangentMode::Linear;candidate.curves[c].keys.push_back(key);}
  } else candidate.keys.push_back({1,time,value});
  if(!validAnimationPropertyData(candidate,time,error))return false;
  out=std::move(candidate);return true;
}
const AnimationPropertyTrack *AnimationClipAsset::propertyTrack(u64 id) const {
  for(const auto &track:properties)if(track.id==id)return &track;
  return nullptr;
}
AnimationPropertyTrack *AnimationClipAsset::propertyTrack(u64 id) {
  for(auto &track:properties)if(track.id==id)return &track;
  return nullptr;
}
bool AnimationClipAsset::addPropertyTrack(AnimationClipBinding binding,AnimationPropertyAddress address,
    AnimationPropertyData data,u64 &created,std::string &error) {
  error.clear();if(!valid(&error))return false;
  if(!validAnimationPropertyAddress(address)||address.kind!=data.kind)return fail(error,"Endereço ou tipo da propriedade inválidos");
  auto candidate=*this;
  for(auto &curve:data.curves)for(auto &key:curve.keys)key.id=candidate.nextId++;
  for(auto &key:data.keys)key.id=candidate.nextId++;
  if(!validAnimationPropertyData(data,duration,error))return false;
  const auto found=std::find_if(candidate.bindings.begin(),candidate.bindings.end(),[&](const auto &b){return b.path==binding.path;});
  if(found!=candidate.bindings.end()) {
    if(found->sourceNode!=binding.sourceNode)return fail(error,"Outro nó ocupa este binding");
    binding.id=found->id;
  } else {binding.id=candidate.nextId++;candidate.bindings.push_back(binding);}
  AnimationPropertyTrack track;track.id=candidate.nextId++;track.binding=binding.id;track.address=std::move(address);track.data=std::move(data);
  const auto id=track.id;candidate.properties.push_back(std::move(track));
  if(!candidate.valid(&error))return false;
  *this=std::move(candidate);created=id;return true;
}
bool AnimationClipAsset::putPropertyValue(u64 id,float time,const AnimationPropertyValue &value,std::string &error) {
  error.clear();if(!valid(&error)||!std::isfinite(time)||time<0||time>duration)return fail(error,"Tempo de propriedade fora do clipe");
  auto candidate=*this;auto *track=candidate.propertyTrack(id);
  if(!track)return fail(error,"Canal tipado ausente");
  const auto dimensions=animationPropertyComponents(track->data.kind);
  if(dimensions) {
    float values[4]{};if(!continuousValue(track->data.kind,value,values))return fail(error,"Valor incompatível com a propriedade");
    for(u32 c=0;c<dimensions;++c) {
      AnimationCurveKey key;key.time=time;key.incoming=key.outgoing=AnimationTangentMode::Linear;
      const auto &keys=track->data.curves[c].keys;const auto found=std::find_if(keys.begin(),keys.end(),[&](const auto &k){return k.time==time;});
      if(found!=keys.end())key=*found;
      key.value=values[c];u64 created=0;
      if(!putAnimationCurveKey(track->data.curves[c],key,candidate.nextId,created))return fail(error,"Chave tipada não finita, sem precisão ou acima do orçamento");
    }
  } else {
    auto &keys=track->data.keys;const auto found=std::lower_bound(keys.begin(),keys.end(),time,[](const auto &key,float t){return key.time<t;});
    if(found!=keys.end()&&found->time==time)found->value=value;
    else keys.insert(found,{candidate.nextId++,time,value});
  }
  if(!candidate.valid(&error))return false;
  *this=std::move(candidate);return true;
}
bool AnimationClipAsset::removePropertyTrack(u64 id,std::string &error) {
  error.clear();if(!valid(&error))return false;
  if(properties.size()+tracks.size()==1||!propertyTrack(id))return fail(error,"Preserve ao menos um canal no clipe");
  auto candidate=*this;const auto binding=candidate.propertyTrack(id)->binding;
  std::erase_if(candidate.properties,[&](const auto &track){return track.id==id;});
  if(std::none_of(candidate.tracks.begin(),candidate.tracks.end(),[&](const auto &track){return track.binding==binding;})&&
     std::none_of(candidate.properties.begin(),candidate.properties.end(),[&](const auto &track){return track.binding==binding;})&&
     std::none_of(candidate.drivers.inputs.begin(),candidate.drivers.inputs.end(),[&](const auto &input){return input.binding==binding;})&&
     std::none_of(candidate.drivers.targets.begin(),candidate.drivers.targets.end(),[&](const auto &output){return output.binding==binding;}))
    std::erase_if(candidate.bindings,[&](const auto &b){return b.id==binding;});
  if(!candidate.valid(&error))return false;
  *this=std::move(candidate);return true;
}
bool copyAnimationPropertyRange(AnimationClipAsset &destination,const AnimationClipAsset &excerpt,float at,std::string &error) {
  auto candidate=destination;
  for(const auto &source:excerpt.properties) {
    auto *target=candidate.propertyTrack(source.id);
    if(!target||target->binding!=source.binding||target->address!=source.address||target->data.leftContinuous!=source.data.leftContinuous)
      return fail(error,"Destino tipado incompatível com o trecho");
    for(u32 c=0;c<source.data.curves.size();++c)for(auto key:source.data.curves[c].keys) {
      key.id=0;key.time=static_cast<float>(double(key.time)+at);u64 result=0;
      if(!putAnimationCurveKey(target->data.curves[c],key,candidate.nextId,result))return fail(error,"Colagem tipada excede precisão ou orçamento");
    }
    for(const auto &key:source.data.keys)if(!candidate.putPropertyValue(source.id,static_cast<float>(double(key.time)+at),key.value,error))return false;
  }
  if(!candidate.valid(&error))return false;
  destination=std::move(candidate);return true;
}
bool animationPropertyValuesNear(const AnimationPropertyValue &a,const AnimationPropertyValue &b,double tolerance) {
  if(a.index()!=b.index()||!std::isfinite(tolerance)||tolerance<0)return false;
  const auto near=[&](double x,double y){return std::isfinite(x)&&std::isfinite(y)&&std::abs(x-y)<=tolerance;};
  switch(a.index()) {
  case 0:return near(std::get<float>(a),std::get<float>(b));
  case 1:{const auto x=std::get<AnimationPropertyVector2>(a),y=std::get<AnimationPropertyVector2>(b);return near(x.x,y.x)&&near(x.y,y.y);}
  case 2:{const auto x=std::get<AnimationPropertyVector3>(a),y=std::get<AnimationPropertyVector3>(b);return near(x.x,y.x)&&near(x.y,y.y)&&near(x.z,y.z);}
  case 3:{const auto x=std::get<AnimationPropertyColor>(a),y=std::get<AnimationPropertyColor>(b);return near(x.r,y.r)&&near(x.g,y.g)&&near(x.b,y.b);}
  case 4:{const auto x=std::get<AnimationPropertyQuaternion>(a),y=std::get<AnimationPropertyQuaternion>(b);
    const double dot=double(x.x)*y.x+double(x.y)*y.y+double(x.z)*y.z+double(x.w)*y.w;
    return std::isfinite(dot)&&2*std::acos(std::clamp(std::abs(dot),0.0,1.0))*180/3.141592653589793<=tolerance;}
  case 5:return std::get<bool>(a)==std::get<bool>(b);
  case 6:return std::get<u32>(a)==std::get<u32>(b);
  case 7:return std::get<AnimationPropertyObject>(a).id==std::get<AnimationPropertyObject>(b).id;
  case 8:return std::get<AssetGuid>(a)==std::get<AssetGuid>(b);
  }
  return false;
}
}
