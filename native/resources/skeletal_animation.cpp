#include "resources/skeletal_animation.h"

#include <algorithm>
#include <cmath>

namespace ae::resources {
namespace {

bool invert(const float m[16], float out[16]) {
  float inv[16];
  inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
  inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
  inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
  inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
  inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
  inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
  inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
  inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
  inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
  inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
  inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
  inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
  inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
  inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
  inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
  inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
  const float determinant = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
  if (!std::isfinite(determinant) || std::fabs(determinant) < 1e-20f) return false;
  for (u32 i = 0; i < 16; ++i) out[i] = inv[i] / determinant;
  return true;
}

void multiply(const float a[16], const float b[16], float out[16]) {
  float result[16];
  for (u32 column = 0; column < 4; ++column)
    for (u32 row = 0; row < 4; ++row) {
      float sum = 0;
      for (u32 k = 0; k < 4; ++k) sum += a[k * 4 + row] * b[column * 4 + k];
      result[column * 4 + row] = sum;
    }
  std::copy(result, result + 16, out);
}

void normalizeQuaternion(float q[4]) {
  const float length = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
  if (!(length > 1e-12f) || !std::isfinite(length)) {
    q[0] = q[1] = q[2] = 0;
    q[3] = 1;
    return;
  }
  for (u32 i = 0; i < 4; ++i) q[i] /= length;
}

// Slerp pelo menor arco, como a especificação pede para LINEAR em rotação.
void slerp(const float a[4], const float b[4], float t, float out[4]) {
  float target[4]{b[0], b[1], b[2], b[3]};
  float cosine = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
  if (cosine < 0) {
    cosine = -cosine;
    for (auto &value : target) value = -value;
  }
  float wa = 1 - t, wb = t;
  if (cosine < 0.9995f) {
    const float angle = std::acos(std::min(cosine, 1.0f));
    const float sine = std::sin(angle);
    wa = std::sin((1 - t) * angle) / sine;
    wb = std::sin(t * angle) / sine;
  }
  for (u32 i = 0; i < 4; ++i) out[i] = wa * a[i] + wb * target[i];
  normalizeQuaternion(out);
}

} // namespace

float wrapAnimationTime(float time, float duration, AnimationWrapMode mode, bool &finished) {
  finished = false;
  if (!std::isfinite(time)) time = 0;
  if (!(duration > 0)) {
    finished = mode == AnimationWrapMode::Once;
    return 0;
  }
  switch (mode) {
    case AnimationWrapMode::Loop: {
      float local = std::fmod(time, duration);
      return local < 0 ? local + duration : local;
    }
    case AnimationWrapMode::PingPong: {
      float local = std::fmod(std::fabs(time), 2 * duration);
      return local > duration ? 2 * duration - local : local;
    }
    case AnimationWrapMode::ClampForever: return std::clamp(time, 0.0f, duration);
    case AnimationWrapMode::Once:
    default:
      if (time >= duration) {
        finished = true;
        return duration;
      }
      return std::max(time, 0.0f);
  }
}

bool validAnimationChannel(const AnimationChannel &channel) {
  const usize keys = channel.times.size();
  if (!keys) return false;
  for (usize i = 0; i < keys; ++i) {
    if (!std::isfinite(channel.times[i]) || channel.times[i] < 0) return false;
    if (i && channel.times[i] < channel.times[i - 1]) return false;
  }
  const usize perKey = channel.components() * (channel.interpolation == AnimationInterpolation::CubicSpline ? 3u : 1u);
  if (channel.values.size() != keys * perKey) return false;
  for (const float value : channel.values)
    if (!std::isfinite(value)) return false;
  return true;
}

bool sampleAnimationChannel(const AnimationChannel &channel, float time, float out[4]) {
  if (!validAnimationChannel(channel)) return false;
  const u32 components = channel.components();
  const bool cubic = channel.interpolation == AnimationInterpolation::CubicSpline;
  const usize keys = channel.times.size();
  const auto value = [&](usize key, u32 part) -> const float * {
    // CUBICSPLINE: part 0 tangente de entrada, 1 valor, 2 tangente de saída.
    return channel.values.data() + (cubic ? (key * 3 + part) : key) * components;
  };
  const auto emit = [&](const float *source) {
    for (u32 i = 0; i < components; ++i) out[i] = source[i];
    if (channel.path == AnimationPath::Rotation) normalizeQuaternion(out);
    return true;
  };
  if (keys == 1 || time <= channel.times.front()) return emit(value(0, 1));
  if (time >= channel.times.back()) return emit(value(keys - 1, 1));
  const auto upper = std::upper_bound(channel.times.begin(), channel.times.end(), time);
  const usize next = static_cast<usize>(upper - channel.times.begin());
  const usize previous = next - 1;
  const float t0 = channel.times[previous], t1 = channel.times[next];
  const float span = t1 - t0;
  if (channel.interpolation == AnimationInterpolation::Step || !(span > 0)) return emit(value(previous, 1));
  const float s = (time - t0) / span;
  if (channel.interpolation == AnimationInterpolation::Linear) {
    const float *a = value(previous, 1), *b = value(next, 1);
    if (channel.path == AnimationPath::Rotation) {
      slerp(a, b, s, out);
      return true;
    }
    for (u32 i = 0; i < components; ++i) out[i] = a[i] + (b[i] - a[i]) * s;
    return true;
  }
  // Hermite cúbico da especificação: v(s) = (2s³-3s²+1)·v0 + (s³-2s²+s)·Δt·b0 +
  // (-2s³+3s²)·v1 + (s³-s²)·Δt·a1, com b0 a tangente de saída de v0 e a1 a de
  // entrada de v1.
  const float s2 = s * s, s3 = s2 * s;
  const float h00 = 2 * s3 - 3 * s2 + 1, h10 = s3 - 2 * s2 + s, h01 = -2 * s3 + 3 * s2, h11 = s3 - s2;
  const float *v0 = value(previous, 1), *b0 = value(previous, 2), *v1 = value(next, 1), *a1 = value(next, 0);
  for (u32 i = 0; i < components; ++i)
    out[i] = h00 * v0[i] + h10 * span * b0[i] + h01 * v1[i] + h11 * span * a1[i];
  if (channel.path == AnimationPath::Rotation) normalizeQuaternion(out);
  return true;
}

void composeTransform(const float t[3], const float q[4], const float s[3], float out[16]) {
  const float x = q[0], y = q[1], z = q[2], w = q[3];
  const float xx = x * x, yy = y * y, zz = z * z, xy = x * y, xz = x * z, yz = y * z, wx = w * x, wy = w * y, wz = w * z;
  out[0] = (1 - 2 * (yy + zz)) * s[0];
  out[1] = 2 * (xy + wz) * s[0];
  out[2] = 2 * (xz - wy) * s[0];
  out[3] = 0;
  out[4] = 2 * (xy - wz) * s[1];
  out[5] = (1 - 2 * (xx + zz)) * s[1];
  out[6] = 2 * (yz + wx) * s[1];
  out[7] = 0;
  out[8] = 2 * (xz + wy) * s[2];
  out[9] = 2 * (yz - wx) * s[2];
  out[10] = (1 - 2 * (xx + yy)) * s[2];
  out[11] = 0;
  out[12] = t[0];
  out[13] = t[1];
  out[14] = t[2];
  out[15] = 1;
}

bool computeSkinPalette(const SkinDefinition &skin, const float drawModel[16],
                        const std::vector<float> &jointWorlds, std::vector<float> &palette) {
  const usize joints = skin.joints.size();
  if (!joints || joints > MaximumSkinJoints || jointWorlds.size() != joints * 16 ||
      skin.inverseBind.size() != joints * 16)
    return false;
  float inverseModel[16];
  if (!invert(drawModel, inverseModel)) return false;
  std::vector<float> result(joints * 16);
  for (usize j = 0; j < joints; ++j) {
    float relative[16];
    multiply(inverseModel, jointWorlds.data() + j * 16, relative);
    multiply(relative, skin.inverseBind.data() + j * 16, result.data() + j * 16);
    for (u32 i = 0; i < 16; ++i)
      if (!std::isfinite(result[j * 16 + i])) return false;
  }
  palette = std::move(result);
  return true;
}

bool skinnedLocalBounds(const SkinDefinition &skin, const std::vector<float> &palette,
                        float center[3], float &radius) {
  const usize joints = skin.joints.size();
  if (palette.size() != joints * 16 || skin.jointSpheres.size() != joints * 4) return false;
  float minimum[3]{0, 0, 0}, maximum[3]{0, 0, 0};
  bool any = false;
  for (usize j = 0; j < joints; ++j) {
    const float *sphere = skin.jointSpheres.data() + j * 4;
    if (sphere[3] < 0) continue;
    const float *m = palette.data() + j * 16;
    float c[3];
    for (u32 axis = 0; axis < 3; ++axis)
      c[axis] = m[axis] * sphere[0] + m[4 + axis] * sphere[1] + m[8 + axis] * sphere[2] + m[12 + axis];
    // Escala máxima da paleta: a esfera continua contendo os vértices mesmo
    // com junta escalada.
    float scale = 0;
    for (u32 column = 0; column < 3; ++column)
      scale = std::max(scale, std::sqrt(m[column * 4] * m[column * 4] + m[column * 4 + 1] * m[column * 4 + 1] +
                                        m[column * 4 + 2] * m[column * 4 + 2]));
    const float r = sphere[3] * scale;
    for (u32 axis = 0; axis < 3; ++axis) {
      if (!any || c[axis] - r < minimum[axis]) minimum[axis] = c[axis] - r;
      if (!any || c[axis] + r > maximum[axis]) maximum[axis] = c[axis] + r;
    }
    any = true;
  }
  if (!any) return false;
  float squared = 0;
  for (u32 axis = 0; axis < 3; ++axis) {
    center[axis] = (minimum[axis] + maximum[axis]) * .5f;
    const float half = (maximum[axis] - minimum[axis]) * .5f;
    squared += half * half;
  }
  radius = std::sqrt(squared);
  return std::isfinite(radius);
}

} // namespace ae::resources
