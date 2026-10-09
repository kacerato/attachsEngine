#include "resources/animation_clip_selection.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace ae::resources {
bool expandAnimationKeySelection(const AnimationClipAsset &clip,std::span<const AnimationKeyAddress> selection,
                                 std::vector<AnimationKeyAddress> &out,std::string &diagnostic) {
  diagnostic.clear();
  if(!clip.valid(&diagnostic))return false;
  if(selection.empty()||selection.size()>AnimationClipAsset::MaximumKeys) {diagnostic="Seleção vazia ou excede o limite de chaves";return false;}
  std::vector<AnimationKeyAddress> result;std::unordered_set<u64> ids;
  for(const auto &address:selection) {
    const auto *track=clip.track(address.track);
    if(!track||address.component>=track->curves.size()) {diagnostic="Canal selecionado não existe";return false;}
    const auto &keys=track->curves[address.component].keys;
    const auto found=std::find_if(keys.begin(),keys.end(),[&](const auto &key){return key.id==address.key;});
    if(found==keys.end()) {diagnostic="Chave selecionada não existe nesta revisão";return false;}
    if(track->path==AnimationPath::Rotation&&track->rotationMode!=AnimationRotationMode::Euler) {
      const auto index=static_cast<usize>(found-keys.begin());
      for(u32 c=0;c<track->curves.size();++c)if(ids.insert(track->curves[c].keys[index].id).second)
        result.push_back({address.track,c,track->curves[c].keys[index].id});
    } else if(ids.insert(address.key).second)result.push_back(address);
  }
  out=std::move(result);return true;
}
bool transformAnimationKeyTimes(AnimationClipAsset &clip,std::span<const AnimationKeyAddress> selection,
                                double pivot,double scale,double offset,std::string &diagnostic) {
  std::vector<AnimationKeyAddress> expanded;
  if(!expandAnimationKeySelection(clip,selection,expanded,diagnostic))return false;
  if(!std::isfinite(pivot)||!std::isfinite(scale)||scale<=0||!std::isfinite(offset)) {diagnostic="Transformação de tempo inválida";return false;}
  auto candidate=clip;std::unordered_set<u64> changedTracks;
  for(const auto &address:expanded) {
    auto *track=candidate.track(address.track);auto &curve=track->curves[address.component];
    auto key=std::find_if(curve.keys.begin(),curve.keys.end(),[&](const auto &k){return k.id==address.key;});
    const double time=pivot+(double(key->time)-pivot)*scale+offset;
    const double incoming=key->inSlope/scale,outgoing=key->outSlope/scale;
    if(!std::isfinite(time)||time<0||time>clip.duration||!std::isfinite(incoming)||!std::isfinite(outgoing)||
       std::abs(incoming)>std::numeric_limits<float>::max()||std::abs(outgoing)>std::numeric_limits<float>::max()) {
      diagnostic="Seleção ultrapassa a duração ou a precisão do clipe";return false;
    }
    key->time=static_cast<float>(time);key->inSlope=static_cast<float>(incoming);key->outSlope=static_cast<float>(outgoing);
    track->sourceOverride=true;changedTracks.insert(track->id);
  }
  // Do not sort a collision away. Existing order is part of the transform's
  // contract, and checking after all writes permits moving adjacent groups.
  for(const auto id:changedTracks)for(const auto &curve:candidate.track(id)->curves)
    for(usize i=1;i<curve.keys.size();++i)if(curve.keys[i].time<=curve.keys[i-1].time) {
      diagnostic="Seleção cruza ou colide com uma chave não movida";return false;
    }
  if(!candidate.valid(&diagnostic))return false;
  clip=std::move(candidate);return true;
}
bool eraseAnimationKeySelection(AnimationClipAsset &clip,std::span<const AnimationKeyAddress> selection,
                                std::string &diagnostic) {
  std::vector<AnimationKeyAddress> expanded;
  if(!expandAnimationKeySelection(clip,selection,expanded,diagnostic))return false;
  auto candidate=clip;
  for(const auto &address:expanded) {
    const auto *track=candidate.track(address.track);const auto &keys=track->curves[address.component].keys;
    const auto key=std::find_if(keys.begin(),keys.end(),[&](const auto &k){return k.id==address.key;});
    if(key==keys.end())continue; // A prior group operation retired this component.
    if(!candidate.eraseKey(address.track,address.component,address.key,diagnostic))return false;
  }
  if(!candidate.valid(&diagnostic))return false;
  clip=std::move(candidate);return true;
}
} // namespace ae::resources
