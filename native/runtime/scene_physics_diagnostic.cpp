#include "runtime/scene_physics.h"
#include <algorithm>
#include <cmath>
namespace ae::runtime {
bool ScenePhysics::diagnostic(const GameWorld &world, ObjectId id,
                              float distance, Diagnostic &out) const {
  if (!world_ || ownerWorldId_ != world.worldId() ||
      !world.alive(world.handle(id)) || !std::isfinite(distance) ||
      distance <= 0 || distance > 1000)
    return false;
  Diagnostic value;
  value.object = id;
  value.authority = world.authorityOf(world.handle(id));
  value.hasControl=motorControlState(id,value.control);
  for (const auto &c : characters_)
    if (c.id == id) {
      if (!c.motor->runtimeState(value.character))
        return false;
      value.hasCharacter = true;
      value.hasSupport = value.character.hasGroundPoint && value.character.groundState == AetherCharacterGroundState::OnGround;
      value.support = objectForBody(value.character.groundBody);
      value.motorSupport = true;
      value.supportPoint = value.character.groundPoint;
      value.supportNormal = value.character.groundNormal;
    }
  for (const auto &binding : bindings_)
    if (binding.id == id) {
      if (!AetherPhysics_BodyCommandV1(world_, binding.body, 0, {}, {},
                                       &value.body))
        return false;
      value.hasBody = true;
      value.colliderCount = static_cast<u32>(binding.colliders.size());
      for (const auto &identity : binding.colliders) {
        const auto *e = world.graph().find(identity.object);
        const auto *c =
            e ? e->components.findInstance(identity.instance) : nullptr;
        if (c && &c->type() == &scene::Collider::descriptor)
          ++value.shapeCounts[static_cast<u32>(
              static_cast<const scene::Collider &>(*c).shape)];
      }
      if (!AetherPhysics_TryGetBodyGroundProbesV1(
              world_, binding.body, value.probes, &value.probeCount))
        return false;
      DynamicMotorState motor;
      if (dynamicMotorState(id, motor) && motor.hasMeasuredStep) {
        value.motorSupport = true;
        value.hasSupport = motor.grounded;
        value.support = motor.support;
        value.supportPoint = {motor.point[0], motor.point[1], motor.point[2]};
        value.supportNormal = {motor.normal[0], motor.normal[1],
                               motor.normal[2]};
      } else {
        QueryFilter filter;
        filter.ignore = id;
        filter.gameplayLayerMask = 0;
        const auto *e = world.graph().find(id);
        for (u32 layer = 0; layer < GameplayLayers::kCount; ++layer)
          if (world.graph().layers().interacts(e->layer, layer))
            filter.gameplayLayerMask |= 1u << layer;
        float nearest = distance + 1;
        for (u32 i = 0; i < value.probeCount; ++i) {
          const float origin[]{value.probes[i].x, value.probes[i].y + .02f,
                               value.probes[i].z},
              direction[]{0, -distance - .02f, 0};
          QueryHit hit;
          if (rayCast(origin, direction, filter, hit) && hit.hasNormal &&
              hit.normal[1] > 0 && hit.distance < nearest) {
            nearest = hit.distance;
            value.hasSupport = true;
            value.support = hit.object;
            value.supportPoint = {hit.point[0], hit.point[1], hit.point[2]};
            value.supportNormal = {hit.normal[0], hit.normal[1], hit.normal[2]};
          }
        }
      }
    }
  out = value;
  return true;
}
} // namespace ae::runtime
