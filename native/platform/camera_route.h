#pragma once

#include "core/base.h"
#include "platform/free_camera_controller.h"

#include <span>
#include <type_traits>
#include <vector>

namespace ae::platform {

// Runtime policy for the camera this frame, mirroring the u32-backed
// enum+sanitize+Name pattern already used by renderer::GpuCostIsolation.
// Off is always the default: a route only drives the camera when a launch
// option explicitly opts in.
enum class CameraRouteMode : u32 {
  Off = 0,
  Record = 1,
  Replay = 2,
  Count = 3,
};

inline constexpr CameraRouteMode sanitizeCameraRouteMode(u32 value) noexcept {
  return value < static_cast<u32>(CameraRouteMode::Count) ? static_cast<CameraRouteMode>(value)
                                                            : CameraRouteMode::Off;
}

inline constexpr const char *cameraRouteModeName(CameraRouteMode mode) noexcept {
  switch (mode) {
    case CameraRouteMode::Off: return "off";
    case CameraRouteMode::Record: return "record";
    case CameraRouteMode::Replay: return "replay";
    case CameraRouteMode::Count: break;
  }
  return "off";
}

inline constexpr u32 CameraRouteMagic = 0x54524541; // "AERT", little-endian (Aether Route).
inline constexpr u32 CameraRouteVersion = 1;
inline constexpr u32 CameraRouteHeaderSize = 32;
// Generous sanity ceiling (36 min at 120 Hz) that rejects corrupt/garbage
// tick counts before any allocation, same spirit as MapPackage's MaximumDraws.
inline constexpr u32 CameraRouteMaximumTicks = 262144;

// One pose per rendered frame. Deliberately not "per tick of wall-clock time":
// the render loop has no fixed simulation tick today, so indexing playback by
// tickRateHz * elapsedSeconds would sample different poses at the same instant
// under different FPS/thermal conditions across two runs -- defeating the one
// purpose a route exists for, which is a byte-identical A/B comparison.
struct CameraRouteSample final {
  float position[3];
  float yaw;
  float pitch;
};
static_assert(std::is_standard_layout_v<CameraRouteSample> && sizeof(CameraRouteSample) == 20);

struct CameraRouteData final {
  // FNV-1a fingerprint of the AEMAP package the route was recorded against
  // (same algorithm as MapPackageView::contentFingerprint). A route replayed
  // over a different map/build must be rejected by the caller, not silently
  // driven through geometry that no longer matches.
  u64 sceneFingerprint = 0;
  // Descriptive only -- never used to index playback. Lets a human/QA report
  // estimate intended duration; sample() always advances one tick per frame.
  u32 tickRateHz = 0;
  std::vector<CameraRouteSample> samples;
};

usize encodedCameraRouteSize(u32 tickCount);

// Pure, testable encode. outBuffer must be exactly encodedCameraRouteSize(count)
// bytes. Fails only for invalid input (zero/oversized count, null samples).
bool encodeCameraRoute(const CameraRouteSample *samples, u32 count, u64 sceneFingerprint,
                       u32 tickRateHz, std::span<u8> outBuffer);

// Pure, testable decode. Fails closed: rejects unknown magic/version, a header
// size that does not match this reader, a tick count outside sane bounds, a
// byte length that does not exactly match the declared tick count, and any
// non-finite sample (matches the fail-closed discipline of decodeMapPackage --
// this is playback input driving physics/culling, not visibility data that
// should fail open).
bool decodeCameraRoute(std::span<const u8> bytes, CameraRouteData &out);

// Records one FreeCameraState per frame actually drawn (pushSample is meant to
// be called once per successful drawFrame(), never on a skipped/failed frame).
// Zero-alloc per push after an optional reserve().
class CameraRouteRecorder final {
public:
  void reserve(u32 expectedTicks);
  void reset();
  void pushSample(const FreeCameraState &state);
  u32 sampleCount() const { return static_cast<u32>(samples_.size()); }

  // Atomic write (temp file + rename, via platform::replaceAssetFile) so a
  // crash or kill mid-write never leaves a half-written route on disk.
  bool writeToFile(const char *path, u64 sceneFingerprint, u32 tickRateHz) const;

private:
  std::vector<CameraRouteSample> samples_;
};

// Loads a route recorded by CameraRouteRecorder and samples it by frame
// ordinal, looping by modulo so a short recorded route can drive an arbitrarily
// long soak run. The caller (android_main.cpp) is responsible for comparing
// data().sceneFingerprint against the loaded map and refusing replay on
// mismatch -- this class does not know which scene is currently loaded.
class CameraRoutePlayer final {
public:
  bool loadFromFile(const char *path);

  bool loaded() const { return !data_.samples.empty(); }
  const CameraRouteData &data() const { return data_; }
  u32 tickCount() const { return static_cast<u32>(data_.samples.size()); }

  // Undefined only in the sense of returning a default-constructed pose when
  // !loaded(); never crashes on an empty/unloaded route (fail open, matching
  // frustum_visibility's philosophy for uncertain/absent data).
  FreeCameraState sample(u64 frameOrdinal) const;

private:
  CameraRouteData data_;
};

} // namespace ae::platform
