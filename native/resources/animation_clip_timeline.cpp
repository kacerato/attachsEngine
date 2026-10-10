#include "resources/animation_clip_timeline.h"
#include "resources/animation_clip_clipboard.h"
#include "resources/animation_clip_region_edit.h"
#include "resources/animation_clip_edit_helpers.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace ae::resources {
namespace {
bool cueIds(const AnimationClipAsset &clip,std::span<const u64> ids,std::unordered_set<u64> &out,std::string &error) {
  if(!clip.valid(&error))return false;
  if(ids.empty()||ids.size()>MaximumAnimationCues) {error="Seleção de cues vazia ou acima do limite";return false;}
  for(const auto id:ids)if(!clip.cue(id)||!out.insert(id).second) {error="Cue ausente, repetido ou de outra revisão";return false;}
  return true;
}
void sortCues(AnimationClipAsset &clip) {
  std::sort(clip.cues.begin(),clip.cues.end(),[](const auto &a,const auto &b){return a.time!=b.time?a.time<b.time:a.id<b.id;});
}
bool publish(AnimationClipAsset &clip,AnimationClipAsset &candidate,std::string &error) {
  if(!candidate.valid(&error))return false;
  clip=std::move(candidate);return true;
}
}
bool insertAnimationTime(AnimationClipAsset &clip,float at,float length,std::string &error) {
  error.clear();
  if(!clip.valid(&error))return false;
  const double duration=double(clip.duration)+length;
  if(!std::isfinite(at)||!std::isfinite(length)||at<0||at>clip.duration||length<=0||duration>86400) {
    error="Inserção exige tempo no clipe e duração positiva dentro de 86400 s";return false;
  }
  auto candidate=clip;candidate.duration=static_cast<float>(duration);
  insertAnimationRegionTime(candidate,at,length);
  if(candidate.duration<=clip.duration) {error="Inserção menor que a precisão temporal";return false;}
  for(auto &track:candidate.tracks) {
    for(auto &curve:track.curves)for(auto &key:curve.keys)if(key.time>=at) {
      const auto old=key.time;key.time=static_cast<float>(double(old)+length);
      if(key.time<=old) {error="Inserção menor que a precisão de uma chave";return false;}
    }
    track.sourceOverride=true;
  }
  for(auto &track:candidate.properties) {
    for(auto &curve:track.data.curves)for(auto &key:curve.keys)if(key.time>=at)key.time=static_cast<float>(double(key.time)+length);
    for(auto &key:track.data.keys)if(key.time>=at)key.time=static_cast<float>(double(key.time)+length);
  }
  for(auto &cue:candidate.cues)if(cue.time>=at)cue.time=static_cast<float>(double(cue.time)+length);
  for(auto &layer:candidate.layers)if(layer.referenceTime>=at)layer.referenceTime=static_cast<float>(double(layer.referenceTime)+length);
  sortCues(candidate);return publish(clip,candidate,error);
}
bool removeAnimationTime(AnimationClipAsset &clip,float start,float end,std::string &error) {
  error.clear();if(!clip.valid(&error))return false;
  if(!std::isfinite(start)||!std::isfinite(end)||start<0||end<=start||end>clip.duration) {error="Remoção exige início < fim dentro do clipe";return false;}
  auto candidate=clip;const double length=double(end)-start;candidate.duration=static_cast<float>(double(clip.duration)-length);
  removeAnimationRegionTime(candidate,start,end);
  for(auto &layer:candidate.layers)if(layer.referenceTime>=start) {
    if(layer.referenceTime<end) {error="Intervalo contém referência aditiva; escolha outra referência antes de remover";return false;}
    layer.referenceTime=static_cast<float>(double(layer.referenceTime)-length);
  }
  for(auto &track:candidate.tracks) {
    for(auto &curve:track.curves) {
      std::erase_if(curve.keys,[&](const auto &key){return key.time>=start&&key.time<end;});
      if(curve.keys.empty()) {error="Remoção apagaria todas as chaves de um canal";return false;}
      for(auto &key:curve.keys)if(key.time>=end)key.time=static_cast<float>(double(key.time)-length);
    }
    if(track.path==AnimationPath::Rotation&&track.rotationMode==AnimationRotationMode::ProgressiveQuaternion&&
       !rebuildProgressiveAnimationTrack(track,error))return false;
    track.sourceOverride=true;
  }
  std::erase_if(candidate.cues,[&](const auto &cue){return cue.time>=start&&cue.time<end;});
  for(auto &track:candidate.properties) {
    for(auto &curve:track.data.curves) {
      std::erase_if(curve.keys,[&](const auto &key){return key.time>=start&&key.time<end;});
      for(auto &key:curve.keys)if(key.time>=end)key.time=static_cast<float>(double(key.time)-length);
    }
    std::erase_if(track.data.keys,[&](const auto &key){return key.time>=start&&key.time<end;});
    for(auto &key:track.data.keys)if(key.time>=end)key.time=static_cast<float>(double(key.time)-length);
  }
  for(auto &cue:candidate.cues)if(cue.time>=end)cue.time=static_cast<float>(double(cue.time)-length);
  sortCues(candidate);return publish(clip,candidate,error);
}
bool duplicateAnimationRange(AnimationClipAsset &clip,float start,float end,float at,std::string &error) {
  error.clear();if(!clip.valid(&error))return false;
  if(!std::isfinite(start)||!std::isfinite(end)||!std::isfinite(at)||start<0||end<=start||end>clip.duration||at<0||at>clip.duration) {
    error="Duplicação exige intervalo não vazio e destino dentro do clipe";return false;
  }
  auto excerpt=clip;
  // The clipboard carries raw authored curves, not a composed bake. Reference
  // times belong to the original layer and are shifted by ripple publication.
  for(auto &layer:excerpt.layers)layer.referenceTime=-1;
  if(!excerpt.crop(start,end,error))return false;
  std::vector<AnimationKeyAddress> addresses;
  for(const auto &track:excerpt.tracks)for(u32 c=0;c<track.curves.size();++c)for(const auto &key:track.curves[c].keys)
    addresses.push_back({track.id,c,key.id});
  AnimationKeyClipboard clipboard;if(!addresses.empty()&&!clipboard.copy(excerpt,addresses,error))return false;
  auto candidate=clip;std::vector<AnimationKeyAddress> pasted;
  if(!insertAnimationTime(candidate,at,static_cast<float>(double(end)-start+1.0/clip.displayRate),error))return false;
  if(!addresses.empty()&&!clipboard.paste(candidate,at,AnimationPasteMode::Replace,pasted,error))return false;
  if(!copyAnimationPropertyRange(candidate,excerpt,at,error))return false;
  if(!copyAnimationRegions(candidate,excerpt.regions,at,error))return false;
  for(auto cue:clip.cues)if(cue.time>=start&&(cue.time<end||(end==clip.duration&&cue.time==end))) {
    cue.id=0;cue.time=static_cast<float>(double(at)+double(cue.time)-start);u64 created=0;
    if(!candidate.putCue(std::move(cue),created,error))return false;
  }
  return publish(clip,candidate,error);
}
bool transformAnimationCueTimes(AnimationClipAsset &clip,std::span<const u64> ids,double pivot,double scale,double offset,std::string &error) {
  error.clear();std::unordered_set<u64> selected;if(!cueIds(clip,ids,selected,error))return false;
  if(!std::isfinite(pivot)||!std::isfinite(scale)||scale<=0||!std::isfinite(offset)) {error="Transformação de cues inválida";return false;}
  auto candidate=clip;
  for(auto &cue:candidate.cues)if(selected.contains(cue.id)) {
    const double time=pivot+(double(cue.time)-pivot)*scale+offset;
    if(!std::isfinite(time)||time<0||time>clip.duration) {error="Cues ultrapassam o intervalo do clipe";return false;}
    cue.time=static_cast<float>(time);
  }
  sortCues(candidate);return publish(clip,candidate,error);
}
bool moveAnimationRange(AnimationClipAsset &clip,float start,float end,float at,std::string &error) {
  error.clear();if(!clip.valid(&error))return false;
  const double offset=double(at)-start;
  if(!std::isfinite(start)||!std::isfinite(end)||!std::isfinite(at)||start<0||end<=start||end>clip.duration||at<0||double(end)+offset>clip.duration) {
    error="Movimento exige intervalo não vazio e trecho inteiro dentro do clipe";return false;
  }
  const auto included=[&](float time){return time>=start&&(time<end||(end==clip.duration&&time==end));};
  std::vector<AnimationKeyAddress> keys;std::vector<u64> cues;
  for(const auto &track:clip.tracks)for(u32 c=0;c<track.curves.size();++c)for(const auto &key:track.curves[c].keys)
    if(included(key.time))keys.push_back({track.id,c,key.id});
  for(const auto &cue:clip.cues)if(included(cue.time))cues.push_back(cue.id);
  auto candidate=clip;
  for(auto &r:candidate.regions) {
    const bool overlaps=r.start<end&&r.end>start;
    if(overlaps&&(r.start<start||r.end>end)) {error="Mover trecho cortaria uma região; inclua suas duas fronteiras";return false;}
    if(overlaps) {r.start=static_cast<float>(r.start+offset);r.end=static_cast<float>(r.end+offset);}
  }
  sortAnimationRegions(candidate);
  for(auto &track:candidate.properties) {
    for(auto &curve:track.data.curves) {
      for(auto &key:curve.keys)if(included(key.time))key.time=static_cast<float>(double(key.time)+offset);
      std::sort(curve.keys.begin(),curve.keys.end(),[](const auto &a,const auto &b){return a.time<b.time;});
    }
    for(auto &key:track.data.keys)if(included(key.time))key.time=static_cast<float>(double(key.time)+offset);
    std::sort(track.data.keys.begin(),track.data.keys.end(),[](const auto &a,const auto &b){return a.time<b.time;});
  }
  if(!keys.empty()&&!transformAnimationKeyTimes(candidate,keys,0,1,offset,error))return false;
  if(!cues.empty()&&!transformAnimationCueTimes(candidate,cues,0,1,offset,error))return false;
  for(auto &layer:candidate.layers)if(layer.referenceTime>=0&&included(layer.referenceTime))layer.referenceTime=static_cast<float>(double(layer.referenceTime)+offset);
  return publish(clip,candidate,error);
}
bool eraseAnimationCues(AnimationClipAsset &clip,std::span<const u64> ids,std::string &error) {
  error.clear();std::unordered_set<u64> selected;if(!cueIds(clip,ids,selected,error))return false;
  auto candidate=clip;std::erase_if(candidate.cues,[&](const auto &cue){return selected.contains(cue.id);});
  return publish(clip,candidate,error);
}
bool AnimationCueClipboard::copy(const AnimationClipAsset &clip,std::span<const u64> ids,std::string &error) {
  error.clear();std::unordered_set<u64> selected;if(!cueIds(clip,ids,selected,error))return false;
  AnimationCueClipboard candidate;
  for(const auto &cue:clip.cues)if(selected.contains(cue.id))candidate.cues_.push_back(cue);
  candidate.first_=candidate.cues_.front().time;*this=std::move(candidate);return true;
}
bool AnimationCueClipboard::paste(AnimationClipAsset &clip,float at,std::vector<u64> &selection,std::string &error) const {
  error.clear();if(!clip.valid(&error))return false;
  if(empty()||!std::isfinite(at)||at<0||at>clip.duration||cues_.size()>MaximumAnimationCues-clip.cues.size()) {error="Clipboard de cues vazio, destino inválido ou limite excedido";return false;}
  auto candidate=clip;std::vector<u64> result;
  for(auto cue:cues_) {
    const double time=double(at)+double(cue.time)-first_;
    if(time>clip.duration) {error="Cues colados ultrapassam a duração do clipe";return false;}
    cue.id=0;cue.time=static_cast<float>(time);u64 id=0;
    if(!candidate.putCue(std::move(cue),id,error))return false;
    result.push_back(id);
  }
  if(!publish(clip,candidate,error))return false;
  selection=std::move(result);return true;
}
}
