// Métodos e eventos de componente no mundo de Play.
//
// O descritor (scene::ComponentMethod/ComponentEvent) diz o que existe; aqui
// mora a função que produz o efeito e a fila que transporta os acontecimentos.
// Scripts (pela extensão da ABI) e, depois, conexões autoradas usam o mesmo
// caminho: nenhum dos dois reimplementa "tocar áudio" ou "parar timer".
//
// A fila é do mundo de Play, não do documento: nada dela é serializado, e o
// Stop a descarta inteira. Cada consumidor tem o próprio cursor; um evento sai
// da fila quando todos os consumidores ligados passaram por ele.
#pragma once
#include "runtime/game_world.h"
#include "scene/components.h"

#include <array>
#include <deque>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ae::runtime {
class SceneTimers;
class SceneTweens;
class SceneAudio;
class ScenePaths;
class SceneTweenSequences;
class ScenePhysicsQueries;
class ScenePhysics;
class SceneVirtualCameras;
class SceneAnimatorGraphs;

struct ComponentOperationServices {
  GameWorld *world=nullptr;
  SceneTimers *timers=nullptr;
  SceneTweens *tweens=nullptr;
  SceneAudio *audio=nullptr;
  ScenePaths *paths=nullptr;
  SceneTweenSequences *sequences=nullptr;
  ScenePhysicsQueries *queries=nullptr;
  const ScenePhysics *physics=nullptr;
  SceneVirtualCameras *cameras=nullptr;
  SceneAnimatorGraphs *animators=nullptr;
};

using ComponentMethodInvoke=WorldStatus (*)(const ComponentOperationServices &,ComponentHandle,
                                            std::span<const scene::ComponentOperationValue>,
                                            scene::ComponentOperationValue &);
struct ComponentMethodBinding {
  const scene::ComponentType *type=nullptr;
  std::string_view method;
  ComponentMethodInvoke invoke=nullptr;
};
std::span<const ComponentMethodBinding> componentMethodBindings();

// Valida mundo, handle, tipo, método declarado e argumentos antes de chamar a
// função efetiva. Método declarado sem serviço de Play responde NotRunning.
WorldStatus invokeComponentMethod(const ComponentOperationServices &services,ComponentHandle component,
                                  std::string_view method,std::span<const scene::ComponentOperationValue> arguments,
                                  scene::ComponentOperationValue &result);

// Vazio quando todo método declarado tem exatamente uma função e toda função
// corresponde a um método declarado; senão, uma linha por divergência.
std::vector<std::string> auditComponentOperations();

struct ComponentEventRecord {
  u64 sequence=0;
  ObjectHandle object{};
  u64 instance=0;               // zero: o backend identifica só o objeto
  const scene::ComponentType *type=nullptr;
  u32 event=0;                  // índice em type->events
  u32 count=0;                  // valores válidos em `values`
  std::array<scene::ComponentOperationValue,scene::kComponentEventPayloadLimit> values{};
};

class ComponentEventQueue final {
public:
  enum class Consumer : u32 { Scripts=0, Connections=1 };
  static constexpr usize Capacity=4096;
  void reset();
  // Só consumidores ligados seguram eventos; sem nenhum, emitir não acumula.
  void attach(Consumer consumer,bool attached);
  bool attached(Consumer consumer) const noexcept {return attached_[index(consumer)];}
  // Falso para evento não declarado, payload incompatível ou objeto morto: são
  // erros do produtor, nunca silenciosamente convertidos.
  bool emit(GameWorld &world,ObjectId object,u64 instance,const scene::ComponentType &type,std::string_view event,
            std::span<const scene::ComponentOperationValue> payload={});
  // Entrega em ordem os eventos existentes no início da chamada; os emitidos
  // durante a entrega ficam para a próxima, o que limita cascatas A→B→A.
  // `deliver(record,lost)` recebe quantos eventos este consumidor perdeu por
  // estouro da capacidade antes deste; o retorno soma as perdas da chamada.
  template<class Deliver> u64 consume(Consumer consumer,Deliver &&deliver,usize limit=static_cast<usize>(-1)) {
    auto &cursor=cursor_[index(consumer)];
    u64 lost=0,total=0;
    if(!records_.empty() && cursor<records_.front().sequence) {lost=records_.front().sequence-cursor;cursor=records_.front().sequence;}
    if(records_.empty() && cursor<next_) {lost=next_-cursor;cursor=next_;}
    total=lost;
    const u64 end=next_;
    usize delivered=0;
    while(cursor<end && !records_.empty() && delivered<limit) {
      if(cursor<records_.front().sequence) {const auto gap=records_.front().sequence-cursor;lost+=gap;total+=gap;cursor=records_.front().sequence;continue;}
      const auto offset=static_cast<usize>(cursor-records_.front().sequence);
      if(offset>=records_.size()) break;
      const ComponentEventRecord record=records_[offset];
      ++cursor;++delivered;
      deliver(record,lost);
      lost=0;
    }
    trim();
    return total;
  }
  usize pending(Consumer consumer) const noexcept;
  u64 dropped() const noexcept {return dropped_;}
private:
  static constexpr usize index(Consumer consumer) noexcept {return static_cast<usize>(consumer);}
  void trim();
  std::deque<ComponentEventRecord> records_;
  u64 next_=1,dropped_=0;
  std::array<u64,2> cursor_{1,1};
  std::array<bool,2> attached_{};
};

} // namespace ae::runtime
