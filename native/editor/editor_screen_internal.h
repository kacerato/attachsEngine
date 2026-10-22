#pragma once
#include "editor/editor_document.h"
#include <string_view>
#include <vector>

namespace ae::editor {
// Shared geometry: drawing and input must use the same row/splitter limits.
inline constexpr float kRowHeight=28.0f;
inline constexpr float kPanelMinimum=150.0f;
std::vector<std::string_view> componentGroups(const scene::ComponentValue &);
// PK-2: na janela Iluminação o Ambiente da cena mostra só o que a janela
// Lighting da Unity mostra (céu, luz ambiente e neblina), na ordem dela;
// exposição, pós e volume ficam no Inspector do objeto.
std::vector<std::string_view> componentGroups(const scene::ComponentValue &, bool lightingWindow);
std::vector<u32> namedPhysicsLayers(const runtime::GameplayLayers &);
// PK-2: o Ambiente que a janela Iluminação edita -- o Volume global ativo de
// menor prioridade (o "padrão" da cena). Nenhum: kInvalidEntity, e a cena usa o
// ambiente padrão do renderer (renderer::defaultSceneEnvironment).
EditorEntityId sceneLightingEnvironment(const runtime::SceneGraph &,u64 *instance=nullptr);
}
