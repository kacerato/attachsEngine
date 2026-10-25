#pragma once
// Linhas da cena (G6a; Unity LineRenderer): cada componente Linha vira faixas
// viradas para a câmera no passe das partículas, um trecho por par de pontos
// (ou por repetição da imagem, no modo Repetir). Roda sobre o documento
// autoral (a linha aparece na edição) e sobre o grafo do Play, onde o Laser
// reescreve os pontos a cada quadro.
#include "renderer/particles/particle_simulation.h"
#include "runtime/scene_graph.h"
#include "runtime/scene_particles.h"

namespace ae::runtime {

// Acrescenta instâncias e lotes ao quadro das partículas, depois dos lotes já
// presentes, e reordena os lotes de trás para a frente. Devolve quantas linhas
// entraram. No máximo kMaximumLineSegments trechos por linha.
inline constexpr u32 kMaximumLineSegments = 256;
u32 appendLineRenderers(const SceneGraph &graph, const float (&camera)[3], renderer::particles::ParticleFrame &out,
                        const ParticleMaterialResolver *materials = nullptr);

} // namespace ae::runtime
