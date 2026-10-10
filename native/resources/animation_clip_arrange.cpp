#include "resources/animation_clip_arrange.h"
#include "resources/animation_clip_region_edit.h"
#include "resources/animation_clip_timeline.h"
#include "resources/animation_clip_clipboard.h"
#include "resources/animation_clip_edit_helpers.h"
#include "core/rotation_math.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace ae::resources {
namespace {
bool range(const AnimationClipAsset &clip,float start,float end,std::string &error) {
  error.clear();if(!clip.valid(&error))return false;
  if(!std::isfinite(start)||!std::isfinite(end)||start<0||end<=start||end>clip.duration) {
    error="Escolha um intervalo não vazio dentro do clipe";return false;
  }
  return true;
}
bool pose(const AnimationClipTrack &track,float time,std::vector<float> &out,std::string &error) {
  out.clear();const auto count=track.path==AnimationPath::Rotation&&track.rotationMode==AnimationRotationMode::ProgressiveQuaternion?4u:track.components();
  for(u32 c=0;c<count;++c) {
    AnimationCurveSample sample;if(!sampleValidatedAnimationCurve(track.curves[c],time,sample)||!std::isfinite(sample.value)) {error="Canal não produz pose finita na fronteira";return false;}
    out.push_back(static_cast<float>(sample.value));
  }
  return true;
}
}
bool arrangeAnimationSelection(AnimationClipAsset &clip,std::span<const AnimationKeyAddress> keys,
    std::span<const u64> cues,AnimationArrange mode,float first,float last,std::string &error) {
  error.clear();if(!clip.valid(&error))return false;
  if(static_cast<u32>(mode)>3||!std::isfinite(first)||first<0||first>clip.duration||
     (mode==AnimationArrange::Distribute&&(!std::isfinite(last)||last<=first||last>clip.duration))||
     (keys.empty()&&cues.empty())||cues.size()>MaximumAnimationCues) {error="Seleção, modo ou destino temporal inválido";return false;}
  std::vector<AnimationKeyAddress> expanded;
  if(!keys.empty()&&!expandAnimationKeySelection(clip,keys,expanded,error))return false;
  std::set<u64> selectedKeys,selectedCues;std::map<u64,std::pair<float,float>> bounds;std::set<float> instants;
  const auto collect=[&](u64 group,float time) {
    const auto [it,inserted]=bounds.try_emplace(group,time,time);
    if(!inserted) {it->second.first=std::min(it->second.first,time);it->second.second=std::max(it->second.second,time);}
    instants.insert(time);
  };
  for(const auto &address:expanded) {
    selectedKeys.insert(address.key);
    const auto &curve=clip.track(address.track)->curves[address.component];
    const auto key=std::find_if(curve.keys.begin(),curve.keys.end(),[&](const auto &value){return value.id==address.key;});
    collect(address.track,key->time);
  }
  for(const auto id:cues) {
    const auto *cue=clip.cue(id);if(!cue||!selectedCues.insert(id).second) {error="Cue selecionado ausente ou repetido";return false;}
    collect(0,cue->time);
  }
  if(mode==AnimationArrange::Distribute&&instants.size()<2) {error="Distribuir exige pelo menos dois instantes distintos";return false;}
  std::map<float,float> distributed;
  if(mode==AnimationArrange::Distribute) {usize i=0;for(const auto time:instants)distributed[time]=static_cast<float>(double(first)+(double(last)-first)*double(i++)/double(instants.size()-1));}
  const auto destination=[&](u64 group,float old) {
    if(mode==AnimationArrange::Distribute)return distributed.at(old);
    const auto bound=bounds.at(group);const double anchor=mode==AnimationArrange::AlignStart?bound.first:mode==AnimationArrange::AlignEnd?bound.second:(double(bound.first)+bound.second)*.5;
    return static_cast<float>(double(old)+double(first)-anchor);
  };
  auto candidate=clip;
  for(auto &track:candidate.tracks) {
    bool changed=false;
    for(auto &curve:track.curves) {
      for(auto &key:curve.keys)if(selectedKeys.contains(key.id)) {
        key.time=destination(track.id,key.time);changed=true;
        if(!std::isfinite(key.time)||key.time<0||key.time>clip.duration) {error="Alinhamento ultrapassa o clipe";return false;}
      }
      for(usize i=1;i<curve.keys.size();++i)if(curve.keys[i].time<=curve.keys[i-1].time) {error="Operação cruza ou coincide com outra chave; nenhum canal foi alterado";return false;}
    }
    if(changed) {
      track.sourceOverride=true;
      if(track.path==AnimationPath::Rotation&&track.rotationMode==AnimationRotationMode::ProgressiveQuaternion&&!rebuildProgressiveAnimationTrack(track,error))return false;
    }
  }
  for(auto &cue:candidate.cues)if(selectedCues.contains(cue.id)) {
    cue.time=destination(0,cue.time);if(!std::isfinite(cue.time)||cue.time<0||cue.time>clip.duration) {error="Alinhamento de cues ultrapassa o clipe";return false;}
  }
  std::sort(candidate.cues.begin(),candidate.cues.end(),[](const auto &a,const auto &b){return a.time!=b.time?a.time<b.time:a.id<b.id;});
  if(!candidate.valid(&error))return false;
  clip=std::move(candidate);return true;
}
bool closeAnimationRange(AnimationClipAsset &clip,float start,float end,std::string &error) {
  if(!range(clip,start,end,error))return false;
  auto candidate=clip;
  for(const auto &track:clip.tracks) {
    std::vector<float> values;
    if(!pose(track,start,values,error)||!candidate.putPose(track.id,end,values,error))return false;
  }
  for(const auto &track:clip.properties) {
    CompiledAnimationProperty sampler;AnimationPropertyValue value;
    if(!compileAnimationProperty(track.data,clip.duration,sampler,error)||!sampler.sample(start,value)||!candidate.putPropertyValue(track.id,end,value,error))return false;
  }
  if(!candidate.valid(&error))return false;
  clip=std::move(candidate);return true;
}
bool repeatAnimationRange(AnimationClipAsset &clip,float start,float end,u32 copies,double tolerance,std::string &error) {
  if(!range(clip,start,end,error))return false;
  if(!copies||copies>64||!std::isfinite(tolerance)||tolerance<0||tolerance>1) {error="Repetição exige 1–64 cópias e tolerância entre 0 e 1";return false;}
  const double length=double(end)-start,added=length*copies;
  if(double(clip.duration)+added>86400) {error="Repetição excede 86400 segundos";return false;}
  for(const auto &track:clip.tracks) {
    std::vector<float> a,b;if(!pose(track,start,a,error)||!pose(track,end,b,error))return false;
    double residual=0;
    if(track.path==AnimationPath::Rotation&&track.rotationMode!=AnimationRotationMode::Euler)residual=rotationDistanceDegrees(a.data(),b.data());
    else for(usize c=0;c<a.size();++c)residual=std::max(residual,std::abs(double(a[c])-b[c]));
    if(!std::isfinite(residual)||residual>tolerance) {error="Fronteira aberta no canal "+std::to_string(track.id)+"; erro "+std::to_string(residual)+". Feche início/fim antes de repetir";return false;}
  }
  auto excerpt=clip;for(auto &layer:excerpt.layers)layer.referenceTime=-1;
  for(const auto &track:clip.properties) {
    CompiledAnimationProperty sampler;AnimationPropertyValue first,last;
    if(!compileAnimationProperty(track.data,clip.duration,sampler,error)||!sampler.sample(start,first)||!sampler.sample(end,last)||
       !animationPropertyValuesNear(first,last,tolerance)) {error="Fronteira aberta na propriedade "+std::to_string(track.id);return false;}
  }
  if(!excerpt.crop(start,end,error))return false;
  if(excerpt.regions.size()>((MaximumAnimationRegions-clip.regions.size())/copies)) {error="Repetição excede o orçamento de regiões";return false;}
  std::vector<AnimationKeyAddress> addresses;usize existing=0;
  for(const auto &track:clip.tracks)for(const auto &curve:track.curves)existing+=curve.keys.size();
  for(const auto &track:excerpt.tracks)for(u32 c=0;c<track.curves.size();++c)for(const auto &key:track.curves[c].keys)addresses.push_back({track.id,c,key.id});
  if(addresses.size()*(copies+1)>AnimationClipAsset::MaximumKeys-existing) {error="Repetição excede o orçamento de chaves";return false;}
  AnimationKeyClipboard clipboard;if(!addresses.empty()&&!clipboard.copy(excerpt,addresses,error))return false;
  auto candidate=clip;if(!insertAnimationTime(candidate,end,static_cast<float>(added),error))return false;
  for(u32 cycle=0;!addresses.empty()&&cycle<=copies;++cycle) {
    std::vector<AnimationKeyAddress> pasted;
    if(!clipboard.paste(candidate,static_cast<float>(double(start)+length*cycle),AnimationPasteMode::Replace,pasted,error))return false;
  }
  for(u32 cycle=0;cycle<=copies;++cycle)if(!copyAnimationPropertyRange(candidate,excerpt,static_cast<float>(double(start)+length*cycle),error))return false;
  for(u32 cycle=1;cycle<=copies;++cycle)for(auto cue:clip.cues)if(cue.time>=start&&cue.time<end) {
    cue.id=0;cue.time=static_cast<float>(double(cue.time)+length*cycle);u64 id=0;
    if(!candidate.putCue(std::move(cue),id,error))return false;
  }
  for(u32 cycle=1;cycle<=copies;++cycle)if(!copyAnimationRegions(candidate,excerpt.regions,double(start)+length*cycle,error))return false;
  if(!candidate.valid(&error))return false;
  clip=std::move(candidate);return true;
}
}
