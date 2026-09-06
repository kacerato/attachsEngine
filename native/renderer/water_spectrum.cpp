#include "renderer/water_fft.h"
#include <cmath>
#include <algorithm>

namespace ae::renderer {
namespace {
constexpr double Pi=3.14159265358979323846;
constexpr double Gravity=9.81;

// Explicit integer generator avoids std::normal_distribution implementation
// differences. Half-bin samples exclude both zero and one for Box-Muller.
double uniform(u32 &state) {
  state=1664525u*state+1013904223u;
  return (static_cast<double>(state)+0.5)/4294967296.0;
}

// Um trem de ondas, já com o pico e a escala derivados do seu próprio vento e
// pista. Existir como dado é o que permite avaliar dois deles no mesmo laço.
struct WaveTrain final {
  double peak=0,alpha=0,wind=0,swell=0,spread=0,direction=0,weight=0;
};

WaveTrain makeTrain(double wind,double fetch,double swell,double spread,
                    double direction,double weight) {
  WaveTrain train{};
  if(wind<=0 || weight<=0) return train;  // peso zero: o laço pula o trem
  train.peak=22*std::cbrt(Gravity*Gravity/(wind*fetch));
  train.alpha=.076*std::pow(wind*wind/(Gravity*fetch),.22);
  train.wind=wind; train.swell=swell; train.spread=spread;
  train.direction=direction; train.weight=weight;
  return train;
}

// Densidade direcional de um trem, em (k, ângulo). Separada do laço para que os
// dois sistemas passem exatamente pela mesma física — um swell cruzado com
// fórmula ligeiramente diferente do trem principal seria um bug invisível.
double trainDensity(const WaveTrain &train,double omega,double derivative,
                    double k,double angleFromX,double depth) {
  if(train.weight<=0) return 0;
  const double sigma=omega<=train.peak?.07:.09;
  const double relative=(omega-train.peak)/(sigma*train.peak);
  const double enhancement=std::pow(3.3,std::exp(-.5*relative*relative));
  const double ratio=train.peak/omega;
  const double jonswap=train.alpha*Gravity*Gravity/std::pow(omega,5)*
    std::exp(-1.25*std::pow(ratio,4))*enhancement;
  const double wh=std::min(omega*std::sqrt(depth/Gravity),2.0);
  const double attenuation=wh<=1?.5*wh*wh:1-.5*(2-wh)*(2-wh);
  const double p=omega/train.peak;
  const double exponent=omega<=train.peak?6.97*std::pow(p,4.06):
    9.77*std::pow(p,-2.33-1.45*(train.wind*train.peak/Gravity-1.17));
  const double shape=std::min(exponent+16*std::tanh(train.peak/omega)*train.swell*train.swell,10000.0);
  // Exact normalization of cos(theta/2)^(2s), evaluated in log space.
  const double normalization=std::exp(std::lgamma(shape+1)-std::lgamma(shape+.5))/(2*std::sqrt(Pi));
  const double angle=angleFromX-train.direction;
  const double directional=normalization*std::pow(std::abs(std::cos(angle*.5)),2*shape);
  const double spreading=(1-train.spread)*directional+train.spread/(2*Pi);
  return train.weight*jonswap*attenuation*spreading*derivative/k;
}
}

bool validateWaterSpectrum(const WaterSpectrumSettings &s) noexcept {
  const float values[]={s.patchLength,s.depth,s.windSpeed,s.windDirection,s.fetch,
    s.swell,s.spread,s.shortWaveDamping,s.minimumWavelength,s.maximumWavelength,
    s.crossSwell.windSpeed,s.crossSwell.directionRadians,s.crossSwell.fetch,
    s.crossSwell.swell,s.crossSwell.spread,s.crossSwell.weight};
  for(float value:values) if(!std::isfinite(value)) return false;
  // O segundo trem usa as mesmas faixas do primeiro: é o mesmo modelo físico,
  // e aceitar um vento de 200 m/s só nele seria uma porta lateral para a mesma
  // instabilidade numérica.
  const auto &cross=s.crossSwell;
  if(cross.windSpeed<0 || cross.windSpeed>100 || cross.fetch<1 || cross.fetch>10000000 ||
     cross.swell<0 || cross.swell>2 || cross.spread<0 || cross.spread>1 ||
     cross.weight<0 || cross.weight>1) return false;
  return s.resolution>=8 && s.resolution<=2048 && (s.resolution&(s.resolution-1))==0 &&
    s.patchLength>=1 && s.patchLength<=100000 && s.depth>=.1f && s.depth<=10000 &&
    s.windSpeed>=0 && s.windSpeed<=100 && s.fetch>=1 && s.fetch<=10000000 &&
    s.swell>=0 && s.swell<=2 && s.spread>=0 && s.spread<=1 &&
    s.shortWaveDamping>=0 && s.shortWaveDamping<=100 && s.minimumWavelength>=0 &&
    s.maximumWavelength>s.minimumWavelength;
}

bool generateWaterSpectrum(const WaterSpectrumSettings &s,
                          std::vector<std::complex<float>> &amplitudes) {
  if(!validateWaterSpectrum(s)) return false;
  const u32 n=s.resolution;
  std::vector<std::complex<float>> result(static_cast<usize>(n)*n);
  // Mar parado é os dois trens em zero. Com o cruzado ligado e o principal
  // parado ainda existe swell chegando de longe, que é situação real.
  if(s.windSpeed==0 && s.crossSwell.windSpeed==0) { amplitudes=std::move(result); return true; }
  const WaveTrain trains[2]={
    makeTrain(s.windSpeed,s.fetch,s.swell,s.spread,s.windDirection,1.0),
    makeTrain(s.crossSwell.windSpeed,s.crossSwell.fetch,s.crossSwell.swell,
              s.crossSwell.spread,s.crossSwell.directionRadians,s.crossSwell.weight),
  };
  const double dk=2*Pi/s.patchLength;
  u32 state=s.seed;
  for(u32 y=0;y<n;++y) for(u32 x=0;x<n;++x) {
    // Keep random sequence independent of spectral band selection.
    const double radius=std::sqrt(-2*std::log(uniform(state)));
    const double randomAngle=2*Pi*uniform(state);
    // Drop Nyquist lines: derivative channels cannot represent their sign
    // unambiguously on an even grid. DC must not change the water level.
    if((x==0 && y==0) || x==n/2 || y==n/2) continue;
    const double kx=(x<n/2?static_cast<int>(x):static_cast<int>(x)-static_cast<int>(n))*dk;
    const double kz=(y<n/2?static_cast<int>(y):static_cast<int>(y)-static_cast<int>(n))*dk;
    const double k=std::hypot(kx,kz), wavelength=2*Pi/k;
    if(wavelength<s.minimumWavelength || wavelength>s.maximumWavelength) continue;
    const double kd=k*s.depth, t=std::tanh(kd);
    const double omega=std::sqrt(Gravity*k*t);
    const double derivative=.5*Gravity*(t+kd*(1-t*t))/omega;
    const double angleFromX=std::atan2(kz,kx);
    double density=0;
    for(const WaveTrain &train:trains)
      density+=trainDensity(train,omega,derivative,k,angleFromX,s.depth);
    // O amortecimento de onda curta é da água, não do trem: aplicá-lo por
    // sistema o contaria duas vezes num mar cruzado.
    density*=std::exp(-k*k*s.shortWaveDamping*s.shortWaveDamping);
    // E[|Gaussian complex|²]=2. The conjugate pair in evolution contributes
    // another factor two. N² compensates the normalized inverse transform.
    const double scale=.5*std::sqrt(std::max(0.0,density)*dk*dk)*n*n;
    const double real=radius*std::cos(randomAngle)*scale;
    const double imaginary=radius*std::sin(randomAngle)*scale;
    if(!std::isfinite(real) || !std::isfinite(imaginary)) return false;
    result[y*n+x]={static_cast<float>(real),static_cast<float>(imaginary)};
  }
  amplitudes=std::move(result); return true;
}

bool WaterSpectralField::initialize(const WaterSpectrumSettings &settings) {
  std::vector<std::complex<float>> amplitudes;
  return generateWaterSpectrum(settings,amplitudes) &&
    initialize(settings.resolution,settings.patchLength,settings.depth,amplitudes);
}
} // namespace ae::renderer
