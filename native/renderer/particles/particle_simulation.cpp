#include "renderer/particles/particle_simulation.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>

namespace ae::renderer::particles {
namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegrees = kPi / 180.0f;
constexpr float kGravity = 9.81f;
// Canais de sorteio por partícula. Fixos: mudar um canal muda o resultado de
// toda cena salva com semente manual.
enum Channel : u32 {
  ChannelLifetime = 1, ChannelSpeed, ChannelSize, ChannelRotation, ChannelColor,
  ChannelSizeOverLifetime, ChannelRotationOverLifetime, ChannelColorOverLifetime, ChannelGravity,
  ChannelVelocityX, ChannelVelocityY, ChannelVelocityZ, ChannelSpeedModifier, ChannelLimit,
  ChannelForceX, ChannelForceY, ChannelForceZ, ChannelNoise,
  ChannelSizeBySpeed, ChannelColorBySpeed, ChannelRotationBySpeed,
  ChannelSheetFrame, ChannelSheetStart, ChannelSheetRow, ChannelTrail
};
constexpr u32 kTrailStride = scene::ParticleSystemSettings::kTrailPoints * 4;
// Velocidade normalizada na faixa autoral (Size/Color/Rotation by Speed).
float speedFraction(float speed, const float (&range)[2]) {
  const float span = range[1] - range[0];
  if (!(std::abs(span) > 1e-6f)) return speed >= range[1] ? 1.0f : 0.0f;
  return std::clamp((speed - range[0]) / span, 0.0f, 1.0f);
}

// Ruído de valor 3D suave (trilinear com smoothstep). Barato e contínuo:
// basta para turbulência de partícula.
float latticeValue(i32 x, i32 y, i32 z, u32 seed) {
  u32 h = static_cast<u32>(x) * 0x8da6b343U ^ static_cast<u32>(y) * 0xd8163841U ^ static_cast<u32>(z) * 0xcb1ab31fU ^ seed;
  h ^= h >> 15; h *= 0x2c1b3c6dU; h ^= h >> 12; h *= 0x297a2d39U; h ^= h >> 15;
  return static_cast<float>(h >> 8) * (2.0f / 16777216.0f) - 1.0f;
}
float valueNoise(float x, float y, float z, u32 seed) {
  const float fx = std::floor(x), fy = std::floor(y), fz = std::floor(z);
  const auto ix = static_cast<i32>(fx), iy = static_cast<i32>(fy), iz = static_cast<i32>(fz);
  const auto smooth = [](float t) { return t * t * (3 - 2 * t); };
  const float tx = smooth(x - fx), ty = smooth(y - fy), tz = smooth(z - fz);
  float plane[2];
  for (i32 k = 0; k < 2; ++k) {
    const float a = latticeValue(ix, iy, iz + k, seed) + (latticeValue(ix + 1, iy, iz + k, seed) - latticeValue(ix, iy, iz + k, seed)) * tx;
    const float b = latticeValue(ix, iy + 1, iz + k, seed) + (latticeValue(ix + 1, iy + 1, iz + k, seed) - latticeValue(ix, iy + 1, iz + k, seed)) * tx;
    plane[k] = a + (b - a) * ty;
  }
  return plane[0] + (plane[1] - plane[0]) * tz;
}

void transformPoint(const float (&m)[16], const float (&p)[3], float (&out)[3]) {
  for (u32 k = 0; k < 3; ++k) out[k] = m[12 + k] + m[k] * p[0] + m[4 + k] * p[1] + m[8 + k] * p[2];
}
void transformVector(const float (&m)[16], const float (&v)[3], float (&out)[3]) {
  for (u32 k = 0; k < 3; ++k) out[k] = m[k] * v[0] + m[4 + k] * v[1] + m[8 + k] * v[2];
}
float length3(const float (&v)[3]) { return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }
void normalize3(float (&v)[3], const float (&fallback)[3]) {
  const float l = length3(v);
  if (!(l > 1e-8f)) { std::copy(fallback, fallback + 3, v); return; }
  for (auto &c : v) c /= l;
}
// Inversa da parte linear 3x3 (colunas), para levar a gravidade de mundo ao
// espaço local de um emissor rotacionado/escalado.
bool inverseLinear(const float (&m)[16], float (&out)[9]) {
  const float a = m[0], b = m[4], c = m[8], d = m[1], e = m[5], f = m[9], g = m[2], h = m[6], i = m[10];
  const float det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
  if (!(std::abs(det) > 1e-12f) || !std::isfinite(det)) return false;
  const float s = 1.0f / det;
  // Linhas da inversa na disposição de colunas (out[col*3+row]).
  const float r[9]{(e * i - f * h) * s, (c * h - b * i) * s, (b * f - c * e) * s,
                   (f * g - d * i) * s, (a * i - c * g) * s, (c * d - a * f) * s,
                   (d * h - e * g) * s, (b * g - a * h) * s, (a * e - b * d) * s};
  for (u32 row = 0; row < 3; ++row)
    for (u32 col = 0; col < 3; ++col) out[col * 3 + row] = r[row * 3 + col];
  return true;
}
// Rotação euler em graus na convenção da cena Astra (runtime/transform_math.h:
// Rz * Ry * Rx), em colunas, para o deslocamento da forma ler como o Transform.
void eulerMatrix(const float (&degrees)[3], float (&m)[9]) {
  const float x = degrees[0] * kDegrees, y = degrees[1] * kDegrees, z = degrees[2] * kDegrees;
  const float cx = std::cos(x), sx = std::sin(x), cy = std::cos(y), sy = std::sin(y), cz = std::cos(z), sz = std::sin(z);
  const float rz[9]{cz, sz, 0, -sz, cz, 0, 0, 0, 1};
  const float rx[9]{1, 0, 0, 0, cx, sx, 0, -sx, cx};
  const float ry[9]{cy, 0, -sy, 0, 1, 0, sy, 0, cy};
  float t[9]{};
  const auto mul = [](const float *a, const float *b, float *out) {
    for (u32 col = 0; col < 3; ++col)
      for (u32 row = 0; row < 3; ++row)
        out[col * 3 + row] = a[0 * 3 + row] * b[col * 3 + 0] + a[1 * 3 + row] * b[col * 3 + 1] + a[2 * 3 + row] * b[col * 3 + 2];
  };
  mul(ry, rx, t);
  mul(rz, t, m);
}
u32 hash32(u32 x) {
  x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
  return x;
}
float lerp(float a, float b, float t) { return a + (b - a) * t; }
} // namespace

u16 floatToHalf(float value) {
  u32 bits;
  std::memcpy(&bits, &value, 4);
  const u32 sign = (bits >> 16) & 0x8000u;
  if (!std::isfinite(value)) return static_cast<u16>(sign | (std::isnan(value) ? 0x7e00u : 0x7bffu));
  float magnitude = std::min(std::abs(value), 65504.0f);
  std::memcpy(&bits, &magnitude, 4);
  const i32 exponent = static_cast<i32>((bits >> 23) & 0xff) - 127 + 15;
  u32 mantissa = bits & 0x7fffffu;
  if (exponent <= 0) {
    if (exponent < -10) return static_cast<u16>(sign);
    mantissa |= 0x800000u;
    const u32 shift = static_cast<u32>(14 - exponent);
    u32 half = mantissa >> shift;
    if ((mantissa >> (shift - 1)) & 1u) ++half;  // arredonda
    return static_cast<u16>(sign | half);
  }
  u32 half = (static_cast<u32>(exponent) << 10) | (mantissa >> 13);
  if (mantissa & 0x1000u) ++half;
  if (half >= 0x7c00u) half = 0x7bffu;
  return static_cast<u16>(sign | half);
}
float halfToFloat(u16 value) {
  const u32 sign = (value & 0x8000u) << 16, exponent = (value >> 10) & 0x1fu, mantissa = value & 0x3ffu;
  u32 bits;
  if (exponent == 0) {
    if (mantissa == 0) bits = sign;
    else {
      float f = static_cast<float>(mantissa) / 1024.0f / 16384.0f;
      return (value & 0x8000u) ? -f : f;
    }
  } else if (exponent == 31) bits = sign | 0x7f800000u | (mantissa << 13);
  else bits = sign | ((exponent - 15 + 127) << 23) | (mantissa << 13);
  float result;
  std::memcpy(&result, &bits, 4);
  return result;
}

void ParticleCurveLut::bake(const scene::ScriptCurve &curve) {
  for (u32 i = 0; i < kSamples; ++i) {
    float v = 0;
    if (!scene::tryEvaluateScriptCurve(curve, static_cast<float>(i) / (kSamples - 1), v) || !std::isfinite(v)) v = 0;
    values[i] = v;
  }
}
float ParticleCurveLut::sample(float t) const {
  const float x = std::clamp(t, 0.0f, 1.0f) * (kSamples - 1);
  const u32 i = std::min(static_cast<u32>(x), kSamples - 2);
  return lerp(values[i], values[i + 1], x - static_cast<float>(i));
}
void ParticleGradientLut::bake(const scene::ScriptGradient &gradient) {
  for (u32 i = 0; i < kSamples; ++i) {
    float c[4]{1, 1, 1, 1};
    if (!scene::tryEvaluateScriptGradient(gradient, static_cast<float>(i) / (kSamples - 1), c)) c[0] = c[1] = c[2] = c[3] = 1;
    for (u32 k = 0; k < 4; ++k) values[i][k] = c[k];
  }
}
void ParticleGradientLut::sample(float t, float (&out)[4]) const {
  const float x = std::clamp(t, 0.0f, 1.0f) * (kSamples - 1);
  const u32 i = std::min(static_cast<u32>(x), kSamples - 2);
  const float f = x - static_cast<float>(i);
  for (u32 k = 0; k < 4; ++k) out[k] = lerp(values[i][k], values[i + 1][k], f);
}

void BakedMinMaxCurve::bake(const scene::ParticleMinMaxCurve &source) {
  mode = source.mode;
  constantMin = source.constantMin;
  constantMax = source.constantMax;
  multiplier = source.multiplier;
  if (source.usesCurves()) {
    curveMax.bake(source.curveMax);
    if (mode == scene::ParticleCurveMode::TwoCurves) curveMin.bake(source.curveMin);
  }
}
float BakedMinMaxCurve::evaluate(float t, float random) const {
  switch (mode) {
  case scene::ParticleCurveMode::Constant: return constantMax;
  case scene::ParticleCurveMode::Curve: return curveMax.sample(t) * multiplier;
  case scene::ParticleCurveMode::TwoConstants: return lerp(constantMin, constantMax, random);
  case scene::ParticleCurveMode::TwoCurves: return lerp(curveMin.sample(t), curveMax.sample(t), random) * multiplier;
  }
  return constantMax;
}
void BakedMinMaxGradient::bake(const scene::ParticleMinMaxGradient &source) {
  mode = source.mode;
  std::copy(source.colorMin, source.colorMin + 4, colorMin);
  std::copy(source.colorMax, source.colorMax + 4, colorMax);
  if (mode == scene::ParticleGradientMode::Gradient || mode == scene::ParticleGradientMode::TwoGradients ||
      mode == scene::ParticleGradientMode::RandomColor)
    gradientMax.bake(source.gradientMax);
  if (mode == scene::ParticleGradientMode::TwoGradients) gradientMin.bake(source.gradientMin);
}
void BakedMinMaxGradient::evaluate(float t, float random, float (&out)[4]) const {
  switch (mode) {
  case scene::ParticleGradientMode::Color: std::copy(colorMax, colorMax + 4, out); return;
  case scene::ParticleGradientMode::Gradient: gradientMax.sample(t, out); return;
  case scene::ParticleGradientMode::TwoColors:
    for (u32 k = 0; k < 4; ++k) out[k] = lerp(colorMin[k], colorMax[k], random);
    return;
  case scene::ParticleGradientMode::TwoGradients: {
    float a[4], b[4];
    gradientMin.sample(t, a);
    gradientMax.sample(t, b);
    for (u32 k = 0; k < 4; ++k) out[k] = lerp(a[k], b[k], random);
    return;
  }
  case scene::ParticleGradientMode::RandomColor: gradientMax.sample(random, out); return;
  }
  std::copy(colorMax, colorMax + 4, out);
}

void Pcg32::seed(u64 value, u64 sequence) {
  state = 0;
  increment = (sequence << 1u) | 1u;
  next();
  state += value;
  next();
}
u32 Pcg32::next() {
  const u64 old = state;
  state = old * 6364136223846793005ULL + increment;
  const u32 shifted = static_cast<u32>(((old >> 18u) ^ old) >> 27u);
  const u32 rotation = static_cast<u32>(old >> 59u);
  return (shifted >> rotation) | (shifted << ((~rotation + 1u) & 31u));
}
float particleRandom(u32 seed, u32 channel) {
  return static_cast<float>(hash32(seed ^ (channel * 0x9e3779b9u)) >> 8) * (1.0f / 16777216.0f);
}

void ParticleEmitter::configure(const scene::ParticleSystemSettings &settings, u32 seed) {
  settings_ = settings;
  startDelay_.bake(settings.startDelay);
  startLifetime_.bake(settings.startLifetime);
  startSpeed_.bake(settings.startSpeed);
  startSize_.bake(settings.startSize);
  startRotation_.bake(settings.startRotation);
  gravity_.bake(settings.gravityModifier);
  rate_.bake(settings.emissionRate);
  sizeOverLifetime_.bake(settings.sizeOverLifetime);
  velocity_[0].bake(settings.velocityX); velocity_[1].bake(settings.velocityY); velocity_[2].bake(settings.velocityZ);
  speedModifier_.bake(settings.speedModifier);
  limitSpeed_.bake(settings.limitSpeed);
  force_[0].bake(settings.forceX); force_[1].bake(settings.forceY); force_[2].bake(settings.forceZ);
  noiseStrength_.bake(settings.noiseStrength);
  sizeBySpeed_.bake(settings.sizeBySpeed);
  rotationBySpeed_.bake(settings.rotationBySpeed);
  colorBySpeed_.bake(settings.colorBySpeed);
  sheetFrame_.bake(settings.sheetFrameOverTime);
  sheetStart_.bake(settings.sheetStartFrame);
  rotationOverLifetime_.bake(settings.rotationOverLifetime);
  startColor_.bake(settings.startColor);
  colorOverLifetime_.bake(settings.colorOverLifetime);
  float rotation[9];
  eulerMatrix(settings.shapeRotation, rotation);
  std::copy(rotation, rotation + 9, shapeMatrix_);
  std::copy(settings.shapePosition, settings.shapePosition + 3, shapeMatrix_ + 9);
  if (!configured_ || seed != baseSeed_) {
    baseSeed_ = seed;
    rng_.seed(seed);
  }
  configured_ = true;
  // Trilhas desligadas no inspetor: os pontos somem já, não no próximo passo.
  if (!settings.trailsEnabled) { trail_.clear(); trailCount_.clear(); }
  // Teto autoral reduzido: as excedentes saem pelas mais velhas do fim da lista.
  while (alive() > capacity()) kill(alive() - 1);
}

u32 ParticleEmitter::capacity() const noexcept {
  const auto authored = static_cast<u32>(std::clamp(settings_.maxParticles, 1.0f, scene::ParticleSystemSettings::kMaximumParticles));
  return std::min(authored, capacityLimit_);
}

void ParticleEmitter::play() {
  if (state_ == State::Paused) { state_ = State::Playing; return; }
  if (state_ == State::Playing && emitting_) return;
  state_ = State::Playing;
  emitting_ = true;
  finished_ = false;
  systemTime_ = 0;
  loop_ = 0;
  emitAccumulator_ = 0;
  burstFired_.fill(0);
  delayRemaining_ = std::max(0.0f, startDelay_.evaluate(0, rng_.unit()));
  prewarmPending_ = settings_.looping && settings_.prewarm;
}
void ParticleEmitter::stop(bool clearParticles) {
  emitting_ = false;
  if (clearParticles) clear();
  if (state_ == State::Paused) state_ = State::Playing;
  if (alive() == 0 && state_ != State::Stopped) { state_ = State::Stopped; finished_ = true; }
}
void ParticleEmitter::pause() {
  if (state_ == State::Playing) state_ = State::Paused;
}
void ParticleEmitter::clear() {
  for (auto *v : {&px_, &py_, &pz_, &vx_, &vy_, &vz_, &ax_, &ay_, &az_, &age_, &lifetime_, &baseSize_, &rotation_, &colorR_, &colorG_, &colorB_, &colorA_})
    v->clear();
  seed_.clear();
  trail_.clear();
  trailCount_.clear();
}
void ParticleEmitter::restart() {
  clear();
  hasLastOrigin_ = false;
  distanceAccumulator_ = 0;
  rng_.seed(baseSeed_);
  state_ = State::Stopped;
  emitting_ = false;
  play();
}

void ParticleEmitter::emit(u32 count, const float (&world)[16]) {
  const float norm = settings_.duration > 0 ? std::clamp(systemTime_ / settings_.duration, 0.0f, 1.0f) : 0;
  spawn(count, world, norm, 0);
  if (state_ == State::Stopped && alive()) state_ = State::Playing;
}

void ParticleEmitter::setCollisionPlanes(const ParticleCollisionPlane *planes, u32 count) {
  planeCount_ = std::min<u32>(count, 2);
  for (u32 i = 0; i < planeCount_; ++i) planes_[i] = planes[i];
}

void ParticleEmitter::record(std::vector<ParticleEvent> &events, u32 i, const float (&world)[16]) {
  if (events.size() >= kMaximumEvents) return;
  ParticleEvent event;
  const float p[3]{px_[i], py_[i], pz_[i]}, v[3]{vx_[i], vy_[i], vz_[i]};
  if (settings_.simulationSpace == scene::ParticleSimulationSpace::Local) {
    transformPoint(world, p, event.position);
    transformVector(world, v, event.velocity);
  } else {
    std::copy(p, p + 3, event.position);
    std::copy(v, v + 3, event.velocity);
  }
  events.push_back(event);
}

// Collision (Planes): a partícula que atravessa o plano volta à superfície
// (mais o raio), devolve a velocidade normal pelo Quique e perde a tangencial
// pelo Amortecimento. Os planos chegam em mundo e vão ao espaço da simulação.
void ParticleEmitter::collide(float dt, const float (&world)[16]) {
  (void)dt;
  if (!settings_.collisionEnabled || !planeCount_) return;
  const bool local = settings_.simulationSpace == scene::ParticleSimulationSpace::Local;
  float inverse[9]{1, 0, 0, 0, 1, 0, 0, 0, 1};
  if (local && !inverseLinear(world, inverse)) return;
  ParticleCollisionPlane planes[2];
  for (u32 k = 0; k < planeCount_; ++k) {
    planes[k] = planes_[k];
    if (!local) continue;
    // Ponto: inversa afim. Normal: transposta da linear, ou seja (M^-1)^T aplicada à normal.
    const float d[3]{planes_[k].point[0] - world[12], planes_[k].point[1] - world[13], planes_[k].point[2] - world[14]};
    for (u32 r = 0; r < 3; ++r) {
      planes[k].point[r] = inverse[r] * d[0] + inverse[3 + r] * d[1] + inverse[6 + r] * d[2];
      planes[k].normal[r] = world[r * 4] * planes_[k].normal[0] + world[r * 4 + 1] * planes_[k].normal[1] + world[r * 4 + 2] * planes_[k].normal[2];
    }
    const float up[3]{0, 1, 0};
    normalize3(planes[k].normal, up);
  }
  const bool wantsCollisions = settings_.subEmittersEnabled && settings_.subEmitterCollision != 0;
  const float bounce = std::max(0.0f, settings_.collisionBounce), keepTangent = 1.0f - std::clamp(settings_.collisionDampen, 0.0f, 1.0f);
  for (u32 i = alive(); i-- > 0;) {
    bool hit = false;
    for (u32 k = 0; k < planeCount_; ++k) {
      const auto &plane = planes[k];
      const float radius = baseSize_[i] * std::max(0.0f, settings_.collisionRadiusScale) * 0.5f;
      const float distance = (px_[i] - plane.point[0]) * plane.normal[0] + (py_[i] - plane.point[1]) * plane.normal[1] +
                             (pz_[i] - plane.point[2]) * plane.normal[2] - radius;
      const float approach = vx_[i] * plane.normal[0] + vy_[i] * plane.normal[1] + vz_[i] * plane.normal[2];
      if (distance >= 0 || approach >= 0) continue;
      // De volta à superfície; normal refletida, tangencial amortecida.
      px_[i] -= plane.normal[0] * distance; py_[i] -= plane.normal[1] * distance; pz_[i] -= plane.normal[2] * distance;
      const float nx = plane.normal[0] * approach, ny = plane.normal[1] * approach, nz = plane.normal[2] * approach;
      vx_[i] = (vx_[i] - nx) * keepTangent - nx * bounce;
      vy_[i] = (vy_[i] - ny) * keepTangent - ny * bounce;
      vz_[i] = (vz_[i] - nz) * keepTangent - nz * bounce;
      hit = true;
    }
    if (!hit) continue;
    if (wantsCollisions) record(collisions_, i, world);
    age_[i] += lifetime_[i] * std::clamp(settings_.collisionLifetimeLoss, 0.0f, 1.0f);
    const float speed = std::sqrt(vx_[i] * vx_[i] + vy_[i] * vy_[i] + vz_[i] * vz_[i]);
    if (age_[i] >= lifetime_[i] || speed < settings_.collisionMinKillSpeed) {
      if (settings_.subEmittersEnabled && settings_.subEmitterDeath) record(deaths_, i, world);
      kill(i);
    }
  }
}

void ParticleEmitter::kill(u32 index) {
  const u32 last = alive() - 1;
  if (index != last) {
    px_[index] = px_[last]; py_[index] = py_[last]; pz_[index] = pz_[last];
    vx_[index] = vx_[last]; vy_[index] = vy_[last]; vz_[index] = vz_[last];
    ax_[index] = ax_[last]; ay_[index] = ay_[last]; az_[index] = az_[last];
    age_[index] = age_[last]; lifetime_[index] = lifetime_[last]; baseSize_[index] = baseSize_[last];
    rotation_[index] = rotation_[last];
    colorR_[index] = colorR_[last]; colorG_[index] = colorG_[last]; colorB_[index] = colorB_[last]; colorA_[index] = colorA_[last];
    seed_[index] = seed_[last];
    if (!trailCount_.empty()) {
      std::copy_n(trail_.begin() + static_cast<std::ptrdiff_t>(last) * kTrailStride, kTrailStride,
                  trail_.begin() + static_cast<std::ptrdiff_t>(index) * kTrailStride);
      trailCount_[index] = trailCount_[last];
    }
  }
  for (auto *v : {&px_, &py_, &pz_, &vx_, &vy_, &vz_, &ax_, &ay_, &az_, &age_, &lifetime_, &baseSize_, &rotation_, &colorR_, &colorG_, &colorB_, &colorA_})
    v->pop_back();
  seed_.pop_back();
  if (!trailCount_.empty()) {
    trail_.resize(trail_.size() - kTrailStride);
    trailCount_.pop_back();
  }
}

// Formas na convenção da Unity: cone, caixa e "forma desligada" emitem para +Z;
// aresta emite para +Y; círculo e esfera para fora do centro.
void ParticleEmitter::sampleShape(Pcg32 &rng, float (&position)[3], float (&direction)[3]) const {
  const auto &s = settings_;
  position[0] = position[1] = position[2] = 0;
  direction[0] = direction[1] = 0;
  direction[2] = 1;
  if (!s.shapeEnabled) return;
  const float thickness = std::clamp(s.shapeRadiusThickness, 0.0f, 1.0f);
  const float arc = std::clamp(s.shapeArc, 0.0f, 360.0f) * kDegrees;
  const auto unitSphere = [&](float (&v)[3]) {
    const float z = rng.unit() * 2 - 1, a = rng.unit() * 2 * kPi, r = std::sqrt(std::max(0.0f, 1 - z * z));
    v[0] = r * std::cos(a); v[1] = r * std::sin(a); v[2] = z;
  };
  switch (s.shapeType) {
  case scene::ParticleShapeType::Sphere:
  case scene::ParticleShapeType::Hemisphere: {
    float n[3];
    unitSphere(n);
    if (s.shapeType == scene::ParticleShapeType::Hemisphere) n[2] = std::abs(n[2]);
    const float inner = 1 - thickness;
    const float r = s.shapeRadius * std::cbrt(lerp(inner * inner * inner, 1.0f, rng.unit()));
    for (u32 k = 0; k < 3; ++k) { position[k] = n[k] * r; direction[k] = n[k]; }
    break;
  }
  case scene::ParticleShapeType::Cone:
  case scene::ParticleShapeType::Circle: {
    const float theta = rng.unit() * arc;
    const float inner = 1 - thickness;
    const float ratio = std::sqrt(lerp(inner * inner, 1.0f, rng.unit()));
    const float r = s.shapeRadius * ratio;
    const float c = std::cos(theta), sn = std::sin(theta);
    position[0] = r * c; position[1] = r * sn;
    if (s.shapeType == scene::ParticleShapeType::Circle) { direction[0] = c; direction[1] = sn; direction[2] = 0; break; }
    // Inclinação proporcional à distância do eixo (as linhas saem do ápice
    // virtual do cone); cone de raio zero sorteia dentro do ângulo.
    const float tilt = std::clamp(s.shapeAngle, 0.0f, 90.0f) * kDegrees * (s.shapeRadius > 0 ? ratio : std::sqrt(rng.unit()));
    direction[0] = std::sin(tilt) * c; direction[1] = std::sin(tilt) * sn; direction[2] = std::cos(tilt);
    if (s.shapeConeEmitFrom == scene::ParticleConeEmitFrom::Volume) {
      const float along = rng.unit() * s.shapeLength / std::max(direction[2], 1e-3f);
      for (u32 k = 0; k < 3; ++k) position[k] += direction[k] * along;
    }
    break;
  }
  case scene::ParticleShapeType::Box: {
    const float half[3]{s.shapeBox[0] * .5f, s.shapeBox[1] * .5f, s.shapeBox[2] * .5f};
    if (s.shapeBoxEmitFrom == scene::ParticleBoxEmitFrom::Volume) {
      for (u32 k = 0; k < 3; ++k) position[k] = (rng.unit() * 2 - 1) * half[k];
    } else if (s.shapeBoxEmitFrom == scene::ParticleBoxEmitFrom::Shell) {
      // Face sorteada pela área.
      const float areas[3]{half[1] * half[2], half[0] * half[2], half[0] * half[1]};
      const float total = areas[0] + areas[1] + areas[2];
      float pick = rng.unit() * (total > 0 ? total : 1);
      u32 axis = 0;
      while (axis < 2 && pick > areas[axis]) { pick -= areas[axis]; ++axis; }
      for (u32 k = 0; k < 3; ++k) position[k] = (rng.unit() * 2 - 1) * half[k];
      position[axis] = (rng.unit() < .5f ? -1.0f : 1.0f) * half[axis];
    } else {
      // Aresta sorteada pelo comprimento: 4 arestas paralelas a cada eixo.
      const float lengths[3]{half[0], half[1], half[2]};
      const float total = lengths[0] + lengths[1] + lengths[2];
      float pick = rng.unit() * (total > 0 ? total : 1);
      u32 axis = 0;
      while (axis < 2 && pick > lengths[axis]) { pick -= lengths[axis]; ++axis; }
      for (u32 k = 0; k < 3; ++k) position[k] = (rng.unit() < .5f ? -1.0f : 1.0f) * half[k];
      position[axis] = (rng.unit() * 2 - 1) * half[axis];
    }
    break;
  }
  case scene::ParticleShapeType::Donut: {
    // Ponto no anel (plano XY, como o Círculo) e, em volta dele, no tubo; a
    // direção sai do centro do tubo (Unity Donut).
    const float theta = rng.unit() * arc, phi = rng.unit() * 2 * kPi;
    const float inner = 1 - thickness;
    const float tube = std::max(0.0f, s.shapeDonutRadius) * std::sqrt(lerp(inner * inner, 1.0f, rng.unit()));
    const float radial[3]{std::cos(theta), std::sin(theta), 0};
    const float out[3]{radial[0] * std::cos(phi), radial[1] * std::cos(phi), std::sin(phi)};
    for (u32 k = 0; k < 3; ++k) { position[k] = radial[k] * s.shapeRadius + out[k] * tube; direction[k] = out[k]; }
    break;
  }
  case scene::ParticleShapeType::Rectangle:
    // Retângulo no plano XY, emitindo para +Z (Unity Rectangle).
    position[0] = (rng.unit() * 2 - 1) * s.shapeBox[0] * .5f;
    position[1] = (rng.unit() * 2 - 1) * s.shapeBox[1] * .5f;
    break;
  case scene::ParticleShapeType::Edge:
    position[0] = (rng.unit() * 2 - 1) * s.shapeRadius;
    direction[0] = 0; direction[1] = 1; direction[2] = 0;
    break;
  }
  if (s.shapeSpherizeDirection > 0) {
    float out[3]{position[0], position[1], position[2]};
    normalize3(out, direction);
    for (u32 k = 0; k < 3; ++k) direction[k] = lerp(direction[k], out[k], s.shapeSpherizeDirection);
  }
  if (s.shapeRandomDirection > 0) {
    float random[3];
    unitSphere(random);
    for (u32 k = 0; k < 3; ++k) direction[k] = lerp(direction[k], random[k], s.shapeRandomDirection);
  }
  const float up[3]{0, 0, 1};
  normalize3(direction, up);
  // Deslocamento da forma (posição + rotação do módulo Shape).
  float p[3], d[3];
  for (u32 k = 0; k < 3; ++k) {
    p[k] = shapeMatrix_[9 + k] + shapeMatrix_[k] * position[0] + shapeMatrix_[3 + k] * position[1] + shapeMatrix_[6 + k] * position[2];
    d[k] = shapeMatrix_[k] * direction[0] + shapeMatrix_[3 + k] * direction[1] + shapeMatrix_[6 + k] * direction[2];
  }
  std::copy(p, p + 3, position);
  std::copy(d, d + 3, direction);
}

void ParticleEmitter::spawn(u32 count, const float (&world)[16], float systemNormalized, float spreadSeconds) {
  const u32 room = capacity() > alive() ? capacity() - alive() : 0;
  count = std::min(count, room);
  if (!count) return;
  const bool worldSpace = settings_.simulationSpace == scene::ParticleSimulationSpace::World;
  for (u32 i = 0; i < count; ++i) {
    const u32 seed = rng_.next();
    float position[3], direction[3];
    sampleShape(rng_, position, direction);
    if (worldSpace) {
      float p[3], d[3];
      transformPoint(world, position, p);
      transformVector(world, direction, d);
      const float fallback[3]{0, 0, 1};
      normalize3(d, fallback);
      std::copy(p, p + 3, position);
      std::copy(d, d + 3, direction);
    }
    const float lifetime = std::max(0.01f, startLifetime_.evaluate(systemNormalized, particleRandom(seed, ChannelLifetime)));
    const float speed = startSpeed_.evaluate(systemNormalized, particleRandom(seed, ChannelSpeed));
    // Partículas da mesma rajada de taxa nascem espalhadas no quadro, não juntas.
    const float offset = spreadSeconds > 0 ? spreadSeconds * (static_cast<float>(count - i) - .5f) / static_cast<float>(count) : 0;
    float color[4];
    startColor_.evaluate(systemNormalized, particleRandom(seed, ChannelColor), color);
    px_.push_back(position[0] + direction[0] * speed * offset);
    py_.push_back(position[1] + direction[1] * speed * offset);
    pz_.push_back(position[2] + direction[2] * speed * offset);
    float inherited[3]{0, 0, 0};
    if (settings_.inheritVelocityEnabled) {
      for (u32 k = 0; k < 3; ++k) inherited[k] = emitterVelocity_[k] * settings_.inheritVelocityMultiplier;
      if (!worldSpace) {
        float inverse[9];
        if (inverseLinear(world, inverse)) {
          const float w[3]{inherited[0], inherited[1], inherited[2]};
          for (u32 k = 0; k < 3; ++k) inherited[k] = inverse[k] * w[0] + inverse[3 + k] * w[1] + inverse[6 + k] * w[2];
        }
      }
    }
    vx_.push_back(direction[0] * speed + inherited[0]);
    vy_.push_back(direction[1] * speed + inherited[1]);
    vz_.push_back(direction[2] * speed + inherited[2]);
    ax_.push_back(0); ay_.push_back(0); az_.push_back(0);
    age_.push_back(std::min(offset, lifetime * .999f));
    lifetime_.push_back(lifetime);
    baseSize_.push_back(std::max(0.0f, startSize_.evaluate(systemNormalized, particleRandom(seed, ChannelSize))));
    rotation_.push_back(startRotation_.evaluate(systemNormalized, particleRandom(seed, ChannelRotation)) * kDegrees);
    colorR_.push_back(color[0]); colorG_.push_back(color[1]); colorB_.push_back(color[2]); colorA_.push_back(color[3]);
    seed_.push_back(seed);
    if (settings_.trailsEnabled) {
      trail_.resize(trail_.size() + kTrailStride, 0.0f);
      trailCount_.push_back(0);
    }
    if (settings_.subEmittersEnabled && settings_.subEmitterBirth) record(births_, alive() - 1, world);
  }
}

void ParticleEmitter::advanceParticles(float dt, const float (&gravity)[3], const float (&world)[16]) {
  // Morte primeiro: a lista fica compacta antes de integrar.
  const bool wantsDeaths = settings_.subEmittersEnabled && settings_.subEmitterDeath != 0;
  for (u32 i = alive(); i-- > 0;) {
    age_[i] += dt;
    if (age_[i] >= lifetime_[i]) {
      if (wantsDeaths) record(deaths_, i, world);
      kill(i);
    }
  }
  const u32 n = alive();
  const float norm = settings_.duration > 0 ? std::clamp(systemTime_ / settings_.duration, 0.0f, 1.0f) : 0;
  const float gMin = gravity_.evaluate(norm, 0), gMax = gravity_.evaluate(norm, 1);
  const bool gravityRandom = gMin != gMax;
  for (u32 i = 0; i < n; ++i) {
    const float g = gravityRandom ? lerp(gMin, gMax, particleRandom(seed_[i], ChannelGravity)) : gMin;
    vx_[i] += gravity[0] * g * dt;
    vy_[i] += gravity[1] * g * dt;
    vz_[i] += gravity[2] * g * dt;
  }
  // Vetores dos módulos levados ao espaço da simulação: o vento de mundo não
  // gira com um emissor Local, e a velocidade Local gira com o objeto no Mundo.
  const bool simLocal = settings_.simulationSpace == scene::ParticleSimulationSpace::Local;
  float inverse[9]{1, 0, 0, 0, 1, 0, 0, 0, 1};
  const bool invertible = inverseLinear(world, inverse);
  const auto toSimulation = [&](scene::ParticleSimulationSpace space, float (&v)[3]) {
    float out[3];
    if (simLocal && space == scene::ParticleSimulationSpace::World) {
      if (!invertible) return;
      for (u32 k = 0; k < 3; ++k) out[k] = inverse[k] * v[0] + inverse[3 + k] * v[1] + inverse[6 + k] * v[2];
    } else if (!simLocal && space == scene::ParticleSimulationSpace::Local) {
      transformVector(world, v, out);
    } else {
      return;
    }
    std::copy(out, out + 3, v);
  };
  if (settings_.forceOverLifetimeEnabled)
    for (u32 i = 0; i < n; ++i) {
      const float t = age_[i] / lifetime_[i];
      float f[3]{force_[0].evaluate(t, particleRandom(seed_[i], ChannelForceX)),
                 force_[1].evaluate(t, particleRandom(seed_[i], ChannelForceY)),
                 force_[2].evaluate(t, particleRandom(seed_[i], ChannelForceZ))};
      toSimulation(settings_.forceSpace, f);
      vx_[i] += f[0] * dt; vy_[i] += f[1] * dt; vz_[i] += f[2] * dt;
    }
  if (settings_.limitVelocityEnabled) {
    // Dampen calibrado em quadros de 30 Hz, como a Unity; independente do passo.
    const float keep = std::pow(1.0f - std::clamp(settings_.limitDampen, 0.0f, 1.0f), dt * 30.0f);
    const float drag = std::max(0.0f, 1.0f - settings_.limitDrag * dt);
    for (u32 i = 0; i < n; ++i) {
      const float limit = std::max(0.0f, limitSpeed_.evaluate(age_[i] / lifetime_[i], particleRandom(seed_[i], ChannelLimit)));
      const float speed = std::sqrt(vx_[i] * vx_[i] + vy_[i] * vy_[i] + vz_[i] * vz_[i]);
      float scale = drag;
      if (speed > limit && speed > 0) scale *= (limit + (speed - limit) * keep) / speed;
      vx_[i] *= scale; vy_[i] *= scale; vz_[i] *= scale;
    }
  }
  const bool animated = settings_.velocityOverLifetimeEnabled || settings_.noiseEnabled;
  for (u32 i = 0; i < n; ++i) {
    float a[3]{0, 0, 0};
    float speedScale = 1;
    const float t = age_[i] / lifetime_[i];
    if (settings_.velocityOverLifetimeEnabled) {
      a[0] = velocity_[0].evaluate(t, particleRandom(seed_[i], ChannelVelocityX));
      a[1] = velocity_[1].evaluate(t, particleRandom(seed_[i], ChannelVelocityY));
      a[2] = velocity_[2].evaluate(t, particleRandom(seed_[i], ChannelVelocityZ));
      toSimulation(settings_.velocitySpace, a);
      speedScale = speedModifier_.evaluate(t, particleRandom(seed_[i], ChannelSpeedModifier));
    }
    if (settings_.noiseEnabled) {
      const float strength = noiseStrength_.evaluate(t, particleRandom(seed_[i], ChannelNoise));
      const float scroll = settings_.noiseScrollSpeed * systemTime_;
      float frequency = std::max(0.01f, settings_.noiseFrequency), amplitude = 1, total = 0;
      float sum[3]{0, 0, 0};
      const auto octaves = static_cast<u32>(std::clamp(settings_.noiseOctaves, 1.0f, 4.0f));
      for (u32 o = 0; o < octaves; ++o) {
        const float x = px_[i] * frequency + scroll, y = py_[i] * frequency, z = pz_[i] * frequency - scroll;
        for (u32 k = 0; k < 3; ++k) sum[k] += amplitude * valueNoise(x, y, z, baseSeed_ + k * 0x9e3779b9U + o * 0x632be5abU);
        total += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
      }
      for (u32 k = 0; k < 3; ++k) a[k] += strength * sum[k] / total;
    }
    if (animated) { ax_[i] = a[0]; ay_[i] = a[1]; az_[i] = a[2]; }
    px_[i] += (vx_[i] + a[0]) * speedScale * dt;
    py_[i] += (vy_[i] + a[1]) * speedScale * dt;
    pz_[i] += (vz_[i] + a[2]) * speedScale * dt;
  }
  if (settings_.rotationBySpeedEnabled)
    for (u32 i = 0; i < alive(); ++i) {
      const float vx = vx_[i] + ax_[i], vy = vy_[i] + ay_[i], vz = vz_[i] + az_[i];
      const float t = speedFraction(std::sqrt(vx * vx + vy * vy + vz * vz), settings_.rotationBySpeedRange);
      rotation_[i] += rotationBySpeed_.evaluate(t, particleRandom(seed_[i], ChannelRotationBySpeed)) * kDegrees * dt;
    }
  if (settings_.rotationOverLifetimeEnabled)
    for (u32 i = 0; i < alive(); ++i)
      rotation_[i] += rotationOverLifetime_.evaluate(age_[i] / lifetime_[i], particleRandom(seed_[i], ChannelRotationOverLifetime)) * kDegrees * dt;
  // Por último: a colisão pode matar partículas e encurtar a lista.
  collide(dt, world);
}

void ParticleEmitter::simulate(float deltaSeconds, const float (&world)[16]) {
  if (!configured_ || state_ != State::Playing || !std::isfinite(deltaSeconds) || deltaSeconds <= 0) return;
  if (prewarmPending_) {
    // Pré-aquecer: um ciclo inteiro em passos de 1/30 s antes do primeiro quadro.
    prewarmPending_ = false;
    const u32 steps = std::min(600u, static_cast<u32>(std::ceil(settings_.duration * 30.0f)));
    for (u32 i = 0; i < steps; ++i) simulate(1.0f / 30.0f, world);
  }
  const float dt = std::min(deltaSeconds, 0.25f) * std::max(0.0f, settings_.simulationSpeed);
  if (dt <= 0) return;
  // Gravidade é de mundo (-Y). No espaço local, levada pela inversa do objeto.
  float gravity[3]{0, -kGravity, 0};
  if (settings_.simulationSpace == scene::ParticleSimulationSpace::Local) {
    float inverse[9];
    if (inverseLinear(world, inverse))
      for (u32 k = 0; k < 3; ++k) gravity[k] = inverse[3 + k] * -kGravity;
  }
  // Velocidade do emissor pelo deslocamento do objeto (primeiro quadro: zero).
  if (hasLastOrigin_) {
    float travelled = 0;
    for (u32 k = 0; k < 3; ++k) {
      const float delta = world[12 + k] - lastOrigin_[k];
      emitterVelocity_[k] = delta / dt;
      travelled += delta * delta;
    }
    distanceAccumulator_ += std::sqrt(travelled) * std::max(0.0f, settings_.emissionRateOverDistance);
  } else {
    emitterVelocity_[0] = emitterVelocity_[1] = emitterVelocity_[2] = 0;
  }
  std::copy(world + 12, world + 15, lastOrigin_);
  hasLastOrigin_ = true;
  advanceParticles(dt, gravity, world);
  updateTrails();
  if (emitting_ && settings_.emissionEnabled && distanceAccumulator_ >= 1) {
    const auto count = static_cast<u32>(std::min(distanceAccumulator_, 1.0e6f));
    distanceAccumulator_ -= static_cast<float>(count);
    const float norm = settings_.duration > 0 ? std::clamp(systemTime_ / settings_.duration, 0.0f, 1.0f) : 0;
    spawn(count, world, norm, 0);
  }

  float remaining = dt;
  if (emitting_ && delayRemaining_ > 0) {
    const float wait = std::min(delayRemaining_, remaining);
    delayRemaining_ -= wait;
    remaining -= wait;
  }
  const float duration = std::max(settings_.duration, 0.05f);
  while (emitting_ && remaining > 0) {
    const float step = std::min(remaining, duration - systemTime_);
    const float t0 = systemTime_, t1 = systemTime_ + step;
    const float norm = t0 / duration;
    remaining -= step;
    if (settings_.emissionEnabled) {
      emitAccumulator_ += std::max(0.0f, rate_.evaluate(norm, rng_.unit())) * step;
      if (emitAccumulator_ >= 1) {
        const auto count = static_cast<u32>(std::min(emitAccumulator_, 1.0e6f));
        emitAccumulator_ -= static_cast<float>(count);
        spawn(count, world, norm, step);
        // As que nasceram antes do fim deste trecho já envelheceram o resto do quadro.
        if (remaining > 0) for (u32 i = alive() - std::min(count, alive()); i < alive(); ++i) age_[i] += remaining;
      }
      for (usize b = 0; b < settings_.activeBursts(); ++b) {
        const auto &burst = settings_.bursts[b];
        const auto cycles = static_cast<u32>(burst.cycles);
        while (cycles == 0 || burstFired_[b] < cycles) {
          const float at = burst.time + static_cast<float>(burstFired_[b]) * burst.interval;
          // Em ordem: uma rajada que caiu antes deste trecho (atraso inicial,
          // passo longo) sai agora; nenhuma passa do fim do ciclo.
          if (at >= duration || at > t1) break;
          ++burstFired_[b];
          if (rng_.unit() <= burst.probability) {
            const float pick = rng_.unit();
            const float low = std::min(burst.countMin, burst.countMax), high = std::max(burst.countMin, burst.countMax);
            const auto count = static_cast<u32>(std::lround(lerp(low, high, pick)));
            spawn(count, world, at / duration, 0);
          }
        }
      }
    }
    systemTime_ = t1;
    if (systemTime_ >= duration - 1e-6f) {
      if (settings_.looping) {
        systemTime_ = 0;
        ++loop_;
        burstFired_.fill(0);
      } else {
        systemTime_ = duration;
        emitting_ = false;
      }
    }
  }
  if (!emitting_ && alive() == 0) {
    state_ = State::Stopped;
    finished_ = true;
  }
}

void ParticleEmitter::build(const float (&world)[16], const float (&camera)[3], std::vector<ParticleInstance> &out) const {
  const u32 n = alive();
  if (!n) return;
  const bool local = settings_.simulationSpace == scene::ParticleSimulationSpace::Local;
  std::vector<u32> order(n);
  std::iota(order.begin(), order.end(), 0u);
  const usize first = out.size();
  out.resize(first + n);
  // Posições de mundo primeiro: a ordenação por distância precisa delas.
  std::vector<float> key;
  const auto worldPosition = [&](u32 i, float (&p)[3]) {
    const float lp[3]{px_[i], py_[i], pz_[i]};
    if (local) transformPoint(world, lp, p);
    else std::copy(lp, lp + 3, p);
  };
  switch (settings_.sortMode) {
  case scene::ParticleSortMode::ByDistance: {
    key.resize(n);
    for (u32 i = 0; i < n; ++i) {
      float p[3];
      worldPosition(i, p);
      const float d[3]{p[0] - camera[0], p[1] - camera[1], p[2] - camera[2]};
      key[i] = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
    }
    std::sort(order.begin(), order.end(), [&](u32 a, u32 b) { return key[a] > key[b] || (key[a] == key[b] && a < b); });
    break;
  }
  case scene::ParticleSortMode::OldestInFront:
    std::sort(order.begin(), order.end(), [&](u32 a, u32 b) { return age_[a] < age_[b] || (age_[a] == age_[b] && a < b); });
    break;
  case scene::ParticleSortMode::YoungestInFront:
    std::sort(order.begin(), order.end(), [&](u32 a, u32 b) { return age_[a] > age_[b] || (age_[a] == age_[b] && a < b); });
    break;
  case scene::ParticleSortMode::None: break;
  }
  for (u32 slot = 0; slot < n; ++slot) {
    const u32 i = order[slot];
    auto &instance = out[first + slot];
    float p[3];
    worldPosition(i, p);
    std::copy(p, p + 3, instance.position);
    const float lv[3]{vx_[i] + ax_[i], vy_[i] + ay_[i], vz_[i] + az_[i]};
    float v[3];
    if (local) transformVector(world, lv, v);
    else std::copy(lv, lv + 3, v);
    std::copy(v, v + 3, instance.velocity);
    const float t = age_[i] / lifetime_[i];
    float size = baseSize_[i];
    if (settings_.sizeOverLifetimeEnabled) size *= std::max(0.0f, sizeOverLifetime_.evaluate(t, particleRandom(seed_[i], ChannelSizeOverLifetime)));
    const float speed = std::sqrt(lv[0] * lv[0] + lv[1] * lv[1] + lv[2] * lv[2]);
    if (settings_.sizeBySpeedEnabled)
      size *= std::max(0.0f, sizeBySpeed_.evaluate(speedFraction(speed, settings_.sizeBySpeedRange), particleRandom(seed_[i], ChannelSizeBySpeed)));
    instance.size = size;
    instance.rotation = rotation_[i];
    float color[4]{colorR_[i], colorG_[i], colorB_[i], colorA_[i]};
    if (settings_.colorOverLifetimeEnabled) {
      float over[4];
      colorOverLifetime_.evaluate(t, particleRandom(seed_[i], ChannelColorOverLifetime), over);
      for (u32 k = 0; k < 4; ++k) color[k] *= over[k];
    }
    if (settings_.colorBySpeedEnabled) {
      float bySpeed[4];
      colorBySpeed_.evaluate(speedFraction(speed, settings_.colorBySpeedRange), particleRandom(seed_[i], ChannelColorBySpeed), bySpeed);
      for (u32 k = 0; k < 4; ++k) color[k] *= bySpeed[k];
    }
    for (u32 k = 0; k < 4; ++k) instance.color[k] = floatToHalf(std::max(0.0f, color[k]));
    instance.random = seed_[i];
    instance.frame = sheetFrame(t, seed_[i]);
  }
}

// Texture Sheet Animation, modo Grid da Unity: a curva Frame over Time (em
// fração da sequência) é lida em fract(t × Ciclos), soma o Quadro inicial e dá
// a volta na sequência. Em Uma linha, a sequência é a linha escolhida ou
// sorteada por partícula.
float ParticleEmitter::sheetFrame(float t, u32 seed) const {
  const auto &s = settings_;
  if (!s.textureSheetEnabled) return 0;
  const u32 columns = static_cast<u32>(std::max(1.0f, s.sheetTiles[0])), rows = static_cast<u32>(std::max(1.0f, s.sheetTiles[1]));
  const u32 frames = s.sheetFrameCount();
  if (frames <= 1 && rows <= 1) return 0;
  const float cycle = t * std::max(s.sheetCycles, 0.0f);
  const float phase = cycle - std::floor(cycle);
  const float over = std::clamp(sheetFrame_.evaluate(phase, particleRandom(seed, ChannelSheetFrame)), 0.0f, 1.0f);
  const float start = std::floor(std::max(0.0f, sheetStart_.evaluate(0, particleRandom(seed, ChannelSheetStart))));
  const u32 index = static_cast<u32>(std::fmod(std::floor(over * static_cast<float>(frames) + start), static_cast<float>(std::max(frames, 1u))));
  if (s.sheetAnimation == scene::ParticleSheetAnimation::WholeSheet) return static_cast<float>(std::min(index, columns * rows - 1));
  const u32 row = s.sheetRowMode == scene::ParticleSheetRowMode::Random
      ? std::min(rows - 1, static_cast<u32>(particleRandom(seed, ChannelSheetRow) * static_cast<float>(rows)))
      : std::min(rows - 1, static_cast<u32>(s.sheetRow));
  return static_cast<float>(row * columns + std::min(index, columns - 1));
}

// Trails (modo Particles): cada partícula grava a posição quando andou a
// Distância mínima e esquece pontos mais velhos que Vida da trilha × vida dela.
// Os pontos ficam no espaço da simulação, como as próprias partículas.
void ParticleEmitter::updateTrails() {
  const u32 n = alive();
  if (!settings_.trailsEnabled) {
    trail_.clear();
    trailCount_.clear();
    return;
  }
  if (trailCount_.size() != n) {
    // Módulo ligado com partículas já vivas: começam sem pontos.
    trail_.assign(static_cast<usize>(n) * kTrailStride, 0.0f);
    trailCount_.assign(n, 0);
  }
  const float minimum = std::max(settings_.trailMinVertexDistance, 0.0f);
  for (u32 i = 0; i < n; ++i) {
    float *points = trail_.data() + static_cast<usize>(i) * kTrailStride;
    u32 count = trailCount_[i];
    const float keep = std::max(settings_.trailLifetime, 0.0f) * lifetime_[i];
    u32 expired = 0;
    while (expired < count && age_[i] - points[expired * 4 + 3] > keep) ++expired;
    if (expired) {
      std::copy(points + expired * 4, points + count * 4, points);
      count -= expired;
    }
    bool record = count == 0;
    if (!record) {
      const float *last = points + (count - 1) * 4;
      const float d[3]{px_[i] - last[0], py_[i] - last[1], pz_[i] - last[2]};
      record = d[0] * d[0] + d[1] * d[1] + d[2] * d[2] >= minimum * minimum;
    }
    if (record && keep > 0) {
      if (count == scene::ParticleSystemSettings::kTrailPoints) {
        std::copy(points + 4, points + count * 4, points);
        --count;
      }
      float *slot = points + count * 4;
      slot[0] = px_[i]; slot[1] = py_[i]; slot[2] = pz_[i]; slot[3] = age_[i];
      ++count;
    }
    trailCount_[i] = static_cast<u8>(count);
  }
}

void ParticleEmitter::buildTrails(const float (&world)[16], std::vector<ParticleInstance> &out) const {
  const auto &s = settings_;
  if (!s.trailsEnabled || trailCount_.size() != alive()) return;
  const bool local = s.simulationSpace == scene::ParticleSimulationSpace::Local;
  const auto toWorld = [&](const float *p, float (&w)[3]) {
    const float point[3]{p[0], p[1], p[2]};
    if (local) transformPoint(world, point, w);
    else std::copy(point, point + 3, w);
  };
  for (u32 i = 0; i < alive(); ++i) {
    const u32 count = trailCount_[i];
    if (!count || particleRandom(seed_[i], ChannelTrail) >= s.trailRatio) continue;
    const float *points = trail_.data() + static_cast<usize>(i) * kTrailStride;
    // Do mais novo (a própria partícula) ao mais velho: o segmento k liga o
    // ponto k ao ponto k+1 da lista invertida.
    float newer[3];
    const float head[3]{px_[i], py_[i], pz_[i]};
    toWorld(head, newer);
    const float t = age_[i] / lifetime_[i];
    float size = baseSize_[i];
    if (s.sizeOverLifetimeEnabled) size *= std::max(0.0f, sizeOverLifetime_.evaluate(t, particleRandom(seed_[i], ChannelSizeOverLifetime)));
    float color[4]{1, 1, 1, 1};
    if (s.trailInheritColor) {
      color[0] = colorR_[i]; color[1] = colorG_[i]; color[2] = colorB_[i]; color[3] = colorA_[i];
      if (s.colorOverLifetimeEnabled) {
        float over[4];
        colorOverLifetime_.evaluate(t, particleRandom(seed_[i], ChannelColorOverLifetime), over);
        for (u32 k = 0; k < 4; ++k) color[k] *= over[k];
      }
    }
    for (u32 k = count; k-- > 0;) {
      float older[3];
      toWorld(points + k * 4, older);
      const float segment[3]{newer[0] - older[0], newer[1] - older[1], newer[2] - older[2]};
      if (segment[0] * segment[0] + segment[1] * segment[1] + segment[2] * segment[2] > 1e-10f) {
        // Fração ao longo da trilha no meio do segmento: 0 junto da partícula.
        const float along = (static_cast<float>(count - k) - 0.5f) / static_cast<float>(count);
        ParticleInstance instance;
        std::copy(newer, newer + 3, instance.position);
        std::copy(segment, segment + 3, instance.velocity);
        instance.size = size * (s.trailWidth + (s.trailWidthEnd - s.trailWidth) * along);
        const float fade = s.trailFade ? 1.0f - along : 1.0f;
        for (u32 c = 0; c < 3; ++c) instance.color[c] = floatToHalf(std::max(0.0f, color[c]));
        instance.color[3] = floatToHalf(std::clamp(color[3] * fade, 0.0f, 1.0f));
        instance.random = seed_[i];
        if (instance.size > 0) out.push_back(instance);
      }
      std::copy(older, older + 3, newer);
    }
  }
}

ParticleMaterialParameters ParticleEmitter::trailMaterial() const {
  auto m = material();
  m.renderMode = static_cast<u32>(scene::ParticleRenderMode::StretchedBillboard);
  m.stretchVelocityScale = 1;  // comprimento = o segmento
  m.stretchLengthScale = 0;
  m.sheetTiles[0] = m.sheetTiles[1] = 1;
  return m;
}

ParticleMaterialParameters ParticleEmitter::material() const {
  const auto &s = settings_;
  ParticleMaterialParameters m;
  m.blend = static_cast<u32>(s.materialBlend);
  m.texture = static_cast<u32>(s.materialTexture);
  m.noise = static_cast<u32>(s.materialNoise);
  m.mask = static_cast<u32>(s.materialMask);
  m.renderMode = static_cast<u32>(s.renderMode);
  for (u32 k = 0; k < 3; ++k) m.tint[k] = s.materialTint[k] * s.materialEmission;
  std::copy(s.materialScroll, s.materialScroll + 2, m.scroll);
  std::copy(s.materialNoiseScroll, s.materialNoiseScroll + 2, m.noiseScroll);
  m.noiseScale = s.materialNoiseScale;
  m.noiseStrength = s.materialNoiseStrength;
  m.noiseDistortion = s.materialNoiseDistortion;
  m.centerGlow = s.materialCenterGlow;
  m.centerGlowPower = s.materialCenterGlowPower;
  m.softDistance = s.materialSoftDistance;
  m.stretchVelocityScale = s.stretchVelocityScale;
  m.stretchLengthScale = s.stretchLengthScale;
  m.maxScreenSize = s.maxParticleSize;
  if (s.textureSheetEnabled) {
    m.sheetTiles[0] = std::max(1.0f, s.sheetTiles[0]);
    m.sheetTiles[1] = std::max(1.0f, s.sheetTiles[1]);
  }
  return m;
}

} // namespace ae::renderer::particles
