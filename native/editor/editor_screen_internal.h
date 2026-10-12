#pragma once
#include "editor/editor_document.h"
#include <string_view>
#include <vector>

namespace ae::editor {
// Shared geometry: drawing and input must use the same row/splitter limits.
inline constexpr float kRowHeight=28.0f;
inline constexpr float kPanelMinimum=150.0f;
std::vector<std::string_view> componentGroups(const scene::ComponentValue &);
std::vector<u32> namedPhysicsLayers(const runtime::GameplayLayers &);
}
