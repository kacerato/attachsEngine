// A ponte entre o mundo de execução e o Jolt.
//
// Era `editor/editor_scene_physics.h` e lia o documento do editor. Agora lê o
// `GameWorld`: o mesmo adaptador serve a um consumidor sem editor, e é ele que
// declara ao mundo quem publica a pose de cada objeto (`TransformAuthority`),
// de modo que um script que tente mover um corpo receba uma recusa explicada em
// vez de uma posição que volta no passo seguinte.
//
// Uma thread só: todos os handles de corpo pertencem a ela.
#pragma once
#include "runtime/game_world.h"
#include "runtime/scene_components.h"
#include "physics/jolt_bridge.h"
#include "physics/character_motor.h"
#include "runtime/physics_field_sample.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace ae::runtime {

// Filtro de consulta em termos do PROJETO, não do backend: camadas de gameplay,
// classes de movimento e o objeto que faz a consulta (que quase sempre não quer
// acertar a si mesmo).
struct QueryFilter {
  u32 gameplayLayerMask = 0xffffffffu;
  bool includeStatic = true;
  bool includeDynamic = true;
  // Sensores ficam de fora por padrão: um raio de visada que parasse na zona de
  // detecção invisível do próprio jogo seria um erro difícil de ver.
  bool includeSensors = false;
  ObjectId ignore = kInvalidObject;
};

struct QueryHit {
  // Body owner; filtering and body commands retain this identity.
  ObjectId object = kInvalidObject;
  // Instância do componente Colisor que respondeu, quando o corpo foi montado a
  // partir de colisores autorados. Zero quando o backend não sabe dizer.
  u64 colliderInstance = 0;
  // Authored shape identity is this object together with the local component ID.
  ObjectId colliderObject = kInvalidObject;
  float point[3]{};
  float normal[3]{};
  float distance = 0;
  float fraction = 0;
  // Falso significa "não há normal", não "a normal é zero". Um Exit de contato e
  // um overlap parado não têm direção de superfície para oferecer.
  bool hasNormal = false;
  bool isSensor = false;
};

enum class QueryShapeKind : u32 { Box = 0, Sphere = 1, Capsule = 2, Cylinder = 3 };
struct QueryShapeDesc {
  QueryShapeKind kind = QueryShapeKind::Sphere;
  float halfExtent[3]{.5f, .5f, .5f};
  float radius = .5f;
  float halfHeight = .5f;
  float rotation[4]{0, 0, 0, 1};
};
bool validPhysicsQueryVector(const float *value);
bool validPhysicsQueryRay(const float *origin,const float *translation);
bool validPhysicsQueryShape(const QueryShapeDesc &shape);

// Fase de um contato sólido entregue ao consumidor, no mesmo vocabulário dos
// sensores: 0 Enter, 1 Stay, 2 Exit.
// O movimento de um Personagem bateu num corpo (Unity OnControllerColliderHit):
// uma vez por objeto tocado em cada passo físico com colisão real.
struct CharacterHit {
  ObjectId character=0;u64 instance=0;ObjectId other=0;
  float point[3]{},normal[3]{};
};
struct ContactEvent {
  ObjectId first = kInvalidObject;
  ObjectId second = kInvalidObject;
  u32 phase = 0;
  bool hasNormal = false;
  float normal[3]{};
};

// De onde vem a forma do colisor Malha. A física não conhece o pacote de mapa
// nem o editor: quem monta o mundo entrega os triângulos da malha `mesh` (o
// índice que o MeshRenderer guarda por slot) no referencial do OBJETO que a
// desenha — o mesmo em que o renderer a põe —, 9 floats por triângulo,
// acrescentados ao fim de `out`.
class CollisionGeometrySource {
public:
  virtual ~CollisionGeometrySource() = default;
  virtual bool meshTriangles(u32 mesh, std::vector<float> &out) const = 0;
  // Referência autoral persistente. A implementação resolve o GUID na
  // biblioteca carregada; o índice acima permanece para cenas antigas e para
  // a herança das malhas visuais do MeshRenderer.
  virtual bool meshTriangles(const resources::AssetGuid &, std::vector<float> &) const { return false; }
};

class ScenePhysics final {
public:
  ~ScenePhysics() { stop(); }
  ScenePhysics() = default;
  ScenePhysics(const ScenePhysics &) = delete;
  ScenePhysics &operator=(const ScenePhysics &) = delete;

  // Monta corpos, juntas e personagens a partir do grafo do mundo e registra as
  // autoridades de pose. Falha deixa o mundo sem autoridades e o erro em error().
  // Sem `geometry`, um colisor Malha recusa o início com motivo, em vez de
  // simular sem a forma.
  bool start(GameWorld &gameWorld, const CollisionGeometrySource *geometry = nullptr);
  bool advance(double elapsed, GameWorld &world, bool (*beforeStep)(void *, float) = nullptr,
               void *context = nullptr, bool (*trigger)(void *, ObjectId, ObjectId, u32) = nullptr,
               bool (*contact)(void *, const ContactEvent &) = nullptr,
               bool (*jointBroken)(void *, ObjectId, u64, float) = nullptr,
               bool (*characterHit)(void *, const CharacterHit &) = nullptr);
  // Junta quebrada nesta execução de Play: fora do solver e não recriada pelas
  // reconstruções seguintes; o componente autoral não muda.
  bool jointBroken(ObjectId owner,u64 instance) const;

  // --- consultas ----------------------------------------------------------
  // Todas devolvem identidade de OBJETO, não de corpo nativo: um script nunca
  // precisa saber que existe um handle do Jolt. `rayCastAll` e `overlap`
  // devolvem a contagem REAL de acertos, que pode exceder `capacity` — quem
  // chama decide se repete com um buffer maior, em vez de receber um resultado
  // truncado sem saber.
  bool rayCast(const float origin[3], const float direction[3], const QueryFilter &filter, QueryHit &out) const;
  u32 rayCastAll(const float origin[3], const float direction[3], const QueryFilter &filter,
                 QueryHit *out, u32 capacity) const;
  bool shapeCast(const QueryShapeDesc &shape, const float origin[3], const float direction[3],
                 const QueryFilter &filter, QueryHit &out) const;
  u32 overlap(const QueryShapeDesc &shape, const float origin[3], const QueryFilter &filter,
              QueryHit *out, u32 capacity) const;
  bool applyBodyForce(ObjectId id, const float *value, u32 kind);
  bool getBodyVelocity(ObjectId id, float *out) const;
  WorldStatus bodyCommand(const GameWorld &,ObjectHandle,u64,u32,AetherVec3,AetherVec3,AetherBodyStateV1 &);
  WorldStatus fieldQuery(const GameWorld &,ObjectHandle,u64,u32,const float *,u32,PhysicsFieldSample &) const;
  bool setBodyVelocity(ObjectId id, const float *velocity);
  bool moveKinematic(ObjectId id, const float *pose);
  void stop();
  // Contrato Invalidate::PhysicsBody/PhysicsShape: uma propriedade de corpo,
  // colisor, junta ou personagem mudou em execução, então o solver é montado de
  // novo a partir do grafo atual (pose publicada, valores novos). Corpos móveis
  // continuam com a velocidade linear e angular que tinham, exceto quando a
  // própria velocidade inicial autorada mudou — aí vale o valor pedido. O
  // personagem preserva movimento se instância, cápsula e pose não mudaram.
  bool rebuild(GameWorld &world, const CollisionGeometrySource *geometry);
  bool setCharacterMove(ObjectId id, float right, float forward, float yaw);
  bool setDynamicMotorMove(ObjectId id,float right,float forward,float yaw);
  bool setDynamicMotorScriptMove(ObjectId id,float right,float forward,float yaw);
  bool jumpDynamicMotor(ObjectId id);
  struct DynamicMotorState {bool grounded=false,hasMeasuredStep=false;ObjectId support=0;float normal[3]{0,1,0},supportVelocity[3]{},move[2]{},point[3]{};};
  bool dynamicMotorState(ObjectId id,DynamicMotorState &out) const;
  struct Diagnostic {
    ObjectId object=0,support=0;TransformAuthority authority=TransformAuthority::Free;
    bool hasBody=false,hasCharacter=false,hasSupport=false,motorSupport=false;
    AetherBodyStateV1 body{};physics::CharacterMotor::RuntimeState character{};
    u32 colliderCount=0,probeCount=0;
    u32 shapeCounts[5]{};
    AetherVec3 probes[AetherBodyGroundProbeCapacityV1]{},supportPoint{},supportNormal{};
  };
  // Read-only, explicit inspection. Geometry probes are solver shape support
  // points; an ordinary body's ray hit is a query, not a contact/grounded claim.
  bool diagnostic(const GameWorld &,ObjectId,float probeDistance,Diagnostic &) const;
  // Novo quadro de scripts: descarta comando anterior. Um comando emitido em
  // Update vale para todos os subpassos físicos desse quadro; FixedUpdate pode
  // substituí-lo antes do subpasso corrente.
  void beginScriptInputFrame();
  bool setCharacterScriptMove(ObjectId id,float right,float forward,float yaw);
  bool jumpCharacter(ObjectId id,const GameWorld *world=nullptr);
  WorldStatus characterState(const GameWorld &world,ObjectId id,physics::CharacterMotor::RuntimeState &out) const;
  // Solta corpo, personagem e mapeamento de um objeto removido no ponto seguro.
  // Juntas ligadas a ele deixam de existir junto com o corpo no Jolt.
  void releaseObject(ObjectId id);
  const std::string &error() const { return error_; }
  u32 bodyCount() const { return static_cast<u32>(bindings_.size()); }
  u32 jointCount() const { return jointCount_; }
  AetherPhysicsWorld *world() const noexcept { return world_; }
  // Objeto dono de um corpo nativo, para mapear resultados de consulta de volta
  // à identidade do mundo. Zero quando o corpo não pertence a esta cena.
  ObjectId objectForBody(AetherBodyHandle body) const;

private:
  bool applyDynamicMotors(GameWorld &,float);
  struct DynamicMotorBinding {ObjectId id=0;u64 instance=0;float right=0,forward=0,yaw=0;bool pendingJump=false;DynamicMotorState state{};float scriptRight=0,scriptForward=0,scriptYaw=0;bool scriptMoveActive=false;};
  std::vector<DynamicMotorBinding> dynamicMotors_;
  bool applyPhysicsFields(GameWorld &,float);
  std::vector<std::pair<ObjectId,u64>> fieldCandidates_;
  std::vector<PhysicsFieldFrame> fieldFrames_;
  u64 fieldRevision_=0;
  bool applyContinuousForces(GameWorld &world);
  bool synchronizePoses(GameWorld &world);
  struct Binding {
    ObjectId id;
    AetherBodyHandle body;
    float scale[3];
    bool moving;
    // Instâncias de Colisor na MESMA ordem das partes do composto: é o que
    // transforma "subforma 2" de volta em "o segundo colisor deste objeto".
    struct ColliderIdentity { ObjectId object; u64 instance; };
    std::vector<ColliderIdentity> colliders;
    // Velocidade inicial autorada quando o corpo foi criado; `rebuild` compara
    // com a atual para saber se o pedido foi mudar a velocidade.
    float authoredVelocity[3]{};
    // Interpolação da pose desenhada: pose do passo anterior e do último passo.
    u32 interpolation=0;
    AetherVec3 previousPosition{},currentPosition{};
    AetherQuat previousRotation{0,0,0,1},currentRotation{0,0,0,1};
    bool hasPose=false;
  };
  bool publishBodyPose(GameWorld &world,const Binding &binding,AetherVec3 p,AetherQuat q);
  bool publishInterpolatedPoses(GameWorld &world);
  QueryHit describeHit(AetherBodyHandle body, u32 subShapeId) const;
  AetherPhysicsWorld *world_ = nullptr;
  u32 ownerWorldId_=0;
  std::vector<Binding> bindings_;
  std::unordered_map<AetherBodyHandle, ObjectId> objects_;
  std::vector<AetherTriggerEvent> events_;
  std::vector<AetherContactEventV1> contacts_;
  struct CharacterBinding {
    ObjectId id;
    u64 instance=0;
    float shapeFingerprint[4]{};
    std::unique_ptr<physics::CharacterMotor> motor;
    float world[16];
    float eyeHeight;
    float jumpSpeed = 0;
    float right = 0, forward = 0, yaw = 0;
    float scriptRight=0,scriptForward=0,scriptYaw=0;
    bool scriptMoveActive=false;
  };
  std::vector<CharacterBinding> characters_;
  std::vector<AetherCharacterContactV1> characterContacts_;
  u32 jointCount_ = 0;
  struct JointBinding {ObjectId owner=0;u64 instance=0;AetherJointHandle handle=AetherJointHandle_Invalid;float breakForce=0,breakTorque=0;};
  std::vector<JointBinding> joints_;
  std::vector<std::pair<ObjectId,u64>> brokenJoints_;
  u32 brokenWorldId_=0;
  double accumulated_ = 0;
  std::string error_;
};

} // namespace ae::runtime
