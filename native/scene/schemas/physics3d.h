// Família Física 3D (backend Jolt). Incluído somente por scene/component_schema.h.
#pragma once
#include "scene/character.h"
#include "scene/collider.h"
#include "scene/joint.h"
#include "scene/physics_body.h"
#include "scene/constant_force.h"
#include "scene/physics_event_connection.h"
#include "scene/physics_field.h"

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
inline constexpr std::array<ComponentRule,2> physicsConnectionRequirements{{
  {"astra.physics.body","Adicione Corpo físico ao emissor da conexão"},
  {"astra.physics.collider","Adicione Colisor 3D ao emissor da conexão"}
}};
inline constexpr std::array<ComponentSchema, 10> physics3dSchemas{{
  {.type=&GravityField::descriptor,.name="Campo de gravidade",.description="Gravidade local em volume sobre corpos dinâmicos",.family=ComponentFamily::Physics3D,.structuralInPlay=PlayMutability::SafePoint,.consumer="runtime/scene_physics_fields.inl → Jolt",.subfamily="Campos",.icon="physics/field-gravity",.searchTerms="Area3D Gravity Zone Gravidade",.reference="https://docs.godotengine.org/en/4.5/classes/class_area3d.html",.apiName="GravityField"},
  {.type=&WindField::descriptor,.name="Campo de vento",.description="Arrasto para velocidade local do ar, dependente da massa",.family=ComponentFamily::Physics3D,.structuralInPlay=PlayMutability::SafePoint,.consumer="runtime/scene_physics_fields.inl → Jolt",.subfamily="Campos",.icon="physics/field-wind",.searchTerms="Area3D Wind Vento",.reference="https://docs.godotengine.org/en/4.5/classes/class_area3d.html",.apiName="WindField"},
  {.type=&DragField::descriptor,.name="Campo de arrasto",.description="Amortecimento linear e angular local",.family=ComponentFamily::Physics3D,.structuralInPlay=PlayMutability::SafePoint,.consumer="runtime/scene_physics_fields.inl → Jolt",.subfamily="Campos",.icon="physics/field-drag",.searchTerms="Area3D Damp Drag Arrasto",.reference="https://docs.godotengine.org/en/4.5/classes/class_area3d.html",.apiName="DragField"},
  {.type=&RadialField::descriptor,.name="Campo radial",.description="Atração, repulsão e vórtice ao redor do centro",.family=ComponentFamily::Physics3D,.structuralInPlay=PlayMutability::SafePoint,.consumer="runtime/scene_physics_fields.inl → Jolt",.subfamily="Campos",.icon="physics/field-radial",.searchTerms="Area3D Point Gravity Vortex Radial",.reference="https://docs.godotengine.org/en/4.5/classes/class_area3d.html",.apiName="RadialField"},
  {.type=&PhysicsEventConnection3D::descriptor, .name="Conexão física 3D", .description="Evento de sensor/contato altera a ativação de um receptor",
   .family=ComponentFamily::Physics3D, .requirements=physicsConnectionRequirements,
   .structuralInPlay=PlayMutability::SafePoint, .consumer="runtime/scene_physics_connections.h → GameWorld::setActive", .invalidates=Invalidate::PhysicsBody,
   .subfamily="Eventos", .icon="event/physics-connection", .searchTerms="Trigger Collision Area3D Signal Evento Conexao",
   .reference="https://docs.godotengine.org/en/4.5/classes/class_area3d.html", .apiName="PhysicsEventConnection3D"},
  {.type=&ConstantForce::descriptor, .name="Força constante", .description="Força e torque contínuos sobre corpo dinâmico",
   .family=ComponentFamily::Physics3D, .requirements=jointRequirements,
   .structuralInPlay=PlayMutability::SafePoint,
   .consumer="runtime/scene_physics.cpp → ApplyBodyForceV1 antes de cada passo Jolt",
   .subfamily="Forças", .icon="component/constant-force", .searchTerms="ConstantForce AddRelativeForce AddRelativeTorque Propulsão",
   .reference="https://docs.unity3d.com/6000.0/Documentation/Manual/class-ConstantForce.html",
   .apiName="ConstantForce"},
  {.type=&PhysicsBody::descriptor, .name="Corpo físico", .description="Massa e resposta física",
   .family=ComponentFamily::Physics3D, .conflicts=bodyConflicts,
   .consumer="runtime/scene_physics.cpp → Jolt", .invalidates=Invalidate::PhysicsBody,
   .subfamily="Corpos", .icon="component/physics", .searchTerms="Rigidbody RigidBody3D StaticBody3D Massa",
   .reference="https://docs.unity3d.com/6000.0/Documentation/Manual/class-Rigidbody.html",
   .apiName="PhysicsBody"},
  {.type=&Character::descriptor, .name="Personagem", .description="Locomoção com cápsula",
   .family=ComponentFamily::Physics3D, .conflicts=characterConflicts,
   .consumer="runtime/scene_physics.cpp → CharacterVirtual", .invalidates=Invalidate::PhysicsBody|Invalidate::PhysicsShape,
   .subfamily="Corpos", .icon="component/character", .searchTerms="CharacterController CharacterBody3D Jogador",
   .reference="https://docs.unity3d.com/6000.0/Documentation/Manual/class-CharacterController.html",
   .apiName="Character"},
  {.type=&Collider::descriptor, .name="Colisor 3D", .description="Volume de contato",
   .family=ComponentFamily::Physics3D, .conflicts=colliderConflicts,
   .consumer="runtime/scene_physics.cpp → forma do Jolt", .invalidates=Invalidate::PhysicsShape,
   .subfamily="Formas", .icon="component/collider",
   .searchTerms="BoxCollider SphereCollider CapsuleCollider MeshCollider CylinderShape3D CollisionShape3D",
   .reference="https://docs.unity3d.com/6000.0/Documentation/Manual/class-BoxCollider.html",
   .apiName="Collider"},
  {.type=&Joint::descriptor, .name="Junta", .description="Nove mecanismos Jolt com limites, referenciais e motores",
   .family=ComponentFamily::Physics3D, .requirements=jointRequirements,
   .structuralInPlay=PlayMutability::SafePoint,
   .consumer="runtime/scene_physics.cpp → constraint do Jolt", .invalidates=Invalidate::PhysicsBody,
   .subfamily="Juntas", .icon="component/joint", .searchTerms="HingeJoint SliderJoint SpringJoint FixedJoint ConeTwistJoint3D Generic6DOFJoint3D",
   .reference="https://docs.godotengine.org/en/4.5/classes/class_generic6dofjoint3d.html",
   .apiName="Joint"}
}};
} // namespace ae::scene
