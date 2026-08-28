#pragma once
#include "core/base.h"
#include <cmath>
#include <limits>

namespace ae::profiler {
// O intervalo deve caber em uma volta do contador. Bits superiores não são
// definidos quando timestampValidBits < 64; o delta usa aritmética modular.
inline bool gpuTimestampMilliseconds(u64 begin, u64 end, u32 validBits,
                                     double periodNs, double &milliseconds) {
  milliseconds = std::numeric_limits<double>::quiet_NaN();
  if (validBits == 0 || validBits > 64 || !std::isfinite(periodNs) || periodNs <= 0) return false;
  const u64 mask = validBits == 64 ? UINT64_MAX : ((u64{1} << validBits) - 1);
  const u64 delta = (end - begin) & mask;
  if (delta == 0) return false;
  milliseconds = static_cast<double>(delta) * periodNs / 1.0e6;
  return std::isfinite(milliseconds) && milliseconds > 0;
}
} // namespace ae::profiler
