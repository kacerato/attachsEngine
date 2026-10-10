#pragma once
#include "resources/animation_driver.h"
#include "resources/animation_property.h"

namespace ae::resources {
struct AnimationClipBinding;
enum class AnimationDriverInputKind : u8 {ClipTime,Property,AnimatorParameter};
struct AnimationDriverTarget {u64 binding=0;AnimationPropertyAddress address;};
struct AnimationDriverInput : AnimationDriverTarget {
  AnimationDriverInputKind kind=AnimationDriverInputKind::ClipTime;
  u64 parameter=0;AssetGuid controller{};
};
struct AnimationClipDrivers {
  AnimationDriverGraph graph;
  std::vector<AnimationDriverInput> inputs;
  std::vector<AnimationDriverTarget> targets;
  bool empty() const {return graph.nodes.empty();}
};
struct CompiledAnimationDriverTarget {u32 node=0;AnimationPropertyAddress address;};
struct CompiledAnimationDriverInput : CompiledAnimationDriverTarget {
  AnimationDriverInputKind kind=AnimationDriverInputKind::ClipTime;
  u64 parameter=0;AssetGuid controller{};
};
struct CompiledAnimationDriverBindings {
  CompiledAnimationDrivers graph;
  std::vector<CompiledAnimationDriverInput> inputs;
  std::vector<CompiledAnimationDriverTarget> targets;
};
bool compileAnimationDriverBindings(const AnimationClipDrivers &,std::span<const AnimationClipBinding>,
                                   CompiledAnimationDriverBindings &,std::string &error);
void writeAnimationClipDrivers(std::ostream &,const AnimationClipDrivers &);
bool readAnimationClipDrivers(std::istream &,AnimationClipDrivers &,std::string &error);
}
