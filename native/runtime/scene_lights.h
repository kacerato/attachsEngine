#pragma once
#include "renderer/punctual_lights.h"
#include "runtime/scene_graph.h"
#include <vector>

namespace ae::runtime {
// Percorre a cena e devolve as luzes acesas, já com pose de mundo resolvida
// pela hierarquia. Objeto desativado — ou com um ancestral desativado — não
// acende, pela mesma regra que apaga a malha.
//
// Isto vale tanto para o documento autoral quanto para o mundo em execução:
// os dois são `SceneGraph`, e é por isso que mexer numa luz por script durante
// o Play muda a tela sem nenhum caminho separado.
bool collectSceneLights(const SceneGraph &graph, std::vector<renderer::SceneLight> &out);
} // namespace ae::runtime
