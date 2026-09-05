// Implementação de jolt_bridge.h. Ver o comentário de topo do header para o
// escopo desta fatia vertical (item 4.1.1 do plano) e o que fica para depois.
#include "physics/jolt_bridge.h"
#include "physics/water_buoyancy.h"

// O próprio Jolt pede para Jolt.h vir antes de qualquer outro header do Jolt.
#include <Jolt/Jolt.h>

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/IssueReporting.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <limits>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

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

/// Item 4.1.6 (física 2D): converte AetherAllowedDOFs para JPH::EAllowedDOFs. Nota de
/// convenção: AetherAllowedDOFs::All == 0 na fronteira (ver comentário completo em
/// jolt_bridge.h), mas JPH::EAllowedDOFs::All == 0b111111 — a tradução abaixo faz essa
/// inversão explicitamente, não é um cast direto de bits (os dois enums NÃO compartilham
/// layout, só os bits de 1 a 5 coincidem por termos escolhido os mesmos valores de propósito).
JPH::EAllowedDOFs ToJoltAllowedDOFs(AetherAllowedDOFs dofs) {
  if (dofs == AetherAllowedDOFs::All) return JPH::EAllowedDOFs::All;
  return static_cast<JPH::EAllowedDOFs>(static_cast<ae::u8>(dofs));
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

/// Extraído de AetherPhysics_CreateBody (item 4.1.1) para ser reusado pelas queries de
/// shapecast/overlap (item 4.1.4), que precisam da mesma conversão AetherShapeDesc->JPH::Shape
/// sem criar um corpo. Devolve shape nulo (RefConst vazio) em erro de criação — mesma
/// disciplina do resto da fronteira, chamador confere antes de usar.
JPH::RefConst<JPH::Shape> ToJoltShape(const AetherShapeDesc &desc) {
  if (desc.kind == AetherShapeKind::Box) {
    JPH::BoxShapeSettings settings(ToJolt(desc.boxHalfExtent));
    auto result = settings.Create();
    return result.HasError() ? nullptr : result.Get();
  }
  if (desc.kind == AetherShapeKind::Capsule) {
    JPH::CapsuleShapeSettings settings(desc.capsuleHalfHeight, desc.sphereRadius);
    auto result = settings.Create();
    return result.HasError() ? nullptr : result.Get();
  }
  JPH::SphereShapeSettings settings(desc.sphereRadius);
  auto result = settings.Create();
  return result.HasError() ? nullptr : result.Get();
}

/// Filtro de ObjectLayer para queries (item 4.1.4) a partir da máscara AetherQueryLayerMask.
/// ShouldCollide(ObjectLayer) simplesmente confere se o bit correspondente está ligado —
/// mapeamento direto porque hoje só existem as 2 camadas fixas Layers::NonMoving/Moving (ver
/// comentário em AetherQueryLayerMask, jolt_bridge.h). Sem herdar de JPH::ObjectLayerFilter
/// diretamente teria que reimplementar essa checagem em cada chamador — centralizado aqui.
class QueryLayerFilter final : public JPH::ObjectLayerFilter {
public:
  explicit QueryLayerFilter(AetherQueryLayerMask mask) : mMask(mask) {}
  bool ShouldCollide(JPH::ObjectLayer inLayer) const override {
    ae::u32 bit = inLayer == Layers::NonMoving ? static_cast<ae::u32>(AetherQueryLayerMask::Static)
                                                : static_cast<ae::u32>(AetherQueryLayerMask::Dynamic);
    return (static_cast<ae::u32>(mMask) & bit) != 0;
  }

private:
  AetherQueryLayerMask mMask;
};

/// Espelha exatamente JPH::IgnoreSingleBodyFilter (Jolt/Physics/Body/BodyFilter.h) — não
/// reusamos a classe do Jolt diretamente porque ela guarda um JPH::BodyID, e aqui só temos o
/// AetherBodyHandle antes de decidir se é válido; um wrapper fino evita expor o tipo do Jolt
/// na assinatura de uma função auxiliar nossa.
class QueryBodyFilter final : public JPH::BodyFilter {
public:
  explicit QueryBodyFilter(AetherBodyHandle ignoreBody)
      : mIgnore(ignoreBody == AetherBodyHandle_Invalid ? JPH::BodyID() : JPH::BodyID(ignoreBody)) {}
  bool ShouldCollide(const JPH::BodyID &inBodyID) const override { return mIgnore != inBodyID; }

private:
  JPH::BodyID mIgnore;
};

/// Copia um CollideShapeResult/ShapeCastResult (ambos têm os mesmos 3 campos de contato,
/// ShapeCastResult herda de CollideShapeResult) para o AetherShapeQueryHit da fronteira.
/// Template sobre o tipo de resultado porque as duas structs do Jolt não compartilham uma
/// base pública com esses campos além da herança direta — evita duplicar o corpo da função
/// para CollideShapeResult (usado por OverlapShape) e ShapeCastResult (usado por
/// ShapeCastClosest).
template <typename JoltResult>
AetherShapeQueryHit ToShapeQueryHit(const JoltResult &hit, float fraction) {
  AetherShapeQueryHit out{};
  out.body = hit.mBodyID2.GetIndexAndSequenceNumber();
  out.fraction = fraction;
  out.contactPointOnQuery = FromJolt(hit.mContactPointOn1);
  out.contactPointOnHit = FromJolt(hit.mContactPointOn2);
  out.penetrationAxis = FromJolt(hit.mPenetrationAxis);
  return out;
}

// ------------------------------------------------------------ handles índice+geração
//
// Diferente de AetherBodyHandle (que É o JPH::BodyID, sem tabela nossa — o Jolt já resolve
// reciclagem de índice sozinho), nem JPH::Constraint nem JPH::CharacterVirtual carregam um
// índice denso reciclável embutido (ao contrário de corpo). Handle = (índice na tabela << 16) |
// (geração & 0xFFFF), mesmo esquema de detecção de use-after-free que EntitySlot.Version faz no
// lado C# (World.cs): destruir o recurso incrementa a geração do slot, então um handle antigo
// nunca combina por acidente com o slot reciclado por um recurso novo. Compartilhado entre
// juntas (4.1.3) e character controllers (4.1.5) — mesma disciplina, tipo de recurso diferente.
constexpr ae::u32 kGenerationalHandleBits = 16;
constexpr ae::u32 kGenerationalHandleMask = (1u << kGenerationalHandleBits) - 1u;

ae::u32 PackGenerationalHandle(ae::u32 index, ae::u32 generation) {
  return (index << kGenerationalHandleBits) | (generation & kGenerationalHandleMask);
}
ae::u32 GenerationalHandleIndex(ae::u32 h) { return h >> kGenerationalHandleBits; }
ae::u32 GenerationalHandleGeneration(ae::u32 h) { return h & kGenerationalHandleMask; }

struct JointSlot {
  JPH::Ref<JPH::Constraint> constraint; // null = slot livre
  JPH::BodyID body1;
  JPH::BodyID body2;
  ae::u32 generation = 0;
};

JPH::MotorSettings ToJoltMotorSettings(const AetherJointMotorDesc &desc, bool isAngular) {
  JPH::MotorSettings settings;
  settings.mSpringSettings.mMode = JPH::ESpringMode::FrequencyAndDamping;
  settings.mSpringSettings.mFrequency = desc.springFrequency;
  settings.mSpringSettings.mDamping = desc.springDamping;
  if (isAngular) settings.SetTorqueLimit(desc.maxForceOrTorque);
  else settings.SetForceLimit(desc.maxForceOrTorque);
  return settings;
}

JPH::EMotorState ToJoltMotorState(AetherMotorState state) {
  switch (state) {
    case AetherMotorState::Velocity: return JPH::EMotorState::Velocity;
    case AetherMotorState::Position: return JPH::EMotorState::Position;
    case AetherMotorState::PositionAndVelocity: return JPH::EMotorState::PositionAndVelocity;
    default: return JPH::EMotorState::Off;
  }
}

// ------------------------------------------------------------ character controller (4.1.5)

// Mesmo esquema índice+geração de JointSlot — JPH::CharacterVirtual não é adicionado à
// broadphase (não tem BodyID) e não carrega índice denso reciclável. Guardamos as duas
// cápsulas (de pé / agachada) já convertidas para não reconstruir a forma toda vez que o
// jogo alterna estado — troca de shape (AetherPhysics_SetCharacterCrouching) é evento raro,
// mas reconstruir uma CapsuleShape do zero a cada chamada seria desperdício desnecessário.
struct CharacterSlot {
  JPH::Ref<JPH::CharacterVirtual> character; // null = slot livre
  JPH::RefConst<JPH::Shape> standingShape;
  JPH::RefConst<JPH::Shape> crouchingShape;
  bool isCrouching = false;
  ae::u32 generation = 0;
};

AetherCharacterGroundState FromJoltGroundState(JPH::CharacterBase::EGroundState state) {
  switch (state) {
    case JPH::CharacterBase::EGroundState::OnGround: return AetherCharacterGroundState::OnGround;
    case JPH::CharacterBase::EGroundState::OnSteepGround: return AetherCharacterGroundState::OnSteepGround;
    case JPH::CharacterBase::EGroundState::NotSupported: return AetherCharacterGroundState::NotSupported;
    default: return AetherCharacterGroundState::InAir;
  }
}

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

// ------------------------------------------------------------ triggers/sensors (GAP-PHY-03)
//
// O Jolt chama ContactListener em workers concorrentes e OnContactRemoved só
// entrega ids/subshapes. Por isso o listener conserva o conjunto de subcontatos
// ativo, agrega-o por par dirigido sensor->outro e protege tudo por um mutex.
// Eventos publicados são uma fotografia por Step, ordenada e sem duplicatas:
// uma shape composta com vários subshapes ainda produz um único Enter/Stay/Exit
// lógico para o par de corpos.
class TriggerContactListener final : public JPH::ContactListener {
public:
  void RegisterSensor(AetherBodyHandle body, ae::u32 eventLayerMask) {
    std::lock_guard<std::mutex> lock(mMutex);
    mSensorMasks[body] = eventLayerMask;
  }

  void UnregisterSensor(AetherBodyHandle body) {
    std::lock_guard<std::mutex> lock(mMutex);
    mSensorMasks.erase(body);
  }

  void BeginStep() {
    std::lock_guard<std::mutex> lock(mMutex);
    mFrameEvents = std::move(mPendingEvents);
    mPendingEvents.clear();
    mInStep = true;
  }

  void KeepOverlappingBodiesAwake(JPH::BodyInterface &bodyInterface) {
    std::lock_guard<std::mutex> lock(mMutex);
    // O Jolt publica OnContactRemoved ao colocar um corpo para dormir, mesmo
    // que ele continue geometricamente dentro de um sensor. Isso é correto
    // para constraints, mas seria um Exit falso para gameplay. Só os pares de
    // trigger realmente ativos são reativados; corpos fora de sensores mantêm
    // a política normal de sleep.
    for (const auto &entry : mActivePairCounts) {
      const JPH::BodyID sensor(entry.first.sensor);
      const JPH::BodyID other(entry.first.other);
      if (bodyInterface.IsAdded(sensor) &&
          bodyInterface.GetMotionType(sensor) != JPH::EMotionType::Static)
        bodyInterface.ActivateBody(sensor);
      if (bodyInterface.IsAdded(other) &&
          bodyInterface.GetMotionType(other) != JPH::EMotionType::Static)
        bodyInterface.ActivateBody(other);
    }
  }

  void EndStep() {
    std::lock_guard<std::mutex> lock(mMutex);
    mInStep = false;
    std::sort(mFrameEvents.begin(), mFrameEvents.end(), EventLess);
    mFrameEvents.erase(std::unique(mFrameEvents.begin(), mFrameEvents.end(), EventEqual),
                       mFrameEvents.end());
  }

  ae::i32 CopyEvents(AetherTriggerEvent *outEvents, ae::i32 maxResults) {
    std::lock_guard<std::mutex> lock(mMutex);
    const ae::i32 count = static_cast<ae::i32>(mFrameEvents.size());
    const ae::i32 toCopy = outEvents != nullptr && maxResults > 0
                               ? std::min(count, maxResults)
                               : 0;
    std::copy_n(mFrameEvents.begin(), toCopy, outEvents);
    return count;
  }

  void OnContactAdded(const JPH::Body &body1, const JPH::Body &body2,
                      const JPH::ContactManifold &manifold,
                      JPH::ContactSettings &) override {
    std::lock_guard<std::mutex> lock(mMutex);
    const JPH::SubShapeIDPair subPair(body1.GetID(), manifold.mSubShapeID1,
                                      body2.GetID(), manifold.mSubShapeID2);
    if (mActiveSubShapes.find(subPair) != mActiveSubShapes.end()) return;

    ActiveSubShape active{};
    AddDirection(body1, body2, active);
    AddDirection(body2, body1, active);
    mActiveSubShapes.emplace(subPair, active);
  }

  void OnContactPersisted(const JPH::Body &body1, const JPH::Body &body2,
                          const JPH::ContactManifold &manifold,
                          JPH::ContactSettings &) override {
    std::lock_guard<std::mutex> lock(mMutex);
    const JPH::SubShapeIDPair subPair(body1.GetID(), manifold.mSubShapeID1,
                                      body2.GetID(), manifold.mSubShapeID2);
    auto it = mActiveSubShapes.find(subPair);
    if (it == mActiveSubShapes.end()) {
      // Pode ocorrer depois de troca de shape/manifold. Trate como um contato
      // novo em vez de emitir Stay sem Enter.
      ActiveSubShape active{};
      AddDirection(body1, body2, active);
      AddDirection(body2, body1, active);
      mActiveSubShapes.emplace(subPair, active);
      return;
    }
    for (ae::u32 i = 0; i < it->second.count; ++i)
      AppendEvent(it->second.directions[i], AetherTriggerEventType::Stay);
  }

  void OnContactRemoved(const JPH::SubShapeIDPair &subPair) override {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mActiveSubShapes.find(subPair);
    if (it == mActiveSubShapes.end()) return;
    for (ae::u32 i = 0; i < it->second.count; ++i) {
      const DirectedPair pair = it->second.directions[i];
      auto countIt = mActivePairCounts.find(pair);
      if (countIt == mActivePairCounts.end()) continue;
      if (--countIt->second == 0) {
        AppendEvent(pair, AetherTriggerEventType::Exit);
        mActivePairCounts.erase(countIt);
      }
    }
    mActiveSubShapes.erase(it);
  }

private:
  struct DirectedPair {
    AetherBodyHandle sensor = AetherBodyHandle_Invalid;
    AetherBodyHandle other = AetherBodyHandle_Invalid;
    bool operator<(const DirectedPair &rhs) const {
      return sensor != rhs.sensor ? sensor < rhs.sensor : other < rhs.other;
    }
  };

  struct ActiveSubShape {
    DirectedPair directions[2]{};
    ae::u32 count = 0;
  };

  static ae::u32 LayerBit(JPH::ObjectLayer layer) {
    return layer == Layers::NonMoving
               ? static_cast<ae::u32>(AetherQueryLayerMask::Static)
               : static_cast<ae::u32>(AetherQueryLayerMask::Dynamic);
  }

  void AddDirection(const JPH::Body &candidateSensor, const JPH::Body &other,
                    ActiveSubShape &active) {
    if (!candidateSensor.IsSensor()) return;
    const DirectedPair pair{candidateSensor.GetID().GetIndexAndSequenceNumber(),
                            other.GetID().GetIndexAndSequenceNumber()};
    auto maskIt = mSensorMasks.find(pair.sensor);
    if (maskIt == mSensorMasks.end() || (maskIt->second & LayerBit(other.GetObjectLayer())) == 0)
      return;
    if (active.count < 2) active.directions[active.count++] = pair;
    ae::u32 &count = mActivePairCounts[pair];
    if (count++ == 0) AppendEvent(pair, AetherTriggerEventType::Enter);
  }

  void AppendEvent(const DirectedPair &pair, AetherTriggerEventType type) {
    AetherTriggerEvent event{pair.sensor, pair.other, type, 0};
    (mInStep ? mFrameEvents : mPendingEvents).push_back(event);
  }

  static bool EventLess(const AetherTriggerEvent &a, const AetherTriggerEvent &b) {
    if (a.sensor != b.sensor) return a.sensor < b.sensor;
    if (a.other != b.other) return a.other < b.other;
    return static_cast<ae::u32>(a.type) < static_cast<ae::u32>(b.type);
  }

  static bool EventEqual(const AetherTriggerEvent &a, const AetherTriggerEvent &b) {
    return a.sensor == b.sensor && a.other == b.other && a.type == b.type;
  }

  std::mutex mMutex;
  std::map<AetherBodyHandle, ae::u32> mSensorMasks;
  std::map<JPH::SubShapeIDPair, ActiveSubShape> mActiveSubShapes;
  std::map<DirectedPair, ae::u32> mActivePairCounts;
  std::vector<AetherTriggerEvent> mPendingEvents;
  std::vector<AetherTriggerEvent> mFrameEvents;
  bool mInStep = false;
};

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
  TriggerContactListener triggerListener;
  JPH::PhysicsSystem physicsSystem;
  JPH::TempAllocatorImpl tempAllocator;
  JPH::JobSystemThreadPool jobSystem;

  // Tabela de juntas deste mundo — ver comentário de JointSlot acima. Slots livres formam
  // uma lista encadeada através de `freeList` (índice do próximo livre, ou -1); mesma
  // estratégia de reciclagem de índice que World.cs usa para EntitySlot (_freeIndices).
  std::vector<JointSlot> jointSlots;
  std::vector<ae::u32> freeJointSlots;

  // Tabela de character controllers deste mundo — ver comentário de CharacterSlot acima.
  std::vector<CharacterSlot> characterSlots;
  std::vector<ae::u32> freeCharacterSlots;

  AetherPhysicsWorldDescV2 desc{};
  AetherPhysicsStepStatsV2 stepStats{};

  static ae::usize TempAllocatorBytes(const AetherPhysicsWorldDescV2 &worldDesc) {
    // O Jolt usa este bloco para arrays transitórios de corpos, candidatos da
    // broad phase e constraints. Um teto fixo de 8 MiB quebrava justamente os
    // cenários V2 densos; a estimativa abaixo acompanha as capacidades que o
    // chamador decidiu, mantendo 8 MiB como piso para mundos pequenos.
    constexpr ae::u64 minimum = 8ull * 1024ull * 1024ull;
    const ae::u64 estimated = static_cast<ae::u64>(worldDesc.maxBodies) * 512ull +
                              static_cast<ae::u64>(worldDesc.maxBodyPairs) * 128ull +
                              static_cast<ae::u64>(worldDesc.maxContactConstraints) * 1024ull +
                              static_cast<ae::u64>(worldDesc.maxBroadPhasePairs) * 64ull;
    const ae::u64 selected = std::max(minimum, estimated);
    const ae::u64 sizeLimit = static_cast<ae::u64>(std::numeric_limits<ae::usize>::max());
    return static_cast<ae::usize>(std::min(selected, sizeLimit));
  }

  explicit AetherPhysicsWorld(const AetherPhysicsWorldDescV2 &worldDesc)
      : tempAllocator(TempAllocatorBytes(worldDesc)),
        jobSystem(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, WorkerThreadCount()), desc(worldDesc) {
    stepStats.structSize = sizeof(AetherPhysicsStepStatsV2);
    stepStats.apiVersion = AetherPhysicsWorldApiVersionV2;

    physicsSystem.Init(desc.maxBodies, /*inNumBodyMutexes*/ 0, desc.maxBodyPairs,
                        desc.maxContactConstraints,
                        broadPhaseLayerInterface, objectVsBroadPhaseLayerFilter, objectLayerPairFilter);

    JPH::PhysicsSettings settings = physicsSystem.GetPhysicsSettings();
    settings.mMaxInFlightBodyPairs = static_cast<int>(desc.maxBroadPhasePairs);
    physicsSystem.SetPhysicsSettings(settings);
    physicsSystem.SetContactListener(&triggerListener);
  }

  static int WorkerThreadCount() {
    // hardware_concurrency() pode devolver 0 (não é garantido pela spec) — nunca deixamos o
    // job system do Jolt receber uma contagem de threads negativa/absurda por causa disso.
    unsigned hw = std::thread::hardware_concurrency();
    return static_cast<int>(hw > 1 ? hw - 1 : 1);
  }
};

namespace {

ae::u32 SaturatingMultiply(ae::u32 value, ae::u32 multiplier) {
  const ae::u64 result = static_cast<ae::u64>(value) * multiplier;
  return result > std::numeric_limits<ae::u32>::max() ? std::numeric_limits<ae::u32>::max()
                                                       : static_cast<ae::u32>(result);
}

AetherPhysicsWorldDescV2 MakeV1CompatibleDesc(AetherVec3 gravity, ae::u32 maxBodies) {
  AetherPhysicsWorldDescV2 desc{};
  desc.structSize = sizeof(desc);
  desc.apiVersion = AetherPhysicsWorldApiVersionV2;
  desc.gravity = gravity;
  desc.maxBodies = maxBodies;
  desc.maxBodyPairs = std::max<ae::u32>(1024, SaturatingMultiply(maxBodies, 4));
  desc.maxContactConstraints = std::max<ae::u32>(1024, SaturatingMultiply(maxBodies, 2));
  desc.maxBroadPhasePairs = std::max<ae::u32>(16384, desc.maxBodyPairs);
  desc.overflowPolicy = AetherPhysicsOverflowPolicy::BuildDefault;
  return desc;
}

bool IsValidWorldDesc(const AetherPhysicsWorldDescV2 *desc) {
  if (desc == nullptr) {
    std::fprintf(stderr, "[Physics] CreateWorldV2 recusado: descritor nulo.\n");
    return false;
  }
  if (desc->structSize < sizeof(AetherPhysicsWorldDescV2) ||
      desc->apiVersion != AetherPhysicsWorldApiVersionV2) {
    std::fprintf(stderr,
                 "[Physics] CreateWorldV2 recusado: structSize=%u (mínimo=%zu), apiVersion=%u (esperado=%u).\n",
                 desc->structSize, sizeof(AetherPhysicsWorldDescV2), desc->apiVersion,
                 AetherPhysicsWorldApiVersionV2);
    return false;
  }
  if (desc->maxBodies == 0 || desc->maxBodyPairs < 4 || desc->maxContactConstraints == 0 ||
      desc->maxBroadPhasePairs < 1024 ||
      desc->maxBodies > JPH::PhysicsSystem::cMaxBodiesLimit ||
      desc->maxBodyPairs > JPH::PhysicsSystem::cMaxBodyPairsLimit ||
      desc->maxContactConstraints > JPH::PhysicsSystem::cMaxContactConstraintsLimit ||
      desc->maxBroadPhasePairs > static_cast<ae::u32>(std::numeric_limits<int>::max())) {
    std::fprintf(stderr,
                 "[Physics] CreateWorldV2 recusado: limites inválidos (bodies=%u, bodyPairs=%u, contacts=%u, broadPhasePairs=%u).\n",
                 desc->maxBodies, desc->maxBodyPairs, desc->maxContactConstraints,
                 desc->maxBroadPhasePairs);
    return false;
  }
  if (desc->overflowPolicy != AetherPhysicsOverflowPolicy::BuildDefault &&
      desc->overflowPolicy != AetherPhysicsOverflowPolicy::Warning &&
      desc->overflowPolicy != AetherPhysicsOverflowPolicy::FailFast) {
    std::fprintf(stderr, "[Physics] CreateWorldV2 recusado: política de overflow desconhecida (%u).\n",
                 static_cast<ae::u32>(desc->overflowPolicy));
    return false;
  }
  return true;
}

bool ShouldFailFast(AetherPhysicsOverflowPolicy policy) {
  if (policy == AetherPhysicsOverflowPolicy::FailFast) return true;
  if (policy == AetherPhysicsOverflowPolicy::Warning) return false;
#ifdef NDEBUG
  return false;
#else
  return true;
#endif
}

bool ShouldLogOccurrence(ae::u64 count) {
  // Primeiro evento e potências de dois: diagnóstico imediato sem inundar o
  // log em release quando o mesmo mundo permanece saturado por muitos frames.
  return count == 1 || (count & (count - 1)) == 0;
}

void ReportUpdateErrors(AetherPhysicsWorld &world, ae::u32 flags) {
  world.stepStats.lastErrorFlags = flags;
  ++world.stepStats.overflowSteps;

  struct ErrorInfo {
    ae::u32 flag;
    const char *category;
    ae::u32 capacity;
    ae::u64 *counter;
  } errors[] = {
      {static_cast<ae::u32>(AetherPhysicsUpdateError::ManifoldCacheFull), "ManifoldCacheFull",
       world.desc.maxContactConstraints, &world.stepStats.manifoldCacheFullCount},
      {static_cast<ae::u32>(AetherPhysicsUpdateError::BodyPairCacheFull), "BodyPairCacheFull",
       world.desc.maxBodyPairs, &world.stepStats.bodyPairCacheFullCount},
      {static_cast<ae::u32>(AetherPhysicsUpdateError::ContactConstraintsFull),
       "ContactConstraintsFull", world.desc.maxContactConstraints,
       &world.stepStats.contactConstraintsFullCount},
  };

  for (ErrorInfo &error : errors) {
    if ((flags & error.flag) == 0) continue;
    ++*error.counter;
    if (ShouldLogOccurrence(*error.counter)) {
      std::fprintf(stderr,
                   "[Physics] overflow=%s capacity=%u occurrences=%llu overflowSteps=%llu totalSteps=%llu "
                   "(bodies=%u bodyPairs=%u contacts=%u broadPhasePairs=%u).\n",
                   error.category, error.capacity, static_cast<unsigned long long>(*error.counter),
                   static_cast<unsigned long long>(world.stepStats.overflowSteps),
                   static_cast<unsigned long long>(world.stepStats.totalSteps), world.desc.maxBodies,
                   world.desc.maxBodyPairs, world.desc.maxContactConstraints,
                   world.desc.maxBroadPhasePairs);
      std::fflush(stderr);
    }
  }

  if (ShouldFailFast(world.desc.overflowPolicy)) {
    AE_CHECK(false, "overflow de capacidade no PhysicsSystem::Update; consulte o diagnóstico [Physics]");
  }
}

AetherBodyHandle CreateBodyInternal(AetherPhysicsWorld &world, const AetherBodyDescV2 &desc) {
  JPH::RefConst<JPH::Shape> shape = ToJoltShape(desc.shape);
  if (shape == nullptr) return AetherBodyHandle_Invalid;

  JPH::EAllowedDOFs allowedDOFs = ToJoltAllowedDOFs(desc.allowedDOFs);
  if (desc.motionType == AetherMotionType::Dynamic &&
      (static_cast<ae::u8>(allowedDOFs) & 0b111) == 0) {
    return AetherBodyHandle_Invalid;
  }

  JPH::BodyCreationSettings bodySettings(
      shape, JPH::RVec3(desc.position.x, desc.position.y, desc.position.z), ToJolt(desc.rotation),
      ToJoltMotionType(desc.motionType), ToObjectLayer(desc.motionType));
  bodySettings.mFriction = desc.friction;
  bodySettings.mRestitution = desc.restitution;
  bodySettings.mAllowedDOFs = allowedDOFs;
  bodySettings.mIsSensor = desc.isSensor != 0;
  // Sensores cinemáticos precisam enxergar volumes estáticos. O default do
  // Jolt pula kinematic-vs-non-dynamic porque dois corpos não dinâmicos não
  // precisam de solver; para triggers, porém, o contato é o próprio resultado.
  bodySettings.mCollideKinematicVsNonDynamic = bodySettings.mIsSensor;

  JPH::BodyInterface &bodyInterface = world.physicsSystem.GetBodyInterface();
  JPH::BodyID id = bodyInterface.CreateAndAddBody(
      bodySettings, desc.motionType == AetherMotionType::Static ? JPH::EActivation::DontActivate
                                                                : JPH::EActivation::Activate);
  if (id.IsInvalid()) return AetherBodyHandle_Invalid;

  const AetherBodyHandle handle = id.GetIndexAndSequenceNumber();
  if (bodySettings.mIsSensor) world.triggerListener.RegisterSensor(handle, desc.eventLayerMask);
  return handle;
}

} // namespace

extern "C" {

AetherPhysicsWorld *AetherPhysics_CreateWorld(AetherVec3 gravity, ae::u32 maxBodies) {
  const AetherPhysicsWorldDescV2 desc = MakeV1CompatibleDesc(gravity, maxBodies);
  return AetherPhysics_CreateWorldV2(&desc);
}

AetherPhysicsWorld *AetherPhysics_CreateWorldV2(const AetherPhysicsWorldDescV2 *desc) {
  if (!IsValidWorldDesc(desc)) return nullptr;
  EnsureGlobalTypesRegistered();
  auto *world = new AetherPhysicsWorld(*desc);
  world->physicsSystem.SetGravity(ToJolt(desc->gravity));
  return world;
}

void AetherPhysics_DestroyWorld(AetherPhysicsWorld *world) {
  delete world;
}

AetherBodyHandle AetherPhysics_CreateBody(AetherPhysicsWorld *world, const AetherBodyDesc *desc) {
  if (world == nullptr || desc == nullptr) return AetherBodyHandle_Invalid;
  AetherBodyDescV2 v2{};
  v2.structSize = sizeof(v2);
  v2.apiVersion = AetherBodyApiVersionV2;
  v2.shape = desc->shape;
  v2.position = desc->position;
  v2.rotation = desc->rotation;
  v2.motionType = desc->motionType;
  v2.friction = desc->friction;
  v2.restitution = desc->restitution;
  v2.allowedDOFs = desc->allowedDOFs;
  v2.isSensor = 0;
  v2.eventLayerMask = static_cast<ae::u32>(AetherQueryLayerMask::All);
  return CreateBodyInternal(*world, v2);
}

AetherBodyHandle AetherPhysics_CreateStaticTriangleMesh(
    AetherPhysicsWorld *world, const AetherVec3 *vertices, ae::u32 vertexCount,
    const ae::u32 *indices, ae::u32 indexCount, float friction) {
  if (world == nullptr || vertices == nullptr || indices == nullptr || vertexCount < 3 ||
      indexCount < 3 || indexCount % 3 != 0 || !std::isfinite(friction) ||
      friction < 0.0f || friction > 1.0f) return AetherBodyHandle_Invalid;

  JPH::VertexList meshVertices;
  meshVertices.reserve(vertexCount);
  for (ae::u32 index = 0; index < vertexCount; ++index) {
    const AetherVec3 &vertex = vertices[index];
    if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) ||
        !std::isfinite(vertex.z)) return AetherBodyHandle_Invalid;
    meshVertices.emplace_back(vertex.x,vertex.y,vertex.z);
  }
  JPH::IndexedTriangleList triangles;
  triangles.reserve(indexCount/3);
  for (ae::u32 index = 0; index < indexCount; index += 3) {
    if (indices[index] >= vertexCount || indices[index+1] >= vertexCount ||
        indices[index+2] >= vertexCount) return AetherBodyHandle_Invalid;
    triangles.emplace_back(indices[index],indices[index+1],indices[index+2],0);
  }
  JPH::MeshShapeSettings meshSettings(std::move(meshVertices),std::move(triangles));
  meshSettings.mBuildQuality = JPH::MeshShapeSettings::EBuildQuality::FavorRuntimePerformance;
  auto result = meshSettings.Create();
  if (result.HasError()) return AetherBodyHandle_Invalid;

  JPH::BodyCreationSettings bodySettings(result.Get(),JPH::RVec3::sZero(),
      JPH::Quat::sIdentity(),JPH::EMotionType::Static,Layers::NonMoving);
  bodySettings.mFriction = friction;
  JPH::BodyID id = world->physicsSystem.GetBodyInterface().CreateAndAddBody(
      bodySettings,JPH::EActivation::DontActivate);
  return id.IsInvalid() ? AetherBodyHandle_Invalid : id.GetIndexAndSequenceNumber();
}

AetherBodyHandle AetherPhysics_CreateBodyV2(AetherPhysicsWorld *world,
                                             const AetherBodyDescV2 *desc) {
  constexpr ae::u32 validMask = static_cast<ae::u32>(AetherQueryLayerMask::All);
  if (world == nullptr || desc == nullptr || desc->structSize < sizeof(AetherBodyDescV2) ||
      desc->apiVersion != AetherBodyApiVersionV2 || (desc->eventLayerMask & ~validMask) != 0) {
    return AetherBodyHandle_Invalid;
  }
  return CreateBodyInternal(*world, *desc);
}

ae::i32 AetherPhysics_CreateBodiesV2(AetherPhysicsWorld *world,
                                     const AetherBodyDescV2 *descs,
                                     AetherBodyHandle *outHandles,
                                     ae::i32 count) {
  if (count < 0 || world == nullptr || (count > 0 && (descs == nullptr || outHandles == nullptr)))
    return 0;
  if (count == 0) return 0;

  std::fill_n(outHandles, count, AetherBodyHandle_Invalid);
  constexpr ae::u32 validMask = static_cast<ae::u32>(AetherQueryLayerMask::All);
  JPH::BodyInterface &bodyInterface = world->physicsSystem.GetBodyInterface();
  std::vector<JPH::BodyID> ids;
  std::vector<JPH::BodyID> activateIds;
  ids.reserve(static_cast<ae::usize>(count));
  activateIds.reserve(static_cast<ae::usize>(count));

  auto rollback = [&] {
    if (!ids.empty()) bodyInterface.DestroyBodies(ids.data(), static_cast<int>(ids.size()));
    std::fill_n(outHandles, count, AetherBodyHandle_Invalid);
  };

  // Primeiro cria e atribui IDs, ainda fora da broadphase. Qualquer descriptor
  // inválido ou falta de capacidade desfaz o prefixo inteiro, garantindo a
  // semântica all-or-none documentada na ABI.
  for (ae::i32 i = 0; i < count; ++i) {
    const AetherBodyDescV2 &desc = descs[i];
    if (desc.structSize < sizeof(AetherBodyDescV2) ||
        desc.apiVersion != AetherBodyApiVersionV2 ||
        (desc.eventLayerMask & ~validMask) != 0) {
      rollback();
      return 0;
    }

    JPH::RefConst<JPH::Shape> shape = ToJoltShape(desc.shape);
    JPH::EAllowedDOFs allowedDOFs = ToJoltAllowedDOFs(desc.allowedDOFs);
    if (shape == nullptr ||
        (desc.motionType == AetherMotionType::Dynamic &&
         (static_cast<ae::u8>(allowedDOFs) & 0b111) == 0)) {
      rollback();
      return 0;
    }

    JPH::BodyCreationSettings settings(
        shape, JPH::RVec3(desc.position.x, desc.position.y, desc.position.z),
        ToJolt(desc.rotation), ToJoltMotionType(desc.motionType), ToObjectLayer(desc.motionType));
    settings.mFriction = desc.friction;
    settings.mRestitution = desc.restitution;
    settings.mAllowedDOFs = allowedDOFs;
    settings.mIsSensor = desc.isSensor != 0;
    settings.mCollideKinematicVsNonDynamic = settings.mIsSensor;

    JPH::Body *body = bodyInterface.CreateBody(settings);
    if (body == nullptr) {
      rollback();
      return 0;
    }
    const JPH::BodyID id = body->GetID();
    ids.push_back(id);
    if (desc.motionType != AetherMotionType::Static) activateIds.push_back(id);
    outHandles[i] = id.GetIndexAndSequenceNumber();
  }

  // Uma preparação/finalização para o lote evita degradar a árvore incremental
  // da broadphase como ocorreria com N chamadas CreateAndAddBody.
  JPH::BodyInterface::AddState addState =
      bodyInterface.AddBodiesPrepare(ids.data(), static_cast<int>(ids.size()));
  bodyInterface.AddBodiesFinalize(ids.data(), static_cast<int>(ids.size()), addState,
                                  JPH::EActivation::DontActivate);
  if (!activateIds.empty())
    bodyInterface.ActivateBodies(activateIds.data(), static_cast<int>(activateIds.size()));

  for (ae::i32 i = 0; i < count; ++i)
    if (descs[i].isSensor != 0)
      world->triggerListener.RegisterSensor(outHandles[i], descs[i].eventLayerMask);
  return count;
}

void AetherPhysics_DestroyBody(AetherPhysicsWorld *world, AetherBodyHandle handle) {
  AetherPhysics_DestroyBodies(world, &handle, 1);
}

void AetherPhysics_DestroyBodies(AetherPhysicsWorld *world,
                                 const AetherBodyHandle *handles,
                                 ae::i32 count) {
  if (world == nullptr || handles == nullptr || count <= 0) return;
  JPH::BodyInterface &bodyInterface = world->physicsSystem.GetBodyInterface();
  std::vector<JPH::BodyID> ids;
  ids.reserve(static_cast<ae::usize>(count));
  for (ae::i32 i = 0; i < count; ++i) {
    if (handles[i] == AetherBodyHandle_Invalid) continue;
    const JPH::BodyID id(handles[i]);
    if (bodyInterface.IsSensor(id)) world->triggerListener.UnregisterSensor(handles[i]);
    ids.push_back(id);
  }
  if (ids.empty()) return;
  bodyInterface.RemoveBodies(ids.data(), static_cast<int>(ids.size()));
  bodyInterface.DestroyBodies(ids.data(), static_cast<int>(ids.size()));
}

void AetherPhysics_Step(AetherPhysicsWorld *world, float deltaTime, ae::i32 collisionSteps) {
  (void)AetherPhysics_StepV2(world, deltaTime, collisionSteps);
}

ae::u32 AetherPhysics_StepV2(AetherPhysicsWorld *world, float deltaTime, ae::i32 collisionSteps) {
  if (world == nullptr) return static_cast<ae::u32>(AetherPhysicsUpdateError::None);
  world->triggerListener.KeepOverlappingBodiesAwake(world->physicsSystem.GetBodyInterface());
  world->triggerListener.BeginStep();
  const JPH::EPhysicsUpdateError updateError =
      world->physicsSystem.Update(deltaTime, collisionSteps, &world->tempAllocator, &world->jobSystem);
  world->triggerListener.EndStep();
  const ae::u32 flags = static_cast<ae::u32>(updateError);
  ++world->stepStats.totalSteps;
  world->stepStats.lastErrorFlags = flags;
  if (flags != 0) ReportUpdateErrors(*world, flags);
  return flags;
}

ae::i32 AetherPhysics_GetTriggerEvents(AetherPhysicsWorld *world,
                                       AetherTriggerEvent *outEvents,
                                       ae::i32 maxResults) {
  return world == nullptr ? 0 : world->triggerListener.CopyEvents(outEvents, maxResults);
}

ae::i32 AetherPhysics_GetStepStatsV2(const AetherPhysicsWorld *world,
                                     AetherPhysicsStepStatsV2 *outStats) {
  if (world == nullptr || outStats == nullptr ||
      outStats->structSize < sizeof(AetherPhysicsStepStatsV2) ||
      outStats->apiVersion != AetherPhysicsWorldApiVersionV2) {
    return 0;
  }
  *outStats = world->stepStats;
  return 1;
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

ae::i32 AetherPhysics_MoveKinematicV2(AetherPhysicsWorld *world, AetherBodyHandle handle,
                                      AetherVec3 targetPosition, AetherQuat targetRotation,
                                      float deltaTime) {
  if (world == nullptr || handle == AetherBodyHandle_Invalid || deltaTime <= 0.0f) return 0;
  const JPH::BodyID id(handle);
  JPH::BodyInterface &bodyInterface = world->physicsSystem.GetBodyInterface();
  if (bodyInterface.GetMotionType(id) != JPH::EMotionType::Kinematic) return 0;
  bodyInterface.MoveKinematic(id, JPH::RVec3(targetPosition.x, targetPosition.y, targetPosition.z),
                              ToJolt(targetRotation), deltaTime);
  return 1;
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

// ---------------------------------------------------------------- queries (4.1.4)

ae::i32 AetherPhysics_RayCastAll(AetherPhysicsWorld *world, AetherVec3 origin, AetherVec3 direction,
                                  AetherQueryLayerMask layerMask, AetherBodyHandle ignoreBody,
                                  AetherBodyHandle *outBodies, float *outFractions, ae::i32 maxResults) {
  if (world == nullptr) return 0;

  JPH::RRayCast ray(JPH::RVec3(origin.x, origin.y, origin.z), ToJolt(direction));
  JPH::AllHitCollisionCollector<JPH::CastRayCollector> collector;
  QueryLayerFilter layerFilter(layerMask);
  QueryBodyFilter bodyFilter(ignoreBody);
  world->physicsSystem.GetNarrowPhaseQuery().CastRay(ray, {}, collector, {}, layerFilter, bodyFilter);

  // AllHitCollisionCollector não ordena por padrão (ver CollisionCollectorImpl.h) — ordenar
  // por fração aqui é o que faz "todos os hits ao longo do raio" ter uma ordem previsível
  // (mais perto primeiro), em vez da ordem arbitrária de travessia da broadphase.
  collector.Sort();

  ae::i32 count = static_cast<ae::i32>(collector.mHits.size());
  ae::i32 toCopy = maxResults > 0 ? std::min(count, maxResults) : 0;
  for (ae::i32 i = 0; i < toCopy; ++i) {
    if (outBodies != nullptr) outBodies[i] = collector.mHits[i].mBodyID.GetIndexAndSequenceNumber();
    if (outFractions != nullptr) outFractions[i] = collector.mHits[i].mFraction;
  }
  return count;
}

ae::i32 AetherPhysics_ShapeCastClosest(AetherPhysicsWorld *world, const AetherShapeDesc *shape,
                                        AetherVec3 origin, AetherQuat rotation, AetherVec3 direction,
                                        AetherQueryLayerMask layerMask, AetherBodyHandle ignoreBody,
                                        AetherShapeQueryHit *outHit) {
  if (world == nullptr || shape == nullptr) return 0;
  JPH::RefConst<JPH::Shape> joltShape = ToJoltShape(*shape);
  if (joltShape == nullptr) return 0;

  JPH::RMat44 startTransform = JPH::RMat44::sRotationTranslation(ToJolt(rotation), JPH::RVec3(origin.x, origin.y, origin.z));
  JPH::RShapeCast cast = JPH::RShapeCast::sFromWorldTransform(joltShape, JPH::Vec3::sReplicate(1.0f), startTransform, ToJolt(direction));

  JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
  QueryLayerFilter layerFilter(layerMask);
  QueryBodyFilter bodyFilter(ignoreBody);
  world->physicsSystem.GetNarrowPhaseQuery().CastShape(cast, {}, JPH::RVec3::sZero(), collector, {}, layerFilter, bodyFilter);

  if (!collector.HadHit()) return 0;
  if (outHit != nullptr) *outHit = ToShapeQueryHit(collector.mHit, collector.mHit.mFraction);
  return 1;
}

ae::i32 AetherPhysics_OverlapShape(AetherPhysicsWorld *world, const AetherShapeDesc *shape,
                                    AetherVec3 origin, AetherQuat rotation,
                                    AetherQueryLayerMask layerMask, AetherBodyHandle ignoreBody,
                                    AetherShapeQueryHit *outHits, ae::i32 maxResults) {
  if (world == nullptr || shape == nullptr) return 0;
  JPH::RefConst<JPH::Shape> joltShape = ToJoltShape(*shape);
  if (joltShape == nullptr) return 0;

  JPH::RMat44 transform = JPH::RMat44::sRotationTranslation(ToJolt(rotation), JPH::RVec3(origin.x, origin.y, origin.z));
  JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
  QueryLayerFilter layerFilter(layerMask);
  QueryBodyFilter bodyFilter(ignoreBody);
  world->physicsSystem.GetNarrowPhaseQuery().CollideShape(joltShape, JPH::Vec3::sReplicate(1.0f), transform, {},
                                                            JPH::RVec3::sZero(), collector, {}, layerFilter, bodyFilter);

  ae::i32 count = static_cast<ae::i32>(collector.mHits.size());
  ae::i32 toCopy = maxResults > 0 ? std::min(count, maxResults) : 0;
  for (ae::i32 i = 0; i < toCopy; ++i) {
    if (outHits != nullptr) outHits[i] = ToShapeQueryHit(collector.mHits[i], 0.0f);
  }
  return count;
}

// ---------------------------------------------------------------- juntas e motores (4.1.3)

namespace {

constexpr float kJointPi = 3.14159265358979323846f;

bool IsFinite(AetherVec3 value) {
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool IsValidMotor(const AetherJointMotorDesc &motor) {
  const ae::u32 state = static_cast<ae::u32>(motor.state);
  return state <= static_cast<ae::u32>(AetherMotorState::PositionAndVelocity) &&
         std::isfinite(motor.targetVelocity) && std::isfinite(motor.targetPosition) &&
         std::isfinite(motor.maxForceOrTorque) && motor.maxForceOrTorque >= 0.0f &&
         std::isfinite(motor.springFrequency) && motor.springFrequency >= 0.0f &&
         std::isfinite(motor.springDamping) && motor.springDamping >= 0.0f;
}

bool IsValidJointDesc(const AetherJointDesc &desc) {
  const ae::u32 kind = static_cast<ae::u32>(desc.kind);
  if (kind > static_cast<ae::u32>(AetherJointKind::Distance) ||
      !IsFinite(desc.point1) || !IsFinite(desc.point2) || !IsFinite(desc.axis1) ||
      !IsFinite(desc.axis2) || !std::isfinite(desc.limitsMin) ||
      !std::isfinite(desc.limitsMax) || !IsValidMotor(desc.motor))
    return false;

  if (desc.kind == AetherJointKind::Hinge || desc.kind == AetherJointKind::Slider) {
    if (ToJolt(desc.axis1).LengthSq() <= 1.0e-12f ||
        ToJolt(desc.axis2).LengthSq() <= 1.0e-12f)
      return false;
    if (desc.limitsMin > desc.limitsMax) return false;
  }

  if (desc.kind == AetherJointKind::Hinge &&
      (desc.limitsMin < -kJointPi || desc.limitsMin > 0.0f ||
       desc.limitsMax < 0.0f || desc.limitsMax > kJointPi))
    return false;

  return true;
}

void TransformJointFrame(const JPH::Body &reference, AetherJointDesc &desc) {
  const JPH::Quat rotation = reference.GetRotation();
  const JPH::RVec3 position = reference.GetPosition();
  desc.point1 = FromJolt(position + rotation * ToJolt(desc.point1));
  desc.point2 = FromJolt(position + rotation * ToJolt(desc.point2));
  desc.axis1 = FromJolt(rotation * ToJolt(desc.axis1));
  desc.axis2 = FromJolt(rotation * ToJolt(desc.axis2));
}

AetherJointHandle CreateJointWorldSpace(AetherPhysicsWorld *world, AetherBodyHandle body1,
                                         AetherBodyHandle body2, const AetherJointDesc *desc,
                                         AetherJointSpace inputSpace) {
  if (world == nullptr || desc == nullptr) return AetherJointHandle_Invalid;
  if (body1 == AetherBodyHandle_Invalid || body2 == AetherBodyHandle_Invalid) return AetherJointHandle_Invalid;
  if (!IsValidJointDesc(*desc)) return AetherJointHandle_Invalid;

  // Constraint::Create(Body&, Body&) precisa de referências reais, não de BodyID — diferente
  // do resto desta fronteira (que opera inteiramente via BodyInterface, por BodyID). Um lock
  // de escrita é o padrão idiomático do próprio Jolt para isso (ver BodyLockWrite em
  // Jolt/Physics/Body/BodyLock.h); write, não read, porque adicionar uma constraint deixa o
  // corpo referenciado por ela (mConstraints, contabilidade interna do Jolt).
  //
  // BodyLockMultiWrite (não dois BodyLockWrite sequenciais): travar dois BodyLockWrite um
  // após o outro dispara o assert de ordem de PhysicsLock.h ("A lock of same or higher
  // priority was already taken") — o segundo lock pega a MESMA categoria PerBody que o
  // primeiro já detém, o que o Jolt trata como possível deadlock e recusa (pego só ao
  // rodar o teste real, não por inspeção — ver docs/ESTADO.md). BodyLockMultiWrite existe
  // exatamente para travar N corpos de uma vez, resolvendo a ordem de mutex internamente.
  JPH::BodyLockInterfaceLocking const &lockInterface = world->physicsSystem.GetBodyLockInterface();
  JPH::BodyID bodyIds[2] = {JPH::BodyID(body1), JPH::BodyID(body2)};
  JPH::Ref<JPH::Constraint> constraint;
  {
    // Escopo próprio: BodyLockMultiWrite precisa liberar os dois locks ANTES de chamarmos
    // ActivateBody abaixo — ActivateBody pega o lock PerBody internamente, e um segundo lock
    // da mesma categoria enquanto BodyLockMultiWrite ainda está vivo dispara o mesmo assert
    // de possível deadlock que a criação da constraint já pegou (ver comentário abaixo sobre
    // BodyLockMultiWrite) — aqui é o MESMO tipo de bug, só que entre o lock de criação da
    // constraint e o ActivateBody que vem depois, não entre dois locks de corpo individuais.
    JPH::BodyLockMultiWrite lock(lockInterface, bodyIds, 2);
    JPH::Body *jphBody1 = lock.GetBody(0);
    JPH::Body *jphBody2 = lock.GetBody(1);
    if (jphBody1 == nullptr || jphBody2 == nullptr) return AetherJointHandle_Invalid;

    AetherJointDesc worldDesc = *desc;
    if (inputSpace == AetherJointSpace::LocalToBody1)
      TransformJointFrame(*jphBody1, worldDesc);
    else if (inputSpace == AetherJointSpace::LocalToBody2)
      TransformJointFrame(*jphBody2, worldDesc);

    const AetherJointDesc *resolved = &worldDesc;

    switch (resolved->kind) {
      case AetherJointKind::Point: {
        JPH::PointConstraintSettings settings;
        settings.mSpace = JPH::EConstraintSpace::WorldSpace;
        settings.mPoint1 = JPH::RVec3(resolved->point1.x, resolved->point1.y, resolved->point1.z);
        settings.mPoint2 = JPH::RVec3(resolved->point2.x, resolved->point2.y, resolved->point2.z);
        constraint = settings.Create(*jphBody1, *jphBody2);
        break;
      }
      case AetherJointKind::Hinge: {
        JPH::HingeConstraintSettings settings;
        settings.mSpace = JPH::EConstraintSpace::WorldSpace;
        settings.mPoint1 = JPH::RVec3(resolved->point1.x, resolved->point1.y, resolved->point1.z);
        settings.mPoint2 = JPH::RVec3(resolved->point2.x, resolved->point2.y, resolved->point2.z);
        settings.mHingeAxis1 = ToJolt(resolved->axis1).Normalized();
        settings.mHingeAxis2 = ToJolt(resolved->axis2).Normalized();
        // Normal perpendicular ao eixo, gerada automaticamente — a fronteira não pede uma
        // normal explícita do chamador (só usada por Jolt para desenhar/definir ângulo zero;
        // GetNormalizedPerpendicular() é o mesmo helper que o próprio Jolt usa internamente
        // quando o eixo muda sem normal fornecida).
        settings.mNormalAxis1 = settings.mHingeAxis1.GetNormalizedPerpendicular();
        settings.mNormalAxis2 = settings.mHingeAxis2.GetNormalizedPerpendicular();
        settings.mLimitsMin = resolved->limitsMin;
        settings.mLimitsMax = resolved->limitsMax;
        settings.mMotorSettings = ToJoltMotorSettings(resolved->motor, /*isAngular*/ true);
        auto *hinge = static_cast<JPH::HingeConstraint *>(settings.Create(*jphBody1, *jphBody2));
        hinge->SetMotorState(ToJoltMotorState(resolved->motor.state));
        hinge->SetTargetAngularVelocity(resolved->motor.targetVelocity);
        hinge->SetTargetAngle(resolved->motor.targetPosition);
        constraint = hinge;
        break;
      }
      case AetherJointKind::Slider: {
        JPH::SliderConstraintSettings settings;
        settings.mSpace = JPH::EConstraintSpace::WorldSpace;
        settings.mPoint1 = JPH::RVec3(resolved->point1.x, resolved->point1.y, resolved->point1.z);
        settings.mPoint2 = JPH::RVec3(resolved->point2.x, resolved->point2.y, resolved->point2.z);
        settings.mSliderAxis1 = ToJolt(resolved->axis1).Normalized();
        settings.mSliderAxis2 = ToJolt(resolved->axis2).Normalized();
        settings.mNormalAxis1 = settings.mSliderAxis1.GetNormalizedPerpendicular();
        settings.mNormalAxis2 = settings.mSliderAxis2.GetNormalizedPerpendicular();
        settings.mLimitsMin = resolved->limitsMin;
        settings.mLimitsMax = resolved->limitsMax;
        settings.mMotorSettings = ToJoltMotorSettings(resolved->motor, /*isAngular*/ false);
        auto *slider = static_cast<JPH::SliderConstraint *>(settings.Create(*jphBody1, *jphBody2));
        slider->SetMotorState(ToJoltMotorState(resolved->motor.state));
        slider->SetTargetVelocity(resolved->motor.targetVelocity);
        slider->SetTargetPosition(resolved->motor.targetPosition);
        constraint = slider;
        break;
      }
      case AetherJointKind::Distance: {
        JPH::DistanceConstraintSettings settings;
        settings.mSpace = JPH::EConstraintSpace::WorldSpace;
        settings.mPoint1 = JPH::RVec3(resolved->point1.x, resolved->point1.y, resolved->point1.z);
        settings.mPoint2 = JPH::RVec3(resolved->point2.x, resolved->point2.y, resolved->point2.z);
        settings.mMinDistance = resolved->limitsMin;
        settings.mMaxDistance = resolved->limitsMax;
        constraint = settings.Create(*jphBody1, *jphBody2);
        break;
      }
      default:
        return AetherJointHandle_Invalid;
    }
  } // fim do escopo de `lock` — libera os dois BodyLockWrite antes de ActivateBody abaixo

  if (constraint == nullptr) return AetherJointHandle_Invalid;
  world->physicsSystem.AddConstraint(constraint);

  // Criar uma junta sobre um corpo dormindo (ex.: preso por outra junta, já assentado) não
  // acorda os corpos sozinho — o Jolt só recalcula o solver de contato/constraint para
  // corpos ativos. Sem isso a nova junta fica "presa" num corpo adormecido e só passa a
  // fazer efeito quando algo mais o acordar por acidente.
  world->physicsSystem.GetBodyInterface().ActivateBody(bodyIds[0]);
  world->physicsSystem.GetBodyInterface().ActivateBody(bodyIds[1]);

  if (!world->freeJointSlots.empty()) {
    ae::u32 index = world->freeJointSlots.back();
    world->freeJointSlots.pop_back();
    world->jointSlots[index] = JointSlot{constraint, bodyIds[0], bodyIds[1], world->jointSlots[index].generation};
    return PackGenerationalHandle(index, world->jointSlots[index].generation);
  }
  ae::u32 index = static_cast<ae::u32>(world->jointSlots.size());
  world->jointSlots.push_back(JointSlot{constraint, bodyIds[0], bodyIds[1], 0});
  return PackGenerationalHandle(index, 0);
}

} // namespace

AetherJointHandle AetherPhysics_CreateJoint(AetherPhysicsWorld *world, AetherBodyHandle body1,
                                             AetherBodyHandle body2, const AetherJointDesc *desc) {
  return CreateJointWorldSpace(world, body1, body2, desc, AetherJointSpace::World);
}

AetherJointHandle AetherPhysics_CreateJointV2(AetherPhysicsWorld *world, AetherBodyHandle body1,
                                               AetherBodyHandle body2, const AetherJointDescV2 *desc) {
  if (desc == nullptr || desc->structSize < sizeof(AetherJointDescV2) ||
      desc->apiVersion != AetherJointApiVersionV2)
    return AetherJointHandle_Invalid;
  if (desc->space != AetherJointSpace::World && desc->space != AetherJointSpace::LocalToBody1 &&
      desc->space != AetherJointSpace::LocalToBody2)
    return AetherJointHandle_Invalid;

  AetherJointDesc v1{};
  v1.kind = desc->kind;
  v1.point1 = desc->point1;
  v1.point2 = desc->point2;
  v1.axis1 = desc->axis1;
  v1.axis2 = desc->axis2;
  v1.limitsMin = desc->limitsMin;
  v1.limitsMax = desc->limitsMax;
  v1.motor = desc->motor;
  return CreateJointWorldSpace(world, body1, body2, &v1, desc->space);
}

namespace {
// Resolve um AetherJointHandle para o slot correspondente, ou nullptr se o handle é
// inválido, de outro mundo, ou aponta para uma junta já destruída (geração não bate — mesma
// checagem que World.Exists faz em EntitySlot.Version do lado C#).
JointSlot *ResolveJointSlot(AetherPhysicsWorld *world, AetherJointHandle handle) {
  if (world == nullptr || handle == AetherJointHandle_Invalid) return nullptr;
  ae::u32 index = GenerationalHandleIndex(handle);
  if (index >= world->jointSlots.size()) return nullptr;
  JointSlot &slot = world->jointSlots[index];
  if (slot.constraint == nullptr || slot.generation != GenerationalHandleGeneration(handle)) return nullptr;
  return &slot;
}
} // namespace

void AetherPhysics_DestroyJoint(AetherPhysicsWorld *world, AetherJointHandle handle) {
  JointSlot *slot = ResolveJointSlot(world, handle);
  if (slot == nullptr) return;
  world->physicsSystem.RemoveConstraint(slot->constraint);
  // Espelha o ActivateBody de AetherPhysics_CreateJoint: um corpo que a junta mantinha em
  // equilíbrio (ex.: pêndulo parado, preso e adormecido) não tem motivo físico para
  // continuar dormindo depois que a força que o segurava desaparece — sem isso ele fica
  // "congelado" no ar até algo mais acordá-lo por acidente (pego só ao rodar o teste real:
  // corpo preso e destravado não voltava a cair — ver docs/ESTADO.md).
  JPH::BodyInterface &bodyInterface = world->physicsSystem.GetBodyInterface();
  bodyInterface.ActivateBody(slot->body1);
  bodyInterface.ActivateBody(slot->body2);
  slot->constraint = nullptr;
  slot->generation = (slot->generation + 1) & kGenerationalHandleMask;
  world->freeJointSlots.push_back(GenerationalHandleIndex(handle));
}

void AetherPhysics_SetJointMotor(AetherPhysicsWorld *world, AetherJointHandle handle, const AetherJointMotorDesc *motor) {
  JointSlot *slot = ResolveJointSlot(world, handle);
  if (slot == nullptr || motor == nullptr) return;

  JPH::EConstraintSubType subType = slot->constraint->GetSubType();
  if (subType == JPH::EConstraintSubType::Hinge) {
    auto *hinge = static_cast<JPH::HingeConstraint *>(slot->constraint.GetPtr());
    hinge->GetMotorSettings() = ToJoltMotorSettings(*motor, /*isAngular*/ true);
    hinge->SetMotorState(ToJoltMotorState(motor->state));
    hinge->SetTargetAngularVelocity(motor->targetVelocity);
    hinge->SetTargetAngle(motor->targetPosition);
  } else if (subType == JPH::EConstraintSubType::Slider) {
    auto *slider = static_cast<JPH::SliderConstraint *>(slot->constraint.GetPtr());
    slider->GetMotorSettings() = ToJoltMotorSettings(*motor, /*isAngular*/ false);
    slider->SetMotorState(ToJoltMotorState(motor->state));
    slider->SetTargetVelocity(motor->targetVelocity);
    slider->SetTargetPosition(motor->targetPosition);
  }
  // Point/Distance: sem motor no Jolt, chamada é um no-op silencioso (ver comentário em
  // jolt_bridge.h sobre AetherMotorState) — não é erro do chamador, é a API real do Jolt.
}

float AetherPhysics_GetJointPosition(AetherPhysicsWorld *world, AetherJointHandle handle) {
  JointSlot *slot = ResolveJointSlot(world, handle);
  if (slot == nullptr) return 0.0f;

  JPH::EConstraintSubType subType = slot->constraint->GetSubType();
  if (subType == JPH::EConstraintSubType::Hinge)
    return static_cast<JPH::HingeConstraint *>(slot->constraint.GetPtr())->GetCurrentAngle();
  if (subType == JPH::EConstraintSubType::Slider)
    return static_cast<JPH::SliderConstraint *>(slot->constraint.GetPtr())->GetCurrentPosition();
  return 0.0f;
}

// ---------------------------------------------------------------- character controller (4.1.5)

namespace {
CharacterSlot *ResolveCharacterSlot(AetherPhysicsWorld *world, AetherCharacterHandle handle) {
  if (world == nullptr || handle == AetherCharacterHandle_Invalid) return nullptr;
  ae::u32 index = GenerationalHandleIndex(handle);
  if (index >= world->characterSlots.size()) return nullptr;
  CharacterSlot &slot = world->characterSlots[index];
  if (slot.character == nullptr || slot.generation != GenerationalHandleGeneration(handle)) return nullptr;
  return &slot;
}
} // namespace

AetherCharacterHandle AetherPhysics_CreateCharacter(AetherPhysicsWorld *world, const AetherCharacterDesc *desc,
                                                      AetherVec3 position, AetherQuat rotation) {
  if (world == nullptr || desc == nullptr) return AetherCharacterHandle_Invalid;

  // Base da cápsula em (0,0,0), mesma exigência documentada em CharacterBaseSettings::mShape
  // ("make sure the shape is made so that the bottom of the shape is at (0, 0, 0)") — por isso
  // o offset RotatedTranslatedShape empurra o CENTRO da cápsula para cima em
  // (halfHeight+radius), não a origem crua da CapsuleShapeSettings (que já é centrada na
  // origem por padrão do Jolt).
  auto makeCapsuleAtOrigin = [](float radius, float halfHeight) -> JPH::RefConst<JPH::Shape> {
    JPH::CapsuleShapeSettings capsule(halfHeight, radius);
    auto capsuleResult = capsule.Create();
    if (capsuleResult.HasError()) return nullptr;
    JPH::RotatedTranslatedShapeSettings offset(JPH::Vec3(0, halfHeight + radius, 0), JPH::Quat::sIdentity(), capsuleResult.Get());
    auto offsetResult = offset.Create();
    return offsetResult.HasError() ? nullptr : offsetResult.Get();
  };

  JPH::RefConst<JPH::Shape> standingShape = makeCapsuleAtOrigin(desc->radius, desc->standingHalfHeight);
  if (standingShape == nullptr) return AetherCharacterHandle_Invalid;
  JPH::RefConst<JPH::Shape> crouchingShape = makeCapsuleAtOrigin(desc->radius, desc->crouchingHalfHeight);
  if (crouchingShape == nullptr) return AetherCharacterHandle_Invalid;

  JPH::CharacterVirtualSettings settings;
  settings.mShape = standingShape;
  settings.mMaxSlopeAngle = desc->maxSlopeAngle;
  settings.mMass = desc->mass;
  settings.mMaxStrength = desc->maxStrength;
  // mUp/mSupportingVolume ficam nos defaults do Jolt (Y-up, aceita qualquer contato) — a
  // engine usa Y-para-cima em toda a matemática (ver CONVENCOES.md), consistente com o
  // resto desta fronteira (Layers, gravidade em -Y nos testes, etc).

  JPH::Ref<JPH::CharacterVirtual> character = new JPH::CharacterVirtual(
      &settings, JPH::RVec3(position.x, position.y, position.z), ToJolt(rotation), &world->physicsSystem);

  if (!world->freeCharacterSlots.empty()) {
    ae::u32 index = world->freeCharacterSlots.back();
    world->freeCharacterSlots.pop_back();
    world->characterSlots[index] = CharacterSlot{character, standingShape, crouchingShape, false, world->characterSlots[index].generation};
    return PackGenerationalHandle(index, world->characterSlots[index].generation);
  }
  ae::u32 index = static_cast<ae::u32>(world->characterSlots.size());
  world->characterSlots.push_back(CharacterSlot{character, standingShape, crouchingShape, false, 0});
  return PackGenerationalHandle(index, 0);
}

void AetherPhysics_DestroyCharacter(AetherPhysicsWorld *world, AetherCharacterHandle handle) {
  CharacterSlot *slot = ResolveCharacterSlot(world, handle);
  if (slot == nullptr) return;
  slot->character = nullptr;
  slot->standingShape = nullptr;
  slot->crouchingShape = nullptr;
  slot->generation = (slot->generation + 1) & kGenerationalHandleMask;
  world->freeCharacterSlots.push_back(GenerationalHandleIndex(handle));
}

void AetherPhysics_SetCharacterVelocity(AetherPhysicsWorld *world, AetherCharacterHandle handle, AetherVec3 velocity) {
  CharacterSlot *slot = ResolveCharacterSlot(world, handle);
  if (slot == nullptr) return;
  slot->character->SetLinearVelocity(ToJolt(velocity));
}

AetherVec3 AetherPhysics_GetCharacterVelocity(AetherPhysicsWorld *world, AetherCharacterHandle handle) {
  CharacterSlot *slot = ResolveCharacterSlot(world, handle);
  if (slot == nullptr) return {0.0f, 0.0f, 0.0f};
  return FromJolt(slot->character->GetLinearVelocity());
}

void AetherPhysics_UpdateCharacter(AetherPhysicsWorld *world, AetherCharacterHandle handle, float deltaTime,
                                    AetherVec3 gravity, AetherQueryLayerMask layerMask, AetherBodyHandle ignoreBody) {
  CharacterSlot *slot = ResolveCharacterSlot(world, handle);
  if (slot == nullptr) return;

  // Defaults do próprio Jolt para ExtendedUpdateSettings (ver CharacterVirtual.h) — 40cm de
  // step-up para degraus, 50cm de stick-to-floor. Não expostos como parâmetro nesta fatia: são
  // valores razoáveis para um personagem humano padrão: expor tudo criaria uma assinatura de
  // função gigante para um caso de uso ainda hipotético (personagem não-humano com proporções
  // muito diferentes) — ajustável depois se for preciso, sem quebrar ABI (adicionar um segundo
  // AetherPhysics_UpdateCharacterEx com settings explícitos).
  JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;

  QueryLayerFilter layerFilter(layerMask);
  QueryBodyFilter bodyFilter(ignoreBody);
  slot->character->ExtendedUpdate(deltaTime, ToJolt(gravity), updateSettings,
                                   world->physicsSystem.GetDefaultBroadPhaseLayerFilter(Layers::Moving),
                                   layerFilter, bodyFilter, {}, world->tempAllocator);
}

void AetherPhysics_GetCharacterTransform(AetherPhysicsWorld *world, AetherCharacterHandle handle,
                                          AetherVec3 *outPosition, AetherQuat *outRotation) {
  CharacterSlot *slot = ResolveCharacterSlot(world, handle);
  if (slot == nullptr) return;
  if (outPosition != nullptr) {
    JPH::RVec3 pos = slot->character->GetPosition();
    *outPosition = {pos.GetX(), pos.GetY(), pos.GetZ()};
  }
  if (outRotation != nullptr) *outRotation = FromJolt(slot->character->GetRotation());
}

AetherCharacterGroundState AetherPhysics_GetCharacterGroundState(AetherPhysicsWorld *world, AetherCharacterHandle handle) {
  CharacterSlot *slot = ResolveCharacterSlot(world, handle);
  if (slot == nullptr) return AetherCharacterGroundState::InAir;
  return FromJoltGroundState(slot->character->GetGroundState());
}

AetherVec3 AetherPhysics_GetCharacterGroundVelocity(AetherPhysicsWorld *world, AetherCharacterHandle handle) {
  CharacterSlot *slot = ResolveCharacterSlot(world, handle);
  if (slot == nullptr) return {0.0f, 0.0f, 0.0f};
  return FromJolt(slot->character->GetGroundVelocity());
}

AetherVec3 AetherPhysics_GetCharacterGroundNormal(AetherPhysicsWorld *world, AetherCharacterHandle handle) {
  CharacterSlot *slot = ResolveCharacterSlot(world, handle);
  if (slot == nullptr) return {0.0f, 0.0f, 0.0f};
  return FromJolt(slot->character->GetGroundNormal());
}

ae::i32 AetherPhysics_SetCharacterCrouching(AetherPhysicsWorld *world, AetherCharacterHandle handle, ae::i32 crouching) {
  CharacterSlot *slot = ResolveCharacterSlot(world, handle);
  if (slot == nullptr) return 0;

  bool wantCrouching = crouching != 0;
  if (wantCrouching == slot->isCrouching) return 1; // já está no estado pedido — no-op bem-sucedido

  JPH::RefConst<JPH::Shape> targetShape = wantCrouching ? slot->crouchingShape : slot->standingShape;
  // maxPenetrationDepth pequeno (não 0, não FLT_MAX): 0 recusaria mesmo o padding normal do
  // personagem (mCharacterPadding = 2cm, ver CharacterVirtualSettings), FLT_MAX aceitaria
  // qualquer penetração e nunca recusaria ficar de pé debaixo de algo baixo — o comportamento
  // que agachar/levantar deveria ter. 5cm é generoso o bastante para o padding normal sem
  // deixar o personagem atravessar objetos de verdade.
  bool success = slot->character->SetShape(targetShape, 0.05f,
                                            world->physicsSystem.GetDefaultBroadPhaseLayerFilter(Layers::Moving),
                                            world->physicsSystem.GetDefaultLayerFilter(Layers::Moving), {}, {},
                                            world->tempAllocator);
  if (success) slot->isCrouching = wantCrouching;
  return success ? 1 : 0;
}

} // extern "C"

ae::i32 AetherPhysics_ApplyWaterForces(AetherPhysicsWorld *world,
                                       const AetherWaterBodySample *samples, ae::i32 count,
                                       const ae::physics::BuoyancySettings *settings,
                                       AetherWaterForceStats *outStats) {
  if (outStats != nullptr) *outStats = {};
  if (world == nullptr || settings == nullptr) return -1;
  if (count < 0 || (count > 0 && samples == nullptr)) return -1;
  if (!ae::physics::validateBuoyancySettings(*settings)) return -1;

  const JPH::BodyLockInterfaceLocking &lockInterface = world->physicsSystem.GetBodyLockInterface();
  const JPH::Vec3 gravity = world->physicsSystem.GetGravity();
  ae::i32 applied = 0;
  AetherWaterForceStats stats{};
  for (ae::i32 index = 0; index < count; ++index) {
    const AetherWaterBodySample &sample = samples[index];
    if (sample.body == AetherBodyHandle_Invalid) continue;
    // Um lock por corpo cobre posição, rotação, massa, velocidades e a própria
    // aplicação da força. Passar pela BodyInterface faria seis locks separados
    // para o mesmo corpo no mesmo passo.
    JPH::BodyLockWrite lock(lockInterface, JPH::BodyID(sample.body));
    if (!lock.Succeeded()) continue;
    JPH::Body &body = lock.GetBody();
    if (!body.IsDynamic()) continue;
    ++stats.bodiesConsidered;
    // Corpo dormindo permanece dormindo: acordar a frota inteira a cada onda
    // drena bateria e é o modo de falha clássico de água em jogo. Quem deve
    // acordar um corpo é contato, entrada ou uma política explícita de despertar.
    if (!body.IsActive()) continue;

    ae::physics::BuoyantShape shape{};
    shape.kind = sample.shapeKind == 0 ? ae::physics::BuoyantShapeKind::Sphere
                                       : ae::physics::BuoyantShapeKind::Box;
    shape.halfExtent = sample.halfExtent;
    const JPH::RVec3 position = body.GetPosition();
    const JPH::Quat rotation = body.GetRotation();
    const ae::physics::WaterPlane plane{sample.planeNormal, sample.planeOffset};
    const ae::physics::SubmergedVolume submerged = ae::physics::submergedVolume(
        shape,
        {static_cast<float>(position.GetX()), static_cast<float>(position.GetY()),
         static_cast<float>(position.GetZ())},
        {rotation.GetX(), rotation.GetY(), rotation.GetZ(), rotation.GetW()}, plane);
    if (submerged.volume <= 0.0f) continue;
    ++stats.bodiesSubmerged;
    stats.submergedVolume += submerged.volume;

    // A velocidade lida é a do centro de carena, não a do centro de massa: um
    // casco jogando tem água passando pelo lado molhado mesmo com o centro
    // parado, e é essa diferença que amortece o balanço.
    const JPH::RVec3 centre(submerged.centroid.x, submerged.centroid.y, submerged.centroid.z);
    const JPH::MotionProperties *motion = body.GetMotionProperties();
    const float inverseMass = motion != nullptr ? motion->GetInverseMass() : 0.0f;
    if (!(inverseMass > 0.0f)) continue;
    const float mass = 1.0f / inverseMass;
    const JPH::Vec3 pointVelocity = body.GetPointVelocity(centre);
    const JPH::Vec3 angular = body.GetAngularVelocity();

    ae::physics::BuoyancyInput input{};
    input.submerged = submerged;
    input.mass = mass;
    input.bodyVelocity = {pointVelocity.GetX(), pointVelocity.GetY(), pointVelocity.GetZ()};
    input.waterVelocity = sample.waterVelocity;
    input.angularVelocity = {angular.GetX(), angular.GetY(), angular.GetZ()};
    input.referenceArea = sample.referenceArea;
    const ae::physics::BuoyancyForces forces = ae::physics::evaluateBuoyancy(input, *settings);
    if (forces.clamped) ++stats.bodiesClamped;

    // Massa adicionada não é expressável como força, então entra na razão pela
    // qual o solver vai dividir. O Jolt integra (F + m*g)/m; o resultado
    // desejado é (F + m*g)/m_efetiva, e o termo de gravidade abaixo é o que faz
    // os dois coincidirem em vez de deixar a massa adicionada fora da queda.
    const float ratio = mass / JPH::max(forces.effectiveMass, 1.0e-6f);
    const JPH::Vec3 force(forces.force.x, forces.force.y, forces.force.z);
    body.AddForce(force * ratio + gravity * (mass * (ratio - 1.0f)), centre);
    if (forces.torque.x != 0.0f || forces.torque.y != 0.0f || forces.torque.z != 0.0f)
      body.AddTorque(JPH::Vec3(forces.torque.x, forces.torque.y, forces.torque.z));
    ++applied;
  }
  if (outStats != nullptr) *outStats = stats;
  return applied;
}
