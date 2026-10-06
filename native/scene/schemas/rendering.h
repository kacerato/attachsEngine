// Família Renderização: geometria, deformação, nível de detalhe e ambiente.
// Incluído somente por scene/component_schema.h.
#pragma once
#include "scene/environment.h"
#include "scene/lod_group.h"
#include "scene/mesh_renderer.h"
#include "scene/skinned_mesh.h"
#include "scene/ui_canvas.h"

namespace ae::scene {
inline constexpr std::array<ComponentRule, 1> skinnedMeshRequirements{{
  {"astra.render.mesh", "Adicione Malha a este objeto"}
}};
#include "scene/generated/rendering_schemas.inc"
} // namespace ae::scene
