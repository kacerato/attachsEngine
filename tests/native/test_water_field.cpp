#include "harness.h"
#include "renderer/water_field.h"

#include <array>
#include <cmath>
#include <thread>
#include <vector>

using namespace ae::renderer;

namespace {
WaterProfile twoWaveProfile() {
  WaterProfile p{};
  p.waveCount = 2;
  p.waves[0] = {{1.0f, 0.0f}, 0.5f, 8.0f, 1.2f, 0.4f, 0.0f};
  p.waves[1] = {{0.0f, 1.0f}, 0.15f, 2.0f, 2.0f, 0.2f, 0.3f};
  return p;
}

WaterFieldSetup baseSetup() {
  WaterFieldSetup setup{};
  setup.profile = twoWaveProfile();
  setup.baseHeight = 3.0f;
  return setup;
}
} // namespace

AE_TEST(Water_field_rejects_configuration_that_cannot_be_sampled) {
  WaterField field;
  WaterFieldSetup setup = baseSetup();
  setup.profile.waves[0].direction = {2.0f, 0.0f};
  AE_EXPECT_TRUE(!field.configure(setup), "invalid profile must not configure a field");
  AE_EXPECT_TRUE(!field.status().configured, "rejected configuration stays unconfigured");

  setup = baseSetup();
  setup.exclusionCount = MaximumWaterExclusionVolumes + 1;
  AE_EXPECT_TRUE(!field.configure(setup), "exclusion capacity is a contract, not a hint");

  setup = baseSetup();
  setup.hasBathymetry = true;
  setup.bottomHeight = setup.baseHeight;
  AE_EXPECT_TRUE(!field.configure(setup), "a bottom at the surface is not a depth");
}

AE_TEST(Water_field_reports_analytic_fallback_when_spectral_is_requested) {
  WaterField field;
  WaterFieldSetup setup = baseSetup();
  setup.requested = WaterFieldProvider::SpectralGpu;
  AE_EXPECT_TRUE(field.configure(setup), "fallback still yields a usable field");
  const WaterFieldStatus status = field.status();
  AE_EXPECT_EQ(status.requested, WaterFieldProvider::SpectralGpu, "requested is preserved");
  AE_EXPECT_EQ(status.resolved, WaterFieldProvider::Analytic, "resolved reports the truth");
  AE_EXPECT_TRUE(status.fallbackReason != nullptr, "a fallback must be named, never silent");

  setup.requested = WaterFieldProvider::Analytic;
  AE_EXPECT_TRUE(field.configure(setup), "analytic configures");
  AE_EXPECT_TRUE(field.status().fallbackReason == nullptr,
                 "no fallback reason when the request was honoured");
}

AE_TEST(Water_field_batch_refuses_undersized_output_without_writing) {
  WaterField field;
  AE_EXPECT_TRUE(field.configure(baseSetup()), "configure");
  const std::array<WaterVec2, 3> positions{{{0.0f, 0.0f}, {1.0f, 2.0f}, {5.0f, 5.0f}}};
  std::array<WaterFieldSample, 2> tooSmall{};
  tooSmall[0].height = 123.0f;
  AE_EXPECT_TRUE(!field.sample(positions, 0.5, tooSmall), "undersized output is rejected");
  AE_EXPECT_TRUE(tooSmall[0].height == 123.0f, "rejected batch writes nothing at all");

  std::array<WaterFieldSample, 3> ok{};
  AE_EXPECT_TRUE(field.sample(positions, 0.5, ok), "matching output is accepted");
  AE_EXPECT_TRUE(!field.sample(positions, std::nan(""), ok), "non-finite time is rejected");
}

AE_TEST(Water_field_batch_matches_single_query_and_includes_base_height) {
  WaterField field;
  const WaterFieldSetup setup = baseSetup();
  AE_EXPECT_TRUE(field.configure(setup), "configure");
  std::array<WaterVec2, 4> positions{{{0.0f, 0.0f}, {3.5f, -2.0f}, {-7.0f, 11.0f}, {40.0f, 40.0f}}};
  std::array<WaterFieldSample, 4> samples{};
  AE_EXPECT_TRUE(field.sample(positions, 1.75, samples), "batch");
  for (std::size_t index = 0; index < positions.size(); ++index) {
    const float single = field.heightOnly(positions[index], 1.75);
    AE_EXPECT_TRUE(std::abs(single - samples[index].height) < 1.0e-6f,
                   "batch and single query must not diverge");
    const WaterSample raw = sampleWaterSurface(setup.profile, positions[index], 1.75f);
    AE_EXPECT_TRUE(std::abs(samples[index].height - (setup.baseHeight + raw.height)) < 1.0e-5f,
                   "field height carries the base height of the surface");
    AE_EXPECT_TRUE(ae::renderer::hasWaterFieldFlag(samples[index].flags, WaterFieldFlag::Valid),
                   "configured field yields valid samples");
    AE_EXPECT_TRUE(!ae::renderer::hasWaterFieldFlag(samples[index].flags,
                                                    WaterFieldFlag::DepthKnown),
                   "depth is not reported without bathymetry");
    AE_EXPECT_TRUE(samples[index].jacobian == 1.0f,
                   "vertical-only displacement has determinant one");
  }
}

AE_TEST(Water_field_horizontal_orbital_velocity_follows_linear_theory) {
  // A single deep-water component has horizontal orbital velocity in phase with
  // its own elevation: u = omega * eta along the propagation direction. A hull
  // drifting in a swell depends on this term, so it is pinned by a closed form.
  WaterProfile profile{};
  profile.waveCount = 1;
  profile.waves[0] = {{1.0f, 0.0f}, 0.6f, 12.0f, 1.4f, 0.0f, 0.0f};
  WaterFieldSetup setup{};
  setup.profile = profile;
  WaterField field;
  AE_EXPECT_TRUE(field.configure(setup), "configure");

  for (float x : {0.0f, 2.3f, 7.1f, -4.5f}) {
    const std::array<WaterVec2, 1> position{{{x, 0.0f}}};
    std::array<WaterFieldSample, 1> out{};
    AE_EXPECT_TRUE(field.sample(position, 0.9, out), "sample");
    const WaterFieldSample &sample = out[0];
    const float elevation = sample.height;  // base height is zero in this setup
    const float expected = profile.waves[0].speed * elevation;
    AE_EXPECT_TRUE(std::abs(sample.velocity.x - expected) < 1.0e-4f,
                   "horizontal orbital velocity must equal omega times elevation");
    AE_EXPECT_TRUE(std::abs(sample.velocity.z) < 1.0e-6f,
                   "a wave travelling along X carries no orbital velocity across it");
  }
}

AE_TEST(Water_field_exclusion_removes_water_and_feathers_without_jumps) {
  WaterFieldSetup setup = baseSetup();
  setup.exclusionCount = 1;
  setup.exclusions[0].shape = WaterExclusionShape::Circle;
  setup.exclusions[0].center = {0.0f, 0.0f};
  setup.exclusions[0].halfExtent = {4.0f, 4.0f};
  setup.exclusions[0].feather = 1.0f;
  WaterField field;
  AE_EXPECT_TRUE(field.configure(setup), "configure");

  std::array<WaterVec2, 1> centre{{{0.0f, 0.0f}}};
  std::array<WaterFieldSample, 1> inside{};
  AE_EXPECT_TRUE(field.sample(centre, 0.0, inside), "sample centre");
  AE_EXPECT_TRUE(inside[0].coverage == 0.0f, "the centre of the volume has no water");
  AE_EXPECT_TRUE(ae::renderer::hasWaterFieldFlag(inside[0].flags, WaterFieldFlag::Excluded),
                 "zero coverage raises the exclusion flag");

  float previous = 0.0f;
  for (int step = 0; step <= 80; ++step) {
    const float radius = 3.5f + static_cast<float>(step) * 0.025f;
    std::array<WaterVec2, 1> position{{{radius, 0.0f}}};
    std::array<WaterFieldSample, 1> sample{};
    AE_EXPECT_TRUE(field.sample(position, 0.0, sample), "sample ring");
    AE_EXPECT_TRUE(sample[0].coverage >= previous - 1.0e-6f, "coverage grows outwards");
    AE_EXPECT_TRUE(sample[0].coverage - previous < 0.12f,
                   "feathered coverage must not step, or the shoreline pops");
    previous = sample[0].coverage;
  }
  AE_EXPECT_TRUE(previous > 0.99f, "water returns fully outside the feather band");
}

AE_TEST(Water_field_depth_is_reported_only_when_bathymetry_is_configured) {
  WaterFieldSetup setup = baseSetup();
  setup.hasBathymetry = true;
  setup.bottomHeight = -9.0f;
  WaterField field;
  AE_EXPECT_TRUE(field.configure(setup), "configure");
  std::array<WaterVec2, 1> position{{{2.0f, 2.0f}}};
  std::array<WaterFieldSample, 1> sample{};
  AE_EXPECT_TRUE(field.sample(position, 0.25, sample), "sample");
  AE_EXPECT_TRUE(ae::renderer::hasWaterFieldFlag(sample[0].flags, WaterFieldFlag::DepthKnown),
                 "configured bathymetry is reported as known");
  AE_EXPECT_TRUE(std::abs(sample[0].depth - 12.0f) < 1.0e-5f,
                 "depth measures the still surface down to the bottom");
}

AE_TEST(Water_field_is_deterministic_across_threads) {
  WaterField field;
  AE_EXPECT_TRUE(field.configure(baseSetup()), "configure");
  constexpr std::size_t count = 512;
  std::vector<WaterVec2> positions(count);
  for (std::size_t index = 0; index < count; ++index)
    positions[index] = {static_cast<float>(index) * 0.37f - 90.0f,
                        static_cast<float>(index) * -0.61f + 20.0f};

  std::vector<WaterFieldSample> reference(count);
  AE_EXPECT_TRUE(field.sample(positions, 4.5, reference), "reference batch");

  std::array<std::vector<WaterFieldSample>, 4> results;
  std::array<std::thread, 4> workers;
  for (std::size_t worker = 0; worker < workers.size(); ++worker) {
    results[worker].resize(count);
    workers[worker] = std::thread([&field, &positions, &results, worker] {
      field.sample(positions, 4.5, results[worker]);
    });
  }
  for (auto &worker : workers) worker.join();

  for (const auto &result : results)
    for (std::size_t index = 0; index < count; ++index) {
      AE_EXPECT_TRUE(result[index].height == reference[index].height,
                     "concurrent queries must be bit-identical, not merely close");
      AE_EXPECT_TRUE(result[index].velocity.x == reference[index].velocity.x &&
                         result[index].velocity.y == reference[index].velocity.y,
                     "velocity is a pure function of position and time");
    }
}

AE_TEST(Water_field_unconfigured_reports_nothing_valid) {
  WaterField field;
  AE_EXPECT_TRUE(!field.status().configured, "a fresh field is unconfigured");
  std::array<WaterVec2, 1> position{{{0.0f, 0.0f}}};
  std::array<WaterFieldSample, 1> sample{};
  AE_EXPECT_TRUE(!field.sample(position, 0.0, sample), "unconfigured field refuses to answer");
  AE_EXPECT_TRUE(sample[0].flags == 0, "no flag is raised when nothing was sampled");
}
