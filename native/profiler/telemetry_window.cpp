#include "profiler/telemetry_window.h"

#include <cmath>
#include <cstdio>

namespace ae::profiler {
namespace {

void appendNumber(std::string &out, double value) {
  if (!std::isfinite(value)) { out += "null"; return; }
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%.4g", value);
  out += buffer;
}

// [média, p50, p95, p99, máximo] em ms; null quando a métrica não foi medida.
void appendDistribution(std::string &out, const char *name, const Distribution &value, bool measured) {
  out += ",\"";
  out += name;
  out += "\":";
  if (!measured) { out += "null"; return; }
  out += '[';
  appendNumber(out, value.mean); out += ',';
  appendNumber(out, value.p50); out += ',';
  appendNumber(out, value.p95); out += ',';
  appendNumber(out, value.p99); out += ',';
  appendNumber(out, value.maximum);
  out += ']';
}

const Distribution &metric(const FrameProfileSummary &summary, FrameMetric which) {
  return summary.metrics[static_cast<u32>(which)];
}

} // namespace

std::string telemetryJsonString(std::string_view text) {
  std::string out;
  out.reserve(text.size() + 2);
  out += '"';
  for (const char c : text) {
    const auto byte = static_cast<unsigned char>(c);
    if (c == '"' || c == '\\') { out += '\\'; out += c; }
    else if (byte < 0x20) {
      char escaped[8];
      std::snprintf(escaped, sizeof(escaped), "\\u%04x", byte);
      out += escaped;
    } else out += c;
  }
  out += '"';
  return out;
}

std::string telemetryWindowJson(const FrameProfileSummary &summary, const TelemetryWindowContext &context) {
  if (summary.samples == 0 || !(summary.elapsedMs > 0)) return {};
  std::string out;
  out.reserve(640);
  out += "{\"schema\":";
  out += telemetryJsonString(TelemetryWindowSchema);
  out += ",\"seq\":";
  out += std::to_string(context.sequence);
  out += ",\"scene\":";
  out += telemetryJsonString(context.scene);
  out += ",\"mode\":";
  out += telemetryJsonString(context.mode);
  out += ",\"tier\":";
  out += telemetryJsonString(context.tier);
  out += ",\"frames\":";
  out += std::to_string(summary.samples);
  out += ",\"elapsed_ms\":";
  appendNumber(out, summary.elapsedMs);
  out += ",\"fps\":";
  appendNumber(out, summary.presentFps);
  appendDistribution(out, "interval_ms", metric(summary, FrameMetric::Interval), true);
  appendDistribution(out, "thread_cpu_ms", metric(summary, FrameMetric::ThreadCpu), true);
  appendDistribution(out, "process_cpu_ms", metric(summary, FrameMetric::ProcessCpu), true);
  const auto &gpu = metric(summary, FrameMetric::GpuFrame);
  appendDistribution(out, "gpu_ms", gpu, gpu.maximum > 0);
  out += ",\"render\":[";
  out += std::to_string(context.renderWidth); out += ',';
  out += std::to_string(context.renderHeight); out += ',';
  appendNumber(out, context.renderScale);
  out += "],\"thermal\":";
  out += std::to_string(context.thermalStatus);
  out += ",\"thermal_headroom\":";
  if (context.thermalHeadroom >= 0) appendNumber(out, context.thermalHeadroom);
  else out += "null";
  out += ",\"target_fps\":";
  out += std::to_string(context.targetFps);
  out += ",\"draws\":";
  out += std::to_string(context.drawCalls);
  out += ",\"triangles\":";
  out += std::to_string(context.triangles);
  out += '}';
  return out;
}

} // namespace ae::profiler
