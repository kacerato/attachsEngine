#pragma once
#include "runtime/animation_property_binding.h"
#include "resources/animation_driver_bindings.h"

namespace ae::runtime {
using AnimationDriverParameterResolver=std::function<bool(ObjectId,u64,u64,resources::AssetGuid,double &,std::string &)>;
struct AnimationDriverRequest {
  std::shared_ptr<const resources::CompiledAnimationDriverBindings> binding;
  std::vector<ObjectId> targets;
  float time=0,weight=1,coverage=1;u32 layer=0;ObjectId mask=0,sourceOwner=0;u64 sourceInstance=0;
};
bool evaluateAnimationDriverRequests(const SceneGraph &,std::span<const AnimationDriverRequest>,
    const AnimationGuiResolver &,const AnimationDriverParameterResolver &,
    std::vector<AnimationPropertyWrite> &,const AnimationPropertyMixer::Baselines &,
    std::vector<AnimationPropertyContribution> &captured,std::string &error);
}
