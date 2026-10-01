// Diferença e aplicação POR CAMPO entre dois valores do mesmo componente.
//
// Aplicar um preset inteiro é o caso fácil: substitui o valor e pronto. O caso
// que o plano universal exige — e que faltava — é o outro: "quero a rugosidade
// e a normal deste preset, mas não a cor, e não quero perder o material que já
// escolhi neste objeto". Sem isso, um preset é tudo-ou-nada e o autor passa a
// evitar presets em objetos já ajustados, que é exatamente onde eles valeriam.
//
// Três regras que este arquivo existe para garantir:
//
// 1. **Uma transação.** Os campos escolhidos entram num candidato clonado e o
//    componente só é substituído se o conjunto inteiro for válido. Metade de um
//    preset aplicada é um estado que ninguém pediu e que o Undo não descreve.
// 2. **A mesma validação de sempre.** Domínio, predicado de edição e
//    `valid()` do componente valem igual, venha o valor do dedo, do preset, de
//    um script ou de uma animação.
// 3. **Recurso é endereço, não cópia.** Um binding de recurso carrega a
//    identidade; aplicar entre projetos exige remapear ou recusar, nunca
//    apontar para "o recurso que estava naquele índice".
//
// Camada `scene`: sem editor, sem I/O. A biblioteca de presets (que é arquivo)
// vive no editor e consome estas funções.
#pragma once
#include "scene/component_reflection.h"

namespace ae::scene {

enum class FieldKind : u32 { Number, Boolean, Enum, Reference, Resource, SlotNumber, SlotEnum };

// Endereço de um campo dentro de um componente. `slot` é usado por recurso e
// por propriedade de slot: o mesmo campo em endereços diferentes do mesmo valor.
struct FieldAddress {
  std::string_view id;
  u32 slot = 0;
  FieldKind kind = FieldKind::Number;
  friend bool operator==(const FieldAddress &a, const FieldAddress &b) noexcept {
    return a.id == b.id && a.slot == b.slot && a.kind == b.kind;
  }
};

struct FieldDelta {
  FieldAddress address;
  const char *label = "";
  std::string_view group;
  std::string current, candidate;
  bool differs = false;
  // Falso quando o campo existe mas não pode receber o valor neste estado —
  // um predicado de edição ou um binding somente-leitura. Continua aparecendo
  // no diff, com o motivo, em vez de sumir sem explicação.
  bool applicable = true;
};

namespace detail {
// Três estados, não dois: o binding pode não ter valor (herda da fonte ou do
// material compartilhado), ter ausência declarada (o autor TIROU o recurso) ou
// ter identidade. Colapsar os dois primeiros em "vazio" é o que faz um preset
// aplicado apagar uma textura herdada sem que ninguém tenha pedido.
inline std::string describeGuid(const ComponentResourceBinding &binding, const resources::AssetGuid &value) {
  if (binding.declaresNone(value)) return "sem recurso";
  if (!value.valid()) return binding.inheritable ? "herda" : "nenhum";
  return value.text();
}
} // namespace detail

// O diff completo entre o valor atual e um candidato do MESMO tipo. Campos
// iguais também voltam na lista: a interface precisa mostrar o que NÃO muda
// para o autor confirmar que um preset não vai levar algo junto sem avisar.
inline std::vector<FieldDelta> componentDelta(const ComponentValue &current, const ComponentValue &candidate) {
  std::vector<FieldDelta> rows;
  if (&current.type() != &candidate.type()) return rows;
  const auto &type = current.type();
  for (const auto &property : type.numbers) {
    if (property.id.empty()) continue;
    FieldDelta row;
    row.address = {property.id, 0, FieldKind::Number};
    row.label = property.name;
    row.group = property.presentation.group;
    row.current = detail::formatNumber(property.read(current));
    row.candidate = detail::formatNumber(property.read(candidate));
    row.applicable = property.write != nullptr && property.presentation.isEditable(current);
    row.differs = property.read(current) != property.read(candidate);
    rows.push_back(std::move(row));
  }
  for (const auto &property : type.booleans) {
    if (property.id.empty()) continue;
    FieldDelta row;
    row.address = {property.id, 0, FieldKind::Boolean};
    row.label = property.name;
    row.group = property.presentation.group;
    row.current = property.read(current) ? "verdadeiro" : "falso";
    row.candidate = property.read(candidate) ? "verdadeiro" : "falso";
    row.applicable = property.write != nullptr && property.presentation.isEditable(current);
    row.differs = property.read(current) != property.read(candidate);
    rows.push_back(std::move(row));
  }
  for (const auto &property : type.enums) {
    if (property.id.empty()) continue;
    FieldDelta row;
    row.address = {property.id, 0, FieldKind::Enum};
    row.label = property.name;
    row.group = property.presentation.group;
    const auto name = [&](u32 value) {
      for (const auto &option : property.options) if (option.value == value) return std::string(option.name);
      return std::to_string(value);
    };
    row.current = name(property.read(current));
    row.candidate = name(property.read(candidate));
    row.applicable = property.write != nullptr && property.presentation.isEditable(current);
    row.differs = property.read(current) != property.read(candidate);
    rows.push_back(std::move(row));
  }
  for (const auto &property : type.references) {
    if (property.id.empty() || !property.read) continue;
    FieldDelta row;
    row.address = {property.id, 0, FieldKind::Reference};
    row.label = property.name;
    row.group = property.presentation.group;
    const auto name = [&](u64 target) {
      return target ? std::to_string(target) : std::string(property.nullLabel ? property.nullLabel : "Nenhum");
    };
    row.current = name(property.read(current));
    row.candidate = name(property.read(candidate));
    row.applicable = property.write != nullptr && property.presentation.isEditable(current);
    row.differs = property.read(current) != property.read(candidate);
    rows.push_back(std::move(row));
  }
  for (const auto &property : type.slotNumbers) {
    if (property.id.empty() || !property.read) continue;
    const u32 slots = std::max(property.slotCount(current), property.slotCount(candidate));
    for (u32 slot = 0; slot < slots; ++slot) {
      FieldDelta row;
      row.address = {property.id, slot, FieldKind::SlotNumber};
      row.label = property.name;
      row.group = property.presentation.group;
      const bool inCurrent = slot < property.slotCount(current), inCandidate = slot < property.slotCount(candidate);
      row.current = inCurrent ? detail::formatNumber(property.read(current, slot)) : "sem slot";
      row.candidate = inCandidate ? detail::formatNumber(property.read(candidate, slot)) : "sem slot";
      row.applicable = inCurrent && inCandidate && property.write != nullptr;
      row.differs = row.current != row.candidate;
      rows.push_back(std::move(row));
    }
  }
  for (const auto &property : type.slotEnums) {
    if (property.id.empty() || !property.read) continue;
    const auto name = [&](u32 value) {
      for (const auto &option : property.options) if (option.value == value) return std::string(option.name);
      return std::to_string(value);
    };
    const u32 slots = std::max(property.slotCount(current), property.slotCount(candidate));
    for (u32 slot = 0; slot < slots; ++slot) {
      FieldDelta row;
      row.address = {property.id, slot, FieldKind::SlotEnum};
      row.label = property.name;
      row.group = property.presentation.group;
      const bool inCurrent = slot < property.slotCount(current), inCandidate = slot < property.slotCount(candidate);
      row.current = inCurrent ? name(property.read(current, slot)) : "sem slot";
      row.candidate = inCandidate ? name(property.read(candidate, slot)) : "sem slot";
      row.applicable = inCurrent && inCandidate && property.write != nullptr;
      row.differs = row.current != row.candidate;
      rows.push_back(std::move(row));
    }
  }
  for (const auto &binding : type.resourceBindings) {
    if (binding.id.empty() || !binding.read) continue;
    const u32 slots = std::max(binding.slotCount(current), binding.slotCount(candidate));
    for (u32 slot = 0; slot < slots; ++slot) {
      FieldDelta row;
      row.address = {binding.id, slot, FieldKind::Resource};
      row.label = binding.name;
      row.group = binding.presentation.group;
      const bool inCurrent = slot < binding.slotCount(current), inCandidate = slot < binding.slotCount(candidate);
      row.current = inCurrent ? detail::describeGuid(binding, binding.read(current, slot)) : "sem slot";
      row.candidate = inCandidate ? detail::describeGuid(binding, binding.read(candidate, slot)) : "sem slot";
      // Um slot que não existe no destino não é aplicável: criar slot é mudar a
      // geometria do objeto, não copiar um valor.
      row.applicable = inCurrent && inCandidate && binding.write != nullptr;
      row.differs = row.current != row.candidate;
      rows.push_back(std::move(row));
    }
  }
  return rows;
}

// Resultado de uma aplicação, no formato que o plano pede: o que entrou, o que
// foi recusado e por quê. `pendingGpu`/`degraded` pertencem à publicação do
// recurso na GPU e são preenchidos por quem publica, não por esta camada.
struct ComponentApplyOutcome {
  u32 applied = 0;
  u32 rejected = 0;
  const char *error = nullptr;
  bool ok() const noexcept { return error == nullptr; }
};

// Aplica SOMENTE os campos endereçados, em uma transação. Nada é escrito na
// coleção antes de o candidato inteiro passar por `valid()`.
inline ComponentApplyOutcome applyComponentFields(Components &components, const ComponentValue &source,
    std::span<const FieldAddress> fields, u64 instanceId = 0) {
  ComponentApplyOutcome outcome;
  const auto *current = instanceId ? components.findInstance(instanceId) : components.find(source.type().id);
  if (!current || &current->type() != &source.type()) { outcome.error = "Componente ausente"; return outcome; }
  if (!instanceId && source.type().allowMultiple) {
    u32 count = 0;
    for (usize i = 0; i < components.size(); ++i) if (components.at(i)->type().id == source.type().id) ++count;
    if (count > 1) { outcome.error = "Várias instâncias deste tipo; escolha uma"; return outcome; }
  }
  auto candidate = current->clone();
  if (!candidate || &candidate->type() != &source.type()) { outcome.error = "Falha ao clonar componente"; return outcome; }
  const auto &type = source.type();
  const bool portableSlots=sameComponentCollectionStructure(*current,source);
  // Reject the whole selection before publishing any unrelated fields.
  // Positional fields cannot silently cross reordered persistent collections.
  for(const auto &field:fields) {
    bool structural=field.kind==FieldKind::SlotNumber || field.kind==FieldKind::SlotEnum;
    if(field.kind==FieldKind::Resource)
      for(const auto &binding:type.resourceBindings)
        if(binding.id==field.id && binding.elementId) structural=true;
    if(structural && !portableSlots) {
      outcome.rejected=static_cast<u32>(fields.size());
      outcome.error="A identidade ou ordem da coleção mudou; aplicação por posição recusada";
      return outcome;
    }
  }
  for (const auto &field : fields) {
    bool matched = false;
    switch (field.kind) {
    case FieldKind::Number:
      for (const auto &property : type.numbers) {
        if (property.id != field.id) continue;
        matched = true;
        auto *destination = property.write ? property.write(*candidate) : nullptr;
        if (!destination || !property.presentation.isEditable(*current)) { ++outcome.rejected; break; }
        const float value = property.read(source);
        if (!std::isfinite(value) || value < property.minimum || value > property.maximum) { ++outcome.rejected; break; }
        *destination = value;
        ++outcome.applied;
      }
      break;
    case FieldKind::Boolean:
      for (const auto &property : type.booleans) {
        if (property.id != field.id) continue;
        matched = true;
        if (!property.write || !property.presentation.isEditable(*current)) { ++outcome.rejected; break; }
        property.write(*candidate, property.read(source));
        ++outcome.applied;
      }
      break;
    case FieldKind::Enum:
      for (const auto &property : type.enums) {
        if (property.id != field.id) continue;
        matched = true;
        if (!property.write || !property.presentation.isEditable(*current)) { ++outcome.rejected; break; }
        const u32 value = property.read(source);
        bool known = false;
        for (const auto &option : property.options) if (option.value == value) known = true;
        if (!known) { ++outcome.rejected; break; }
        property.write(*candidate, value);
        ++outcome.applied;
      }
      break;
    case FieldKind::Reference:
      for (const auto &property : type.references) {
        if (property.id != field.id) continue;
        matched = true;
        if (!property.write || !property.read || !property.presentation.isEditable(*current)) { ++outcome.rejected; break; }
        property.write(*candidate, property.read(source));
        ++outcome.applied;
      }
      break;
    case FieldKind::SlotNumber:
      for (const auto &property : type.slotNumbers) {
        if (property.id != field.id) continue;
        matched = true;
        if (!property.write || !property.read || field.slot >= property.slotCount(source) ||
            field.slot >= property.slotCount(*candidate)) { ++outcome.rejected; break; }
        const float value = property.read(source, field.slot);
        if (!std::isfinite(value) || value < property.minimum || value > property.maximum) { ++outcome.rejected; break; }
        if (!property.write(*candidate, field.slot, value)) { ++outcome.rejected; break; }
        ++outcome.applied;
      }
      break;
    case FieldKind::SlotEnum:
      for (const auto &property : type.slotEnums) {
        if (property.id != field.id) continue;
        matched = true;
        if (!property.write || !property.read || field.slot >= property.slotCount(source) ||
            field.slot >= property.slotCount(*candidate)) { ++outcome.rejected; break; }
        const u32 value = property.read(source, field.slot);
        bool known = false;
        for (const auto &option : property.options) if (option.value == value) known = true;
        if (!known || !property.write(*candidate, field.slot, value)) { ++outcome.rejected; break; }
        ++outcome.applied;
      }
      break;
    case FieldKind::Resource:
      for (const auto &binding : type.resourceBindings) {
        if (binding.id != field.id) continue;
        matched = true;
        if (!binding.write || !binding.read || field.slot >= binding.slotCount(source) ||
            field.slot >= binding.slotCount(*candidate)) { ++outcome.rejected; break; }
        if (!binding.write(*candidate, field.slot, binding.read(source, field.slot))) { ++outcome.rejected; break; }
        ++outcome.applied;
      }
      break;
    }
    if (!matched) ++outcome.rejected;
  }
  if (!outcome.applied) { outcome.error = outcome.rejected ? "Nenhum campo pôde ser aplicado" : "Nenhum campo escolhido"; return outcome; }
  if (!candidate->valid()) { outcome.error = "A combinação escolhida deixaria o componente inválido"; outcome.rejected += outcome.applied; outcome.applied = 0; return outcome; }
  if (!components.replaceInstance(current->instanceId(), *candidate)) {
    outcome.error = "Falha ao publicar o componente";
    outcome.rejected += outcome.applied; outcome.applied = 0;
  }
  return outcome;
}

// Atalho para "aplicar tudo o que muda": a lista de endereços vem do próprio
// diff. É o comportamento antigo de aplicar um preset, expresso com o mesmo
// mecanismo do seletivo — não há dois caminhos de aplicação.
inline std::vector<FieldAddress> changedFields(const std::vector<FieldDelta> &delta) {
  std::vector<FieldAddress> fields;
  for (const auto &row : delta) if (row.differs && row.applicable) fields.push_back(row.address);
  return fields;
}

// Todos os recursos que um valor endereça, com binding e slot. É o que responde
// "o que este preset exige do projeto de destino?" antes de aplicar.
struct ResourceDependency {
  FieldAddress address;
  const char *label = "";
  resources::AssetType kind = resources::AssetType::Mesh;
  resources::AssetGuid guid;
};
inline std::vector<ResourceDependency> componentResourceDependencies(const ComponentValue &value) {
  std::vector<ResourceDependency> result;
  for (const auto &binding : value.type().resourceBindings) {
    if (!binding.read) continue;
    for (u32 slot = 0; slot < binding.slotCount(value); ++slot) {
      const auto guid = binding.read(value, slot);
      if (!guid.valid()) continue;
      result.push_back({{binding.id, slot, FieldKind::Resource}, binding.name, binding.kind, guid});
    }
  }
  return result;
}

// Remapeia identidades de recurso dentro de um valor — o passo que falta para
// levar um preset de um projeto para outro sem apontar para o vazio. Quem
// chama decide o mapa (por caminho, por nome, por escolha do autor); aqui só se
// garante que a troca é por identidade e que nada fora do mapa é tocado.
struct ResourceRemapEntry { resources::AssetGuid from, to; };
inline u32 remapComponentResources(ComponentValue &value, std::span<const ResourceRemapEntry> mapping) {
  u32 changed = 0;
  for (const auto &binding : value.type().resourceBindings) {
    if (!binding.read || !binding.write) continue;
    for (u32 slot = 0; slot < binding.slotCount(value); ++slot) {
      const auto current = binding.read(value, slot);
      if (!current.valid()) continue;
      for (const auto &entry : mapping)
        if (entry.from == current && binding.write(value, slot, entry.to)) { ++changed; break; }
    }
  }
  return changed;
}

// Recursos que o projeto de destino NÃO tem. Aplicar assim mesmo produziria
// referência quebrada; o autor precisa ver a lista antes, não o resultado.
inline std::vector<ResourceDependency> missingResourceDependencies(const ComponentValue &value,
    const resources::AssetRegistry &registry) {
  std::vector<ResourceDependency> missing;
  for (const auto &dependency : componentResourceDependencies(value)) {
    const auto *record = registry.find(dependency.guid);
    if (!record || record->type != dependency.kind) missing.push_back(dependency);
  }
  return missing;
}

} // namespace ae::scene
