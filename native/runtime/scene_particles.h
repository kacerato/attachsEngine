#pragma once
// Partículas da cena (G5 P1): os emissores vivos de um grafo.
//
// O mesmo serviço roda sobre o documento autoral (prévia do editor, como o
// GPUParticles3D da Godot emite na vista) e sobre o grafo do Play (GameWorld).
// Ele reconcilia os componentes ParticleSystem com emissores de execução,
// aplica o teto de partículas do nível do aparelho sem tocar no valor autoral,
// avança a simulação em jobs (um emissor por job, nunca dividido: é o que
// mantém o resultado determinístico) e monta o quadro que o renderer desenha.
//
// Nada daqui é serializado. Stop do editor descarta o mundo de Play inteiro;
// a prévia do editor recomeça quando a cena é reaberta.
#include "core/base.h"
#include "renderer/particles/particle_simulation.h"
#include "runtime/scene_graph.h"
#include "scene/particle_system.h"

#include <functional>
#include <map>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace ae::runtime {

// PK-10a: o que o Material de projeto de um sistema entrega ao lote. `texture`
// é o índice da biblioteca publicada (~0 = sem imagem: fica a textura própria).
struct ParticleMaterialLook {
  u32 texture = ~0u;
  float color[3]{1, 1, 1};
  bool additive = false;
};
// Quem conhece os recursos do projeto (o editor) resolve; falso = material
// ausente ou sem shader Partícula, e o lote usa o material embutido.
using ParticleMaterialResolver = std::function<bool(const resources::AssetGuid &, ParticleMaterialLook &)>;

class SceneParticles final {
public:
  enum class Command : u32 { Play, Stop, StopAndClear, Pause, Clear, Emit, Restart };
  struct Status {
    bool playing = false, emitting = false, paused = false;
    u32 alive = 0;
    float time = 0;
  };
  // Sistema que terminou neste avanço, com a ação autoral a aplicar.
  struct Finished {
    ObjectId object = 0;
    u64 instance = 0;
    scene::ParticleStopAction action = scene::ParticleStopAction::None;
  };

  void reset();
  // Teto total de partículas vivas (nível do aparelho). Repartido entre os
  // emissores na proporção do máximo autoral de cada um.
  void setBudget(u32 total) { budget_ = total ? total : 1; }
  u32 budget() const noexcept { return budget_; }
  // Prévia do editor: sistemas que não repetem recomeçam um segundo depois de
  // terminar, para o autor ver a rajada enquanto edita.
  void setPreviewLoop(bool enabled) { previewLoop_ = enabled; }

  // Reconcilia com o grafo e avança `deltaSeconds`. Zero só reconcilia.
  void advance(const SceneGraph &graph, float deltaSeconds, std::vector<Finished> *finished = nullptr);
  // Instâncias e lotes do quadro, ordenados de trás para a frente pela câmera.
  void build(const SceneGraph &graph, const float (&camera)[3], renderer::particles::ParticleFrame &out,
             const ParticleMaterialResolver *materials = nullptr) const;

  // Operação de script/editor sobre um componente. Falso quando o componente
  // não existe neste grafo.
  bool command(const SceneGraph &graph, ObjectId object, u64 instance, Command command, u32 count = 0);
  bool status(const SceneGraph &graph, ObjectId object, u64 instance, Status &out);
  // Leitura para a interface: o estado do último avanço, sem reconciliar.
  bool peek(ObjectId object, u64 instance, Status &out) const;
  u32 capacityOf(ObjectId object, u64 instance) const;
  // Reinicia todos os emissores (prévia do editor).
  void restartAll();

  u32 emitterCount() const noexcept { return static_cast<u32>(entries_.size()); }
  u32 aliveTotal() const noexcept;
  float lastSimulationMs() const noexcept { return simulationMs_; }

private:
  struct Key {
    ObjectId object = 0;
    u64 instance = 0;
    bool operator<(const Key &other) const { return object != other.object ? object < other.object : instance < other.instance; }
  };
  struct Entry {
    renderer::particles::ParticleEmitter emitter;
    std::string fingerprint;
    float world[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    bool active = false, seen = false;
    float idle = 0;  // segundos parado (recomeço da prévia)
  };
  void reconcile(const SceneGraph &graph);
  void distributeBudget();
  // Planos do módulo Colisão, lidos do grafo a cada quadro (o plano pode andar).
  void preparePlanes(const SceneGraph &graph, Entry &entry) const;
  // Sub-emissores: cada evento do quadro faz o sistema referenciado emitir ali.
  void routeSubEmitters(const SceneGraph &graph);
  Entry *firstEntryOf(ObjectId object);

  std::map<Key, Entry> entries_;
  u64 knownRevision_ = ~u64{0};
  u32 budget_ = renderer::particles::particleTierBudget(renderer::particles::ParticleTier::S);
  bool previewLoop_ = false;
  float simulationMs_ = 0;
};

// Semente efetiva: a manual, ou uma derivada do objeto e da instância (estável
// entre execuções; dois sistemas iguais em objetos diferentes não coincidem).
u32 particleSeed(const scene::ParticleSystemSettings &settings, ObjectId object, u64 instance);

} // namespace ae::runtime
