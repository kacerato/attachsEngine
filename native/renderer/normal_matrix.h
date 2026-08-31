#pragma once

#include "core/base.h"

#include <cmath>

namespace ae::renderer {

// Builds transpose(inverse(mat3(model))) once when a render transform changes.
// Input and output are column-major. Each output column occupies a vec4-sized
// slot so it can be consumed directly as an instanced Vulkan vertex attribute;
// column 0.w carries the determinant sign used to preserve tangent handedness.
// On failure, `outColumns` is left untouched.
inline bool buildNormalMatrix(const float *model, float *outColumns) noexcept {
  if (model == nullptr || outColumns == nullptr) return false;

  double a[3][3]{};
  double maximum = 0.0;
  for (u32 row = 0; row < 3; ++row) {
    for (u32 column = 0; column < 3; ++column) {
      const double value = model[column * 4 + row];
      if (!std::isfinite(value)) return false;
      a[row][column] = value;
      maximum = std::fmax(maximum, std::fabs(value));
    }
  }
  if (maximum == 0.0) return false;

  const double determinant =
      a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1]) -
      a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0]) +
      a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);
  const double scaledDeterminant = determinant / (maximum * maximum * maximum);
  if (!std::isfinite(determinant) || !std::isfinite(scaledDeterminant) ||
      std::fabs(scaledDeterminant) <= 1.0e-8) return false;

  // Cofactor matrix divided by det is inverse-transpose.
  const double normal[3][3] = {
      {(a[1][1] * a[2][2] - a[1][2] * a[2][1]) / determinant,
       (a[1][2] * a[2][0] - a[1][0] * a[2][2]) / determinant,
       (a[1][0] * a[2][1] - a[1][1] * a[2][0]) / determinant},
      {(a[0][2] * a[2][1] - a[0][1] * a[2][2]) / determinant,
       (a[0][0] * a[2][2] - a[0][2] * a[2][0]) / determinant,
       (a[0][1] * a[2][0] - a[0][0] * a[2][1]) / determinant},
      {(a[0][1] * a[1][2] - a[0][2] * a[1][1]) / determinant,
       (a[0][2] * a[1][0] - a[0][0] * a[1][2]) / determinant,
       (a[0][0] * a[1][1] - a[0][1] * a[1][0]) / determinant},
  };
  float encoded[12]{};
  for (u32 column = 0; column < 3; ++column) {
    for (u32 row = 0; row < 3; ++row) {
      const float value = static_cast<float>(normal[row][column]);
      if (!std::isfinite(value)) return false;
      encoded[column * 4 + row] = value;
    }
  }
  encoded[3] = determinant < 0.0 ? -1.0f : 1.0f;
  for (u32 index = 0; index < 12; ++index) outColumns[index] = encoded[index];
  return true;
}

} // namespace ae::renderer
