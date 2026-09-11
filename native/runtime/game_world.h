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
#include "runtime/transform_math.h"
#include "scene/component_properties.h"
#include "scene/component_schema.h"

#include <string>
#include <vector>

namespace ae::runtime {

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
};

const char *worldStatusMessage(WorldStatus status) noexcept;

// Quem escreve a pose de um objeto durante o Play. Escrever transform por script
// em um objeto cuja pose é publicada pela física dessincronizaria os dois lados.
enum class TransformAuthority : u32 { Free, PhysicsBody, Character };

// Uma operação estrutural pendente, aplicada no próximo ponto seguro.
struct PendingCommand {
  enum class Kind : u32 { Destroy, Reparent, RemoveComponent } kind = Kind::Destroy;
  ObjectId object = kInvalidObject;
  ObjectId parent = kInvalidObject;
  u32 childIndex = 0;
  u64 instance = 0;
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
  bool activeInHierarchy(const ObjectHandle &handle) const noexcept;
  WorldStatus setActive(const ObjectHandle &handle, bool active);

  // --- ciclo de vida ------------------------------------------------------
  // Criação imediata: o handle devolvido já resolve, aceita componentes e pode
  // ser guardado. `status` explica a recusa quando o handle volta inválido.
  ObjectHandle createObject(const ObjectHandle &parent, std::string_view name, WorldStatus &status);
  // Marca o objeto e a subárvore como vencidos na hora; o armazenamento sai no
  // próximo `flush()`. Handles guardados passam a ser recusados imediatamente.
  WorldStatus destroyObject(const ObjectHandle &handle);
  WorldStatus setParent(const ObjectHandle &handle, const ObjectHandle &parent, u32 childIndex);

  // --- componentes --------------------------------------------------------
  u32 componentCount(const ObjectHandle &handle) const noexcept;
  ComponentHandle componentAt(const ObjectHandle &handle, u32 index) const noexcept;
  ComponentHandle findComponent(const ObjectHandle &handle, std::string_view typeId, u32 ordinal = 0) const noexcept;
  std::string_view componentTypeId(const ComponentHandle &component) const noexcept;
  const scene::ComponentValue *readComponent(const ComponentHandle &component) const noexcept;
  ComponentHandle addComponent(const ObjectHandle &handle, std::string_view typeId, WorldStatus &status);
  WorldStatus removeComponent(const ComponentHandle &component);

  WorldStatus getProperty(const ComponentHandle &component, std::string_view propertyId,
                          scene::ComponentPropertyValue &out) const;
  WorldStatus setProperty(const ComponentHandle &component, std::string_view propertyId,
                          const scene::ComponentPropertyValue &value);

  // --- transform ----------------------------------------------------------
  WorldStatus localTransform(const ObjectHandle &handle, Transform &out) const;
  WorldStatus setLocalTransform(const ObjectHandle &handle, const Transform &value);
  WorldStatus worldTransform(const ObjectHandle &handle, Transform &out) const;
  WorldStatus setWorldTransform(const ObjectHandle &handle, const Transform &value);
  TransformAuthority authorityOf(const ObjectHandle &handle) const noexcept;
  // O adaptador de física declara quem publica pose depois de montar os corpos.
  void setAuthority(ObjectId id, TransformAuthority authority);
  void clearAuthorities();

  // --- ponto seguro e relógio --------------------------------------------
  // Aplica a fila. Devolve quantos comandos foram aplicados; comandos dirigidos
  // a objetos que já não existem são descartados em silêncio, porque o destino
  // deles já é o estado final pedido.
  // `destroyed`, quando fornecido, recebe os ids removidos nesta passagem: é
  // como o adaptador de física descobre que precisa soltar os corpos deles.
  u32 flush(std::vector<ObjectId> *destroyed = nullptr);
  u32 pendingCommandCount() const noexcept { return static_cast<u32>(commands_.size()); }
  double elapsedSeconds() const noexcept { return elapsed_; }
  void advanceClock(double delta) noexcept { if (delta > 0) elapsed_ += delta; }

private:
  struct Slot {
    u32 generation = 0;  // zero quando o id nunca existiu ou já foi destruído
  };
  static u32 nextWorldId() noexcept;
  WorldStatus resolve(const ObjectHandle &handle, const SceneObject *&out) const noexcept;
  scene::Components *editComponents(ObjectId id);
  bool queue(const PendingCommand &command);
  void markSubtreeStale(ObjectId id);

  SceneGraph graph_;
  std::vector<Slot> slots_;
  std::vector<TransformAuthority> authorities_;
  std::vector<PendingCommand> commands_;
  std::vector<ObjectId> scratch_;
  u32 worldId_ = 0;
  u64 structuralRevision_ = 0;
  double elapsed_ = 0;
};

} // namespace ae::runtime
