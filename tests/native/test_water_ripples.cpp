#include "harness.h"
#include "renderer/water_ripples.h"

#include <algorithm>
#include <cmath>

using namespace ae::renderer;

namespace {

WaterRippleSettings settings() {
  WaterRippleSettings value{};
  value.areaSize = 32.0f;
  value.resolution = 64;
  value.propagationSpeed = 4.0f;
  value.damping = 0.5f;
  value.maximumAmplitude = 2.0f;
  return value;
}

} // namespace

AE_TEST(WaterRipples_reject_configurations_the_solver_cannot_integrate) {
  AE_EXPECT_TRUE(validateWaterRipples(settings()), "reference settings");
  WaterRippleSettings broken = settings();
  broken.resolution = 4;
  AE_EXPECT_TRUE(!validateWaterRipples(broken), "resolution has a floor");
  broken = settings(); broken.areaSize = 0.0f;
  AE_EXPECT_TRUE(!validateWaterRipples(broken), "area must be positive");
  broken = settings(); broken.propagationSpeed = 0.0f;
  AE_EXPECT_TRUE(!validateWaterRipples(broken), "a wave that does not travel is not a wave");
  broken = settings(); broken.damping = -1.0f;
  AE_EXPECT_TRUE(!validateWaterRipples(broken), "negative damping would inject energy");
  broken = settings(); broken.maximumAmplitude = 0.0f;
  AE_EXPECT_TRUE(!validateWaterRipples(broken), "amplitude envelope must be positive");
  broken = settings(); broken.areaSize = std::nanf("");
  AE_EXPECT_TRUE(!validateWaterRipples(broken), "non finite area");
}

AE_TEST(WaterRipples_maximum_step_follows_the_courant_condition) {
  const auto reference = settings();
  const float cell = reference.areaSize / static_cast<float>(reference.resolution);
  const float maximum = waterRippleMaximumStep(reference);
  AE_EXPECT_TRUE(maximum > 0.0f, "a stable step exists");
  // Courant abaixo de 1/raiz(2), com margem: operar no limite exato deixa erro
  // de ponto flutuante bastar para atravessá-lo.
  const float courant = reference.propagationSpeed * maximum / cell;
  AE_EXPECT_TRUE(courant < 0.7071f, "stays under the two dimensional Courant limit");

  // Grade mais fina exige passo menor; velocidade maior também. Se essa relação
  // se inverter, o solver estará subdividindo pouco justamente onde precisa.
  WaterRippleSettings finer = reference;
  finer.resolution = reference.resolution * 2u;
  AE_EXPECT_TRUE(waterRippleMaximumStep(finer) < maximum, "finer grid needs smaller steps");
  WaterRippleSettings faster = reference;
  faster.propagationSpeed = reference.propagationSpeed * 2.0f;
  AE_EXPECT_TRUE(waterRippleMaximumStep(faster) < maximum, "faster waves need smaller steps");
}

AE_TEST(WaterRipples_impulse_creates_a_local_disturbance_that_travels_outward) {
  WaterRippleField field;
  AE_EXPECT_TRUE(field.initialize(settings()), "initialize");
  AE_EXPECT_TRUE(field.totalEnergy() == 0.0, "a fresh field is flat");

  field.addImpulse(0.0f, 0.0f, 2.0f, -0.5f);
  AE_EXPECT_TRUE(field.totalEnergy() > 0.0, "the impulse deforms the surface");
  // Sinal negativo afunda, que é o que um casco faz ao deslocar água.
  AE_EXPECT_TRUE(field.height(0.0f, 0.0f) < 0.0f, "a negative impulse pushes the surface down");

  // Longe do impulso a superfície ainda está parada; é a propagação que leva
  // a perturbação até lá.
  const float far = 10.0f;
  AE_EXPECT_TRUE(std::fabs(field.height(far, 0.0f)) < 1e-6f, "the disturbance starts local");
  // Tempo suficiente para a frente percorrer os dez metros: a 4 m/s são 2,5 s,
  // e um teste mais curto mediria só a paciência de quem o escreveu.
  for (int step = 0; step < 360; ++step) AE_EXPECT_TRUE(field.advance(1.0f / 120.0f), "advance");
  AE_EXPECT_TRUE(std::fabs(field.height(far, 0.0f)) > 1e-5f, "and travels outward");
}

AE_TEST(WaterRipples_energy_decays_instead_of_accumulating) {
  WaterRippleField field;
  auto configuration = settings();
  configuration.damping = 2.0f;
  AE_EXPECT_TRUE(field.initialize(configuration), "initialize");
  field.addImpulse(0.0f, 0.0f, 3.0f, 1.0f);
  const double injected = field.totalEnergy();

  for (int step = 0; step < 240; ++step) AE_EXPECT_TRUE(field.advance(1.0f / 120.0f), "advance");
  const double remaining = field.totalEnergy();
  // Sem amortecimento a energia injetada nunca sai e a área vira ruído
  // acumulado depois de alguns minutos.
  AE_EXPECT_TRUE(remaining < injected * 0.5, "the surface settles");
  AE_EXPECT_TRUE(std::isfinite(remaining), "and does so without diverging");
}

AE_TEST(WaterRipples_survive_a_step_far_longer_than_the_stability_limit) {
  // Um quadro longo é exatamente o que acontece quando o aparelho está sob
  // pressão, e é quando um solver explícito costuma explodir.
  WaterRippleField field;
  AE_EXPECT_TRUE(field.initialize(settings()), "initialize");
  field.addImpulse(0.0f, 0.0f, 2.0f, 1.0f);
  const float maximum = waterRippleMaximumStep(settings());
  AE_EXPECT_TRUE(field.advance(maximum * 40.0f), "a very long frame is accepted");
  AE_EXPECT_TRUE(std::isfinite(field.totalEnergy()), "and does not diverge");
  // Saturação seria um valor absurdo, não apenas não-finito.
  AE_EXPECT_TRUE(field.totalEnergy() < 1.0e6, "nor saturate");

  for (int step = 0; step < 200; ++step) AE_EXPECT_TRUE(field.advance(1.0f / 60.0f), "keep going");
  AE_EXPECT_TRUE(std::isfinite(field.totalEnergy()), "still finite afterwards");
}

AE_TEST(WaterRipples_borders_radiate_instead_of_echoing) {
  WaterRippleField field;
  auto configuration = settings();
  configuration.damping = 0.0f;  // isola a borda: só ela pode remover energia
  AE_EXPECT_TRUE(field.initialize(configuration), "initialize");
  field.addImpulse(0.0f, 0.0f, 1.5f, 1.0f);

  // A frente sai do centro, alcança a moldura a 16 m e, se ela refletisse,
  // voltaria por volta de 8 s — 32 m de ida e volta a 4 m/s. O eco no centro é
  // a medida direta da reflexão, e não confunde com o platô que a equação de
  // onda deixa para trás.
  float echo = 0.0f;
  for (int step = 1; step <= 1440; ++step) {
    AE_EXPECT_TRUE(field.advance(1.0f / 120.0f), "advance");
    if (step > 240) echo = std::max(echo, std::fabs(field.height(0.0f, 0.0f)));
  }
  // Uma parede rígida devolve por volta de 12% do impulso ao centro; a condição
  // de radiação devolve menos de 1%.
  AE_EXPECT_TRUE(echo < 0.02f, "the border radiates instead of echoing");
}

AE_TEST(WaterRipples_sample_outside_the_area_reads_flat_water) {
  WaterRippleField field;
  AE_EXPECT_TRUE(field.initialize(settings()), "initialize");
  field.addImpulse(0.0f, 0.0f, 3.0f, 1.0f);
  // Fora da área não existe simulação, e devolver lixo ali colocaria degrau na
  // superfície exatamente na fronteira entre o espectro e a ondulação.
  AE_EXPECT_EQ(field.height(1000.0f, 0.0f), 0.0f, "far outside is flat");
  AE_EXPECT_EQ(field.height(0.0f, -1000.0f), 0.0f, "on the other axis too");
  float slopeX = 1.0f, slopeZ = 1.0f;
  field.slope(1000.0f, 1000.0f, slopeX, slopeZ);
  AE_EXPECT_EQ(slopeX, 0.0f, "and carries no slope");
  AE_EXPECT_EQ(slopeZ, 0.0f, "on either axis");
}

AE_TEST(WaterRipples_fade_to_the_local_grid_edge_without_a_normal_seam) {
  WaterRippleField field;
  AE_EXPECT_TRUE(field.initialize(settings()), "initialize");
  // A perturbação toca a moldura para tornar uma descontinuidade observável.
  field.addImpulse(-15.5f, 0.0f, 2.0f, 1.0f);
  const float boundary = -settings().areaSize * 0.5f;
  AE_EXPECT_EQ(field.height(boundary, 0.0f), 0.0f, "the exact boundary is flat");
  AE_EXPECT_EQ(field.height(boundary - 0.001f, 0.0f), 0.0f, "outside remains flat");
  const float near = field.height(boundary + 0.01f, 0.0f);
  const float inward = field.height(boundary + 0.50f, 0.0f);
  AE_EXPECT_TRUE(std::fabs(near) < std::fabs(inward) * 0.01f,
                 "the edge approaches zero continuously");
  float slopeX = 0.0f, slopeZ = 0.0f;
  field.slope(boundary, 0.0f, slopeX, slopeZ);
  AE_EXPECT_TRUE(std::isfinite(slopeX) && std::isfinite(slopeZ), "edge normal stays finite");
}

AE_TEST(WaterRipples_copy_snapshot_is_exact_and_does_not_expose_ownership) {
  WaterRippleField field;
  AE_EXPECT_TRUE(field.initialize(settings()), "initialize");
  field.addImpulse(0.0f, 0.0f, 2.0f, 0.5f);
  std::vector<float> snapshot(static_cast<ae::usize>(settings().resolution) * settings().resolution);
  AE_EXPECT_TRUE(field.copyHeightsTo(snapshot), "complete destination accepted");
  AE_EXPECT_TRUE(std::any_of(snapshot.begin(), snapshot.end(), [](float value) { return value != 0.0f; }),
                 "snapshot contains the disturbance");
  std::vector<float> tooSmall(snapshot.size() - 1u);
  AE_EXPECT_TRUE(!field.copyHeightsTo(tooSmall), "short destination rejected");
  snapshot.assign(snapshot.size(), 0.0f);
  AE_EXPECT_TRUE(field.totalEnergy() > 0.0, "caller changes do not mutate the field");
}

AE_TEST(WaterRipples_slope_points_away_from_a_bump) {
  WaterRippleField field;
  AE_EXPECT_TRUE(field.initialize(settings()), "initialize");
  field.addImpulse(0.0f, 0.0f, 4.0f, 1.0f);
  float slopeX = 0.0f, slopeZ = 0.0f;
  // À direita de um monte a superfície desce com x; à esquerda, sobe.
  field.slope(2.0f, 0.0f, slopeX, slopeZ);
  AE_EXPECT_TRUE(slopeX < 0.0f, "the surface falls away on the far side");
  field.slope(-2.0f, 0.0f, slopeX, slopeZ);
  AE_EXPECT_TRUE(slopeX > 0.0f, "and rises on the near side");
  // No topo exato a inclinação passa por zero.
  field.slope(0.0f, 0.0f, slopeX, slopeZ);
  AE_EXPECT_TRUE(std::fabs(slopeX) < 1e-3f && std::fabs(slopeZ) < 1e-3f, "the crest is level");
}

AE_TEST(WaterRipples_recentring_drops_the_history_instead_of_dragging_it) {
  WaterRippleField field;
  AE_EXPECT_TRUE(field.initialize(settings()), "initialize");
  field.addImpulse(0.0f, 0.0f, 3.0f, 1.0f);
  AE_EXPECT_TRUE(field.totalEnergy() > 0.0, "there is something to drag");

  field.recenter(500.0f, -200.0f);
  // Arrastar o conteúdo junto prenderia a fase da onda à câmera, e o padrão
  // inteiro deslizaria com o observador em vez de ficar na água.
  AE_EXPECT_EQ(field.totalEnergy(), 0.0, "the history is dropped");
  AE_EXPECT_EQ(field.centreX(), 500.0f, "the centre moved");

  // E a área nova responde a impulsos nas coordenadas novas.
  field.addImpulse(500.0f, -200.0f, 2.0f, 1.0f);
  AE_EXPECT_TRUE(field.height(500.0f, -200.0f) > 0.0f, "the moved area is live");
}

AE_TEST(WaterRipples_refuse_garbage_without_corrupting_the_surface) {
  WaterRippleField field;
  AE_EXPECT_TRUE(!field.advance(1.0f / 60.0f), "an uninitialised field advances nothing");
  AE_EXPECT_TRUE(field.initialize(settings()), "initialize");
  AE_EXPECT_TRUE(!field.advance(std::nanf("")), "non finite step is refused");
  AE_EXPECT_TRUE(!field.advance(-1.0f), "negative step is refused");
  field.addImpulse(std::nanf(""), 0.0f, 1.0f, 1.0f);
  field.addImpulse(0.0f, 0.0f, -1.0f, 1.0f);
  field.addImpulse(0.0f, 0.0f, 1.0f, std::nanf(""));
  AE_EXPECT_EQ(field.totalEnergy(), 0.0, "no bad impulse reached the grid");
  AE_EXPECT_TRUE(field.advance(1.0f / 60.0f), "and the field still works");
}

AE_TEST(WaterRipples_bound_overlapping_sources_before_they_become_geometry_artifacts) {
  WaterRippleField field;
  auto configuration = settings();
  configuration.maximumAmplitude = 0.75f;
  AE_EXPECT_TRUE(field.initialize(configuration), "initialize");
  for (int impulse = 0; impulse < 100; ++impulse)
    field.addImpulse(0.0f, 0.0f, 3.0f, 10.0f);
  AE_EXPECT_TRUE(field.height(0.0f, 0.0f) <= configuration.maximumAmplitude,
                 "overlapping impulses obey the authored envelope");
  for (int step = 0; step < 120; ++step)
    AE_EXPECT_TRUE(field.advance(1.0f / 120.0f), "advance saturated field");
  std::vector<float> snapshot(static_cast<ae::usize>(configuration.resolution) *
                              configuration.resolution);
  AE_EXPECT_TRUE(field.copyHeightsTo(snapshot), "snapshot");
  AE_EXPECT_TRUE(std::all_of(snapshot.begin(), snapshot.end(), [&](float value) {
                   return std::isfinite(value) && std::fabs(value) <= configuration.maximumAmplitude;
                 }), "every cell remains finite and inside the envelope");
}

AE_TEST(WaterWake_emission_is_spatial_and_frame_rate_independent) {
  auto configuration=settings();
  configuration.areaSize=128;
  configuration.resolution=128;
  WaterRippleField sixty,oneTwenty;
  AE_EXPECT_TRUE(sixty.initialize(configuration) && oneTwenty.initialize(configuration),"fields");
  WaterWakeSettings wake{}; wake.spacing=2; wake.strength=1;
  WaterWakeEmitter a,b;
  AE_EXPECT_TRUE(a.configure(wake) && b.configure(wake),"wake configuration");
  WaterWakeInput input{};
  input.forward={0,1}; input.velocity={0,10}; input.halfLength=10; input.halfWidth=2;
  input.submergedFraction=.75f;
  for(int frame=0;frame<60;++frame) {
    input.position.y=10.0f*(frame+1)/60.0f; input.deltaSeconds=1.0f/60.0f;
    AE_EXPECT_TRUE(a.update(input,sixty),"60 Hz emission");
  }
  for(int frame=0;frame<120;++frame) {
    input.position.y=10.0f*(frame+1)/120.0f; input.deltaSeconds=1.0f/120.0f;
    AE_EXPECT_TRUE(b.update(input,oneTwenty),"120 Hz emission");
  }
  AE_EXPECT_EQ(a.emittedSections(),5u,"five spatial sections at 60 Hz");
  AE_EXPECT_EQ(b.emittedSections(),5u,"same spatial sections at 120 Hz");
  AE_EXPECT_TRUE(sixty.totalEnergy()>0 && oneTwenty.totalEnergy()>0,"wake deforms both fields");
}

AE_TEST(WaterWake_validation_is_transactional_and_dry_bodies_emit_nothing) {
  WaterWakeEmitter emitter;
  WaterWakeSettings good{};
  AE_EXPECT_TRUE(emitter.configure(good),"valid settings");
  auto invalid=good; invalid.spacing=0;
  AE_EXPECT_TRUE(!emitter.configure(invalid),"invalid edit rejected");
  AE_EXPECT_TRUE(emitter.settings().spacing==good.spacing,"old settings preserved");
  WaterRippleField field;
  AE_EXPECT_TRUE(field.initialize(settings()),"field");
  WaterWakeInput input{};
  input.velocity={0,20}; input.deltaSeconds=1; input.submergedFraction=0;
  AE_EXPECT_TRUE(emitter.update(input,field),"dry update is valid no-op");
  AE_EXPECT_EQ(field.totalEnergy(),0.0,"dry hull cannot leave a wake");
}
