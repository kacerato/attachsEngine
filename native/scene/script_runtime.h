#pragma once
#include "core/base.h"

namespace ae::scene {
// ABI v3 é independente de Editor e de headers do CLR. Todos os callbacks e
// chamadas de runtime executam na thread dona do mundo, e os buffers só valem
// durante a chamada.
//
// **Compatibilidade:** os campos de v2 permanecem nas mesmas posições. Um
// consumidor que declare `version==2` continua encontrando o que esperava; o
// runtime gerenciado exige v3 para usar os campos novos e recusa `size`
// divergente, porque uma struct maior do que a acordada seria lida além do fim.
//
// Convenções dos campos novos:
//   • `u64` de objeto é o id persistente no mundo; zero é "nenhum".
//   • `generation` devolve zero para objeto destruído — é assim que um handle
//     guardado por um script vence ainda dentro do mesmo callback.
//   • funções `int` devolvem 1/0 para sucesso/recusa e -1 quando o argumento
//     nem chega a ser avaliado; `lastStatus` traz o motivo real da última recusa.
//   • propriedade trafega como (kind, bits): 0=float (padrão IEEE em 32 bits),
//     1=bool, 2=enum u32, 3=referência de objeto.
struct ScriptSceneAccess {
  u32 version=3,size=sizeof(ScriptSceneAccess);
  void *context=nullptr;
  int (*exists)(void *,u64)=nullptr;
  int (*getTransform)(void *,u64,float *)=nullptr; // position3 quaternion4 scale3, local space
  int (*setTransform)(void *,u64,const float *)=nullptr;
  int (*setVelocity)(void *,u64,const float *)=nullptr; // world space
  int (*moveKinematic)(void *,u64,const float *)=nullptr; // world position3 quaternion4
  void (*log)(void *,u64,const u8 *,int)=nullptr;
  int (*bodyForce)(void *,u64,const float *,u32)=nullptr; // force/impulse/torque/angular impulse
  int (*getVelocity)(void *,u64,float *)=nullptr;
  // --- v3: identidade -----------------------------------------------------
  u32 (*worldId)(void *)=nullptr;
  u32 (*generation)(void *,u64)=nullptr;
  u32 (*lastStatus)(void *)=nullptr;
  // --- v3: hierarquia -----------------------------------------------------
  u64 (*parentOf)(void *,u64)=nullptr;
  int (*childCount)(void *,u64)=nullptr;
  u64 (*childAt)(void *,u64,u32)=nullptr;
  u64 (*findChild)(void *,u64,const u8 *,int,int)=nullptr;
  int (*getName)(void *,u64,u8 *,int)=nullptr;
  int (*setName)(void *,u64,const u8 *,int)=nullptr;
  int (*getActive)(void *,u64)=nullptr;
  int (*setActive)(void *,u64,int)=nullptr;
  // --- v3: ciclo de vida --------------------------------------------------
  u64 (*createObject)(void *,u64,const u8 *,int)=nullptr;
  int (*destroyObject)(void *,u64)=nullptr;
  int (*setParent)(void *,u64,u64,u32)=nullptr;
  // --- v3: componentes ----------------------------------------------------
  int (*componentCount)(void *,u64)=nullptr;
  u64 (*componentAt)(void *,u64,u32,u8 *,int)=nullptr;
  u64 (*findComponent)(void *,u64,const u8 *,int,u32)=nullptr;
  u64 (*addComponent)(void *,u64,const u8 *,int)=nullptr;
  int (*removeComponent)(void *,u64,u64)=nullptr;
  int (*getProperty)(void *,u64,u64,const u8 *,int,u32 *,u64 *)=nullptr;
  int (*setProperty)(void *,u64,u64,const u8 *,int,u32,u64)=nullptr;
  // --- v3: transform de mundo --------------------------------------------
  int (*getWorldTransform)(void *,u64,float *)=nullptr;
  int (*setWorldTransform)(void *,u64,const float *)=nullptr;
  bool available() const {
    return exists&&getTransform&&setTransform&&setVelocity&&moveKinematic&&log&&bodyForce&&getVelocity&&
           worldId&&generation&&lastStatus&&parentOf&&childCount&&childAt&&findChild&&getName&&setName&&
           getActive&&setActive&&createObject&&destroyObject&&setParent&&componentCount&&componentAt&&
           findComponent&&addComponent&&removeComponent&&getProperty&&setProperty&&
           getWorldTransform&&setWorldTransform;
  }
};
struct ScriptRuntimeApi {
  int (*start)(const u8 *,int,const u8 *,int,const ScriptSceneAccess *)=nullptr;
  int (*update)(float)=nullptr;
  int (*fixedUpdate)(float)=nullptr;
  void (*stop)()=nullptr;
  int (*copyDiagnostics)(u8 *,int)=nullptr;
  int (*trigger)(u64,u64,u32)=nullptr;
  bool available() const {return start&&update&&fixedUpdate&&stop&&copyDiagnostics&&trigger;}
};
}
