#include "runtime/scene_player_input.h"

#include "scene/player_input.h"

#include <algorithm>
#include <vector>

namespace ae::runtime {
u32 emitPlayerInputEvents(GameWorld &world,const InputService &input,ComponentEventQueue &events) {
  if(!world.running()) return 0;
  const auto &actions=input.map().actions();
  std::vector<ObjectId> ids;
  world.graph().collectSubtree(world.graph().root(),ids);
  u32 emitted=0;
  for(const ObjectId id:ids) {
    const auto handle=world.handle(id);
    if(!world.activeInHierarchy(handle)) continue;
    for(u32 ordinal=0;;++ordinal) {
      const auto component=world.findComponent(handle,scene::PlayerInput::descriptor.id,ordinal);
      if(!component.valid()) break;
      const auto *value=world.readComponent(component);
      if(!value) break;
      const auto player=scene::playerInput(*value);
      if(!player.enabled) continue;
      const usize count=std::min<usize>(actions.size(),scene::kPlayerInputActionCount);
      for(usize i=0;i<count;++i) {
        if(!((player.actions>>i)&1u)) continue;
        const auto &name=actions[i].id;
        const auto payload=scene::ComponentOperationValue::makeInteger(static_cast<i64>(i+1));
        if(input.justPressed(name)&&events.emit(world,id,component.instance,scene::PlayerInput::descriptor,"performed",std::span(&payload,1))) ++emitted;
        if(input.justReleased(name)&&events.emit(world,id,component.instance,scene::PlayerInput::descriptor,"canceled",std::span(&payload,1))) ++emitted;
      }
    }
  }
  return emitted;
}
} // namespace ae::runtime
