#pragma once
#include "core/base.h"

namespace ae::editor {
// Analytic two-bone IK (raiz -> meio -> ponta) in world space. Keeps the
// current bend plane, so a knee/elbow never flips sides. Matrices are
// column-major 4x4 world transforms; scale/shear in the 3x3 part is preserved
// by rotating the existing basis. Returns the solved worlds of root and mid;
// the end inherits through the hierarchy. Unreachable targets stretch the
// chain straight toward the target without changing bone lengths.
struct TwoBoneIkResult {
  float root[16]{},mid[16]{};
  float reached[3]{};
  bool clamped=false;
};
bool solveTwoBoneIk(const float rootWorld[16],const float midWorld[16],const float endWorld[16],
                    const float target[3],TwoBoneIkResult &out);
} // namespace ae::editor
