// O contrato comum dos componentes: uma lista só, lida por todo mundo.
//
// O descritor (`ComponentType`) diz o que um componente É — id persistente,
// versão, propriedades reflexivas, migração. O schema diz o que o EDITOR e o
// RUNTIME podem fazer com ele: em que família aparece, o que exige, com o que
// é incompatível, e o que pode mudar com o Play rodando.
//
// Essa separação existe porque a alternativa já esteve no repositório: uma
// tabela no catálogo do inspetor e outra no caminho de execução, mantidas à mão.
// Duas listas independentes divergem — um componente ganha uma regra em uma
// delas e a API em C# continua aceitando o que a interface recusa.
//
// **Registro por família.** tools/component_contracts/<família>.json gera a
// tabela incluída por scene/schemas/<família>.h e a concatenação constexpr.
// Tipos, requisitos e consumidores continuam sendo C++ real. O Add, o Inspector,
// o arquivo, o mundo e a API C# usam este registro. Para atualizar todos os
// bindings: python tools/generate-component-contracts.py --sync-api.
//
// Quem consome este arquivo: o catálogo do inspetor, o mundo de execução
// (runtime/game_world.cpp), o arquivo de cena e, pelo mundo, a API em C#.
#pragma once
#include "scene/components.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <vector>

namespace ae::scene {

// Famílias do catálogo, na ordem em que o Add as apresenta. Os valores são
// estáveis porque a UI guarda a família escolhida; família sem tipo registrado
// simplesmente não aparece.
enum class ComponentFamily : u32 {
  Logic, Rendering, Lighting, Camera, Physics3D, Animation, Physics2D, Audio, Count
};
inline constexpr const char *componentFamilyName(ComponentFamily family) {
  switch (family) {
  case ComponentFamily::Logic: return "Lógica";
  case ComponentFamily::Rendering: return "Renderização";
  case ComponentFamily::Lighting: return "Luz";
  case ComponentFamily::Camera: return "Câmera";
  case ComponentFamily::Physics3D: return "Física 3D";
  case ComponentFamily::Animation: return "Animação";
  case ComponentFamily::Physics2D: return "Física 2D";
  case ComponentFamily::Audio: return "Áudio";
  case ComponentFamily::Count: break;
  }
  return "";
}
// Ícone da família no trilho do Add, pelo nome do catálogo do atlas. Diferente
// dos ícones dos tipos: o trilho nomeia a CATEGORIA, não um componente dela.
inline constexpr std::string_view componentFamilyIcon(ComponentFamily family) {
  switch (family) {
  case ComponentFamily::Logic: return "scripting/nodes";
  case ComponentFamily::Rendering: return "primitive/cube";
  case ComponentFamily::Lighting: return "lighting/scene-lighting";
  case ComponentFamily::Camera: return "runtime/camera";
  case ComponentFamily::Physics3D: return "physics/dynamic-sphere";
  case ComponentFamily::Animation: return "runtime/play";
  case ComponentFamily::Physics2D: return "physics/body-2d";
  case ComponentFamily::Audio: return "audio/source";
  case ComponentFamily::Count: break;
  }
  return "";
}

// Quando uma alteração pode ser aceita com o Play rodando. `SafePoint` significa
// que ela entra na fila do mundo de execução e é aplicada entre passos, nunca
// no meio de um callback de script ou de física.
enum class PlayMutability : u32 { Never, SafePoint };

struct ComponentRule {
  std::string_view typeId;
  const char *message;
};

struct ComponentSchema {
  const ComponentType *type=nullptr;
  const char *name="";
  const char *description="";
  ComponentFamily family=ComponentFamily::Logic;
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
  // Apresentação e descoberta. O ícone é o nome no catálogo do atlas
  // (`assets/astra-visual/icons/named/catalog.json`); o editor o resolve uma
  // vez e um teste exige que todo nome exista. Os termos de busca trazem nomes
  // de outras engines para a descoberta e não prometem equivalência de API.
  std::string_view subfamily{};
  std::string_view icon{};
  std::string_view searchTerms{};
  // Referência oficial estudada (Unity 6000.0 ou Godot 4.5), com versão no link.
  std::string_view reference{};
  // Falso para tipos anexados por outro fluxo (comportamento C# vem da área de
  // código, com o script escolhido); continuam no registro e no arquivo.
  bool listedInAdd = true;
  // Nome da fachada C# gerada (`Astra.Components.<apiName>`); vazio quando o
  // tipo tem API própria (Comportamento é a classe Behavior).
  std::string_view apiName{};
  bool allowMultiple() const noexcept { return type->allowMultiple; }
};
} // namespace ae::scene

#include "scene/generated/schema_includes.inc"

namespace ae::scene {
namespace detail {
template <usize... Sizes>
constexpr auto joinComponentSchemas(const std::array<ComponentSchema, Sizes> &...families) {
  std::array<ComponentSchema, (Sizes + ... + 0)> joined{};
  usize cursor = 0;
  ((std::copy(families.begin(), families.end(), joined.begin() + cursor), cursor += Sizes), ...);
  return joined;
}
} // namespace detail

#include "scene/generated/schema_registry.inc"

// Identidade é o contrato do arquivo: dois registros com o mesmo id tornariam a
// leitura ambígua, e a leitura recusa ambiguidade em vez de escolher pela ordem.
// Os descritores não são constexpr, então a verificação roda nos testes.
inline bool componentSchemaIdsUnique() {
  for (usize i = 0; i < componentSchemas.size(); ++i)
    for (usize j = 0; j < i; ++j)
      if (componentSchemas[i].type->id == componentSchemas[j].type->id) return false;
  return true;
}

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
      // Teto do formato e da memória por objeto; a Unity não tem teto, então
      // a recusa diz qual é em vez de só "limite".
      static_assert(Components::MaximumCount==64,"a mensagem do limite cita o número");
      plan.error="Limite de 64 componentes por objeto";return false;
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
