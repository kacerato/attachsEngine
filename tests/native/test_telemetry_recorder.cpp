#include "harness.h"
#include "profiler/telemetry_recorder.h"

#include <string>

using namespace ae;
using namespace ae::profiler;

namespace {
struct Feed {
  TelemetryRecorder recorder;
  FrameCounters counters{};
  RenderPhaseTimings phases{};
  // Avança `frames` quadros de 16,6 ms e conta quantas janelas fecharam.
  u32 run(u32 frames, const TelemetryWindowContext &context, std::string &last) {
    u32 windows = 0;
    for (u32 frame = 0; frame < frames; ++frame) {
      counters.wallNs += 16'666'667;
      counters.threadCpuNs += 9'000'000;
      counters.processCpuNs += 11'000'000;
      std::string line;
      if (recorder.record(counters, phases, context, line)) { ++windows; last = line; }
    }
    return windows;
  }
};

TelemetryWindowContext context(std::string_view mode) {
  TelemetryWindowContext value{};
  value.scene = "authored-project";
  value.mode = mode;
  value.tier = "s";
  value.targetFps = 60;
  return value;
}

// 5 s de aquecimento a 60 Hz = 300 quadros, mais o primeiro quadro de referência.
constexpr u32 WarmupFrames = 301;
}

AE_TEST(telemetry_recorder_emits_numbered_windows_after_warmup) {
  Feed feed;
  std::string last;
  AE_EXPECT_TRUE(feed.run(WarmupFrames + TelemetryRecorder::WindowFrames - 1, context("edit"), last) == 0,
                 "nenhuma janela antes de 600 quadros medidos");
  AE_EXPECT_TRUE(feed.run(1, context("edit"), last) == 1, "primeira janela fecha no quadro 600");
  AE_EXPECT_TRUE(last.find("\"seq\":0") != std::string::npos, last.c_str());
  AE_EXPECT_TRUE(last.find("\"mode\":\"edit\"") != std::string::npos, "modo na janela");
  AE_EXPECT_TRUE(feed.run(TelemetryRecorder::WindowFrames, context("edit"), last) == 1,
                 "janelas seguintes sem novo aquecimento");
  AE_EXPECT_TRUE(last.find("\"seq\":1") != std::string::npos, last.c_str());
  AE_EXPECT_TRUE(feed.recorder.windowsEmitted() == 2, "duas janelas na sessão");
}

AE_TEST(telemetry_recorder_restarts_window_when_mode_changes) {
  Feed feed;
  std::string last;
  feed.run(WarmupFrames + 400, context("edit"), last);
  // Entrar em Play no meio da janela descarta os 400 quadros de edição.
  AE_EXPECT_TRUE(feed.run(WarmupFrames + TelemetryRecorder::WindowFrames - 2, context("play"), last) == 0,
                 "janela de Play exige aquecimento e 600 quadros próprios");
  AE_EXPECT_TRUE(feed.run(2, context("play"), last) == 1, "janela de Play fecha");
  AE_EXPECT_TRUE(last.find("\"mode\":\"play\"") != std::string::npos, last.c_str());
  AE_EXPECT_TRUE(last.find("\"seq\":0") != std::string::npos, "sequência continua da sessão");
}
