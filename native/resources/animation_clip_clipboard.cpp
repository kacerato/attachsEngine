#include "resources/animation_clip_clipboard.h"
#include "resources/animation_clip_edit_helpers.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace ae::resources {
bool AnimationKeyClipboard::copy(const AnimationClipAsset &clip,std::span<const AnimationKeyAddress> selection,std::string &diagnostic) {
  std::vector<AnimationKeyAddress> expanded;
  if(!expandAnimationKeySelection(clip,selection,expanded,diagnostic))return false;
  AnimationKeyClipboard candidate;candidate.clip_=clip.guid;candidate.source_=clip.source;
  candidate.first_=std::numeric_limits<float>::max();candidate.count_=expanded.size();
  std::unordered_set<u64> selected;for(const auto &address:expanded)selected.insert(address.key);
  for(const auto &track:clip.tracks) {
    Entry entry;entry.track=track.id;entry.layer=track.layer;entry.layerName=clip.layer(track.layer)->name;entry.blend=clip.layer(track.layer)->blend;entry.path=track.path;entry.rotation=track.rotationMode;entry.weightCount=track.weightCount;
    entry.binding=*std::find_if(clip.bindings.begin(),clip.bindings.end(),[&](const auto &b){return b.id==track.binding;});
    entry.curves.resize(track.components());bool used=false;
    for(u32 c=0;c<track.components();++c)for(usize i=0;i<track.curves[c].keys.size();++i) {
      const auto &key=track.curves[c].keys[i];if(!selected.contains(key.id))continue;
      Key copied;copied.value=key;
      if(track.rotationMode==AnimationRotationMode::ProgressiveQuaternion&&c==4) {
        copied.inSpan=i?double(key.value)-track.curves[c].keys[i-1].value:0;
        copied.outSpan=i+1<track.curves[c].keys.size()?double(track.curves[c].keys[i+1].value)-key.value:0;
      }
      entry.curves[c].push_back(copied);used=true;candidate.first_=std::min(candidate.first_,key.time);candidate.last_=std::max(candidate.last_,key.time);
    }
    if(used)candidate.entries_.push_back(std::move(entry));
  }
  *this=std::move(candidate);return true;
}
bool AnimationKeyClipboard::paste(AnimationClipAsset &clip,float at,AnimationPasteMode mode,
                                  std::vector<AnimationKeyAddress> &selection,std::string &diagnostic) const {
  diagnostic.clear();
  if(empty()||!clip.valid(&diagnostic)||!std::isfinite(at)||at<0||at>clip.duration||static_cast<u8>(mode)>2) {
    if(diagnostic.empty())diagnostic="Clipboard, modo ou tempo de colagem inválido";
    return false;
  }
  auto candidate=clip;std::vector<u64> targets;std::unordered_set<u64> unique;
  for(const auto &entry:entries_) {
    const AnimationClipTrack *target=nullptr;
    if(clip_==clip.guid)target=candidate.track(entry.track);
    else for(const auto &binding:candidate.bindings) {
      const bool match=source_.valid()&&source_==clip.source&&entry.binding.sourceNode.valid()?binding.sourceNode==entry.binding.sourceNode:binding.path==entry.binding.path;
      if(!match)continue;
      for(const auto &track:candidate.tracks)if(track.binding==binding.id&&track.path==entry.path) {
        const auto *layer=candidate.layer(track.layer);
        if(entry.layer?(track.layer&&layer->name==entry.layerName&&layer->blend==entry.blend):!track.layer) {
          if(target) {diagnostic="Camadas de destino ambíguas; use nomes distintos antes de colar entre clipes";return false;}
          target=&track;
        }
      }
    }
    if(!target||target->path!=entry.path||target->rotationMode!=entry.rotation||target->weightCount!=entry.weightCount||!unique.insert(target->id).second) {
      diagnostic="Destino não possui o binding/propriedade/modo compatível; configure o canal antes de colar";return false;
    }
    targets.push_back(target->id);
  }
  const double offset=double(at)-first_,extent=double(last_)-first_;
  if(mode!=AnimationPasteMode::Replace) {
    // A final frame gap retains both the last pasted key and the old key at the
    // insertion boundary. A single copied pose inserts one display frame.
    const double shift=extent+1.0/candidate.displayRate;
    if(candidate.duration+shift>86400) {diagnostic="Inserção excede a duração máxima";return false;}
    candidate.duration=static_cast<float>(candidate.duration+shift);
    for(auto &layer:candidate.layers)if(layer.referenceTime>=at) {
      bool affected=false,unaffected=false;
      for(const auto &track:candidate.tracks)if(track.layer==layer.id)
        (mode==AnimationPasteMode::InsertAllTracks||unique.contains(track.id)?affected:unaffected)=true;
      if(affected&&unaffected) {diagnostic="Inserção parcial deslocaria a referência compartilhada; use Inserir tudo";return false;}
      if(affected)layer.referenceTime=static_cast<float>(layer.referenceTime+shift);
    }
    for(auto &track:candidate.tracks)if(mode==AnimationPasteMode::InsertAllTracks||unique.contains(track.id)) {
      for(auto &curve:track.curves)for(auto &key:curve.keys)if(key.time>=at)key.time=static_cast<float>(key.time+shift);
      track.sourceOverride=true;
    }
  } else if(double(at)+extent>candidate.duration) {diagnostic="Chaves coladas ultrapassam a duração do clipe";return false;}
  std::vector<AnimationKeyAddress> result;
  for(usize e=0;e<entries_.size();++e) {
    const auto &entry=entries_[e];auto *target=candidate.track(targets[e]);
    const bool progressive=entry.rotation==AnimationRotationMode::ProgressiveQuaternion;
    // Write all quaternion components before validation: an intermediate
    // component must never turn a full-pose paste into a zero quaternion error.
    for(u32 c=0;c<entry.curves.size();++c) {
      float previous=-1;
      for(const auto &copied:entry.curves[c]) {
      auto key=copied.value;key.id=0;key.time=static_cast<float>(key.time+offset);
      if(!std::isfinite(key.time)||key.time<=previous||key.time<0||key.time>candidate.duration) {diagnostic="Colagem excede precisão ou duração";return false;}
      previous=key.time;
      if(progressive&&c==4) {
        AnimationCurveSample old;
        if(!sampleValidatedAnimationCurve(target->curves[c],key.time,old)) {diagnostic="Progressão de destino não produz valor";return false;}
        key.value=static_cast<float>(old.value);key.incoming=key.outgoing=AnimationTangentMode::Linear;
        key.broken=key.weightedIn=key.weightedOut=false;key.inSlope=key.outSlope=0;
      }
      u64 id=0;if(!putAnimationCurveKey(target->curves[c],key,candidate.nextId,id)) {diagnostic="Chave colada inválida ou sem precisão temporal";return false;}
      result.push_back({target->id,c,id});
      }
    }
    if(progressive) {
      if(!rebuildProgressiveAnimationTrack(*target,diagnostic))return false;
      auto &curve=target->curves[4];
      for(const auto &copied:entry.curves[4]) {
        const float time=static_cast<float>(copied.value.time+offset);
        auto found=std::find_if(curve.keys.begin(),curve.keys.end(),[&](const auto &key){return key.time==time;});
        const auto index=static_cast<usize>(found-curve.keys.begin());const auto id=found->id;const auto value=found->value;
        *found=copied.value;found->id=id;found->time=time;found->value=value;
        for(bool incoming:{true,false}) {
          const double original=incoming?copied.inSpan:copied.outSpan;
          const double current=incoming?(index?double(value)-curve.keys[index-1].value:0):(index+1<curve.keys.size()?double(curve.keys[index+1].value)-value:0);
          auto &slope=incoming?found->inSlope:found->outSlope;
          const double scaled=original?slope*current/original:0;
          if(!std::isfinite(scaled)||std::abs(scaled)>std::numeric_limits<float>::max()) {diagnostic="Handle colado excede a precisão";return false;}
          slope=static_cast<float>(scaled);
        }
      }
    }
    target->sourceOverride=true;
  }
  if(!candidate.valid(&diagnostic))return false;
  clip=std::move(candidate);selection=std::move(result);return true;
}
}
