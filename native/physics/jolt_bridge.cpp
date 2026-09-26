// Implementação de jolt_bridge.h. Ver o comentário de topo do header para o
// escopo desta fatia vertical (item 4.1.1 do plano) e o que fica para depois.
#include "physics/jolt_bridge.h"
#include "physics/water_buoyancy.h"

// O próprio Jolt pede para Jolt.h vir antes de qualquer outro header do Jolt.
#include <Jolt/Jolt.h>
#include "physics/collision_cooking_internal.h"
#include "physics/jolt_init.h"

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
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
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
// A ObjectLayer do Jolt passa a carregar DUAS informacoes: a camada de gameplay
// escolhida pelo projeto (0..31) e a classe de movimento (estatico ou movel).
// Codificar as duas no mesmo valor e o que permite a matriz de interacao do
// projeto valer no SOLVER, e nao apenas no filtro de consulta -- um par que a
// matriz proibe nunca gera contato, em vez de gerar e ser descartado depois.
constexpr JPH::uint GameplayLayerCount = 32;
constexpr JPH::ObjectLayer NonMoving = 0;  // camada de gameplay 0, estatico
constexpr JPH::ObjectLayer Moving = 1;     // camada de gameplay 0, movel
constexpr JPH::uint NumLayers = GameplayLayerCount * 2;
constexpr bool IsMoving(JPH::ObjectLayer layer) { return (layer & 1) != 0; }
constexpr ae::u32 Gameplay(JPH::ObjectLayer layer) { return static_cast<ae::u32>(layer >> 1); }
constexpr JPH::ObjectLayer Encode(ae::u32 gameplayLayer, bool moving) {
  return static_cast<JPH::ObjectLayer>((gameplayLayer % GameplayLayerCount) * 2 + (moving ? 1 : 0));
}
} // namespace Layers

namespace BroadPhaseLayers {
constexpr JPH::BroadPhaseLayer NonMoving(0);
constexpr JPH::BroadPhaseLayer Moving(1);
constexpr JPH::uint NumLayers = 2;
} // namespace BroadPhaseLayers

class BroadPhaseLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
  BroadPhaseLayerInterfaceImpl() {
    for (JPH::uint layer = 0; layer < Layers::NumLayers; ++layer)
      mObjectToBroadPhase[layer] = Layers::IsMoving(static_cast<JPH::ObjectLayer>(layer))
                                       ? BroadPhaseLayers::Moving
                                       : BroadPhaseLayers::NonMoving;
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
    // Estatico contra estatico nunca precisa de par: dois corpos parados nao
    // podem comecar a se tocar sem que um deles se mova.
    if (!Layers::IsMoving(inLayer1)) return inLayer2 == BroadPhaseLayers::Moving;
    return true;
  }
};

// A matriz de interacao pertence ao MUNDO, nao ao filtro: o filtro so a le. Um
// bit ligado em [a][b] significa "objetos da camada a colidem com os da camada
// b"; a reciprocidade e conferida por quem escreve a matriz.
class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter {
public:
  void Bind(const ae::u32 *matrix) { mMatrix = matrix; }
  bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override {
    if (!Layers::IsMoving(inObject1) && !Layers::IsMoving(inObject2)) return false;
    if (mMatrix == nullptr) return true;
    const ae::u32 first = Layers::Gameplay(inObject1);
    const ae::u32 second = Layers::Gameplay(inObject2);
    if (first >= Layers::GameplayLayerCount || second >= Layers::GameplayLayerCount) return true;
    return (mMatrix[first] & (1u << second)) != 0;
  }

private:
  const ae::u32 *mMatrix = nullptr;
};

JPH::EMotionType ToJoltMotionType(AetherMotionType type) {
  switch (type) {
    case AetherMotionType::Static: return JPH::EMotionType::Static;
    case AetherMotionType::Kinematic: return JPH::EMotionType::Kinematic;
    default: return JPH::EMotionType::Dynamic;
  }
}

JPH::ObjectLayer ToObjectLayer(AetherMotionType type) {
  return Layers::Encode(0, type != AetherMotionType::Static);
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
// `userData` vai na forma FOLHA: `Shape::GetSubShapeUserData` de um composto
// devolve o dado da folha acertada, não o da entrada do composto.
JPH::RefConst<JPH::Shape> ToJoltShape(const AetherShapeDesc &desc, ae::u64 userData = 0) {
  if (desc.kind == AetherShapeKind::Box) {
    JPH::BoxShapeSettings settings(ToJolt(desc.boxHalfExtent));
    settings.mUserData = userData;
    auto result = settings.Create();
    return result.HasError() ? nullptr : result.Get();
  }
  if (desc.kind == AetherShapeKind::Capsule) {
    JPH::CapsuleShapeSettings settings(desc.capsuleHalfHeight, desc.sphereRadius);
    settings.mUserData = userData;
    auto result = settings.Create();
    return result.HasError() ? nullptr : result.Get();
  }
  JPH::SphereShapeSettings settings(desc.sphereRadius);
  settings.mUserData = userData;
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
  explicit QueryLayerFilter(AetherQueryLayerMask mask, ae::u32 gameplayMask = 0xffffffffu)
      : mMask(mask), mGameplayMask(gameplayMask) {}
  bool ShouldCollide(JPH::ObjectLayer inLayer) const override {
    const ae::u32 bit = Layers::IsMoving(inLayer) ? static_cast<ae::u32>(AetherQueryLayerMask::Dynamic)
                                                  : static_cast<ae::u32>(AetherQueryLayerMask::Static);
    if ((static_cast<ae::u32>(mMask) & bit) == 0) return false;
    const ae::u32 gameplay = Layers::Gameplay(inLayer);
    return gameplay >= Layers::GameplayLayerCount || (mGameplayMask & (1u << gameplay)) != 0;
  }

private:
  AetherQueryLayerMask mMask;
  ae::u32 mGameplayMask;
};

/// Espelha exatamente JPH::IgnoreSingleBodyFilter (Jolt/Physics/Body/BodyFilter.h) — não
/// reusamos a classe do Jolt diretamente porque ela guarda um JPH::BodyID, e aqui só temos o
/// AetherBodyHandle antes de decidir se é válido; um wrapper fino evita expor o tipo do Jolt
/// na assinatura de uma função auxiliar nossa.
class QueryBodyFilter final : public JPH::BodyFilter {
public:
  explicit QueryBodyFilter(AetherBodyHandle ignoreBody, bool includeSensors = true)
      : mIgnore(ignoreBody == AetherBodyHandle_Invalid ? JPH::BodyID() : JPH::BodyID(ignoreBody)),
        mIncludeSensors(includeSensors) {}
  bool ShouldCollide(const JPH::BodyID &inBodyID) const override { return mIgnore != inBodyID; }
  // Sensores participam das queries do Jolt como qualquer corpo. Um raycast de
  // visada que atravessasse a zona de deteccao do proprio jogo acertaria o
  // volume invisivel; por isso o filtro e explicito, nao um padrao herdado.
  bool ShouldCollideLocked(const JPH::Body &inBody) const override {
    return mIncludeSensors || !inBody.IsSensor();
  }

private:
  JPH::BodyID mIgnore;
  bool mIncludeSensors;
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
    mFrameContacts = std::move(mPendingContacts);
    mPendingContacts.clear();
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
    std::stable_sort(mFrameContacts.begin(), mFrameContacts.end(), ContactLess);
    mFrameContacts.erase(std::unique(mFrameContacts.begin(), mFrameContacts.end(), ContactEqual),
                         mFrameContacts.end());
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
    AddSolid(body1, body2, manifold, active);
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
      AddSolid(body1, body2, manifold, active);
      mActiveSubShapes.emplace(subPair, active);
      return;
    }
    for (ae::u32 i = 0; i < it->second.count; ++i)
      AppendEvent(it->second.directions[i], AetherTriggerEventType::Stay);
    if (it->second.hasSolid)
      AppendContact(it->second.solid, AetherContactEventType::Stay,
                    OrientedNormal(body1, it->second.solid, manifold), true);
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
    if (it->second.hasSolid) {
      auto solidIt = mSolidPairCounts.find(it->second.solid);
      if (solidIt != mSolidPairCounts.end() && --solidIt->second == 0) {
        // O Jolt nao informa geometria em OnContactRemoved: o Exit sai sem
        // normal, e quem consome sabe disso pelo sinalizador.
        AppendContact(it->second.solid, AetherContactEventType::Exit, AetherVec3{0, 0, 0}, false);
        mSolidPairCounts.erase(solidIt);
      }
    }
    mActiveSubShapes.erase(it);
  }

  ae::i32 CopyContacts(AetherContactEventV1 *outEvents, ae::i32 maxResults) {
    std::lock_guard<std::mutex> lock(mMutex);
    const ae::i32 count = static_cast<ae::i32>(mFrameContacts.size());
    const ae::i32 toCopy = outEvents != nullptr && maxResults > 0 ? std::min(count, maxResults) : 0;
    std::copy_n(mFrameContacts.begin(), toCopy, outEvents);
    return count;
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
    // Par solido (nenhum dos dois e sensor), ordenado por handle para que o
    // mesmo contato nao produza dois pares diferentes conforme a ordem em que
    // o Jolt entrega os corpos.
    DirectedPair solid{};
    bool hasSolid = false;
  };

  static ae::u32 LayerBit(JPH::ObjectLayer layer) {
    return Layers::IsMoving(layer) ? static_cast<ae::u32>(AetherQueryLayerMask::Dynamic)
                                   : static_cast<ae::u32>(AetherQueryLayerMask::Static);
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

  static DirectedPair OrderedPair(const JPH::Body &a, const JPH::Body &b) {
    const AetherBodyHandle first = a.GetID().GetIndexAndSequenceNumber();
    const AetherBodyHandle second = b.GetID().GetIndexAndSequenceNumber();
    return first <= second ? DirectedPair{first, second} : DirectedPair{second, first};
  }

  // A normal do manifold move o corpo 2 do Jolt para fora do corpo 1. Como o
  // par e reordenado por handle, ela e invertida quando a ordem nao coincide --
  // senao metade dos contatos chegaria com a normal apontando ao contrario.
  static AetherVec3 OrientedNormal(const JPH::Body &joltFirst, const DirectedPair &pair,
                                   const JPH::ContactManifold &manifold) {
    const JPH::Vec3 normal = joltFirst.GetID().GetIndexAndSequenceNumber() == pair.sensor
                                 ? manifold.mWorldSpaceNormal
                                 : -manifold.mWorldSpaceNormal;
    return AetherVec3{normal.GetX(), normal.GetY(), normal.GetZ()};
  }

  void AddSolid(const JPH::Body &body1, const JPH::Body &body2,
                const JPH::ContactManifold &manifold, ActiveSubShape &active) {
    if (body1.IsSensor() || body2.IsSensor()) return;
    const DirectedPair pair = OrderedPair(body1, body2);
    active.solid = pair;
    active.hasSolid = true;
    ae::u32 &count = mSolidPairCounts[pair];
    if (count++ == 0)
      AppendContact(pair, AetherContactEventType::Enter, OrientedNormal(body1, pair, manifold), true);
  }

  void AppendContact(const DirectedPair &pair, AetherContactEventType type,
                     AetherVec3 normal, bool hasNormal) {
    AetherContactEventV1 event{pair.sensor, pair.other, type, hasNormal ? 1u : 0u, normal};
    (mInStep ? mFrameContacts : mPendingContacts).push_back(event);
  }

  static bool ContactLess(const AetherContactEventV1 &a, const AetherContactEventV1 &b) {
    if (a.first != b.first) return a.first < b.first;
    if (a.second != b.second) return a.second < b.second;
    return static_cast<ae::u32>(a.type) < static_cast<ae::u32>(b.type);
  }
  static bool ContactEqual(const AetherContactEventV1 &a, const AetherContactEventV1 &b) {
    return a.first == b.first && a.second == b.second && a.type == b.type;
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
  std::map<DirectedPair, ae::u32> mSolidPairCounts;
  std::vector<AetherContactEventV1> mPendingContacts;
  std::vector<AetherContactEventV1> mFrameContacts;
  bool mInStep = false;
};

} // namespace

// Definição real do tipo opaco declarado em jolt_bridge.h. Cada mundo tem seu
// próprio alocador temporário e pool de threads — simples e correto para a
// fatia vertical; compartilhar esses recursos entre mundos simultâneos (ex.:
// preview do editor + play-in-editor ao mesmo tempo) é uma otimização futura,
// não uma correção necessária agora.
struct AetherPhysicsWorld {
  // Tudo ligado por padrao: um projeto que nunca configura camadas se comporta
  // exatamente como antes deste recurso existir.
  ae::u32 layerInteraction[Layers::GameplayLayerCount];
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
    // Sem configuração, todas as camadas interagem: o padrão de migração de um
    // projeto que nunca ouviu falar de camadas é o comportamento anterior.
    for (ae::u32 layer = 0; layer < Layers::GameplayLayerCount; ++layer)
      layerInteraction[layer] = 0xffffffffu;
    objectLayerPairFilter.Bind(layerInteraction);

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

AetherBodyHandle CreateBodyInternal(AetherPhysicsWorld &world, const AetherBodyDescV2 &desc, AetherVec3 localCenter={0,0,0},
    JPH::RefConst<JPH::Shape> suppliedShape=nullptr,const AetherBodyDynamicsV1 *dynamics=nullptr,bool massless=false) {
  JPH::RefConst<JPH::Shape> shape = suppliedShape != nullptr ? suppliedShape : ToJoltShape(desc.shape);
  if (shape == nullptr) return AetherBodyHandle_Invalid;
  if(localCenter.x!=0 || localCenter.y!=0 || localCenter.z!=0) {
    JPH::RotatedTranslatedShapeSettings translated(ToJolt(localCenter),JPH::Quat::sIdentity(),shape);
    const auto result=translated.Create();if(result.HasError()) return AetherBodyHandle_Invalid;
    shape=result.Get();
  }

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
  if(dynamics) {
    bodySettings.mLinearDamping=dynamics->linearDamping;
    bodySettings.mAngularDamping=dynamics->angularDamping;
    bodySettings.mGravityFactor=dynamics->gravityFactor;
    bodySettings.mAngularVelocity=ToJolt(dynamics->angularVelocity);
    bodySettings.mAllowSleeping=dynamics->allowSleeping!=0;
  }
  // Corpo cinemático com malha de triângulos: a malha não tem volume, e o Jolt
  // ainda monta propriedades de movimento para ele. Massa fornecida evita a
  // massa inválida; um cinemático nunca a usa para responder a forças.
  if(massless) {
    bodySettings.mOverrideMassProperties=JPH::EOverrideMassProperties::MassAndInertiaProvided;
    bodySettings.mMassPropertiesOverride.SetMassAndInertiaOfSolidBox(JPH::Vec3::sReplicate(1.0f),1.0f);
  }
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
  ae::physics::ensureJoltInitialized();
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

AetherBodyHandle AetherPhysics_CreateBodyWithLocalCenterV2(AetherPhysicsWorld *world,
    const AetherBodyDescV2 *desc,AetherVec3 localCenter) {
  constexpr ae::u32 validMask=static_cast<ae::u32>(AetherQueryLayerMask::All);
  if(!world||!desc||desc->structSize<sizeof(AetherBodyDescV2)||desc->apiVersion!=AetherBodyApiVersionV2||
      (desc->eventLayerMask&~validMask)!=0||!std::isfinite(localCenter.x)||!std::isfinite(localCenter.y)||!std::isfinite(localCenter.z)) return AetherBodyHandle_Invalid;
  return CreateBodyInternal(*world,*desc,localCenter);
}

AetherBodyHandle AetherPhysics_CreateCompoundBodyV1(AetherPhysicsWorld *world,
    const AetherBodyDescV2 *desc,const AetherCompoundPart *parts,ae::u32 count,
    const AetherBodyDynamicsV1 *dynamics) {
  const auto finite=[](AetherVec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);};
  const auto unit=[](AetherQuat q){const float n=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;return std::isfinite(n)&&std::abs(n-1)<.001f;};
  if(!world||!desc||!parts||count<1||count>256||!dynamics||dynamics->structSize<sizeof(*dynamics)||dynamics->apiVersion!=1||
      desc->structSize<sizeof(*desc)||desc->apiVersion!=AetherBodyApiVersionV2||(desc->eventLayerMask&~3u)||
      static_cast<ae::u32>(desc->motionType)>2||!finite(desc->position)||!unit(desc->rotation)||
      !std::isfinite(desc->friction)||desc->friction<0||!std::isfinite(desc->restitution)||desc->restitution<0||desc->restitution>1||
      !std::isfinite(dynamics->linearDamping)||dynamics->linearDamping<0||dynamics->linearDamping>10||
      !std::isfinite(dynamics->angularDamping)||dynamics->angularDamping<0||dynamics->angularDamping>10||
      !std::isfinite(dynamics->gravityFactor)||std::abs(dynamics->gravityFactor)>100||!finite(dynamics->angularVelocity)) return AetherBodyHandle_Invalid;
  JPH::StaticCompoundShapeSettings compound;
  for(ae::u32 i=0;i<count;++i) {
    const auto &p=parts[i];const auto &shape=p.shape;
    if(!finite(p.position)||!unit(p.rotation)||static_cast<ae::u32>(shape.kind)>2) return AetherBodyHandle_Invalid;
    if(shape.kind==AetherShapeKind::Box) {
      if(!finite(shape.boxHalfExtent)||shape.boxHalfExtent.x<=0||shape.boxHalfExtent.y<=0||shape.boxHalfExtent.z<=0) return AetherBodyHandle_Invalid;
    } else if(!std::isfinite(shape.sphereRadius)||shape.sphereRadius<=0||
        (shape.kind==AetherShapeKind::Capsule&&(!std::isfinite(shape.capsuleHalfHeight)||shape.capsuleHalfHeight<=0))) return AetherBodyHandle_Invalid;
    const auto native=ToJoltShape(shape,i);if(native==nullptr) return AetherBodyHandle_Invalid;
    compound.AddShape(ToJolt(p.position),ToJolt(p.rotation),native.GetPtr(),i);
  }
  const auto result=compound.Create();if(result.HasError()) return AetherBodyHandle_Invalid;
  return CreateBodyInternal(*world,*desc,{0,0,0},result.Get(),dynamics);
}

AetherBodyHandle AetherPhysics_CreateCompoundBodyV3(AetherPhysicsWorld *world,
    const AetherBodyDescV2 *desc,const AetherCompoundPartV3 *parts,ae::u32 count,
    const AetherBodyDynamicsV1 *dynamics) {
  const auto finite=[](AetherVec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);};
  const auto unit=[](AetherQuat q){const float n=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;return std::isfinite(n)&&std::abs(n-1)<.001f;};
  if(!world||!desc||!parts||count<1||count>256||!dynamics||dynamics->structSize<sizeof(*dynamics)||dynamics->apiVersion!=1||
      desc->structSize<sizeof(*desc)||desc->apiVersion!=AetherBodyApiVersionV2||(desc->eventLayerMask&~3u)||
      static_cast<ae::u32>(desc->motionType)>2||!finite(desc->position)||!unit(desc->rotation)||
      !std::isfinite(desc->friction)||desc->friction<0||!std::isfinite(desc->restitution)||desc->restitution<0||desc->restitution>1||
      !std::isfinite(dynamics->linearDamping)||dynamics->linearDamping<0||dynamics->linearDamping>10||
      !std::isfinite(dynamics->angularDamping)||dynamics->angularDamping<0||dynamics->angularDamping>10||
      !std::isfinite(dynamics->gravityFactor)||std::abs(dynamics->gravityFactor)>100||!finite(dynamics->angularVelocity)) return AetherBodyHandle_Invalid;
  JPH::StaticCompoundShapeSettings compound;
  bool mesh=false;
  for(ae::u32 i=0;i<count;++i) {
    const auto &p=parts[i];const auto &shape=p.base.shape;
    if(!finite(p.base.position)||!unit(p.base.rotation)||static_cast<ae::u32>(p.geometry)>2 ||
       !ae::physics::validMeshCooking(p.cooking)) return AetherBodyHandle_Invalid;
    JPH::RefConst<JPH::Shape> native;
    if(p.geometry==AetherPartGeometry::Primitive) {
      if(static_cast<ae::u32>(shape.kind)>2) return AetherBodyHandle_Invalid;
      if(shape.kind==AetherShapeKind::Box) {
        if(!finite(shape.boxHalfExtent)||shape.boxHalfExtent.x<=0||shape.boxHalfExtent.y<=0||shape.boxHalfExtent.z<=0) return AetherBodyHandle_Invalid;
      } else if(!std::isfinite(shape.sphereRadius)||shape.sphereRadius<=0||
          (shape.kind==AetherShapeKind::Capsule&&(!std::isfinite(shape.capsuleHalfHeight)||shape.capsuleHalfHeight<=0))) return AetherBodyHandle_Invalid;
      native=ToJoltShape(shape,i);
    } else {
      const bool hull=p.geometry==AetherPartGeometry::ConvexHull;
      if(!p.vertices||p.vertexCount<(hull?4u:3u)) return AetherBodyHandle_Invalid;
      for(ae::u32 v=0;v<p.vertexCount;++v) if(!finite(p.vertices[v])) return AetherBodyHandle_Invalid;
      if(hull) {
        const auto result=ae::physics::detail::createConvexHullShape(
            std::span<const AetherVec3>(p.vertices,p.vertexCount),p.cooking,i);
        if(result.HasError()) return AetherBodyHandle_Invalid;
        native=result.Get();
      } else {
        if(desc->motionType==AetherMotionType::Dynamic||!p.indices||p.indexCount<3||p.indexCount%3) return AetherBodyHandle_Invalid;
        for(ae::u32 k=0;k<p.indexCount;++k) if(p.indices[k]>=p.vertexCount) return AetherBodyHandle_Invalid;
        JPH::VertexList vertices;vertices.reserve(p.vertexCount);
        for(ae::u32 v=0;v<p.vertexCount;++v) vertices.emplace_back(p.vertices[v].x,p.vertices[v].y,p.vertices[v].z);
        JPH::IndexedTriangleList triangles;triangles.reserve(p.indexCount/3);
        for(ae::u32 k=0;k<p.indexCount;k+=3) triangles.emplace_back(p.indices[k],p.indices[k+1],p.indices[k+2],0);
        JPH::MeshShapeSettings settings(std::move(vertices),std::move(triangles));
        settings.mBuildQuality=(p.cooking.flags&AetherMeshCookingOptimizeRuntime)
            ?JPH::MeshShapeSettings::EBuildQuality::FavorRuntimePerformance
            :JPH::MeshShapeSettings::EBuildQuality::FavorBuildSpeed;
        settings.mActiveEdgeCosThresholdAngle=std::cos(p.cooking.activeEdgeAngleDegrees*0.01745329252f);
        settings.mUserData=i;
        const auto result=settings.Create();if(result.HasError()) return AetherBodyHandle_Invalid;
        native=result.Get();mesh=true;
      }
    }
    if(native==nullptr) return AetherBodyHandle_Invalid;
    compound.AddShape(ToJolt(p.base.position),ToJolt(p.base.rotation),native.GetPtr(),i);
  }
  const auto result=compound.Create();if(result.HasError()) return AetherBodyHandle_Invalid;
  return CreateBodyInternal(*world,*desc,{0,0,0},result.Get(),dynamics,mesh&&desc->motionType==AetherMotionType::Kinematic);
}

AetherBodyHandle AetherPhysics_CreateCompoundBodyV2(AetherPhysicsWorld *world,
    const AetherBodyDescV2 *desc,const AetherCompoundPartV2 *parts,ae::u32 count,
    const AetherBodyDynamicsV1 *dynamics) {
  if(!parts || count<1 || count>256) return AetherBodyHandle_Invalid;
  std::vector<AetherCompoundPartV3> upgraded(count);
  for(ae::u32 i=0;i<count;++i) {
    upgraded[i].base=parts[i].base;upgraded[i].geometry=parts[i].geometry;
    upgraded[i].vertices=parts[i].vertices;upgraded[i].vertexCount=parts[i].vertexCount;
    upgraded[i].indices=parts[i].indices;upgraded[i].indexCount=parts[i].indexCount;
    upgraded[i].cooking=AetherMeshCookingDefaultsV1;
  }
  return AetherPhysics_CreateCompoundBodyV3(world,desc,upgraded.data(),count,dynamics);
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

ae::i32 AetherPhysics_GetContactEventsV1(AetherPhysicsWorld *world,
                                         AetherContactEventV1 *outEvents,
                                         ae::i32 maxResults) {
  return world == nullptr ? 0 : world->triggerListener.CopyContacts(outEvents, maxResults);
}

ae::i32 AetherPhysics_SetLayerInteractionV1(AetherPhysicsWorld *world, const ae::u32 *matrix, ae::u32 count) {
  if (world == nullptr || matrix == nullptr || count != Layers::GameplayLayerCount) return 0;
  // Reciprocidade conferida ANTES de escrever: o Jolt consulta o par uma vez
  // so, em ordem nao especificada, entao uma matriz assimetrica faria a colisao
  // depender da ordem de criacao dos corpos.
  for (ae::u32 a = 0; a < count; ++a)
    for (ae::u32 b = 0; b < count; ++b)
      if (((matrix[a] >> b) & 1u) != ((matrix[b] >> a) & 1u)) return 0;
  for (ae::u32 a = 0; a < count; ++a) world->layerInteraction[a] = matrix[a];
  return 1;
}

ae::i32 AetherPhysics_SetBodyGameplayLayerV1(AetherPhysicsWorld *world, AetherBodyHandle body, ae::u32 layer) {
  if (world == nullptr || body == AetherBodyHandle_Invalid || layer >= Layers::GameplayLayerCount) return 0;
  JPH::BodyInterface &bodies = world->physicsSystem.GetBodyInterface();
  const JPH::BodyID id(body);
  if (!bodies.IsAdded(id)) return 0;
  const bool moving = bodies.GetMotionType(id) != JPH::EMotionType::Static;
  bodies.SetObjectLayer(id, Layers::Encode(layer, moving));
  return 1;
}

ae::i32 AetherPhysics_GetSubShapeUserDataV1(AetherPhysicsWorld *world, AetherBodyHandle body,
                                            ae::u32 subShapeId, ae::u64 *outUserData) {
  if (world == nullptr || outUserData == nullptr || body == AetherBodyHandle_Invalid) return 0;
  JPH::BodyLockRead lock(world->physicsSystem.GetBodyLockInterface(), JPH::BodyID(body));
  if (!lock.Succeeded()) return 0;
  const JPH::Shape *shape = lock.GetBody().GetShape();
  if (shape == nullptr) return 0;
  JPH::SubShapeID id;
  id.SetValue(static_cast<JPH::SubShapeID::Type>(subShapeId));
  *outUserData = shape->GetSubShapeUserData(id);
  return 1;
}

ae::u32 AetherPhysics_GetBodyGameplayLayerV1(const AetherPhysicsWorld *world, AetherBodyHandle body) {
  if (world == nullptr || body == AetherBodyHandle_Invalid) return 0xffffffffu;
  const JPH::BodyInterface &bodies =
      const_cast<AetherPhysicsWorld *>(world)->physicsSystem.GetBodyInterface();
  const JPH::BodyID id(body);
  if (!bodies.IsAdded(id)) return 0xffffffffu;
  return Layers::Gameplay(bodies.GetObjectLayer(id));
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

ae::i32 AetherPhysics_SetAllowSleepingV2(AetherPhysicsWorld *world, AetherBodyHandle handle,
                                         ae::u32 allowed) {
  if (world == nullptr || handle == AetherBodyHandle_Invalid || allowed > 1) return 0;
  JPH::BodyLockWrite lock(world->physicsSystem.GetBodyLockInterface(), JPH::BodyID(handle));
  if (!lock.Succeeded() || !lock.GetBody().IsDynamic()) return 0;
  lock.GetBody().SetAllowSleeping(allowed != 0);
  return 1;
}

ae::i32 AetherPhysics_ApplyBodyForceV1(AetherPhysicsWorld *world,AetherBodyHandle handle,AetherVec3 value,AetherBodyForceKind kind) {
  if(!world||handle==AetherBodyHandle_Invalid||static_cast<ae::u32>(kind)>3||
      !std::isfinite(value.x)||!std::isfinite(value.y)||!std::isfinite(value.z)) return 0;
  const JPH::BodyID id(handle);
  {
    JPH::BodyLockWrite lock(world->physicsSystem.GetBodyLockInterface(),id);
    if(!lock.Succeeded()||!lock.GetBody().IsDynamic()) return 0;
    auto &body=lock.GetBody();const auto v=ToJolt(value);
    switch(kind) {
      case AetherBodyForceKind::Force:body.AddForce(v);break;
      case AetherBodyForceKind::Impulse:body.AddImpulse(v);break;
      case AetherBodyForceKind::Torque:body.AddTorque(v);break;
      case AetherBodyForceKind::AngularImpulse:body.AddAngularImpulse(v);break;
    }
  }
  // BodyInterface acquires its own lock; never activate while holding BodyLockWrite.
  world->physicsSystem.GetBodyInterface().ActivateBody(id);return 1;
}
ae::i32 AetherPhysics_TryGetBodyVelocityV1(AetherPhysicsWorld *world,AetherBodyHandle handle,AetherVec3 *out) {
  if(!world||handle==AetherBodyHandle_Invalid||!out) return 0;
  JPH::BodyLockRead lock(world->physicsSystem.GetBodyLockInterface(),JPH::BodyID(handle));
  if(!lock.Succeeded()) return 0;
  *out=FromJolt(lock.GetBody().GetLinearVelocity());return 1;
}
ae::i32 AetherPhysics_TryGetBodyAngularVelocityV1(AetherPhysicsWorld *world,AetherBodyHandle handle,AetherVec3 *out) {
  if(!world||handle==AetherBodyHandle_Invalid||!out) return 0;
  JPH::BodyLockRead lock(world->physicsSystem.GetBodyLockInterface(),JPH::BodyID(handle));
  if(!lock.Succeeded()) return 0;
  *out=FromJolt(lock.GetBody().GetAngularVelocity());return 1;
}
ae::i32 AetherPhysics_SetBodyAngularVelocityV1(AetherPhysicsWorld *world,AetherBodyHandle handle,AetherVec3 value) {
  if(!world||handle==AetherBodyHandle_Invalid||!std::isfinite(value.x)||!std::isfinite(value.y)||!std::isfinite(value.z)) return 0;
  {
    JPH::BodyLockRead lock(world->physicsSystem.GetBodyLockInterface(),JPH::BodyID(handle));
    if(!lock.Succeeded()||lock.GetBody().IsStatic()) return 0;
  }
  world->physicsSystem.GetBodyInterface().SetAngularVelocity(JPH::BodyID(handle),ToJolt(value));return 1;
}

ae::i32 AetherPhysics_SetMassV2(AetherPhysicsWorld *world,AetherBodyHandle handle,float mass) {
  if(!world || handle==AetherBodyHandle_Invalid || !std::isfinite(mass) || mass<=0 || mass>1e6f) return 0;
  JPH::BodyLockWrite lock(world->physicsSystem.GetBodyLockInterface(),JPH::BodyID(handle));
  if(!lock.Succeeded() || !lock.GetBody().IsDynamic()) return 0;
  auto &body=lock.GetBody();auto properties=body.GetShape()->GetMassProperties();
  properties.ScaleToMass(mass);
  auto *motion=body.GetMotionProperties();motion->SetMassProperties(motion->GetAllowedDOFs(),properties);
  return 1;
}

ae::i32 AetherPhysics_TryGetBodyPoseV2(AetherPhysicsWorld *world, AetherBodyHandle handle,
                                      AetherVec3 *position, AetherQuat *rotation) {
  if (world == nullptr || handle == AetherBodyHandle_Invalid) return 0;
  JPH::BodyLockRead lock(world->physicsSystem.GetBodyLockInterface(), JPH::BodyID(handle));
  if (!lock.Succeeded()) return 0;
  const auto &body = lock.GetBody();
  const auto p = body.GetPosition();
  const auto q = body.GetRotation();
  if (position) *position = {static_cast<float>(p.GetX()), static_cast<float>(p.GetY()), static_cast<float>(p.GetZ())};
  if (rotation) *rotation = {q.GetX(), q.GetY(), q.GetZ(), q.GetW()};
  return 1;
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

// --------------------------------------------------- queries com contato real (V2)

namespace {

bool ValidQueryFilter(const AetherQueryFilterV1 *filter) {
  return filter != nullptr && filter->structSize == sizeof(AetherQueryFilterV1) &&
         filter->apiVersion == AetherQueryFilterApiVersionV1 &&
         static_cast<ae::u32>(filter->layerMask) <= static_cast<ae::u32>(AetherQueryLayerMask::All);
}

bool FiniteVec(AetherVec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

// A normal so existe com uma segunda consulta ao corpo acertado: o RayCastResult
// do Jolt traz apenas corpo, sub-shape e fracao. Fazer essa consulta aqui e o que
// permite devolver um contato completo em vez de zeros com cara de contato.
AetherRayQueryHitV1 ToRayHit(AetherPhysicsWorld *world, const JPH::RRayCast &ray,
                             const JPH::RayCastResult &hit) {
  AetherRayQueryHitV1 out{};
  out.body = hit.mBodyID.GetIndexAndSequenceNumber();
  out.subShapeId = hit.mSubShapeID2.GetValue();
  out.fraction = hit.mFraction;
  const JPH::RVec3 point = ray.GetPointOnRay(hit.mFraction);
  out.point = AetherVec3{static_cast<float>(point.GetX()), static_cast<float>(point.GetY()),
                         static_cast<float>(point.GetZ())};
  JPH::BodyLockRead lock(world->physicsSystem.GetBodyLockInterface(), hit.mBodyID);
  if (lock.Succeeded()) {
    const JPH::Body &body = lock.GetBody();
    out.normal = FromJolt(body.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, point));
    out.isSensor = body.IsSensor() ? 1u : 0u;
  }
  return out;
}

ae::u32 BodyIsSensor(AetherPhysicsWorld *world, const JPH::BodyID &id) {
  JPH::BodyLockRead lock(world->physicsSystem.GetBodyLockInterface(), id);
  return lock.Succeeded() && lock.GetBody().IsSensor() ? 1u : 0u;
}

} // namespace

ae::i32 AetherPhysics_RayCastClosestV2(AetherPhysicsWorld *world, AetherVec3 origin, AetherVec3 direction,
                                       const AetherQueryFilterV1 *filter, AetherRayQueryHitV1 *outHit) {
  if (world == nullptr || !ValidQueryFilter(filter) || !FiniteVec(origin) || !FiniteVec(direction)) return 0;
  // Raio de comprimento zero nao tem direcao: a fracao devolvida seria
  // indefinida e a normal, arbitraria. Recusar e o unico resultado honesto.
  if (direction.x == 0.0f && direction.y == 0.0f && direction.z == 0.0f) return 0;

  const JPH::RRayCast ray(JPH::RVec3(origin.x, origin.y, origin.z), ToJolt(direction));
  JPH::ClosestHitCollisionCollector<JPH::CastRayCollector> collector;
  QueryLayerFilter layerFilter(filter->layerMask, filter->gameplayLayerMask);
  QueryBodyFilter bodyFilter(filter->ignoreBody, filter->includeSensors != 0);
  world->physicsSystem.GetNarrowPhaseQuery().CastRay(ray, {}, collector, {}, layerFilter, bodyFilter);
  if (!collector.HadHit()) return 0;
  if (outHit != nullptr) *outHit = ToRayHit(world, ray, collector.mHit);
  return 1;
}

ae::i32 AetherPhysics_RayCastAllV2(AetherPhysicsWorld *world, AetherVec3 origin, AetherVec3 direction,
                                   const AetherQueryFilterV1 *filter,
                                   AetherRayQueryHitV1 *outHits, ae::i32 maxResults) {
  if (world == nullptr || !ValidQueryFilter(filter) || !FiniteVec(origin) || !FiniteVec(direction)) return 0;
  if (direction.x == 0.0f && direction.y == 0.0f && direction.z == 0.0f) return 0;

  const JPH::RRayCast ray(JPH::RVec3(origin.x, origin.y, origin.z), ToJolt(direction));
  JPH::AllHitCollisionCollector<JPH::CastRayCollector> collector;
  QueryLayerFilter layerFilter(filter->layerMask, filter->gameplayLayerMask);
  QueryBodyFilter bodyFilter(filter->ignoreBody, filter->includeSensors != 0);
  world->physicsSystem.GetNarrowPhaseQuery().CastRay(ray, {}, collector, {}, layerFilter, bodyFilter);
  collector.Sort();

  const ae::i32 count = static_cast<ae::i32>(collector.mHits.size());
  const ae::i32 toCopy = outHits != nullptr && maxResults > 0 ? std::min(count, maxResults) : 0;
  for (ae::i32 i = 0; i < toCopy; ++i) outHits[i] = ToRayHit(world, ray, collector.mHits[i]);
  return count;
}

ae::i32 AetherPhysics_ShapeCastClosestV2(AetherPhysicsWorld *world, const AetherShapeDesc *shape,
                                         AetherVec3 origin, AetherQuat rotation, AetherVec3 direction,
                                         const AetherQueryFilterV1 *filter, AetherShapeQueryHit *outHit,
                                         ae::u32 *outSubShapeId, ae::u32 *outIsSensor) {
  if (world == nullptr || shape == nullptr || !ValidQueryFilter(filter) ||
      !FiniteVec(origin) || !FiniteVec(direction)) return 0;
  JPH::RefConst<JPH::Shape> joltShape = ToJoltShape(*shape);
  if (joltShape == nullptr) return 0;

  const JPH::RMat44 startTransform =
      JPH::RMat44::sRotationTranslation(ToJolt(rotation), JPH::RVec3(origin.x, origin.y, origin.z));
  const JPH::RShapeCast cast =
      JPH::RShapeCast::sFromWorldTransform(joltShape, JPH::Vec3::sReplicate(1.0f), startTransform, ToJolt(direction));

  JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
  QueryLayerFilter layerFilter(filter->layerMask, filter->gameplayLayerMask);
  QueryBodyFilter bodyFilter(filter->ignoreBody, filter->includeSensors != 0);
  world->physicsSystem.GetNarrowPhaseQuery().CastShape(cast, {}, JPH::RVec3::sZero(), collector, {},
                                                       layerFilter, bodyFilter);
  if (!collector.HadHit()) return 0;
  if (outHit != nullptr) *outHit = ToShapeQueryHit(collector.mHit, collector.mHit.mFraction);
  if (outSubShapeId != nullptr) *outSubShapeId = collector.mHit.mSubShapeID2.GetValue();
  if (outIsSensor != nullptr) *outIsSensor = BodyIsSensor(world, collector.mHit.mBodyID2);
  return 1;
}

ae::i32 AetherPhysics_OverlapShapeV2(AetherPhysicsWorld *world, const AetherShapeDesc *shape,
                                     AetherVec3 origin, AetherQuat rotation,
                                     const AetherQueryFilterV1 *filter, AetherShapeQueryHit *outHits,
                                     ae::u32 *outSubShapeIds, ae::u32 *outSensorFlags, ae::i32 maxResults) {
  if (world == nullptr || shape == nullptr || !ValidQueryFilter(filter) || !FiniteVec(origin)) return 0;
  JPH::RefConst<JPH::Shape> joltShape = ToJoltShape(*shape);
  if (joltShape == nullptr) return 0;

  const JPH::RMat44 transform =
      JPH::RMat44::sRotationTranslation(ToJolt(rotation), JPH::RVec3(origin.x, origin.y, origin.z));
  JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
  QueryLayerFilter layerFilter(filter->layerMask, filter->gameplayLayerMask);
  QueryBodyFilter bodyFilter(filter->ignoreBody, filter->includeSensors != 0);
  world->physicsSystem.GetNarrowPhaseQuery().CollideShape(joltShape, JPH::Vec3::sReplicate(1.0f), transform, {},
                                                          JPH::RVec3::sZero(), collector, {}, layerFilter, bodyFilter);

  const ae::i32 count = static_cast<ae::i32>(collector.mHits.size());
  const ae::i32 toCopy = maxResults > 0 ? std::min(count, maxResults) : 0;
  for (ae::i32 i = 0; i < toCopy; ++i) {
    if (outHits != nullptr) outHits[i] = ToShapeQueryHit(collector.mHits[i], 0.0f);
    if (outSubShapeIds != nullptr) outSubShapeIds[i] = collector.mHits[i].mSubShapeID2.GetValue();
    if (outSensorFlags != nullptr) outSensorFlags[i] = BodyIsSensor(world, collector.mHits[i].mBodyID2);
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
  return AetherPhysics_ApplyWaterForcesV2(world,samples,nullptr,count,settings,outStats);
}

ae::i32 AetherPhysics_ApplyWaterForcesV2(AetherPhysicsWorld *world,
    const AetherWaterBodySample *samples,const float *verticalDepths,ae::i32 count,
    const ae::physics::BuoyancySettings *settings,AetherWaterForceStats *outStats) {
  if (outStats != nullptr) *outStats = {};
  if (world == nullptr || settings == nullptr) return -1;
  if (count < 0 || (count > 0 && samples == nullptr)) return -1;
  if (!ae::physics::validateBuoyancySettings(*settings)) return -1;
  if(verticalDepths) for(ae::i32 i=0;i<count;++i) if(!std::isfinite(verticalDepths[i])) return -1;

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
    const ae::physics::SubmergedVolume submerged = ae::physics::submergedWaterVolume(
        shape,
        {static_cast<float>(position.GetX()), static_cast<float>(position.GetY()),
         static_cast<float>(position.GetZ())},
        {rotation.GetX(), rotation.GetY(), rotation.GetZ(), rotation.GetW()}, plane,verticalDepths?verticalDepths[index]:-1);
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
