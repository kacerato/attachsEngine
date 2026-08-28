#pragma once
#include "core/base.h"
#include <algorithm>
#include <span>

namespace ae::platform {
// Corpus sintético RGB opaco: blocos sólidos, rampas e checker intrabloco.
// Não representa qualidade de compressão de fotos, normais, HDR ou alpha.
inline bool fillAstcProbePattern(std::span<u8> pixels, u32 width, u32 height) {
  if (width == 0 || height == 0 || static_cast<u64>(width) * height > SIZE_MAX / 4 ||
      pixels.size() != static_cast<usize>(width) * height * 4) return false;
  for (u32 y = 0; y < height; ++y) {
    for (u32 x = 0; x < width; ++x) {
      const u32 bx = x / 4, by = y / 4;
      const u32 kind = (bx + by) % 3;
      const u8 ramp = static_cast<u8>(((x % 4) + 4 * (y % 4)) * 17);
      const u8 checker = ((x + y) % 2) ? 255 : 0;
      const usize offset = (static_cast<usize>(y) * width + x) * 4;
      pixels[offset] = kind == 0 ? static_cast<u8>(bx * 7) : kind == 1 ? ramp : checker;
      pixels[offset + 1] = kind == 0 ? static_cast<u8>(by * 11) : kind == 1 ? ramp : checker;
      pixels[offset + 2] = kind == 0 ? static_cast<u8>((bx + by) * 5) : kind == 1 ? ramp : checker;
      pixels[offset + 3] = 255;
    }
  }
  return true;
}

inline bool compareAstcProbePixels(std::span<const u8> source, std::span<const u8> decoded,
                                  u32 &maxDifference) {
  maxDifference = 0;
  if (source.empty() || source.size() != decoded.size() || source.size() % 4 != 0) return false;
  for (usize i = 0; i < source.size(); ++i) {
    const auto difference = source[i] > decoded[i] ? source[i] - decoded[i] : decoded[i] - source[i];
    maxDifference = std::max(maxDifference, static_cast<u32>(difference));
  }
  return true;
}
} // namespace ae::platform
