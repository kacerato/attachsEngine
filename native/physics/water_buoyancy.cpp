#include "physics/water_buoyancy.h"

#include <algorithm>
#include <cmath>

namespace ae::physics {
namespace {
constexpr float Pi = 3.14159265358979323846f;

AetherVec3 add(AetherVec3 a, AetherVec3 b) noexcept { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
AetherVec3 subtract(AetherVec3 a, AetherVec3 b) noexcept { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
AetherVec3 scale(AetherVec3 a, float s) noexcept { return {a.x * s, a.y * s, a.z * s}; }
float dot(AetherVec3 a, AetherVec3 b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z; }
AetherVec3 cross(AetherVec3 a, AetherVec3 b) noexcept {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
float length(AetherVec3 a) noexcept { return std::sqrt(dot(a, a)); }
bool finite(AetherVec3 a) noexcept {
  return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z);
}

AetherVec3 rotate(AetherQuat q, AetherVec3 v) noexcept {
  const AetherVec3 axis{q.x, q.y, q.z};
  const AetherVec3 t = scale(cross(axis, v), 2.0f);
  return add(add(v, scale(t, q.w)), cross(axis, t));
}

float signedHeight(const WaterPlane &plane, AetherVec3 point) noexcept {
  return dot(plane.normal, point) + plane.offset;
}

// Signed volume: the caller supplies corners in any winding, so the sign is
// discarded and only the magnitude and centroid are used.
void accumulateTetrahedron(AetherVec3 a, AetherVec3 b, AetherVec3 c, AetherVec3 d,
                           float &volume, AetherVec3 &weightedCentroid) noexcept {
  const float signedVolume = dot(subtract(b, a), cross(subtract(c, a), subtract(d, a))) / 6.0f;
  const float magnitude = std::abs(signedVolume);
  if (!(magnitude > 0.0f)) return;
  const AetherVec3 centroid = scale(add(add(a, b), add(c, d)), 0.25f);
  volume += magnitude;
  weightedCentroid = add(weightedCentroid, scale(centroid, magnitude));
}

AetherVec3 interpolateToSurface(AetherVec3 below, float heightBelow, AetherVec3 above,
                                float heightAbove) noexcept {
  const float span = heightAbove - heightBelow;
  const float t = std::abs(span) > 1.0e-12f ? (-heightBelow) / span : 0.0f;
  return add(below, scale(subtract(above, below), std::clamp(t, 0.0f, 1.0f)));
}
} // namespace

bool fitWaterPlane(const WaterPlaneSample *samples, usize count, WaterPlane &out) noexcept {
  if (samples == nullptr || count == 0) return false;

  double sumX = 0, sumZ = 0, sumH = 0;
  for (usize index = 0; index < count; ++index) {
    const auto &sample = samples[index];
    if (!std::isfinite(sample.x) || !std::isfinite(sample.z) || !std::isfinite(sample.height))
      return false;
    sumX += sample.x; sumZ += sample.z; sumH += sample.height;
  }
  const double inverse = 1.0 / static_cast<double>(count);
  const double meanX = sumX * inverse, meanZ = sumZ * inverse, meanH = sumH * inverse;

  // Plano horizontal na altura média é a resposta correta quando não há
  // informação para inclinar — e é exatamente o que o código fazia antes.
  const auto horizontal = [&]() {
    out.normal = {0.0f, 1.0f, 0.0f};
    out.offset = static_cast<float>(-meanH);
    return std::isfinite(out.offset);
  };
  if (count < 3) return horizontal();

  // Sistema normal em coordenadas centradas: o termo constante sai do ajuste e
  // sobra um 2x2 bem condicionado, mesmo com o corpo a quilômetros da origem.
  double xx = 0, xz = 0, zz = 0, xh = 0, zh = 0;
  for (usize index = 0; index < count; ++index) {
    const double x = samples[index].x - meanX;
    const double z = samples[index].z - meanZ;
    const double h = samples[index].height - meanH;
    xx += x * x; xz += x * z; zz += z * z; xh += x * h; zh += z * h;
  }
  const double determinant = xx * zz - xz * xz;
  // Amostras colineares (ou coincidentes) não determinam um plano. O limiar é
  // relativo à escala das próprias amostras: um absoluto rejeitaria um casco
  // pequeno e aceitaria ruído num casco grande.
  const double scale = xx + zz;
  if (!(scale > 0) || !(determinant > 1e-9 * scale * scale)) return horizontal();

  const double slopeX = (zz * xh - xz * zh) / determinant;
  const double slopeZ = (xx * zh - xz * xh) / determinant;
  if (!std::isfinite(slopeX) || !std::isfinite(slopeZ)) return horizontal();

  // A superfície é (x, slopeX*x + slopeZ*z + c, z); sua normal é (-a, 1, -b).
  const double length = std::sqrt(slopeX * slopeX + 1.0 + slopeZ * slopeZ);
  if (!(length > 0) || !std::isfinite(length)) return horizontal();
  const double nx = -slopeX / length, ny = 1.0 / length, nz = -slopeZ / length;

  // Altura do plano no centroide é a média, por construção do ajuste centrado.
  const double offset = -(nx * meanX + ny * meanH + nz * meanZ);
  if (!std::isfinite(offset)) return horizontal();

  out.normal = {static_cast<float>(nx), static_cast<float>(ny), static_cast<float>(nz)};
  out.offset = static_cast<float>(offset);
  return true;
}

SubmergedVolume submergedTetrahedron(const AetherVec3 corners[4], const WaterPlane &plane) noexcept {
  SubmergedVolume result{};
  for (int index = 0; index < 4; ++index)
    if (!finite(corners[index])) return result;
  if (!finite(plane.normal) || !std::isfinite(plane.offset)) return result;
  const float normalLength = length(plane.normal);
  if (!(normalLength > 1.0e-6f)) return result;
  const WaterPlane unit{scale(plane.normal, 1.0f / normalLength), plane.offset / normalLength};

  // Sort corners so the submerged ones come first. Sorting by height also makes
  // the interpolation stable when a corner sits exactly on the surface.
  AetherVec3 point[4];
  float height[4];
  for (int index = 0; index < 4; ++index) {
    point[index] = corners[index];
    height[index] = signedHeight(unit, corners[index]);
  }
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3 - i; ++j)
      if (height[j] > height[j + 1]) {
        std::swap(height[j], height[j + 1]);
        std::swap(point[j], point[j + 1]);
      }

  int below = 0;
  for (int index = 0; index < 4; ++index)
    if (height[index] < 0.0f) ++below;

  float volume = 0.0f;
  AetherVec3 weighted{};
  if (below == 0) return result;
  if (below == 4) {
    accumulateTetrahedron(point[0], point[1], point[2], point[3], volume, weighted);
  } else if (below == 1) {
    // One corner under water: the wet part is a small tetrahedron on that corner.
    const AetherVec3 a = interpolateToSurface(point[0], height[0], point[1], height[1]);
    const AetherVec3 b = interpolateToSurface(point[0], height[0], point[2], height[2]);
    const AetherVec3 c = interpolateToSurface(point[0], height[0], point[3], height[3]);
    accumulateTetrahedron(point[0], a, b, c, volume, weighted);
  } else if (below == 3) {
    // Three corners under water: take the whole tetrahedron and remove the dry
    // corner's tetrahedron. Building the wet prism directly would need a
    // consistent winding that adds nothing here.
    float whole = 0.0f;
    AetherVec3 wholeWeighted{};
    accumulateTetrahedron(point[0], point[1], point[2], point[3], whole, wholeWeighted);
    const AetherVec3 a = interpolateToSurface(point[0], height[0], point[3], height[3]);
    const AetherVec3 b = interpolateToSurface(point[1], height[1], point[3], height[3]);
    const AetherVec3 c = interpolateToSurface(point[2], height[2], point[3], height[3]);
    float dry = 0.0f;
    AetherVec3 dryWeighted{};
    accumulateTetrahedron(point[3], a, b, c, dry, dryWeighted);
    volume = whole - dry;
    weighted = subtract(wholeWeighted, dryWeighted);
    if (!(volume > 0.0f)) return result;
  } else {
    // Two under, two over: the wet part is a wedge, split into three tetrahedra.
    const AetherVec3 a = interpolateToSurface(point[0], height[0], point[2], height[2]);
    const AetherVec3 b = interpolateToSurface(point[0], height[0], point[3], height[3]);
    const AetherVec3 c = interpolateToSurface(point[1], height[1], point[2], height[2]);
    const AetherVec3 d = interpolateToSurface(point[1], height[1], point[3], height[3]);
    accumulateTetrahedron(point[0], point[1], a, b, volume, weighted);
    accumulateTetrahedron(point[1], a, b, d, volume, weighted);
    accumulateTetrahedron(point[1], a, c, d, volume, weighted);
  }
  if (!(volume > 0.0f) || !std::isfinite(volume)) return result;
  result.volume = volume;
  result.centroid = scale(weighted, 1.0f / volume);
  return result;
}

float buoyantShapeVolume(const BuoyantShape &shape) noexcept {
  if (!finite(shape.halfExtent)) return 0.0f;
  if (shape.kind == BuoyantShapeKind::Sphere) {
    const float radius = shape.halfExtent.x;
    if (!(radius > 0.0f)) return 0.0f;
    return 4.0f / 3.0f * Pi * radius * radius * radius;
  }
  const AetherVec3 &half = shape.halfExtent;
  if (!(half.x > 0.0f) || !(half.y > 0.0f) || !(half.z > 0.0f)) return 0.0f;
  return 8.0f * half.x * half.y * half.z;
}

SubmergedVolume submergedVolume(const BuoyantShape &shape, AetherVec3 position,
                                AetherQuat rotation, const WaterPlane &plane) noexcept {
  SubmergedVolume result{};
  if (!finite(position) || !finite(plane.normal) || !std::isfinite(plane.offset)) return result;
  const float normalLength = length(plane.normal);
  if (!(normalLength > 1.0e-6f)) return result;
  const WaterPlane unit{scale(plane.normal, 1.0f / normalLength), plane.offset / normalLength};

  if (shape.kind == BuoyantShapeKind::Sphere) {
    const float radius = shape.halfExtent.x;
    if (!(radius > 0.0f)) return result;
    const float centreHeight = signedHeight(unit, position);
    if (centreHeight >= radius) return result;
    if (centreHeight <= -radius) {
      result.volume = buoyantShapeVolume(shape);
      result.centroid = position;
      return result;
    }
    // Spherical cap of height h below the surface, in closed form. Its centroid
    // sits on the axis at 3(2r-h)^2 / (4(3r-h)) from the cap's flat face.
    const float capHeight = radius - centreHeight;
    result.volume = Pi * capHeight * capHeight * (radius - capHeight / 3.0f);
    const float centroidFromFace =
        3.0f * (2.0f * radius - capHeight) * (2.0f * radius - capHeight) /
        (4.0f * (3.0f * radius - capHeight));
    const float axialOffset = centreHeight - centroidFromFace;
    result.centroid = add(position, scale(unit.normal, axialOffset));
    return result;
  }

  const AetherVec3 &half = shape.halfExtent;
  if (!(half.x > 0.0f) || !(half.y > 0.0f) || !(half.z > 0.0f)) return result;
  const float sign[8][3] = {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
                            {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1}};
  AetherVec3 corner[8];
  for (int index = 0; index < 8; ++index)
    corner[index] = add(position, rotate(rotation, {sign[index][0] * half.x, sign[index][1] * half.y,
                                                    sign[index][2] * half.z}));
  // Six tetrahedra sharing the 0-6 diagonal tile the box exactly, so the sum is
  // the box and every clipped piece is exact rather than sampled.
  static constexpr int tiling[6][4] = {{0, 1, 2, 6}, {0, 2, 3, 6}, {0, 3, 7, 6},
                                       {0, 7, 4, 6}, {0, 4, 5, 6}, {0, 5, 1, 6}};
  float volume = 0.0f;
  AetherVec3 weighted{};
  for (const auto &indices : tiling) {
    const AetherVec3 tetrahedron[4] = {corner[indices[0]], corner[indices[1]], corner[indices[2]],
                                       corner[indices[3]]};
    const SubmergedVolume part = submergedTetrahedron(tetrahedron, unit);
    if (part.volume <= 0.0f) continue;
    volume += part.volume;
    weighted = add(weighted, scale(part.centroid, part.volume));
  }
  if (!(volume > 0.0f)) return result;
  result.volume = volume;
  result.centroid = scale(weighted, 1.0f / volume);
  return result;
}

bool validateBuoyancySettings(const BuoyancySettings &settings) noexcept {
  return std::isfinite(settings.fluidDensity) && settings.fluidDensity > 0.0f &&
         settings.fluidDensity <= 20000.0f && std::isfinite(settings.gravity) &&
         settings.gravity > 0.0f && settings.gravity <= 100.0f &&
         std::isfinite(settings.linearDrag) && settings.linearDrag >= 0.0f &&
         settings.linearDrag <= 100.0f && std::isfinite(settings.viscousDrag) &&
         settings.viscousDrag >= 0.0f && settings.viscousDrag <= 100.0f &&
         std::isfinite(settings.angularDrag) &&
         settings.angularDrag >= 0.0f && settings.angularDrag <= 100.0f &&
         std::isfinite(settings.addedMassCoefficient) && settings.addedMassCoefficient >= 0.0f &&
         settings.addedMassCoefficient <= 4.0f &&
         std::isfinite(settings.maximumAccelerationGravities) &&
         settings.maximumAccelerationGravities > 0.0f &&
         settings.maximumAccelerationGravities <= 1000.0f;
}

BuoyancyForces evaluateBuoyancy(const BuoyancyInput &input,
                                const BuoyancySettings &settings) noexcept {
  BuoyancyForces result{};
  result.effectiveMass = input.mass;
  if (!validateBuoyancySettings(settings)) return result;
  if (!std::isfinite(input.mass) || input.mass <= 0.0f) return result;
  if (!std::isfinite(input.submerged.volume) || input.submerged.volume <= 0.0f) return result;
  if (!finite(input.submerged.centroid) || !finite(input.bodyVelocity) ||
      !finite(input.waterVelocity) || !finite(input.angularVelocity) ||
      !std::isfinite(input.referenceArea) || input.referenceArea < 0.0f)
    return result;

  result.applicationPoint = input.submerged.centroid;
  const float displacedMass = settings.fluidDensity * input.submerged.volume;
  // Buoyancy acts against gravity through the centre of buoyancy, which is the
  // centroid of the wet volume, not the centre of mass. The offset between the
  // two is the entire righting moment.
  AetherVec3 force{0.0f, displacedMass * settings.gravity, 0.0f};

  // Drag responds to motion relative to the water, so a hull sitting in a swell
  // is pushed by the orbital velocity instead of ignoring it.
  const AetherVec3 relative = subtract(input.bodyVelocity, input.waterVelocity);
  const float speed = length(relative);
  if (speed > 1.0e-5f) {
    const float quadratic = 0.5f * settings.fluidDensity * settings.linearDrag *
                            input.referenceArea * speed * speed;
    force = subtract(force, scale(relative, quadratic / speed));
    force = subtract(force, scale(relative, settings.viscousDrag * displacedMass));
  }

  const float angularSpeed = length(input.angularVelocity);
  if (angularSpeed > 1.0e-5f) {
    const float magnitude = settings.angularDrag * displacedMass * angularSpeed;
    result.torque = scale(input.angularVelocity, -magnitude);
  }

  // Added mass is reported rather than folded into the force: the caller's
  // integrator needs it on the left-hand side, and applying it as a force would
  // make it depend on an acceleration that has not been solved yet.
  result.effectiveMass = input.mass + settings.addedMassCoefficient * displacedMass;

  const float limit = settings.maximumAccelerationGravities * settings.gravity * input.mass;
  const float applied = length(force);
  if (applied > limit) {
    force = scale(force, limit / applied);
    result.clamped = true;
  }
  result.force = force;
  return result;
}

} // namespace ae::physics
