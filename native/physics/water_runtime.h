#pragma once

#include "physics/water_field_adapter.h"
#include "renderer/water_ripples.h"
#include "renderer/water_world.h"

namespace ae::physics {

struct WaterBodyBinding final {
  AetherBodyHandle body = AetherBodyHandle_Invalid;
  BuoyantShape shape{};
  float referenceArea = 1.0f;
};

enum class WaterContactPhase : u32 { Enter = 0, Exit = 1 };
struct WaterContactEvent final {
  AetherBodyHandle body = AetherBodyHandle_Invalid;
  renderer::WaterVolumeId volume = renderer::InvalidWaterVolume;
  WaterContactPhase phase = WaterContactPhase::Enter;
  float submergedFraction = 0;
  double simulationTime = 0;
};

struct WaterRuntimeSettings final {
  BuoyancySettings forces{};
  u32 layers = ~0u;
  float enterFraction = .02f;
  float exitFraction = .005f;
  // Quanto do movimento vertical do corpo vira ondulação. Zero desliga o laço
  // de volta e a água deixa de saber que o corpo existe — que é o estado
  // anterior a este campo.
  float rippleGain = 1.0f;
};
bool validateWaterRuntimeSettings(const WaterRuntimeSettings &settings) noexcept;

struct WaterRuntimeStats final {
  u32 queried = 0;
  u32 unavailableBodies = 0;
  AetherWaterForceStats forces{};
};

// Fixed-step integration edge. Apply once immediately BEFORE Physics_Step,
// with the same monotonically increasing simulation clock; it does not own or
// step the physics world. Registration and destruction belong to the caller's
// scene lifecycle. No allocations after construction, no renderer/GPU access.
// A single owner thread mutates the registry and applies forces.
class WaterRuntime final {
public:
  static constexpr u32 Capacity = 128;
  bool bind(const WaterBodyBinding &binding) noexcept;
  bool unbind(AetherBodyHandle body) noexcept;
  void clear() noexcept;
  // `ripples` fecha o laço: a água empurra o corpo pelas forças, e o corpo
  // empurra a água de volta injetando o próprio deslocamento. Nulo mantém o
  // comportamento anterior, em que a superfície ignora quem flutua nela.
  bool apply(AetherPhysicsWorld *physics, const renderer::WaterWorld &water,
             double simulationTime, const WaterRuntimeSettings &settings = {},
             renderer::WaterRippleField *ripples = nullptr) noexcept;
  std::span<const WaterContactEvent> events() const noexcept { return {events_.data(), eventCount_}; }
  WaterRuntimeStats stats() const noexcept { return stats_; }
  float submergedFraction(AetherBodyHandle body) const noexcept;

private:
  struct Slot final {
    WaterBodyBinding binding{};
    renderer::WaterVolumeId wetVolume = renderer::InvalidWaterVolume;
    float submergedFraction = 0.0f;
  };
  std::array<Slot, Capacity> slots_{};
  // Switching volumes can generate an exit and an enter for each body.
  std::array<WaterContactEvent, Capacity * 2> events_{};
  // Cinco sondagens por corpo: o centro e as quatro pontas da sua pegada. O
  // centro sozinho não determina inclinação alguma, e é ele que existia antes.
  // Quatro pontas bastam para o ajuste por mínimos quadrados e mantêm a consulta
  // ao campo num lote só — a alternativa, uma consulta por ponta, gastaria o
  // orçamento inteiro de travessia de fronteira em flutuação.
  static constexpr u32 ProbesPerBody = 5;
  std::array<renderer::WaterVec2, Capacity * ProbesPerBody> positions_{};
  std::array<renderer::WaterVolumeQuery, Capacity * ProbesPerBody> queries_{};
  std::array<AetherVec3, Capacity> bodyPositions_{};
  std::array<AetherQuat, Capacity> rotations_{};
  std::array<WaterPlaneSample, ProbesPerBody> planeSamples_{};
  std::array<u32, Capacity> indices_{};
  std::array<AetherWaterBodySample, Capacity> forces_{};
  u32 eventCount_ = 0;
  double lastTime_ = 0;
  bool applied_ = false;
  WaterRuntimeStats stats_{};
};

} // namespace ae::physics
