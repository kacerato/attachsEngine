// A matriz de propriedades, lida do código em vez de escrita à mão.
//
// O plano universal de cenários realistas pede, para cada campo autorável, uma
// ficha com identidade, tipo, padrão, unidade, domínio, dependência, consumidor,
// invalidação e estado. Uma tabela em Markdown mantida à mão para isso envelhece
// no primeiro commit: alguém acrescenta uma propriedade e a matriz continua
// afirmando a versão anterior. Aqui a matriz é UMA PROJEÇÃO dos descritores que
// o editor, a API em C# e o mundo de execução já usam — não há como divergir.
//
// Este arquivo também carrega as regras que impedem uma linha incompleta de
// existir. `auditComponentContracts()` recusa:
//
//  - propriedade sem PropertyId (não teria API, preset, animação nem migração);
//  - PropertyId repetido no mesmo tipo (`setComponentProperty` já responde
//    `AmbiguousProperty`, mas o defeito deve aparecer no teste, não em uso);
//  - tripla apontando para canal inexistente;
//  - componente sem consumidor declarado;
//  - capacidade citada que não existe no registro do motor;
//  - propriedade dependente de capacidade `Planned` — é exatamente o botão sem
//    shader atrás que o plano proíbe.
//
// Camada: `scene`. Nada de editor, renderer ou I/O — a matriz é gerada tanto
// pelo teste de host quanto por uma ferramenta de documentação.
#pragma once
#include "scene/component_schema.h"
#include <string>
#include <vector>

namespace ae::scene {

enum class PropertyKind : u32 { Number, Boolean, Enum, Reference };

inline const char *propertyKindName(PropertyKind kind) {
  switch (kind) {
  case PropertyKind::Number: return "número";
  case PropertyKind::Boolean: return "booleano";
  case PropertyKind::Enum: return "enumeração";
  case PropertyKind::Reference: return "referência";
  }
  return "?";
}

// Uma linha da matriz. Só existem valores RESOLVIDOS: consumidor e capacidade
// já vêm com a herança do esquema aplicada, porque quem lê a matriz quer saber
// o que vale para a propriedade, não onde a declaração foi escrita.
struct PropertyContract {
  std::string_view typeId;
  u32 schemaVersion = 0;
  std::string_view propertyId;
  const char *label = "";
  PropertyKind kind = PropertyKind::Number;
  std::string_view group;
  std::string_view unit;
  const char *help = nullptr;
  // Domínio dos números; ignorado nos demais tipos.
  float minimum = 0, maximum = 0, step = 0;
  // Valor inicial de um componente recém-criado, formatado para leitura.
  std::string defaultValue;
  // Domínio enumerado, ou o tipo exigido por uma referência.
  std::string domain;
  bool conditional = false; // tem predicado de visibilidade
  bool readOnly = false;    // tem predicado de edição, ou não tem escrita
  // A propriedade existe uma vez POR SLOT do componente, não uma por componente.
  // `slots` é quantos endereços um valor recém-criado tem; a matriz publica isso
  // porque o "corte do alfa" de uma malha de seis primitivas são seis valores.
  bool perSlot = false;
  u32 slots = 0;
  std::string_view capability;
  std::string_view consumer;
  u32 invalidates = 0;
  PlayMutability playMutability = PlayMutability::SafePoint;
  core::CapabilityState capabilityState = core::CapabilityState::Implemented;
  const char *limitation = "";
};

inline std::string describeInvalidation(u32 mask) {
  struct Entry { u32 bit; const char *name; };
  static constexpr Entry entries[]{
    {Invalidate::Draw, "desenho"},
    {Invalidate::MaterialDescriptor, "material"},
    {Invalidate::MeshDerived, "derivados da malha"},
    {Invalidate::TextureResidency, "residência de textura"},
    {Invalidate::LightCluster, "seleção de luzes"},
    {Invalidate::ShadowMap, "mapa de sombra"},
    {Invalidate::Probes, "sondas"},
    {Invalidate::PhysicsShape, "forma física"},
    {Invalidate::PhysicsBody, "corpo físico"},
    {Invalidate::Policy, "política resolvida"},
    {Invalidate::Script, "comportamento"},
    {Invalidate::Transform, "pose e bounds"},
    {Invalidate::Input, "entrada"}};
  std::string result;
  for (const auto &entry : entries)
    if (mask & entry.bit) { if (!result.empty()) result += ", "; result += entry.name; }
  return result.empty() ? std::string("nada") : result;
}

namespace detail {
inline std::string formatNumber(float value) {
  if (value == static_cast<long long>(value) && value > -1e9f && value < 1e9f)
    return std::to_string(static_cast<long long>(value));
  std::string text = std::to_string(value);
  while (text.size() > 1 && text.back() == '0') text.pop_back();
  if (!text.empty() && text.back() == '.') text.pop_back();
  return text;
}
// A capacidade da propriedade estreita a do componente; vazia herda.
inline std::string_view resolveCapability(const ComponentSchema &schema, const PropertyPresentation &p) {
  return p.capability.empty() ? schema.capability : p.capability;
}
inline std::string_view resolveConsumer(const ComponentSchema &schema, const PropertyPresentation &p) {
  return p.consumer.empty() ? schema.consumer : p.consumer;
}
} // namespace detail

// Preenche os campos comuns e resolve a herança de contrato uma vez só.
inline PropertyContract propertyContractBase(const ComponentSchema &schema,
    std::string_view propertyId, const char *label, PropertyKind kind, const PropertyPresentation &presentation) {
  PropertyContract row;
  row.typeId = schema.type->id;
  row.schemaVersion = schema.type->version;
  row.propertyId = propertyId;
  row.label = label;
  row.kind = kind;
  row.group = presentation.group;
  row.unit = presentation.unit;
  row.help = presentation.help;
  row.conditional = presentation.visible != nullptr;
  row.readOnly = presentation.editable != nullptr;
  row.capability = detail::resolveCapability(schema, presentation);
  row.consumer = detail::resolveConsumer(schema, presentation);
  row.invalidates = schema.invalidates | presentation.invalidates;
  row.playMutability = schema.propertiesInPlay;
  if (const auto *capability = core::findEngineCapability(row.capability)) {
    row.capabilityState = capability->state;
    row.limitation = capability->limitation;
  }
  return row;
}

// Todas as propriedades de um tipo, na ordem em que o descritor as declara.
inline std::vector<PropertyContract> componentContracts(const ComponentSchema &schema) {
  std::vector<PropertyContract> rows;
  const auto &type = *schema.type;
  const auto probe = type.create ? type.create() : nullptr;
  for (const auto &property : type.numbers) {
    auto row = propertyContractBase(schema, property.id, property.name, PropertyKind::Number, property.presentation);
    row.minimum = property.minimum;
    row.maximum = property.maximum;
    row.step = property.dragStep;
    row.readOnly = row.readOnly || property.write == nullptr;
    if (probe) row.defaultValue = detail::formatNumber(property.read(*probe));
    row.domain = detail::formatNumber(property.minimum) + " … " + detail::formatNumber(property.maximum);
    rows.push_back(std::move(row));
  }
  for (const auto &property : type.booleans) {
    auto row = propertyContractBase(schema, property.id, property.name, PropertyKind::Boolean, property.presentation);
    row.readOnly = row.readOnly || property.write == nullptr;
    if (probe) row.defaultValue = property.read(*probe) ? "verdadeiro" : "falso";
    row.domain = "verdadeiro | falso";
    rows.push_back(std::move(row));
  }
  for (const auto &property : type.enums) {
    auto row = propertyContractBase(schema, property.id, property.name, PropertyKind::Enum, property.presentation);
    row.readOnly = row.readOnly || property.write == nullptr;
    for (const auto &option : property.options) {
      if (!row.domain.empty()) row.domain += " | ";
      row.domain += option.name;
      if (probe && property.read(*probe) == option.value) row.defaultValue = option.name;
    }
    rows.push_back(std::move(row));
  }
  for (const auto &property : type.slotNumbers) {
    auto row = propertyContractBase(schema, property.id, property.name, PropertyKind::Number, property.presentation);
    row.minimum = property.minimum;
    row.maximum = property.maximum;
    row.step = property.dragStep;
    row.perSlot = true;
    row.readOnly = row.readOnly || property.write == nullptr;
    if (probe) {
      row.slots = property.slotCount(*probe);
      if (row.slots && property.read) row.defaultValue = detail::formatNumber(property.read(*probe, 0));
    }
    row.domain = detail::formatNumber(property.minimum) + " … " + detail::formatNumber(property.maximum);
    rows.push_back(std::move(row));
  }
  for (const auto &property : type.slotEnums) {
    auto row = propertyContractBase(schema, property.id, property.name, PropertyKind::Enum, property.presentation);
    row.perSlot = true;
    row.readOnly = row.readOnly || property.write == nullptr;
    if (probe) row.slots = property.slotCount(*probe);
    for (const auto &option : property.options) {
      if (!row.domain.empty()) row.domain += " | ";
      row.domain += option.name;
      if (probe && row.slots && property.read && property.read(*probe, 0) == option.value) row.defaultValue = option.name;
    }
    rows.push_back(std::move(row));
  }
  for (const auto &property : type.references) {
    auto row = propertyContractBase(schema, property.id, property.name, PropertyKind::Reference, property.presentation);
    row.readOnly = row.readOnly || property.write == nullptr;
    row.defaultValue = property.nullLabel ? property.nullLabel : "Nenhum";
    row.domain = property.requiredType.empty() ? std::string("qualquer objeto") : std::string(property.requiredType);
    switch (property.scope) {
    case ObjectReferenceScope::SelfOrAncestor: row.domain += " · neste objeto ou ancestral"; break;
    case ObjectReferenceScope::Other: row.domain += " · outro objeto"; break;
    case ObjectReferenceScope::Descendant: row.domain += " · abaixo deste objeto"; break;
    case ObjectReferenceScope::Any: break;
    }
    rows.push_back(std::move(row));
  }
  return rows;
}

// A matriz inteira, na ordem do registro de esquemas.
inline std::vector<PropertyContract> allComponentContracts() {
  std::vector<PropertyContract> rows;
  for (const auto &schema : componentSchemas) {
    auto typeRows = componentContracts(schema);
    rows.insert(rows.end(), std::make_move_iterator(typeRows.begin()), std::make_move_iterator(typeRows.end()));
  }
  return rows;
}

// Um domínio enumerado usa `|` para separar opções, e `|` é o separador de
// coluna do Markdown: sem escapar, uma enumeração de três opções vira três
// colunas e desalinha a tabela inteira.
inline std::string escapeTableCell(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (const char c : text) {
    if (c == '|') out += '\\';
    out += c;
  }
  return out;
}

// A matriz publicável. Mesmo texto para o relatório do editor e para o
// documento do repositório: quem lê o arquivo em `docs/` está lendo o código.
inline std::string componentMatrixMarkdown() {
  std::string out =
      "# Matriz de propriedades dos componentes\n\n"
      "**Gerado por `scene::componentMatrixMarkdown()`** a partir dos descritores de componente.\n"
      "Não edite à mão: acrescente a propriedade no descritor e regenere.\n"
      "Uma linha só existe aqui quando tem identidade persistente, consumidor declarado e\n"
      "capacidade do motor disponível — as três condições que `auditComponentContracts()` exige.\n";
  for (const auto &schema : componentSchemas) {
    out += "\n## ";
    out += schema.name;
    out += " · `";
    out += std::string(schema.type->id);
    out += "` v" + std::to_string(schema.type->version) + "\n\n";
    out += std::string(schema.description) + ". **Consumidor:** " + std::string(schema.consumer) + ".";
    if (!schema.capability.empty()) {
      const auto *capability = core::findEngineCapability(schema.capability);
      out += " **Capacidade:** `" + std::string(schema.capability) + "` (" +
             core::capabilityStateName(capability ? capability->state : core::CapabilityState::Planned) + ").";
    }
    out += " **Invalida:** " + describeInvalidation(schema.invalidates) + ".\n\n";
    // Recursos vêm antes das propriedades porque são a pergunta que o autor faz
    // primeiro ao levar um componente para outro projeto: de que arquivos isto
    // depende? Herança e ausência declarada são estados distintos e aparecem.
    if (!schema.type->resourceBindings.empty()) {
      out += "**Recursos endereçados**\n\n";
      out += "| Binding | Rótulo | Tipo de recurso | Herda | Ausência declarada |\n|---|---|---|---|---|\n";
      for (const auto &binding : schema.type->resourceBindings)
        out += "| `" + std::string(binding.id) + "` | " + escapeTableCell(binding.name) + " | " +
               resources::assetTypeName(binding.kind) + " | " + (binding.inheritable ? "sim" : "não") + " | " +
               (binding.none.valid() ? "sim" : "não") + " |\n";
      out += "\n**Propriedades**\n\n";
    }
    out += "| PropertyId | Rótulo | Tipo | Grupo | Padrão | Domínio | Unidade | Consumidor | Invalida | Condicional | Por slot |\n";
    out += "|---|---|---|---|---|---|---|---|---|---|---|\n";
    for (const auto &row : componentContracts(schema)) {
      out += "| `" + std::string(row.propertyId) + "` | " + escapeTableCell(row.label) + " | " +
             propertyKindName(row.kind) + " | " + escapeTableCell(row.group) + " | " +
             escapeTableCell(row.defaultValue) + " | " + escapeTableCell(row.domain) + " | " +
             escapeTableCell(row.unit) + " | " + escapeTableCell(row.consumer) + " | " +
             describeInvalidation(row.invalidates) + " | " + (row.conditional ? "sim" : "não") + " | " +
             (row.perSlot ? "sim" : "não") + " |\n";
    }
  }
  out += "\n## Capacidades do motor\n\n";
  out += "| Capacidade | Nome | Estado | Onde vive | Limite |\n|---|---|---|---|---|\n";
  for (const auto &capability : core::engineCapabilities)
    out += "| `" + std::string(capability.id) + "` | " + capability.name + " | " +
           core::capabilityStateName(capability.state) + " | " + std::string(capability.owner) + " | " +
           (capability.limitation && *capability.limitation ? capability.limitation : "—") + " |\n";
  return out;
}

struct ContractIssue {
  std::string_view typeId;
  std::string_view propertyId;
  std::string message;
};

// As regras que impedem uma ficha incompleta de entrar no repositório.
inline std::vector<ContractIssue> auditComponentContracts() {
  std::vector<ContractIssue> issues;
  const auto report = [&](std::string_view type, std::string_view property, std::string message) {
    issues.push_back({type, property, std::move(message)});
  };
  for (const auto &schema : componentSchemas) {
    const auto &type = *schema.type;
    if (schema.consumer.empty())
      report(type.id, {}, "componente sem consumidor declarado");
    if (!core::engineCapabilityDeclared(schema.capability))
      report(type.id, {}, "capacidade não registrada: " + std::string(schema.capability));
    const auto rows = componentContracts(schema);
    for (usize i = 0; i < rows.size(); ++i) {
      const auto &row = rows[i];
      if (row.propertyId.empty()) {
        report(type.id, {}, std::string("propriedade sem identidade: ") + row.label);
        continue;
      }
      for (usize j = 0; j < i; ++j)
        if (rows[j].propertyId == row.propertyId)
          report(type.id, row.propertyId, "identidade de propriedade repetida");
      if (row.consumer.empty())
        report(type.id, row.propertyId, "propriedade sem consumidor");
      // Uma propriedade por slot que não escreve é um endereço que a API e o
      // preset enxergam e não conseguem aplicar — pior que não existir.
      if (row.perSlot && row.readOnly)
        report(type.id, row.propertyId, "propriedade por slot sem escrita endereçável");
      if (!core::engineCapabilityDeclared(row.capability))
        report(type.id, row.propertyId, "capacidade não registrada: " + std::string(row.capability));
      else if (!core::engineCapabilityAuthorable(row.capability))
        report(type.id, row.propertyId,
               "propriedade depende de capacidade planejada (" + std::string(row.capability) + ")");
    }
    // Um binding sem identidade não pode ser endereçado por preset, remap nem
    // reparo — e um binding sem leitura não entra em nenhum relatório de
    // dependência, que é o mesmo que o recurso não existir para o projeto.
    for (usize i = 0; i < type.resourceBindings.size(); ++i) {
      const auto &binding = type.resourceBindings[i];
      if (binding.id.empty()) { report(type.id, {}, std::string("binding de recurso sem identidade: ") + binding.name); continue; }
      for (usize j = 0; j < i; ++j)
        if (type.resourceBindings[j].id == binding.id) report(type.id, binding.id, "identidade de binding repetida");
      if (!binding.slots || !binding.read) report(type.id, binding.id, "binding de recurso sem leitura");
    }
    // Uma tripla é um atalho de edição sobre canais persistentes: se um canal
    // não existe, o editor de vetor grava em lugar nenhum.
    for (const auto &triple : type.triples) {
      if (triple.id.empty()) report(type.id, {}, "tripla sem identidade");
      for (const auto &channel : triple.channels) {
        bool found = false;
        for (const auto &number : type.numbers) if (number.id == channel) found = true;
        if (!found) report(type.id, triple.id, "canal inexistente: " + std::string(channel));
      }
    }
  }
  return issues;
}

} // namespace ae::scene
