#pragma once
#include "editor/editor_document.h"
#include "editor/editor_history.h"
#include "resources/import_node_map.h"
#include "scene/import_link.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace ae::editor {
// Slot da malha no pacote carregado, a partir da identidade. Resolver JÁ na
// mutação faz o passo de desfazer/refazer carregar um slot válido; sem
// resolvedor, o slot fica zero até a próxima reconciliação de slots.
using ImportSlotResolver = std::function<u32(const resources::AssetGuid &)>;
// Reconciliação das instâncias de uma fonte importada com a revisão nova do
// mapa de nós (M08.2).
//
// Três lados por campo: BASE (o que a fonte tinha quando o objeto foi
// reconciliado, guardado no vínculo), NOVO (o mapa atual) e LOCAL (o objeto).
//   • local == base  → o usuário não mexeu: recebe o novo;
//   • novo == base   → a fonte não mudou: o local fica;
//   • os três diferem → CONFLITO: o local fica e o conflito é relatado.
// Nós novos entram nas instâncias que ainda não os conheciam; nós que uma
// instância conhecia e não tem mais foram apagados pelo usuário e continuam
// apagados. Nó removido da fonte sai da cena só se não carregar nada do
// usuário; senão fica ÓRFÃO, com os dados, esperando decisão explícita.
//
// Opera com histórico (edição interativa, um passo de desfazer) ou direto no
// documento (carga de cena, antes de existir histórico).
struct ImportReconcileReport {
  u32 instances = 0, updated = 0, created = 0, removed = 0, orphaned = 0;
  u32 conflicts = 0, keptDeleted = 0, adopted = 0, unproven = 0, skippedInstances = 0;
  // Entrega 2: nós cujas partes por primitiva viraram slots, e nós cujas partes
  // ficaram como estavam porque migrar perderia algo.
  u32 consolidated = 0, legacyParts = 0;
  std::vector<std::string> notes;
  bool changed() const noexcept { return updated || created || removed || orphaned || adopted || consolidated; }
};

enum ImportOverride : u32 {
  ImportOverrideName = 1,
  ImportOverridePosition = 2,
  ImportOverrideRotation = 4,
  ImportOverrideScale = 8,
  ImportOverrideParent = 16,
  ImportOverrideMesh = 32,
  ImportOverrideAll = 63
};

// Pose local de um nó do mapa em TRS. Falso quando a matriz não cabe em TRS.
bool importNodeTransform(const resources::ImportNodeRecord &node, EditorTransform &out);
// Preenche a base do vínculo com os valores do nó nesta revisão. `slots`
// falso descreve a representação legada (objeto do nó sem malha e uma parte
// por primitiva), usada até a migração provar que pode consolidar.
void setImportLinkBase(scene::ImportLink &link, const resources::ImportNodeRecord &node,
                       const EditorTransform &transform, i32 primitive, u32 revision, bool slots = true);
std::string importEntityName(std::string_view name);

// Liga objetos de cenas anteriores ao vínculo, SÓ quando a prova existe: a
// malha aponta para a identidade de um desenho do mapa e cada ancestral tem o
// nome do nó pai correspondente. Qualquer divergência deixa o objeto como está.
u32 adoptLegacyImportInstances(EditorDocument &document, EditorHistory *history, const resources::AssetGuid &source,
                               const resources::ImportNodeMap &map, ImportReconcileReport &report);
bool reconcileImportInstances(EditorDocument &document, EditorHistory *history, const resources::AssetGuid &source,
                              const resources::ImportNodeMap &map, ImportReconcileReport &report,
                              const ImportSlotResolver &slotOf = {});

// O que o objeto tem de diferente da base. Zero sem vínculo ou quando órfão.
u32 importOverrides(const EditorDocument &document, EditorEntityId id);
bool revertImportOverrides(EditorDocument &document, EditorHistory &history, EditorEntityId id, u32 mask,
                           const ImportSlotResolver &slotOf = {});
// Remove o vínculo de todos os objetos da instância do objeto dado.
u32 unlinkImportInstance(EditorDocument &document, EditorHistory &history, EditorEntityId member);
bool unlinkImportObject(EditorDocument &document, EditorHistory &history, EditorEntityId id);
} // namespace ae::editor
