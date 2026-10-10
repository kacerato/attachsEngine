#include "resources/unity_animation_merge.h"
#include <algorithm>
#include <limits>
#include <map>
#include <set>

namespace ae::resources {
namespace {
const AnimationClipBinding *binding(const AnimationClipAsset &clip,u64 id) {for(const auto &value:clip.bindings)if(value.id==id)return &value;return nullptr;}
const AnimationClipTrack *track(const AnimationClipAsset &clip,std::string_view path,AnimationPath property) {
  for(const auto &value:clip.tracks)if(!value.layer&&value.path==property){const auto *owner=binding(clip,value.binding);if(owner&&owner->path==path)return &value;}
  return nullptr;
}
bool keyEqual(const AnimationCurveKey &a,const AnimationCurveKey &b) {
  return a.id==b.id&&a.time==b.time&&a.value==b.value&&a.inSlope==b.inSlope&&a.outSlope==b.outSlope&&
    a.inWeight==b.inWeight&&a.outWeight==b.outWeight&&a.incoming==b.incoming&&a.outgoing==b.outgoing&&
    a.weightedIn==b.weightedIn&&a.weightedOut==b.weightedOut&&a.broken==b.broken;
}
bool equal(const AnimationClipTrack *a,const AnimationClipTrack *b) {
  if(!a||!b)return a==b;
  if(a->id!=b->id||a->binding!=b->binding||a->path!=b->path||a->weightCount!=b->weightCount||a->rotationMode!=b->rotationMode||a->curves.size()!=b->curves.size())return false;
  for(usize c=0;c<a->curves.size();++c){const auto &x=a->curves[c].keys,&y=b->curves[c].keys;
    if(x.size()!=y.size()||!std::equal(x.begin(),x.end(),y.begin(),keyEqual))return false;}
  return true;
}
}
bool mergeUnityAnimation(const AnimationClipAsset &baseline,const AnimationClipAsset &local,
    const AnimationClipAsset &incoming,UnityReimportPolicy policy,AnimationClipAsset &out,
    AnimationClipAsset &nextBaseline,UnityAnimationMergeReport &report,std::string &diagnostic) {
  const auto fail=[&](const char *reason){diagnostic=reason;return false;};
  if(static_cast<u8>(policy)>2||baseline.guid!=local.guid||local.guid!=incoming.guid||local.revision==std::numeric_limits<u32>::max()||
      !baseline.valid(&diagnostic)||!local.valid(&diagnostic)||!incoming.valid(&diagnostic))return fail("Baseline, revisão ou clipe de reimportação inválidos.");
  auto source=incoming;source.nextId=std::max(local.nextId,baseline.nextId);source.revision=local.revision+1;
  u64 maximumNew=source.bindings.size()+source.tracks.size();
  for(const auto &entry:source.tracks)for(const auto &curve:entry.curves)maximumNew+=curve.keys.size();
  if(source.nextId>=std::numeric_limits<u64>::max()-maximumNew)return fail("Alocador de identidades do clipe esgotado.");
  UnityAnimationMergeReport result;
  std::map<u64,u64> bindingIds;
  for(auto &entry:source.bindings) {
    const AnimationClipBinding *old=nullptr;
    for(const auto &value:baseline.bindings)if(value.path==entry.path){old=&value;break;}
    if(!old)for(const auto &value:local.bindings)if(value.path==entry.path){old=&value;break;}
    const auto oldId=entry.id;entry.id=old?old->id:source.nextId++;
    bindingIds.emplace(oldId,entry.id);
  }
  for(auto &value:source.tracks)value.binding=bindingIds.at(value.binding);
  for(auto &entry:source.tracks) {
    const auto *owner=binding(source,entry.binding);if(!owner)return fail("Canal da origem sem binding.");
    const auto *old=track(baseline,owner->path,entry.path);
    if(!old)old=track(local,owner->path,entry.path);
    entry.id=old?old->id:source.nextId++;
    for(usize c=0;c<entry.curves.size();++c)for(auto &key:entry.curves[c].keys) {
      const AnimationCurveKey *previous=nullptr;
      if(old&&c<old->curves.size())for(const auto &value:old->curves[c].keys)if(value.time==key.time){previous=&value;break;}
      key.id=previous?previous->id:source.nextId++;
    }
  }
  auto merged=local;merged.revision=source.revision;merged.nextId=source.nextId;merged.tracks.clear();merged.bindings.clear();
  const auto conflict=[&](const std::string &path){if(result.conflicts.size()<64)result.conflicts.push_back(path);};
  std::set<u64> adopted;
  const auto addBinding=[&](const AnimationClipBinding &owner)->bool {
    for(const auto &existing:merged.bindings)if(existing.id==owner.id)return existing.path==owner.path;
    merged.bindings.push_back(owner);return true;
  };
  const auto adopt=[&](const AnimationClipTrack &entry,const AnimationClipAsset &from,bool preserved)->bool {
    const auto *owner=binding(from,entry.binding);if(!owner)return false;
    auto mergedOwner=*owner;
    if(&from==&source) {
      const auto *old=binding(baseline,owner->id),*current=binding(local,owner->id);
      if(old&&current) {
        if(current->path!=old->path&&owner->path==old->path)mergedOwner.path=current->path;
        if(current->name!=old->name&&owner->name==old->name)mergedOwner.name=current->name;
      }
    }
    if(!addBinding(mergedOwner))return false;
    merged.tracks.push_back(entry);if(preserved)merged.tracks.back().sourceOverride=true;
    adopted.insert(entry.id);return true;
  };
  for(const auto &entry:source.tracks) {
    const auto *owner=binding(source,entry.binding);const auto *old=baseline.track(entry.id),*current=local.track(entry.id);
    const bool localChanged=!equal(old,current),sourceChanged=!equal(old,&entry);
    if(localChanged&&sourceChanged&&!equal(current,&entry))conflict(owner->path+" · "+std::to_string(static_cast<u32>(entry.path)));
    const bool preserve=localChanged&&(!sourceChanged||policy!=UnityReimportPolicy::ReceiveSource);
    if(preserve) {
      ++result.preserved;if(current&&!adopt(*current,local,true))return fail("Binding local conflitante.");
      // A locally removed track stays removed until ReceiveSource is explicit.
      adopted.insert(entry.id);
    } else {
      old?++result.updated:++result.added;
      if(!adopt(entry,source,false))return fail("Binding de origem conflitante.");
    }
  }
  for(const auto &entry:local.tracks)if(!adopted.contains(entry.id)) {
    const auto *old=baseline.track(entry.id);
    if(entry.layer||!old){if(!adopt(entry,local,false))return fail("Canal autoral sem binding.");continue;}
    if(!equal(old,&entry)) {
      const auto *owner=binding(local,entry.binding);conflict((owner?owner->path:std::string("?"))+" · removido na origem");
      if(policy!=UnityReimportPolicy::ReceiveSource){if(!adopt(entry,local,true))return fail("Binding removido incompatível.");++result.preserved;continue;}
    }
    ++result.removed;
  }
  for(const auto &property:merged.properties){const auto *owner=binding(local,property.binding);if(!owner||!addBinding(*owner))return fail("Propriedade autoral sem binding.");}
  // Driver binding references are authoring dependencies even without a keyed
  // track. Retain their original identities rather than pruning by curves.
  for(const auto &owner:local.bindings)if(std::none_of(merged.bindings.begin(),merged.bindings.end(),[&](const auto &value){return value.id==owner.id;})) {
    bool imported=false;for(const auto &old:baseline.bindings)imported|=old.id==owner.id;
    if(!imported||!merged.drivers.graph.nodes.empty())if(!addBinding(owner))return fail("Binding de driver conflitante.");
  }
  const auto mergeValue=[&](const auto &base,const auto &current,const auto &next,const char *name) {
    if(current!=base&&next!=base&&current!=next)conflict(name);
    return current!=base&&(next==base||policy!=UnityReimportPolicy::ReceiveSource)?current:next;
  };
  merged.name=mergeValue(baseline.name,local.name,source.name,"Nome do clipe");
  merged.duration=mergeValue(baseline.duration,local.duration,source.duration,"Duração do clipe");
  merged.displayRate=mergeValue(baseline.displayRate,local.displayRate,source.displayRate,"Taxa do clipe");
  if(!result.conflicts.empty()&&policy==UnityReimportPolicy::RefuseConflict){report=std::move(result);return fail("Fonte e edições locais alteraram os mesmos canais; escolha Manter local ou Receber origem.");}
  if(!source.valid(&diagnostic)||!merged.valid(&diagnostic))return false;
  out=std::move(merged);nextBaseline=std::move(source);report=std::move(result);diagnostic.clear();return true;
}
} // namespace ae::resources
