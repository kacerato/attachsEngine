#pragma once
// PK-6: as sondas saem do MESMO grafo que luzes e desenhos (documento autoral
// fora do Play, mundo de Play dentro dele), com a pose de mundo resolvida pela
// hierarquia. Objeto inativo, ou com ancestral inativo, não tem sonda.
#include "renderer/light_probes.h"
#include "renderer/reflection_probes.h"
#include "runtime/scene_graph.h"

#include <span>
#include <vector>

namespace ae::runtime {

// Sondas de reflexo acesas, com a caixa de mundo (alinhada aos eixos, como na
// Unity) e a chave da captura.
bool collectSceneReflectionProbes(const SceneGraph &graph, std::vector<renderer::SceneReflectionProbe> &out);

// Um Light Probe Group aceso, com as posições de mundo atuais dos pontos.
struct SceneLightProbeGroup {
  u64 objectId = 0;
  u64 instance = 0;
  bool removeRinging = false;
  resources::AssetGuid baked{};
  std::vector<u64> pointIds;
  std::vector<std::array<float, 3>> positions;
  u64 positionsHash() const noexcept { return renderer::lightProbePositionsHash(positions, pointIds); }
};
bool collectSceneLightProbeGroups(const SceneGraph &graph, std::vector<SceneLightProbeGroup> &out);

// Junta os recursos assados dos grupos acesos num conjunto único e o
// tetraedraliza — a Unity também tetraedraliza todas as sondas da cena juntas.
// Grupos sem recurso carregado ficam de fora. "Remover anéis" é aplicado aqui,
// no carregamento, sobre o SH gravado: o recurso não muda ao alternar a opção.
struct LightProbeBakeLookup {
  resources::AssetGuid guid{};
  const renderer::LightProbeBakeData *data = nullptr;
};
bool buildSceneLightProbeSet(std::span<const SceneLightProbeGroup> groups, std::span<const LightProbeBakeLookup> bakes,
                             renderer::LightProbeSet &out);

// RenderProbe() da Unity no mundo de Play: cada pedido soma um na sonda; o
// extrator copia a contagem para SceneReflectionProbe::renderRequests e o
// renderer captura quando ela muda (sonda Tempo real em "Por script").
class SceneReflectionProbeRequests {
public:
  void request(u64 objectId, u64 instance) {
    for (auto &entry : entries_)
      if (entry.objectId == objectId && entry.instance == instance) { ++entry.count; return; }
    entries_.push_back({objectId, instance, 1});
  }
  u32 count(u64 objectId, u64 instance) const noexcept {
    for (const auto &entry : entries_)
      if (entry.objectId == objectId && entry.instance == instance) return entry.count;
    return 0;
  }
  void apply(std::span<renderer::SceneReflectionProbe> probes) const noexcept {
    for (auto &probe : probes) probe.renderRequests = count(probe.objectId, probe.instance);
  }
  void reset() { entries_.clear(); }
private:
  struct Entry { u64 objectId = 0, instance = 0; u32 count = 0; };
  std::vector<Entry> entries_;
};

} // namespace ae::runtime
