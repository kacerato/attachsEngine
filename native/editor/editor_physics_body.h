#pragma once
#include "editor/editor_document.h"
#include "scene/physics_body.h"
namespace ae::editor {
using EditorPhysicsBody=scene::PhysicsBody;
using scene::physicsBodyNumbers;
using EditorCollider=scene::Collider;
using scene::colliderNumbers;
inline const EditorCollider *colliderComponent(const EditorEntity &e,u64 instance=0) {
  const auto *c=instance?e.components.findInstance(instance):e.components.find(EditorCollider::descriptor);
  return c && &c->type()==&EditorCollider::descriptor?static_cast<const EditorCollider*>(c):nullptr;
}
inline EditorCollider *editCollider(EditorEntity &e,u64 instance=0) {
  auto *c=instance?e.components.editInstance(instance):e.components.edit(EditorCollider::descriptor);
  return c && &c->type()==&EditorCollider::descriptor?static_cast<EditorCollider*>(c):nullptr;
}
inline const EditorPhysicsBody *physicsBody(const EditorEntity &e) {
  return static_cast<const EditorPhysicsBody*>(e.components.find(EditorPhysicsBody::descriptor));
}
inline EditorPhysicsBody *editPhysicsBody(EditorEntity &e) {
  return static_cast<EditorPhysicsBody*>(e.components.edit(EditorPhysicsBody::descriptor));
}
}
