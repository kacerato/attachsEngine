#pragma once
#include <algorithm>
#include <cmath>

namespace ae::editor {
// The visual scale is shared by drawing and pointer routing. Exact entry
// remains available for world coordinates, seeds and values beyond the
// practical scale.
struct TerrainControlRange {
  float low = 0, high = 1, step = 0;
  bool enabled = false, quadratic = false;
  float value(float t) const {
    t = std::clamp(t, 0.f, 1.f);
    float v = std::lerp(low, high, quadratic ? t * t : t);
    if (step > 0)
      v = low + std::round((v - low) / step) * step;
    return std::clamp(v, low, high);
  }
  float position(float v) const {
    float t = high > low ? std::clamp((v - low) / (high - low), 0.f, 1.f) : 0;
    return quadratic ? std::sqrt(t) : t;
  }
};
template <class State>
TerrainControlRange terrainControlRange(const State &s, unsigned field) {
  const auto &r = s.terrainDraft;
  TerrainControlRange range{0, 1, 0, true, false};
  if ((field >= 64 && field <= 76) || (field >= 96 && field <= 110)) {
    range.enabled = s.terrainBrush.layer < r.surface.layers.size();
    if (field == 64 || field == 65)
      range = {.01f, 2048, 0, range.enabled, true};
    else if (field == 66 || field == 67)
      range = {-2048, 2048, 0, range.enabled};
    else if (field == 68)
      range = {-180, 180, 0, range.enabled};
    else if (field == 72)
      range = {0, 4, 0, range.enabled};
    else if (field == 73)
      range = {.1f, 32, 0, range.enabled, true};
    else if (field == 74 || field == 75)
      range = {.01f, 16, 0, range.enabled, true};
    else if (field == 76)
      range = {0, 8, 0, range.enabled};
    else if (field == 96)
      range = {0, 8, 0, range.enabled};
    else
      range = {0, 1, 0, range.enabled};
    return range;
  }
  switch (field) {
  case 1:
    range = {8, 2048, 0, true, true};
    break;
  case 2: {
    unsigned maximum = 128;
    if (unsigned(r.mode) != 0)
      while (maximum > 8 &&
             (maximum + 1ull) * (maximum + 1) * (r.verticalCells + 1) > 600000)
        maximum -= 8;
    range = {8, float(maximum), 8, true};
    break;
  }
  case 3:
    range = {0, 512, 0, true, true};
    break;
  case 4:
    range = {0, 256, 0, true, true};
    break;
  case 5:
    range = {.1f, 32, 0, true, true};
    break;
  case 6:
    range = {1, 8, 1, true};
    break;
  case 7:
    range = {
        8,
        float(std::min(
            64u, unsigned(600000ull / ((r.cells + 1ull) * (r.cells + 1)) - 1))),
        1, true};
    break;
  case 8:
    range = {.1f, 256, 0, true, true};
    break;
  case 9:
    range = {.1f, std::max(8.f, r.size * .25f), 0, true, true};
    break;
  case 10:
    range = {0, 8, 0, true, true};
    break;
  case 14:
    range = {.1f, 512, 0, true, true};
    break;
  case 15:
  case 19:
  case 24:
  case 25:
  case 36:
  case 38:
  case 39:
    break;
  case 16:
  case 22:
  case 23:
    range = {-180, 180, 0, true};
    break;
  case 17:
  case 20:
  case 21:
    range = {.1f, 10, 0, true, true};
    break;
  case 18:
    range = {.05f, 1, 0, true};
    break;
  case 26:
  case 28:
    range = {1, 4, 1, true};
    break;
  case 29:
    range = {.01f, 10, 0, r.lodLevels > 1, true};
    break;
  case 60:
    range = {8, 512, 8, unsigned(r.mode) == 0};
    break;
  case 34:
    range = {-512, 512, 0, true};
    break;
  case 37:
    range = {-2048, 2048, 0, true};
    break;
  case 35:
    range = {0, 89.9f, 0, true};
    break;
  case 32:
  case 33:
  case 40: {
    if (s.terrainPath >= r.paths.size()) {
      range.enabled = false;
      break;
    }
    const auto &p = r.paths[s.terrainPath];
    if (field == 32) {
      const float allowed =
          p.keepWidthRatio
              ? std::nextafter(
                    std::min(2048.f, 2048.f * p.width / p.shoulderWidth), .1f)
              : p.shoulderWidth;
      range = {.1f, std::min(allowed, std::max(8.f, r.size * .5f)), 0, true,
               true};
    } else if (field == 33)
      range = {p.width,
               std::max(p.width, std::min(2048.f, std::max(8.f, r.size))), 0,
               true, true};
    else
      range = {0, 512, 0, true, true};
    break;
  }
  case 50:
  case 51: {
    if (s.terrainPath >= r.paths.size()) {
      range.enabled = false;
      break;
    }
    const auto &p = r.paths[s.terrainPath];
    const auto &curve = s.terrainSection == 2 ? p.textureProfile : p.profile;
    if (curve.empty()) {
      range.enabled = false;
      break;
    }
    const auto selected =
        std::min(s.terrainProfilePoint, unsigned(curve.size() - 1));
    if (field == 51)
      range = s.terrainSection == 2 ? TerrainControlRange{0, 1, 0, true}
                                    : TerrainControlRange{-1, 2, 0, true};
    else if (selected && selected + 1 < curve.size()) {
      range = {std::nextafter(curve[selected - 1].x, 1.f),
               std::nextafter(curve[selected + 1].x, 0.f), 0, true};
      range.enabled = range.low < range.high;
    } else
      range.enabled = false;
    break;
  }
  default:
    range.enabled = false;
    break;
  }
  return range;
}
} // namespace ae::editor
