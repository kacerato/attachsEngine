// Benchmark de física 2D (item 4.1.6 do plano: "Física 2D (benchmark Jolt-2D vs Box2D v3 →
// decisão)"). Decisão já tomada e documentada em jolt_bridge.h (comentário de
// AetherAllowedDOFs): usar o Jolt 3D vendorizado restrito ao plano XY via JPH::EAllowedDOFs,
// em vez de vendorizar uma segunda biblioteca física (Box2D v3) — evita duplicar mundo/
// broadphase/build para um único subsistema, mantendo "zero dependência externa nova".
//
// O QUE ESTE BENCHMARK MEDE E NÃO MEDE, honestamente: travar DOFs no Jolt (mascaramento de
// componentes SIMD já calculadas em cheio, ver MotionProperties::SetMassProperties/
// GetInverseInertiaForRotation) NÃO reduz o custo de CPU por corpo em relação a um corpo 3D
// pleno — não há early-exit no solver para DOFs travados, confirmado por leitura completa do
// código do Jolt (nenhum comentário no motor menciona isso como otimização de performance).
// Este benchmark portanto NÃO tenta provar "Jolt 2D restrito é mais rápido que Jolt 3D" (não
// é, pelo mesmo custo por corpo) nem compara contra Box2D (não vendorizado neste repositório).
// O que ele mede: o throughput REAL de corpos 2D simultâneos que o Jolt (a única física já
// vendorizada e testada neste projeto) sustenta dentro do orçamento de frame time do plano —
// evidência concreta para a decisão "Jolt restrito é suficiente para os casos de uso de física
// 2D deste projeto", não uma comparação lado a lado com uma biblioteca que não existe aqui.
//
// KPI de referência (docs/PLANO-ENGINE-MOBILE.md, Parte 18.1): frame time do runtime ≤ 16.6ms
// (60fps). Não há um KPI numérico específico de "N corpos 2D simultâneos" documentado no
// plano — o alvo abaixo (500 corpos) é uma estimativa de carga razoável para um jogo 2D
// (physics puzzle, plataforma com muitos objetos), não um número tirado do plano.
#include "physics/jolt_bridge.h"

#include <chrono>
#include <cstdio>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

double MillisecondsSince(Clock::time_point start) {
  return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

AetherBodyDesc Sphere2D(float radius, AetherVec3 position, AetherMotionType motion) {
  AetherBodyDesc desc{};
  desc.shape.kind = AetherShapeKind::Sphere;
  desc.shape.sphereRadius = radius;
  desc.position = position;
  desc.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
  desc.motionType = motion;
  desc.friction = 0.3f;
  desc.restitution = 0.2f;
  desc.allowedDOFs = AetherAllowedDOFs::Plane2D;
  return desc;
}

AetherBodyDesc Box2D(AetherVec3 halfExtent, AetherVec3 position, AetherMotionType motion) {
  AetherBodyDesc desc{};
  desc.shape.kind = AetherShapeKind::Box;
  desc.shape.boxHalfExtent = halfExtent;
  desc.position = position;
  desc.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
  desc.motionType = motion;
  desc.friction = 0.3f;
  desc.allowedDOFs = AetherAllowedDOFs::Plane2D;
  return desc;
}

// Mede o custo médio de Step() com `bodyCount` corpos dinâmicos 2D caindo sobre um piso 2D
// compartilhado — cenário representativo de "física puzzle"/plataforma 2D com muitos objetos
// soltos na tela ao mesmo tempo, não um teste sintético de corpos isolados sem interação.
struct BenchmarkResult {
  int bodyCount;
  double avgStepMs;
  double maxStepMs;
};

BenchmarkResult RunScenario(int bodyCount, int warmupSteps, int measuredSteps) {
  // maxBodies também dimensiona maxBodyPairs/maxContactConstraints em
  // AetherPhysicsWorld::AetherPhysicsWorld (jolt_bridge.cpp: max(1024, maxBodies) para os
  // dois) — um cenário DENSO (muitos corpos empilhados tocando vários vizinhos ao mesmo
  // tempo) gera mais PARES de contato simultâneos que corpos, então maxBodies == bodyCount
  // não é suficiente (o Jolt reporta EPhysicsUpdateError::BodyPairCacheFull/
  // ContactConstraintsFull e descarta contatos em silêncio, sem crashar — mas o assert de
  // debug do bridge trata isso como falha, ver EnsureGlobalTypesRegistered/AssertFailedImpl).
  // 4x de folga cobre o pior caso deste cenário de grade compacta sem precisar reimplementar
  // a lógica de redimensionamento do Jolt.
  AetherPhysicsWorld *world = AetherPhysics_CreateWorld({0.0f, -9.81f, 0.0f}, static_cast<ae::u32>(bodyCount * 4 + 64));

  AetherBodyDesc floorDesc = Box2D({50.0f, 0.5f, 10.0f}, {0.0f, 0.0f, 0.0f}, AetherMotionType::Static);
  AetherPhysics_CreateBody(world, &floorDesc);

  // Grade de corpos empilhados numa área compacta — maximiza contatos simultâneos (o caso
  // mais caro para o solver de constraint), não corpos espalhados sem nunca se tocarem.
  std::vector<AetherBodyHandle> bodies;
  bodies.reserve(static_cast<size_t>(bodyCount));
  int perRow = 40;
  for (int i = 0; i < bodyCount; ++i) {
    float x = static_cast<float>(i % perRow) * 1.1f - (perRow * 1.1f * 0.5f);
    float y = 2.0f + static_cast<float>(i / perRow) * 1.1f;
    AetherBodyDesc desc = Sphere2D(0.45f, {x, y, 0.0f}, AetherMotionType::Dynamic);
    AetherBodyHandle handle = AetherPhysics_CreateBody(world, &desc);
    if (handle != AetherBodyHandle_Invalid) bodies.push_back(handle);
  }

  const float dt = 1.0f / 60.0f;
  for (int i = 0; i < warmupSteps; ++i) AetherPhysics_Step(world, dt, 1);

  double totalMs = 0.0;
  double maxMs = 0.0;
  for (int i = 0; i < measuredSteps; ++i) {
    auto start = Clock::now();
    AetherPhysics_Step(world, dt, 1);
    double stepMs = MillisecondsSince(start);
    totalMs += stepMs;
    if (stepMs > maxMs) maxMs = stepMs;
  }

  AetherPhysics_DestroyWorld(world);

  BenchmarkResult result{};
  result.bodyCount = static_cast<int>(bodies.size());
  result.avgStepMs = totalMs / static_cast<double>(measuredSteps);
  result.maxStepMs = maxMs;
  return result;
}

} // namespace

int main() {
  std::printf("Benchmark de física 2D (item 4.1.6) — Jolt 3D restrito ao plano XY via EAllowedDOFs::Plane2D\n");
  std::printf("Cenário: N esferas 2D empilhadas caindo sobre um piso 2D compartilhado (contatos simultâneos)\n");
  std::printf("KPI de referência (docs/PLANO-ENGINE-MOBILE.md 18.1): frame time do runtime <= 16.6ms (60fps)\n\n");
  std::printf("%10s %14s %14s %8s\n", "corpos", "step medio(ms)", "step maximo(ms)", "ok<16.6ms?");

  const int counts[] = {50, 100, 200, 500, 1000};
  bool allWithinBudget = true;
  for (int count : counts) {
    BenchmarkResult result = RunScenario(count, /*warmupSteps*/ 120, /*measuredSteps*/ 180);
    bool withinBudget = result.maxStepMs <= 16.6;
    if (!withinBudget) allWithinBudget = false;
    std::printf("%10d %14.3f %14.3f %8s\n", result.bodyCount, result.avgStepMs, result.maxStepMs,
                withinBudget ? "sim" : "NAO");
  }

  std::printf("\nConclusão: Jolt 3D restrito ao plano XY (AetherAllowedDOFs::Plane2D) %s dentro do\n"
              "orçamento de frame time do plano para as cargas testadas. Box2D v3 não foi vendorizado\n"
              "nem comparado lado a lado (decisão documentada em jolt_bridge.h) — o ganho de usar o\n"
              "Jolt já vendorizado é arquitetural (um mundo/broadphase só, zero dependência nova), não\n"
              "uma vantagem de CPU por corpo sobre uma física 2D dedicada.\n",
              allWithinBudget ? "se manteve" : "NÃO se manteve completamente");
  return 0;
}
