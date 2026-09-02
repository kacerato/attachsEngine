#pragma once

#include "core/frame_policy.h"
#include "profiler/frame_statistics.h"

namespace ae::profiler {

// Um frame sempre possui uma etapa limitante, mas "CPU e GPU com a mesma
// porcentagem" não é uma meta útil: as duas trabalham em pipeline. A política
// distingue somente pressão contra os budgets da cadência. Trabalho ocioso não
// é preenchido artificialmente e espera de apresentação não vira redução de
// resolução por engano.
enum class FramePressureKind : u32 {
  Unknown = 0,
  WithinBudget,
  Cpu,
  Gpu,
  Mixed,
  Presentation,
};

struct FramePressurePolicy final {
  // p95 acima de 90% do budget já perdeu a margem operacional, mesmo antes de
  // ultrapassar o intervalo físico do display.
  double lanePressureRatio = 0.90;
  // Uma janela só é presentation-limited se o p95 do intervalo também estiver
  // atrasado. Isso impede que uma espera normal de VSYNC seja chamada gargalo.
  double lateIntervalRatio = 1.05;
};

struct FramePressureResult final {
  FramePressureKind kind = FramePressureKind::Unknown;
  double cpuP95BudgetRatio = 0;
  double gpuP95BudgetRatio = 0;
  double intervalP95BudgetRatio = 0;
  double presentationWaitP95Ms = 0;
};

FramePressureResult classifyFramePressure(const FrameProfileSummary &summary,
                                          const FrameBudget &budget,
                                          const FramePressurePolicy &policy = {});
const char *framePressureKindName(FramePressureKind kind);

} // namespace ae::profiler
