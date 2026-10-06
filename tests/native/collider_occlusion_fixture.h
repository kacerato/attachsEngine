#pragma once
#include "convex_bake_fixture.h"
namespace ae::test {
inline editor::EditorEntityId addOcclusionWall(editor::EditorSession &s,editor::EditorEntityId source) {
  auto &g=s.document();const auto *part=g.find(source);if(!part)return 0;
  const auto original=*part; // Creating an entity may relocate the scene's storage.
  const auto wall=g.createEntity(g.root(),runtime::ObjectKind::Mesh,"VisibleWall");auto value=*g.find(wall);
  value.components=original.components;value.transform=original.transform;
  while(value.components.remove(scene::Collider::descriptor)){}
  while(value.components.remove(scene::PhysicsBody::descriptor)){}
  value.transform.position[2]-=2;value.transform.scale[0]=value.transform.scale[1]=2;
  value.transform.scale[2]=.25f;
  return g.applyEntityValues(wall,value)?wall:0;
}
}
