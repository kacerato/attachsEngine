#pragma once
#include "editor/editor_document.h"
#include "runtime/scene_components.h"
#include "scene/physics_body.h"
namespace ae::editor {
using EditorPhysicsBody=scene::PhysicsBody;
using scene::physicsBodyNumbers;
using EditorCollider=scene::Collider;
using scene::colliderNumbers;
// Definidos em runtime/scene_components.h: um acessor só no programa inteiro.
using runtime::colliderComponent;
using runtime::editCollider;
using runtime::physicsBody;
using runtime::editPhysicsBody;
}
