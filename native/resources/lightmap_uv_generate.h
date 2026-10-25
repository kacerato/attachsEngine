#pragma once
// G3c: "Generate Lightmap UVs" do Model Importer da Unity 6, com o xatlas (MIT).
//
// Na Unity a caixa fica no Model Import Settings › Model › Geometry, com Hard
// Angle (padrão 88°: arestas mais vivas que isso viram costura de UV) e Pack
// Margin (padrão 4 px, a folga entre ilhas supondo que a malha ocupa um lightmap
// de 1024 px). O resultado vai para o canal UV2 da malha — aqui, UV1 do vértice
// de 48 bytes, o mesmo que o shader lê como `vUv1`.
//
// Cada desenho estático vira um atlas próprio em [0,1]: ilhas sem sobreposição,
// costuradas nas arestas vivas e nas costuras de UV0. O xatlas pode duplicar
// vértice na costura; o desenho passa a ter vértices novos e os índices dele
// são reescritos. Desenhos deformados (skin ou blend shape) ficam como estão:
// lightmap é para o que não se move, como na Unity.
//
// Referências: https://docs.unity3d.com/6000.0/Documentation/Manual/LightingGiUvs-GeneratingLightmappingUVs.html
//              https://github.com/jpcy/xatlas
#include "core/base.h"
#include "renderer/map_package.h"

#include <span>
#include <string>
#include <vector>

namespace ae::resources {

struct LightmapUvSettings {
  // Arestas com ângulo diedro acima deste viram costura (graus, 1..180).
  float hardAngleDegrees = 88.0f;
  // Folga entre ilhas, em pixels de um lightmap de 1024 px (1..64).
  float packMarginPixels = 4.0f;
  bool valid() const noexcept {
    return hardAngleDegrees >= 1.0f && hardAngleDegrees <= 180.0f && packMarginPixels >= 1.0f && packMarginPixels <= 64.0f;
  }
};

struct LightmapUvReport {
  u32 drawsUnwrapped = 0;
  u32 drawsDeformed = 0;   // skin/blend shape: ficam sem UV2 nova
  u32 drawsDegenerate = 0; // sem área: nada a desembrulhar
  u32 charts = 0;
  u64 verticesBefore = 0, verticesAfter = 0;
};

// Reescreve UV1 dos desenhos estáticos de `vertices`/`indices`/`draws` (passo
// renderer::MapVertexStride). `deformed[d]` != 0 marca o desenho que não deve
// ser tocado. Quando nada no arquivo deforma (`compact`), os vértices que
// ficarem órfãos saem do buffer; com skin/blend shape os índices absolutos
// precisam continuar valendo e os vértices novos entram no fim.
// Falha fechada: em erro, nada é alterado e `diagnostic` diz o motivo.
bool generateLightmapUvs(std::vector<u8> &vertices, std::vector<u32> &indices,
                         std::vector<renderer::MapDrawRecord> &draws, std::span<const u8> deformed,
                         const LightmapUvSettings &settings, bool compact, LightmapUvReport &report,
                         std::string &diagnostic);

} // namespace ae::resources
