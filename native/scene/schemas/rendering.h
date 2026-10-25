// Família Renderização: geometria, deformação, nível de detalhe e ambiente.
// Incluído somente por scene/component_schema.h.
#pragma once
#include "scene/environment.h"
#include "scene/lod_group.h"
#include "scene/mesh_renderer.h"
#include "scene/skinned_mesh.h"
#include "scene/ui_canvas.h"
#include "scene/vfx_behaviors.h"
#include "scene/weather.h"

namespace ae::scene {
inline constexpr std::array<ComponentRule, 1> skinnedMeshRequirements{{
  {"astra.render.mesh", "Adicione Malha a este objeto"}
}};
// O clima é o script de clima da Unity sobre um Volume: escreve as nuvens e a
// neblina do Ambiente do próprio objeto.
inline constexpr std::array<ComponentRule, 1> weatherRequirements{{
  {"astra.render.environment", "Adicione um Ambiente a este objeto: o clima muda as nuvens dele"}
}};
#include "scene/generated/rendering_schemas.inc"
} // namespace ae::scene
