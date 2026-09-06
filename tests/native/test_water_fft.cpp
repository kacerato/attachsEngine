#include "harness.h"
#include "renderer/water_fft.h"
#include "renderer/water_cascades.h"
#include <cmath>
#include <complex>
#include <vector>

using namespace ae::renderer;

AE_TEST(Water_spectral_live_controls_validate_and_clock_preserves_phase) {
  WaterSpectralControls controls;
  AE_EXPECT_TRUE(validateWaterSpectralControls(controls),"default controls");
  controls.displacement=4;
  AE_EXPECT_TRUE(!validateWaterSpectralControls(controls),"reject gain outside culling envelope");
  controls.displacement=1; controls.foam.growth=-1;
  AE_EXPECT_TRUE(!validateWaterSpectralControls(controls),"reject invalid foam rates");
  WaterSpectralClock clock;
  AE_EXPECT_TRUE(clock.advance(100,1)==0,"arbitrary wall clock starts at phase zero");
  AE_EXPECT_TRUE(clock.advance(101,1)==1,"normal speed");
  AE_EXPECT_TRUE(clock.advance(102,2)==3,"speed change integrates elapsed time, not absolute time");
  AE_EXPECT_TRUE(clock.advance(112,0)==3,"paused phase");
  AE_EXPECT_TRUE(clock.advance(113,1)==4,"resume without jump");
  AE_EXPECT_TRUE(clock.advance(10,1)==4,"wall-clock rewind preserves phase");
  AE_EXPECT_TRUE(clock.advance(11,1)==5,"continue after rewind");
}

AE_TEST(Water_default_cascades_fit_declared_mobile_buffer_budget) {
  const auto settings=defaultWaterCascadeSettings();
  AE_EXPECT_TRUE(validateWaterCascades(settings,4ull*1024*1024)==WaterCascadeError::None,"default authoring valid");
  AE_EXPECT_EQ(waterCascadeGpuBufferBytes(settings),3342336ull,"three 128 cascades including foam and slope textures");
  for(const auto &cascade:settings)
    AE_EXPECT_TRUE(cascade.spectrum.minimumWavelength>=2*cascade.spectrum.patchLength/cascade.spectrum.resolution,"band respects spatial Nyquist");
}

AE_TEST(Water_spectrum_authoring_updates_every_band_without_changing_structure) {
  const auto base=defaultWaterCascadeSettings();
  WaterSpectrumAuthoringSettings authored{};
  authored.windSpeed=18;
  authored.fetch=450000;
  authored.depth=73;
  authored.swell=1.4f;
  authored.spread=.35f;
  authored.shortWaveDamping=.22f;
  authored.crossSwell={11,1.1f,180000,1.2f,.25f,.4f};
  authored.cascadeDisplacement={1.5f,.8f,1.9f,1};
  authored.cascadeChoppiness={2.2f,1.3f,.6f,1};
  std::vector<WaterCascadeSettings> result;
  AE_EXPECT_TRUE(authorWaterCascades(base,authored,result),"author valid global spectrum");
  AE_EXPECT_EQ(result.size(),base.size(),"cascade count is structural and preserved");
  for(ae::usize index=0;index<result.size();++index) {
    AE_EXPECT_EQ(result[index].spectrum.resolution,base[index].spectrum.resolution,
                 "live authoring preserves allocated resolution");
    AE_EXPECT_TRUE(result[index].spectrum.patchLength==base[index].spectrum.patchLength,
                   "live authoring preserves domain");
    AE_EXPECT_TRUE(result[index].spectrum.windSpeed==18 &&
                   result[index].spectrum.fetch==450000 &&
                   result[index].spectrum.crossSwell.weight==.4f,
                   "every band consumes the same physical sea state");
    AE_EXPECT_TRUE(result[index].displacementScale==authored.cascadeDisplacement[index] &&
                   result[index].choppiness==authored.cascadeChoppiness[index],
                   "per-band axes remain independent");
  }
  const auto previous=result;
  authored.fetch=0;
  AE_EXPECT_TRUE(!authorWaterCascades(base,authored,result),"reject invalid physical authoring");
  AE_EXPECT_TRUE(result[1].spectrum.fetch==previous[1].spectrum.fetch,
                 "rejected edit preserves the complete previous set");
}

AE_TEST(Water_cascades_partition_shared_boundary_without_double_energy) {
  std::array<WaterCascadeSettings,2> settings{};
  for(auto &c:settings) { c.spectrum.resolution=32; c.spectrum.patchLength=256; }
  settings[0].spectrum.minimumWavelength=1; settings[0].spectrum.maximumWavelength=32;
  settings[1].spectrum.minimumWavelength=32; settings[1].spectrum.maximumWavelength=256;
  std::vector<WaterCascadeSpectrum> cascades;
  AE_EXPECT_TRUE(generateWaterCascades(settings,139264,cascades),"exact budget");
  AE_EXPECT_TRUE(cascades[0].amplitudes[8]==std::complex<float>{},"short cascade excludes shared 32m wave");
  AE_EXPECT_TRUE(std::norm(cascades[1].amplitudes[8])>0,"long cascade owns shared 32m wave");
  WaterSpectrumSettings full=settings[0].spectrum; full.maximumWavelength=256;
  std::vector<std::complex<float>> combined;
  AE_EXPECT_TRUE(generateWaterSpectrum(full,combined),"unpartitioned reference");
  for(ae::usize i=0;i<combined.size();++i)
    AE_EXPECT_TRUE(std::abs(cascades[0].amplitudes[i]+cascades[1].amplitudes[i]-combined[i])<.000001f,"partition preserves realization and energy");
  AE_EXPECT_TRUE(!generateWaterCascades(settings,139263,cascades),"budget enforced");
  AE_EXPECT_EQ(cascades.size(),2u,"failure preserves previous data");
  settings[1].spectrum.minimumWavelength=31;
  AE_EXPECT_TRUE(validateWaterCascades(settings,139264)==WaterCascadeError::OverlappingBands,"overlap rejected");
}

AE_TEST(Water_foam_evolution_is_frame_rate_independent) {
  WaterFoamSettings settings;
  const float single=evolveWaterFoam(0,.2f,1,settings);
  for(int hz:{30,60,120}) {
    float foam=0;
    for(int i=0;i<hz;++i) foam=evolveWaterFoam(foam,.2f,1.0f/hz,settings);
    AE_EXPECT_TRUE(std::abs(foam-single)<.00001f,"same compression produces same foam at all rates");
  }
  const float faded=evolveWaterFoam(single,1,2,settings);
  AE_EXPECT_TRUE(std::abs(faded-single*std::exp(-1.0f))<.000001f,"uncompressed water decays exponentially");
  AE_EXPECT_TRUE(evolveWaterFoam(.7f,1,0,settings)==.7f,"paused history stable");
  settings.growth=0; settings.decay=0;
  AE_EXPECT_TRUE(evolveWaterFoam(.7f,0,100,settings)==.7f,"zero rates preserve history");
  settings.decay=-1;
  AE_EXPECT_TRUE(evolveWaterFoam(.7f,0,100,settings)==.7f,"invalid rates preserve history");
}

AE_TEST(Water_spectral_derivatives_match_oblique_wave) {
  WaterSpectralField field;
  using Channel=WaterSpectralField::Channel;
  std::vector<std::complex<float>> initial(64);
  initial[9]={32,0};
  AE_EXPECT_TRUE(field.initialize(8,32,20,initial),"oblique wave");
  const double k=6.2831853071795864769/32;
  const double magnitude=k*std::sqrt(2.0);
  const double omega=std::sqrt(9.81*magnitude*std::tanh(magnitude*20));
  for(double time:{0.0,2.0,1000.0}) {
    AE_EXPECT_TRUE(field.update(time),"evolution");
    for(int y=0;y<8;++y) for(int x=0;x<8;++x) {
      const int index=y*8+x;
      const double phase=6.2831853071795864769*(x+y)/8+omega*time;
      const double displacement=-std::sin(phase)/std::sqrt(2.0);
      const double slope=-k*std::sin(phase);
      const double derivative=-k*std::cos(phase)/std::sqrt(2.0);
      for(auto c:{Channel::DisplacementX,Channel::DisplacementZ})
        AE_EXPECT_TRUE(std::abs(field.channel(c)[index].real()-displacement)<.00001,"horizontal displacement");
      for(auto c:{Channel::SlopeX,Channel::SlopeZ})
        AE_EXPECT_TRUE(std::abs(field.channel(c)[index].real()-slope)<.00001,"analytic slope");
      for(auto c:{Channel::DisplacementXX,Channel::DisplacementXZ,Channel::DisplacementZZ})
        AE_EXPECT_TRUE(std::abs(field.channel(c)[index].real()-derivative)<.00001,"horizontal derivative");
      AE_EXPECT_TRUE(std::abs(field.jacobian(index,2)-(1+4*derivative))<.00001,"full mixed Jacobian");
      AE_EXPECT_TRUE(field.jacobian(index,0)==1,"no compression without choppiness");
    }
  }
}

AE_TEST(Water_wind_spectrum_is_seeded_and_preserves_invalid_output) {
  WaterSpectrumSettings settings;
  settings.resolution=32;
  std::vector<std::complex<float>> a,b;
  AE_EXPECT_TRUE(generateWaterSpectrum(settings,a),"initial spectrum");
  AE_EXPECT_TRUE(generateWaterSpectrum(settings,b),"repeat");
  AE_EXPECT_TRUE(a==b,"same seed is reproducible");
  settings.seed++;
  AE_EXPECT_TRUE(generateWaterSpectrum(settings,b),"new seed");
  AE_EXPECT_TRUE(a!=b,"seed changes realization");
  const auto previous=b;
  settings.depth=-1;
  AE_EXPECT_TRUE(!generateWaterSpectrum(settings,b),"invalid depth");
  AE_EXPECT_TRUE(b==previous,"failed authoring preserves output");
  settings.depth=20; settings.windSpeed=0;
  AE_EXPECT_TRUE(generateWaterSpectrum(settings,b),"calm water");
  for(auto value:b) AE_EXPECT_TRUE(value==std::complex<float>{},"zero wind is flat");
}

AE_TEST(Water_wind_spectrum_filters_bands_and_evolves_real_surface) {
  WaterSpectrumSettings settings;
  settings.resolution=32; settings.minimumWavelength=20; settings.maximumWavelength=80;
  std::vector<std::complex<float>> a;
  AE_EXPECT_TRUE(generateWaterSpectrum(settings,a),"band spectrum");
  double energy=0;
  for(int y=0;y<32;++y) for(int x=0;x<32;++x) {
    const int kx=x<16?x:x-32, kz=y<16?y:y-32;
    const double length=std::hypot(kx,kz);
    if(length==0 || x==16 || y==16 || settings.patchLength/length<20 || settings.patchLength/length>80)
      AE_EXPECT_TRUE(a[y*32+x]==std::complex<float>{},"excluded modes are zero");
    energy+=std::norm(a[y*32+x]);
  }
  AE_EXPECT_TRUE(energy>0,"retains wave energy");
  WaterSpectralField field;
  AE_EXPECT_TRUE(field.initialize(settings),"settings overload");
  for(double time:{0.0,10.0,100000.0}) {
    AE_EXPECT_TRUE(field.update(time),"wind evolution");
    double mean=0;
    for(auto value:field.heights()) {
      AE_EXPECT_TRUE(std::isfinite(value.real()) && std::abs(value.imag())<.00001f,"finite real surface");
      mean+=value.real();
    }
    AE_EXPECT_TRUE(std::abs(mean/1024)<.000001,"no water-level drift");
  }
}

AE_TEST(Water_wind_direction_controls_energy_distribution) {
  WaterSpectrumSettings settings; settings.resolution=32; settings.spread=0;
  std::vector<std::complex<float>> forward,backward;
  AE_EXPECT_TRUE(generateWaterSpectrum(settings,forward),"forward wind");
  settings.windDirection=3.14159265358979323846f;
  AE_EXPECT_TRUE(generateWaterSpectrum(settings,backward),"opposite wind");
  AE_EXPECT_TRUE(std::norm(forward[2])>100*std::norm(backward[2]),"positive X energy follows wind");
  AE_EXPECT_TRUE(std::norm(backward[30])>100*std::norm(forward[30]),"negative X energy follows wind");
}

AE_TEST(Water_spectral_evolution_matches_single_deep_water_wave) {
  WaterSpectralField field;
  std::vector<std::complex<float>> initial(64);
  initial[1]={32,0}; // conjugate pair yields unit amplitude after 1/N squared
  AE_EXPECT_TRUE(field.initialize(8,32,20,initial),"spectral field");
  const double k=6.2831853071795864769/32;
  const double omega=std::sqrt(9.81*k*std::tanh(k*20));
  for(double time:{0.0,1.0,12345.0}) {
    AE_EXPECT_TRUE(field.update(time),"evolution");
    auto height=field.heights();
    for(int y=0;y<8;++y) for(int x=0;x<8;++x) {
      const double expected=std::cos(6.2831853071795864769*x/8+omega*time);
      AE_EXPECT_TRUE(std::abs(height[y*8+x].real()-expected)<.000002,"dispersion and axis order");
      AE_EXPECT_TRUE(std::abs(height[y*8+x].imag())<.000002,"Hermitian pairing gives real field");
    }
  }
}

AE_TEST(Water_fft_matches_independent_direct_transform) {
  WaterFftPlan plan;
  AE_EXPECT_TRUE(plan.initialize(16),"radix two plan");
  std::vector<std::complex<float>> input(16);
  for(int i=0;i<16;++i) input[i]={std::sin(i*.7f),std::cos(i*.3f)};
  auto output=input;
  AE_EXPECT_TRUE(plan.transform(output,false),"forward");
  for(int k=0;k<16;++k) {
    std::complex<double> expected{};
    for(int j=0;j<16;++j) {
      const double angle=-6.2831853071795864769*j*k/16;
      expected+=std::complex<double>(input[j])*std::complex<double>(std::cos(angle),std::sin(angle));
    }
    AE_EXPECT_TRUE(std::abs(std::complex<double>(output[k])-expected)<.00002,"independent DFT");
  }
  AE_EXPECT_TRUE(plan.transform(output,true),"inverse");
  for(int i=0;i<16;++i) AE_EXPECT_TRUE(std::abs(output[i]-input[i])<.000002f,"normalized roundtrip");
}

AE_TEST(Water_fft_2d_impulse_and_roundtrip) {
  WaterFftPlan plan;
  AE_EXPECT_TRUE(plan.initialize(128),"production minimum resolution");
  std::vector<std::complex<float>> data(128*128);
  data[0]={1,0};
  AE_EXPECT_TRUE(plan.transform2D(data,false),"2D forward");
  for(const auto value:data) AE_EXPECT_TRUE(std::abs(value-std::complex<float>(1,0))<.000001f,"flat impulse spectrum");
  AE_EXPECT_TRUE(plan.transform2D(data,true),"2D inverse");
  AE_EXPECT_TRUE(std::abs(data[0]-std::complex<float>(1,0))<.000001f,"normalization N squared");
  for(ae::usize i=1;i<data.size();++i) AE_EXPECT_TRUE(std::abs(data[i])<.000001f,"no leakage");
}

AE_TEST(Water_fft_rejects_bad_sizes_without_losing_plan) {
  WaterFftPlan plan;
  std::vector<std::complex<float>> values(8);
  AE_EXPECT_TRUE(!plan.transform(values,false),"uninitialized");
  AE_EXPECT_TRUE(plan.initialize(8),"valid plan");
  for(ae::u32 invalid:{0u,1u,7u,4096u})
    AE_EXPECT_TRUE(!plan.initialize(invalid),"invalid resolution");
  AE_EXPECT_EQ(plan.size(),8u,"previous plan preserved");
  AE_EXPECT_TRUE(plan.transform(values,false),"still usable");
  AE_EXPECT_TRUE(!plan.transform2D(values,true),"reject mismatched field");
}

namespace {

// Energia total do campo. Parseval: a soma dos módulos ao quadrado dos modos é
// proporcional à variância da superfície, que é o que "quanto mar existe" quer
// dizer fisicamente.
double spectralEnergy(const std::vector<std::complex<float>> &amplitudes) {
  double total = 0.0;
  for (const auto &mode : amplitudes) {
    total += static_cast<double>(mode.real()) * mode.real() +
             static_cast<double>(mode.imag()) * mode.imag();
  }
  return total;
}

// Energia contida num setor angular em torno de uma direção, para responder
// "de onde vem a onda" sem depender de inspeção visual.
double energyTowards(const std::vector<std::complex<float>> &amplitudes, ae::u32 resolution,
                     double directionRadians, double halfWidthRadians) {
  double total = 0.0;
  for (ae::u32 y = 0; y < resolution; ++y) {
    for (ae::u32 x = 0; x < resolution; ++x) {
      const int kx = x < resolution / 2 ? static_cast<int>(x)
                                        : static_cast<int>(x) - static_cast<int>(resolution);
      const int kz = y < resolution / 2 ? static_cast<int>(y)
                                        : static_cast<int>(y) - static_cast<int>(resolution);
      if (kx == 0 && kz == 0) continue;
      double delta = std::atan2(static_cast<double>(kz), static_cast<double>(kx)) - directionRadians;
      while (delta > 3.14159265358979323846) delta -= 2.0 * 3.14159265358979323846;
      while (delta < -3.14159265358979323846) delta += 2.0 * 3.14159265358979323846;
      if (std::fabs(delta) > halfWidthRadians) continue;
      const auto &mode = amplitudes[y * resolution + x];
      total += static_cast<double>(mode.real()) * mode.real() +
               static_cast<double>(mode.imag()) * mode.imag();
    }
  }
  return total;
}

} // namespace

AE_TEST(Water_cross_swell_off_reproduces_the_single_train_bit_for_bit) {
  // A garantia que sustenta as outras: nenhuma cena existente pode mudar por
  // este trabalho ter entrado. O padrão desliga o segundo trem, e desligado
  // significa idêntico, não parecido.
  WaterSpectrumSettings settings{};
  settings.resolution = 64;
  std::vector<std::complex<float>> baseline, unchanged;
  AE_EXPECT_TRUE(generateWaterSpectrum(settings, baseline), "baseline spectrum");

  settings.crossSwell = WaterSwellSystem{};  // explicitamente o padrão
  AE_EXPECT_TRUE(generateWaterSpectrum(settings, unchanged), "explicit default spectrum");
  AE_EXPECT_EQ(baseline.size(), unchanged.size(), "same mode count");
  for (ae::usize index = 0; index < baseline.size(); ++index) {
    AE_EXPECT_EQ(baseline[index].real(), unchanged[index].real(), "identical real part");
    AE_EXPECT_EQ(baseline[index].imag(), unchanged[index].imag(), "identical imaginary part");
  }

  // Peso zero também desliga, mesmo com vento configurado: é como um autor
  // silencia o sistema sem perder os parâmetros que ajustou.
  settings.crossSwell.windSpeed = 12.0f;
  settings.crossSwell.weight = 0.0f;
  std::vector<std::complex<float>> silenced;
  AE_EXPECT_TRUE(generateWaterSpectrum(settings, silenced), "silenced spectrum");
  for (ae::usize index = 0; index < baseline.size(); ++index) {
    AE_EXPECT_EQ(baseline[index].real(), silenced[index].real(), "zero weight changes nothing");
  }
}

AE_TEST(Water_cross_swell_adds_energy_from_its_own_direction) {
  WaterSpectrumSettings settings{};
  settings.resolution = 64;
  settings.windDirection = 0.0f;          // trem principal ao longo de +X
  std::vector<std::complex<float>> single;
  AE_EXPECT_TRUE(generateWaterSpectrum(settings, single), "single train");

  settings.crossSwell.windSpeed = 10.0f;
  settings.crossSwell.directionRadians = 1.5707963f;  // cruzado a 90 graus
  settings.crossSwell.weight = 1.0f;
  std::vector<std::complex<float>> crossed;
  AE_EXPECT_TRUE(generateWaterSpectrum(settings, crossed), "crossed sea");

  // Mar cruzado tem mais energia que qualquer um dos trens sozinho. Normalizar
  // para manter a energia constante faria ligar o segundo trem baixar a altura
  // significativa, que é o oposto do que acontece no mar.
  AE_EXPECT_TRUE(spectralEnergy(crossed) > spectralEnergy(single) * 1.05,
                 "the crossed sea carries more energy");

  // E a energia nova chega da direção do segundo trem, não espalhada.
  const double quarter = 0.7853981634;  // 45 graus de meia largura
  const double alongCross = energyTowards(crossed, settings.resolution, 1.5707963, quarter) -
                            energyTowards(single, settings.resolution, 1.5707963, quarter);
  const double alongMain = energyTowards(crossed, settings.resolution, 0.0, quarter) -
                           energyTowards(single, settings.resolution, 0.0, quarter);
  AE_EXPECT_TRUE(alongCross > 0.0, "energy grows towards the cross swell");
  AE_EXPECT_TRUE(alongCross > alongMain * 4.0, "and grows far more there than along the main train");
}

AE_TEST(Water_cross_swell_alone_still_makes_a_sea) {
  // Vento local parado com swell chegando de longe é situação real, e é o
  // caso em que um espectro de trem único devolve mar de vidro.
  WaterSpectrumSettings settings{};
  settings.resolution = 64;
  settings.windSpeed = 0.0f;
  settings.crossSwell.windSpeed = 9.0f;
  settings.crossSwell.directionRadians = 2.0f;
  std::vector<std::complex<float>> swellOnly;
  AE_EXPECT_TRUE(generateWaterSpectrum(settings, swellOnly), "swell without local wind");
  AE_EXPECT_TRUE(spectralEnergy(swellOnly) > 0.0, "distant swell still raises a sea");

  settings.crossSwell.windSpeed = 0.0f;
  std::vector<std::complex<float>> flat;
  AE_EXPECT_TRUE(generateWaterSpectrum(settings, flat), "both trains calm");
  AE_EXPECT_EQ(spectralEnergy(flat), 0.0, "no wind anywhere is a flat sea");
}

AE_TEST(Water_cross_swell_obeys_the_same_limits_as_the_main_train) {
  WaterSpectrumSettings settings{};
  settings.resolution = 32;
  AE_EXPECT_TRUE(validateWaterSpectrum(settings), "defaults are valid");

  // Mesmo modelo físico, mesmas faixas: aceitar 200 m/s só no segundo trem
  // seria uma porta lateral para a mesma instabilidade numérica.
  settings.crossSwell.windSpeed = 200.0f;
  AE_EXPECT_TRUE(!validateWaterSpectrum(settings), "wind speed is bounded");
  settings.crossSwell = WaterSwellSystem{};
  settings.crossSwell.weight = 1.5f;
  AE_EXPECT_TRUE(!validateWaterSpectrum(settings), "weight is a fraction");
  settings.crossSwell = WaterSwellSystem{};
  settings.crossSwell.spread = -0.1f;
  AE_EXPECT_TRUE(!validateWaterSpectrum(settings), "spread is bounded");
  settings.crossSwell = WaterSwellSystem{};
  settings.crossSwell.fetch = 0.0f;
  AE_EXPECT_TRUE(!validateWaterSpectrum(settings), "fetch has a floor");
}
