// Família Física 3D (backend Jolt). Incluído somente por scene/component_schema.h.
#pragma once
#include "scene/character.h"
#include "scene/dynamic_body_motor.h"
#include "scene/collider.h"
#include "scene/collision_recipe.h"
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
inline constexpr std::array<ComponentRule, 3> characterConflicts{{
  {"astra.physics.body", "Incompatível com corpo físico"},
  {"astra.physics.collider", "O personagem traz a própria cápsula; remova o Colisor 3D"},
  {"astra.physics.dynamic_motor", "Escolha Character ou motor de corpo dinâmico"}
}};
inline constexpr std::array<ComponentRule, 1> colliderConflicts{{
  {"astra.physics.character", "O personagem já possui cápsula própria"}
}};
inline constexpr std::array<ComponentRule, 1> jointRequirements{{
  {"astra.physics.body", "Adicione Corpo físico a este objeto"}
}};
inline constexpr std::array<ComponentRule,1> dynamicMotorRequirements{{
  {"astra.physics.body","Adicione Corpo físico dinâmico ao receptor"}
}};
inline constexpr std::array<ComponentRule,2> physicsConnectionRequirements{{
  {"astra.physics.body","Adicione Corpo físico ao emissor da conexão"},
  {"astra.physics.collider","Adicione Colisor 3D ao emissor da conexão"}
}};
#include "scene/generated/physics3d_schemas.inc"
} // namespace ae::scene
