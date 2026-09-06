#include "harness.h"
#include "renderer/water_field.h"
#include "renderer/water_world.h"
#include "renderer/water_shading.h"
#include "renderer/map_draw_update.h"
#include "renderer/gpu_cost_isolation.h"
#include "renderer/water_spectral_mirror.h"

#include <array>
#include <cmath>
#include <string_view>
#include <thread>
#include <vector>

using namespace ae::renderer;

AE_TEST(Map_dynamic_pose_updates_matrix_and_bounds_transactionally) {
  MapDrawRecord source{};
  source.indexCount = 36;
  source.materialIndex = 4;
  const float model[16] = {2,0,0,0, 0,3,0,0, 0,0,1,0, 10,20,30,1};
  const float center[3] = {1,2,3};
  MapDrawUpdate update{};
  AE_EXPECT_TRUE(prepareMapDrawUpdate(2, source, model, center, 1, update), "valid transform");
  AE_EXPECT_TRUE(update.draw.boundsCenter[0] == 12 && update.draw.boundsCenter[1] == 26 &&
                 update.draw.boundsCenter[2] == 33, "bounds transformed from local space");
  AE_EXPECT_TRUE(update.draw.boundsRadius == 3, "conservative scale");
  AE_EXPECT_TRUE(update.instance.model[12] == 10 && update.draw.materialIndex == 4 &&
                 update.draw.indexCount == 36, "instance and immutable geometry");
  float invalid[16]{};
  AE_EXPECT_TRUE(!prepareMapDrawUpdate(7, source, invalid, center, 1, update), "singular rejected");
  AE_EXPECT_TRUE(update.drawIndex == 2 && update.draw.boundsRadius == 3, "old result preserved");
}

AE_TEST(Water_shading_variance_preserves_flat_normals_and_bounds_broadening) {
  AE_EXPECT_TRUE(validateWaterShading({}), "defaults valid");
  AE_EXPECT_TRUE(!validateWaterShading({2, 1}), "AA envelope");
  AE_EXPECT_TRUE(!validateWaterShading({.5f, -1}), "foam envelope");
  WaterShadingSettings detail{};
  detail.microDisplacement = 2.0f;
  AE_EXPECT_TRUE(!validateWaterShading(detail), "micro geometry envelope");
  detail = {};
  AE_EXPECT_TRUE(std::abs(maximumWaterDetailDisplacement(detail) - .54f) < 1e-6f,
                 "foam and micro bounds are reserved together");
  AE_EXPECT_TRUE(std::abs(filteredWaterRoughness(.2f, 0, 1) - .2f) < 1e-6f, "flat floor");
  AE_EXPECT_TRUE(std::abs(filteredWaterRoughness(.2f, 1, 0) - .2f) < 1e-6f, "disabled");
  const float filtered = filteredWaterRoughness(.2f, .1f, .5f);
  AE_EXPECT_TRUE(filtered > .2f && filtered < 1, "roughen unresolved variation");
  AE_EXPECT_TRUE(filteredWaterRoughness(1, 1, 1) == 1, "bounded maximum");
}

AE_TEST(Water_field_live_invalid_edit_and_invalid_batch_are_transactional) {
  WaterField field;
  WaterFieldSetup setup{};
  setup.baseHeight = 7;
  AE_EXPECT_TRUE(field.configure(setup), "valid field");
  setup.requested = static_cast<WaterFieldProvider>(99);
  AE_EXPECT_TRUE(!field.configure(setup), "unknown provider rejected");
  AE_EXPECT_TRUE(field.heightOnly({}, 0) == 7, "old field remains");
  std::array<WaterVec2, 2> positions{{{0, 0}, {std::nanf(""), 0}}};
  std::array<WaterFieldSample, 2> output{};
  output[0].height = 99;
  AE_EXPECT_TRUE(!field.sample(positions, 0, output), "whole batch rejected");
  AE_EXPECT_TRUE(output[0].height == 99, "no partial overwrite");
}

AE_TEST(Water_world_resolves_layers_priority_height_and_stable_id) {
  WaterWorld world;
  WaterFieldSetup low{}, high{};
  high.baseHeight = 4;
  AE_EXPECT_TRUE(world.setVolume(20, low, 0, 1), "first volume");
  AE_EXPECT_TRUE(world.setVolume(10, high, 0, 2), "second volume");
  const WaterVec2 position{};
  WaterVolumeQuery result{};
  AE_EXPECT_TRUE(world.sample({&position, 1}, 0, 3, {&result, 1}) && result.volume == 10, "highest");
  AE_EXPECT_TRUE(world.sample({&position, 1}, 0, 1, {&result, 1}) && result.volume == 20, "layers");
  AE_EXPECT_TRUE(world.setVolume(20, low, 2, 1), "raise priority");
  AE_EXPECT_TRUE(world.sample({&position, 1}, 0, 3, {&result, 1}) && result.volume == 20, "priority");
  AE_EXPECT_TRUE(world.setVolume(5, low, 2, 1), "tie");
  AE_EXPECT_TRUE(world.sample({&position, 1}, 0, 3, {&result, 1}) && result.volume == 5, "stable id");
  AE_EXPECT_TRUE(world.removeVolume(5), "unload");
  AE_EXPECT_TRUE(world.volumeCount() == 2, "count");
  world.clear();
  AE_EXPECT_TRUE(world.sample({&position, 1}, 0, 3, {&result, 1}) && result.volume == 0, "empty world");
}

AE_TEST(Water_world_failed_edits_preserve_previous_volume_and_query_output) {
  WaterWorld world;
  WaterFieldSetup setup{};
  AE_EXPECT_TRUE(world.setVolume(1, setup), "valid volume");
  setup.currents.maximumSpeed = -1;
  AE_EXPECT_TRUE(!world.setVolume(1, setup), "reject invalid edit");
  const WaterVec2 position{};
  WaterVolumeQuery result{};
  AE_EXPECT_TRUE(world.sample({&position, 1}, 0, ~0u, {&result, 1}) && result.volume == 1, "old preserved");
  AE_EXPECT_TRUE(!world.sample({&position, 1}, std::nan(""), ~0u, {&result, 1}), "invalid time");
  AE_EXPECT_TRUE(result.volume == 1, "output preserved");
  AE_EXPECT_TRUE(!world.setVolume(0, {}), "reserved id");
}

AE_TEST(Water_bathymetry_interpolates_and_preserves_unknown_outside_tile) {
  WaterBathymetry grid{};
  grid.width = grid.height = 2;
  grid.bottomHeights[0] = -2; grid.bottomHeights[1] = -4;
  grid.bottomHeights[2] = -6; grid.bottomHeights[3] = -8;
  float bottom = 99;
  AE_EXPECT_TRUE(sampleWaterBottom(grid, {.5f, .5f}, bottom), "inside tile");
  AE_EXPECT_TRUE(std::abs(bottom + 5) < 1e-5f, "bilinear bottom");
  AE_EXPECT_TRUE(!sampleWaterBottom(grid, {2, 0}, bottom), "unknown outside");
  AE_EXPECT_TRUE(bottom == -5, "failed query leaves output unchanged");
  AE_EXPECT_TRUE(sampleWaterBottom(grid, {1, 1}, bottom) && bottom == -8, "last corner");
  grid.spacing.x = 0;
  AE_EXPECT_TRUE(!validateWaterBathymetry(grid), "invalid spacing");
}

AE_TEST(Water_field_handles_bounded_domains_and_dry_bathymetry) {
  WaterFieldSetup setup{};
  setup.bounded = true;
  setup.boundary.halfExtent = {10, 10};
  setup.bathymetry.width = setup.bathymetry.height = 2;
  setup.bathymetry.bottomHeights[0] = 1;
  setup.bathymetry.bottomHeights[1] = -3;
  setup.bathymetry.bottomHeights[2] = -3;
  setup.bathymetry.bottomHeights[3] = -3;
  WaterField field;
  AE_EXPECT_TRUE(field.configure(setup), "bounded field");
  std::array<WaterVec2, 3> positions{{{0, 0}, {1, 1}, {20, 0}}};
  std::array<WaterFieldSample, 3> samples{};
  AE_EXPECT_TRUE(field.sample(positions, 0, samples), "sample batch");
  AE_EXPECT_TRUE(samples[0].coverage == 0 && samples[0].depth == 0, "land is dry");
  AE_EXPECT_TRUE(samples[1].coverage > 0 && samples[1].depth == 3, "local depth");
  AE_EXPECT_TRUE(samples[2].coverage == 0, "outside domain");
  AE_EXPECT_TRUE(!hasWaterFieldFlag(samples[2].flags, WaterFieldFlag::DepthKnown), "no invented depth");
}

AE_TEST(Water_currents_have_finite_core_smooth_boundary_and_speed_budget) {
  WaterCurrentSettings settings{};
  settings.count = 1;
  settings.sources[0].kind = WaterCurrentKind::Vortex;
  settings.sources[0].speed = 8.0f;
  AE_EXPECT_TRUE(validateWaterCurrents(settings), "valid current");
  const auto center = sampleWaterCurrent(settings, {0, 0});
  const auto middle = sampleWaterCurrent(settings, {5, 0});
  const auto edge = sampleWaterCurrent(settings, {10, 0});
  AE_EXPECT_TRUE(center.x == 0 && center.y == 0, "no singular core");
  AE_EXPECT_TRUE(middle.x == 0 && std::abs(middle.y - 2.0f) < 1e-5f, "tangential flow");
  AE_EXPECT_TRUE(edge.x == 0 && edge.y == 0, "compact support");
  settings.uniform = {20, 20};
  settings.maximumSpeed = 3;
  const auto capped = sampleWaterCurrent(settings, {5, 0});
  AE_EXPECT_TRUE(std::abs(std::hypot(capped.x, capped.y) - 3.0f) < 1e-5f, "global budget");
  settings.sources[0].radius = 0;
  AE_EXPECT_TRUE(!validateWaterCurrents(settings), "reject zero radius");
}

AE_TEST(Water_field_exposes_current_separately_from_orbital_velocity) {
  WaterFieldSetup setup{};
  setup.currents.uniform = {2, -3};
  WaterField field;
  AE_EXPECT_TRUE(field.configure(setup), "configure flat water with current");
  std::array<WaterVec2, 1> positions{{{0, 0}}};
  std::array<WaterFieldSample, 1> samples{};
  AE_EXPECT_TRUE(field.sample(positions, 0, samples), "batched query");
  AE_EXPECT_TRUE(samples[0].flow.x == 2 && samples[0].flow.y == -3, "current exposed");
  AE_EXPECT_TRUE(samples[0].velocity.x == 0, "do not double count current");
}

AE_TEST(Water_currents_reverse_radial_flow_and_validate_directional_sources) {
  WaterCurrentSettings settings{};
  settings.count = 1;
  auto &source = settings.sources[0];
  source.kind = WaterCurrentKind::Radial;
  source.speed = -4;
  const auto flow = sampleWaterCurrent(settings, {5, 0});
  AE_EXPECT_TRUE(std::abs(flow.x + 1.0f) < 1e-5f && flow.y == 0, "inward radial flow");
  source.kind = WaterCurrentKind::Directional;
  source.direction = {0, 0};
  AE_EXPECT_TRUE(!validateWaterCurrents(settings), "direction must be normalized");
  source.direction = {0, 1};
  AE_EXPECT_TRUE(validateWaterCurrents(settings), "unit direction");
  source.kind = static_cast<WaterCurrentKind>(99);
  AE_EXPECT_TRUE(!validateWaterCurrents(settings), "unknown source type rejected");
}

AE_TEST(Water_field_exclusion_disables_currents_inside_dry_volumes) {
  WaterFieldSetup setup{};
  setup.currents.uniform = {4, 2};
  setup.exclusionCount = 1;
  WaterField field;
  AE_EXPECT_TRUE(field.configure(setup), "configure");
  std::array<WaterVec2, 1> positions{{{0, 0}}};
  std::array<WaterFieldSample, 1> samples{};
  AE_EXPECT_TRUE(field.sample(positions, 0, samples), "query");
  AE_EXPECT_TRUE(hasWaterFieldFlag(samples[0].flags, WaterFieldFlag::Excluded), "dry interior");
  AE_EXPECT_TRUE(samples[0].flow.x == 0 && samples[0].flow.y == 0, "no current in dry interior");
}

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

AE_TEST(Water_field_resolves_the_cpu_mirror_and_reports_what_it_cannot_compute) {
  WaterMirrorSettings mirrorSettings{};
  mirrorSettings.spectrum.resolution = 32;
  mirrorSettings.spectrum.patchLength = 64.0f;
  mirrorSettings.spectrum.depth = 40.0f;
  mirrorSettings.spectrum.windSpeed = 9.0f;
  mirrorSettings.spectrum.seed = 3;
  WaterSpectralMirror mirror;
  AE_EXPECT_TRUE(mirror.initialize(mirrorSettings), "mirror initializes");

  WaterFieldSetup setup = baseSetup();
  setup.requested = WaterFieldProvider::SpectralCpu;
  setup.mirror = &mirror;
  WaterField field;

  // An un-evaluated mirror is not a provider: resolving to it would hand the
  // caller a flat sea while claiming the spectral path is running.
  AE_EXPECT_TRUE(field.configure(setup), "configure before the first update");
  AE_EXPECT_EQ(field.status().resolved, WaterFieldProvider::Analytic, "falls back before update");
  AE_EXPECT_TRUE(field.status().fallbackReason != nullptr, "and says why");

  AE_EXPECT_TRUE(mirror.update(2.0), "evaluate the mirror");
  AE_EXPECT_TRUE(field.configure(setup), "configure again");
  const WaterFieldStatus status = field.status();
  AE_EXPECT_EQ(status.resolved, WaterFieldProvider::SpectralCpu, "mirror is resolved");
  AE_EXPECT_TRUE(status.fallbackReason == nullptr, "no fallback when resolved");
  AE_EXPECT_EQ(status.cascadeResolution, 32u, "resolution is reported, not guessed");
  AE_EXPECT_TRUE(status.ageFrames == 0u, "the mirror has no age: it is evaluated at the query time");

  std::array<WaterVec2, 3> positions{{{0.0f, 0.0f}, {13.0f, -5.0f}, {-21.0f, 8.0f}}};
  std::array<WaterFieldSample, 3> samples{};
  AE_EXPECT_TRUE(field.sample(positions, 2.0, samples), "sample");
  bool anyDisplacement = false;
  for (const auto &sample : samples) {
    AE_EXPECT_TRUE(std::isfinite(sample.height), "mirror height is finite");
    AE_EXPECT_TRUE(!ae::renderer::hasWaterFieldFlag(sample.flags, WaterFieldFlag::JacobianKnown),
                   "the mirror does not compute the determinant and must not claim it");
    if (std::abs(sample.height - setup.baseHeight) > 1.0e-6f) anyDisplacement = true;
  }
  AE_EXPECT_TRUE(anyDisplacement, "a resolved mirror must actually move the surface");
}

AE_TEST(Water_field_reports_and_samples_the_complete_spectral_mirror_set) {
  const auto cascades = defaultWaterCascadeSettings();
  WaterSpectralMirrorSet mirrors;
  AE_EXPECT_TRUE(mirrors.initialize(cascades, {}), "configure mirror set");
  AE_EXPECT_TRUE(mirrors.update(0.75), "evaluate mirror set");

  WaterFieldSetup setup = baseSetup();
  setup.requested = WaterFieldProvider::SpectralCpu;
  setup.mirrorSet = &mirrors;
  WaterField field;
  AE_EXPECT_TRUE(field.configure(setup), "configure field");
  const WaterFieldStatus status = field.status();
  AE_EXPECT_EQ(status.resolved, WaterFieldProvider::SpectralCpu, "set becomes active provider");
  AE_EXPECT_EQ(status.cascadesActive, 3u, "status exposes every combined band");
  AE_EXPECT_EQ(status.cascadeResolution, 128u, "status exposes maximum mirror resolution");

  std::array<WaterVec2, 2> positions{{{3.0f, 7.0f}, {-19.0f, 41.0f}}};
  std::array<WaterFieldSample, 2> samples{};
  AE_EXPECT_TRUE(field.sample(positions, 0.75, samples), "batch sample");
  AE_EXPECT_TRUE(std::isfinite(samples[0].height) && std::isfinite(samples[1].height),
                 "physics receives finite combined heights");
  AE_EXPECT_TRUE(status.fallbackReason == nullptr, "resolved multicascade path is not a fallback");
}

AE_TEST(Water_cost_isolation_falls_back_to_full_instead_of_reading_past_the_enum) {
  using ae::renderer::WaterCostIsolation;
  AE_EXPECT_EQ(ae::renderer::sanitizeWaterCostIsolation(0), WaterCostIsolation::Full, "zero is production");
  AE_EXPECT_EQ(ae::renderer::sanitizeWaterCostIsolation(7), WaterCostIsolation::NoVertexSpectral,
               "last mode is valid");
  // An out-of-range diagnostic value must not select a mode by index arithmetic:
  // a shader branching on garbage would silently draw something else.
  AE_EXPECT_EQ(ae::renderer::sanitizeWaterCostIsolation(8), WaterCostIsolation::Full, "past the end is full");
  AE_EXPECT_EQ(ae::renderer::sanitizeWaterCostIsolation(4000000000u), WaterCostIsolation::Full, "huge is full");
  AE_EXPECT_TRUE(std::string_view(ae::renderer::waterCostIsolationName(WaterCostIsolation::NoShadow)) ==
                     "no-shadow",
                 "every mode reports a name the capture can be labelled with");
}

AE_TEST(Water_field_sums_the_ripple_field_into_the_shared_query) {
  // O ponto de junção: quem pergunta a altura da água recebe o mar, a interação
  // e a ondulação somados, sem saber que são três sistemas diferentes.
  WaterRippleField ripples;
  WaterRippleSettings rippleSettings{};
  rippleSettings.areaSize = 20.0f;
  rippleSettings.resolution = 40;
  AE_EXPECT_TRUE(ripples.initialize(rippleSettings), "ripple field");
  ripples.addImpulse(0.0f, 0.0f, 2.0f, 0.5f);

  WaterFieldSetup setup{};
  setup.profile = defaultOceanWaterProfile();
  setup.profile.waveCount = 0;  // isola a ondulação do espectro
  WaterField without;
  AE_EXPECT_TRUE(without.configure(setup), "configure without ripples");
  const float flat = without.heightOnly({0.0f, 0.0f}, 0.0);

  setup.ripples = &ripples;
  WaterField with;
  AE_EXPECT_TRUE(with.configure(setup), "configure with ripples");
  const float raised = with.heightOnly({0.0f, 0.0f}, 0.0);
  AE_EXPECT_TRUE(raised > flat + 0.1f, "the ripple lifts the surface");

  // Fora da área simulada a ondulação devolve zero, e somar zero é exatamente o
  // que se quer na fronteira: sem degrau entre o que é simulado e o que não é.
  const float outside = with.heightOnly({500.0f, 0.0f}, 0.0);
  const float outsideWithout = without.heightOnly({500.0f, 0.0f}, 0.0);
  AE_EXPECT_TRUE(std::fabs(outside - outsideWithout) < 1e-5f, "no step at the boundary");

  // A inclinação da ondulação chega na normal, senão a física sente uma
  // superfície plana onde a geometria mostra um monte.
  WaterVec2 probe{2.0f, 0.0f};
  WaterFieldSample sample{};
  AE_EXPECT_TRUE(with.sample({&probe, 1}, 0.0, {&sample, 1}), "sample");
  AE_EXPECT_TRUE(std::fabs(sample.normal.x) > 1e-3f, "the ripple tilts the normal");
}
