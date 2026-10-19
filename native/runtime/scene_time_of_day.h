#pragma once
#include "runtime/scene_graph.h"
#include <functional>

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
} // namespace ae::runtime
