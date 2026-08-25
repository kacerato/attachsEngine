// Implementação de jolt_bridge.h. Ver o comentário de topo do header para o
// escopo desta fatia vertical (item 4.1.1 do plano) e o que fica para depois.
#include "physics/jolt_bridge.h"

// O próprio Jolt pede para Jolt.h vir antes de qualquer outro header do Jolt.
#include <Jolt/Jolt.h>

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/IssueReporting.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <thread>

namespace {

// ------------------------------------------------------------ camadas de colisão
//
// Fatia vertical: só duas camadas, estático vs dinâmico/cinemático — o mesmo
// esquema do exemplo HelloWorld do Jolt. O plano (item 4.1.1) pede "camadas de
// colisão" no plural; um esquema configurável por jogo (N camadas nomeadas) é
// trabalho de um incremento futuro sobre esta base, não desta fatia.
namespace Layers {
constexpr JPH::ObjectLayer NonMoving = 0;
constexpr JPH::ObjectLayer Moving = 1;
constexpr JPH::uint NumLayers = 2;
} // namespace Layers

namespace BroadPhaseLayers {
constexpr JPH::BroadPhaseLayer NonMoving(0);
constexpr JPH::BroadPhaseLayer Moving(1);
constexpr JPH::uint NumLayers = 2;
} // namespace BroadPhaseLayers

class BroadPhaseLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
  BroadPhaseLayerInterfaceImpl() {
    mObjectToBroadPhase[Layers::NonMoving] = BroadPhaseLayers::NonMoving;
    mObjectToBroadPhase[Layers::Moving] = BroadPhaseLayers::Moving;
  }
  JPH::uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NumLayers; }
  JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
    return mObjectToBroadPhase[inLayer];
  }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
  // Só existe (e só é pura na base) quando o profiler do Jolt está habilitado
  // (JPH_PROFILE_ENABLED, ligado por padrão no build de Debug do Jolt) — usada
  // apenas para rotular camadas em capturas de profile, não afeta simulação.
  const char *GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
    switch (static_cast<JPH::BroadPhaseLayer::Type>(inLayer)) {
      case static_cast<JPH::BroadPhaseLayer::Type>(BroadPhaseLayers::NonMoving): return "NonMoving";
      case static_cast<JPH::BroadPhaseLayer::Type>(BroadPhaseLayers::Moving): return "Moving";
      default: return "Invalid";
    }
  }
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

private:
  JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NumLayers];
};

class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
  bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
    if (inLayer1 == Layers::NonMoving) return inLayer2 == BroadPhaseLayers::Moving;
    return true; // Moving colide com tudo
  }
};

class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter {
public:
  bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override {
    if (inObject1 == Layers::NonMoving) return inObject2 == Layers::Moving;
    return true; // Moving colide com tudo, incluindo outro Moving
  }
};

JPH::EMotionType ToJoltMotionType(AetherMotionType type) {
  switch (type) {
    case AetherMotionType::Static: return JPH::EMotionType::Static;
    case AetherMotionType::Kinematic: return JPH::EMotionType::Kinematic;
    default: return JPH::EMotionType::Dynamic;
  }
}

JPH::ObjectLayer ToObjectLayer(AetherMotionType type) {
  return type == AetherMotionType::Static ? Layers::NonMoving : Layers::Moving;
}

JPH::Vec3 ToJolt(AetherVec3 v) { return JPH::Vec3(v.x, v.y, v.z); }
AetherVec3 FromJolt(JPH::Vec3 v) { return {v.GetX(), v.GetY(), v.GetZ()}; }
// Sem overload separado para JPH::RVec3: com DOUBLE_PRECISION=OFF (nossa
// configuração fixa do Jolt, ver CMakeLists.txt — a engine usa float em toda
// a matemática por convenção), Jolt/Math/Real.h define RVec3 como um alias
// direto de Vec3 (não um tipo distinto), então um segundo overload seria uma
// redefinição da mesma função, não uma sobrecarga real.
JPH::Quat ToJolt(AetherQuat q) { return JPH::Quat(q.x, q.y, q.z, q.w); }
AetherQuat FromJolt(JPH::Quat q) { return {q.GetX(), q.GetY(), q.GetZ(), q.GetW()}; }

// ------------------------------------------------------------ Trace/AssertFailed
//
// Os handlers padrão do Jolt (DummyTrace/DummyAssertFailed, ver
// Jolt/Core/IssueReporting.cpp) descartam a mensagem e, no caso de assert,
// simplesmente devolvem `true` — o que dispara JPH_BREAKPOINT (uma instrução
// de trap) sem nenhum texto explicando o quê falhou. Isso combina mal com
// nosso ambiente de teste: um assert interno do Jolt viraria um SIGTRAP puro
// e indecifrável no meio do runner, em vez de uma mensagem legível. Instalamos
// handlers reais aqui, no mesmo estilo stderr+detalhes de core/assert.cpp
// (AE_CHECK) — consistência de diagnóstico entre nosso código e o do Jolt.
void TraceImpl(const char *inFMT, ...) {
  va_list args;
  va_start(args, inFMT);
  char buffer[1024];
  std::vsnprintf(buffer, sizeof(buffer), inFMT, args);
  va_end(args);
  std::fprintf(stderr, "[Jolt] %s\n", buffer);
  std::fflush(stderr);
}

#ifdef JPH_ENABLE_ASSERTS
bool AssertFailedImpl(const char *inExpression, const char *inMessage, const char *inFile, JPH::uint inLine) {
  std::fprintf(stderr, "[Jolt] asserção falhou: %s\n  motivo: %s\n  em %s:%u\n", inExpression,
               inMessage != nullptr ? inMessage : "(sem mensagem)", inFile, inLine);
  std::fflush(stderr);
  return true; // dispara o breakpoint (JPH_BREAKPOINT) depois de já termos impresso o diagnóstico
}
#endif // JPH_ENABLE_ASSERTS

// ------------------------------------------------------------ inicialização global
//
// Factory/RegisterTypes são estado GLOBAL do processo por design do Jolt (não
// por mundo) — ver o comentário no HelloWorld do Jolt. Inicializado uma única
// vez, na primeira criação de mundo; nunca desfeito (o processo do editor/jogo
// vive pela mesma duração que precisaria manter isso vivo de qualquer forma, e
// desfazer errado — antes do último mundo ser destruído — é pior que nunca
// desfazer). Estática de função: inicialização thread-safe garantida pelo
// C++11 sem precisar de <mutex>. Precisa vir DEPOIS de TraceImpl/AssertFailedImpl
// acima (usa os dois por nome dentro do corpo do lambda).
void EnsureGlobalTypesRegistered() {
  static bool registered = [] {
    JPH::Trace = TraceImpl;
    JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = AssertFailedImpl;)
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();
    return true;
  }();
  (void)registered;
}

} // namespace

// Definição real do tipo opaco declarado em jolt_bridge.h. Cada mundo tem seu
// próprio alocador temporário e pool de threads — simples e correto para a
// fatia vertical; compartilhar esses recursos entre mundos simultâneos (ex.:
// preview do editor + play-in-editor ao mesmo tempo) é uma otimização futura,
// não uma correção necessária agora.
struct AetherPhysicsWorld {
  BroadPhaseLayerInterfaceImpl broadPhaseLayerInterface;
  ObjectVsBroadPhaseLayerFilterImpl objectVsBroadPhaseLayerFilter;
  ObjectLayerPairFilterImpl objectLayerPairFilter;
  JPH::PhysicsSystem physicsSystem;
  JPH::TempAllocatorImpl tempAllocator;
  JPH::JobSystemThreadPool jobSystem;

  explicit AetherPhysicsWorld(ae::u32 maxBodies)
      : tempAllocator(8 * 1024 * 1024),
        jobSystem(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, WorkerThreadCount()) {
    const JPH::uint maxBodyPairs = std::max<JPH::uint>(1024, maxBodies);
    const JPH::uint maxContactConstraints = std::max<JPH::uint>(1024, maxBodies);
    physicsSystem.Init(maxBodies, /*inNumBodyMutexes*/ 0, maxBodyPairs, maxContactConstraints,
                        broadPhaseLayerInterface, objectVsBroadPhaseLayerFilter, objectLayerPairFilter);
  }

  static int WorkerThreadCount() {
    // hardware_concurrency() pode devolver 0 (não é garantido pela spec) — nunca deixamos o
    // job system do Jolt receber uma contagem de threads negativa/absurda por causa disso.
    unsigned hw = std::thread::hardware_concurrency();
    return static_cast<int>(hw > 1 ? hw - 1 : 1);
  }
};

extern "C" {

AetherPhysicsWorld *AetherPhysics_CreateWorld(AetherVec3 gravity, ae::u32 maxBodies) {
  EnsureGlobalTypesRegistered();
  auto *world = new AetherPhysicsWorld(maxBodies);
  world->physicsSystem.SetGravity(ToJolt(gravity));
  return world;
}

void AetherPhysics_DestroyWorld(AetherPhysicsWorld *world) {
  delete world;
}

AetherBodyHandle AetherPhysics_CreateBody(AetherPhysicsWorld *world, const AetherBodyDesc *desc) {
  if (world == nullptr || desc == nullptr) return AetherBodyHandle_Invalid;

  JPH::RefConst<JPH::Shape> shape;
  if (desc->shape.kind == AetherShapeKind::Box) {
    JPH::BoxShapeSettings settings(ToJolt(desc->shape.boxHalfExtent));
    auto result = settings.Create();
    if (result.HasError()) return AetherBodyHandle_Invalid;
    shape = result.Get();
  } else {
    JPH::SphereShapeSettings settings(desc->shape.sphereRadius);
    auto result = settings.Create();
    if (result.HasError()) return AetherBodyHandle_Invalid;
    shape = result.Get();
  }

  JPH::BodyCreationSettings bodySettings(
      shape, JPH::RVec3(desc->position.x, desc->position.y, desc->position.z), ToJolt(desc->rotation),
      ToJoltMotionType(desc->motionType), ToObjectLayer(desc->motionType));
  bodySettings.mFriction = desc->friction;
  bodySettings.mRestitution = desc->restitution;

  JPH::BodyInterface &bodyInterface = world->physicsSystem.GetBodyInterface();
  JPH::BodyID id = bodyInterface.CreateAndAddBody(
      bodySettings, desc->motionType == AetherMotionType::Static ? JPH::EActivation::DontActivate
                                                                   : JPH::EActivation::Activate);
  if (id.IsInvalid()) return AetherBodyHandle_Invalid;
  return id.GetIndexAndSequenceNumber();
}

void AetherPhysics_DestroyBody(AetherPhysicsWorld *world, AetherBodyHandle handle) {
  if (world == nullptr || handle == AetherBodyHandle_Invalid) return;
  JPH::BodyID id(handle);
  JPH::BodyInterface &bodyInterface = world->physicsSystem.GetBodyInterface();
  bodyInterface.RemoveBody(id);
  bodyInterface.DestroyBody(id);
}

void AetherPhysics_Step(AetherPhysicsWorld *world, float deltaTime, ae::i32 collisionSteps) {
  if (world == nullptr) return;
  world->physicsSystem.Update(deltaTime, collisionSteps, &world->tempAllocator, &world->jobSystem);
}

void AetherPhysics_GetTransform(AetherPhysicsWorld *world, AetherBodyHandle handle, AetherVec3 *outPosition,
                                 AetherQuat *outRotation) {
  if (world == nullptr || handle == AetherBodyHandle_Invalid) return;
  JPH::BodyID id(handle);
  JPH::BodyInterface &bodyInterface = world->physicsSystem.GetBodyInterface();
  if (outPosition != nullptr) *outPosition = FromJolt(bodyInterface.GetCenterOfMassPosition(id));
  if (outRotation != nullptr) *outRotation = FromJolt(bodyInterface.GetRotation(id));
}

void AetherPhysics_SetLinearVelocity(AetherPhysicsWorld *world, AetherBodyHandle handle, AetherVec3 velocity) {
  if (world == nullptr || handle == AetherBodyHandle_Invalid) return;
  world->physicsSystem.GetBodyInterface().SetLinearVelocity(JPH::BodyID(handle), ToJolt(velocity));
}

AetherVec3 AetherPhysics_GetLinearVelocity(AetherPhysicsWorld *world, AetherBodyHandle handle) {
  if (world == nullptr || handle == AetherBodyHandle_Invalid) return {0.0f, 0.0f, 0.0f};
  return FromJolt(world->physicsSystem.GetBodyInterface().GetLinearVelocity(JPH::BodyID(handle)));
}

ae::i32 AetherPhysics_IsActive(AetherPhysicsWorld *world, AetherBodyHandle handle) {
  if (world == nullptr || handle == AetherBodyHandle_Invalid) return 0;
  return world->physicsSystem.GetBodyInterface().IsActive(JPH::BodyID(handle)) ? 1 : 0;
}

ae::i32 AetherPhysics_RayCastClosest(AetherPhysicsWorld *world, AetherVec3 origin, AetherVec3 direction,
                                      AetherBodyHandle *outBody, float *outHitFraction) {
  if (world == nullptr) return 0;

  JPH::RRayCast ray(JPH::RVec3(origin.x, origin.y, origin.z), ToJolt(direction));
  JPH::RayCastResult hit;
  bool found = world->physicsSystem.GetNarrowPhaseQuery().CastRay(ray, hit);
  if (!found) return 0;

  if (outBody != nullptr) *outBody = hit.mBodyID.GetIndexAndSequenceNumber();
  if (outHitFraction != nullptr) *outHitFraction = hit.mFraction;
  return 1;
}

} // extern "C"
