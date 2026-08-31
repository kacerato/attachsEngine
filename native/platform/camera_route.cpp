#include "platform/camera_route.h"

#include "platform/atomic_asset_file.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace ae::platform {
namespace {

void writeWord(std::span<u8> buffer, usize offset, u32 value) {
  buffer[offset] = static_cast<u8>(value);
  buffer[offset + 1] = static_cast<u8>(value >> 8);
  buffer[offset + 2] = static_cast<u8>(value >> 16);
  buffer[offset + 3] = static_cast<u8>(value >> 24);
}

void writeWide(std::span<u8> buffer, usize offset, u64 value) {
  writeWord(buffer, offset, static_cast<u32>(value));
  writeWord(buffer, offset + 4, static_cast<u32>(value >> 32));
}

u32 readWord(std::span<const u8> buffer, usize offset) {
  return static_cast<u32>(buffer[offset]) | static_cast<u32>(buffer[offset + 1]) << 8 |
         static_cast<u32>(buffer[offset + 2]) << 16 | static_cast<u32>(buffer[offset + 3]) << 24;
}

u64 readWide(std::span<const u8> buffer, usize offset) {
  return static_cast<u64>(readWord(buffer, offset)) | static_cast<u64>(readWord(buffer, offset + 4)) << 32;
}

} // namespace

usize encodedCameraRouteSize(u32 tickCount) {
  return static_cast<usize>(CameraRouteHeaderSize) +
         static_cast<usize>(tickCount) * sizeof(CameraRouteSample);
}

bool encodeCameraRoute(const CameraRouteSample *samples, u32 count, u64 sceneFingerprint,
                       u32 tickRateHz, std::span<u8> outBuffer) {
  if (samples == nullptr || count == 0 || count > CameraRouteMaximumTicks) return false;
  if (outBuffer.size() != encodedCameraRouteSize(count)) return false;
  for (u32 index = 0; index < count; ++index) {
    const CameraRouteSample &sample = samples[index];
    if (!std::isfinite(sample.position[0]) || !std::isfinite(sample.position[1]) ||
        !std::isfinite(sample.position[2]) || !std::isfinite(sample.yaw) ||
        !std::isfinite(sample.pitch)) return false;
  }
  writeWord(outBuffer, 0, CameraRouteMagic);
  writeWord(outBuffer, 4, CameraRouteVersion);
  writeWord(outBuffer, 8, CameraRouteHeaderSize);
  writeWord(outBuffer, 12, 0); // Reserved, kept zero for future header growth.
  writeWide(outBuffer, 16, sceneFingerprint);
  writeWord(outBuffer, 24, tickRateHz);
  writeWord(outBuffer, 28, count);
  std::memcpy(outBuffer.data() + CameraRouteHeaderSize, samples, count * sizeof(CameraRouteSample));
  return true;
}

bool decodeCameraRoute(std::span<const u8> bytes, CameraRouteData &out) {
  if (bytes.size() < CameraRouteHeaderSize) return false;
  if (readWord(bytes, 0) != CameraRouteMagic || readWord(bytes, 4) != CameraRouteVersion ||
      readWord(bytes, 8) != CameraRouteHeaderSize) return false;
  const u32 tickCount = readWord(bytes, 28);
  if (tickCount == 0 || tickCount > CameraRouteMaximumTicks) return false;
  if (bytes.size() != encodedCameraRouteSize(tickCount)) return false;

  CameraRouteData decoded;
  decoded.sceneFingerprint = readWide(bytes, 16);
  decoded.tickRateHz = readWord(bytes, 24);
  decoded.samples.resize(tickCount);
  std::memcpy(decoded.samples.data(), bytes.data() + CameraRouteHeaderSize,
              static_cast<usize>(tickCount) * sizeof(CameraRouteSample));
  for (const CameraRouteSample &sample : decoded.samples) {
    if (!std::isfinite(sample.position[0]) || !std::isfinite(sample.position[1]) ||
        !std::isfinite(sample.position[2]) || !std::isfinite(sample.yaw) ||
        !std::isfinite(sample.pitch)) return false;
  }

  out = std::move(decoded);
  return true;
}

void CameraRouteRecorder::reserve(u32 expectedTicks) { samples_.reserve(expectedTicks); }

void CameraRouteRecorder::reset() { samples_.clear(); }

void CameraRouteRecorder::pushSample(const FreeCameraState &state) {
  if (samples_.size() >= CameraRouteMaximumTicks) return;
  CameraRouteSample sample{};
  sample.position[0] = state.position[0];
  sample.position[1] = state.position[1];
  sample.position[2] = state.position[2];
  sample.yaw = state.yaw;
  sample.pitch = state.pitch;
  samples_.push_back(sample);
}

bool CameraRouteRecorder::writeToFile(const char *path, u64 sceneFingerprint, u32 tickRateHz) const {
  if (samples_.empty()) return false;
  const u32 count = static_cast<u32>(samples_.size());
  std::vector<u8> buffer(encodedCameraRouteSize(count));
  if (!encodeCameraRoute(samples_.data(), count, sceneFingerprint, tickRateHz, buffer)) return false;

  struct Cursor final {
    const u8 *data;
    usize size;
    usize offset;
  };
  Cursor cursor{buffer.data(), buffer.size(), 0};
  const AssetRead reader = [](void *context, void *destination, usize capacity) -> int {
    auto *state = static_cast<Cursor *>(context);
    const usize remaining = state->size - state->offset;
    const usize chunk = remaining < capacity ? remaining : capacity;
    if (chunk == 0) return 0;
    std::memcpy(destination, state->data + state->offset, chunk);
    state->offset += chunk;
    return static_cast<int>(chunk);
  };
  return replaceAssetFile(path, buffer.size(), reader, &cursor);
}

bool CameraRoutePlayer::loadFromFile(const char *path) {
  data_ = CameraRouteData{};
  if (path == nullptr || *path == '\0') return false;
  std::FILE *input = std::fopen(path, "rb");
  if (input == nullptr) return false;
  bool ok = std::fseek(input, 0, SEEK_END) == 0;
  const long sizeSigned = ok ? std::ftell(input) : -1;
  ok = ok && sizeSigned >= 0 && std::fseek(input, 0, SEEK_SET) == 0;
  if (!ok) {
    std::fclose(input);
    return false;
  }
  std::vector<u8> bytes(static_cast<usize>(sizeSigned));
  const usize read = bytes.empty() ? 0 : std::fread(bytes.data(), 1, bytes.size(), input);
  std::fclose(input);
  if (read != bytes.size()) return false;
  return decodeCameraRoute(bytes, data_);
}

FreeCameraState CameraRoutePlayer::sample(u64 frameOrdinal) const {
  if (data_.samples.empty()) return FreeCameraState{};
  const CameraRouteSample &recorded = data_.samples[frameOrdinal % data_.samples.size()];
  FreeCameraState state{};
  state.position[0] = recorded.position[0];
  state.position[1] = recorded.position[1];
  state.position[2] = recorded.position[2];
  state.yaw = recorded.yaw;
  state.pitch = recorded.pitch;
  return state;
}

} // namespace ae::platform
