// Fronteira C ABI blittable sobre o Jolt Physics (vendorizado em
// native/third_party/JoltPhysics — ver VENDORED_COMMIT.txt para a versão exata).
// Implementa o item 4.1.1 do plano ("Integração do Jolt: mundo, corpos, formas,
// dormência, camadas de colisão") como fatia vertical: um mundo, corpos estático/
// cinemático/dinâmico com forma caixa ou esfera, passo de simulação, leitura de
// transform/velocidade, e um raycast simples. Juntas, motores, character
// controller, decomposição convexa e determinismo em ponto fixo (itens 4.1.3,
// 4.1.5, 4.1.7, 4.1.8) ficam para incrementos futuros — ver docs/ESTADO.md.
//
// Regra de fronteira (a mesma do CONVENCOES.md §2, aplicada aqui entre C++ nosso
// e C++ do Jolt): só tipos POD cruzam esta fronteira. Nenhum tipo do Jolt
// (JPH::Vec3, JPH::BodyID, ...) aparece em jolt_bridge.h — só os tipos definidos
// aqui. Isso é o que permite o C# do lado de cima falar com isto via P/Invoke
// sem nunca conhecer a existência do Jolt.
#pragma once

#include "core/base.h"

extern "C" {

struct AetherVec3 {
  float x, y, z;
};

struct AetherQuat {
  float x, y, z, w;
};

enum class AetherMotionType : ae::u32 {
  Static = 0,
  Kinematic = 1,
  Dynamic = 2,
};

enum class AetherShapeKind : ae::u32 {
  Box = 0,
  Sphere = 1,
};

struct AetherShapeDesc {
  AetherShapeKind kind;
  AetherVec3 boxHalfExtent; // usado quando kind == Box
  float sphereRadius;       // usado quando kind == Sphere
};

struct AetherBodyDesc {
  AetherShapeDesc shape;
  AetherVec3 position;
  AetherQuat rotation;
  AetherMotionType motionType;
  float friction;    // [0,1], padrão razoável: 0.5
  float restitution; // [0,1], padrão razoável: 0.0 (sem quique)
};

// Opaco de propósito — o layout real (PhysicsSystem, alocador temporário, job
// system, filtros de camada) vive só em jolt_bridge.cpp.
struct AetherPhysicsWorld;

// Handle denso: é o próprio JPH::BodyID (índice + número de sequência
// empacotados por Jolt) reinterpretado como uint32 — não precisamos de uma
// tabela de handles nossa, o Jolt já resolve reciclagem de índice sozinho.
using AetherBodyHandle = ae::u32;
constexpr AetherBodyHandle AetherBodyHandle_Invalid = 0xFFFFFFFFu;

/// Cria um mundo de física com a gravidade e capacidade de corpos dados.
/// `maxBodies` é um teto rígido (ver PhysicsSystem::Init do Jolt) — criar mais
/// corpos que isso falha silenciosamente (devolve handle inválido), nunca
/// estoura buffer. Devolve nullptr em falha de inicialização (praticamente só
/// out-of-memory).
AetherPhysicsWorld *AetherPhysics_CreateWorld(AetherVec3 gravity, ae::u32 maxBodies);

/// Destrói o mundo e TODOS os corpos nele — nenhum AetherBodyHandle deste
/// mundo continua válido depois desta chamada.
void AetherPhysics_DestroyWorld(AetherPhysicsWorld *world);

/// Cria um corpo e já o adiciona ao mundo (ativo, se dinâmico). Devolve
/// AetherBodyHandle_Invalid se o mundo já está no teto de `maxBodies`.
AetherBodyHandle AetherPhysics_CreateBody(AetherPhysicsWorld *world, const AetherBodyDesc *desc);

/// Remove e destrói um corpo. Handle passa a ser inválido depois desta chamada.
void AetherPhysics_DestroyBody(AetherPhysicsWorld *world, AetherBodyHandle handle);

/// Avança a simulação. `collisionSteps` segue a mesma convenção do Jolt: 1 é o
/// caso comum para `deltaTime` de até 1/60s; passos maiores exigem mais de 1
/// para manter a simulação estável (ver comentário no HelloWorld do Jolt).
void AetherPhysics_Step(AetherPhysicsWorld *world, float deltaTime, ae::i32 collisionSteps);

/// Lê a transform atual do corpo (posição do centro de massa + rotação).
/// Ponteiros de saída nulos são ignorados individualmente — chamador pode
/// pedir só posição, só rotação, ou as duas.
void AetherPhysics_GetTransform(AetherPhysicsWorld *world, AetherBodyHandle handle,
                                 AetherVec3 *outPosition, AetherQuat *outRotation);

void AetherPhysics_SetLinearVelocity(AetherPhysicsWorld *world, AetherBodyHandle handle, AetherVec3 velocity);
AetherVec3 AetherPhysics_GetLinearVelocity(AetherPhysicsWorld *world, AetherBodyHandle handle);

/// 1 se o corpo está ativo (não dormindo, não estático) — corpos dinâmicos que
/// param de se mover são colocados para dormir automaticamente pelo Jolt.
ae::i32 AetherPhysics_IsActive(AetherPhysicsWorld *world, AetherBodyHandle handle);

/// Raycast contra todos os corpos do mundo. Devolve 1 se acertou algo, e
/// preenche outBody/outHitFraction (fração ao longo do segmento origin ->
/// origin + direction, em [0,1] se acertou dentro do próprio comprimento do
/// vetor `direction` — direction NÃO é normalizado internamente, seu
/// comprimento define o alcance do raio, mesma convenção do Jolt).
ae::i32 AetherPhysics_RayCastClosest(AetherPhysicsWorld *world, AetherVec3 origin, AetherVec3 direction,
                                      AetherBodyHandle *outBody, float *outHitFraction);

} // extern "C"
