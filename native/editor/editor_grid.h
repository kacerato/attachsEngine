#pragma once
#include "editor/editor_view.h"
#include <array>
#include <algorithm>
#include <cmath>

namespace ae::editor {
struct EditorGridSettings {
  float planeHeight = 0;
  float desiredCellPixels = 32;
  float minimumSpacing = .001f;
  float maximumSpacing = 1000000;
};
struct EditorGridLine {
  float from[3]{}, to[3]{};
  float opacity = 1;
  bool major = false;
  // 1 = world X, 2 = world Z. Axes never follow the camera.
  u32 axis = 0;
};
struct EditorGrid {
  std::array<EditorGridLine, 258> lines{};
  u32 count = 0;
  float spacing = 0;
};
// Camera-relative coverage, world-aligned cells, bounded CPU/UI cost. This is
// an editor overlay, so it cannot z-fight with a coplanar scene mesh.
inline EditorGrid buildEditorGrid(const EditorViewport &view,
                                  const EditorGridSettings &settings = {}) noexcept {
  EditorGrid grid;
  if (!isViewportValid(view) || !std::isfinite(settings.planeHeight) ||
      !std::isfinite(settings.desiredCellPixels) || settings.desiredCellPixels <= 0 ||
      !std::isfinite(settings.minimumSpacing) || settings.minimumSpacing <= 0 ||
      !std::isfinite(settings.maximumSpacing) || settings.maximumSpacing < settings.minimumSpacing)
    return grid;
  const auto ray = screenPointToRay(view, {view.rect.x + view.rect.width*.5f,
                                         view.rect.y + view.rect.height*.5f});
  if (!ray.valid) return grid;
  const float altitude = std::abs(ray.origin[1] - settings.planeHeight);
  float depth = std::max(altitude, view.frustum.nearPlane * 10);
  float centerX = ray.origin[0], centerZ = ray.origin[2];
  // Limite contínuo perto do horizonte: evita trocar abruptamente entre
  // a interseção distante e a posição da câmera ao atravessar um limiar.
  const float facing = (settings.planeHeight-ray.origin[1])*ray.direction[1];
  if (facing > 0) {
    const float hit = altitude/std::max(std::abs(ray.direction[1]), .1f);
    depth = std::max(depth, hit);
    centerX += hit*ray.direction[0]; centerZ += hit*ray.direction[2];
  }
  const float wanted = std::clamp(2*depth*view.frustum.tangentHalfVertical*
      settings.desiredCellPixels/view.rect.height, settings.minimumSpacing, settings.maximumSpacing);
  grid.spacing = std::clamp(std::pow(10.0f, std::floor(std::log10(wanted))),
                            settings.minimumSpacing, settings.maximumSpacing);
  const float minorOpacity = std::clamp((10-wanted/grid.spacing)/9, 0.0f, 1.0f);
  const double cellX = std::floor(static_cast<double>(centerX)/grid.spacing);
  const double cellZ = std::floor(static_cast<double>(centerZ)/grid.spacing);
  constexpr int half = 64;
  for (int orientation = 0; orientation < 2; ++orientation) {
    const double centerCell = orientation == 0 ? cellX : cellZ;
    const double alongCell = orientation == 0 ? cellZ : cellX;
    for (int i = -half; i <= half; ++i) {
      const double cell = centerCell+i;
      auto &line = grid.lines[grid.count++];
      const int fixed = orientation == 0 ? 0 : 2, along = orientation == 0 ? 2 : 0;
      line.from[1] = line.to[1] = settings.planeHeight;
      line.from[fixed] = line.to[fixed] = static_cast<float>(cell*grid.spacing);
      line.from[along] = static_cast<float>((alongCell-half)*grid.spacing);
      line.to[along] = static_cast<float>((alongCell+half)*grid.spacing);
      line.major = std::fmod(cell,10.0) == 0;
      line.axis = cell == 0 ? (orientation == 0 ? 2u : 1u) : 0u;
      const float edge = std::clamp((half-std::abs(i))/12.0f,0.0f,1.0f);
      line.opacity = edge*(line.major ? 1 : minorOpacity);
    }
  }
  return grid;
}
} // namespace ae::editor
