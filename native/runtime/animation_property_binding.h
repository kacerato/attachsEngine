#pragma once
#include "runtime/scene_graph.h"
#include "resources/animation_property.h"
#include <functional>
#include <string>
#include <map>
#include <optional>

namespace ae::ui {class GuiRuntime;}
namespace ae::runtime {
struct AnimationPropertyDescriptor {
  resources::AnimationPropertyAddress address;
  std::string name,unit,help;
  float minimum=0,maximum=0;
};
// UI leases are resolved by identity for every publication. No borrowed node
// pointer survives removal, resource replacement, preview cancellation or Play.
using AnimationGuiResolver=std::function<ui::GuiRuntime*(ObjectId,u64,resources::AssetGuid)>;
std::vector<AnimationPropertyDescriptor> animationPropertyCatalog(const scene::ComponentValue &);
bool readAnimationProperty(const scene::ComponentValue &,const resources::AnimationPropertyAddress &,
                           resources::AnimationPropertyValue &,std::string &error);
bool writeAnimationProperty(scene::ComponentValue &,const resources::AnimationPropertyAddress &,
                            const resources::AnimationPropertyValue &,std::string &error);
struct AnimationPropertyWrite {
  ObjectId object=kInvalidObject;
  resources::AnimationPropertyAddress address;
  resources::AnimationPropertyValue value;
};
// First resolve every binding, then validate each completed component candidate;
// only then publish the batch. A broken target never partially animates peers.
bool publishAnimationProperties(SceneGraph &,std::span<const AnimationPropertyWrite>,
                                const AnimationGuiResolver &,std::string &error);
bool readAnimationPropertyTarget(const SceneGraph &,ObjectId,const resources::AnimationPropertyAddress &,
                                 const AnimationGuiResolver &,resources::AnimationPropertyValue &,std::string &error);
struct AnimationPropertyContribution : AnimationPropertyWrite {
  float weight=1;u32 layer=0;bool additive=false;
  std::optional<resources::AnimationPropertyValue> reference;
  ObjectId sourceOwner=kInvalidObject;u64 sourceInstance=0;float coverage=0;
};
class AnimationPropertyMixer final {
public:
  using Baselines=std::map<std::string,resources::AnimationPropertyValue>;
  using BeforePublication=std::function<bool(std::vector<AnimationPropertyWrite> &,const Baselines &,std::string &)>;
  void reset(){rest_.clear();gui_={};}
  void setGuiResolver(AnimationGuiResolver value){gui_=std::move(value);}
  bool apply(SceneGraph &,std::span<const AnimationPropertyContribution>,std::string &error,const BeforePublication &before={});
private:
  std::map<std::string,resources::AnimationPropertyValue> rest_;
  AnimationGuiResolver gui_;
};
}
