// Os acessores tipados de componente, definidos uma vez só.
//
// Eles existiam em `editor/editor_physics_body.h`, `editor_character.h` e
// companhia, cada um em `ae::editor`. Como o objeto de cena agora é
// `runtime::SceneObject`, mantê-los lá obrigaria o mundo de execução a incluir
// cabeçalhos do editor para perguntar se um objeto tem corpo físico. Os
// cabeçalhos do editor passaram a reexportar estes com `using`, então continua
// existindo um único `physicsBody(objeto)` no programa.
#pragma once
#include "runtime/scene_graph.h"
#include "scene/camera.h"
#include "scene/camera_look.h"
#include "scene/character.h"
#include "scene/collider.h"
#include "scene/joint.h"
#include "scene/mesh_renderer.h"
#include "scene/physics_body.h"
#include "scene/script_behavior.h"

#include <limits>

namespace ae::runtime {

inline const scene::PhysicsBody *physicsBody(const SceneObject &e) {
  return static_cast<const scene::PhysicsBody *>(e.components.find(scene::PhysicsBody::descriptor));
}
inline scene::PhysicsBody *editPhysicsBody(SceneObject &e) {
  return static_cast<scene::PhysicsBody *>(e.components.edit(scene::PhysicsBody::descriptor));
}
inline const scene::Collider *colliderComponent(const SceneObject &e, u64 instance = 0) {
  const auto *c = instance ? e.components.findInstance(instance) : e.components.find(scene::Collider::descriptor);
  return c && &c->type() == &scene::Collider::descriptor ? static_cast<const scene::Collider *>(c) : nullptr;
}
inline scene::Collider *editCollider(SceneObject &e, u64 instance = 0) {
  auto *c = instance ? e.components.editInstance(instance) : e.components.edit(scene::Collider::descriptor);
  return c && &c->type() == &scene::Collider::descriptor ? static_cast<scene::Collider *>(c) : nullptr;
}
inline const scene::Character *characterComponent(const SceneObject &e) {
  return static_cast<const scene::Character *>(e.components.find(scene::Character::descriptor));
}
inline scene::Character *editCharacter(SceneObject &e) {
  return static_cast<scene::Character *>(e.components.edit(scene::Character::descriptor));
}
inline const scene::Camera *cameraComponent(const SceneObject &e) {
  return static_cast<const scene::Camera *>(e.components.find(scene::Camera::descriptor));
}
inline scene::Camera *editCamera(SceneObject &e) {
  return static_cast<scene::Camera *>(e.components.edit(scene::Camera::descriptor));
}
inline const scene::CameraLook *cameraLook(const SceneObject &e) {
  return static_cast<const scene::CameraLook *>(e.components.find(scene::CameraLook::descriptor));
}
inline scene::CameraLook *editCameraLook(SceneObject &e) {
  return static_cast<scene::CameraLook *>(e.components.edit(scene::CameraLook::descriptor));
}
inline const scene::MeshRenderer *meshRenderer(const SceneObject &e) {
  return static_cast<const scene::MeshRenderer *>(e.components.find(scene::MeshRenderer::descriptor));
}
inline scene::MeshRenderer *editMeshRenderer(SceneObject &e) {
  return static_cast<scene::MeshRenderer *>(e.components.edit(scene::MeshRenderer::descriptor));
}
inline u32 meshAsset(const SceneObject &e) {
  const auto *m = meshRenderer(e);
  return m ? m->mesh : 0;
}
inline const scene::MaterialParameters &meshMaterial(const SceneObject &e) {
  static const scene::MaterialParameters defaults;
  const auto *m = meshRenderer(e);
  return m ? m->material : defaults;
}

// Drafts may retain a null reference; execution requires a compatible live target.
inline bool referenceAccepts(const SceneGraph &graph, ObjectId source,
    const scene::ComponentObjectReference &property, u64 target, bool requireTarget = false) {
  if (!target && !requireTarget) return true;
  if (!target && property.scope == scene::ObjectReferenceScope::SelfOrAncestor) target = source;
  if (!target || target > std::numeric_limits<ObjectId>::max()) return false;
  const auto id = static_cast<ObjectId>(target);
  const auto *object = graph.find(id);
  if (!object || (!property.requiredType.empty() && !object->components.find(property.requiredType))) return false;
  if (property.scope == scene::ObjectReferenceScope::Other && id == source) return false;
  if (property.scope == scene::ObjectReferenceScope::SelfOrAncestor && id != source && !graph.isDescendantOf(source, id)) return false;
  if (property.scope == scene::ObjectReferenceScope::SelfOrAncestor && !property.requiredType.empty())
    for (auto p = source; p != id; p = graph.find(p)->parent)
      if (graph.find(p)->components.find(property.requiredType)) return false;
  return true;
}
inline bool referencesAccept(const SceneGraph &graph, ObjectId source, const scene::ComponentValue &value) {
  for (const auto &property : value.type().references)
    if (!referenceAccepts(graph, source, property, property.read(value))) return false;
  return true;
}

} // namespace ae::runtime
