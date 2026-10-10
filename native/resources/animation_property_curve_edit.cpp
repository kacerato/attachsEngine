#include "resources/animation_property_curve_edit.h"
#include <algorithm>
#include <cmath>

namespace ae::resources {
namespace {bool fail(std::string &error,const char *text){error=text;return false;}}
bool putAnimationPropertyCurveKey(AnimationClipAsset &asset,u64 id,u32 component,AnimationCurveKey key,u64 &created,std::string &error) {
  auto candidate=asset;auto *track=candidate.propertyTrack(id);
  if(!track||component>=track->data.curves.size())return fail(error,"Propriedade discreta ou canal de curva ausente");
  if(track->data.kind==AnimationPropertyKind::Quaternion) {
    auto &keys=track->data.curves[component].keys;
    const auto found=std::find_if(keys.begin(),keys.end(),[&](const auto &k){return key.id?k.id==key.id:k.time==key.time;});
    if(key.id&&found==keys.end())return fail(error,"Chave quaternion removida");
    if(found!=keys.end()) {
      const auto index=usize(found-keys.begin());
      for(u32 c=0;c<4;++c)if(c!=component) {
        auto other=track->data.curves[c].keys[index];other.time=key.time;u64 ignored=0;
        if(!putAnimationCurveKey(track->data.curves[c],other,candidate.nextId,ignored))return fail(error,"Tempos quaternion colidem");
      }
    } else {
      CompiledAnimationProperty compiled;AnimationPropertyValue value;
      if(!compileAnimationProperty(track->data,asset.duration,compiled,error)||!compiled.sample(key.time,value)||
         !candidate.putPropertyValue(id,key.time,value,error))return false;
      track=candidate.propertyTrack(id);
    }
  }
  u64 result=0;
  if(!putAnimationCurveKey(track->data.curves[component],key,candidate.nextId,result)||!candidate.valid(&error))return false;
  asset=std::move(candidate);created=result;return true;
}
bool splitAnimationPropertyCurveKey(AnimationClipAsset &asset,u64 id,u32 component,float time,u64 &created,std::string &error) {
  auto candidate=asset;auto *track=candidate.propertyTrack(id);
  if(!track||component>=track->data.curves.size()||!std::isfinite(time)||time<0||time>asset.duration)return fail(error,"Tempo ou curva tipada inválidos");
  const auto &keys=track->data.curves[component].keys;
  if(const auto found=std::find_if(keys.begin(),keys.end(),[&](const auto &k){return k.time==time;});found!=keys.end()){created=found->id;return true;}
  u64 result=0;
  if(track->data.kind==AnimationPropertyKind::Quaternion) {
    for(const auto &curve:track->data.curves)for(const auto &key:curve.keys)
      if(key.incoming!=AnimationTangentMode::Linear||key.outgoing!=AnimationTangentMode::Linear)
        return fail(error,"Subdivisão quaternion curva exige bake explícito");
    CompiledAnimationProperty compiled;AnimationPropertyValue value;
    if(!compileAnimationProperty(track->data,asset.duration,compiled,error)||!compiled.sample(time,value)||!candidate.putPropertyValue(id,time,value,error))return false;
    track=candidate.propertyTrack(id);
    const auto &newKeys=track->data.curves[component].keys;const auto found=std::find_if(newKeys.begin(),newKeys.end(),[&](const auto &k){return k.time==time;});
    if(found==newKeys.end())return fail(error,"Subdivisão perdeu identidade quaternion");
    result=found->id;
  } else if(!splitAnimationCurve(track->data.curves[component],time,candidate.nextId,result))return fail(error,"Subdivisão tipada exige intervalo com handles finitos");
  if(!candidate.valid(&error))return false;
  asset=std::move(candidate);created=result;return true;
}
bool eraseAnimationPropertyCurveKey(AnimationClipAsset &asset,u64 id,u32 component,u64 key,std::string &error) {
  auto candidate=asset;auto *track=candidate.propertyTrack(id);
  if(!track)return fail(error,"Canal tipado ausente");
  if(track->data.curves.empty()) {
    if(component||track->data.keys.size()<=1||std::none_of(track->data.keys.begin(),track->data.keys.end(),[&](const auto &k){return k.id==key;}))return fail(error,"Preserve uma chave discreta");
    std::erase_if(track->data.keys,[&](const auto &k){return k.id==key;});
  } else {
    if(component>=track->data.curves.size())return fail(error,"Componente de curva ausente");
    const auto &keys=track->data.curves[component].keys;const auto found=std::find_if(keys.begin(),keys.end(),[&](const auto &k){return k.id==key;});
    if(found==keys.end()||keys.size()<=1)return fail(error,"Preserve uma chave na curva");
    if(track->data.kind==AnimationPropertyKind::Quaternion) {
      const usize index=usize(found-keys.begin());for(auto &curve:track->data.curves)curve.keys.erase(curve.keys.begin()+index);
    } else track->data.curves[component].keys.erase(found);
  }
  if(!candidate.valid(&error))return false;
  asset=std::move(candidate);return true;
}
}
