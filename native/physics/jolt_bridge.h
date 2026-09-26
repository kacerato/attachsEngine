// Fronteira C ABI blittable sobre o Jolt Physics (vendorizado em
// native/third_party/JoltPhysics — ver VENDORED_COMMIT.txt para a versão exata).
// Implementa o item 4.1.1 do plano ("Integração do Jolt: mundo, corpos, formas,
// dormência, camadas de colisão") como fatia vertical: um mundo, corpos estático/
// cinemático/dinâmico com forma caixa ou esfera, passo de simulação, leitura de
// transform/velocidade, e um raycast simples. E o item 4.1.3 ("Juntas e motores"):
// Hinge (dobradiça, com motor angular), Slider (pistão/prismática, com motor
// linear), Point (trava um ponto, sem motor) e Distance (mola rígida entre dois
// pontos, sem motor) — as 4 juntas de 2 corpos mais comuns em jogos. SixDOF
// (genérica, 6 eixos independentes) e as juntas de veículo/engrenagem/polia do
// Jolt ficam de fora desta fatia: cobrem casos bem mais raros e cada uma merece
// sua própria fatia testada, não um apêndice apressado aqui. E o item 4.1.4
// ("Queries"): RayCastAll (multi-hit), ShapeCastClosest (varredura), OverlapShape
// (overlap parado) — complementam o raycast simples de 4.1.1. E o item 4.1.5
// ("Character controller"): sobre JPH::CharacterVirtual — mover, detecção de
// chão/rampa/parede, degraus (ExtendedUpdate/WalkStairs), deslizar em rampa
// íngreme, agachar (SetShape com checagem de espaço), e o dado necessário para o
// chamador "grudar" numa plataforma móvel (GetGroundVelocity/GetGroundBodyID).
// Escalar (subir paredes) e nadar (buoyancy) ficam de fora desta fatia — o Jolt
// não tem NENHUM suporte nativo para nenhum dos dois; cada um seria um subsistema
// próprio construído do zero sobre queries manuais, não uma extensão natural desta
// fatia — ver docs/ESTADO.md. Decomposição convexa e determinismo em ponto fixo
// (itens 4.1.7, 4.1.8) também ficam para incrementos futuros.
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
  // Capsule (item 4.1.5): a forma padrão de character controller — cilindro com
  // tampas esféricas, sem quinas para travar em degraus/rampas. Adicionada aqui
  // (não como um enum de forma separado só para personagem) porque AetherShapeDesc
  // já é genérico o bastante e ShapeCastClosest/OverlapShape (4.1.4) também passam
  // a poder variar cápsulas de graça, sem mudança de ABI adicional.
  Capsule = 2,
};

struct AetherShapeDesc {
  AetherShapeKind kind;
  AetherVec3 boxHalfExtent;  // usado quando kind == Box
  float sphereRadius;        // usado quando kind == Sphere ou Capsule (raio da cápsula)
  float capsuleHalfHeight;   // usado quando kind == Capsule — altura do CILINDRO (sem as tampas); altura total = 2*(capsuleHalfHeight+sphereRadius)
};

// Graus de liberdade permitidos a um corpo dinâmico/cinemático — espelha JPH::EAllowedDOFs
// (item 4.1.6, física 2D). AVISO DE CONVENÇÃO: aqui All = 0 (não 0b111111 como no Jolt) de
// propósito — um AetherBodyDesc{} zero-inicializado (padrão comum nos testes e em qualquer
// struct C# default) precisa continuar significando "corpo 3D normal, sem restrição", não
// "todos os eixos travados" (que o próprio Jolt documenta como inválido — crasha por divisão
// por zero num corpo Dynamic, ver MotionProperties::SetMassProperties). A conversão para
// JPH::EAllowedDOFs na implementação inverte isso: All aqui (0) -> All do Jolt (0b111111).
enum class AetherAllowedDOFs : ae::u32 {
  All = 0,
  TranslationX = 1u << 0,
  TranslationY = 1u << 1,
  TranslationZ = 1u << 2,
  RotationX = 1u << 3,
  RotationY = 1u << 4,
  RotationZ = 1u << 5,
  // Plano XY do Jolt (mão-esquerda, Y-para-cima na convenção da engine — ver
  // CONVENCOES.md §5): trava profundidade (Z) e as duas rotações que tirariam o corpo do
  // plano da tela (X, Y), deixando livre translação em X/Y e giro em torno de Z (rotação "na
  // tela"). É o caso de uso mais comum de física 2D (plataforma 2D vista de lado/de frente).
  // Para "visto de cima" (top-down), monte a combinação manualmente
  // (TranslationX|TranslationZ|RotationY) — não há constante pronta para isso, mesma
  // limitação do próprio JPH::EAllowedDOFs::Plane2D, que também só cobre o caso lateral.
  Plane2D = TranslationX | TranslationY | RotationZ,
};

struct AetherBodyDesc {
  AetherShapeDesc shape;
  AetherVec3 position;
  AetherQuat rotation;
  AetherMotionType motionType;
  float friction;    // [0,1], padrão razoável: 0.5
  float restitution; // [0,1], padrão razoável: 0.0 (sem quique)
  AetherAllowedDOFs allowedDOFs; // AetherAllowedDOFs::All (== 0) por padrão — ver comentário acima
};

// ABI de criação de corpo versionada (GAP-PHY-03). A V1 permanece congelada:
// aumentar AetherBodyDesc quebraria callers já compilados que passam a struct
// por ponteiro. eventLayerMask usa os bits de AetherQueryLayerMask (Static=1,
// Dynamic=2); fica como u32 aqui porque o enum é declarado na seção de queries
// abaixo e PODs de fronteira não precisam carregar dependências de ordem.
constexpr ae::u32 AetherBodyApiVersionV2 = 2;

struct AetherBodyDescV2 {
  ae::u32 structSize;
  ae::u32 apiVersion;
  AetherShapeDesc shape;
  AetherVec3 position;
  AetherQuat rotation;
  AetherMotionType motionType;
  float friction;
  float restitution;
  AetherAllowedDOFs allowedDOFs;
  ae::u32 isSensor;       // 0 = sólido; qualquer outro valor = sensor sem resposta física
  ae::u32 eventLayerMask; // bits AetherQueryLayerMask; All (3) é o default da fachada C#
};

// Additive ABI: each part is in the authored body frame, with scale baked into dimensions.
// No pointers survive creation. Structure versions are independent of the frozen body V2.
struct AetherCompoundPart {
  AetherShapeDesc shape;
  AetherVec3 position;
  AetherQuat rotation;
};
struct AetherBodyDynamicsV1 {
  ae::u32 structSize;
  ae::u32 apiVersion;
  float linearDamping;
  float angularDamping;
  float gravityFactor;
  AetherVec3 angularVelocity;
  ae::u32 allowSleeping;
};

// Opaco de propósito — o layout real (PhysicsSystem, alocador temporário, job
// system, filtros de camada) vive só em jolt_bridge.cpp.
struct AetherPhysicsWorld;

// ABI V2 (GAP-PHY-01): os quatro limites representam recursos diferentes do
// Jolt e nunca devem voltar a ser derivados implicitamente de maxBodies.
// maxBroadPhasePairs configura PhysicsSettings::mMaxInFlightBodyPairs, isto é,
// o buffer temporário de candidatos produzidos pela broad phase antes da narrow
// phase. Todos os descritores versionados começam com structSize/apiVersion para
// permitir acrescentar campos no final sem reinterpretar layouts antigos.
constexpr ae::u32 AetherPhysicsWorldApiVersionV2 = 2;

enum class AetherPhysicsOverflowPolicy : ae::u32 {
  // FailFast em Debug/teste; Warning em Release. É o default recomendado para
  // que desenvolvimento nunca continue após perda de contatos, enquanto uma
  // build de usuário preserva o processo e mantém diagnóstico/contadores.
  BuildDefault = 0,
  // Em build Debug o próprio Jolt possui um assert interno anterior ao retorno
  // de Update, portanto até Warning termina imediatamente — exatamente a regra
  // de teste do plano. Esta opção controla o comportamento recuperável Release.
  Warning = 1,
  FailFast = 2,
};

struct AetherPhysicsWorldDescV2 {
  ae::u32 structSize;
  ae::u32 apiVersion;
  AetherVec3 gravity;
  ae::u32 maxBodies;
  ae::u32 maxBodyPairs;
  ae::u32 maxContactConstraints;
  ae::u32 maxBroadPhasePairs;
  AetherPhysicsOverflowPolicy overflowPolicy;
};

enum class AetherPhysicsUpdateError : ae::u32 {
  None = 0,
  ManifoldCacheFull = 1u << 0,
  BodyPairCacheFull = 1u << 1,
  ContactConstraintsFull = 1u << 2,
};

struct AetherPhysicsStepStatsV2 {
  ae::u32 structSize;
  ae::u32 apiVersion;
  ae::u64 totalSteps;
  ae::u64 overflowSteps;
  ae::u64 manifoldCacheFullCount;
  ae::u64 bodyPairCacheFullCount;
  ae::u64 contactConstraintsFullCount;
  ae::u32 lastErrorFlags;
  ae::u32 reserved;
};

// Handle denso: é o próprio JPH::BodyID (índice + número de sequência
// empacotados por Jolt) reinterpretado como uint32 — não precisamos de uma
// tabela de handles nossa, o Jolt já resolve reciclagem de índice sozinho.
using AetherBodyHandle = ae::u32;
constexpr AetherBodyHandle AetherBodyHandle_Invalid = 0xFFFFFFFFu;

/// Símbolo V1 preservado por compatibilidade. Internamente é mapeado para V2
/// com limites conservadores separados; código novo deve usar CreateWorldV2.
AetherPhysicsWorld *AetherPhysics_CreateWorld(AetherVec3 gravity, ae::u32 maxBodies);

/// Cria um mundo a partir do descritor versionado. Devolve nullptr e emite um
/// diagnóstico se versão, tamanho, limites ou política forem inválidos.
AetherPhysicsWorld *AetherPhysics_CreateWorldV2(const AetherPhysicsWorldDescV2 *desc);

/// Destrói o mundo e TODOS os corpos nele — nenhum AetherBodyHandle deste
/// mundo continua válido depois desta chamada.
void AetherPhysics_DestroyWorld(AetherPhysicsWorld *world);

/// Cria um corpo e já o adiciona ao mundo (ativo, se dinâmico). Devolve
/// AetherBodyHandle_Invalid se o mundo já está no teto de `maxBodies`, OU (item 4.1.6) se
/// `motionType == Dynamic` e `allowedDOFs` não deixa nenhum eixo de translação livre — essa
/// combinação é inválida no Jolt (crasha por divisão por zero em MotionProperties::
/// SetMassProperties; um corpo totalmente travado deveria ser Static, não Dynamic com todos
/// os DOFs travados) e esta fronteira recusa a criação em vez de deixar o processo abortar.
AetherBodyHandle AetherPhysics_CreateBody(AetherPhysicsWorld *world, const AetherBodyDesc *desc);

/// Cria um único corpo estático de triangle mesh a partir de posições e índices
/// já cozidos. A função copia/otimiza os dados no MeshShape do Jolt; os buffers
/// do chamador podem ser liberados após o retorno. Destinado a colisão de mundo
/// estático importada, nunca a meshes dinâmicas. Índices devem formar triângulos.
AetherBodyHandle AetherPhysics_CreateStaticTriangleMesh(
    AetherPhysicsWorld *world, const AetherVec3 *vertices, ae::u32 vertexCount,
    const ae::u32 *indices, ae::u32 indexCount, float friction);

/// Variante versionada que expõe sensor/filtro sem alterar o layout V1. Rejeita
/// versão, tamanho e bits de filtro desconhecidos devolvendo Invalid.
AetherBodyHandle AetherPhysics_CreateBodyV2(AetherPhysicsWorld *world, const AetherBodyDescV2 *desc);

/// Cria um lote de corpos com uma única travessia de ABI e uma única inserção
/// ampla na broadphase. Semântica transacional: ou todos os `count` corpos são
/// criados/adicionados e o retorno é `count`, ou nenhum permanece vivo, todos
/// os outHandles ficam Invalid e o retorno é 0. Arrays podem ser nulos somente
/// quando count == 0.
// Additional entry point preserves V1/V2 layouts. Center is shape-local after
// scale, relative to the authored body origin; body pose APIs keep that origin.
AetherBodyHandle AetherPhysics_CreateBodyWithLocalCenterV2(AetherPhysicsWorld *world,
    const AetherBodyDescV2 *desc,AetherVec3 localCenter);

// Creates one body from 1..256 primitive parts. V2 desc.shape is unused here.
AetherBodyHandle AetherPhysics_CreateCompoundBodyV1(AetherPhysicsWorld *world,
    const AetherBodyDescV2 *desc,const AetherCompoundPart *parts,ae::u32 count,
    const AetherBodyDynamicsV1 *dynamics);

// Geometria de uma parte de composto (Mesh Collider da Unity). `Primitive` usa
// `base.shape`; as outras duas usam os vértices, já no referencial da parte e com
// a escala aplicada. Os ponteiros só precisam viver durante a chamada.
//
// - `ConvexHull` (Convex ligado): casco convexo dos vértices; índices ignorados.
//   Serve a qualquer tipo de movimento.
// - `TriangleMesh` (Convex desligado): os triângulos exatos. Só em corpo estático
//   ou cinemático — uma malha arbitrária não tem volume, logo não tem massa, e o
//   Jolt não resolve malha contra malha. É a mesma recusa da Unity para
//   MeshCollider não convexo em Rigidbody não cinemático.
enum class AetherPartGeometry : ae::u32 { Primitive = 0, ConvexHull = 1, TriangleMesh = 2 };
struct AetherCompoundPartV2 {
  AetherCompoundPart base;
  AetherPartGeometry geometry;
  const AetherVec3 *vertices;
  ae::u32 vertexCount;
  const ae::u32 *indices;
  ae::u32 indexCount;
};
// Mesmo contrato da V1 (1..256 partes, o dado de usuário da subforma é o índice
// da parte), com partes de malha. Devolve Invalid para malha não convexa em corpo
// dinâmico, geometria vazia/não finita, índice fora do intervalo ou casco
// degenerado (malha plana não tem casco sólido).
AetherBodyHandle AetherPhysics_CreateCompoundBodyV2(AetherPhysicsWorld *world,
    const AetherBodyDescV2 *desc,const AetherCompoundPartV2 *parts,ae::u32 count,
    const AetherBodyDynamicsV1 *dynamics);

// Opções de cooking consumidas pelo backend. O layout versionado mantém V2
// congelada e permite que editor, runtime e futuros bindings usem a mesma
// semântica sem conhecer tipos do Jolt.
enum AetherMeshCookingFlags : ae::u32 {
  AetherMeshCookingOptimizeRuntime = 1u << 0
};
struct AetherMeshCookingV1 {
  ae::u32 structSize;
  ae::u32 apiVersion;
  ae::u32 flags;
  float hullTolerance;
  float activeEdgeAngleDegrees;
};
inline constexpr AetherMeshCookingV1 AetherMeshCookingDefaultsV1{
  sizeof(AetherMeshCookingV1),1,AetherMeshCookingOptimizeRuntime,0.001f,5.0f
};
struct AetherCompoundPartV3 {
  AetherCompoundPart base;
  AetherPartGeometry geometry;
  const AetherVec3 *vertices;
  ae::u32 vertexCount;
  const ae::u32 *indices;
  ae::u32 indexCount;
  AetherMeshCookingV1 cooking;
};
// V3 aplica qualidade da árvore de busca, tolerância do casco e limiar de
// arestas ativas. V2 continua disponível e encaminha os padrões acima.
AetherBodyHandle AetherPhysics_CreateCompoundBodyV3(AetherPhysicsWorld *world,
    const AetherBodyDescV2 *desc,const AetherCompoundPartV3 *parts,ae::u32 count,
    const AetherBodyDynamicsV1 *dynamics);

ae::i32 AetherPhysics_CreateBodiesV2(AetherPhysicsWorld *world,
                                     const AetherBodyDescV2 *descs,
                                     AetherBodyHandle *outHandles,
                                     ae::i32 count);

/// Remove e destrói um corpo. Handle passa a ser inválido depois desta chamada.
void AetherPhysics_DestroyBody(AetherPhysicsWorld *world, AetherBodyHandle handle);

/// Remove/destrói um lote numa única operação de broadphase. Handles Invalid
/// são ignorados; os demais seguem o mesmo contrato de validade da API unitária.
void AetherPhysics_DestroyBodies(AetherPhysicsWorld *world,
                                 const AetherBodyHandle *handles,
                                 ae::i32 count);

/// Avança a simulação. `collisionSteps` segue a mesma convenção do Jolt: 1 é o
/// caso comum para `deltaTime` de até 1/60s; passos maiores exigem mais de 1
/// para manter a simulação estável (ver comentário no HelloWorld do Jolt).
void AetherPhysics_Step(AetherPhysicsWorld *world, float deltaTime, ae::i32 collisionSteps);

/// Variante V2: devolve a máscara AetherPhysicsUpdateError produzida pelo
/// Jolt. O símbolo V1 continua chamável, mas também registra/contabiliza os
/// mesmos erros antes de descartar apenas o valor de retorno para preservar ABI.
ae::u32 AetherPhysics_StepV2(AetherPhysicsWorld *world, float deltaTime, ae::i32 collisionSteps);

enum class AetherTriggerEventType : ae::u32 {
  Enter = 0,
  Stay = 1,
  Exit = 2,
};

struct AetherTriggerEvent {
  AetherBodyHandle sensor;
  AetherBodyHandle other;
  AetherTriggerEventType type;
  ae::u32 reserved;
};

/// Copia a fotografia de eventos do último Step completo. O retorno é a
/// quantidade REAL; se exceder maxResults, só o prefixo determinístico cabe no
/// buffer, mas o chamador detecta truncamento pelo retorno. A fotografia não é
/// consumida pela leitura e é substituída no próximo Step.
ae::i32 AetherPhysics_GetTriggerEvents(AetherPhysicsWorld *world,
                                       AetherTriggerEvent *outEvents,
                                       ae::i32 maxResults);

/// Copia os contadores cumulativos do mundo. O chamador deve inicializar
/// structSize e apiVersion; devolve 1 em sucesso, 0 em argumento/versão inválido.
ae::i32 AetherPhysics_GetStepStatsV2(const AetherPhysicsWorld *world, AetherPhysicsStepStatsV2 *outStats);

/// Lê a transform atual do corpo (posição do centro de massa + rotação).
/// Ponteiros de saída nulos são ignorados individualmente — chamador pode
/// pedir só posição, só rotação, ou as duas.
void AetherPhysics_GetTransform(AetherPhysicsWorld *world, AetherBodyHandle handle,
                                 AetherVec3 *outPosition, AetherQuat *outRotation);

void AetherPhysics_SetLinearVelocity(AetherPhysicsWorld *world, AetherBodyHandle handle, AetherVec3 velocity);
// World-space vectors. Force/torque accumulate until Step; impulses act immediately.
// Commands require a live dynamic body. Handles are checked under the world lock.
enum class AetherBodyForceKind : ae::u32 { Force=0, Impulse=1, Torque=2, AngularImpulse=3 };
ae::i32 AetherPhysics_ApplyBodyForceV1(AetherPhysicsWorld *world,AetherBodyHandle handle,AetherVec3 value,AetherBodyForceKind kind);
ae::i32 AetherPhysics_TryGetBodyVelocityV1(AetherPhysicsWorld *world,AetherBodyHandle handle,AetherVec3 *out);
// Velocidade angular em rad/s, espaço do mundo. Leitura sob o lock do corpo;
// a escrita recusa corpo estático e valor não finito. Usadas para recriar um
// corpo no solver sem parar o que ele estava fazendo.
ae::i32 AetherPhysics_TryGetBodyAngularVelocityV1(AetherPhysicsWorld *world,AetherBodyHandle handle,AetherVec3 *out);
ae::i32 AetherPhysics_SetBodyAngularVelocityV1(AetherPhysicsWorld *world,AetherBodyHandle handle,AetherVec3 value);
ae::i32 AetherPhysics_SetMassV2(AetherPhysicsWorld *world,AetherBodyHandle handle,float mass);
// Body-origin pose (not centre of mass). A stale/destroyed handle returns zero
// and leaves outputs untouched; read under one body lock.
ae::i32 AetherPhysics_TryGetBodyPoseV2(AetherPhysicsWorld *world, AetherBodyHandle handle,
                                      AetherVec3 *position, AetherQuat *rotation);
// Explicit per-body energy policy. Does not implicitly wake a sleeping body.
ae::i32 AetherPhysics_SetAllowSleepingV2(AetherPhysicsWorld *world, AetherBodyHandle handle,
                                         ae::u32 allowed);
AetherVec3 AetherPhysics_GetLinearVelocity(AetherPhysicsWorld *world, AetherBodyHandle handle);

/// Move um corpo cinemático até o alvo durante deltaTime, gerando velocidades
/// linear/angular físicas para que corpos e characters apoiados sejam
/// transportados pelo solver. Devolve 1 em sucesso; 0 para mundo/handle
/// inválido, deltaTime não positivo ou corpo que não seja Kinematic.
ae::i32 AetherPhysics_MoveKinematicV2(AetherPhysicsWorld *world, AetherBodyHandle handle,
                                      AetherVec3 targetPosition, AetherQuat targetRotation,
                                      float deltaTime);

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

// ---------------------------------------------------------------- queries (4.1.4)
//
// Multi-hit (RayCastAll), shapecast (ShapeCastClosest) e overlap parado (OverlapShape) —
// as três formas de query que RayCastClosest sozinho não cobre. O Jolt entrega múltiplos
// hits via um "collector" (padrão callback/visitor, ver JPH::CollisionCollector); a
// fronteira C ABI não pode devolver um std::vector, então o chamador fornece um buffer
// (outBodies/outFractions, tamanho maxResults) e recebe de volta quantos hits couberam —
// mesmo padrão de "buffer do chamador" que qualquer API C nativa usa para evitar alocação
// do lado nativo que o C# teria que liberar depois.
//
// AetherQueryLayerMask filtra por camada de colisão: hoje o mundo só tem duas camadas
// (estático/dinâmico-cinemático, ver Layers::NonMoving/Moving em jolt_bridge.cpp) — os bits
// abaixo mapeiam 1:1 para essas duas, combináveis com OR bit a bit. Um esquema de N camadas
// nomeadas (o que o plano pede no plural, "camadas de colisão", item 4.1.1) continua sendo
// trabalho futuro; isto é o filtro simples que o esquema atual de 2 camadas já suporta sem
// mudança de ABI dos corpos existentes.
enum class AetherQueryLayerMask : ae::u32 {
  None = 0,
  Static = 1u << 0,
  Dynamic = 1u << 1,
  All = Static | Dynamic,
};

/// Resultado rico de uma query de shapecast/overlap — RayCastAll usa só body+fraction (não
/// tem ponto/normal de contato disponível no Jolt sem uma segunda chamada, ver
/// Body::GetWorldSpaceSurfaceNormal — fora do escopo desta fatia), mas ShapeCast/Overlap
/// devolvem contato de verdade (JPH::CollideShapeResult), então valem uma struct própria.
struct AetherShapeQueryHit {
  AetherBodyHandle body;
  float fraction;              // só significativo em ShapeCastClosest (0 em OverlapShape — não há "ao longo de quê")
  AetherVec3 contactPointOnQuery;   // ponto de contato no shape de CONSULTA (o que você está varrendo/sobrepondo)
  AetherVec3 contactPointOnHit;     // ponto de contato no corpo ACERTADO
  AetherVec3 penetrationAxis;       // direção de menor penetração/separação — não normalizada (convenção do Jolt)
};

/// Raycast que acerta TODOS os corpos ao longo do segmento, não só o mais próximo.
/// outBodies/outFractions são paralelos (mesmo índice = mesmo hit), tamanho maxResults cada
/// (buffer do CHAMADOR — esta função nunca aloca do lado nativo). Devolve a contagem real de
/// hits encontrados, que pode ser MAIOR que maxResults (o excesso é descartado, não é erro —
/// mesma convenção de `snprintf`: o chamador decide se quer chamar de novo com buffer maior
/// olhando o valor de retorno). layerMask filtra por camada antes mesmo de testar contra
/// cada corpo — corpos fora da máscara nunca entram no resultado nem contam para a
/// contagem devolvida.
ae::i32 AetherPhysics_RayCastAll(AetherPhysicsWorld *world, AetherVec3 origin, AetherVec3 direction,
                                  AetherQueryLayerMask layerMask, AetherBodyHandle ignoreBody,
                                  AetherBodyHandle *outBodies, float *outFractions, ae::i32 maxResults);

/// Varre uma forma (a mesma AetherShapeDesc usada para criar corpos — Box ou Sphere) do
/// ponto `origin` ao longo de `direction` (comprimento = alcance, mesma convenção de
/// RayCastClosest) e devolve o hit MAIS PRÓXIMO ao longo do caminho. `rotation` é a
/// orientação da forma de consulta (não muda durante a varredura — o Jolt não modela rotação
/// progressiva num shapecast). Devolve 1 se algo foi acertado.
ae::i32 AetherPhysics_ShapeCastClosest(AetherPhysicsWorld *world, const AetherShapeDesc *shape,
                                        AetherVec3 origin, AetherQuat rotation, AetherVec3 direction,
                                        AetherQueryLayerMask layerMask, AetherBodyHandle ignoreBody,
                                        AetherShapeQueryHit *outHit);

/// Quais corpos sobrepõem uma forma PARADA (sem movimento) numa posição/rotação dadas — o
/// "trigger volume" mais comum em jogos (ex.: zona de detecção). outHits/maxResults segue o
/// mesmo padrão de buffer do chamador de RayCastAll. Devolve a contagem real de hits (pode
/// exceder maxResults, mesma convenção).
ae::i32 AetherPhysics_OverlapShape(AetherPhysicsWorld *world, const AetherShapeDesc *shape,
                                    AetherVec3 origin, AetherQuat rotation,
                                    AetherQueryLayerMask layerMask, AetherBodyHandle ignoreBody,
                                    AetherShapeQueryHit *outHits, ae::i32 maxResults);

// --------------------------------------------------- queries com contato real (V2)
//
// As funções acima devolvem corpo e fração; um raycast de gameplay precisa também
// de ponto e NORMAL de superfície, e de saber se acertou um sensor. O Jolt não
// entrega normal no RayCastResult: ela vem de uma segunda consulta ao corpo
// acertado (Body::GetWorldSpaceSurfaceNormal com o sub-shape do hit). É isso que
// as funções V2 fazem — devolver zero no lugar da normal seria entregar um valor
// que parece contato e não é.

struct AetherQueryFilterV1 {
  ae::u32 structSize;
  ae::u32 apiVersion;
  AetherQueryLayerMask layerMask;
  AetherBodyHandle ignoreBody;
  /// 0 descarta corpos sensores do resultado; 1 os inclui, marcados em `isSensor`.
  ae::u32 includeSensors;
  /// Bit por camada de gameplay (0..31). Todos ligados aceita qualquer camada.
  ae::u32 gameplayLayerMask;
};
inline constexpr ae::u32 AetherQueryFilterApiVersionV1 = 1;

struct AetherRayQueryHitV1 {
  AetherBodyHandle body;
  /// Sub-shape acertado dentro de um corpo composto. É o que permite ao chamador
  /// distinguir qual parte da composição respondeu.
  ae::u32 subShapeId;
  float fraction;
  AetherVec3 point;
  /// Normal de superfície no ponto, em espaço de mundo, já normalizada pelo Jolt.
  AetherVec3 normal;
  ae::u32 isSensor;
  ae::u32 reserved;
};

/// Raio mais próximo com ponto, normal e sub-shape. Devolve 1 quando acertou.
/// `direction` não é normalizado: seu comprimento é o alcance (convenção do Jolt).
/// Raio de comprimento zero é recusado (devolve 0) em vez de produzir um hit
/// degenerado com fração indefinida.
ae::i32 AetherPhysics_RayCastClosestV2(AetherPhysicsWorld *world, AetherVec3 origin, AetherVec3 direction,
                                       const AetherQueryFilterV1 *filter, AetherRayQueryHitV1 *outHit);

/// Todos os hits ao longo do raio, ordenados do mais próximo ao mais distante.
/// Buffer do chamador; o retorno é a contagem REAL e pode exceder maxResults.
ae::i32 AetherPhysics_RayCastAllV2(AetherPhysicsWorld *world, AetherVec3 origin, AetherVec3 direction,
                                   const AetherQueryFilterV1 *filter,
                                   AetherRayQueryHitV1 *outHits, ae::i32 maxResults);

/// Varredura e sobreposição com o mesmo filtro das V2. O hit continua sendo o
/// AetherShapeQueryHit (ponto nos dois corpos + eixo de penetração), acrescido do
/// sub-shape acertado e do sinalizador de sensor em buffers paralelos opcionais.
ae::i32 AetherPhysics_ShapeCastClosestV2(AetherPhysicsWorld *world, const AetherShapeDesc *shape,
                                         AetherVec3 origin, AetherQuat rotation, AetherVec3 direction,
                                         const AetherQueryFilterV1 *filter, AetherShapeQueryHit *outHit,
                                         ae::u32 *outSubShapeId, ae::u32 *outIsSensor);
ae::i32 AetherPhysics_OverlapShapeV2(AetherPhysicsWorld *world, const AetherShapeDesc *shape,
                                     AetherVec3 origin, AetherQuat rotation,
                                     const AetherQueryFilterV1 *filter, AetherShapeQueryHit *outHits,
                                     ae::u32 *outSubShapeIds, ae::u32 *outSensorFlags, ae::i32 maxResults);

// ------------------------------------------------- camadas de gameplay (V1)
//
// As camadas amplas do Jolt (NonMoving/Moving) continuam existindo e continuam
// decidindo o que a broadphase precisa parear. Por cima delas, cada corpo
// carrega uma CAMADA DE GAMEPLAY nomeada pelo projeto, e a matriz abaixo decide
// quais pares de camadas chegam a colidir. A matriz vale no solver, não só nas
// consultas: um par proibido nunca gera contato, em vez de gerar e ser
// descartado depois. Um projeto que nunca configura nada tem todas as camadas
// interagindo, que é o comportamento anterior a este recurso.

inline constexpr ae::u32 AetherPhysicsGameplayLayerCount = 32;

/// Substitui a matriz de interação do mundo. `matrix[a]` é a máscara de camadas
/// com que a camada `a` colide. A matriz PRECISA ser recíproca — se a colide com
/// b, b colide com a — porque o Jolt consulta o par uma vez só, em ordem não
/// especificada; uma matriz assimétrica é recusada (devolve 0) em vez de
/// produzir colisão que depende da ordem de criação dos corpos.
ae::i32 AetherPhysics_SetLayerInteractionV1(AetherPhysicsWorld *world, const ae::u32 *matrix, ae::u32 count);

/// Move um corpo já criado para outra camada de gameplay. Devolve 1 em sucesso;
/// 0 para mundo/handle inválido ou camada fora de [0, 32).
ae::i32 AetherPhysics_SetBodyGameplayLayerV1(AetherPhysicsWorld *world, AetherBodyHandle body, ae::u32 layer);

/// A camada de gameplay atual do corpo, ou 0xffffffff quando o handle não vale.
ae::u32 AetherPhysics_GetBodyGameplayLayerV1(const AetherPhysicsWorld *world, AetherBodyHandle body);

/// Dado de usuário da subforma acertada por uma query. `AetherPhysics_CreateCompoundBodyV1`
/// e a V2 gravam nele o ÍNDICE da parte na ordem em que foi passada (na forma
/// folha: é ela que o Jolt devolve, não o dado da entrada do composto), então é por aqui que
/// um chamador volta de "acertei o corpo X, subforma S" para "acertei o colisor
/// que eu mesmo montei na posição N". Devolve 1 em sucesso.
ae::i32 AetherPhysics_GetSubShapeUserDataV1(AetherPhysicsWorld *world, AetherBodyHandle body,
                                            ae::u32 subShapeId, ae::u64 *outUserData);

// ------------------------------------------------------- contatos sólidos (V1)
//
// `AetherTriggerEvent` só existe para pares em que um dos corpos é SENSOR. Um
// contato sólido — a caixa que encosta no chão, o personagem que bate na porta —
// nunca aparecia ali. Estes eventos são a outra metade: mesmo agrupamento por
// par de corpos e por passo, mesma fotografia estável, e a normal do manifold
// quando o Jolt a fornece.
enum class AetherContactEventType : ae::u32 { Enter = 0, Stay = 1, Exit = 2 };

struct AetherContactEventV1 {
  AetherBodyHandle first;
  AetherBodyHandle second;
  AetherContactEventType type;
  /// 1 quando `normal` foi preenchida. O Jolt não informa geometria em
  /// OnContactRemoved, então um Exit chega SEM normal — e isso é dito, não
  /// disfarçado com um vetor zero que pareceria um contato de frente.
  ae::u32 hasNormal;
  /// Direção ao longo da qual mover `second` para fora de `first`, em mundo.
  AetherVec3 normal;
};

/// Fotografia dos contatos sólidos do último Step. Mesma convenção de
/// truncamento de AetherPhysics_GetTriggerEvents: o retorno é a contagem real.
ae::i32 AetherPhysics_GetContactEventsV1(AetherPhysicsWorld *world,
                                         AetherContactEventV1 *outEvents,
                                         ae::i32 maxResults);

// ---------------------------------------------------------------- juntas e motores (4.1.3)

enum class AetherJointKind : ae::u32 {
  Point = 0,
  Hinge = 1,
  Slider = 2,
  Distance = 3,
};

/// Estado de motor — mesmo conjunto de JPH::EMotorState, reinterpretado como uint32 na
/// fronteira (ver AetherMotionType acima pelo mesmo motivo). Só Hinge e Slider têm motor;
/// Point e Distance ignoram este campo (não há JPH::MotorSettings nelas — ver
/// docs/ESTADO.md, seção de juntas, para por que não é um "TODO", é a API real do Jolt).
enum class AetherMotorState : ae::u32 {
  Off = 0,
  Velocity = 1,
  Position = 2,
  PositionAndVelocity = 3,
};

/// Motor de uma junta: alvo (posição OU velocidade, a depender de `state`) mais os limites
/// de força/torque que o motor pode aplicar para chegar lá. `springFrequency`/`springDamping`
/// alimentam JPH::SpringSettings no modo FrequencyAndDamping (o mais intuitivo de expor:
/// "oscila N vezes por segundo, com este amortecimento" em vez de rigidez crua em N/m) —
/// usados só quando `state` inclui Position.
struct AetherJointMotorDesc {
  AetherMotorState state;
  float targetVelocity;       // rad/s (Hinge) ou m/s (Slider)
  float targetPosition;       // rad (Hinge) ou m (Slider) — clampado aos limites da junta pelo Jolt
  float maxForceOrTorque;     // N (Slider) ou N*m (Hinge); limite simétrico [-max, +max]
  float springFrequency;      // Hz, > 0. Ignorado se state não incluir Position.
  float springDamping;        // [0,1] tipicamente; 0 = sem amortecimento, 1 = crítico.
};

/// Descreve uma junta a ser criada entre dois corpos já existentes no mesmo mundo.
/// `point1`/`axis1`/`normal1` são no referencial do corpo 1 (mundo, sempre — esta fatia só
/// expõe EConstraintSpace::WorldSpace; espaço local ao corpo é um incremento futuro),
/// `point2`/`axis2`/`normal2` no referencial do corpo 2 — mesma convenção do Jolt
/// (HingeConstraintSettings::mPoint1/mPoint2 etc.), para Point e Distance só `point1`/
/// `point2` importam. `axis1`/`axis2` é o eixo da dobradiça (Hinge) ou de deslizamento
/// (Slider); ignorado por Point/Distance. `limitsMin`/`limitsMax` seguem a mesma convenção
/// de HingeConstraintSettings/SliderConstraintSettings (radianos ou metros conforme o tipo);
/// para Distance são a distância mínima/máxima permitida (negativo = auto-detectar da
/// distância inicial entre os corpos, mesma convenção de JPH::DistanceConstraintSettings).
/// PARA HINGE ESPECIFICAMENTE: `limitsMin`/`limitsMax` são exigidos pelo Jolt dentro de
/// [-pi,0]/[0,pi] (assert em HingeConstraint.cpp — não é validação nossa, é a API real);
/// para uma dobradiça SEM limite, capaz de girar livremente como uma roda (útil com motor
/// de velocidade contínua), use exatamente limitsMin=-pi e limitsMax=+pi — é o valor exato
/// em que o Jolt desliga a checagem de limite internamente (HingeConstraint::SetLimits:
/// `mHasLimits = mLimitsMin > -pi || mLimitsMax < pi`), não um "limite muito largo". Slider
/// não tem essa restrição de faixa — FLT_MAX de fato significa "sem limite" ali.
struct AetherJointDesc {
  AetherJointKind kind;
  AetherVec3 point1;
  AetherVec3 point2;
  AetherVec3 axis1;
  AetherVec3 axis2;
  float limitsMin;
  float limitsMax;
  AetherJointMotorDesc motor;
};

/// Referencial no qual todos os pontos/eixos do descritor V2 são expressos. O Jolt só
/// oferece WorldSpace ou LocalToBodyCOM por constraint; expor LocalToBody1/2 aqui é mais
/// previsível para autoria: a fronteira converte ponto (posição + rotação) e eixo (somente
/// rotação) pelo transform do corpo de referência e cria a constraint em WorldSpace.
enum class AetherJointSpace : ae::u32 {
  World = 0,
  LocalToBody1 = 1,
  LocalToBody2 = 2,
};

/// ABI versionada da junta. A V1 acima permanece congelada e equivale a Space::World.
/// structSize/apiVersion permitem acrescentar campos ao final sem reinterpretar callers
/// compilados contra o layout anterior.
constexpr ae::u32 AetherJointApiVersionV2 = 2;

struct AetherJointDescV2 {
  ae::u32 structSize;
  ae::u32 apiVersion;
  AetherJointKind kind;
  AetherJointSpace space;
  AetherVec3 point1;
  AetherVec3 point2;
  AetherVec3 axis1;
  AetherVec3 axis2;
  float limitsMin;
  float limitsMax;
  AetherJointMotorDesc motor;
};

using AetherJointHandle = ae::u32;
constexpr AetherJointHandle AetherJointHandle_Invalid = 0xFFFFFFFFu;

/// Cria uma junta entre dois corpos do MESMO mundo e já a adiciona à simulação. Devolve
/// AetherJointHandle_Invalid se algum dos dois handles de corpo for inválido/de outro mundo,
/// ou se `desc` for nulo. Ao contrário de AetherBodyHandle (que é o próprio JPH::BodyID
/// reinterpretado — o Jolt já resolve reciclagem de índice sozinho), uma constraint do Jolt
/// não vem com um índice denso embutido, então este handle é uma tabela própria mantida em
/// AetherPhysicsWorld (índice + número de geração, mesmo esquema de detecção de
/// use-after-free que o resto da engine usa em World.cs — ver EntitySlot.Version).
AetherJointHandle AetherPhysics_CreateJoint(AetherPhysicsWorld *world, AetherBodyHandle body1,
                                             AetherBodyHandle body2, const AetherJointDesc *desc);

/// Versão nova com referencial explícito e validação integral antes de tocar no Jolt.
/// LocalToBody1/2 transforma TODOS os pontos/eixos pelo corpo indicado. Retorna Invalid para
/// versão/tamanho/espaço/tipo/motor inválido, eixo degenerado ou limites incompatíveis. Para
/// Hinge, [-pi,+pi] exatos representam rotação contínua; valores fora do contrato
/// min=[-pi,0], max=[0,+pi] são recusados, nunca truncados silenciosamente.
AetherJointHandle AetherPhysics_CreateJointV2(AetherPhysicsWorld *world, AetherBodyHandle body1,
                                               AetherBodyHandle body2, const AetherJointDescV2 *desc);

/// Remove e destrói a junta. Handle passa a ser inválido depois desta chamada. É seguro (e
/// necessário) destruir uma junta antes de destruir os corpos que ela conecta — mas destruir
/// um corpo NÃO destrói automaticamente as juntas que o referenciam (mesma disciplina "sem
/// mágica escondida" do resto desta fronteira); o lado C# é responsável por isso, ver
/// PhysicsSyncSystem. Reativa os dois corpos conectados (equivalente a JPH::BodyInterface::
/// ActivateBody): um corpo que a junta mantinha parado (ex.: pêndulo em equilíbrio, adormecido
/// pelo Jolt por inatividade) não tem por que continuar dormindo quando a força que o segurava
/// desaparece — sem isso ele ficaria "congelado" até algo mais o acordar por acidente. Pela
/// mesma razão, AetherPhysics_CreateJoint também ativa os dois corpos ao criar a junta.
void AetherPhysics_DestroyJoint(AetherPhysicsWorld *world, AetherJointHandle handle);

/// Atualiza o estado do motor de uma junta Hinge/Slider já criada (liga/desliga, muda alvo).
/// Sem efeito silencioso em juntas Point/Distance (não têm motor) ou handle inválido — mesma
/// disciplina defensiva do resto da fronteira (nunca crasha por handle ruim).
void AetherPhysics_SetJointMotor(AetherPhysicsWorld *world, AetherJointHandle handle, const AetherJointMotorDesc *motor);

/// Lê o ângulo atual (Hinge, radianos) ou posição atual (Slider, metros) da junta ao longo do
/// seu eixo — o que JPH::HingeConstraint::GetCurrentAngle()/JPH::SliderConstraint::GetCurrentPosition()
/// expõem. Devolve 0 para Point/Distance/handle inválido (não têm um "avanço ao longo de um eixo"
/// — Point não tem graus de liberdade livres, Distance é medido por AetherPhysics_GetTransform
/// dos dois corpos, não por um único escalar).
float AetherPhysics_GetJointPosition(AetherPhysicsWorld *world, AetherJointHandle handle);

// ---------------------------------------------------------------- character controller (4.1.5)
//
// Sobre JPH::CharacterVirtual — não JPH::Character (corpo rígido de verdade): CharacterVirtual
// é a classe que o próprio Jolt desenha para personagem jogável (degraus, deslizar em rampa,
// trocar de forma ao agachar são exclusivos dela; Character não tem nenhum desses métodos).
// CharacterVirtual não é adicionado à broadphase — não aparece em raycast/overlap de outros
// corpos a menos que o jogo queira isso (fora do escopo desta fatia).
//
// O padrão de uso, documentado no próprio comentário de ExtendedUpdate do Jolt (não é decisão
// nossa, é a API pretendida pelo autor): a cada frame, o CHAMADOR monta a velocidade desejada
// somando (a) a velocidade horizontal do input do jogador, (b) GetGroundVelocity() se estiver
// apoiado num corpo que se move (plataforma), e (c) a integração manual de gravidade — o Jolt
// não aplica gravidade à velocidade do personagem sozinho, só a usa internamente para empurrar
// objetos abaixo dele. Ver AetherCharacter_GetGroundVelocity/GetGroundState abaixo.

enum class AetherCharacterGroundState : ae::u32 {
  OnGround = 0,      // apoiado, chão andável (dentro de maxSlopeAngle)
  OnSteepGround = 1,  // toca uma rampa/parede íngreme demais para subir — desliza
  NotSupported = 2,   // toca algo, mas não o sustenta (ex.: parede vertical pura)
  InAir = 3,          // sem contato nenhum
};

/// Descreve a cápsula do personagem e os parâmetros de JPH::CharacterVirtualSettings mais
/// relevantes para um jogo (o resto fica nos defaults do Jolt — ver jolt_bridge.cpp). radius/
/// halfHeight seguem a mesma convenção de AetherShapeDesc::Capsule. maxSlopeAngle em radianos —
/// setar exatamente 0.9999 (cos) internamente desliga a checagem, mas na fronteira você passa o
/// ÂNGULO (radianos), não o cosseno; um ângulo de pi/2 (90°) já cobre "qualquer rampa é andável"
/// na prática, sem precisar do valor mágico de desligar que o Jolt usa internamente.
struct AetherCharacterDesc {
  float radius;
  float standingHalfHeight;   // metade da altura do CILINDRO em pé (sem as tampas)
  float crouchingHalfHeight;  // idem, agachado — usado só por AetherCharacter_SetCrouching
  float maxSlopeAngle;        // radianos; rampas mais íngremes que isso viram OnSteepGround
  float mass;                 // kg, usado para empurrar objetos ao ficar em cima deles
  float maxStrength;          // N, força máxima com que o personagem empurra outros corpos
};

using AetherCharacterHandle = ae::u32;
constexpr AetherCharacterHandle AetherCharacterHandle_Invalid = 0xFFFFFFFFu;

/// Cria um character controller na posição/rotação dadas, SEMPRE em pé (standingHalfHeight) —
/// não há como nascer já agachado; chame AetherPhysics_SetCharacterCrouching depois se
/// necessário. Devolve AetherCharacterHandle_Invalid se `desc` for nulo ou se a criação da
/// cápsula falhar. Mesma tabela índice+geração de AetherJointHandle — CharacterVirtual não tem
/// índice denso embutido.
/// AVISO: como a criação sempre usa a forma DE PÉ, nascer num espaço apertado demais para essa
/// forma (ex.: debaixo de um vão baixo) faz a resolução de penetração da CRIAÇÃO empurrar o
/// personagem para uma posição inesperada (ex.: para cima de um teto fino) antes mesmo de um
/// SetCharacterCrouching(1) subsequente ter qualquer efeito — não há uma "criação já agachada".
/// Para entrar num vão baixo, crie o personagem num espaço livre, agache, e então mova-o até
/// o vão (mesmo padrão de como um jogador entraria de verdade: agachar antes de entrar, não
/// depois).
AetherCharacterHandle AetherPhysics_CreateCharacter(AetherPhysicsWorld *world, const AetherCharacterDesc *desc,
                                                      AetherVec3 position, AetherQuat rotation);

void AetherPhysics_DestroyCharacter(AetherPhysicsWorld *world, AetherCharacterHandle handle);

/// Define a velocidade linear ANTES de chamar Update — é o único jeito de mover o personagem
/// (não existe "aplicar força"; CharacterVirtual não é dinâmico). Monte esta velocidade somando
/// input do jogador + GetGroundVelocity() (se apoiado numa plataforma) + gravidade acumulada
/// manualmente — mesma responsabilidade que o comentário do Jolt atribui ao chamador.
void AetherPhysics_SetCharacterVelocity(AetherPhysicsWorld *world, AetherCharacterHandle handle, AetherVec3 velocity);
AetherVec3 AetherPhysics_GetCharacterVelocity(AetherPhysicsWorld *world, AetherCharacterHandle handle);

/// Avança a simulação do personagem em deltaTime, com suporte a degraus (JPH::CharacterVirtual::
/// ExtendedUpdate — WalkStairs/StickToFloor, usando os defaults do Jolt para os campos de
/// ExtendedUpdateSettings: 40cm de step-up, 50cm de stick-to-floor, ver jolt_bridge.cpp).
/// gravity é usada só internamente pelo Jolt para empurrar objetos abaixo do personagem — NÃO
/// integra a velocidade vertical do próprio personagem (mesma disciplina "sem mágica escondida"
/// do resto da fronteira; o chamador já deveria ter somado gravidade à velocidade antes de
/// SetCharacterVelocity). Filtra por camada/corpo com a mesma AetherQueryLayerMask/ignoreBody
/// das queries (4.1.4) — útil para o personagem não colidir consigo mesmo caso tenha um corpo
/// rígido próprio, ou para ignorar objetos "fantasma".
void AetherPhysics_UpdateCharacter(AetherPhysicsWorld *world, AetherCharacterHandle handle, float deltaTime,
                                    AetherVec3 gravity, AetherQueryLayerMask layerMask, AetherBodyHandle ignoreBody);

void AetherPhysics_GetCharacterTransform(AetherPhysicsWorld *world, AetherCharacterHandle handle,
                                          AetherVec3 *outPosition, AetherQuat *outRotation);

AetherCharacterGroundState AetherPhysics_GetCharacterGroundState(AetherPhysicsWorld *world, AetherCharacterHandle handle);

/// Velocidade do corpo/superfície sob o personagem (0 se InAir ou handle inválido) — já inclui
/// rotação do corpo de suporte (não é só `linear_velocity`, o Jolt calcula a velocidade do PONTO
/// de contato num corpo que gira, ver CalculateCharacterGroundVelocity). Some isto à velocidade
/// desejada antes de AetherPhysics_SetCharacterVelocity para "grudar" em plataformas móveis —
/// não é automático, é a responsabilidade do chamador descrita no comentário desta seção.
AetherVec3 AetherPhysics_GetCharacterGroundVelocity(AetherPhysicsWorld *world, AetherCharacterHandle handle);

/// Normal da superfície de contato (chão ou rampa) — {0,0,0} se InAir ou handle inválido. Útil
/// para decidir a direção de deslizamento quando GroundState é OnSteepGround.
AetherVec3 AetherPhysics_GetCharacterGroundNormal(AetherPhysicsWorld *world, AetherCharacterHandle handle);

/// Troca entre a cápsula standingHalfHeight/crouchingHalfHeight de AetherCharacterDesc,
/// checando primeiro se há espaço livre para a forma nova (JPH::CharacterVirtual::SetShape com
/// maxPenetrationDepth pequeno — não zero, para tolerar o padding padrão do personagem, mas
/// pequeno o bastante para recusar levantar debaixo de algo baixo). Devolve 1 se a troca teve
/// sucesso, 0 se não havia espaço (ex.: tentando ficar de pé debaixo de algo baixo) — nesse caso
/// a forma permanece a anterior, sem efeito colateral.
ae::i32 AetherPhysics_SetCharacterCrouching(AetherPhysicsWorld *world, AetherCharacterHandle handle, ae::i32 crouching);

} // extern "C"
