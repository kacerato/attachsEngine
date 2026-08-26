// Harness A/B do item 4.1.6: Jolt 3D restrito ao plano XY versus Box2D v3.
//
// O mesmo código gera três executáveis:
//   benchmark_physics2d        compara CPU dos dois backends no mesmo processo;
//   benchmark_physics2d_jolt   mede Jolt isoladamente (RSS/tamanho do binário);
//   benchmark_physics2d_box2d  mede Box2D isoladamente (RSS/tamanho do binário).
//
// O cenário, seed implícita, formas, posições, materiais, dt, warm-up e número
// de amostras são idênticos. Os runners isolados evitam que caches/alocações de
// um motor contaminem a memória do outro. Resultado de host é diagnóstico: a
// decisão final exige os perfis móveis B/C descritos no plano de lacunas.

#if !defined(AETHER_BENCHMARK_BOX2D_ONLY)
#include "physics/jolt_bridge.h"
#define AETHER_BENCHMARK_HAS_JOLT 1
#endif

#if !defined(AETHER_BENCHMARK_JOLT_ONLY)
#include <box2d/box2d.h>
#define AETHER_BENCHMARK_HAS_BOX2D 1
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#elif defined(__linux__) || defined(__ANDROID__)
#include <unistd.h>
#endif

namespace {

using Clock = std::chrono::steady_clock;

constexpr float kTimeStep = 1.0f / 60.0f;
constexpr float kRadius = 0.45f;
constexpr float kFriction = 0.3f;
constexpr float kRestitution = 0.2f;
constexpr int kBodiesPerRow = 40;
constexpr float kBodySpacing = 0.91f;
[[maybe_unused]] constexpr int kJoltCollisionSteps = 4;
[[maybe_unused]] constexpr int kBox2DSubSteps = 4;
[[maybe_unused]] constexpr double kFrameBudgetMs = 16.6;

struct Distribution {
  double meanMs = 0.0;
  double p50Ms = 0.0;
  double p95Ms = 0.0;
  double p99Ms = 0.0;
  double maxMs = 0.0;
};

struct BenchmarkResult {
  const char *backend = "unknown";
  int requestedBodies = 0;
  int createdBodies = 0;
  int trials = 0;
  int warmupSteps = 0;
  int measuredSteps = 0;
  Distribution timing{};
  std::size_t peakRssDeltaBytes = 0;
  std::size_t internalBytes = 0;
  int contactCount = -1;
  double centroidY = 0.0;
  double minY = std::numeric_limits<double>::infinity();
  double maxY = -std::numeric_limits<double>::infinity();
  bool stable = false;
};

struct RunOptions {
  int trials = 3;
  bool quick = false;
  bool csvOnly = false;
};

double ElapsedMilliseconds(Clock::time_point start) {
  return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

std::size_t CurrentResidentBytes() {
#if defined(_WIN32)
  PROCESS_MEMORY_COUNTERS_EX counters{};
  counters.cb = sizeof(counters);
  if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&counters),
                           sizeof(counters)) == 0) {
    return 0;
  }
  return static_cast<std::size_t>(counters.WorkingSetSize);
#elif defined(__linux__) || defined(__ANDROID__)
  FILE *status = std::fopen("/proc/self/statm", "r");
  if (status == nullptr) return 0;
  unsigned long totalPages = 0;
  unsigned long residentPages = 0;
  const int fields = std::fscanf(status, "%lu %lu", &totalPages, &residentPages);
  std::fclose(status);
  (void)totalPages;
  const long pageSize = sysconf(_SC_PAGESIZE);
  if (fields != 2 || pageSize <= 0) return 0;
  return static_cast<std::size_t>(residentPages) * static_cast<std::size_t>(pageSize);
#else
  return 0;
#endif
}

void ObserveResidentPeak(std::size_t baseline, std::size_t &peakDelta) {
  const std::size_t current = CurrentResidentBytes();
  if (current > baseline) peakDelta = std::max(peakDelta, current - baseline);
}

double Quantile(const std::vector<double> &sorted, double q) {
  if (sorted.empty()) return std::numeric_limits<double>::quiet_NaN();
  const double position = q * static_cast<double>(sorted.size() - 1);
  const std::size_t lower = static_cast<std::size_t>(position);
  const std::size_t upper = std::min(lower + 1, sorted.size() - 1);
  const double fraction = position - static_cast<double>(lower);
  return sorted[lower] + (sorted[upper] - sorted[lower]) * fraction;
}

Distribution Summarize(std::vector<double> samples) {
  Distribution result{};
  if (samples.empty()) return result;
  double total = 0.0;
  for (double sample : samples) total += sample;
  result.meanMs = total / static_cast<double>(samples.size());
  std::sort(samples.begin(), samples.end());
  result.p50Ms = Quantile(samples, 0.50);
  result.p95Ms = Quantile(samples, 0.95);
  result.p99Ms = Quantile(samples, 0.99);
  result.maxMs = samples.back();
  return result;
}

float SpawnX(int index) {
  return static_cast<float>(index % kBodiesPerRow) * 1.1f -
         (static_cast<float>(kBodiesPerRow) * 1.1f * 0.5f);
}

float SpawnY(int index) {
  // A grade já nasce a 1 cm do contato (diâmetro=0,90 m). Isso evita que o
  // caso de 5.000 corpos meça apenas queda livre das fileiras superiores por
  // exigir centenas de frames até alcançar o piso.
  return 0.96f + static_cast<float>(index / kBodiesPerRow) * kBodySpacing;
}

#if defined(AETHER_BENCHMARK_HAS_JOLT)
AetherBodyDesc MakeJoltSphere(float x, float y) {
  AetherBodyDesc desc{};
  desc.shape.kind = AetherShapeKind::Sphere;
  desc.shape.sphereRadius = kRadius;
  desc.position = {x, y, 0.0f};
  desc.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
  desc.motionType = AetherMotionType::Dynamic;
  desc.friction = kFriction;
  desc.restitution = kRestitution;
  desc.allowedDOFs = AetherAllowedDOFs::Plane2D;
  return desc;
}

AetherBodyDesc MakeJoltFloor() {
  AetherBodyDesc desc{};
  desc.shape.kind = AetherShapeKind::Box;
  desc.shape.boxHalfExtent = {50.0f, 0.5f, 10.0f};
  desc.position = {0.0f, 0.0f, 0.0f};
  desc.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
  desc.motionType = AetherMotionType::Static;
  desc.friction = kFriction;
  desc.allowedDOFs = AetherAllowedDOFs::Plane2D;
  return desc;
}

std::size_t JoltTempAllocatorBytes(int bodyCount) {
  constexpr std::size_t minimum = 8u * 1024u * 1024u;
  const std::size_t bodies = static_cast<std::size_t>(bodyCount + 1);
  const std::size_t bodyPairs = static_cast<std::size_t>(bodyCount) * 16u;
  const std::size_t contacts = static_cast<std::size_t>(bodyCount) * 8u;
  const std::size_t broadPhasePairs = bodyCount < 1024 ? 16384u : bodyPairs;
  return std::max(minimum, bodies * 512u + bodyPairs * 128u + contacts * 1024u +
                               broadPhasePairs * 64u);
}

BenchmarkResult RunJolt(int bodyCount, int warmupSteps, int measuredSteps, int trials) {
  BenchmarkResult result{"jolt-plane2d", bodyCount, bodyCount, trials, warmupSteps, measuredSteps};
  result.internalBytes = JoltTempAllocatorBytes(bodyCount);
  result.stable = true;
  std::vector<double> samples;
  samples.reserve(static_cast<std::size_t>(trials * measuredSteps));
  std::size_t positionSamples = 0;

  for (int trial = 0; trial < trials; ++trial) {
    const std::size_t rssBaseline = CurrentResidentBytes();
    std::size_t rssPeakDelta = 0;
    AetherPhysicsWorldDescV2 worldDesc{};
    worldDesc.structSize = sizeof(worldDesc);
    worldDesc.apiVersion = AetherPhysicsWorldApiVersionV2;
    worldDesc.gravity = {0.0f, -9.81f, 0.0f};
    worldDesc.maxBodies = static_cast<ae::u32>(bodyCount + 1);
    worldDesc.maxBodyPairs = static_cast<ae::u32>(bodyCount * 16);
    worldDesc.maxContactConstraints = static_cast<ae::u32>(bodyCount * 8);
    worldDesc.maxBroadPhasePairs = static_cast<ae::u32>(bodyCount < 1024 ? 16384 : bodyCount * 16);
    worldDesc.overflowPolicy = AetherPhysicsOverflowPolicy::Warning;
    AetherPhysicsWorld *world = AetherPhysics_CreateWorldV2(&worldDesc);
    if (world == nullptr) {
      result.stable = false;
      result.createdBodies = 0;
      break;
    }

    const AetherBodyDesc floor = MakeJoltFloor();
    if (AetherPhysics_CreateBody(world, &floor) == AetherBodyHandle_Invalid) result.stable = false;
    std::vector<AetherBodyHandle> bodies;
    bodies.reserve(static_cast<std::size_t>(bodyCount));
    for (int i = 0; i < bodyCount; ++i) {
      const AetherBodyDesc desc = MakeJoltSphere(SpawnX(i), SpawnY(i));
      const AetherBodyHandle handle = AetherPhysics_CreateBody(world, &desc);
      if (handle != AetherBodyHandle_Invalid) bodies.push_back(handle);
    }
    result.createdBodies = std::min(result.createdBodies, static_cast<int>(bodies.size()));
    result.stable = result.stable && static_cast<int>(bodies.size()) == bodyCount;
    ObserveResidentPeak(rssBaseline, rssPeakDelta);

    const auto keepAwake = [&](int step) {
      for (std::size_t bodyIndex = 0; bodyIndex < bodies.size(); ++bodyIndex) {
        const AetherBodyHandle handle = bodies[bodyIndex];
        const AetherVec3 velocity = AetherPhysics_GetLinearVelocity(world, handle);
        const float excitation = ((step + static_cast<int>(bodyIndex)) & 1) == 0 ? -0.01f : 0.01f;
        AetherPhysics_SetLinearVelocity(world, handle, {excitation, velocity.y, 0.0f});
      }
    };
    for (int step = 0; step < warmupSteps; ++step) {
      keepAwake(step);
      if (AetherPhysics_StepV2(world, kTimeStep, kJoltCollisionSteps) != 0) result.stable = false;
      ObserveResidentPeak(rssBaseline, rssPeakDelta);
    }
    for (int step = 0; step < measuredSteps; ++step) {
      keepAwake(warmupSteps + step);
      const auto start = Clock::now();
      const ae::u32 errors = AetherPhysics_StepV2(world, kTimeStep, kJoltCollisionSteps);
      samples.push_back(ElapsedMilliseconds(start));
      if (errors != 0) result.stable = false;
      ObserveResidentPeak(rssBaseline, rssPeakDelta);
    }

    for (AetherBodyHandle handle : bodies) {
      AetherVec3 position{};
      AetherQuat rotation{};
      AetherPhysics_GetTransform(world, handle, &position, &rotation);
      if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z) ||
          !std::isfinite(rotation.x) || !std::isfinite(rotation.y) ||
          !std::isfinite(rotation.z) || !std::isfinite(rotation.w)) {
        result.stable = false;
        break;
      }
      result.centroidY += position.y;
      result.minY = std::min(result.minY, static_cast<double>(position.y));
      result.maxY = std::max(result.maxY, static_cast<double>(position.y));
      ++positionSamples;
    }
    AetherPhysics_DestroyWorld(world);
    result.peakRssDeltaBytes = std::max(result.peakRssDeltaBytes, rssPeakDelta);
  }

  result.timing = Summarize(std::move(samples));
  if (positionSamples > 0) result.centroidY /= static_cast<double>(positionSamples);
  return result;
}
#endif

#if defined(AETHER_BENCHMARK_HAS_BOX2D)
BenchmarkResult RunBox2D(int bodyCount, int warmupSteps, int measuredSteps, int trials) {
  BenchmarkResult result{"box2d-v3.1.1", bodyCount, bodyCount, trials, warmupSteps, measuredSteps};
  result.stable = true;
  std::vector<double> samples;
  samples.reserve(static_cast<std::size_t>(trials * measuredSteps));
  std::size_t positionSamples = 0;

  for (int trial = 0; trial < trials; ++trial) {
    const std::size_t rssBaseline = CurrentResidentBytes();
    std::size_t rssPeakDelta = 0;
    const int boxBytesBaseline = b2GetByteCount();
    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity = {0.0f, -9.81f};
    const b2WorldId world = b2CreateWorld(&worldDef);

    b2BodyDef floorBodyDef = b2DefaultBodyDef();
    const b2BodyId floorBody = b2CreateBody(world, &floorBodyDef);
    b2ShapeDef floorShapeDef = b2DefaultShapeDef();
    floorShapeDef.material.friction = kFriction;
    const b2Polygon floor = b2MakeBox(50.0f, 0.5f);
    const b2ShapeId floorShape = b2CreatePolygonShape(floorBody, &floorShapeDef, &floor);
    if (B2_IS_NULL(floorShape)) result.stable = false;

    std::vector<b2BodyId> bodies;
    bodies.reserve(static_cast<std::size_t>(bodyCount));
    b2ShapeDef circleShapeDef = b2DefaultShapeDef();
    circleShapeDef.density = 1.0f;
    circleShapeDef.material.friction = kFriction;
    circleShapeDef.material.restitution = kRestitution;
    const b2Circle circle{{0.0f, 0.0f}, kRadius};
    for (int i = 0; i < bodyCount; ++i) {
      b2BodyDef bodyDef = b2DefaultBodyDef();
      bodyDef.type = b2_dynamicBody;
      bodyDef.position = {SpawnX(i), SpawnY(i)};
      const b2BodyId body = b2CreateBody(world, &bodyDef);
      const b2ShapeId shape = b2CreateCircleShape(body, &circleShapeDef, &circle);
      if (b2Body_IsValid(body) && !B2_IS_NULL(shape)) bodies.push_back(body);
    }
    result.createdBodies = std::min(result.createdBodies, static_cast<int>(bodies.size()));
    result.stable = result.stable && static_cast<int>(bodies.size()) == bodyCount;
    ObserveResidentPeak(rssBaseline, rssPeakDelta);

    const auto keepAwake = [&](int step) {
      for (std::size_t bodyIndex = 0; bodyIndex < bodies.size(); ++bodyIndex) {
        const b2BodyId body = bodies[bodyIndex];
        b2Vec2 velocity = b2Body_GetLinearVelocity(body);
        velocity.x = ((step + static_cast<int>(bodyIndex)) & 1) == 0 ? -0.01f : 0.01f;
        b2Body_SetLinearVelocity(body, velocity);
      }
    };
    for (int step = 0; step < warmupSteps; ++step) {
      keepAwake(step);
      b2World_Step(world, kTimeStep, kBox2DSubSteps);
      ObserveResidentPeak(rssBaseline, rssPeakDelta);
    }
    for (int step = 0; step < measuredSteps; ++step) {
      keepAwake(warmupSteps + step);
      const auto start = Clock::now();
      b2World_Step(world, kTimeStep, kBox2DSubSteps);
      samples.push_back(ElapsedMilliseconds(start));
      ObserveResidentPeak(rssBaseline, rssPeakDelta);
    }

    for (b2BodyId body : bodies) {
      const b2Vec2 position = b2Body_GetPosition(body);
      if (!std::isfinite(position.x) || !std::isfinite(position.y)) {
        result.stable = false;
        break;
      }
      result.centroidY += position.y;
      result.minY = std::min(result.minY, static_cast<double>(position.y));
      result.maxY = std::max(result.maxY, static_cast<double>(position.y));
      ++positionSamples;
    }
    const b2Counters counters = b2World_GetCounters(world);
    result.contactCount = std::max(result.contactCount, counters.contactCount);
    const int boxBytes = b2GetByteCount() - boxBytesBaseline;
    if (boxBytes > 0) result.internalBytes = std::max(result.internalBytes, static_cast<std::size_t>(boxBytes));
    b2DestroyWorld(world);
    result.peakRssDeltaBytes = std::max(result.peakRssDeltaBytes, rssPeakDelta);
  }

  result.timing = Summarize(std::move(samples));
  if (positionSamples > 0) result.centroidY /= static_cast<double>(positionSamples);
  return result;
}
#endif

void PrintResult(const BenchmarkResult &result, bool isolatedProcess, bool csvOnly) {
  const double rssMiB = static_cast<double>(result.peakRssDeltaBytes) / (1024.0 * 1024.0);
  const double internalMiB = static_cast<double>(result.internalBytes) / (1024.0 * 1024.0);
  if (!csvOnly) {
    std::printf("%-15s %7d %8.3f %8.3f %8.3f %8.3f %9.2f %9.2f %8s\n", result.backend,
                result.createdBodies, result.timing.p50Ms, result.timing.p95Ms,
                result.timing.p99Ms, result.timing.maxMs, rssMiB, internalMiB,
                result.stable ? "sim" : "NAO");
  }
  std::printf("RESULT,%s,%d,%d,%d,%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%zu,%zu,%d,%.6f,%.6f,%.6f,%s,%s\n",
              result.backend, result.requestedBodies, result.createdBodies, result.trials,
              result.warmupSteps, result.measuredSteps, result.timing.meanMs,
              result.timing.p50Ms, result.timing.p95Ms, result.timing.p99Ms,
              result.timing.maxMs, result.peakRssDeltaBytes, result.internalBytes,
              result.contactCount, result.centroidY, result.minY, result.maxY,
              result.stable ? "true" : "false",
              isolatedProcess ? "true" : "false");
}

RunOptions ParseOptions(int argc, char **argv) {
  RunOptions options{};
  for (int i = 1; i < argc; ++i) {
    const std::string_view argument(argv[i]);
    if (argument == "--quick") {
      options.quick = true;
    } else if (argument == "--csv-only") {
      options.csvOnly = true;
    } else if (argument.starts_with("--trials=")) {
      const std::string_view value = argument.substr(std::strlen("--trials="));
      int parsed = 0;
      for (char digit : value) {
        if (digit < '0' || digit > '9') {
          parsed = 0;
          break;
        }
        parsed = parsed * 10 + (digit - '0');
      }
      if (parsed > 0 && parsed <= 20) options.trials = parsed;
    }
  }
  return options;
}

} // namespace

int main(int argc, char **argv) {
  const RunOptions options = ParseOptions(argc, argv);
#if defined(AETHER_BENCHMARK_JOLT_ONLY) || defined(AETHER_BENCHMARK_BOX2D_ONLY)
  constexpr bool isolatedProcess = true;
#else
  constexpr bool isolatedProcess = false;
#endif

  if (!options.csvOnly) {
    std::printf("Benchmark fisica 2D A/B v1 — Jolt Plane2D x Box2D v3.1.1\n");
    std::printf("Cenario equivalente: circulos r=%.2f, piso 100x1, dt=1/60, 4 passos internos em ambos\n", kRadius);
    std::printf("RSS comparavel somente nos runners isolados; host nao decide backend mobile.\n");
    std::printf("%-15s %7s %8s %8s %8s %8s %9s %9s %8s\n", "backend", "corpos", "p50(ms)",
                "p95(ms)", "p99(ms)", "max(ms)", "rss(MiB)", "int(MiB)", "estavel");
  }
  std::printf("SCHEMA,backend,requested_bodies,created_bodies,trials,warmup_steps,measured_steps,mean_ms,"
              "p50_ms,p95_ms,p99_ms,max_ms,peak_rss_delta_bytes,internal_bytes,max_contacts,"
              "centroid_y,min_y,max_y,stable,"
              "isolated_process\n");
#if defined(AETHER_BENCHMARK_HAS_JOLT) && defined(AETHER_BENCHMARK_HAS_BOX2D)
  std::printf("COMPARE_SCHEMA,requested_bodies,box2d_p50_gain_percent,box2d_p95_gain_percent,"
              "centroid_delta_percent,height_delta_percent,physically_equivalent,"
              "box2d_p99_within_16_6ms,jolt_p99_within_16_6ms\n");
#endif

  const int fullCounts[] = {50, 100, 200, 500, 1000, 5000};
  const int quickCounts[] = {50, 500};
  const int *counts = options.quick ? quickCounts : fullCounts;
  const int countLength = options.quick ? 2 : 6;
  bool allStable = true;

  for (int index = 0; index < countLength; ++index) {
    const int count = counts[index];
    const int warmupSteps = count >= 5000 ? 30 : 120;
    const int measuredSteps = count >= 5000 ? 60 : 180;
#if defined(AETHER_BENCHMARK_HAS_JOLT)
    const BenchmarkResult jolt = RunJolt(count, warmupSteps, measuredSteps, options.trials);
    PrintResult(jolt, isolatedProcess, options.csvOnly);
    allStable = allStable && jolt.stable;
#endif
#if defined(AETHER_BENCHMARK_HAS_BOX2D)
    const BenchmarkResult box2d = RunBox2D(count, warmupSteps, measuredSteps, options.trials);
    PrintResult(box2d, isolatedProcess, options.csvOnly);
    allStable = allStable && box2d.stable;
#endif
#if defined(AETHER_BENCHMARK_HAS_JOLT) && defined(AETHER_BENCHMARK_HAS_BOX2D)
    const double p50Gain = jolt.timing.p50Ms > 0.0
                               ? (jolt.timing.p50Ms - box2d.timing.p50Ms) / jolt.timing.p50Ms * 100.0
                               : 0.0;
    const double p95Gain = jolt.timing.p95Ms > 0.0
                               ? (jolt.timing.p95Ms - box2d.timing.p95Ms) / jolt.timing.p95Ms * 100.0
                               : 0.0;
    const double centroidDelta = std::abs(jolt.centroidY - box2d.centroidY) /
                                 std::max(1.0, std::abs(jolt.centroidY)) * 100.0;
    const double heightDelta = std::abs(jolt.maxY - box2d.maxY) /
                               std::max(1.0, std::abs(jolt.maxY)) * 100.0;
    const bool physicallyEquivalent = centroidDelta <= 10.0 && heightDelta <= 10.0;
    allStable = allStable && physicallyEquivalent;
    std::printf("COMPARE,%d,%.3f,%.3f,%.3f,%.3f,%s,%s,%s\n", count, p50Gain, p95Gain,
                centroidDelta, heightDelta, physicallyEquivalent ? "true" : "false",
                box2d.timing.p99Ms <= kFrameBudgetMs ? "true" : "false",
                jolt.timing.p99Ms <= kFrameBudgetMs ? "true" : "false");
#endif
  }

  if (!options.csvOnly) {
    std::printf("\nCOMPARE: ganho positivo = Box2D mais rapido. Gate inicial: >=30%% sustentado em B/C.\n");
    std::printf("Saida valida para decisao: estabilidade=%s; falta executar matriz mobile B/C.\n",
                allStable ? "sim" : "NAO");
  }
  return allStable ? 0 : 2;
}
