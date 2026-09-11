// O contrato comum dos componentes: uma lista só, lida por todo mundo.
//
// O descritor (`ComponentType`) diz o que um componente É — id persistente,
// versão, propriedades reflexivas, migração. O schema diz o que o EDITOR e o
// RUNTIME podem fazer com ele: em que categoria aparece, o que exige, com o que
// é incompatível, e o que pode mudar com o Play rodando.
//
// Essa separação existe porque a alternativa já esteve no repositório: uma
// tabela no catálogo do inspetor e outra no caminho de execução, mantidas à mão.
// Duas listas independentes divergem — um componente ganha uma regra em uma
// delas e a API em C# continua aceitando o que a interface recusa.
//
// Quem consome este arquivo: o catálogo do inspetor (ícone e grupo de
// propriedades continuam sendo decisão da UI), o mundo de execução
// (runtime/game_world.cpp) e, por ele, a API de componentes em C#.
#pragma once
#include "scene/components.h"
#include "scene/physics_body.h"
#include "scene/collider.h"
#include "scene/character.h"
#include "scene/camera.h"
#include "scene/camera_look.h"
#include "scene/mesh_renderer.h"
#include "scene/joint.h"
#include "scene/script_behavior.h"

#include <array>

namespace ae::scene {

enum class ComponentCategory : u32 { Camera = 1, Visual = 2, Physics = 3, Script = 4 };

// Quando uma alteração pode ser aceita com o Play rodando. `SafePoint` significa
// que ela entra na fila do mundo de execução e é aplicada entre passos, nunca
// no meio de um callback de script ou de física.
enum class PlayMutability : u32 { Never, SafePoint };

struct ComponentRule {
  std::string_view typeId;
  const char *message;
};

struct ComponentSchema {
  const ComponentType *type;
  const char *name;
  const char *description;
  ComponentCategory category;
  // Tipos que precisam estar no MESMO objeto para este componente existir.
  std::span<const ComponentRule> requirements{};
  // Tipos que não podem coexistir com este no mesmo objeto.
  std::span<const ComponentRule> conflicts{};
  PlayMutability structuralInPlay = PlayMutability::Never;
  PlayMutability propertiesInPlay = PlayMutability::SafePoint;
  bool allowMultiple() const noexcept { return type->allowMultiple; }
};

inline constexpr std::array<ComponentRule, 1> bodyConflicts{{
  {"astra.physics.character", "Incompatível com personagem cápsula"}
}};
inline constexpr std::array<ComponentRule, 2> characterConflicts{{
  {"astra.physics.body", "Incompatível com corpo físico"},
  {"astra.physics.collider", "O personagem já possui cápsula própria"}
}};
inline constexpr std::array<ComponentRule, 1> colliderConflicts{{
  {"astra.physics.character", "O personagem já possui cápsula própria"}
}};
inline constexpr std::array<ComponentRule, 1> lookRequirements{{
  {"astra.camera", "Adicione Câmera a este objeto"}
}};
inline constexpr std::array<ComponentRule, 1> jointRequirements{{
  {"astra.physics.body", "Adicione Corpo físico a este objeto"}
}};

inline const std::array<ComponentSchema, 8> componentSchemas{{
  {&PhysicsBody::descriptor, "Corpo físico", "Massa e resposta física", ComponentCategory::Physics,
    {}, bodyConflicts, PlayMutability::Never, PlayMutability::SafePoint},
  {&Character::descriptor, "Personagem", "Locomoção com cápsula", ComponentCategory::Physics,
    {}, characterConflicts, PlayMutability::Never, PlayMutability::SafePoint},
  {&CameraLook::descriptor, "Olhar", "Rotação da câmera por toque", ComponentCategory::Camera,
    lookRequirements, {}, PlayMutability::SafePoint, PlayMutability::SafePoint},
  {&Collider::descriptor, "Colisor 3D", "Volume de contato", ComponentCategory::Physics,
    {}, colliderConflicts, PlayMutability::Never, PlayMutability::SafePoint},
  {&Joint::descriptor, "Junta", "Conexão, limites e motor entre corpos", ComponentCategory::Physics,
    jointRequirements, {}, PlayMutability::Never, PlayMutability::SafePoint},
  {&Camera::descriptor, "Câmera", "Perspectiva e enquadramento", ComponentCategory::Camera,
    {}, {}, PlayMutability::SafePoint, PlayMutability::SafePoint},
  {&MeshRenderer::descriptor, "Malha", "Geometria e material", ComponentCategory::Visual,
    {}, {}, PlayMutability::SafePoint, PlayMutability::SafePoint},
  {&ScriptBehavior::descriptor, "Comportamento", "Código C# do projeto", ComponentCategory::Script,
    {}, {}, PlayMutability::Never, PlayMutability::Never}
}};

inline const ComponentSchema *findComponentSchema(std::string_view id) {
  for (const auto &schema : componentSchemas) if (schema.type->id == id) return &schema;
  return nullptr;
}
inline const ComponentSchema *findComponentSchema(const ComponentType &type) {
  for (const auto &schema : componentSchemas) if (schema.type == &type) return &schema;
  return nullptr;
}

// Motivo pelo qual este tipo não pode ser anexado à coleção, ou nullptr quando
// pode. Um componente já presente que não aceita múltiplas instâncias também é
// um motivo — a interface mostra a entrada desabilitada em vez de sumir com ela.
inline const char *componentUnavailableReason(const ComponentSchema &schema, const Components &components) {
  for (const auto &rule : schema.conflicts) if (components.find(rule.typeId)) return rule.message;
  for (const auto &rule : schema.requirements) if (!components.find(rule.typeId)) return rule.message;
  if (!schema.allowMultiple() && components.find(schema.type->id)) return "Este objeto já possui este componente";
  return nullptr;
}

// O componente que ainda depende de `typeId`, ou nullptr quando a remoção é
// aceitável. Remover Câmera com Olhar anexado quebraria a exigência declarada
// pelo Olhar; quem chama compõe a mensagem com o nome devolvido.
inline const ComponentSchema *componentRemovalBlockedBy(std::string_view typeId, const Components &components) {
  for (usize i = 0; i < components.size(); ++i) {
    const auto *value = components.at(i);
    if (value->type().id == typeId) continue;
    const auto *schema = findComponentSchema(value->type().id);
    if (!schema) continue;
    for (const auto &rule : schema->requirements)
      if (rule.typeId == typeId) return schema;
  }
  return nullptr;
}

} // namespace ae::scene
