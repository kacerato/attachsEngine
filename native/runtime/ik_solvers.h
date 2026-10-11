#pragma once
#include "core/base.h"
#include <array>
#include <span>
#include <vector>

namespace ae::runtime {
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
                    const float target[3],TwoBoneIkResult &out,const float *pole=nullptr);
// Cadeia de N juntas (raiz..ponta). CCD gira cada junta para apontar a ponta ao
// alvo, da ponta para a raiz; FABRIK alterna passes ponta->raiz e raiz->ponta.
// Comprimentos sao preservados; alvo fora do alcance estica a cadeia em linha
// reta, sem alongar ossos. O resultado informa residuo e convergencia.
enum class ChainIkSolver : u8 { Ccd, Fabrik };
struct ChainIkResult {
  u32 iterations=0;
  float residual=0;
  bool converged=false,clamped=false;
};
bool solveChainIk(ChainIkSolver solver,std::span<const std::array<float,16>> worlds,const float target[3],
                  u32 maximumIterations,float tolerance,std::vector<std::array<float,16>> &out,ChainIkResult &result);
} // namespace ae::runtime
