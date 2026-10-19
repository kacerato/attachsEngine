#include "platform/android/android_telemetry.h"
#include "platform/android/android_launch_options.h"

#include <android/log.h>
#include <cstdio>
#include <cstring>
#include <time.h>

namespace ae::platform::android {
namespace {
constexpr const char *LogTag = "Aether.Android";

bool readClock(clockid_t clock, u64 &out) {
  timespec value{};
  if (clock_gettime(clock, &value) != 0) return false;
  out = static_cast<u64>(value.tv_sec) * 1'000'000'000ULL + static_cast<u64>(value.tv_nsec);
  return true;
}

// A sessão vem do shell como UUID; qualquer outra coisa poderia sair do diretório.
bool safeSessionId(const char *value) {
  const usize length = std::strlen(value);
  if (length == 0 || length > 64) return false;
  for (usize i = 0; i < length; ++i) {
    const char c = value[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || c == '-')) return false;
  }
  return true;
}
} // namespace

const char *telemetryTierName(rhi::DeviceProfile profile) {
  switch (profile) {
    case rhi::DeviceProfile::S: return "s";
    case rhi::DeviceProfile::A: return "a";
    case rhi::DeviceProfile::B: return "b";
    case rhi::DeviceProfile::C: return "c";
  }
  return "c";
}

bool AndroidTelemetry::configure(ANativeActivity *activity) {
  enabled_ = false;
  char directory[512]{}, session[80]{};
  if (!readStringLaunchOption(activity, "aether.telemetry_dir", directory, sizeof(directory)) ||
      !readStringLaunchOption(activity, "aether.telemetry_session", session, sizeof(session)))
    return false;
  if (directory[0] != '/' || !safeSessionId(session)) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "[Telemetry] opções inválidas; coleta desligada.");
    return false;
  }
  queuePath_ = std::string(directory) + "/" + session + ".jsonl";
  environmentPath_ = std::string(directory) + "/" + session + ".env.json";
  recorder_.reset();
  enabled_ = true;
  __android_log_print(ANDROID_LOG_INFO, LogTag, "[Telemetry] coleta ligada (janelas de %u quadros).",
                      profiler::TelemetryRecorder::WindowFrames);
  return true;
}

void AndroidTelemetry::writeEnvironment(const rhi::DeviceFeatures &features, rhi::DeviceProfile recommended,
                                        rhi::DeviceProfile resolved) {
  if (!enabled_) return;
  const std::string name(features.deviceName.data(), strnlen(features.deviceName.data(), features.deviceName.size()));
  const std::string json = std::string("{\"gpu\":{\"name\":") + profiler::telemetryJsonString(name) +
      ",\"vendor\":" + std::to_string(features.vendorId) + ",\"device\":" + std::to_string(features.deviceId) +
      "},\"tier_recommended\":\"" + telemetryTierName(recommended) + "\",\"tier_resolved\":\"" +
      telemetryTierName(resolved) + "\"}\n";
  // Escreve ao lado e renomeia: o shell nunca lê um ambiente pela metade.
  const std::string staged = environmentPath_ + ".tmp";
  FILE *file = std::fopen(staged.c_str(), "wb");
  if (file == nullptr) return;
  const bool written = std::fwrite(json.data(), 1, json.size(), file) == json.size();
  if (std::fclose(file) != 0 || !written || std::rename(staged.c_str(), environmentPath_.c_str()) != 0)
    std::remove(staged.c_str());
}

void AndroidTelemetry::record(const profiler::RenderPhaseTimings &phases,
                              const profiler::TelemetryWindowContext &context) {
  if (!enabled_) return;
  profiler::FrameCounters counters{};
  if (!readClock(CLOCK_MONOTONIC, counters.wallNs) ||
      !readClock(CLOCK_PROCESS_CPUTIME_ID, counters.processCpuNs) ||
      !readClock(CLOCK_THREAD_CPUTIME_ID, counters.threadCpuNs)) {
    enabled_ = false;
    return;
  }
  std::string line;
  if (!recorder_.record(counters, phases, context, line)) return;
  // Abre e fecha a cada janela (uma a cada ~10 s): o shell pode renomear o
  // arquivo para enviá-lo e a próxima janela simplesmente cria outro.
  FILE *file = std::fopen(queuePath_.c_str(), "ab");
  if (file == nullptr) return;
  if (std::fseek(file, 0, SEEK_END) == 0 && std::ftell(file) < MaximumQueueBytes) {
    line.push_back('\n');
    std::fwrite(line.data(), 1, line.size(), file);
  }
  std::fclose(file);
}

} // namespace ae::platform::android
