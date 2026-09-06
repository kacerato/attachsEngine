#pragma once

#include "renderer/water_field.h"

namespace ae::renderer {

using WaterVolumeId = u64;
inline constexpr WaterVolumeId InvalidWaterVolume = 0;

struct WaterVolumeQuery final {
  WaterVolumeId volume = InvalidWaterVolume;
  WaterFieldSample surface{};
};

// Value-owned registry. IDs come from scene resources, never from slot indices.
// Configure/remove between jobs; queries are reentrant while the registry and
// any borrowed spectral mirrors are unchanged. Empty registries allocate no GPU
// resources. A dry region in one volume does not erase a different volume.
class WaterWorld final {
public:
  static constexpr u32 Capacity = 16;
  bool setVolume(WaterVolumeId id, const WaterFieldSetup &setup, i32 priority = 0,
                 u32 layers = ~0u) noexcept;
  bool removeVolume(WaterVolumeId id) noexcept;
  void clear() noexcept;
  u32 volumeCount() const noexcept;
  bool sample(std::span<const WaterVec2> positions, double time, u32 layers,
              std::span<WaterVolumeQuery> output) const noexcept;

private:
  struct Volume final {
    WaterVolumeId id = InvalidWaterVolume;
    i32 priority = 0;
    u32 layers = ~0u;
    WaterField field{};
  };
  std::array<Volume, Capacity> volumes_{};
};

} // namespace ae::renderer
