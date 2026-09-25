#pragma once
#include "core/base.h"

namespace ae::renderer {
// Estatísticas do quadro da cena (S5) — a janela Statistics do Game View da
// Unity, com os números que o renderer realmente usou: o intervalo entre
// quadros apresentados, a GPU do quadro (zero quando não medida: a medição só
// liga com o painel ou as estatísticas abertos), desenhos e triângulos enviados
// depois de culling e LOD, e o que o LOD na própria malha fez.
struct SceneStatistics {
  float frameIntervalMs = 0, gpuFrameMs = 0;
  u32 renderWidth = 0, renderHeight = 0, drawCalls = 0;
  u32 lodDraws = 0, lodReducedDraws = 0;
  u64 triangles = 0, lodBaseTriangles = 0, lodSelectedTriangles = 0;
};
} // namespace ae::renderer
