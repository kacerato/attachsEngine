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
