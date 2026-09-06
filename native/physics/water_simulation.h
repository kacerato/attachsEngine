#pragma once

#include "physics/water_runtime.h"

namespace ae::physics {

struct WaterSimulationSettings final {
  double fixedStep = 1.0 / 60.0;
  double maximumFrameTime = .25;
  u32 maximumSteps = 8;
  WaterRuntimeSettings water{};
};

struct WaterSimulationHooks final {
  void *context = nullptr;
  // Update borrowed spectral mirrors here, before WaterWorld queries them.
  bool (*beforeStep)(void *, double) = nullptr;
  // Called for EVERY step, so catch-up cannot overwrite an unconsumed event.
  void (*afterStep)(void *, std::span<const WaterContactEvent>) = nullptr;
};

enum class WaterSimulationError : u32 { None, InvalidInput, Provider, Forces, Physics };
struct WaterSimulationFrame final {
  WaterSimulationError error = WaterSimulationError::None;
  u32 steps = 0;
  u32 physicsErrors = 0;
  double simulationTime = 0;
  double interpolation = 0;
  double droppedSimulationTime = 0;
};

// Owns step scheduling, NOT the physics world or render scene. Do not step the
// same world elsewhere while this scheduler owns its clock. All mutation and
// hooks run on one owner thread, never from a Jolt callback. No per-frame heap.
class WaterSimulation final {
public:
  bool configure(const WaterSimulationSettings &settings) noexcept;
  bool setWaterSettings(const WaterRuntimeSettings &settings) noexcept;
  bool bind(const WaterBodyBinding &binding) noexcept { return runtime_.bind(binding); }
  bool unbind(AetherBodyHandle body) noexcept { return runtime_.unbind(body); }
  // Clears registrations and clock. Call before destroying/changing worlds.
  void reset() noexcept;
  // `ripples` é repassado ao runtime a cada passo. Ele fica aqui, e não em
  // WaterSimulationSettings, porque é um recurso emprestado: as configurações
  // são copiadas e um ponteiro dentro delas sobreviveria ao que aponta.
  WaterSimulationFrame advance(AetherPhysicsWorld *physics, const renderer::WaterWorld &water,
                                double frameSeconds, double timeScale = 1, bool paused = false,
                                WaterSimulationHooks hooks = {},
                                renderer::WaterRippleField *ripples = nullptr) noexcept;
  WaterRuntimeStats stats() const noexcept { return runtime_.stats(); }
private:
  WaterSimulationSettings settings_{};
  WaterRuntime runtime_{};
  AetherPhysicsWorld *world_ = nullptr;
  double time_ = 0, accumulator_ = 0;
  bool faulted_ = false;
};

} // namespace ae::physics
