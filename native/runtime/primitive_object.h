#pragma once
#include "runtime/scene_graph.h"
#include "scene/primitive.h"
#include "scene/mesh_renderer.h"
#include "scene/physics_body.h"
#include "scene/collider.h"

namespace ae::runtime {
struct PrimitiveResource {
  u32 mesh=0;
  resources::AssetGuid asset{};
  scene::MaterialParameters material;
};
inline bool configurePrimitive(SceneObject &object,scene::PrimitiveType type,const PrimitiveResource &resource) {
  if(!scene::validPrimitive(type) || !resource.mesh || !resource.asset.valid()) return false;
  auto *mesh=static_cast<scene::MeshRenderer*>(object.components.add(scene::MeshRenderer::descriptor));
  if(!mesh) mesh=static_cast<scene::MeshRenderer*>(object.components.edit(scene::MeshRenderer::descriptor));
  auto *body=static_cast<scene::PhysicsBody*>(object.components.add(scene::PhysicsBody::descriptor));
  if(!body) body=static_cast<scene::PhysicsBody*>(object.components.edit(scene::PhysicsBody::descriptor));
  auto *collider=static_cast<scene::Collider*>(object.components.add(scene::Collider::descriptor));
  if(!mesh || !body || !collider) return false;
  mesh->mesh=resource.mesh;mesh->asset=resource.asset;mesh->material=resource.material;
  body->motion=scene::BodyMotion::Static;
  collider->shape=type==scene::PrimitiveType::Sphere?scene::ColliderShape::Sphere:
    type==scene::PrimitiveType::Capsule?scene::ColliderShape::Capsule:
    type==scene::PrimitiveType::Cube?scene::ColliderShape::Box:scene::ColliderShape::Mesh;
  collider->convex=type==scene::PrimitiveType::Cylinder;
  return object.components.valid();
}
}
