#pragma once
#include "runtime/component_operations.h"
#include "runtime/scene_graph.h"
#include <functional>
#include <vector>

namespace ae::runtime {
// G4, do jeito da Unity: a Hora do dia é um "script" que gira o transform da
// Luz direcional do próprio objeto, como os ciclos de dia em [ExecuteAlways].
// O gizmo do sol se move, o Inspector mostra a rotação nova, e o céu, a sombra
// e o ambiente reagem porque leem a luz -- não há direção escondida.
//
// `sceneSeconds` é o relógio da cena (0 fora do Play): a hora avança pela
// Duração do dia. Preserva posição e escala de mundo; só a rotação muda, e só
// quando difere da atual. Devolve quantos objetos foram girados.
// `write` grava a rotação nova: no editor, o documento; no Play, o GameWorld
// (setLocalTransform, que respeita a autoridade do transform).
u32 applyTimeOfDay(const SceneGraph &graph, double sceneSeconds,
                   const std::function<bool(ObjectId, const Transform &)> &write);

// G7: o relógio da Hora do dia no mundo de Play. Avança a PRÓPRIA hora do
// componente do mundo (a autoral do documento nunca muda), de modo que
// `TimeOfDay.Hour = 18.5f` num script vale a partir do quadro seguinte, como
// a hora do ciclo de dia da Unity. Depois disto, applyTimeOfDay e a coleta de
// luzes do Play usam `sceneSeconds = 0`. Emite hour_changed, sunrise, sunset e
// day_changed; o salto escrito por script também emite hour_changed.
class SceneTimeOfDayClock {
public:
  // Devolve quantas Horas do dia avançaram.
  u32 advance(GameWorld &world, double elapsed, ComponentEventQueue *events);
  void reset() { entries_.clear(); }
private:
  struct Entry { ObjectId object = 0; u64 instance = 0; i32 hour = 0; float elevation = 0; bool seen = false; };
  Entry &entry(ObjectId object, u64 instance);
  std::vector<Entry> entries_;
};
} // namespace ae::runtime
