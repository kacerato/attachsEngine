#pragma once

#include "renderer/rendering_policy.h"
#include "renderer/rendering_settings_file.h"

#include <functional>

namespace ae::runtime {

// Estado gráfico de uma única sessão Play. A autoria permanece imutável; mudanças
// feitas por script viram pedidos numerados e só alteram Effective após o host
// confirmar a política realmente publicada no frame boundary.
class RuntimeRenderingState final {
public:
  // `restoringAuthoring` permite ao host substituir um pedido ainda na fila
  // quando Stop acontece antes de ele ser consumido.
  using RequestSink = std::function<bool(u64, const renderer::ProjectRenderingSettings &, bool restoringAuthoring)>;

  void configure(renderer::ProjectRenderingSettings authored,
                 renderer::RenderingCapabilities capabilities,
                 renderer::ThermalPressure thermal, RequestSink sink) {
    authored_ = authored; capabilities_ = capabilities; thermal_ = thermal; sink_ = std::move(sink);
  }
  bool begin(u32 world, const renderer::ResolvedRenderingPolicy &effective);
  void end();
  bool request(u32 expectedWorld, const renderer::ProjectRenderingSettings &settings, u64 &requestId);
  bool complete(u64 requestId, bool success, const renderer::ResolvedRenderingPolicy &effective,
                bool effectiveAvailable=true);
  // Host report of what the renderer really executed in the last frame. It is
  // separate from `effective`: a context can fail after the policy resolved.
  void setExecution(renderer::UpscalingFilter executed, renderer::TemporalUpscalerAvailability status) {
    executedUpscaler_=executed;executedStatus_=status;
  }
  renderer::UpscalingFilter executedUpscaler() const noexcept { return executedUpscaler_; }
  renderer::TemporalUpscalerAvailability executedStatus() const noexcept { return executedStatus_; }
  bool refresh(u32 expectedWorld, renderer::RenderingCapabilities capabilities,
               renderer::ThermalPressure thermal, const renderer::ResolvedRenderingPolicy &effective) {
    if(!active(expectedWorld)) return false;
    capabilities_=capabilities;thermal_=thermal;effective_=effective;return true;
  }

  bool active(u32 expectedWorld) const noexcept { return world_ && world_ == expectedWorld; }
  u32 world() const noexcept { return world_; }
  bool pending() const noexcept { return pendingId_ != 0; }
  u64 pendingId() const noexcept { return pendingId_; }
  bool lastRequestSucceeded() const noexcept { return lastRequestSucceeded_; }
  bool effectiveAvailable() const noexcept { return effectiveAvailable_; }
  const renderer::ProjectRenderingSettings &requested() const noexcept { return requested_; }
  const renderer::ResolvedRenderingPolicy &effective() const noexcept { return effective_; }
  const renderer::RenderingCapabilities &capabilities() const noexcept { return capabilities_; }

private:
  static bool validate(const renderer::ProjectRenderingSettings &settings);
  renderer::ProjectRenderingSettings authored_{}, requested_{}, pendingSettings_{};
  renderer::ResolvedRenderingPolicy effective_{};
  renderer::RenderingCapabilities capabilities_{};
  renderer::ThermalPressure thermal_ = renderer::ThermalPressure::None;
  RequestSink sink_;
  renderer::UpscalingFilter executedUpscaler_ = renderer::UpscalingFilter::Bilinear;
  renderer::TemporalUpscalerAvailability executedStatus_ = renderer::TemporalUpscalerAvailability::Available;
  u32 world_ = 0;
  u64 nextRequestId_ = 1, pendingId_ = 0;
  bool changed_ = false, lastRequestSucceeded_ = true, effectiveAvailable_ = false;
};

} // namespace ae::runtime
