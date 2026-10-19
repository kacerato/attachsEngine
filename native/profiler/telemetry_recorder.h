#pragma once

// GT: transforma o fluxo de quadros em janelas "astra.telemetry.window/1" para a
// telemetria opcional dos testadores. Portátil e sem relógio próprio: quem chama
// lê os contadores (no Android, clock_gettime) e grava a linha devolvida.
//
// Uma janela tem WindowFrames quadros depois de WarmupMilliseconds de aquecimento.
// Trocar de cena, de modo (edição/Play) ou de nível reinicia a janela, para nunca
// misturar números de situações diferentes no mesmo agregado.

#include "core/base.h"
#include "profiler/frame_statistics.h"
#include "profiler/telemetry_window.h"

#include <string>

namespace ae::profiler {

class TelemetryRecorder final {
public:
  static constexpr u32 WindowFrames = FrameStatistics::Capacity;
  static constexpr u32 WarmupMilliseconds = 5000;

  TelemetryRecorder();
  void reset();
  u64 windowsEmitted() const { return sequence_; }

  // Devolve true e preenche outLine (JSON numa linha) quando uma janela fecha.
  // context.sequence é ignorado: a sequência é da sessão, mantida aqui.
  bool record(FrameCounters now, const RenderPhaseTimings &phases,
              const TelemetryWindowContext &context, std::string &outLine);

private:
  FrameStatistics statistics_{};
  u64 sequence_ = 0;
  bool identityValid_ = false;
  std::string scene_, mode_, tier_;
};

} // namespace ae::profiler
