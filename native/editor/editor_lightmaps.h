#pragma once
// G3c: lightmaps no editor (editor_lightmaps.cpp). O fluxo é o da janela
// Lighting da Unity 6: Generate Lighting assa em segundo plano com a barra de
// progresso no canto (aqui, na própria janela), Cancel interrompe, e Clear
// Baked Data desvincula o LightingData da cena. Enquanto assa, o editor
// continua editável; o resultado só entra se a cena não mudou de projeto.
#include "core/base.h"

#include <string>
#include <vector>

namespace ae::editor {

struct EditorLightmapView {
  bool baking = false;
  float progress = 0;
  std::string stage;
  u32 pass = 0, passes = 0;
  float elapsedSeconds = 0, remainingSeconds = -1;
  // Configurações da cena (o componente Iluminação da cena).
  bool settingsExist = false, bakedGi = true;
  u64 settingsEntity = 0, settingsInstance = 0;
  // O que o próximo bake vai encontrar (contado ao abrir a aba e a cada edição).
  u32 contributors = 0, receivers = 0, receiversWithoutUv = 0;
  std::vector<u64> withoutUv; // objetos que recebem pelas sondas por falta de UV2
  u32 bakedLights = 0, mixedLights = 0, realtimeLights = 0;
  // O recurso em vigor.
  bool hasData = false, missing = false, stale = false;
  std::string path;
  u32 pages = 0, pageSize = 0, entries = 0;
  u64 gpuBytes = 0, texels = 0, rays = 0;
  float bakeSeconds = 0, occupancy = 0;
  bool compressed = true;
  // Medido no renderer no último quadro.
  u32 drawsLightmapped = 0;
  std::string lastResult;
};

} // namespace ae::editor
