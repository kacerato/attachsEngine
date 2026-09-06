#include "renderer/water_world.h"

#include <cmath>
#include <limits>

namespace ae::renderer {

bool WaterWorld::setVolume(WaterVolumeId id, const WaterFieldSetup &setup,
                           i32 priority, u32 layers) noexcept {
  if (id == InvalidWaterVolume || layers == 0) return false;
  Volume *target = nullptr;
  for (auto &volume : volumes_) {
    if (volume.id == id) { target = &volume; break; }
    if (target == nullptr && volume.id == InvalidWaterVolume) target = &volume;
  }
  if (target == nullptr) return false;
  WaterField candidate;
  if (!candidate.configure(setup)) return false;
  target->field = candidate;
  target->priority = priority;
  target->layers = layers;
  target->id = id;
  return true;
}

bool WaterWorld::removeVolume(WaterVolumeId id) noexcept {
  if (id == InvalidWaterVolume) return false;
  for (auto &volume : volumes_) if (volume.id == id) {
    volume = {};
    return true;
  }
  return false;
}

void WaterWorld::clear() noexcept { for (auto &volume : volumes_) volume = {}; }
u32 WaterWorld::volumeCount() const noexcept {
  u32 count = 0;
  for (const auto &volume : volumes_) if (volume.id != InvalidWaterVolume) ++count;
  return count;
}

bool WaterWorld::sample(std::span<const WaterVec2> positions, double time, u32 layers,
                        std::span<WaterVolumeQuery> output) const noexcept {
  if (output.size() < positions.size() || !std::isfinite(time) ||
      std::abs(time) > std::numeric_limits<float>::max()) return false;
  for (const auto &position : positions)
    if (!std::isfinite(position.x) || !std::isfinite(position.y)) return false;
  for (usize i = 0; i < positions.size(); ++i) {
    WaterVolumeQuery best{};
    i32 priority = std::numeric_limits<i32>::min();
    for (const auto &volume : volumes_) {
      if (volume.id == InvalidWaterVolume || (volume.layers & layers) == 0) continue;
      WaterFieldSample sample{};
      if (!volume.field.sample(positions.subspan(i, 1), time, {&sample, 1}) ||
          !hasWaterFieldFlag(sample.flags, WaterFieldFlag::Valid) ||
          hasWaterFieldFlag(sample.flags, WaterFieldFlag::Excluded) || sample.coverage <= 0) continue;
      // Stable total order: priority, then highest surface, then smallest ID.
      if (best.volume == InvalidWaterVolume || volume.priority > priority ||
          (volume.priority == priority && (sample.height > best.surface.height ||
           (sample.height == best.surface.height && volume.id < best.volume)))) {
        best = {volume.id, sample};
        priority = volume.priority;
      }
    }
    output[i] = best;
  }
  return true;
}

} // namespace ae::renderer
