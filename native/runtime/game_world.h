// O mundo de execução: o que roda quando o usuário aperta Play.
//
// Ele possui o grafo de cena da sessão, a identidade dos handles, a fila de
// comandos estruturais e o relógio. **Não conhece o editor**: recebe uma cena
// (qualquer `SceneGraph`, inclusive o documento autoral) e a copia para dentro
// de si. O documento original nunca é escrito durante o Play — quem quiser
// provar isso compara a revisão dele antes e depois.
//
// ## Identidade
//
// Um `ObjectHandle` carrega mundo, id e geração. O mundo é um contador global do
// processo: um handle guardado por um script de uma sessão anterior de Play é
// recusado, não reinterpretado no mundo novo. A geração é recusada assim que o
// objeto é destruído, ainda dentro do mesmo callback — e como ids nunca são
// reciclados, nunca existe o caso de um handle antigo acertar um objeto novo.
//
// ## Ponto seguro
//
// Criar é imediato: o handle devolvido já funciona, e o script pode configurar
// o objeto na mesma linha. Destruir, reparentear e remover componente entram na
// fila; `flush()` as aplica entre passos. A diferença é deliberada: uma criação
// no meio de um callback não invalida referência nenhuma, enquanto uma remoção
// invalida — então a remoção marca a referência como vencida na hora (leituras
// passam a ser recusadas) e só mexe no armazenamento no ponto seguro.
//
// Uma thread só: todas as chamadas ocorrem na thread dona do mundo.
#pragma once
#include "runtime/scene_graph.h"
#include "runtime/primitive_object.h"
#include "runtime/transform_math.h"
#include "runtime/simulation_clock.h"
#include "scene/component_properties.h"
#include "scene/component_schema.h"
#include "resources/asset_registry.h"
#include "resources/environment_profile.h"

#include <string>
#include <functional>
#include <deque>
#include <vector>

namespace ae::runtime {
class Prefab;

using ComponentResourceResolver = std::function<bool(resources::AssetGuid,resources::AssetType,std::string_view,u32,
                                                     scene::ComponentValue &)>;

struct ObjectHandle {
  u32 world = 0;
  ObjectId id = kInvalidObject;
  u32 generation = 0;
  bool valid() const noexcept { return world != 0 && id != kInvalidObject && generation != 0; }
  friend bool operator==(const ObjectHandle &a, const ObjectHandle &b) noexcept {
    return a.world == b.world && a.id == b.id && a.generation == b.generation;
  }
};

struct ComponentHandle {
  ObjectHandle object{};
  u64 instance = 0;
  bool valid() const noexcept { return object.valid() && instance != 0; }
};

enum class WorldStatus : u32 {
  Ok,
  NotRunning,
  ForeignWorld,       // handle de outra sessão de Play
  StaleHandle,        // objeto destruído ou geração vencida
  UnknownObject,
  UnknownComponent,   // tipo não registrado no schema
  ComponentMissing,   // instância inexistente neste objeto
  ComponentUnavailable, // exigência/incompatibilidade do schema
  ComponentInUse,     // outro componente ainda depende dele
  NotMutableInPlay,
  TransformOwnedByPhysics,
  InvalidArgument,
  LimitReached,
  Rejected,
  UnknownResource,
  ResourceTypeMismatch,
  ClipNotInComponent, // o clipe existe mas não está na lista do componente Animation
  UnknownElement,     // elemento de coleção removido ou identidade desconhecida
  OperationExpired,   // resultado rastreado saiu da janela de retenção
  PropertyNotTweenable,
  PropertyAlreadyTweening,
  PropertyWrittenExternally,
  UnknownOperation,   // método ou evento não declarado pelo tipo do componente
};

const char *worldStatusMessage(WorldStatus status) noexcept;

// Quem escreve a pose de um objeto durante o Play. Escrever transform por script
// em um objeto cuja pose é publicada pela física dessincronizaria os dois lados.
enum class TransformAuthority : u32 { Free, PhysicsBody, Character, PhysicsBody2D };
enum class ReparentPosePolicy : u32 { KeepLocal, KeepWorld };
enum class WorldOperationState : u32 { Pending, Applied, Failed };

// Uma operação estrutural pendente, aplicada no próximo ponto seguro.
struct PendingCommand {
  enum class Kind : u32 { Destroy, Reparent, RemoveComponent } kind = Kind::Destroy;
  ObjectId object = kInvalidObject;
  ObjectId parent = kInvalidObject;
  u32 childIndex = 0;
  u64 instance = 0;
  ReparentPosePolicy posePolicy = ReparentPosePolicy::KeepLocal;
  u64 operationId = 0;
};

class GameWorld final {
public:
  static constexpr u32 kMaximumPendingCommands = 4096;

  GameWorld() = default;
  GameWorld(const GameWorld &) = delete;
  GameWorld &operator=(const GameWorld &) = delete;

  // Copia a cena para dentro do mundo e abre uma sessão nova. Recusa cenas com
  // componentes de tipo ausente: executar dados que não sabemos interpretar
  // perderia o que o usuário autorou na primeira gravação.
  bool load(const SceneGraph &source);
  void clear();
  bool running() const noexcept { return worldId_ != 0; }
  u32 worldId() const noexcept { return worldId_; }
  // Quantos objetos foram destruídos desde o load. Consumidores (física,
  // renderização) usam a revisão estrutural para saber que precisam reconstruir.
  u64 structuralRevision() const noexcept { return structuralRevision_; }
  // Invalidações (scene::Invalidate) acumuladas pelas mudanças aceitas desde a
  // última consulta: o dono do mundo as consome no ponto seguro e reconstrói o
  // que o contrato do componente declara (corpo e forma física, por exemplo).
  u32 pendingInvalidation() const noexcept { return invalidated_; }
  u32 consumeInvalidation() noexcept { const u32 value = invalidated_; invalidated_ = 0; return value; }

  const SceneGraph &graph() const noexcept { return graph_; }
  // Acesso direto para o adaptador de física publicar poses. Não é a API de
  // gameplay: quem usa isto é dono do mundo, não um script do projeto.
  SceneGraph &poseGraph() noexcept { return graph_; }

  ObjectHandle root() const noexcept { return handle(graph_.root()); }
  ObjectHandle handle(ObjectId id) const noexcept;
  bool alive(const ObjectHandle &handle) const noexcept { return validate(handle) == WorldStatus::Ok; }
  WorldStatus validate(const ObjectHandle &handle) const noexcept;
  const SceneObject *find(const ObjectHandle &handle) const noexcept;

  // --- hierarquia ---------------------------------------------------------
  ObjectHandle parentOf(const ObjectHandle &handle) const noexcept;
  u32 childCount(const ObjectHandle &handle) const noexcept;
  ObjectHandle childAt(const ObjectHandle &handle, u32 index) const noexcept;
  ObjectHandle findChildByName(const ObjectHandle &parent, std::string_view name, bool recursive) const noexcept;
  std::string_view nameOf(const ObjectHandle &handle) const noexcept;
  WorldStatus setName(const ObjectHandle &handle, std::string_view name);
  bool activeSelf(const ObjectHandle &handle) const noexcept;
  bool activeInHierarchy(const ObjectHandle &handle) const noexcept;
  WorldStatus setActive(const ObjectHandle &handle, bool active);
  std::string_view tagOf(const ObjectHandle &handle) const noexcept;
  WorldStatus setTag(const ObjectHandle &handle,std::string_view tag);
  WorldStatus compareTag(const ObjectHandle &handle,std::string_view tag,bool &matches) const;
  WorldStatus findTagged(std::string_view tag,std::span<u64> output,u32 &count,bool firstOnly=false) const;
  WorldStatus setGroups(const ObjectHandle &handle,const ObjectGroups &groups);
  WorldStatus setGroupMembership(const ObjectHandle &handle,std::string_view name,bool member);
  WorldStatus isInGroup(const ObjectHandle &handle,std::string_view name,bool &member) const;
  // Inactive objects belong to groups too; callers choose whether to include
  // them. Pending destruction is always excluded, and order is tree order.
  WorldStatus findGroup(std::string_view name,std::span<u64> output,u32 &count,bool includeInactive=true) const;
  // Camada de gameplay (filtro de física e consultas) e flags de desenho do
  // objeto. A camada recria o corpo; as flags o renderer lê a cada quadro.
  WorldStatus setLayer(const ObjectHandle &handle, u32 layer);
  WorldStatus setRenderFlags(const ObjectHandle &handle, bool visible, bool castShadow, bool receiveShadow);

  // --- ciclo de vida ------------------------------------------------------
  // Criação imediata: o handle devolvido já resolve, aceita componentes e pode
  // ser guardado. `status` explica a recusa quando o handle volta inválido.
  ObjectHandle createObject(const ObjectHandle &parent, std::string_view name, WorldStatus &status);
  ObjectHandle createPrimitive(const ObjectHandle &parent,scene::PrimitiveType type,const PrimitiveResource &resource,WorldStatus &status);
  ObjectHandle instantiate(const ObjectHandle &source,const ObjectHandle &parent,ObjectCloneMap &mapping,WorldStatus &status);
  ObjectHandle instantiate(const Prefab &prefab,const ObjectHandle &parent,ObjectCloneMap &mapping,WorldStatus &status,std::string &diagnostic);
  // Cena aditiva (SceneManager.LoadSceneAdditive): copia a cena inteira de uma
  // vez sob um objeto contêiner com o nome dela, remapeando referências entre
  // objetos da cena. Mesma publicação dos clones: fica pendente até
  // finishInstantiation, que é onde os comportamentos da cena são criados.
  ObjectHandle instantiateScene(const SceneGraph &source,const ObjectHandle &parent,std::string_view name,ObjectCloneMap &mapping,WorldStatus &status);
  WorldStatus finishInstantiation(const ObjectHandle &root,bool commit);
  // Marca o objeto e a subárvore como vencidos na hora; o armazenamento sai no
  // próximo `flush()`. Handles guardados passam a ser recusados imediatamente.
  WorldStatus destroyObject(const ObjectHandle &handle, u64 *operationId = nullptr);
  WorldStatus destroyAfter(const ObjectHandle &handle, double seconds);
  WorldStatus setParent(const ObjectHandle &handle, const ObjectHandle &parent, u32 childIndex,
                        ReparentPosePolicy posePolicy = ReparentPosePolicy::KeepLocal,
                        u64 *operationId = nullptr);

  // --- componentes --------------------------------------------------------
  u32 componentCount(const ObjectHandle &handle) const noexcept;
  ComponentHandle componentAt(const ObjectHandle &handle, u32 index) const noexcept;
  ComponentHandle findComponent(const ObjectHandle &handle, std::string_view typeId, u32 ordinal = 0) const noexcept;
  std::string_view componentTypeId(const ComponentHandle &component) const noexcept;
  const scene::ComponentValue *readComponent(const ComponentHandle &component) const noexcept;
  ComponentHandle addComponent(const ObjectHandle &handle, std::string_view typeId, WorldStatus &status);
  ComponentHandle addBehavior(const ObjectHandle &handle, std::string_view typeId, std::string_view source, WorldStatus &status);
  WorldStatus removeComponent(const ComponentHandle &component, u64 *operationId = nullptr);
  // Tickets valem somente nesta sessão. Resultados concluídos são retidos até
  // 4096 operações rastreadas; um ticket expulso retorna OperationExpired.
  WorldStatus operationResult(u32 world, u64 operationId, WorldOperationState &state,
                              WorldStatus &result) const noexcept;

  WorldStatus getProperty(const ComponentHandle &component, std::string_view propertyId,
                          scene::ComponentPropertyValue &out) const;
  WorldStatus setProperty(const ComponentHandle &component, std::string_view propertyId,
                          const scene::ComponentPropertyValue &value);
  WorldStatus validateTweenNumber(const ComponentHandle &component,std::string_view propertyId,float destination,float &initial) const;
  // Only explicitly eligible independent scalars: no component clone per tick.
  WorldStatus setTweenNumber(const ComponentHandle &component,std::string_view propertyId,float value);
  // A tripla refletida é uma atribuição: valida todos os canais e invariantes
  // antes de publicar qualquer eixo, inclusive para vetores de direção.
  WorldStatus setTriple(const ComponentHandle &component,std::string_view propertyId,
                        const float (&values)[3]);
  WorldStatus getSlotProperty(const ComponentHandle &component,std::string_view propertyId,u32 slot,
                              scene::ComponentPropertyValue &out) const;
  WorldStatus setSlotProperty(const ComponentHandle &component,std::string_view propertyId,u32 slot,
                              const scene::ComponentPropertyValue &value,
                              const ComponentResourceResolver &resolveResource={});
  WorldStatus getResource(const ComponentHandle &component, std::string_view propertyId, u32 slot,
                          resources::AssetGuid &out) const;
  WorldStatus resourceElementId(const ComponentHandle &component,std::string_view propertyId,u32 slot,u64 &out) const;
  WorldStatus getResourceByElementId(const ComponentHandle &component,std::string_view propertyId,u64 elementId,
                                     resources::AssetGuid &out) const;
  WorldStatus setResource(const ComponentHandle &component, std::string_view propertyId, u32 slot,
                          resources::AssetGuid value, const resources::AssetRegistry &assets,
                          std::span<const resources::EnvironmentProfile> environmentProfiles={},
                          const ComponentResourceResolver &resolveResource={});
  WorldStatus setResourceByElementId(const ComponentHandle &component,std::string_view propertyId,u64 elementId,
                                     resources::AssetGuid value,const resources::AssetRegistry &assets,
                                     std::span<const resources::EnvironmentProfile> environmentProfiles={},
                                     const ComponentResourceResolver &resolveResource={});
  // A lista é valor do componente: troca atômica na thread do mundo durante o
  // callback; o avaliador de animação lê o novo valor depois do callback.
  WorldStatus appendAnimationClip(const ComponentHandle &component,resources::AssetGuid clip,u64 &elementId,
                                  const ComponentResourceResolver &resolveResource);
  WorldStatus removeAnimationClip(const ComponentHandle &component,u64 elementId);
  WorldStatus moveAnimationClip(const ComponentHandle &component,u64 elementId,u32 targetIndex);
  // Point edits publish one validated curve value, preserving point identities.
  WorldStatus editPathPoint(const ComponentHandle &component,u32 operation,u64 elementId,u32 index,
                           const float *values,u64 &allocatedId);

  // --- transform ----------------------------------------------------------
  WorldStatus localTransform(const ObjectHandle &handle, Transform &out) const;
  WorldStatus setLocalTransform(const ObjectHandle &handle, const Transform &value);
  // Edição do editor com o Play rodando (Unity: mexer no Transform de um
  // Rigidbody pelo Inspector). Grava a pose mesmo quando a física a publica e
  // pede a recriação do corpo no ponto seguro, que o monta na pose nova.
  // Scripts continuam com `setLocalTransform` e a recusa dele.
  WorldStatus placeLocalTransform(const ObjectHandle &handle, const Transform &value);
  WorldStatus worldTransform(const ObjectHandle &handle, Transform &out) const;
  WorldStatus setWorldTransform(const ObjectHandle &handle, const Transform &value);
  // Aplica o mesmo delta local normalizado usado pelo controle de toque,
  // respeitando CameraLook e a autoridade de pose do mundo de Play.
  WorldStatus applyCameraLook(const ObjectHandle &handle,float x,float y);
  TransformAuthority authorityOf(const ObjectHandle &handle) const noexcept;
  // O adaptador de física declara quem publica pose depois de montar os corpos.
  void setAuthority(ObjectId id, TransformAuthority authority);
  void clearAuthorities();

  // --- ponto seguro e relógio --------------------------------------------
  // Aplica a fila e devolve quantos comandos foram aplicados. Comandos
  // rastreados registram também recusas ocorridas neste ponto seguro.
  // `destroyed`, quando fornecido, recebe os ids removidos nesta passagem: é
  // como o adaptador de física descobre que precisa soltar os corpos deles.
  u32 flush(std::vector<ObjectId> *destroyed = nullptr);
  u32 pendingCommandCount() const noexcept { return static_cast<u32>(commands_.size()); }
  double elapsedSeconds() const noexcept { return elapsed_; }
  void advanceClock(double delta);
  const SimulationClock &clock() const noexcept {return clock_;}
  WorldStatus setTimeScale(float value);
  bool beginFrame(double elapsed,bool editorStep=false);

  // Mudanças de hierarquia aplicadas, para os callbacks ParentChanged e
  // ChildrenChanged (Unity 6000.0 OnTransformParentChanged/ChildrenChanged).
  // ParentChanged vale para o objeto e toda a subárvore dele; ChildrenChanged
  // para o pai cuja lista direta de filhos mudou. Limitada: além do teto, as
  // mais antigas saem e `hierarchyChangesDropped` conta a perda.
  enum class HierarchyChangeKind : u32 { ParentChanged=0, ChildrenChanged=1 };
  struct HierarchyChange { ObjectHandle object{}; HierarchyChangeKind kind=HierarchyChangeKind::ParentChanged; };
  static constexpr usize kHierarchyChangeCapacity=4096;
  // Entrega e esvazia; o consumidor é único (o runtime de scripts).
  std::vector<HierarchyChange> takeHierarchyChanges();
  u64 hierarchyChangesDropped() const noexcept {return hierarchyDropped_;}

private:
  void recordHierarchyChange(ObjectId id,HierarchyChangeKind kind);
  void recordParentChangedSubtree(ObjectId id);
  ObjectHandle registerInstantiation(ObjectId root,const ObjectCloneMap &mapping);
  struct Slot {
    u32 generation = 0;  // zero quando o id nunca existiu ou já foi destruído
  };
  static u32 nextWorldId() noexcept;
  WorldStatus resolve(const ObjectHandle &handle, const SceneObject *&out) const noexcept;
  scene::Components *editComponents(ObjectId id);
  bool queue(PendingCommand command, u64 *operationId);
  void finishOperation(u64 operationId, WorldStatus result);
  void markSubtreeStale(ObjectId id);
  // União das invalidações declaradas pelos componentes da subárvore: ativar,
  // reparentear ou mover um objeto afeta tudo o que está pendurado nele.
  u32 subtreeInvalidation(ObjectId id);
  void invalidateType(std::string_view typeId);

  SceneGraph graph_;
  std::vector<Slot> slots_;
  std::vector<TransformAuthority> authorities_;
  std::vector<PendingCommand> commands_;
  struct OperationRecord { u64 id; WorldOperationState state; WorldStatus result; };
  std::deque<OperationRecord> operationResults_;
  u64 nextOperationId_ = 1;
  std::vector<ObjectId> scratch_;
  u32 worldId_ = 0;
  u64 structuralRevision_ = 0;
  u32 invalidated_ = 0;
  double elapsed_ = 0;
  SimulationClock clock_;
  struct DelayedDestroy { ObjectHandle object; double due; };
  std::vector<DelayedDestroy> delayedDestroy_;
  std::vector<ObjectId> unpublishedClones_;
  std::vector<HierarchyChange> hierarchyChanges_;
  u64 hierarchyDropped_=0;
};

} // namespace ae::runtime
