#pragma once
// Shared graph, independent of scene object IDs. Clip substitutions belong to
// instances. Unity 6000.0 AnimatorOverrideController / ApplyOverrides:
// https://docs.unity3d.com/6000.0/Documentation/Manual/AnimatorOverrideController.html
#include "scene/animator.h"
#include <locale>
#include <sstream>

namespace ae::resources {
struct AnimatorControllerAsset {
  static constexpr u32 FormatVersion=3;
  static constexpr usize MaximumBytes=1024*1024;
  AssetGuid guid{};u32 revision=1;std::string name;
  scene::Animator graph;
  bool valid() const {
    if(!guid.valid()||!revision||!scene::Animator::validName(name)||!graph.valid()||graph.layers.empty()||graph.controller.valid()||
       !graph.clipOverrides.empty()||graph.target||graph.motionSource||!graph.enabled||graph.unscaledTime||graph.speed!=1) return false;
    for(const auto &layer:graph.layers) if(layer.mask) return false;
    return true;
  }
  static scene::Animator portableGraph(const scene::Animator &source) {
    auto graph=source;graph.controller={};graph.clipOverrides.clear();graph.target=graph.motionSource=0;
    graph.enabled=true;graph.unscaledTime=false;graph.speed=1;
    for(auto &layer:graph.layers) layer.mask=0;
    return graph;
  }
  std::string serialize() const {
    std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(9);
    out<<"AEANIMATOR "<<FormatVersion<<' '<<guid.text()<<' '<<revision<<' '<<std::quoted(name)<<' ';
    graph.write(out);return out.str();
  }
  static bool deserialize(std::string_view text,AnimatorControllerAsset &out) {
    if(text.size()>MaximumBytes) return false;
    std::istringstream in{std::string(text)};in.imbue(std::locale::classic());
    AnimatorControllerAsset candidate;std::string magic,guid;u32 version=0;
    if(!(in>>magic>>version>>guid>>candidate.revision>>std::quoted(candidate.name))||magic!="AEANIMATOR"||(version<1||version>FormatVersion)||
       !AssetGuid::parse(guid,candidate.guid)||!candidate.graph.read(in,version+2)||!candidate.valid()) return false;
    in>>std::ws;if(!in.eof()) return false;
    out=std::move(candidate);return true;
  }
};
inline const AnimatorControllerAsset *findAnimatorController(std::span<const AnimatorControllerAsset> library,AssetGuid guid) {
  for(const auto &asset:library) if(asset.guid==guid) return &asset;
  return nullptr;
}
inline std::vector<AssetGuid> animatorControllerClips(const scene::Animator &graph) {
  std::vector<AssetGuid> clips;
  for(const auto &layer:graph.layers) if(layer.referenceClip.valid()&&std::find(clips.begin(),clips.end(),layer.referenceClip)==clips.end()) clips.push_back(layer.referenceClip);
  for(const auto &layer:graph.layers) for(const auto &state:layer.states) for(const auto &motion:state.motions)
    if(motion.clip.valid()&&std::find(clips.begin(),clips.end(),motion.clip)==clips.end()) clips.push_back(motion.clip);
  return clips;
}
// No fallback to the owner's old inline graph on missing resource. Unused
// overrides are retained for source revisions and explicitly diagnosed.
inline bool resolveAnimatorController(const scene::Animator &instance,std::span<const AnimatorControllerAsset> library,
                                      scene::Animator &out,std::string &diagnostic) {
  diagnostic.clear();
  if(!instance.controller.valid()) {out=instance;return true;}
  const auto *asset=findAnimatorController(library,instance.controller);
  if(!asset||!asset->valid()) {diagnostic="Controller ausente ou inválido";return false;}
  out=asset->graph;out.enabled=instance.enabled;out.unscaledTime=instance.unscaledTime;out.speed=instance.speed;
  out.target=instance.target;out.motionSource=instance.motionSource;out.controller=instance.controller;out.clipOverrides=instance.clipOverrides;
  for(auto &layer:out.layers) {
    for(const auto &entry:instance.clipOverrides) if(layer.referenceClip==entry.original) {layer.referenceClip=entry.replacement;break;}
    if(const auto *local=instance.layer(layer.id)) layer.mask=local->mask;
    for(auto &state:layer.states) for(auto &motion:state.motions) for(const auto &entry:instance.clipOverrides)
      if(motion.clip==entry.original) {motion.clip=entry.replacement;break;}
  }
  const auto originals=animatorControllerClips(asset->graph);
  for(const auto &entry:instance.clipOverrides) if(std::find(originals.begin(),originals.end(),entry.original)==originals.end())
    diagnostic="Override sem origem nesta revisão; preservado";
  return out.valid();
}
} // namespace ae::resources
