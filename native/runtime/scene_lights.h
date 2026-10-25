#pragma once
#include "renderer/punctual_lights.h"
#include "runtime/scene_graph.h"
#include "scene/light.h"
#include <vector>

namespace ae::runtime {
// Percorre a cena e devolve as luzes acesas, já com pose de mundo resolvida
// pela hierarquia. Objeto desativado — ou com um ancestral desativado — não
// acende, pela mesma regra que apaga a malha.
//
// Isto vale tanto para o documento autoral quanto para o mundo em execução:
// os dois são `SceneGraph`, e é por isso que mexer numa luz por script durante
// o Play muda a tela sem nenhum caminho separado.
bool collectSceneLights(const SceneGraph &graph, std::vector<renderer::SceneLight> &out,
                        double sceneSeconds = 0.0);
// Cor, intensidade (na unidade do shader), alcance, ângulos e filtros de um
// componente Luz, sem pose nem regras do sol. Usado por collectSceneLights e
// pela luz modelo do módulo Luzes das partículas (PK-10b).
void sceneLightValues(const scene::Light &light, renderer::SceneLight &entry);
} // namespace ae::runtime
