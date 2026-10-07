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

// --- astra.view v1 -------------------------------------------------------------
// Vista de jogo publicada por quem desenha o quadro (runtime/game_view.h).
// Pixels com origem no canto inferior esquerdo, como Screen/Camera da Unity.
inline constexpr std::string_view kScriptView="astra.view";
inline constexpr u32 kScriptViewValid=1,kScriptViewSafeArea=2,kScriptViewAuthoredCamera=4;
struct ScriptViewState {
  u32 size=sizeof(ScriptViewState),flags=0;
  float width=0,height=0,dpi=0;
  float safeX=0,safeY=0,safeWidth=0,safeHeight=0;
  u32 platform=0,reserved=0;
  u64 camera=0;
};
static_assert(sizeof(ScriptViewState)==56);
struct ScriptViewOperations {
  u32 version=1,size=sizeof(ScriptViewOperations);
  int (*state)(void *,ScriptViewState *)=nullptr;
  // origin3/direction3 normalizada; 0 sem vista válida ou entrada inválida.
  int (*screenRay)(void *,float x,float y,float *origin,float *direction)=nullptr;
  // screen3: x,y em pixels e z = profundidade de vista em metros.
  int (*worldToScreen)(void *,const float *world,float *screen)=nullptr;
};

// --- astra.debug v1 ------------------------------------------------------------
// Linhas de depuração do Play (Debug.DrawLine/DrawRay); nunca persistidas.
inline constexpr std::string_view kScriptDebug="astra.debug";
struct ScriptDebugOperations {
  u32 version=1,size=sizeof(ScriptDebugOperations);
  int (*drawLine)(void *,const float *from,const float *to,u32 rgba,float seconds)=nullptr;
};

// --- astra.hierarchy v1 --------------------------------------------------------
// Mudanças de hierarquia aplicadas (GameWorld::takeHierarchyChanges): kind 0 =
// pai mudou (objeto e subárvore), 1 = lista direta de filhos mudou.
inline constexpr std::string_view kScriptHierarchy="astra.hierarchy";
struct ScriptHierarchyChange { u64 object=0; u32 generation=0,kind=0; };
static_assert(sizeof(ScriptHierarchyChange)==16);
struct ScriptHierarchyOperations {
  u32 version=1,size=sizeof(ScriptHierarchyOperations);
  // Copia até `capacity` mudanças e as retira; o resto fica para a próxima.
  int (*pollChanges)(void *,ScriptHierarchyChange *changes,int capacity)=nullptr;
};

// --- astra.haptics v1 ----------------------------------------------------------
// Só existe quando a plataforma entrega um vibrador real (Android). Host sem
// vibrador devolve a família nula: o C# recusa a operação, não finge vibrar.
inline constexpr std::string_view kScriptHaptics="astra.haptics";
struct ScriptHapticsOperations {
  u32 version=1,size=sizeof(ScriptHapticsOperations);
  // milissegundos 1..5000; amplitude 0..1 (0 usa a padrão do aparelho).
  int (*vibrate)(void *,u32 milliseconds,float amplitude)=nullptr;
};

// --- astra.scenes v1 -----------------------------------------------------------
// Cenas do projeto em Play (Unity 6000.0 SceneManager). Nomes são o caminho
// relativo sem extensão ou só o nome do arquivo, quando único no projeto.
// Aditiva: cria a cena sob `parent` e devolve o contêiner; o chamador publica
// pelo mesmo par instantiationAttachments/finishInstantiation dos prefabs.
// Única: aceita o pedido, que troca o mundo inteiro no fim do quadro.
inline constexpr std::string_view kScriptScenes="astra.scenes";
struct ScriptSceneOperations {
  u32 version=1,size=sizeof(ScriptSceneOperations);
  int (*active)(void *,u8 *buffer,int capacity)=nullptr;            // comprimento ou -1
  int (*count)(void *)=nullptr;                                       // -1 sem catálogo
  int (*nameAt)(void *,u32 index,u8 *buffer,int capacity)=nullptr;   // comprimento ou -1
  u64 (*loadAdditive)(void *,u64 parent,const u8 *name,int length)=nullptr;
  int (*requestSingle)(void *,const u8 *name,int length)=nullptr;
};

inline constexpr std::string_view kScriptMotorControl="astra.motor-control";
inline constexpr std::string_view kScriptMotorMotion="astra.motor-motion";
struct ScriptMotorMotionState {
  u32 size=sizeof(ScriptMotorMotionState),flags=0;
  u64 support=0;
  float velocity[3]{},groundVelocity[3]{},normal[3]{},point[3]{};
};
static_assert(sizeof(ScriptMotorMotionState)==64);
struct ScriptMotorMotionOperations {
  u32 version=1,size=sizeof(ScriptMotorMotionOperations);
  int (*state)(void *,u64 object,u32 world,u32 generation,u64 instance,ScriptMotorMotionState *)=nullptr;
};
struct ScriptMotorControlState {
  u32 size=sizeof(ScriptMotorControlState),source=0,candidates=0,flags=0;
  float right=0,forward=0,yaw=0,priority=0;
};
static_assert(sizeof(ScriptMotorControlState)==32);
struct ScriptMotorControlOperations {
  u32 version=1,size=sizeof(ScriptMotorControlOperations);
  int (*command)(void *,u64 object,u32 world,u32 generation,u64 instance,u32 operation,u32 source,const float *move,u32 jump,ScriptMotorControlState *)=nullptr;
};
} // namespace ae::scene
