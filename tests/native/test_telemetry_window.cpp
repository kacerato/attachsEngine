#include "harness.h"
#include "profiler/telemetry_window.h"

#include <string>

using namespace ae;
using namespace ae::profiler;

namespace {
void collect(double gpuMs, FrameProfileSummary &summary) {
  FrameStatistics statistics;
  AE_EXPECT_TRUE(statistics.configure(0, 30), "janela de 30 quadros sem aquecimento");
  FrameCounters counters{};
  RenderPhaseTimings phases{};
  phases.gpuFrameMs = gpuMs;
  FrameSampleResult result = FrameSampleResult::Collecting;
  for (u32 frame = 0; frame <= 30 && result != FrameSampleResult::WindowReady; ++frame) {
    counters.wallNs += 33'333'333;
    counters.threadCpuNs += 12'000'000;
    counters.processCpuNs += 15'000'000;
    result = statistics.record(counters, phases);
  }
  AE_EXPECT_TRUE(result == FrameSampleResult::WindowReady, "janela fechada");
  AE_EXPECT_TRUE(statistics.summarize(summary), "resumo disponível");
}
}

AE_TEST(telemetry_window_json_reports_schema_metrics_and_context) {
  FrameProfileSummary summary;
  collect(18.5, summary);
  TelemetryWindowContext context;
  context.sequence = 7; context.scene = "vila-astra"; context.mode = "play"; context.tier = "s";
  context.renderWidth = 2772; context.renderHeight = 1280; context.renderScale = 1.0f;
  context.thermalStatus = 1; context.thermalHeadroom = 0.42f; context.targetFps = 60;
  context.drawCalls = 128; context.triangles = 900000;
  const auto json = telemetryWindowJson(summary, context);
  AE_EXPECT_TRUE(json.starts_with("{\"schema\":\"astra.telemetry.window/1\",\"seq\":7"), json.c_str());
  AE_EXPECT_TRUE(json.find("\"scene\":\"vila-astra\"") != std::string::npos, "rótulo de cena");
  AE_EXPECT_TRUE(json.find("\"frames\":30") != std::string::npos, "quadros da janela");
  AE_EXPECT_TRUE(json.find("\"thread_cpu_ms\":[12,12,12,12,12]") != std::string::npos, json.c_str());
  AE_EXPECT_TRUE(json.find("\"gpu_ms\":[18.5,") != std::string::npos, "GPU medida");
  AE_EXPECT_TRUE(json.find("\"render\":[2772,1280,1]") != std::string::npos, "resolução e escala");
  AE_EXPECT_TRUE(json.find("\"thermal\":1,\"thermal_headroom\":0.42") != std::string::npos, "térmica");
  AE_EXPECT_TRUE(json.back() == '}' && json.find('\n') == std::string::npos, "um objeto numa linha");
}

AE_TEST(telemetry_window_json_marks_unmeasured_gpu_as_null) {
  FrameProfileSummary summary;
  collect(0.0, summary);
  TelemetryWindowContext context;
  context.scene = "x"; context.mode = "edit"; context.tier = "c";
  const auto json = telemetryWindowJson(summary, context);
  AE_EXPECT_TRUE(json.find("\"gpu_ms\":null") != std::string::npos, "sem timer de GPU não vira zero");
  AE_EXPECT_TRUE(json.find("\"thermal_headroom\":null") != std::string::npos, "headroom indisponível");
}

AE_TEST(telemetry_window_json_refuses_empty_summary_and_escapes_text) {
  FrameProfileSummary empty;
  AE_EXPECT_TRUE(telemetryWindowJson(empty, {}).empty(), "resumo vazio não gera registro");
  AE_EXPECT_TRUE(telemetryJsonString("a\"b\\c\n") == "\"a\\\"b\\\\c\\u000a\"", "escape JSON");
}
