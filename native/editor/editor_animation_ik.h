#pragma once
// O estudio e o Play usam o mesmo solver: a matematica vive no runtime.
#include "runtime/ik_solvers.h"
namespace ae::editor {
using runtime::TwoBoneIkResult;
using runtime::solveTwoBoneIk;
} // namespace ae::editor
