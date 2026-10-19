#pragma once

// GT: telemetria de desempenho opcional no Android. Só liga quando o shell passa
// aether.telemetry_dir e aether.telemetry_session, o que ele faz apenas depois do
// consentimento do testador. O jogo exportado nunca recebe essas opções.
//
// O nativo não envia nada: acrescenta uma linha por janela em
// <dir>/<sessão>.jsonl e escreve <dir>/<sessão>.env.json com a GPU e o nível.
// O shell junta isso ao envelope do aparelho e envia (ver Telemetry.java).

#include "core/base.h"
#include "profiler/telemetry_recorder.h"
#include "rhi/device_profile.h"

#include <string>

struct ANativeActivity;

namespace ae::platform::android {

class AndroidTelemetry final {
public:
  // Teto da fila em disco: ~1.400 janelas, mais de 3 h de uso sem envio.
  static constexpr long MaximumQueueBytes = 1024 * 1024;

  bool configure(ANativeActivity *activity);
  bool enabled() const { return enabled_; }
  void reset() { recorder_.reset(); }
  void writeEnvironment(const rhi::DeviceFeatures &features, rhi::DeviceProfile recommended,
                        rhi::DeviceProfile resolved);
  void record(const profiler::RenderPhaseTimings &phases, const profiler::TelemetryWindowContext &context);

private:
  bool enabled_ = false;
  std::string queuePath_, environmentPath_;
  profiler::TelemetryRecorder recorder_{};
};

const char *telemetryTierName(rhi::DeviceProfile profile);

} // namespace ae::platform::android
