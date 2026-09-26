// Família Física 3D (backend Jolt). Incluído somente por scene/component_schema.h.
#pragma once
#include "scene/character.h"
#include "scene/collider.h"
#include "scene/joint.h"
#include "scene/physics_body.h"

namespace ae::scene {
inline constexpr std::array<ComponentRule, 1> bodyConflicts{{
  {"astra.physics.character", "Incompatível com personagem cápsula"}
}};
// As duas direções do MESMO conflito precisam de frases diferentes.
//
// A mensagem é lida por quem tentou anexar o componente que está sendo
// recusado, e descreve o que fazer. Uma frase só, reusada nos dois sentidos,
// fala do objeto errado: num objeto sem personagem, recusar `Personagem` com
// "o personagem já possui cápsula própria" explica uma situação que não existe.
inline constexpr std::array<ComponentRule, 2> characterConflicts{{
  {"astra.physics.body", "Incompatível com corpo físico"},
  {"astra.physics.collider", "O personagem traz a própria cápsula; remova o Colisor 3D"}
}};
inline constexpr std::array<ComponentRule, 1> colliderConflicts{{
  {"astra.physics.character", "O personagem já possui cápsula própria"}
}};
inline constexpr std::array<ComponentRule, 1> jointRequirements{{
  {"astra.physics.body", "Adicione Corpo físico a este objeto"}
}};
inline constexpr std::array<ComponentSchema, 4> physics3dSchemas{{
  {.type=&PhysicsBody::descriptor, .name="Corpo físico", .description="Massa e resposta física",
   .family=ComponentFamily::Physics3D, .conflicts=bodyConflicts,
   .consumer="runtime/scene_physics.cpp → Jolt", .invalidates=Invalidate::PhysicsBody,
   .subfamily="Corpos", .icon="component/physics", .searchTerms="Rigidbody RigidBody3D StaticBody3D Massa",
   .reference="https://docs.unity3d.com/6000.0/Documentation/Manual/class-Rigidbody.html"},
  {.type=&Character::descriptor, .name="Personagem", .description="Locomoção com cápsula",
   .family=ComponentFamily::Physics3D, .conflicts=characterConflicts,
   .consumer="runtime/scene_physics.cpp → CharacterVirtual", .invalidates=Invalidate::PhysicsBody|Invalidate::PhysicsShape,
   .subfamily="Corpos", .icon="component/character", .searchTerms="CharacterController CharacterBody3D Jogador",
   .reference="https://docs.unity3d.com/6000.0/Documentation/Manual/class-CharacterController.html"},
  {.type=&Collider::descriptor, .name="Colisor 3D", .description="Volume de contato",
   .family=ComponentFamily::Physics3D, .conflicts=colliderConflicts,
   .consumer="runtime/scene_physics.cpp → forma do Jolt", .invalidates=Invalidate::PhysicsShape,
   .subfamily="Formas", .icon="component/collider",
   .searchTerms="BoxCollider SphereCollider CapsuleCollider MeshCollider CollisionShape3D",
   .reference="https://docs.unity3d.com/6000.0/Documentation/Manual/class-BoxCollider.html"},
  {.type=&Joint::descriptor, .name="Junta", .description="Conexão, limites e motor entre corpos",
   .family=ComponentFamily::Physics3D, .requirements=jointRequirements,
   .consumer="runtime/scene_physics.cpp → constraint do Jolt", .invalidates=Invalidate::PhysicsBody,
   .subfamily="Juntas", .icon="component/joint", .searchTerms="HingeJoint SliderJoint SpringJoint Joint3D",
   .reference="https://docs.unity3d.com/6000.0/Documentation/Manual/class-HingeJoint.html"}
}};
} // namespace ae::scene
