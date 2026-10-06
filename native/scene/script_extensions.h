// Famílias opcionais da ABI dos scripts.
//
// `ScriptSceneAccess` cresceu uma versão por função nova, e `available()` exige
// todas: duas frentes que acrescentam funções disputam o mesmo número, e um host
// sem uma única delas recusa o runtime inteiro. A partir da v42 o núcleo fica
// congelado; uma capacidade nova entra como FAMÍLIA nomeada, resolvida por
// `ScriptSceneAccess::extension`, com versão e tamanho próprios.
//
// Regras de uma família:
//  - o nome é estável e versionado pelo campo `version` da própria tabela;
//  - campos só são acrescentados no fim; quem consome confere `size` antes de
//    ler um ponteiro novo, e `version` antes de confiar na semântica;
//  - a tabela pertence à sessão que a entregou e deixa de valer no Stop;
//  - host sem a família devolve nulo, e o lado gerenciado recusa só a operação
//    que dependia dela (NotSupported), nunca a sessão inteira.
#pragma once
#include "scene/components.h"

#include <string_view>

namespace ae::scene {

// --- astra.component.operations v1 ------------------------------------------
// Métodos e eventos declarados nos tipos (ComponentMethod/ComponentEvent),
// executados por runtime/component_operations.h.
inline constexpr std::string_view kScriptComponentOperations="astra.component.operations";

// `type` é o índice do tipo no registro de schemas desta build e `event` o
// índice no tipo; `eventName` traduz os dois para "tipo/evento". `lost` conta
// eventos descartados por estouro da fila ANTES deste.
struct ScriptComponentEvent {
  u64 object=0,instance=0;
  u32 world=0,generation=0;
  u32 type=0,event=0;
  u32 count=0,lost=0;
  ComponentOperationValue values[kComponentEventPayloadLimit]{};
};
static_assert(sizeof(ScriptComponentEvent)==112);

struct ScriptComponentOperations {
  u32 version=1,size=sizeof(ScriptComponentOperations);
  // 1 executou; 0 recusou, com o motivo em ScriptSceneAccess::lastStatus.
  int (*invoke)(void *,u64 object,u32 world,u32 generation,u64 instance,const u8 *method,int methodLength,
                const ComponentOperationValue *arguments,int count,ComponentOperationValue *result)=nullptr;
  // Copia até `capacity` eventos pendentes, em ordem de emissão. Devolve a
  // quantidade copiada; -1 sem mundo ou com argumento inválido.
  int (*pollEvents)(void *,ScriptComponentEvent *events,int capacity)=nullptr;
  // "tipo/evento" em UTF-8, truncado em `capacity`; devolve o comprimento total
  // ou -1 para índice inexistente.
  int (*eventName)(void *,u32 type,u32 event,u8 *buffer,int capacity)=nullptr;
  // 1 quando o tipo declara o evento ("tipo/evento" em UTF-8), 0 caso contrário.
  int (*declaresEvent)(void *,const u8 *name,int length)=nullptr;
};

} // namespace ae::scene
