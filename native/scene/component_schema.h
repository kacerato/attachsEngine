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
#include "scene/light.h"
#include "scene/environment.h"
#include "scene/lod_group.h"
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
  // Contrato do plano universal de cenários (§6), na granularidade do TIPO:
  // quem lê os valores em execução, que capacidade do motor o componente exige
  // e o que precisa ser reconstruído quando qualquer propriedade dele muda.
  // Cada propriedade pode estreitar a capacidade e acrescentar invalidação;
  // nenhuma pode ficar sem consumidor (ver `scene/component_reflection.h`).
  std::string_view consumer{};
  std::string_view capability{};
  u32 invalidates = 0;
  bool allowMultiple() const noexcept { return type->allowMultiple; }
};

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
inline constexpr std::array<ComponentRule, 1> lookRequirements{{
  {"astra.camera", "Adicione Câmera a este objeto"}
}};
inline constexpr std::array<ComponentRule, 1> jointRequirements{{
  {"astra.physics.body", "Adicione Corpo físico a este objeto"}
}};

inline const std::array<ComponentSchema, 11> componentSchemas{{
  {&PhysicsBody::descriptor, "Corpo físico", "Massa e resposta física", ComponentCategory::Physics,
    {}, bodyConflicts, PlayMutability::Never, PlayMutability::SafePoint,
    "runtime/scene_physics.cpp → Jolt", {}, Invalidate::PhysicsBody},
  {&Character::descriptor, "Personagem", "Locomoção com cápsula", ComponentCategory::Physics,
    {}, characterConflicts, PlayMutability::Never, PlayMutability::SafePoint,
    "runtime/scene_physics.cpp → CharacterVirtual", {}, Invalidate::PhysicsBody|Invalidate::PhysicsShape},
  {&CameraLook::descriptor, "Olhar", "Rotação local da câmera por entrada ou script", ComponentCategory::Camera,
    lookRequirements, {}, PlayMutability::SafePoint, PlayMutability::SafePoint,
    "runtime/game_world.cpp → pose da câmera", {}, Invalidate::Input},
  {&Collider::descriptor, "Colisor 3D", "Volume de contato", ComponentCategory::Physics,
    {}, colliderConflicts, PlayMutability::Never, PlayMutability::SafePoint,
    "runtime/scene_physics.cpp → forma do Jolt", {}, Invalidate::PhysicsShape},
  {&Joint::descriptor, "Junta", "Conexão, limites e motor entre corpos", ComponentCategory::Physics,
    jointRequirements, {}, PlayMutability::Never, PlayMutability::SafePoint,
    "runtime/scene_physics.cpp → constraint do Jolt", {}, Invalidate::PhysicsBody},
  {&Camera::descriptor, "Câmera", "Projeção e enquadramento", ComponentCategory::Camera,
    {}, {}, PlayMutability::SafePoint, PlayMutability::SafePoint,
    "renderer/render_view.h → matriz de projeção e culling", {}, Invalidate::Draw},
  {&MeshRenderer::descriptor, "Malha", "Geometria e material", ComponentCategory::Visual,
    {}, {}, PlayMutability::SafePoint, PlayMutability::SafePoint,
    "renderer/map_draw_update.h → instância e material efetivo", "render.material.pbr",
    Invalidate::Draw|Invalidate::MaterialDescriptor},
  {&Light::descriptor, "Luz", "Direcional, pontual ou spot", ComponentCategory::Visual,
    {}, {}, PlayMutability::SafePoint, PlayMutability::SafePoint,
    "runtime/scene_lights.cpp → renderer/punctual_lights.h", {}, Invalidate::LightCluster},
  {&Environment::descriptor, "Ambiente", "Céu, atmosfera, neblina e pós globais ou por volume", ComponentCategory::Visual,
    {}, {}, PlayMutability::SafePoint, PlayMutability::SafePoint,
    "runtime/scene_environment.cpp → renderer e pós", {}, Invalidate::Draw|Invalidate::Policy},
  {&LodGroup::descriptor, "LOD Group", "Nível de detalhe pela altura na tela", ComponentCategory::Visual,
    {}, {}, PlayMutability::SafePoint, PlayMutability::SafePoint,
    "runtime/lod_groups.h → visibilidade do desenho por vista", "render.lod.group", Invalidate::Draw},
  {&ScriptBehavior::descriptor, "Comportamento", "Código C# do projeto", ComponentCategory::Script,
    {}, {}, PlayMutability::Never, PlayMutability::Never,
    "runtime/script_bridge.cpp → runtime .NET", {}, Invalidate::Script}
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

// Resolve the closure before allocating any component. The inspector uses the
// same resolver without cloning large resource/script payloads every frame.
struct ComponentCompositionPlan {
  Components candidate;
  std::vector<std::string_view> addedTypes;
  u64 requestedInstance=0;
  const char *error=nullptr;
  bool ready=false;
};
inline ComponentCompositionPlan planComponentAddition(const Components &source,
    std::string_view requestedType, bool inPlay=false, bool materialize=true) {
  ComponentCompositionPlan plan;
  std::vector<std::string_view> visiting;
  const auto present=[&](std::string_view id) {
    return source.find(id) || std::find(plan.addedTypes.begin(),plan.addedTypes.end(),id)!=plan.addedTypes.end();
  };
  const auto add=[&](auto &&self,std::string_view id,bool dependency)->bool {
    const auto *schema=findComponentSchema(id);
    if(!schema) {plan.error="Tipo de componente não registrado";return false;}
    if(dependency && present(id)) return true;
    if(!dependency && !schema->allowMultiple() && present(id)) {
      plan.error="Este objeto já possui este componente";return false;
    }
    if(std::find(visiting.begin(),visiting.end(),id)!=visiting.end()) {
      plan.error="Dependências de componentes formam um ciclo";return false;
    }
    if(inPlay && schema->structuralInPlay==PlayMutability::Never) {
      plan.error="Uma dependência não pode ser adicionada durante Play";return false;
    }
    visiting.push_back(id);
    for(const auto &rule:schema->requirements) if(!self(self,rule.typeId,true)) return false;
    for(const auto &rule:schema->conflicts) if(present(rule.typeId)) {plan.error=rule.message;return false;}
    // Honor asymmetric declarations too; adding B cannot evade A's conflict.
    for(const auto &existing:componentSchemas) {
      if(present(existing.type->id)) for(const auto &rule:existing.conflicts) if(rule.typeId==id) {
        plan.error="Incompatível com um componente já anexado";return false;
      }
    }
    if(source.size()+plan.addedTypes.size()>=Components::MaximumCount) {
      plan.error="Limite de componentes atingido";return false;
    }
    plan.addedTypes.push_back(id);
    visiting.pop_back();return true;
  };
  plan.ready=add(add,requestedType,false);
  if(plan.ready && materialize) {
    plan.candidate=source;
    for(const auto id:plan.addedTypes) {
      auto *created=plan.candidate.add(*findComponentSchema(id)->type);
      if(!created) {plan.error="Falha ao criar componente";plan.ready=false;break;}
      if(id==requestedType) plan.requestedInstance=created->instanceId();
    }
  }
  return plan;
}
inline const char *componentAdditionBlockedReason(const ComponentSchema &schema,const Components &source) {
  return planComponentAddition(source,schema.type->id,false,false).error;
}
inline const ComponentSchema *componentInstanceRemovalBlockedBy(u64 instance,const Components &source) {
  const auto *removed=source.findInstance(instance);if(!removed) return nullptr;
  for(usize i=0;i<source.size();++i) {
    const auto *other=source.at(i);
    if(other->instanceId()!=instance && other->type().id==removed->type().id) return nullptr;
  }
  return componentRemovalBlockedBy(removed->type().id,source);
}

} // namespace ae::scene
