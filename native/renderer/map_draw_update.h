#pragma once

#include "renderer/gpu_mesh_instance.h"
#include "renderer/material_override.h"
#include "renderer/map_package.h"
#include <cmath>
#include <memory>
#include <vector>
#include "renderer/water_route.h"

namespace ae::renderer {

struct MapDrawUpdate final {
  u32 drawIndex = 0;
  MapDrawRecord draw{};
  GpuMeshInstance instance{};
};

struct MapDrawState {
  u64 objectId=0;
  std::shared_ptr<const WaterRoute> route;
  float waterLayers[4]{1,1,1,1};
  float waterFlowDepth[4]{0,0,3,0};
  u32 sourceDrawIndex = 0;
  MapDrawUpdate pose{};
  bool visible = true;
  bool castShadow = true;
  MaterialOverride material{};
  // G6-B: paleta do skin deste desenho (16 floats coluna principal por junta,
  // no espaço do desenho: inversa(modelo) * mundo(junta) * bind inversa).
  // Nula num desenho sem skin, ou quando a pose não pôde ser avaliada: o
  // renderer usa a pose de bind (identidade). Compartilhada, não copiada.
  std::shared_ptr<const std::vector<float>> skinPalette;
  // Influências por vértice (1, 2 ou 4) e se o vetor de movimento usa a pose
  // anterior dos ossos (skinnedMotionVectors da Unity).
  u8 skinInfluences = 4;
  bool skinnedMotion = true;
};

// Caller supplies mesh-local bounds, never the previous frame's world bounds.
// Rigid/scale transforms use a conservative matrix-norm bound, including shear.
// No mutation on failure; topology/material/LOD identity stays immutable.
inline bool prepareMapDrawUpdate(u32 index, const MapDrawRecord &source,
                                 const float *model, const float *localCenter,
                                 float localRadius, MapDrawUpdate &output) noexcept {
  if (!model || !localCenter || !std::isfinite(localRadius) || localRadius < 0) return false;
  for (u32 i = 0; i < 16; ++i) if (!std::isfinite(model[i])) return false;
  for (u32 i = 0; i < 3; ++i) if (!std::isfinite(localCenter[i])) return false;
  if (model[3] != 0 || model[7] != 0 || model[11] != 0 || model[15] != 1) return false;
  MapDrawUpdate prepared{};
  prepared.drawIndex = index;
  prepared.draw = source;
  const float tint[4] = {1, 1, 1, 1};
  if (!buildGpuMeshInstance(model, tint, &prepared.instance)) return false;
  float normOne = 0, normInfinity = 0;
  for (u32 axis = 0; axis < 3; ++axis) {
    float column = 0, row = 0;
    for (u32 j = 0; j < 3; ++j) {
      column += std::abs(model[axis * 4 + j]);
      row += std::abs(model[j * 4 + axis]);
    }
    normOne = std::max(normOne, column);
    normInfinity = std::max(normInfinity, row);
    prepared.draw.boundsCenter[axis] = model[12 + axis];
    for (u32 j = 0; j < 3; ++j)
      prepared.draw.boundsCenter[axis] += model[j * 4 + axis] * localCenter[j];
    if (!std::isfinite(prepared.draw.boundsCenter[axis])) return false;
  }
  prepared.draw.boundsRadius = localRadius * std::sqrt(normOne * normInfinity);
  if (!std::isfinite(prepared.draw.boundsRadius)) return false;
  std::memcpy(prepared.draw.model, model, sizeof(prepared.draw.model));
  output = prepared;
  return true;
}

} // namespace ae::renderer
