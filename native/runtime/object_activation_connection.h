#pragma once
#include "runtime/game_world.h"

namespace ae::runtime {
inline WorldStatus applyObjectActivationConnection(GameWorld &world,ObjectId receiver,u32 action,u32 count=1) {
  if(action==0)return WorldStatus::Ok;
  if(action>3)return WorldStatus::InvalidArgument;
  const auto target=world.handle(receiver);const auto status=world.validate(target);
  if(status!=WorldStatus::Ok)return status;
  const bool active=action==1?true:action==2?false:((count&1u)?!world.activeSelf(target):world.activeSelf(target));
  return world.setActive(target,active);
}
}
