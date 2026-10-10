#pragma once
#include "runtime/component_operations.h"
#include "resources/skeletal_animation.h"
#include <limits>
namespace ae::runtime {
// Reuses the scene's deferred event bus. No callbacks mutate the evaluated
// graph while poses are being sampled. Preview never supplies this context.
inline u64 emitAnimationClipCues(GameWorld &world,ComponentEventQueue *events,ObjectId owner,u64 instance,
    const scene::ComponentType &type,const resources::AnimationClip &clip,double from,double to,
    resources::AnimationWrapMode mode,u32 &budget,bool validated=false) {
  if(!events)return 0;
  u64 rejected=0;
  const auto suppressed=resources::visitAnimationCues(clip,from,to,mode,[&](const auto &cue){
    --budget;
    const scene::ComponentOperationValue payload[]{scene::ComponentOperationValue::makeInteger(cue.tag),
      scene::ComponentOperationValue::makeNumber(cue.value),scene::ComponentOperationValue::makeInteger(static_cast<i64>(cue.id))};
    if(!events->emit(world,owner,instance,type,"clip_event",payload))++rejected;
  },budget,validated);
  return suppressed>std::numeric_limits<u64>::max()-rejected?std::numeric_limits<u64>::max():suppressed+rejected;
}
inline void emitAnimationCueLoss(GameWorld &world,ComponentEventQueue *events,ObjectId owner,u64 instance,
    const scene::ComponentType &type,u64 count) {
  if(!count||!events)return;
  const auto payload=scene::ComponentOperationValue::makeInteger(static_cast<i64>(std::min(count,static_cast<u64>(std::numeric_limits<i64>::max()))));
  events->emit(world,owner,instance,type,"clip_events_lost",{&payload,1});
}
}
