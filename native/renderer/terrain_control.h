#pragma once
#include "renderer/map_package.h"
namespace ae::renderer {
// Surface v2: a linear, clamp-sampled control map. UV = TEXCOORD_0*scale+offset.
// Applies to the heightfield top; closed sides/base and voxels retain COLOR_0.
struct TerrainControlBinding {
  u32 textureIndex=InvalidMapTexture;
  float uvScale[2]{},uvOffset[2]{};
};
}
